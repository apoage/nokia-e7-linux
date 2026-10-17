# BCM2727 NLoader USB flash-mode — boot-mode entry, MUSB controller, wire framing (level 1)

**Date:** 2026-08-26 (task 108); §10 interrupt decode added 2026-08-31
(task 117); §12 as-built model+glue record added 2026-09-05 (task 123)
**Sources:** static disassembly of `firmware-re/analysis/nloader.bin`
(Thumb, base 0x10020000, file offset = vaddr − 0x10020000), the Symbian
ROM dump (`firmware-re/dumps/romdumpplus.dmp`) for the kernel-side
cross-check, `kernel/linux/drivers/usb/musb/musb_regs.h` as the register
reference, cross-referenced with docs/bcm2727-xti-trace.md §3/§4,
docs/bcm2727-icu-timer.md §4, docs/bcm2727-onenand.md §5.2 — plus one
runtime gdb confirmation against the chain (§1.4).
**Confidence legend:** CONFIRMED = read directly from code/data (or
executed live in the chain); INFERRED = interpretation that fits the
arithmetic but is not 100 % pinned; **MODEL CONTRACT** = model-side
behavior this project defined and validated in the chain (§12 — the
QEMU MUSBHDRC model and its stubs); OPEN = §6.

---

## 0. Executive summary

1. **The flasher boxes' dead-phone USB channel is fully present in
   nloader.bin** — the task-108 gate PASSES. A complete MUSBHDRC device
   stack (EP0 SETUP dispatcher, string descriptors, bulk pipes, ULPI
   PHY access, 8-channel HS DMA) plus the boot-mode selection that
   trips it are all linked into the binary (code 0x10024600-0x10026000,
   strings interleaved).
2. **The USB controller is a Mentor MUSBHDRC at phys sys-block +0x8800**
   — NLoader/pre-MMU view **0x20038800**, kernel view VA **0xC6038800**
   (PA 0x20208800, dual-view rule of xti-trace §3). The Symbian ROM's
   USB OTG driver uses exactly 0xC6038800 (14 literal-pool hits, right
   next to its `"  KRoPower              = 0x%02x"` register-dump
   string) — CONFIRMED on both sides of the MMU. The xti-trace §4
   "0x20025Axx = USB OTG" entry is a *different* block (OTG system
   glue: VBUS/ID/pins/clock), not the core.
3. **Register layout is mainline MUSBHDRC, byte-addressed, with the
   ULPI viewport at 0x74-0x76 and the HS DMA block at +0x200** — offset
   for offset against `kernel/linux/drivers/usb/musb/musb_regs.h`
   (§2.2). Future Linux glue is a `musbr…`-class platform driver job,
   not a decode job.
4. **Boot-mode selection is a hardware "boot source" latch nibble**
   (bits 20-23 of sys-block 0x20040400) read by the mode function at
   **0x10022d42** (called from main at 0x100325c0): value 8 = the flash
   path ("USB -> Update Mode"), 10 = "ADL -> Local Mode" etc. (§1.1).
   The flash-mode reason nibble 1 (Update) at [0x30003e7c] >> 28 then
   gates the USB task start (§1.3).
5. **NLoader's USB stack is POLLED, not interrupt-driven** — there is
   no ICU write anywhere in nloader.bin and no handler-table
   registration for USB; `usb_task_poll` (0x100246ce) loops
   ≤ 28000 × `usb_hw_handle_events` (§2.5). The kernel-side ids are
   DECODED now (§10, task 117): **MUSB core = ICU 0xB7/183, HSDMA =
   ICU 0xD7/215** (both bound by the Symbian USB PDD), plus three
   candidate ids for the OTG system-glue block (0x63-0x65, bound by
   the separate DUsbOtgDriverFactory module).
6. **First framing level:** the device enumerates as
   "Nokia Mobile Device (NLoader mode)" with serial
   `"10000000" + %08X` (a HAL-read unit id); SET_CONFIGURATION(1)
   arms **EP1 IN + EP1 OUT bulk, 512-byte high-speed** (64-byte FS
   other-speed config). **Flash commands are OUT VENDOR control
   requests with a data stage ≤ 1012 bytes**: the driver stages a
   12-byte header copy + payload and hands them up through a callback
   table (§4). That callback layer (and the descriptor RAM blob with
   the VID/PID bytes) is staged in the 0x100070xx service block whose
   contents are **not in nloader.bin** — OPEN, phone-side catch will
   settle it.

---

## 1. Boot-mode entry ("ADL -> Local Mode" / "USB -> Update Mode" ...)

### 1.1 The mode function @ 0x10022d42

Called from NLoader main (0x1003259a) at **0x100325c0**
(`bl 0x10022d42`), return value stored to **[0x30003e7c]** — the
startup-reason word of onenand §5.2 (nibble = bits 31-28:
1=Update, 2=Normal, 4=Test, 8=Local). CONFIRMED (code + runtime §1.4).

Inputs it merges:

- `r4 = bl 0x10020dc0` — HAL reason word via the RAM service table at
  [0x10007040] (veneer `ldr r0,=[[0x10007040]]+4`; in our chain this
  service is stubbed and feeds **0x64**).
- flags from sys-block **[0x20033000+0x18]** bits 4..0
  (charger/power state bits: bit4→0x50, bit2→0x180, bit3→0x1FF,
  bit0→0x2000, bit1→0xC00 ORed into r4 — INFERRED weights, exact
  semantics not chased).
- the **boot-source latch**: `r5 = ([0x20040400] << 8) >> 28` =
  bits 20-23. If r5 == 0 → print nothing, skip the switch. Else
  print `"Boot source : %d"` and dispatch a 16-entry byte table at
  0x10022db0 (index r5, target = 0x10022db0 + 2*byte):

| latch | handler | print | effect on reason r4 (then common tail) |
|---|---|---|---|
| 0 | (early beq) | — | unchanged |
| 1 | 0x10022df6 | "Forced Test Mode" | r4 = (r4 & 0x0FFFFFFF) \| 0x40000000 |
| 2,3,5,6,7,9,11 | 0x10022e34 | "Unknown Mode" | return (the `cmp r5,#8` there is dead code) |
| 4 | 0x10022e2c | "Massmemory boot" | return unchanged |
| 8 | 0x10022dd2 | **"USB -> Update Mode"** | → §1.2 (never returns to the switch tail) |
| 10 | 0x10022dc0 | **"ADL -> Local Mode"** | r4 = (r4 & 0x0FFFFFFF) \| 0x00102000 \| 0x80000000 |
| 12 | 0x10022dda | "Forced Normal Mode" | r4 = (r4 & 0x0FFFFFFF) \| 0x20000000 |
| 13 | 0x10022de6 | **"Forced Local Mode"** | r4 = (r4 & 0x0FFFFFFF) \| 0x80000000 |
| 14 | 0x10022e06 | "FOTA -> Normal Mode" | r4 \|= 0x00030200 \| 0x20000000 |
| 15 | 0x10025e1a | **"FOTA -> Update Mode"** | r4 = (r4 & 0x0FFFFFFF) \| 0x10000000 |

All strings at 0x10022f48-0x10023010, referenced by Thumb `adr`
(NLoader's string idiom — there are **no absolute string pointers
anywhere in the image**; xref by `adr rX, #imm` with word-aligned PC).

The latch is acknowledged by writing **0x00F00000 to 0x20040408**
(clear nibble) and a bit to 0x20040404 (0 or bit 23 depending on path)
— CONFIRMED stores at 0x10022ed0/0x100318de.

### 1.2 Latch 8 — the USB flash-mode path (0x10022dd2 → 0x10022e3e)

```
print "USB -> Update Mode"
r6 = 0x1000f000                       ; shared boot block ("CHAR" magic at +0x10)
retry = [r6+8] (byte, survives warm reset)
if retry == 0: retry = 11, print "Initialise USB delay counter (10)"
else:           print "USB delay counter : %d" (retry-1)
charger = bl 0x10022cea               ; charger-type HAL (CHRG_DET machinery,
                                      ;  strings 0x10020b80-0x10020d10)
  charger == 1 -> bl 0x10022cf2(0)    ; set charging current 0 mA
  charger == 2 -> bl 0x10022cf2(100)  ; 100 mA
  else         -> bl 0x10022cf2(500)  ; 500 mA
[--retry] -> [r6+8]
if retry != 0:
    bl 0x10022d0c(1, 0x20); print "Wait 5 seconds..."
    delay = [0x30003e80] * 5,000,000 iterations
    print "Forced security reset"; bl 0x10022d30(0,0,0x27)
    print "Generate SW Reset"; [0x20033100+0x14] = 2; spin
    ; -> SoC warm reset; NLoader re-runs with retry one lower
else:
    print "Counter expired -> forced security reset"
    ack latch (0x20040408 = 0xF00000, 0x20040404 = 0)
    two bl 0x1003277c(0x30000300, ptr->0x809795A3, 2)   ; secure-side property
    bl 0x10022d30(magic-ptr, r6, r1, 0x27)             ; lookups, INFERRED
    return r4 (unmodified — mode comes from the HAL nibble)
```

So a latched USB boot gives the host a **~50 s window made of ~10
warm-reset rounds of 5 s each**; the MUSB device itself is driven
concurrently by the polled USB task (§1.3) — the resets only bound how
long NLoader waits before declaring the attempt expired. The retry
counter at 0x1000f008 and the mode strings both live across these
resets in the 0x1000f000 block. CONFIRMED (all stores/prints decoded);
the "concurrently" part is INFERRED from the task structure (the
poll loop is the only thing that services the controller).

### 1.3 Who starts the USB/flash task

- main: `reason = bl 0x10022d42; [0x30003e7c] = reason` (0x100325c0-8),
  then `bl 0x10030796` (hw_conf), print, and a hang check
  `(reason & 0x4FFF) == 0 && [0x30003e89] == 0` → print + spin.
- `charging_init` **0x1002f842**: when `([0x30003e7c] >> 28) & 1`
  (nibble bit 0 — **Update mode**) → print "FOTA: Enable charging",
  query charger type (0x1002f722, HAL resource 0x591; 0 and −27 are
  rejected) → **bl 0x1002f6d0(type)** for types 1/2/3.
- **0x1002f6d0** → 0x1002f690 → **usb_task_poll 0x100246ce** (also
  reached from the plain charging path when a charger is present at
  boot — the USB stack doubles as the charger detector).
- `usb_task_poll`: state machine on [0x10007b48+0xC]
  (1=powered…5=configured); first pass runs **usb_hw_init 0x10024848**;
  then ≤ 0x6D60 iterations of `bl 0x10025fb2`
  (**usb_hw_handle_events** = drain "IRQ" status regs + process pipe
  queues); on the suspend flag it re-evaluates the charging current
  (500 mA after configuration, 100 mA resumed-unconfigured — the
  "-> re-enum with 100mA" string at 0x10024800) and re-runs
  **usb_connect 0x10024698** (POWER |= 0x41). Return value = mA.
- The mode pass 0x1003174c and the SUSD determiner 0x100318f2 then
  consume the nibble for image selection exactly as onenand §5.2
  documents (not re-derived here; entry from 0x100318ce starts the
  same task starter after a charger event).

### 1.4 Runtime confirmation (gdb-multiarch, chain boot)

`-S -gdb tcp::1235`, hw breakpoints, 2026-08-26:

```
hbreak *0x10022d42 → hit, LR = 0x100325c5   ; main's BL, Thumb bit set
hbreak *0x10022d98 → hit, r5 = 0            ; boot-source nibble extracted = 0
                                            ; → beq skips the whole mode switch
                                            ;   (no "USB -> Update Mode" path)
hbreak *0x100325c4 → hit, r0 = 0x64         ; reason returned to main
hbreak *0x100325cc → hit, [0x30003e7c] = 0x00000064
```

Then detached; the chain booted on to `e7:~#`. (The determiner's
subsequent nibble-2 write is already chain-validated — every boot log
prints "Startup mode : Normal", onenand §5.3.) This confirms the
Normal-path shape of §1.1: latch 0 → switch skipped → HAL word 0x64
stored verbatim. The [0x20040400] read itself cannot be observed via
gdb (unassigned MMIO in the model errors the stub) — reading the
post-extract register is the workaround. CONFIRMED.

---

## 2. The MUSB controller

### 2.1 Base address

- **NLoader / pre-MMU / secure view: 0x20038800** — direct literal in
  three pools (0x1002477c, 0x10024c24, 0x10025d78) plus the doubly
  indirect driver pointer **[0x100070bc] → base** used by all
  register accessors. CONFIRMED.
- Dual view (xti-trace §3: 0x2003xxxx ≡ 0x2020xxxx): sys offset
  **+0x8800** → kernel PA **0x20208800**, kernel VA **0xC6038800**.
- Kernel side CONFIRMED: `romdumpplus.dmp` contains 0xC6038800 × 14 in
  literal pools (0x80174530, 0x8017b050, 0x8017dffc, …) inside the USB
  OTG PDD region; the pool at 0x80174530 sits next to the driver's
  register-dump format `"  KRoPower              = 0x%02x"` — i.e. the
  Symbian driver prints MUSB POWER from this base.
- The XTI-doc "USB OTG +0x25Axx" block (0x20025Axx / VA 0xC6025Axx)
  stays what its strings say — OTG system glue next to the BT PDD; it
  is NOT the core (no 0x20025Axx constant anywhere in NLoader).
- Not in the kernel early-map table (icu-timer §4): 0xC6038800 is in
  the sys page VA window 0xC6030000+ mapped later by the resource
  manager — consistent with gpio-static §1.1.

### 2.2 Register layout vs `kernel/linux/drivers/usb/musb/musb_regs.h`

Accessors (Thumb, all CONFIRMED):

- `usb_write_reg8(reg, v)` **0x10024e64** / `usb_read_reg8` **0x10024e86**:
  byte at `base + reg` through the [0x100070bc] pointer, RMW on a
  16-bit bus (odd register → high byte of the halfword, even → low).
- `usb_write_reg16/reg 0x10024e44/0x10024e4c/0x10024e54`: halfword.
- FIFO window: **base + 0x20 + 4×ep** (`MUSB_FIFO_OFFSET` exact) —
  the mover at 0x10024e9c computes it and copies packets
  (`usb_hw_enqueue_packet` path).

Confirmed offsets and their use sites:

| off | musb_regs.h name | nloader use (site) |
|---|---|---|
| 0x01 | POWER | connect `\|= 0x41` (SOFTCONN\|ENSUSPEND) @0x100246b8; disconnect clears them (mask 0xBE) @0x1002475e; HS/FS read by state machine |
| 0x02/0x04 | INTRTX / INTRRX | event poll @0x100259f2/f4 |
| 0x06/0x08 | INTRTXE / INTRRXE | event mask @0x100259f8/ee |
| 0x0A/0x0B | INTRUSB / INTRUSBE | poll + enable; init writes INTRUSBE \|= 5 (SUSPEND\|RESET) @0x1002486c |
| 0x0E | INDEX | per-EP window select @0x10025f12 etc. |
| 0x10 | TXMAXP | set at FIFO config @0x10025f38 |
| 0x12 | TXCSR / CSR0 | TX kick \|= TXPKTRDY @0x10025656; EP0 DataEnd+ServicedRxPktRdy 0x48 @0x1002518c; FLUSHFIFO @0x10025f4e |
| 0x14/0x16/0x18 | RXMAXP / RXCSR / RXCOUNT | EP0 RX count read @0x10025134 |
| 0x62/0x63 | TXFIFOSZ / RXFIFOSZ | dynamic FIFO programming @0x10025f2a/5c |
| 0x64/0x66 | TXFIFOADD / RXFIFOADD | @0x10025f20/5a |
| 0x74/0x75/0x76 | ULPI_REG_DATA / ADDR / CONTROL | §2.3 |
| +0x200 | MUSB_HSDMA_INTR (32-bit) | DMA IRQ status read @0x10025a34 |
| +0x200+0x10×ch | HS DMA channel blocks | CONTROL@+4, ADDR@+8, COUNT@+0xC — see §2.4 |

Conclusion: **mainline MUSBHDRC (dynamic-FIFO revision), byte-addressed
on a 16-bit bus.** The only deviation from the musbhsdma reference is
the CONTROL/ADDR/COUNT ordering inside each 0x10-stride channel block
(INFERRED to be the SoC wrapper's order; the mainline musbhdrc
musbhsdma driver expects ADDR@+0/COUNT@+4/CONTROL@+8 — the future
Linux glue must use the BCM2727 order, see §5).

### 2.3 ULPI PHY

Two pieces. (a) The standard MUSB ULPI viewport —
`ulpi_write(addr, data)` @0x10025c42: reg 0x75 ← addr, 0x74 ← data,
0x76 ← 1 (REQ), poll 0x76 bit 1 (CMPLT), 50 000-cycle timeout →
"ULPI write timed out"; `ulpi_read(addr)` @0x10025c8c: 0x75 ← addr,
0x76 ← 5 (REQ|RDN_WR), poll, read 0x74. Bit values are mainline
`MUSB_ULPI_REG_REQ/CMPLT/RDN_WR` exactly. CONFIRMED.
(b) A separate **PHY control block at 0x20038c00** (sys +0x8C00):
bring-up writes [+4] \|= 2, polls [+8] bit 0, [+0xC] = 1,
[+4] = ([+4] & 0xFFFFCFE0) \| 0x2010, then pad/clock release
([0x20031a00+4] = 1<<14 …); teardown in usb_hw_fini 0x10025e90
writes [0x20038c00+4] = 2, [+0xC] = 1, [+4] = ([+4]&0xFFFFCFE0)|0x2000.
INFERRED: power/reset/mode of an external ULPI PHY (ISP-style).

USB clocks/pads (bring-up 0x10025b22): sys clock/capability regs
[0x200330c0+0x14] = 0x10, [0x20033180+0x14] = 0x200,
[0x20033080+4] = 0x110, [0x20033140+0x18] = 0x2008, poll
[0x20033180+0x30] bit 10; pad-mux grid 0x20031800/0x20031900/
0x20031940/0x20031980/0x20031a00 (the +0x18xx/+0x19xx grid of
xti-trace §2). CONFIRMED stores.

### 2.4 HS DMA (musbhsdma-class)

Allocator @0x1002542e: per-endpoint channel byte at
[0x10007b70+0x80+ep+0xC], **8 channels**; exhaustion prints
"No MUSB DMA channels available" (string 0x100257c0, ref 0x10025468).
Arming (ep ≠ 0 pipes, 0x10025bc4 → 0x1002542e): ADDR ← pipe.buf,
COUNT ← pipe.remaining, CONTROL ← `0x600 | (ep << 4) | 0xD`
(burst/mode bits + enable+direction, INFERRED bit names) — written
with the 32-bit accessor at 0x10024e4c into **MUSB + 0x200 + 0x10×ch
(+4/+8/+0xC)**. DMA completion arrives in the polled status word
[MUSB+0x200] and the RX/TX DMA handlers at 0x1002550e-0x1002556a
("RX DMA IRQ, while no RX packet" / "DMA BUS error" / "Short RX
(wanted %d, got %d bytes)" — the short-packet terminator of a bulk
transfer). CONFIRMED.

### 2.5 IRQ — there is none in NLoader

- `usb_hw_handle_events` **0x10025fb2** = `bl 0x100259e4` (read+dispatch
  INTRUSB/INTRTX/INTRRX & enables, incl. "USB reset received!" /
  suspend / resume state transitions) + `bl 0x10025ba4` (pipe queue
  processing). It *is* the "IRQ handler" body — called in a poll loop.
- nloader.bin contains **zero** ICU (0x2040xxxx) accesses and **zero**
  writes to the sys-page handler table 0x20033340 from the USB region
  (the 0x20033340 writes that do exist are the charger/CHAR task and
  the mode pass, §1). CONFIRMED by full-image pool scans.
- Therefore no ICU irq id can be cited from NLoader. [Task 117
  correction: the "Symbian ASSP driver ROM 0x800B7C00-0x800B9E00"
  range this section originally cited is the KERNEL's
  interrupt-services layer (BindInterrupt 0x800B89B0 / Unbind
  0x800B8A18 / EOI 0x800B8A60 / cmd-port 0x800B8A98 / dispatcher),
  not a driver. The USB OTG PDD itself is ROM 0x80173060-0x80191900,
  and its ids are decoded in §10: core 0xB7, HSDMA 0xD7.]

---

## 3. USB device identity and descriptor table

All descriptor data lives in the **staged service block
0x10007074-0x100070B0** (SRAM below the image; same block as the
[0x10007040] HAL table — populated by the pre-NLoader environment,
see §6). The GET_DESCRIPTOR dispatcher 0x10024904 serves:

| wValue>>8 | served | length | notes |
|---|---|---|---|
| 1 (DEVICE) | [0x10007078] | 0x12 | RAM blob — VID/PID/bcd not in nloader.bin (OPEN) |
| 2 (CONFIG) | [0x1000708a] | 0x20 | 32 B = cfg+iface+2×EP; both EP wMaxPacketSize **patched to 0x200 at GET time** (0x10024934) |
| 6 (DEVICE_QUALIFIER) | ptr 0x10023e95 | 0xA | points into code bytes — serves whatever is there (build quirk) |
| 7 (OTHER_SPEED) | same cfg blob, fields patched | 0x20 | wMaxPacket 0x40 (full-speed alt config, bConfigValue 2) |
| 3 (STRING) idx 0 | [0x10007074] | 4 | langid 0x0409 (template `04 03 09 04`) |
| 3 idx 1 | fmt "Nokia" (0x10024cac) | | iManufacturer |
| 3 idx 2-4 | fmt "Nokia Mobile Device (NLoader mode)" (0x10024c78) | | iProduct — the flash-mode name |
| 3 idx 5 | [0x10007070] → buffer 0x10007728 | | iSerial = **"10000000" + %08X** of a HAL-read unit id (0x10021810), built in usb_hw_init and printed as "USB Serial Number: %s" |

Expected on the wire per rom-boot-research §1: VID **0x0421**, PID
0x0105-class (ADL/flash) — the actual bytes sit in the staged RAM blob
and are OPEN until the phone-side catch or a SWBL decode provides
them. String-descriptor builder 0x100260ea (len×2+2, type 3, UTF-16LE
— INFERRED from use, not fully decoded).

Endpoint configuration on SET_CONFIGURATION(1) (0x10024a88): FIFO
config 0x10025ec8 for **{0x81, 0x01} = EP1-IN + EP1-OUT**, packet
size from the callbacks' clock/speed service (512 HS / 64 FS), DBUFF
size encoding 0x10|log2(size/8); then the bulk pipe object
(0x10024a4e, 0x218 B node, maxpkt 512, buffer at +0x18). The
"Only VENDOR requests can have additional EP0 data" / "Too many EP0
bytes to receive" guards and the STD set (GET_DESCRIPTOR,
SET_ADDRESS via [0x10007b70], SET_CONFIGURATION, SET_INTERFACE —
dispatcher 0x10024aac) are the complete EP0 control set;
anything else STALLs ("Unknown STD SETUP request (%d)",
"Unknown CLASS request", "Unknown SETUP request").

---

## 4. Flash-protocol framing — first level

Transport shape (CONFIRMED code; payload semantics one layer up = OPEN):

1. **Command = control OUT vendor request with data stage.**
   The only implemented vendor path (0x10024b50) requires:
   `bmRequestType` bit7 = 0 (OUT), type bits = vendor
   ((bmRequestType>>6)&3 == 2), `wLength != 0` and
   `wLength + 12 <= 0x400` (i.e. **1..1012 bytes**).
2. On accept: 12 bytes are copied from the request buffer to
   **0x10007750** (8-byte SETUP + 4 driver bytes — the request struct
   the driver keeps is 12 B, so the copied header = SETUP + the first
   4 payload-ish bytes, INFERRED), `[0x10007b48+8] = wLength`, and a
   RX pipe is armed with buffer **0x1000775C** for the data stage
   (0x10024b88-0x10024bb0). EP0 RX completes through the EP0 state
   machine (COUNT0-gated FIFO reads at 0x1002512c, CSR0 0x48
   DataEnd+ServicedRx for the status stage).
3. **Hand-up contract:** the driver calls the staged callback table
   `[[0x10007bf0]+0x1c]` (= 0x100070ac) —
   `[+0xC](0, nbytes)` per received chunk ("packet received") and
   `[+4](pipe)` on transfer complete ("End of Transfer" print at
   0x1002517e). The flash-command interpreter lives behind those
   pointers — its code is NOT in nloader.bin's referenced set (§6).
4. **Responses** go through `usb_send(buf, len)` 0x10024898 →
   `usb_hw_enqueue_packet` 0x10025bfc → per-EP queue
   [0x10007b70+4×ep+0x48] → EP0/EP1 TX kick (0x1002560c: INDEX=ep,
   TXCSR |= TXPKTRDY; EP1 additionally arms an HSDMA channel).
   Failure prints "usb_hw_enqueue_packet() failed".
5. Bulk EP1 IN/OUT exist for the data phases (image upload/download)
   — driver-side framing observed: short-packet termination
   ("Short RX (wanted %d, got %d bytes)"), 512-byte HS packets, DMA
   on EP1. What the host must send *inside* the vendor payload to be
   answered (the BB5 flash opcodes) is the second level — deliberately
   not chased here.

Sanity note vs rom-boot-research §2: the OMAP-style "host sends 4-byte
command 0xF0030002 + length + binary" ROM protocol is **not** this
channel — that is the Broadcom ROM's 0421:0106 window; NLoader's
channel (this doc) is what a Phoenix/JAF box talks to *after* the ADL
hand-off, over the vendor-request + bulk transport above. The
phone-side catch (listening host before connecting the powered-off
phone) remains the experiment that will see both windows end to end.

---

## 5. Consequences

- **Linux MUSB glue (the collapsed desk item):** pin the platform
  driver at phys 0x20208800, byte-wide registers (16-bit bus RMW not
  needed from Linux — use `musb_readb/writel` style), FIFOs
  0x20+4×ep, ULPI viewport at 0x74-0x76, HSDMA at +0x200 with the
  BCM2727 channel-register order (CONTROL@+4, ADDR@+8, COUNT@+0xC) —
  i.e. a small `musbhsdma`-variant + clock/pad bring-up
  (0x200331xx/0x20033xxx writes of §2.3) + the 0x20038c00 PHY block.
  **IRQs are decoded (§10): TWO ids, not one** — core
  `interrupts = <183>` (ICU 0xB7, INTRUSB/INTRTX/INTRRX in one
  handler) and HSDMA `interrupts = <215>` (ICU 0xD7, the +0x200
  status word; the Symbian driver runs it as a SEPARATE irq — a
  single-irq Linux glue would have to poll +0x200 in the core
  handler, which is NOT what the firmware does). The OTG
  system-glue block 0x20025Axx (VBUS/ID) is NOT covered by either
  id — its Symbian driver (DUsbOtgDriverFactory, a different module)
  binds 0x63/0x64/0x65 (§10.4); an extcon-class device on one of
  those is a separate decode, attribution OPEN.
- **Phone-side catch brief:** listen before attaching the powered-off
  phone; expect first the ROM device (0421:0106, OMAP-ish ASIC-ID
  window per rom-boot-research §2 — unverified for BCM2727), then the
  NLoader device from this doc: strings as §3, serial
  "10000000XXXXXXXX", config #1 with EP1 IN/OUT bulk 512. Capturing
  the device descriptor settles the VID/PID OPEN item, and a Phoenix
  "dead phone" session captured on this channel turns §4's transport
  into the full command set.
- **QEMU:** DONE — task 123 wrote the MUSB model at 0x20208800/
  0x20038a00-viewport/0x20208c00 stub and validated it against the
  Linux glue in the chain (as-built contract: §12).

## 6. Open items

1. **VID/PID bytes** of the device descriptor — in staged RAM
   (0x10007078), initialized outside nloader.bin (nothing in the image
   references 0x10007074-0x1000708a except the getter; no 0x0421/0x0105
   immediates or byte pairs anywhere in the binary). Settle via
   phone-side catch or a SWBL/secure-side decode of the 0x100070xx
   block population.
   [Task 115: the real SWBL flash copy IS decoded now
   (docs/bcm2727-onenand.md §7.6) — it does **not** populate
   0x100070xx; it carries only an unreferenced accessor returning the
   bounds {0x10006000, 0x10007000}.  The population is NLoader's own
   or a later stage's; the SWBL branch of this item is closed
   negative.]
2. **The 0x100070ac callback table** (packet-received/transfer-complete
   handlers, clock-rate service) — same staging story; the flash
   command interpreter itself is behind it. May live in a later-stage
   component loaded only in Update mode (the OneNAND SWBL gap
   precedent — the OneNAND LLD was likewise absent from our dump).
   [Task 115 correction: that precedent no longer holds — the OneNAND
   LLD is PRESENT in the dump at nloader.bin file 0xE000-0xF000; it
   looked absent only because of the load-base mix-up
   (docs/bcm2727-onenand.md §7.5).  Re-check whether this table has a
   similar present-but-misplaced copy before assuming a missing
   component.]
3. **Kernel-side ICU irq id** for USB (Symbian ASSP table / DTS).
   [Task 117: CLOSED — two ids, CONFIRMED from the Symbian USB PDD's
   own bind/unbind/EOI/cmd-port paths: **core 0xB7/183, HSDMA
   0xD7/215**; OTG-glue candidates 0x63-0x65 in the separate
   DUsbOtgDriverFactory module. Full decode: §10.]
4. Exact charger-type values feeding §1.2's mA choice (which of
   1/2 = wall/host/dynamo) — strings exist (0x10020c14/0x10020d10/
   0x10020d2c/0x10020d48) but the HAL's numeric mapping wasn't
   decoded.
5. The 12-byte command header's field layout (SETUP + 4 bytes) —
   first level only, per scope.

## 7. Reproducibility / anchors

- Disassembly idiom: strings are `adr`-referenced (word-aligned PC);
  constants come via literal pools (Thumb `ldr [pc, #imm]`) or the
  [0x100070bc] base pointer; verify with
  `tools/gpio_static.py`-style xref (base 0x10020000). Scratch tool
  used this session: /tmp/nlre.py (dis/xref/consts) — regenerate if
  needed, do not commit (decode-only task).
- Key addresses: mode fn 0x10022d42 (switch table 0x10022db0, strings
  0x10022f48+), USB task 0x100246ce, usb_hw_init 0x10024848, connect
  0x10024698, SETUP dispatcher 0x10024b50, GET_DESCRIPTOR 0x10024904,
  EP0 RX 0x1002512c, event poll 0x10025fb2 → status reader 0x100259e4,
  reg accessors 0x10024e44/0x10024e54/0x10024e64/0x10024e86,
  FIFO mover 0x10024e9c, FIFO config 0x10025ec8, DMA arm 0x1002542e,
  ULPI 0x10025c42/0x10025c8c, PHY bring-up 0x10025b22, fini 0x10025e90.
- Runtime: the §1.4 gdb transcript (port 1235; port 1234 was held by
  a stale instance — pick a free port).

## 8. Hardware observation — normal power-on, 2026-08-31 (passive)

First on-hardware USB measurement (usbmon + sysfs poll + journal;
strictly passive — 260 SCSI CBWs, all read-class, 0 data-OUT bytes;
evidence: docs/usb-obs-2026-08-31-normalboot.txt). What a NORMAL
power-on with USB attached actually does:

- **No 0421:0106 (ROM mode) and no 0421:0105 (ADL) — at any
  timescale.** usbmon is kernel-level and misses nothing. The §1.4
  staged-descriptor flash-mode (latch-8 "USB -> Update Mode") is NOT
  entered on a normal boot; the old "brief Nokia USB ROM appearance"
  premise is refuted for this unit (see rom-boot-research's 2026-08-31
  correction note).
- **The boot presents 0421:0333 = USB MASS STORAGE for ~38 s** (from
  ~1 s after power-on until the Symbian stack re-enumerates as 0335 at
  ~40 s): single interface 08/06/50 (SCSI transparent, Bulk-Only),
  EP 0x81 IN + 0x01 OUT both 512-byte high-speed, GET_MAX_LUN → 1,
  strings 'Nokia'/'E7-00'/'Bulk transfer mode'/'USB Mass Storage',
  iSerial = the IMEI-derived serial (same as normal mode). This is
  NLoader's mode-table "Massmemory boot" (nloader.bin 0x2fec)
  confirmed real and DEFAULT when booting with a host attached.
- **A raw block channel to the mass memory exists for ~38 s at every
  boot, zero special mode.** Read-only imaging of the eMMC from the
  host is therefore trivially possible in future hardware sessions
  (we will not write). NOTE: this exposes the eMMC (user-data flash),
  NOT the OneNAND — it does not solve the root-TOC/spare-area items.
- **The 0333 descriptor is runtime-staged** like §3's flash-mode one:
  'Bulk transfer mode' / 'USB Mass Storage' / the 0421:0333 constant
  appear in NONE of nloader.bin, romdumpplus.dmp, or the OneNAND img.
  Owner OPEN (SWBL-side payload suspected — see the next point).
- **By-product:** SWBL's flash copy (img 0x140800, 0x2200 B) is NOT
  erased — real strings: 'Reading TOC from block %d', 'NLoader copied
  to SDRAM', 'NLoader authentication failed', 'RAP3NAND', 'NLoader
  image not found', 'SDRAM init failed' (build ts 18:41:18, one
  second after NLoader's own). New desk lead: decode SWBL's 8.7 KB —
  the real flash-TOC reader our shim plays (§ onenand doc §1.4).
- Also confirmed: powered-off phone on USB enumerates NOTHING (no
  charging-mode device) — the off state is silent.

## 9. Hardware observation round 2 — cold attach: 0106 ROM mode CONFIRMED (2026-08-31)

Same passive rails (evidence: docs/usb-obs-2026-08-31-normalboot.txt,
round-2 section). Round 1's "no 0106" result was a TRIGGER question,
not a refutation: **cable attach to the unpowered phone (VBUS cold
boot) enumerates 0421:0106** — the ROM-mode device of the March notes:
class FF/FF/FF, **control-only** (one interface, zero endpoints),
strings 'Nokia'/'Nokia USB ROM', ~3.7 s window, no unsolicited data —
it waits for an EP0 engage command (we sent none; standard
enumeration only). The observed catch rhythm, end to end:

  cold attach → 0106 (3.7 s) → timeout → 0333 massmemory (~40–64 s,
  EM-charging screen) → timeout → **0106 warm-reset loop** (4 cycles
  observed, ~10 s period — matching §1's decoded Update-window shape
  of ~10 warm resets) → gives up → full boot (0335).

Power-button boots (round 1) and force reboots (round 2 ×2) both go
straight to 0333→0335 — identical presentation. So: the flasher entry
lives in the cold-attach ROM window(s) only, and the ~3.7 s recurring
windows + ~10 s rhythm is the catch cadence any future listener must
meet. The engage command itself remains unprobed BY DESIGN (no-write
session); it would be a control-OUT on EP0 per the OMAP analogy —
decode candidates: NLoader's staged-callback framing §4 mapped at the
0x100070xx block.

---

## 10. USB interrupt decode — Symbian ROM PDD (task 117, 2026-08-31)

Static decode of `romdumpplus.dmp` (ARM, VA = 0x80000000 + file
offset). Answers §6 item 3; nothing here touches the kernel/QEMU.

### 10.1 The module and the class

The Symbian USB OTG PDD+PSL is one ROM module at
**0x80173060-0x80191900** (code/rodata 0x801730d8-0x80191600, data
0x80191600-0x80191900; entry thunk 0x801730d8 → 0x8018ebc4; import
veneer row 0x80173124-0x80173430). Bounds determined from the E32
header words at 0x80173060-0x801730A0 (textSize 0x1E528, dataBase
0x80191600) and the module's string/RTTI cluster — NOT from §2.5's
old 0x800B7C00-0x800B9E00 guess (that range is the kernel's
interrupt-services layer). All 14 literal-pool hits of 0xC6038800
(0x80174530 … 0x8018f754) fall inside, as do the
`DUsbGenericController::DumpRegisters()` strings (0x801744e4+,
including `"  KRoPower              = 0x%02x"` at 0x80174534, pool
0x80174530 directly before it). Class inventory via the ROM's
type-descriptor cluster 0x8018FFEC-0x80190394: DUsbOtgController,
DUsbGenericController, DUsbRapuIf/9DUsbRapIf, DUsbRapuVariantIf,
DUsbRapVariantIf, **DUsbRapuInterruptHandler**, DUsbEpManager,
DUsbotghsDmaChannel(Manager), DUsbIdVbusStateMachine,
DUsbCableDetector(ISA), DUsbChargerDetectorIsp1702a, THostEpEngine…

The interrupt manager is **`DUsbRapuInterruptHandler`** (vtable
0x801912cc; "RAPU" = RAP USB). It owns **TWO** ICU ids — every method
of the class handles both:

| method | VA | behavior |
|---|---|---|
| constructor | 0x80189E24 | vtable store; **unbinds 0xB7 then 0xD7** (defensive), `[this+0x18]=0`; 0x1C-byte object, factory 0x80189E80 |
| Install(aIsr, aPtr) | 0x80189F9C | `BindInterrupt(0xB7, aIsr, aPtr)`; then `BindInterrupt(0xD7, [this+0x10], [this+0x14])`; on 2nd failure rollback-unbind 0xB7; `[this+0x18]=1` |
| Uninstall | 0x8018A078 | unbind 0xB7, unbind 0xD7 |
| EOI | 0x8018A110 | EOI(0xB7) → if returned 0 also EOI(0xD7) — writes the id to [0xC606000C] |
| re-arm/cmd-port | 0x8018A19C | writes 0xB7 AND 0xD7 to [0xC6060010] |

Kernel helpers behind the module's veneers (0x80173364/6C/74/7C):
**BindInterrupt 0x800B89B0** (validate id ≤ 0x1A4 else −5; software
table at id×8; SELECT [0xC6060020]=id; PRIORITY [0xC6060024]=2 —
this is the icu-timer §1.2 mailbox), Unbind 0x800B8A18 (software
table only), **EOI 0x800B8A60** (validate; EOI write only if a
handler is bound, else −0x11), cmd-port 0x800B8A98.

### 10.2 The two ids — CONFIRMED, three-way validated

- **ICU 0xB7 (183) = MUSBHDRC core interrupt.**
- **ICU 0xD7 (215) = MUSB HSDMA block interrupt** (separate line).

Validation: (a) range — both ≤ 0x1A4 (the 421-id space; 0x1A3/419 =
uart is the top live neighbor); (b) no collision — ROM-wide scans of
`mov r0,#0xB7/0xD7` show only this class's six/five sites plus two
unrelated constants (a memcpy size at 0x800AE394, a user-side return
ladder at 0x81228F6C); the only direct-BL BindInterrupt caller in
the whole ROM is the kernel's own tick bind (0xF2 at 0x800B8F3C) —
every driver binds through its own import veneer (§10.5);
(c) independent reuse — the id pair recurs consistently across five
distinct paths (install ×2, unbind ×5 sites, EOI ×2, cmd-port ×2,
constructor) and the 0xD7 *handler* is pinned by construction: the
DUsbRapuIf creator (0x80186A70-78) passes fn **0x8018A638** into the
handler-object ctor, landing in `[this+0x10]` = the 0xD7 slot.

Trigger type: nothing edge/level-shaped is visible anywhere in the
bind path — the ICU carries only the priority-2 enable (level-held,
EOI-by-id-writeback, same contract as icu-timer §1.2 / the I2C0
precedent). DT int = ICU id numerically → Linux
`interrupts = <183>` and `<215>`.

### 10.3 The ISRs (handler shape + ack sequence)

**0xB7 — core event dispatcher 0x80173478** (installed through the
controller's enable-interrupts path 0x801760F4/0x80176260-70, whose
variant-if call site loads the 0x80173478 literal from pool
0x80176A24; the PIL-side `[ctrl+0x2C]` ISR field itself is filled at
runtime, no literal store exists):

```
usb  = read8(base+0x0A INTRUSB) & read8(base+0x0B INTRUSBE)
tx   = read16(base+0x02 INTRTX) & read16(base+0x06 INTRTXE)
rx   = read16(base+0x04 INTRRX) & read16(base+0x08 INTRRXE)
  ; the three status READS are the ack (MUSB clear-on-read)
resume&&!suspend quirk check; then per-bit dispatch (mainline
MUSB INTRUSB bit semantics):
  0x02 resume→0x801788A4   0x04 reset→0x801787D0 (HS-gated) + 0x8017881C
  0x08 SOF (counter path)  0x10 connect→0x801788F8 (HS-gated)
  0x20 disconnect→0x8017893C  0x40 sessreq→0x80178990
  0x80 vbuserr→0x801789D4  0x01 suspend→0x80178860,
       debounced against the fast counter, window 0xBB8 (3000)
EP0: tx bit0 → 0x801742D4 + 0x8017D158; remaining tx/rx bits →
       per-EP loop 0x80173434(this, is_tx, mask)
; NO EOI inside the body — work is queued to the controller's
; prioritized DFC set (created in the DUsbOtgController ctor
; 0x80177280, callbacks pool 0x8017759C-0x801775C0); the ids are
; released/re-armed through the class's EOI/cmd-port methods
; (§10.1) invoked from the dispatch chain (virtual — exact tail
; sites not statically pinned).
```

Registers are reached through precomputed literal accessors:
status reader 0x8018F758 (`0xA→ldrb [0xC603880A]`,
`2→ldrh [0xC6038802]`, `4→ldrh [0xC6038804]`, literals
0x8018F794-9C), indexed write 0x8018F71C, ULPI viewport read/write
0x80185D54/0x80185E90 (data 0x74 / addr 0x75 / ctrl 0x76 — mainline
MUSB_ULPI_* bit values). Two role gates read literal registers:
DEVCTL bit2 (0xC6038860) and POWER bit3 (0xC6038801).

**0xD7 — HSDMA ISR 0x8018A638 → 0x8018A544**
(DUsbotghsDmaChannelManager):

```
status = read32(base+0x200)                  ; HSDMA_INTR, read once
for ch in 0..7:                               ; 8 channels (ctor loop)
    ctrl  = read32(base + 0x200 + 0x10*ch + 4)   ; CONTROL@+4
    if ctrl & 8:                              ; channel int enabled
        done = (read32(chan + 0xC) == 0)      ; COUNT@+0xC hit zero
    if done:
        ep = this->chan[ch] ([this+0xC+4*ch])
        ep ? 0x8018AD0C(ep)                   ; channel completion
           : trace + atomic ctrl &= ~8        ; stray: mask the
                         (kernel RMW 0x800B7BEC, IRQ-masked) ; channel
```

Independent Symbian-side confirmation of the §2.2/§2.4 HSDMA layout
(8 channels, +0x200 block, CONTROL@+4/ADDR@+8/COUNT@+0xC — channel
arm at 0x8018AAB0-0x8018AD78 writes exactly those via base-literal
0x8018A860/0x8018AEE4 with the channel block stored as an
offset-from-base). So: **split ids, not one** — core events and DMA
completions are separate ICU lines with separate handlers; the Linux
glue should wire two IRQs (or consciously poll +0x200 in the core
handler, accepting a deviation from the firmware's model).

### 10.4 OTG system-glue block 0x20025Axx — different module, three more ids

No 0xC6025Axx literal exists in the USB PDD module; VBUS/ID is NOT
served by 0xB7/0xD7. The glue block's driver is the adjacent module
**DUsbOtgDriverFactory** (ROM ~0x801C4000-0x801C8Cxx, type name at
0x801C4103; DBtPdd starts 0x801C8D70 — the BC02/BT neighbor of the
i2c doc's note). It binds **THREE ids: 0x63 (99), 0x64 (100),
0x65 (101)** (call sites 0x801C4D94/DBC/DE4 via veneer 0x801C4384,
stored at [obj+0x234/0x238/0x23C], handlers 0x801C49D0/0x801C4988/
0x801C48F0; EOI'd at five sites through 0x801C4314). The code that
works the glue registers (0xC6025A2C/0xC6025A34 pools at
0x801C5F60/0x801C6014) tail-executes EOI of **0x63** (site
0x801C60AC) — but per-id attribution (VBUS vs ID vs wake) is NOT
decoded; record 0x63-0x65 as extcon-class candidates, OPEN.
(Corollary: the kernel's DMA request-line table at 0x800D16C8
carries the glue block 0xC6025A30/0xC6025AA4 — that is system-DMA
routing, not an irq source.)

### 10.5 By-products

- **ULPI PHY identity, Symbian side:** the DMA-manager init
  (0x8018D104) reads the PHY vendor/product words through the
  viewport (ULPI regs 0-3 via 0x8018D044/0x80185D54) and checks
  **vendor 0x0451 (TI), product base 0x1508 (TUSB1210-class)** at
  0x801736FC — corroborates §2.3's "external ULPI PHY" inference.
- DFC/thread names in the module's data: 'USB PDD', 'USB PDD
  helper', 'USB PSL', 'UsbDriver', 'UsbExtension', 'UsbDataIf'.
- The I2C0-style idiom generalizes: every driver module binds
  through its own import veneer; a ROM-wide veneer census (34
  bind/unbind/EOI stubs) is the quick way to enumerate a driver's
  interrupt claims.

### 10.6 Reproducibility

- Method: locate pools (`find 0xC6038800` → 14 hits), confirm module
  bounds from the E32 header at 0x80173060, decode the veneer row
  (0x80173124+), raw-word BL census to the bind/unbind/EOI veneers
  (capstone linear sweeps stop on literal pools — scan words), read
  the id immediates at the call sites, cross-check by RTTI name
  (0x801902F0 "24DUsbRapuInterruptHandler") and handler-shape decode.
- Key addresses: bind/unbind cluster 0x80189F24-0x8018A218; install
  callers 0x801760F4, 0x80176260-70 (literal 0x80173478 via pool
  0x80176A24); core ISR 0x80173478 (+ accessors 0x8018F758/71C,
  ULPI 0x80185D54/0x80185E90); DMA ISR 0x8018A638→0x8018A544; glue
  binds 0x801C4D94-DE4; kernel helpers 0x800B89B0/0x800B8A18/
  0x800B8A60/0x800B8A98.
- Scratch script /tmp/usbdis.py (hits/raw-word BL/veneer census) —
  regenerate if needed, not committed (decode-only task).

---

## 12. As-built: the QEMU model + Linux glue (task 123, 2026-09-05)

Task 123 closed the loop the §5 consequences list waited for: the
MUSBHDRC is now modeled in QEMU and driven by a mainline-musb Linux
glue, both validated against each other in the chain.  USB runs in
emulation — real enumeration, real mass storage — without the phone.
Evidence: `emulation/qemu-e7sw/boot-usb-task123.log` (icount,
`-device usb-storage` on a virtual FAT **and** `-device usb-kbd`,
interactive mount/md5/interrupts at the prompt); plain TCG boots the
same flow to `e7:~#` + `Attached SCSI disk`.

### 12.1 QEMU model (hw/arm/bcm2727.c, "bcm2727-musb" sysbus carrier)

- **Placements:** register file at PA 0x20208800 (0x300 window: core
  +0x000-0x1FF + HSDMA +0x200-0x2FF) with the pre-MMU alias at
  0x20038800 (xti-trace §3 dual view, i2c precedent); PHY-control
  stub at 0x20208c00 + alias 0x20038c00.  IRQs wired into the ICU as
  **183 (0xB7, core) and 215 (0xD7, HSDMA)**.  The model is a sysbus
  device of its own (TYPE_BCM2727_MUSB) — the USB bus must hang off a
  device the `-device` machinery can find (bus search walks the main
  system bus; the SoC itself is an unattached plain DeviceState — same
  reason the chain-mode PL011 console is standalone).
- **Register contract (what is real):** the §2.2 map offset-for-offset
  — common regs incl. INTRUSB/INTRTX/INTRRX **set on events,
  clear-on-read** (the §10.3 Symbian ack model; W1C on write too, so
  both glue styles work), enables as RW storage with the core IRQ
  **level-held on the unmasked pending set**; POWER (RESET write →
  usb_port_reset on the root port, HSMODE reads per attached speed,
  SUSPENDM accepted but never reads back set — musb_port_suspend
  polls for exactly that); DEVCTL (SESSION 0→1 with a device attached
  = host session: HM set, BDEVICE cleared, CONNECT raised; VBUS
  always reads VALID); INDEX + the indexed EP window 0x10-0x1F; flat
  EP windows 0x100+0x10*ep; multipoint busctl 0x80+8*ep (TXFUNCADDR
  drives usb_find_device); FIFO ports 0x20+4*ep; dynamic-FIFO
  programming (TXFIFOSZ/RXFIFOSZ/TXFIFOADD/RXFIFOADD as RW storage);
  CONFIGDATA reads 0x06 (DYNFIFO|SOFTCONE), HWVERS 0x0140 (1.4-class
  — only logged by the driver).
- **Host token engine:** CSR0 per the mainline musb_host expectations
  (SETUPPKT|TXPKTRDY sends the 8-byte SETUP, REQPKT the IN data stage,
  STATUSPKT qualifies status; RXPKTRDY/RXSTALL/NAKTIMEOUT are
  write-0-clears, TXPKTRDY is controller-cleared on completion);
  TXCSR TXPKTRDY / RXCSR REQPKT issue OUT/IN tokens with the
  **device-side endpoint taken from the low nibble of TXTYPE/RXTYPE**
  (musb_schedule muxes non-control transfers to any free hw ep — the
  hub status endpoint on device-ep1 can land on hw ep2); data toggles
  flip per ACKed packet and are read/writable via the DATATOGGLE/
  CLRDATATOG bits the default get/set_toggle ops use.  NAKed requests
  stay armed and are retried on the 1 ms frame timer.  Two
  **model-faithfulness refinements** the validation forced:
  (a) interrupt endpoints speak **once per programmed RXINTERVAL**
  (linear ms full/low speed, logarithmic high speed) — answering
  every resubmitted request instantly let the hub driver's
  hub_irq→hub_resubmit_irq_urb path spin the device-IRQ storm without
  ever scheduling the work that clears the change source;
  (b) a usb_port_reset's internal detach+attach must NOT raise
  CONNECT/DISCONNECT — hub_port_wait_reset reads a mid-reset
  C_CONNECTION as a disconnect and aborts enumeration.
- **ULPI viewport 0x74-0x76:** full access engine (REQ/RDN_WR/CMPLT
  handshake, data at 0x74, addr at 0x75) against a 16-byte
  TUSB1210-class register file — vendor **0x0451**, product **0x1508**
  (§10.5 identity), function control defaulting to normal operation.
  PHY probes succeed against it.
- **What is stubbed (CONTRACT):** the HSDMA block is a decoded but
  behaviorless register file (channel order CONTROL@+4/ADDR@+8/
  COUNT@+0xC per §2.4, status word at +0x200 reads 0) — IRQ 215 is
  registered and never raised, awaiting a future DMA task; the
  PHY-control block is a benign register file whose [+8] bit 0 reads
  1 so any power-sequence poll (§2.3) completes; SOF is not generated
  (FRAME counts on the 1 ms model timer); TESTMODE/BABBLE/EP0 ping
  and the peripheral/gadget role are not modeled.  MUSB_DEBUG=1
  traces register traffic, tokens, and IRQ events (setup/data hex
  dumps included — the bisect tool the task asked for).
- **Topology note:** QEMU auto-chains an 8-port hub when a device
  attaches to a single-port bus (usb_claim_port), so `-device
  usb-storage` enumerates as 1-1.1 behind that hub — full-speed, and
  the hub's status interrupt endpoint is part of the exercised
  surface (multipoint addressing, interrupt pacing, hub class
  requests all live).

### 12.2 Linux side (kernel/linux drivers/usb/musb/musb-bcm2727.c)

- **Glue:** host-only, PIO-only platform driver modeled on jz4740/
  mpfs: registers a "musb-hdrc" child with the flat-window (no
  INDEXED_EP quirk) MHDRC config — multipoint, dyn_fifo, 5 hw
  endpoints, 10-bit FIFO RAM, EP1-3 as 512-byte TX/RX pairs; ISR
  reads INTRUSB/INTRTX/INTRRX (clear-on-read — no writeback needed)
  into musb_interrupt; set_vbus writes DEVCTL SESSION.  NOP
  transceiver (usb_phy_generic) — PHY-less bring-up per the task; the
  TUSB1210 ULPI viewport answers on the model side for future
  probing.
- **DTS** (`bcm2727.dtsi`): `musb@20208800`, compatible
  "brcm,bcm2727b1-musb", `interrupts = <183 215>` +
  `interrupt-names = "mc", "dma"` (musb_probe takes "mc" by name;
  215 stays documented, unserviced while DMA is unused), dr_mode
  host.
- **defconfig:** USB=y, USB_MUSB_HDRC/HOST/BCM2727=y, MUSB_PIO_ONLY=y
  (mandatory for a DMA-less glue — without it musb_init_controller
  demands ops->dma_init), NOP_USB_XCEIV, SCSI/BLK_DEV_SD/
  USB_STORAGE, INPUT/USB_KBD, FAT/VFAT+NLS for the round trip;
  `kernel/configs/` mirror synced.

### 12.3 Validation record (exit criteria)

1. Probe clean: `musb-hdrc.1.auto: MUSB HDRC host driver` + `new USB
   bus ... number 1` + `musb-bcm2727 20208800.usb: ... (host, PIO,
   IRQ 18)`; **zero deferrals**; boot to `e7:~#` under TCG and
   icount.
2. usb-storage: enumerates (QEMU MSD), SCSI INQUIRY/CAPACITY work
   over the bulk pipes, `sda: sda1`, `Attached SCSI disk`;
   `mount -t vfat /dev/sda1 /mnt` + `md5sum /mnt/task123.txt` =
   `d8b87d577ac6ebe82bb68431724bb65b` — byte-identical to the host
   file; clean umount.
3. usb-kbd: enumerates (`input: QEMU QEMU USB Keyboard`,
   hid-generic v1.11 Keyboard) alongside the storage in the same
   boot.
4. Boot diff vs task-122 (docs/boot-122-icount-ttyama0.txt): the
   1202-line NLoader flow **identical**; kernel-side adds only the
   USB/SCSI registration+probe lines plus build-string/memory-size
   artifacts (same class as task-113's i2c diff).
5. Evidence: `emulation/qemu-e7sw/boot-usb-task123.log` (this
   section's run, icount control).
6. /proc/interrupts at the prompt: ICU **183 claimed by
   musb-hdrc.1.auto** with 1181 services; no 215 entry (idle by
   design).  The future serial-console endgame note from §0.5 of the
   task file (dev-serial over USB) remains open tooling work on this
   now-live rail.
