# BCM2727B1 eMMC/SD controller ("Rapu_Memcard") — static decode + phase-2 model

**Date:** 2026-08-22 (task 096 decode; task 097 implementation; task 101
SD ch1 live + ACMD data phases; task 102 IRQ-driven driver)
**Sources:** static disassembly of the Symbian ROM dump
(`firmware-re/dumps/romdumpplus.dmp`, driver at 0x8015E000-0x80166000,
ARM code), cross-referenced with `docs/bcm2727-icu-timer.md` (ICU ids,
sys-block dual view) and `docs/bcm2727-xti-trace.md` §4 (peripheral map).
**Confidence legend:** CONFIRMED = read directly from code/data;
INFERRED = interpretation, arithmetic fits, semantics not 100% pinned;
**MODEL CONTRACT** = defined by the QEMU model + Linux driver pair
(task 097, both sides ours) — these are the semantics hardware
bring-up must confirm or override.

---

## 0. Executive summary

1. The eMMC/SD controller lives at **sys block +0x2800** — kernel VA
   **0xC60328xx** = phys 0x202028xx (dual view, xti-trace §3). It is a
   **custom "Rapu_Memcard" block, NOT SDHCI-compatible**: 16-bit
   registers, two channels with a **0x100 stride** (channel 0 = eMMC,
   channel 1 = SD), command triggered by a register write.
2. **Channel IRQ ids: eMMC = ICU 0x23 (35), SD = ICU 0x24 (36)**
   (CONFIRMED: per-channel id table 0x80164BF4 = {0x23,0x24,...} passed
   to the kernel interrupt-resource calls in ctor 0x80160990 and
   bind/notify 0x801608CC).
3. Driver identity: "Rapu_MMCSD_Resource_Manager" kernel object,
   "MemoryCard_eMMC"/"MemoryCard_SD" channels, DFC queue "MEMCARD_DFCQ";
   power/clock via variant resources "EHwClkMemCard0/1" + voltage rails
   "EVoltage_eMMC{,_C_Core,_C_IO}" (registry at ROM 0x800CC440+).
4. PIO data moves through a **16-bit FIFO port at +0x20**; responses are
   9×16-bit big-endian registers at +0x40..+0x5C; a DMA path exists
   (req mode 2/3/4, command bit 0x1000/0x2000) but is not required for a
   polling driver.
5. This is enough to model the block in QEMU and to write a Linux
   `drivers/mmc/host` driver (PIO polling) + DTS — the plan is §6.

---

## 1. Driver map (ROM, ARM code)

| address | role | evidence |
|---|---|---|
| 0x80160694 | channel ISR (in vtable 0x80165CA0) | saves divisor +0x14→0, reads +0x10 status, handles bits 0x8/0x10, writes cmd 0x10 to +0x00, restores divisor |
| 0x80160720 | init/reset (vtable 0x80165D70) | per-channel reset strobe +0x70 (0x17→0x15), poll +0x74 bit0, RMW +0x0C set 0x0BFF |
| 0x80160810 | channel power on/off | on: RMW +0x0C set 0x800, write +0x2C=0x8080; off: write +0x64=2 |
| 0x801608CC | IRQ bind/notify | id = table[ch] (0x80164BF4), timeouts 50/10000 ms via veneer 0x8015E8AC→0x800C5F74 |
| 0x80160990 | ctor | binds ids 0x23 AND 0x24 (r1 args, same veneer) |
| 0x801627F4 | 9-case command state machine (vtable 0x80165B88) | jump table on req->cmd class |
| 0x801626CC | PIO write loop | `strh buf[i] → [ch+0x20]`, RMW +0x10 set 0x800 to flush |
| 0x80162764 | PIO read loop | `ldrh [ch+0x20]`, RMW +0x10 set 0x400 to ack |
| 0x80163114 | **command issue** | arg → +0x04/+0x08, flags → cmd word, store → +0x00 |
| 0x801632C0 | transfer setup | block size → +0x24 (=size−1), count → +0x28 (=count−1), DMA vs PIO (size ≤ 0x40 → PIO path 0x80163468) |
| 0x80163604 | response readout | 9 regs +0x5C..+0x40 → 18-byte big-endian response buffer |
| 0x80163468 | bus clock set | divisor → +0x14 (0x1F default; computed from channel config rates) |
| 0x80163098 | timeout setup | write +0x18=0x40, timeout → +0x1C (rate×500000≫10), clear +0x34 bit 0x20 |
| 0x80163E3C | channel clock-rate getter | config[0]&0x80 ? (cfg[0x14]≫16)<<9 : cfg[0xC] |

Kernel-call veneers (import stubs, `ldr pc,[pc,#-4]`): 0x8015E8CC →
0x800B7C1C = **16-bit RMW with IRQs masked** (`*r0 = (*r0 & ~r2) | r1`
— r1 = set bits, r2 = clear bits; every "RMW" below is this);
0x8015E8AC → 0x800C5F74 = interrupt-resource call (id in r1);
0x8015E8E4 → 0x800C5F38 = capability query (0x28-byte struct, id 0x23).

Config data (ROM):
- 0x80164BE4 / 0x80165358: channel-offset tables {0, 0x100, 0, 0}
  (two copies, same values).
- 0x80164BF4: per-channel IRQ ids **{0x23, 0x24, 0x23, 0x23}**.
- 0x80164C04: per-channel flags {1, 0, 1, 1}.
- 0x80164BC0/BD4: "MemoryCard_eMMC"/"MemoryCard_SD" descriptors.
- 0x80164C18: "MEMCARD_DFCQ"; 0x80164C28: "Rapu_MMCSD_Resource_Manager".
- 0x80164960: global tick-rate word used in the timeout arithmetic.

---

## 2. Register map (channel window = 0xC6032800 + ch·0x100)

All registers 16-bit unless noted. W/R = driver usage (hardware may
allow more). Offsets from the channel window base.

| off | acc | name | semantics (CONFIRMED usage) |
|---|---|---|---|
| +0x00 | W | **COMMAND** | write triggers command. Low byte = MMC cmd index; bits 8-10 = response type (see §3); 0x1000/0x2000 = DMA/transfer mode; 0x3000 = ADC/mode-5 path; 0x4000 = CMD11/CMD20 stream class; 0x8000 = no-DMA marker. Special: **0x80 = soft reset** (init), **0x10 = stop/abort** (ISR) |
| +0x04 | W | ARG lo | bits 15:0 of the 32-bit command argument |
| +0x08 | W | ARG hi | bits 31:16 (written lo-first, then hi) |
| +0x0C | RW | control / IRQ enable | init RMW sets 0x0BFF (enable status set); channel-on sets 0x800 |
| +0x10 | RW | **transfer status/ack** | PIO write flush: set 0x800; PIO read ack: set 0x400; ISR reads as event status (bits 3/4 error/done class). Treated as "wait-clear-then-set" around transfers |
| +0x14 | RW | clock divisor | 0x1F default at init; ISR saves→0 (stop clock)→restores |
| +0x18 | W | timeout setup | written 0x40 before timeout program |
| +0x1C | W | timeout | computed = tickrate × 500000 ≫ 10, masked ~(0x3F<<16) |
| +0x20 | RW | **FIFO data port** | 16-bit; PIO writes `strh` per halfword (odd byte tail via halfword too); PIO reads `ldrh` |
| +0x24 | W | block size − 1 | per transfer (from granularity fn 0x80163E20) |
| +0x28 | W | block count − 1 | per transfer; CMD16-class also writes it |
| +0x2C | W | mode | 0x8080 at channel-on and at >0x40-size transfers (PIO/FIFO config; INFERRED threshold semantics) |
| +0x34 | RW | control | bit 0x20 cleared at command setup |
| +0x40..+0x5C | R | **response regs (9)** | big-endian byte pairs; read order +0x5C, +0x58 (short response = R1/R3/R6/R7 class), then +0x54..+0x40 for 136-bit (R2) responses; 9×16 = 144 bits = 136 + CRC + end |
| +0x64 | W | power/clock gate | 2 = channel power-down |
| +0x70 | W | reset strobe | 0x17 = assert reset, 0x15 = release (bit 1 = reset) |
| +0x74 | R | reset status | bit 0 = reset done (polled with delay loop between) |

Channel order: **ch0 = eMMC, ch1 = SD** (INFERRED — descriptor order
"MemoryCard_eMMC" then "MemoryCard_SD" in the config block, plus
EHwClkMemCard0 next to the EVoltage_eMMC* rails in the variant
registry; no direct name-assignment store was found — the descriptors
are reached via block-base arithmetic. Pin it during QEMU/Linux
bring-up or with an XTI-jig boot log on hardware).

---

## 3. Command word encoding (+0x00)

From the cmd-issue function 0x80163114:

```
word = cmd_index                     // low byte
     | resp_type << 8                // driver's resp enum:
     //   2 -> 0x100   3 -> 0x900    4 -> 0x200   5 -> 0x300
     //   6 -> 0x400   7 -> 0x500    8 -> 0x600
     | 0x1000  if transfer mode 2    // DMA class A
     | 0x2000  if transfer mode 3/4  // DMA class B
     | 0x3000  if mode 5 (auto/ADC path)
     | 0x4000  if cmd 0x0B or 0x14   // read-ahead / stream class
     | 0x8000  if no DMA channel
CMD7 with arg==0 clears 0x700 (deselect: no response expected)
```

The resp_type numbering is the driver's internal enum (2..8 covering
R0/R1/R2/R3/R6/R7 classes) — exact mapping to MMC response types to be
pinned when implementing (the 0x900 entry is almost certainly the
R2-long class: it is the value ORed when the long-response readout flag
this+0x8E4 is set — see 0x80163194 vs 0x80163648).

## 4. PIO transfer flow (CONFIRMED sequence)

Write (0x801626CC, 0x801632C0): set block size/count (+0x24/+0x28),
program divisor (+0x14) and timeout (+0x18/+0x1C), issue command (+0x00),
then `strh` the payload halfword-by-halfword into +0x20, finally RMW
+0x10 set 0x800 (flush). Read mirrors it: after command, `ldrh` from
+0x20, RMW +0x10 set 0x400 to ack. The ISR (0x80160694) stops the clock
(divisor→0) on interrupt, reads +0x10, sends cmd 0x10 (stop) on the
error path, acks, restores the divisor. **Open:** whether a FIFO-ready
bit must be polled between halfwords (the loops contain none — flow
control is presumably via the IRQ/threshold, or writes simply stall the
bus on hardware; a polling Linux driver must find the ready bit —
candidate: +0x10 read bits).

## 5. Interrupts, clocks, power

- **eMMC = ICU id 0x23 (35), SD = 0x24 (36)** — into the same
  421-id space as the tick timer 0xF2 (icu-timer §2). Both bound by the
  driver ctor (0x80160990) with 50 ms / 10000 ms notify timeouts
  (0x801608CC).
- Clocks "EHwClkMemCard0/1" and rails "EVoltage_eMMC{,_C_Core,_C_IO}"
  are named kernel resources (registry 0x800CC440+); the driver turns a
  channel on/off through its PSU handler ("DMemoryCardPsuHandler",
  table 0x80165518) — channel-off also writes +0x64=2 (hardware gate).
- The two "channels" each see the full 0x100 window; only ids 0/1 are
  real (tables hold 4 slots, slots 2/3 repeat ch0 values).

## 6. Implementation — DONE (task 097; SD ch1 + IRQ driver in 101/102)

Phase 2 landed end-to-end: **NLoader chain boot → Linux 6.12 →
interactive busybox shell → ext4 mounted from the eMMC image → file
written and persisted**. The VFS-panic era is over. Task 101 brought
the **SD channel (ch1)** up alongside it, and task 102 made command
completion **interrupt-driven** (ICU 0x23/0x24, no polling).

### 6.1 What was built

1. **QEMU model** (`emulation/qemu/hw/arm/nokia_e7.c`, "bcm2727.memcard"):
   MMIO 0x20202800, 2×0x100 channels; ch0 = eMMC from
   `-drive if=none,id=emmc` (QEMU `emmc` card type = sd.c eMMC proto),
   ch1 = SD from `id=sd` (task 101: live; image must be power-of-two
   sized or sd.c truncates the CSD capacity). Command register bridges
   `sdbus_do_command()` (returns 0/4/16 payload bytes BE); FIFO +0x20
   bridges `sdbus_read/write_byte`; done raises ICU id 0x23/0x24 (edge;
   the driver unmasks it since task 102). `MEMCARD_DEBUG=1` traces
   commands/FIFO (env-gated, like XTI_DEBUG). The model tracks
   **app-command pairing** (one command after a CMD55 = ACMD) like
   sd.c does — the SD attach path needs it (§6.2).
2. **Linux host driver** `drivers/mmc/host/mmc-bcm2727.c`: PIO with
   **IRQ-driven command completion** (task 102: requests the channel
   ICU id, sleeps on a completion — `/proc/interrupts` shows every
   command as an interrupt; polling remains as the no-IRQ-resource
   fallback), DTS nodes `mmc@20202800` (okay) /
   `mmc@20202900` (**okay** since task 101). Uses `devm_ioremap` —
   NOT `devm_ioremap_resource`: the channel window lives inside the
   GENIO pinctrl node's exclusive 0x4000 claim, and both drivers map
   the same physical register file.
3. **Initramfs** (`emulation/rootfs/build-initramfs.sh`): Debian armel
   `busybox-static` (ARMv5TE soft-float static — runs on ARM1176; the
   local gnueabihf toolchain's glibc is ARMv7 and unusable for static
   userland here). Chain mode now loads `-initrd` at 0x87000000 and
   publishes linux,initrd-start/end in /chosen.
4. **Console**: the chain PL011 (0x40210000) got a real DT node with
   its RX irq wired into the ICU at unused id **0x1a3** — earlycon
   alone is write-only; a shell needs input. Needs BOTH clock names
   (`uartclk` for the driver, `apb_pclk` for the AMBA bus core's
   pre-match periphid read — missing apb_pclk defers the probe
   forever, silently).

### 6.2 Model contract (pins the task-096 open items)

These semantics are ours until hardware says otherwise:

- **Response file +0x40..+0x5C**: 128-bit big-endian, MSB pair at
  +0x5C. 48-bit-class payloads occupy the top two registers
  (+0x5C/+0x58) — exactly the Symbian short-response readout pair;
  R2 fills all eight registers with the 136-bit stream minus its
  leading 8 bits (= Linux's R2 resp[0..3] as 4×BE32).
- **Command word resp field (bits 8-11)**: 0 = none, 1 = 48-bit
  class, 9 = R2 long (0x900 pinned from the ROM). The model tolerates
  any of Symbian's 1..6 as 48-bit (busy variants).
- **Status +0x10**: bit 3 = command done (plain RW: write 0 to clear
  before the next command), bit 4 = error (illegal command or
  expected-response-missing), bit 10 = read-FIFO available (read-only,
  computed), bit 11 = write-flush/ready (read as always set; writes
  of 0x400/0x800 are absorbed acks).
- **FIFO +0x20**: big-endian byte pairs like the response file (high
  halfword byte = first stream byte).
- **Block accounting**: +0x24/+0x28 are the CONTROLLER's byte counters
  (blklen−1 / blkcnt−1, written per transfer by both Symbian and the
  Linux driver); the card's own blklen comes from CMD16 as usual.
- **Stop semantics**: CMD18/CMD25 are open-ended streams — the host
  MUST send CMD12 (word 0x10 abort or a normal 0x10C command) after
  the last byte, else the card never leaves Sending-Data state. The
  Linux driver executes `mrq->stop`/`mrq->sbc` itself (the mmc core
  hands them to the host driver inside one request).
- **App commands carry their own data classes** (task 101): the model
  pairs CMD55 → next command = ACMD exactly as sd.c does. ACMD13
  (SD_STATUS) arms a 64-byte read and ACMD51 (SEND_SCR) an 8-byte
  read (blklen/blkcnt programmed by the driver as usual); ACMD6
  (SET_BUS_WIDTH) arms NOTHING — treating it like plain CMD6 (SD
  SWITCH, 64-byte read) arms a phantom data phase. The eMMC's plain
  CMD13/CMD6 (no data) are unaffected. Getting this wrong does not
  merely lose the SCR/SSR payload: an un-drained ACMD13 leaves the
  sd.c card stuck in Sending-Data state, and the NEXT command comes
  back `rlen=0` — in this QEMU, `sd_illegal` maps to response length
  0 (`sd_do_command` send_response), which the model (correctly)
  flags as ERROR → the mmc core aborts SD attach with -EILSEQ. That
  is exactly how task 101's first boot failed (`mmc1: error -84
  whilst initialising SD card` right after the SSR warning).

Bugs the bring-up caught (all fixed): model must not arm a data phase
on a failed command; error flag must fire for
expected-response-but-rlen-0 (a self-contradictory condition shipped
first and made reads return card-dummy zeros); ICU root handler must
not be `__init` (it executed from poisoned initmem once /init got far
enough to free_initmem — latent since task 094).

### 6.3 Run it (chain boot with the eMMC as real root, SD alongside)

Primary boot since task 098 — the kernel mounts the eMMC itself and
runs `/sbin/init` from the ext4 (no initramfs); since task 101 the SD
card rides ch1 as a second block device (`/dev/mmcblk1`):

```sh
cd emulation/qemu/build
./qemu-system-arm -M nokia-e7 -m 256M -nographic \
  -drive file=../../../firmware-re/analysis/nloader.bin,if=none,id=nloader,format=raw \
  -drive file=../../../firmware-re/nokia-e7-onenand.img,if=none,id=onenand,format=raw \
  -drive file=../../../emulation/rootfs/emmc.img,if=none,id=emmc,format=raw \
  -drive file=../../../emulation/rootfs/sd.img,if=none,id=sd,format=raw \
  -kernel ../../../kernel/linux/arch/arm/boot/zImage \
  -dtb ../../../kernel/linux/arch/arm/boot/dts/broadcom/bcm2727-nokia-e7.dtb \
  -append "earlycon=pl011,0x40210000 console=ttyAMA0 loglevel=7 root=/dev/mmcblk0 rootwait rw"
```

SD card image: `emulation/rootfs/build-sd-card.sh` (64 MiB ext4 data
card with a marker file — NOT a root; power-of-two size is mandatory,
sd.c truncates the CSD capacity otherwise). Drop the `-drive id=sd`
line and the card is simply absent (mmc1 then logs the usual
initialise-failure noise on rescan).

Rescue fallback: add `-initrd ../../../emulation/rootfs/initramfs.cpio.gz`
— `/init` preempts `root=`, mounts the eMMC at /mnt/emmc, drops to a
repair shell.

Kernel build: `make ARCH=arm CROSS_COMPILE=arm-linux-gnueabihf-
bcm2727_nokia_e7_defconfig && make ... zImage dtbs`. Rootfs image:
`emulation/rootfs/build-emmc-root.sh` (busybox root staged into
`emmc-root/`, same armel busybox as the initramfs, `mke2fs -d` into
`emmc.img`, 64MB, no partition table — /dev/mmcblk0 is directly
mountable). Rescue initramfs: `emulation/rootfs/build-initramfs.sh`.
SD card: `emulation/rootfs/build-sd-card.sh` (same mke2fs -d recipe,
marker file, 64 MiB).
Task-098 validation: primary boot reaches `VFS: Mounted root (ext4) on
device 179:0` → `/sbin/init` (busybox init + inittab/rcS from the
eMMC) → `e7:~#` shell; guest writes persist (host-side `debugfs -R
"cat /task098-marker" emmc.img`); rescue boot reads the primary boot's
marker and its own writes persist identically (ext4 journal recovery
also proven across an unclean kill).
Task-101 validation (2026-08-23): `mmc1: new SD card at address a499`
→ `mmcblk1: mmc1:a499 QEMU! 64.0 MiB` → ext4 mounted r/w →
`cat /mnt/task101-sd-marker` returns the host-staged text → write +
sync persisted (host-side `debugfs -R "cat /task101-sd-write" sd.img`)
— full round trip through ch1, eMMC root untouched.
Task-102 validation (2026-08-23): same boot with the IRQ-driven
driver — both channels probe as "IRQ-driven", root still mounts from
the eMMC, and `/proc/interrupts` shows
`bcm2727-icu 35 Edge 20202800.mmc` at 454 events (every command since
boot, incl. the root mount) and `bcm2727-icu 36 Edge 20202900.mmc` at
27, `Err: 0` — zero polling, zero spurious.

### 6.4 Still open (hardware bring-up items)

- +0x0C bit meanings (model: plain RW storage), +0x14 divisor effect,
  +0x18/+0x1C timeout arithmetic: stored but unused by the driver —
  read from silicon when the phone path lands.
- Response-file payload-vs-raw-token layout (contract above is
  payload-based) and the FIFO byte order — confirm with an XTI-jig
  boot log or a Symbian-side trace.
- Channel order ch0=eMMC (INFERRED) — the QEMU model forces it by
  construction.
- FIFO-threshold interrupt semantics: the IRQ driver still polls the
  FIFO ready bits (+0x10 bit 10/11) between halfwords — the emulated
  FIFO is always ready, but silicon flow control (threshold irq?
  bus-stall?) is undecoded; the Symbian PIO loops contain no ready
  polling at all (§4 "Open").
- SD card-detect / write-protect pins: `get_cd` is hard-wired to 1
  (no pin decoded yet); the mmc core logs "host does not support
  reading read-only switch, assuming write-enable".
- DMA descriptor format (req mode 2/3/4): undecoded, driver-RAM
  resident.

## 7. Method / reproducibility

- Scanner scripts (this session): literal-pool scan for 0xC60328xx
  words, LDR-literal xref map, capstone ARM disassembly with pool
  annotation (same method as `tools/gpio_static.py`; scripts kept in
  /tmp/memcard_*.py — fold into tools/ if wanted).
- Anchor check: `strings romdumpplus.dmp | grep Rapu_Memcard` →
  0x80161C7C/0x801622F4; channel tables 0x80164BE4 = {0,0x100,0,0};
  IRQ table 0x80164BF4 = {0x23,0x24,0x23,0x23}.
- Every register in §2 cites its access site in §1; CONFIRMED means the
  store/load was read in the disassembly, not inferred from offset
  adjacency.
