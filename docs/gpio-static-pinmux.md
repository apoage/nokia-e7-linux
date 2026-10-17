# BCM2727B1 GPIO / Pin Configuration — Static ROM Analysis

**Date:** 2026-08-21
**Source:** 100% offline static analysis of `firmware-re/dumps/romdumpplus.dmp`
(31,793,152 bytes, Symbian ROM, virtual base 0x80000000, file offset = vaddr − 0x80000000).
**Tool:** `tools/gpio_static.py` (capstone-based literal/xref/store tracker, written for this task).
**Confidence legend:** CONFIRMED = decoded instruction sequence actually present in the ROM.
INFERRED = interpretation of a CONFIRMED structure whose semantics cannot be proven from the ROM alone.

---

## 0. Executive summary (read this first)

1. **The Symbian ROM contains no pin-mux table and never programs pin alt-functions.**
   The kernel GPIO driver's hardware mode-set entry point (`0x800C19C4` → `0x800C0B24`)
   is a **stub: a single `bx lr`**. There is no FSEL/PADCONF-equivalent write anywhere
   in the 31.8 MB image. CONFIRMED.
2. The BCM2727B1 GPIO controller is **not BCM2835-layout**. It is a custom Broadcom
   "RAPU" GPIO block with 7 banks × 32 pins, per-bank register trios
   (read status / write-1-set / write-1-clear), an IRQ/pull block per bank, and two
   wake-only banks (5, 6). CONFIRMED (register map below).
3. Pin direction, output data, interrupt mode, wake enable and edge detect ARE
   software-programmable at runtime — all via **write-(1<<pin) SET/CLR register pairs**.
   CONFIRMED. These are the only "pin configuration" Symbian performs.
4. Whatever configures pin *functions* (GPIO vs UART/I2C/CSI/DSI routing) happens
   **before Symbian boots** — NLoader / GENIO_INIT / SWBL / TrustZone firmware /
   VideoCore ROM — or is fixed in hardware. This matches the earlier finding that
   GENIO_INIT is BB5-encrypted and invisible to this dump.
   **UPDATE 2026-08-22: the pre-boot pad-function table was recovered in
   plaintext from the OneNAND image (factory data region) — see
   `docs/genio-padtable.md`: 176 pads × 5 function codes + per-bank boolean
   rails, sitting in the same register file just below the wake slots
   (phys 0x20201810–0x20201960 under the −0x30000 print offset). The hardware
   DOES have a function-select register grid; Symbian simply never writes it.**
5. Several corrections to earlier docs: the "GPIO init cluster 0x14C800–0x14E200"
   was a false lead (ASCII spaces, §7), and the 0xC8004000–0xC8006FFF window is
   **kernel/driver RAM**, not MMIO (§2). The real MMIO VA window is 0xC60xxxxx.

**Bottom line for a Linux device tree:** there is no pinmux table to port. A Linux
`pinctrl` driver for this SoC needs to implement direction/pull/irq/wake from the
register map in §3 and treat "function" as fixed-by-bootloader.

---

## 1. How GPIO physical memory enters the kernel (CONFIRMED)

Exactly **six** literal-pool words in the whole ROM reference the physical GPIO
base window 0x20200000–0x2020FFFF; five are code, two of those six hits are
ASCII false positives:

| literal | pool va | referenced by | role |
|---------|---------|---------------|------|
| 0x20200000 | 0x8088575C | `ldr r1` @ 0x80885364 (Thumb, fn 0x80885304) | resource registration |
| 0x20200000 | 0x80886E20 | `ldr r1` @ 0x80886DB2 (Thumb, fn 0x80886D9A) | resource registration |
| 0x20200000 | 0x80889554 | `ldr r5/r2/r7/r1` @ 0x8088933C/68/94/0x8088944E (Thumb, fn 0x808893C4) | resource registration ×4 |
| 0x20200000 | 0x8088E4D4 | `ldr r7` @ 0x8088E2D6 (Thumb, fn 0x8088E284) | resource registration |
| 0x20200000 | 0x8088F274 | `ldr r1` @ 0x8088F06C (Thumb, fn 0x8088F016) | resource registration |
| 0x20200003 | 0x808169F0 | `ldr r5` @ 0x808167F4 (ARM) | *command code*, not an address: `[r1]` is compared against 0x20200001/2/3/4, 0x20300001/2, 0x20400001… in a clock/power dispatch |
| 0x20200A2C | 0x80BAF4E8 | (string data: `",\n  "` from the sqlite schema) | false positive |
| 0x20200000 | 0x80BAFEC0 | (string data: spaces + NUL) | false positive |

Every code site does the same thing — e.g. fn 0x80885304 @ 0x80885364:

```asm
80885364  ldr   r1, [pc, #0x3f4]   ; = 0x20200000   (GPIO physical base)
80885366  movs  r2, #0x81
8088536a  lsls  r2, r2, #0x16      ; r2 = 0x20400000
8088536c  str   r1, [sp]           ; arg5 = 0x20200000
8088536e  str   r2, [sp, #4]       ; arg6 = 0x20400000
...
80885378  bl    0x8088B8CC         ; kernel resource registrar
```

The registrar (0x8088B8CC) builds two descriptor objects and registers the range
with the kernel. The descriptor pairs found around all sites are
{0x11D00000, 0x20400000} and {0x11E00000, 0x20200000} (second element computed as
`0x81<<22` / `0x8F<<21`).

**CONFIRMED:** GPIO physical window starts at 0x20200000 and the registered
region extends to at least 0x20400000 (2 MB "system block": GPIO + clock/power +
security + VideoCore interface, see §2).
**INFERRED:** the descriptor pair is {base, base+size} or {base, end}.

### 1.1 Physical → virtual mapping

The registered physical window is mapped by the kernel into the fixed VA window
**0xC60xxxxx**. The GPIO register file (decoded from the driver init table,
§3) lives at VA 0xC6031990–0xC6031AC0 and 0xC6033400–0xC60335C0. Since the
driver can only be pointing at GPIO, and every other driver in the ROM touches
the same 64 KB VA page (0xC6030000–0xC603FFFF) for clock/power/security
registers, the mapping is page-linear:

| VA page | physical (INFERRED) | content |
|---------|--------------------|---------|
| 0xC6020000–0xC602FFFF | 0x20100000+ | SPI controllers (0xC6020400/0600/0800/0xA00 = Spi1/2/3+1), DMA regs 0xC60204xx |
| **0xC6030000–0xC603FFFF** | **0x20200000–0x2020FFFF** | **GPIO register file + system clock/power/security** |

The clock-gate dispatcher at 0x808167F4 additionally proves that 0x20300000 and
0x20400000 are distinct peripheral bases (GPIO-gate masks loaded from offsets
+0x4C/+0x6C/+0x7C of its config object), so 0x20200000 is *a* base, not the
whole aperture. CONFIRMED.

---

## 2. Corrections to previous documentation

| Old claim (docs/gpio-register-discovery.md, rom-dump-analysis.md) | Status | What is actually there |
|---|---|---|
| "GPIO init cluster ROM 0x14C800–0x14E200, 100+ GPIO base refs" | **WRONG** | The words at 0x14C8xx–0x14E2xx are `0x20202020` = four ASCII **spaces** from string/fill data. Real GPIO references: 6 literals (§1). |
| "Byte-granularity register offsets 0x31–0x36, 0x43, 0x48, 0x54, 0x64, 0x6B, 0x6C, 0x6E, 0x72, 0x74, 0x78" | **WRONG** | Derived from the same ASCII cluster / unrelated struct offsets. No such GPIO register offsets exist. The real register map is in §3. |
| "Kernel VA 0xC8004000–0xC8006FFF = GPIO peripheral window" | **WRONG** | 0xC8004xxx/0xC8006xxx are **kernel/driver RAM** (channel structs, pin records, dispatch tables, bitmaps). No MMIO is ever stored there via literals. The MMIO window is **0xC60xxxxx**. |
| "GPIO driver 0xC0C00–0xC1900 uses 0xC8004150/0xC8004428 registers" | misread | 0xC8004150 = array of 7 channel structs (0x68 B each); 0xC8004428 = 7 × pin-record arrays (bank*0x1000 + pin*0x20). They *contain* pointers to the real registers. |
| "BCM2835-compatible GPFSEL layout" | unsupported | No GPFSEL-style registers are touched by Symbian. Custom bank-trio layout (§3). A pre-boot pad-function grid does exist in hardware (phys 0x20201810–0x20201960, `docs/genio-padtable.md`) but the ROM never writes it. |

---

## 3. GPIO register file map (CONFIRMED)

Decoded from the channel-init function **ROM 0x800C0E14** (ARM code, 7-way jump
table at 0x800C0E20). Each bank has a 0x68-byte channel struct in kernel RAM
(0xC8004150 + bank*0x68) whose fields are absolute register VAs; the table below
reverses that mapping. Address formula gives the VA; physical = VA − 0xC6030000
+ 0x20200000 (INFERRED page mapping, §1.1).

Per-bank register slots (N = bank 0..4):

| Register | VA (bank N) | access pattern | writer function |
|----------|-------------|----------------|-----------------|
| DATIN    | 0xC60319F0 + N*0x10 | read 32-bit input levels | read @ 0x800C0C0C |
| DATSET   | 0xC60319F4 + N*0x10 | write `1<<pin` sets output | 0x800C0C14 (SetPinDataOut) |
| DATCLR   | 0xC60319F8 + N*0x10 | write `1<<pin` clears output | 0x800C0C48 (ClrPinDataOut) |
| WAKESTAT | 0xC6031990 + N*0x10 | read wake status | read @ 0x800C0CD4 |
| WAKESET  | 0xC6031994 + N*0x10 | write `1<<pin` enables wake | 0x800C0CE4 (on=1 path) |
| WAKECLR  | 0xC6031998 + N*0x10 | write `1<<pin` disables wake | 0x800C0CE4 (on=0 path) |
| EDGESET  | 0xC6031A58 + N*0x10 | **write-only** `1<<pin` (also used by MMC/USB drivers, see §4) | 0x800C0D38 (arg≠0) |
| EDGECLR  | 0xC6031A54 + N*0x10 | **write-only** `1<<pin` | 0x800C0D38 (arg=0) |

**EDGE pair is write-only across the entire ROM** (2026-08-22 census,
`tools/edge_census.py`): 6 verified literal xrefs, all stores (4 unique
sites: 2×MMC EDGECLR, 1×MMC EDGESET, 2×USB), zero loads — the one apparent
read at 0x80161d88 is a tracker false positive (r1 clobbered by
`ldr r1,[r0]` at 0x80161d84 before `ldr r2,[r1]`, which reads an object,
not MMIO). The three GPIO API functions (0x800C0C0C/0C14/0D38) have no
direct BL callers — they are dispatched via function tables.

IRQ/direction/pull block per bank — VA 0xC6033400 + N*0x40 (16 registers, Q+0x00…Q+0x3C).
The whole block is now CONFIRMED against the driver's channel structs
(0xC8004150 + bank*0x68, init jump table 0x800C0E14) and implemented as
a Linux gpio irqchip + QEMU model — task 099, docs/bcm2727-gpio-irq.md:

| Offset | behavior (CONFIRMED store pattern) | semantic (INFERRED) |
|--------|-------------------------------------|---------------------|
| Q+0x04 | `1` written on SetPinDirection(out), `0` on (in) | output-enable gate / bank mode |
| Q+0x08 | `1<<pin` via 0x800C0BE0 | interrupt mask set |
| Q+0x0C | read (0x800C0BD8) | mask/status readback |
| Q+0x10 | `1<<pin` on direction=out (0x800C0B30) | direction SET |
| Q+0x14 | `1<<pin` on direction=in; mask written in suspend path 0x800B8B4C…; completion polled at Q+0x3C | direction CLR / irq flush cmd |
| Q+0x24 | 16-bit read (0x800C0E04) | pending status |
| Q+0x28 | `1<<pin` via 0x800C0D8C (arg2==1) | int-enable SET (edge A) |
| Q+0x2C | `1<<pin` via 0x800C0D8C (arg2==0) | int-enable CLR (edge A) |
| Q+0x34 | `1<<pin` via 0x800C0D8C (arg3==2) | int-enable CLR (edge B) |
| Q+0x38 | `1<<pin` via 0x800C0D8C (arg3==1) | int-enable SET (edge B) |
| Q+0x3C | polled for −1 after writes (suspend) | command ack |

Banks 5 and 6 are **wake-only**: DATIN/SET/CLR, EDGE and IRQ fields are 0xFFFFFFFF
(= unsupported, checked with `cmn rX,#1` before every use). Their wake trios:
bank5 = 0xC60319E0/E4/E8, bank6 = 0xC6031AB0/B4/B8. CONFIRMED (jump-table cases
5/6 at ROM 0x800C1204 / 0x800C1228).

Everything is reached by `str value, [channel->field_reg + channel->base(=0)]`;
the `-1` sentinel guards unimplemented banks.

### 3.1 What is in the same 64 KB VA page but is NOT GPIO

The "system page" 0xC6030000 (phys 0x20200000) mixes GPIO with other blocks —
useful for the future Linux platform driver:

| offset | user (ROM driver) | notes |
|--------|-------------------|-------|
| +0x0300 | security/TrustZone drivers (0x800E8BA0, 0x800EB100, 0x800ED660, 0x801411E8): store 0xA | SPI×3 + security write same reg — global module enable? |
| +0x0704/0x0708 | interrupt ASSP (0x800B81C0…) | **DECODED 2026-08-22 as the IFC group-interrupt machinery** (modem GENIO handshake domain, 16-wide groups) — bcm2727-icu-timer.md §2.1. Not the CPU ICU. |
| +0x1868/1870/187C | I2C drivers (0x801C4B64…): stores 0x60000000, 0x10000000, 6, 0x600 | I2C module config/clock |
| +0x1948/1958 | VideoCore driver: stores 0x77 … | VC interface |
| +0x2200–2234, +0x2604–2644 | HwClkControl (PowerResourceManager): halfword/word clock gates | **clock controller** |
| +0x3004/+0x3090/+0x3114/+0x3120/+0x3130 | timer PSL (0x800B8750/0x800B8828/0x800B88C4) + bootstrap (0x80001E60) | **timer clock-class selects + fast-counter capability flags + secure handshake** (bcm2727-icu-timer.md §3.2/§4) |
| +0x3008, +0x3178 | VideoCore driver | VC image/boot regs |
| +0x3300–3358 | kernel/ASSP + GPIO driver (0x800CB2E8) | **DECODED 2026-08-22: GPIO-mux pending/ack pair at +0x3350/+0x3354, dispatched chained off the ICU level-3 path** — bcm2727-icu-timer.md §5 |
| +0x3604–3630, +0x37A4+ | interrupt ASSP (0x800B8DC8/0x800B8D5C) | **DECODED 2026-08-22: OS timer block (enable/mode/reload pair, IRQ id 0xF2 in the ICU @0x20400000)** — bcm2727-icu-timer.md §3 |
| +0x4000–42C8 | VideoCore driver | VC core regs |
| +0x8214, +0x8800+ | display/USB/etc (byte-wide config) | misc |

---

## 4. Every GPIO-relevant store found in the ROM (CONFIRMED)

### 4.1 Direct literal-address stores (absolute confidence)

| code va | target VA | target (GPIO offset) | value | driver / context |
|---------|-----------|----------------------|-------|------------------|
| 0x80189BD0 | 0xC6031A94 | EDGECLR bank4 | **0x1E** (pins bank4+1..4) | USB PSL (`DUsbIdVbusStateMachine` area) — after `bl 0x80173124` |
| 0x801895D4 | 0xC6031A98 | EDGESET bank4 | **0x1E** (pins bank4+1..4) | USB PSL |
| 0x80161AE0 | 0xC6031A64 | EDGECLR bank1 | **0x7C000000** (pins bank1+26..30 = global 58–62) | MMC stack platform (power-down path, stack==1) |
| 0x80161B5C | 0xC6031A64 | EDGECLR bank1 | **0x7C000000** | MMC stack platform |
| 0x80161D50 | 0xC6031A68 | EDGESET bank1 | **0x7C000000** | MMC stack platform (power-up path) |
| 0x8032F3A4 | 0xC60319E4 | WAKESET bank5 | **0x100** (pin 8) | VideoCore driver (DVCDriver, `/Z:/vlls/bcm2727b1/`) |
| 0x8032EA14 | 0xC60319E8 | WAKECLR bank5 | **0x80** (pin 7) | VideoCore driver |
| 0x8032EAF0 | 0xC60319E8 | WAKECLR bank5 | **0x100** (pin 8) | VideoCore driver |
| 0x800B8B50 | 0xC6033414 | IRQ bank0 +0x14 | mask (r2, dynamic) then poll +0x28 | suspend path ("SuspendInt") |
| 0x800B8B68 | 0xC6033454 | IRQ bank1 +0x14 | mask, poll | suspend path |
| 0x800B8B80 | 0xC6033494 | IRQ bank2 +0x14 | mask, poll | suspend path |
| 0x800B8B98 | 0xC60334D4 | IRQ bank3 +0x14 | mask, poll | suspend path |
| 0x800B8BB0 | 0xC6033514 | IRQ bank4 +0x14 | mask, poll | suspend path |

The MMC mask is the strongest pin-identification cross-check in the whole ROM:
bits 26–30 of bank 1 = **global GPIO pins 58–62**, and `docs/gpio-map.md` places
**GPIO59 = VMEM2_EN** (eMMC power boost regulator) — an MMC-stack driver toggling
exactly the memory-power pins. This validates both the bank math (bank = pin>>5)
and the bank1 EDGE/OUT slot semantics (set/clr of output latches).

### 4.2 Driver-indirect stores (via channel struct — pattern CONFIRMED, values runtime)

All of these write `1<<pin` (single-bit) masks; there are no literal value stores:

| function | writes | API meaning (INFERRED names) |
|----------|--------|------------------------------|
| 0x800C0C14 | DATSET[bank] ← 1<<pin | SetPinData |
| 0x800C0C48 | DATCLR[bank] ← 1<<pin | ClrPinData |
| 0x800C0C0C | reads DATIN[bank] | GetPinData |
| 0x800C0B30 | IRQ+0x04 ← 1/0; IRQ+0x10 ← 1<<pin (out) / IRQ+0x14 ← 1<<pin (in) | SetPinDirection |
| 0x800C0BE0 | IRQ+0x08 ← 1<<pin | (irq mask set) |
| 0x800C0CE4 | WAKESET/WAKECLR ← 1<<pin | EnablePinWakeup |
| 0x800C0D38 | EDGESET/EDGECLR ← 1<<pin | EnablePinEdgeDetect |
| 0x800C0D8C | IRQ+0x28/2C/34/38 ← 1<<pin | SetPinInterruptMode |
| 0x800C0E04 | reads IRQ+0x24 (16-bit) | ISR pending |
| **0x800C19C4 → 0x800C0B24** | **NOTHING — `bx lr`** | **SetPinMode: no-op stub** |

The mode argument of the pin API is stored only in the software pin record
(0x800C18E0/0x800C1900 pack `arg>>8` = pin, `arg>>13` = mode into the record at
0xC8004428 + bank*0x1000 + pin*0x20); it never reaches hardware. CONFIRMED.

### 4.3 Functions writing sequences to consecutive GPIO offsets

None exist for GPIO. The one apparent case (0x80161DE0–0x80161E40, "13 consecutive
stores") turned out to write a **RAM** descriptor list at 0xC800D76C, not GPIO —
caught by checking the actual literal value at each pool load. The only
consecutive-offset MMIO writes in the GPIO page are the clock-gate writes at
+0x2208…+0x2234 (I2C/clock, halfword) — not pinmux.

---

## 5. Pin numbering (CONFIRMED data)

* ROM table at **0x800C1BFC**: 88 entries `{kernel_id, pin_in_bank, bank}`,
  terminated by 0xFFFFFFFF. Kernel GPIO resource ids run **0x13C–0x193**
  (316–403); bank0 = 32 pins (ids 0x13C+), bank1 = 32, bank2 = 24. No entries
  for banks 3–6 in the GPIO-API namespace.
* Pin validity check in the API: `id < 0x213` (531). Pin argument encoding in
  several entry points is `pin<<8 | flags`.
* Remap quirk (0x800C1900): incoming pin numbers **≥ 0x6A (106) are reduced by
  0x12 (18)** before bank/pin-in-bank decomposition. INFERRED: two numbering
  domains (schematic/genio numbers vs internal).
* 7 hardware banks exist (channel init), but only banks 0–2 are exposed through
  the kernel GPIO-API table; drivers that need banks 3/4 touch registers
  directly (USB uses bank4, MMC uses bank1 — both CONFIRMED by §4.1).
* Named (modem handshake) pins — the "GENIO" modem pins. The modem-adaptation
  GPIO binding table (0xC80040B0/0xC80040B4, 37 slots, loop at ROM 0x800BCB3C)
  uses these names (strings at 0x800BFEB8+):
  `DSP0, SUSD, PMM_CONF, PMM, UPDAPP, SOS-USER, KEYS, SOS+ISASW, MCUSW` …
  (RAPUYAMA CMT handshake signals; `DPowerGenioCtrlClock` is their clock
  resource). CONFIRMED strings; the full 37-name list is in RAM at boot.

### 5.1 Best-effort pin table (Linux pinctrl equivalent)

There is **no per-pin mux value to port**. The only statically-known pin-level
facts from the ROM:

| pins (bank.bit) | global # | driven by | value | meaning |
|-----------------|----------|-----------|-------|---------|
| bank1.26–30 | 58–62 | MMC platform driver | 0x7C000000 set or clr | eMMC/memory power rails (GPIO59=VMEM2_EN per schematic) |
| bank4.1–4 | 129–132 | USB PSL | 0x1E set or clr | USB OTG ID/VBUS-domain pins |
| bank5.7 | wake-domain | VideoCore driver | 0x80 (clr) | VC wake handshake |
| bank5.8 | wake-domain | VideoCore driver | 0x100 (set/clr) | VC wake handshake |

Everything else (direction/output/irq/wake) is runtime policy, decided by
whatever opened the pin — not a boot-time table.

---

## 6. How values reach the hardware

* **Direct MMIO.** Physical 0x20200000 (+ window to 0x20400000) is registered
  with the kernel resource manager at boot (§1), mapped into the fixed kernel
  VA window at 0xC603xxxx, and every access is a plain `ldr`/`str` — no
  mailbox, no VideoCore command, no TrustZone call for GPIO banks 0–4.
  CONFIRMED.
* The **VideoCore driver** also does direct MMIO to GPIO bank5 wake registers
  (and to system-page +0x3008/+0x3178/+0x4000+); no VCHI message touches GPIO.
* The security drivers use system-page +0x0300 directly.
* Nothing in the ROM writes GPIO via the modem/ISA path.

Therefore: on this phone, whatever "pinmux" exists must be performed by
**earlier boot stages** (NLoader, GENIO_INIT, SWBL, TrustZone Secure-World
firmware, or the VideoCore ROM/second-stage loader). The ROM dump can neither
confirm nor deny those; they live in the BB5-encrypted GENIO_INIT / X-LOADER
images per `docs/bcm2727-gpio-research.md`.

---

## 7. Method / reproducibility

`python3 tools/gpio_static.py firmware-re/dumps/romdumpplus.dmp` prints:
1. all literals in the GPIO phys / VC / 0xC603xxxx / 0xC8004xxx windows with
   verified ARM+Thumb `LDR [pc,#imm]` xrefs (ASCII false positives tagged);
2. every literal→store dataflow event into the 0xC603 window with values;
3. the decoded 7-bank channel register map (from ROM 0x800C0E14);
4. the pin id table decode (ROM 0x800C1BFC).

`--func/--track/--start/--mode` options disassemble arbitrary functions.
Requires capstone (already installed).

Key code locations (file offset = va − 0x80000000):

| va | what |
|----|------|
| 0x80885304, 0x80886D9A, 0x808893C4, 0x8088E284, 0x8088F016 | Thumb fns registering GPIO phys range (§1) |
| 0x8088B8CC | kernel resource registrar callee |
| 0x808167F4 | clock-gate dispatch (0x20200000/0x20300000/0x20400000 as peripheral ids) |
| 0x800C0E14 | GPIO channel init (7-bank jump table → register map) |
| 0x800C1300 / 0x800C1330 / 0x800C1344 / 0x800C1434 | channel/pin-record array init |
| 0x800C0B24 | **SetPinMode hardware stub (`bx lr`)** |
| 0x800C0B30…0x800C0E10 | direction/data/int/wake/edge store functions |
| 0x800C1BFC | pin id table (88 entries) |
| 0x800BCB3C | 37-slot named-pin binding (modem GENIO) |
| 0x800B8B4C…0x800B8BC4 | suspend: GPIO IRQ flush (bank0–4 Q+0x14) |

---

## 8. Open questions, ranked

1. ~~**Who programs pin functions before Symbian?**~~ **ANSWERED 2026-08-22.**
   The applied pad-function table exists in plaintext in the OneNAND image
   at 0x1C5000 (factory data region): 176 pads × function codes 0–4 in a
   22×8 register grid, plus per-bank boolean rails — decoded and saved as
   `docs/genio-padtable.md` + `firmware-re/analysis/genio-padtable.{txt,tsv}`.
   GENIO_INIT/IVE3A/IVE3B remain BB5-encrypted (DES key 416ce871c3bd0a28
   verified NOT to apply); PRIMAPP/RAP3NAND are plaintext ARM boot stages
   (container magic 0x809795A3) but carry no pad table. Remaining sub-questions:
   function-code names, pad-numbering correlation (genio-padtable.md §5).
2. ~~**Exact semantics of the EDGE/OUT set/clr pair (0xC6031A54+0x10*N).**~~
   **NARROWED (2026-08-22), deferred to Linux runtime.** Full-ROM census
   (`tools/edge_census.py`): write-only, never read anywhere in the ROM.
   Naming (edge-detect enable vs output latch vs direction latch) is
   statically undecidable: the per-pin boolean kernel API (0x800C0D38)
   looks enable-class, but the MMC site writes a fixed group mask
   0x7C000000 (pins 58–62 = SleepX/VMEM2_EN/PM area, incl. the OUTPUT
   VMEM2_EN) on eMMC-stack power-up/down — power-rail-latch-like. Runtime
   read from Symbian user mode is impossible (data abort, rc=3, verified
   2026-08-22 on 0xC60319F0/0xC6031A54/0xC6031A64); once Linux boots on
   bare metal the register is trivially readable — implement the pair in
   the pinctrl driver as a write-only secondary set/clr with semantics
   flagged TODO-FOR-LINUX-RUNTIME.
3. **VA→phys mapping of the 0xC60xxxxx window.** **LARGELY ANSWERED
   2026-08-22** (docs/bcm2727-icu-timer.md §4): the kernel bootstrap
   carries an early-mapping table at ROM 0x800020A0 (duplicated at
   0x80001DF4) of (PTE|attrs, VA) pairs — e.g. VA 0xC6060000 ← PA
   0x20400000 (the ICU), VA 0xC6E00000 ← PA 0x20500000, VA 0xC6050000 ←
   PA 0x40000000. The system page 0xC6030000 is NOT in that table (it is
   resource-manager mapped later), so the linear **0xC6030000+off ↔
   0x20200000+off assumption stands but remains INFERRED**; verification
   path unchanged (Linux bare-metal read, or tracing the kernel
   page-table code statically).
4. **The full 37-name modem GENIO pin table** (only 9 names are in static
   data; the rest are populated from elsewhere at boot — likely from the
   modem adaptation ROM area or a variant DLL).
5. **Bank 3 pins (96–127) and banks 5/6 wake pin mapping** to package balls —
   bank3 has full data registers but no kernel-API ids; who uses it?
   (Candidates: touch controller ints GPIO122/123 in schematic numbering —
   unproven.)
6. Whether the `0x6A−0x12` pin remap corresponds to the schematic genio
   numbering (i.e. schematic GPIO122/123 → internal 104/105 → bank3.8/9)
   — would reconcile the schematic pin names with the driver's bank3.

---

*Cross-references: docs/gpio-register-discovery.md (superseded by this file for
the register map), docs/gpio-map.md (schematic pin list), docs/bcm2727-gpio-research.md
(BCM2835 comparison — now largely obsolete), docs/rom-dump-analysis.md (driver
inventory).*
