# BCM2727B1 console / trace output — XTI mailbox decode

**Date:** 2026-08-22 (task 095)
**Sources:** static disassembly of `firmware-re/analysis/nloader.bin` (Thumb,
base 0x10020000), SWBL inside `firmware-re/nokia-e7-onenand.img` (file
0x141200+, Thumb), the Symbian ROM dump, and **runtime validation in the
QEMU chain boot** (NLoader's real log drained live through the model).
**Confidence legend:** CONFIRMED = seen in code/executed in the chain;
INFERRED = interpretation.

---

## 0. Executive summary

1. **There is no pin-level UART in the APE boot chain.** Everything the
   bootloaders (SWBL, NLoader) "print" goes to the **XTI trace mailbox**,
   an SRAM+engine block at **phys 0x40100000**, as tagged binary records;
   each completed record is signalled by a byte write of the channel id to
   the **doorbell at 0x401FD800**. On hardware the records are drained by
   the XTI/STI trace path (R&D tooling/jig); nothing in a retail phone
   turns them into a wire-level UART by itself. CONFIRMED (code) +
   INFERRED (drain path).
2. **The "NLoader UART" from task-093 notes is an I2C command channel.**
   The driver at NLoader 0x10022300 (poll base+8 bit4, data base+0x1C,
   ack 0x10→base+8, length base+0x18) targets one of three selectable
   bases 0x20032200 / 0x20032300 / 0x20032500 — which alias the sys-block
   I2C masters (the Symbian I2C0 driver lives at VA 0xC6032200 with the
   same register shape, next to its "I2CThread_0"/"RxData"/"TxData"
   strings). NLoader pushes companion-chip commands (0xD6 …) through it
   during boot. CONFIRMED.
3. **The sys block is visible at two physical addresses:**
   0x2020xxxx (APE kernel view; VA 0xC603xxxx) ≡ 0x2003xxxx (pre-MMU /
   secure-side view used by SWBL/NLoader). Same offsets observed on both
   sides: pad grid +0x1810-, clock/capability regs +0x30xx/+0x31xx, the
   I2C engines +0x22xx. CONFIRMED by matching register writes across
   SWBL/NLoader vs the Symbian drivers.
4. The decode is **validated end-to-end in QEMU**: the nokia-e7 machine
   now models the mailbox + doorbell, the printf-path patch is gone, and
   NLoader's *real* formatted log (463 records: TOC dump, GENIO, hw_conf,
   OPP tables, PMM dumps, "Start SOS …") prints in the chain boot.

---

## 1. The XTI trace mailbox @ phys 0x40100000

### 1.1 Who uses it

| consumer | use | evidence |
|---|---|---|
| SWBL | trace records (same tags/protocol as NLoader) | pools 0x40100000/0x401FD800 at OneNAND file 0x141444/0x141456 |
| NLoader | all printf output: channel **0xEF** window 0x401EF000 | formatter 0x10021E90 (ARM), wrapper 0x10026204 (Thumb) |
| Symbian kernel | "XTI tick" calibration reads the block's counter at 0x40100000 ("!T51.02040816 ns XTI tick" = 19.6 MHz XTI) | RapidoVariant code 0x800BEACC |
| Symbian XtiRx driver ("XTI RX Driver", XtiRxThread) | direct byte-FIFO ports 0x40103880 / 0x40103EC0; wire bytes 0xC3/0xE2/0xFB | module ~0x803CA2xx-0x803CA4xx, refs 0x803C9904+ |
| Symbian kernel early map | VA 0xC6F00000 ← PA 0x40100000 (locked TLB) | bootstrap table ROM 0x800020A0 |

So the 0x40100000 block is the platform's **XTI/STI trace engine**
("STI = Serial Trace Interface", service-manual glossary): ~1 MB of
channelized trace SRAM + control + FIFO ports.

### 1.2 Record protocol (CONFIRMED, matches drained bytes exactly)

Per-channel 4 KB window at `0x40100000 + (ch << 12)`. Within a window,
records are a stream of 32-bit slots addressed by a **single global
write-offset counter** at NLoader globals `[0x100076F0+0xC]`
(= 0x100076FC), advanced +4 per slot and **masked to 10 bits** — a 1 KB
ring inside the 4 KB window. The counter points **at the last-written
slot** (stores pre-increment).

Record layout (tag 0xC7 = formatted, 0xC3 = plain string):
- byte at slot+0: tag (re-written per record; **clobbered once the ring
  wraps** — the ring offsets 0-3 overlap the tag slot).
- format string, from slot offset 4: **4 chars packed per slot** while
  the source word has no zero byte (the copier at 0x10021DFC uses the
  classic `w - 0x01010101 & ~w & 0x80808080` test); the tail is stored
  **one char per slot (low byte)**; a NUL byte in its own slot ends the
  string. The same packing applies to %s argument strings (copier is
  shared).
- then one slot per argument: %d/%u/%x/%X/%o/%p = one word;
  %c = one byte in a slot; %s = inline string element as above;
  %e/%f/%g = two words (double, low first); %% and %n = no slot.
- **conversion scan is prefix-tolerant** (0x10021EEC loop): after `%`,
  unrecognized chars are skipped *staying in conversion mode* — so
  "%08X" acts as %X, "%sv" prints %s then literal 'v', "% d" acts as %d.
- after the record: `strb channel, [0x401FD800]` — the doorbell.

Important fidelity notes (learned by draining):
- Byte-tail stores do **not** clear the upper 3 bytes of a slot — on a
  wrapped ring those bytes are stale, making a tail slot
  indistinguishable from a packed word. A drainer must **drain-and-clear**
  (zero consumed slots) — which is presumably what the real "XTI pipe
  cleaner" does. The QEMU model drains per doorbell and clears slots.
- The guest offset wraps through the tag slot; treat tag as advisory.

### 1.3 QEMU model (chain boot)

`emulation/qemu/hw/arm/nokia_e7.c`:
- `bcm2727.xti` RAM: 1 MB at 0x40100000 (all modes).
- `bcm2727.xti-doorbell` MMIO at 0x401FD800 (overlap priority 1): on a
  channel-id write, reconstruct the new records (tracked ring offset +
  drain-and-clear) and print them formatted (`nloader[xti239]: …`).
- The task-093 printf **stub patch is removed** — NLoader's real
  formatter runs; values (%08X etc.) print correctly.
- **PL011 moved to 0x40210000** (out of the mailbox); default chain
  cmdline is now `earlycon=pl011,0x40210000`.
- Debug aids: `XTI_DEBUG=1` env dumps doorbell traffic (ch/tag/cur/prev)
  and window snapshots around the wrap to /tmp/xtiwin-NN.bin.

Validation: the chain boot prints NLoader's full real log (463 records —
HW version "RAPgv0.00", "XTI pipe cleaner"/"XTI enabled", SW build
"nloader_111_10.1.1 (Jul 27 2012 18:41:17)", startup reason, clocks,
heap, the complete TOC dump with sizes/addresses, GENIO steps, hw_conf
records (SDRAM/PDRAM/speeds/MDI/M2D/ROM-jump-table 0xFFFD1200/BSI/TOC/
startup reason), the DVFS "fop" OPP tables, the PMM register dumps at
0x30003F00+, "Total bootup time", "Start SOS at 80010000 (->80010000)")
and then Linux boots through to the expected VFS panic with the
self-calibrating delay loop. This is the end-to-end proof of §1.2.

---

## 2. The I2C command channel (ex-"NLoader UART")

Send loop 0x10022300 (`channel_write`): length → [base+0x18]; poll
[base+8] bit 4; byte → [base+0x1C]; ack 0x10 → [base+8]; on [base+8]
bit 1 → write 2, return error. A per-byte delay loop ([0x30003E80] × 200)
paces it. Base = `[0x100076F0+0x24]`, selected by the function at
0x100220D6: arg 0/1/2 → **0x20032200 / 0x20032300 / 0x20032500**; both
observed call sites (0x100225FC, 0x10022C14) select channel 2. Channel
setup additionally programs the routing/pad-mux registers
0x20031810/0x20031814/0x20031834/0x20031854/0x20031858 (3-bit fields) and
companions 0x200319C0+0x28, 0x20031A38, 0x20033080+0x34, 0x200330C0+0x14.
SWBL uses 0x20032600 (+0x64 data / +0x6C cmd-status, poll bit 4) for its
secure-side commands (0x9400/0xF400-style opcodes).

Kernel side: the I2C0 driver (strings "I2CThread_0", "Arbit/Cancel/
RxData/TxData/TimeOut", "DI2CPowerHandler", next to the BT "BC02" power
code) touches **VA 0xC6032200/2208/2218/221C/2224/2228/2234** — same
+0x08/+0x18/+0x1C/+0x24/+0x28 shape. So the NLoader channel bases alias
**sys-block +0x2200/+0x2300/+0x2500** = the SoC's I2C masters (three
exist per the clock names EHwClkI2C0/1/2). What NLoader actually sends:
0xD6-addressed command bursts — companion-chip (PMIC/audio) config, not
text. The 0x10022300 function's callers (0x100224F6/0x1002258A/0x100225D0)
form the command layer (command 0xD6 with 2-byte payloads from a table).

**Consequence:** the task-093 "UART shim" (globals+0x24 pointed at
0x401F0000) was patching an I2C engine, and the "NLoader drives a UART"
line in docs/bcm2727-pinctrl-linux.md §4 item 3 was a misread of this
register shape. No bytes meant for a console ever flow there.
(2026-08-30, task 112: the workaround is RETIRED — the engines are
modeled as `bcm2727.i2c` and NLoader's selector drives them through
the 0x200322xx alias; see docs/bcm2727-i2c.md §6.1.)

## 3. Sys-block dual view: 0x2003xxxx ≡ 0x2020xxxx

Bootloaders (pre-MMU) use 0x2003xxxx; the kernel uses VA 0xC603xxxx =
PA 0x2020xxxx. Matching evidence:
- SWBL writes 0x20033000/0x20033100+4/0x20033140+4/0x20033180+0x14 —
  the +0x30xx/+0x31xx clock/capability registers the kernel reads at
  0xC6033090/0xC6033130 (fast-counter capability flags, icu-timer §3.2).
- NLoader/SWBL routing writes 0x20031810/0x20031834/0x20031858… = the
  pad-mux grid offsets (+0x1810 from the sys base) that GENIO_INIT writes
  as 0x20201810+ ("printed − 0x30000" rule, genio-padtable §3).
- NLoader I2C bases 0x200322xx = kernel's 0xC60322xx.
- The bootstrap blob's "security regs 0x20030300" = sys +0x300;
  "handler table 0x20033340" = sys +0x3340 (i.e. the kernel's IRQ table
  lives in a small SRAM window inside the sys page, 16 B below the
  GPIO-mux pending register at +0x3350).

Whether it is a true hardware alias or a second (secure-side) decode of
the same block is microarchitecture; for all driver purposes the two PAs
interchange. The kernel early-mapping table entry 0xC6020000 ←
0x20020000 already maps the *other* 0x2002xxxx peripherals (below).

## 4. Peripheral map additions (from this session's ROM scans)

| phys | VA (kernel) | block | evidence |
|---|---|---|---|
| 0x20020400 / +0x600 / +0xA00 | 0xC60204xx/06xx/0Axx | **SPI1/2/3 masters** ("DSpiHwRap", "Spi1SpiDriver Thread"…) | three identical register sets +0x04..+0x64, drivers 0x800E9xxx/0x800EBxxx/0x800EDxxx |
| 0x200243xx | 0xC60243xx | **security engine** ("security_drivers/rapu", PA_DRM/PA_HDCP/PA_CRYPT…) | 0x80139xxx-0x80140xxx |
| 0x20024810 / 0x20025810 | 0xC6024810/0xC6025810 | **display supply pair** ("VCDriver", Vaux/Vout/Vio) | 0x8032Axxx-0x8033xxxx |
| 0x20025Axx | 0xC6025Axx | **USB OTG + BT PDD** ("USB OTG Driver", "BC02.Rap", "DBtPdd") | 0x801C4xxx-0x801C8xxx |
| 0x20202800 | 0xC60328xx | **eMMC/SD controller** ("DMemoryCardStackPlatform", "Rapu_Memcard_*", "MemoryCard_eMMC/_SD") | 0x80160xxx-0x80163xxx — the rootfs target |
| 0x20202200 (+0x2300/+0x2500) | 0xC60322xx | **I2C masters** | §2 |
| 0x20202424 | 0xC6032424 | single ref from the ECOMM PDD region (0x800D1618) — semantics open | |
| 0x40100000 | 0xC6F00000 | **XTI trace engine** | §1 |

## 5. Console strategy going forward

- **QEMU chain**: NLoader/SWBL-side logs come from the XTI drain (real
  text, real values). Linux keeps the PL011 earlycon at 0x40210000 —
  that PL011 does not exist on hardware.
- **Hardware**: no APE-visible UART block appears anywhere in the boot
  chain or kernel statics; the live kernel reports HAL EDebugPort = 0.
  Realistic options when we get to hardware:
  1. **XTI trace jig** (R&D hardware on the STI pins/test points) —
     Linux would write NLoader-style records to the mailbox. Needs the
     jig and its protocol decoder.
  2. **Soft-UART TX bitbang on genio42 ("TxCDa", UI-flex area test
     point)** — **DONE 2026-08-23 (task 100)**:
     `drivers/tty/serial/bcm2727_softuart.c` (ttySU0 + console +
     `earlycon=bcm2727su`), QEMU wire tap + byte-exact chain
     validation; see `docs/bcm2727-softuart.md`.  Hardware bring-up
     needs only the wire on the test point.
  3. ECOMM/"Serial::1,2" ports exist in Symbian user space, but their
     PDD gets its base from driver RAM (not statically resolvable) and
     LDD channels are All+TCB-walled from the bridge — dead end for us.

## 6. Reproducibility

- Chain boot with full NLoader log: run command in
  `docs/bcm2727-pinctrl-linux.md` §5 (earlycon now at 0x40210000).
- Offline parser check: the record grammar was verified against live
  window dumps; `xti_read_str`/`xti_parse_record` in nokia_e7.c are the
  reference implementation (a host harness replaying dumped windows
  reproduces the same text).
- NLoader anchors: printf wrapper 0x10026204, formatter 0x10021E90,
  string writer (0xC3) 0x1004C710, copier 0x10021DFC, I2C send
  0x10022300, channel selector 0x100220D6, globals struct 0x100076F0
  (+0x0C stream offset, +0x24 I2C base).
- SWBL: OneNAND file 0x141200+ (build banner "RAMSET SW : (%s %s)" /
  "Jul 27 2012 18:41:18"); trace pools 0x40100000/0x401FD800 at file
  0x141444/0x141456; channel regs 0x20032600 (+0x64/+0x6C).
