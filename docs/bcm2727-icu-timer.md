# BCM2727B1 Interrupt Controller (ICU) + OS Timer — Static ROM Decode

**Date:** 2026-08-22
**Source:** 100% offline static analysis of `firmware-re/dumps/romdumpplus.dmp`
(Symbian ROM, virtual base 0x80000000, file offset = vaddr − 0x80000000).
**Tool:** `tools/gpio_static.py` (literal/xref/dataflow tracker) + raw pool
dumps via capstone.
**Confidence legend:** CONFIRMED = decoded instruction sequence actually
present in the ROM. INFERRED = interpretation of a CONFIRMED structure.

**Validated the same day:** the Linux drivers built from this decode
(`irq-bcm2727-icu.c`, `timer-bcm2727.c`) boot through the QEMU NLoader chain
with a self-calibrating delay loop — the timer→ICU→CPU interrupt path works
end to end (§6).

---

## 0. Executive summary

1. The CPU interrupt controller ("ICU") sits at **phys 0x20400000**
   (kernel VA 0xC6060000). It is a custom Broadcom block with a flat
   **421-source id space (0..0x1A4)**, a select/priority mailbox
   (mask/unmask), a **one-at-a-time pending-id readout**, and
   **EOI-by-id-writeback**. CONFIRMED.
2. The Symbian tick timer fires **interrupt id 0xF2 (242)**, reloads
   38400 = 0x9600, and its registers live in the system block at
   +0x3600-0x37FF (down-counter pair + reload registers). CONFIRMED.
3. The kernel bootstrap ROM data contains the **locked-TLB / early-mapping
   table** (VA←PA pairs at ROM 0x800020A0, duplicated at 0x80001DF4) —
   this pins the ICU phys base and reveals the wider SoC memory map
   (§4). CONFIRMED (static data; PA mapping semantics INFERRED).
4. GPIO interrupts do NOT enter the ICU as bank ids: a **GPIO-mux chained
   dispatcher** is invoked from the ICU's third dispatch level with the mux
   status as argument; the mux pending/ack pair sits at sys-page +0x3350/
   +0x3354. CONFIRMED.
5. The tick rate is **200 MHz-class**: reload 38400 × 5 ns = 192000 ns =
   exactly the `ENanoTickPeriod` the live system reports. CONFIRMED as an
   arithmetic identity; the clock tree itself is INFERRED.

---

## 1. The ASSP interrupt driver (ROM 0x800B7C00-0x800B9E00, ARM code)

The Symbian kernel's interrupt+timer PSL ("interrupt ASSP"). Key functions:

| address | role (INFERRED names) | evidence |
|---|---|---|
| 0x800B7E40 | **IRQ dispatcher** | full-context save, 3-level dispatch (below) |
| 0x800B89A4 | default handler → prints "SpuriousInt" (string @0x800B8BC8) | |
| 0x800B89B0 | **BindInterrupt(id, handler, arg)** | writes software table + select/cmd 2 |
| 0x800B8A18 | UnbindInterrupt(id) | software table only — no MMIO! |
| 0x800B8AC0 | SetInterruptPriority(id, prio<8) | select + write prio to +0x24 |
| 0x800B8C00 | **ICU init** | fills table, enables, flushes |
| 0x800B8D5C | **tick timer ISR** | reload 0x9600 + EOI 0xF2 |
| 0x800B8DC8 | **timer block init** | writes +0x3600/+0x3780 blocks |
| 0x800B8E54 | PSL init: publishes HAL attrs, binds 0xF2 | calls 0x800B8C00/0x800B8DC8 |

Software tables (kernel RAM, phys 0x20033340 for the first — the secure
side reads it from there):
- **0xC8003340**: 421 × 8-byte entries `{flags/arg, handler}`, default
  handler 0x800B89A4.
- **0xC8004068+0x10** (=0xC8004078): the GPIO-mux chained handler pointer
  (the dispatcher's level-3 load `ldr sb, [r8, #0x10]` reads from
  **0xC8004068** — pool word at ROM 0x800B7E30 — NOT from the ICU; an
  earlier reading of this session had it as ICU +0x10 and was wrong).
- Timer PSL housekeeping arrays: 0xC8003210+0xC (timer objects,
  initialized from the table at ROM 0x800BC7D0), 0xC8003228 (18 entries
  {1, -1}), 0xC80032B8.

### 1.1 The three dispatch levels (dispatcher at 0x800B7E40)

```
on IRQ entry (full context save):
  r1 = [0xC606000C]                  ; level 1: hardware-vectored handler
  if r1 != 0: call r1                ; VIC-style; reads 0 on this SoC
  r0 = [0xC6060048]                  ; level 2: pending interrupt id
  if r0 != -1:
      {arg, handler} = [0xC8003340 + r0*8]
      call handler(arg)              ; handler EOIs the ICU itself
      re-read [0xC6060048]           ; loop until -1
  r0 = [0xC6060040]                  ; level 3: GPIO-mux status
  sb = [0xC8004078]                  ; chained handler (driver RAM)
  if sb != 0: call sb(r0)            ; GPIO mux dispatch
  return from exception
```

CONFIRMED (instruction sequence at 0x800B7E40-0x800B7ED4).

### 1.2 Bind/unbind/priority — the programming mailbox

```
BindInterrupt(id, handler, arg):          0x800B89B0
    table[id] = {arg, handler}
    [0xC6060020] = id                     ; SELECT
    [0xC6060024] = 2                      ; PRIORITY = 2 (enabled)

SetInterruptPriority(id, p):              0x800B8AC0 (p < 8 checked)
    [0xC6060020] = id
    [0xC6060024] = p

tick ISR (id 0xF2):                       0x800B8D5C
    [0xC6060010] = 0xF2                   ; command port (re-arm?)
    [0xC60337A8] = 0x9600                 ; timer reload (see §3)
    r0 = [0xC6060080]                     ; read (discarded?)
    [0xC606000C] = 0xF2                   ; EOI by id writeback
```

From this, +0x24 is a per-irq **priority register** where 0 = masked
(INFERRED: unbind never touches the ICU, so "disabled" must be the
priority-0 state; the bind value is 2).

### 1.3 ICU init (0x800B8C00)

```
[0xC6060004] = 1        ; global enable
[0xC606001C] = 0
[0xC6060010] = -1       ; command-port flush
[0xC6060084] = 5
[0xC6060090] = 0 ; [0xC606009C] = 0
then: [0xC6060010] = -1 and poll [0xC6060008] until -1   (0x800B8B18)
      [0xC6060050] = -1 and poll [0xC6060048] until -1
      GPIO bank flushes: sys-page 0x3400+0x40N +0x14 = -1, poll +0x28
```

## 2. ICU register map (phys 0x20400000)

| offset | access | function | status |
|---|---|---|---|
| +0x04 | W | global enable (1) | CONFIRMED |
| +0x08 | R | poll status (flush handshake; -1 = done) | observed |
| +0x0C | R | **vectored handler address** (0 = none; VIC-style) | CONFIRMED |
| +0x0C | W | **EOI** — write the serviced id | CONFIRMED |
| +0x10 | W | command port (timer ISR writes its id; init writes -1) | CONFIRMED stores, semantics open |
| +0x1C | W | zeroed at init | CONFIRMED store |
| +0x20 | W | **interrupt id select** | CONFIRMED |
| +0x24 | W | **priority of selected id** (0 masked / 1-7) | CONFIRMED |
| +0x40 | R | GPIO-mux chained status (arg to level-3 handler) | CONFIRMED (bit layout = model contract, task 099) |
| +0x48 | R | **pending interrupt id** (-1 = none) | CONFIRMED |
| +0x50 | W | flush (write -1, then poll +0x48) | CONFIRMED |
| +0x80 | R | read by tick ISR (value unused?) | observed |
| +0x84 | W | = 5 at init | CONFIRMED store |
| +0x88/+0x90/+0x9C | W/R | zeroed at init; +0x9C read as "more pending" flag | CONFIRMED stores |

Service model (as implemented in Linux + QEMU): read +0x48 returns the
highest-priority enabled pending id and makes it in-service; write that id
to +0x0C to complete it so the next can surface.

**Interrupt id space:** 0..0x1A4 (421). Known residents:
- **0xF2 (242) = system tick timer** (CONFIRMED, bound by PSL init)
- **0x13C-0x193 (316-403) = kernel GPIO pin resource ids** (the pin table
  at ROM 0x800C1BFC, docs/gpio-static-pinmux.md §5) — **NOT ICU ids**
  (task 099: GPIO events ride the level-3 mux, §5); the numbers are
  kernel resource ids only.

Caveat: the "IFC" group-interrupt machinery at sys-page +0x704-0x73C is a
separate mechanism — see §2.1.

### 2.1 IFC group-interrupt machinery (sys-page +0x704-0x73C, partially decoded)

The same driver also drives a second interrupt-like block through the
sys page (VA 0xC60307xx). Functions 0x800B8414 (bind one source) and
0x800B8494 (read-modify-write a mask, 16 sources per group):

```
[0xC6030704] = 1                        ; access gate (0 after)
[0xC603072C] = width - 1                ; priority (width = 0x10 = 16)
[0xC6030718] = 0x60 | ch                ; group select (bind path)
[0xC603071C] = 0x40 | ch                ; group select (mask path)
[0xC6030720] = new 16-bit mask          ; RMW of current mask
[0xC603073C] = 1                        ; latch/commit
[0xC6030704] = 0
```

driven by a **descriptor table at ROM 0x800BC268**: 16-byte entries
`{[+0]=1 valid, [+4]=?, [+8]=channel, [+0xC]=0x10 width}`; entry 0 is all
zero; observed channels 0,1,2,3,4,5,6,8,9,0xA,0xD,0xE,0xF,0x10,0x11…
(37-ish entries — matches the 37-slot modem GENIO named-pin domain,
docs/gpio-static-pinmux.md §5). This is the RAPUYAMA CMT handshake
interrupt framework, NOT the CPU ICU; Linux bring-up does not need it,
but the register protocol above is the decode record.

**Phys base hypothesis (INFERRED):** the bootstrap scattered-register
table at ROM 0x80001DF4 (§4) pairs `0x20500100 + 0x708`, `0x20500104`,
`0x20500000 + 0x71C/0x738/0x73C` — the same offsets. Since the locked-TLB
table maps VA **0xC6E00000 ← PA 0x20500000**, the IFC block most likely
lives at phys 0x20500000 (accessed by Symbian via 0xC6E007xx in the
paths that use absolute addresses, and via 0xC60307xx where the sys-page
alias is used). Unreconciled: why the driver's literal pools address
0xC6030704-0xC603073C while the bootstrap table references 0x20500xxx
for the same offsets — two instances, or a VA alias of one block.

## 3. Timer blocks

### 3.1 Tick timer (system block, phys 0x20203600-0x202037FF)

| offset (VA 0xC603xxxx) | function | status |
|---|---|---|
| +0x3604 | enable (write 1) | CONFIRMED |
| +0x3608 | mode (3 on T0, 6 on the +0x10180 block) | CONFIRMED |
| +0x360C/+0x3610 | aux config (0x493 / 0xE0000000 at init) | CONFIRMED stores |
| +0x3620-0x3630 | zeroed at init (counters) | CONFIRMED |
| +0x3644 | **free-running counter** (Symbian fast-counter reads it twice — latch semantics) | CONFIRMED address |
| +0x3784 | enable (1) | CONFIRMED |
| +0x378C/+0x3790 | aux config (0x493 / 0xE0000000) | CONFIRMED stores |
| **+0x37A4** | reload (0x9600 written by reprogram fn 0x800B9990) | CONFIRMED |
| **+0x37A8** | reload/count (0x9600 written by the ISR every tick) | CONFIRMED |

A second timer block exists one page up (VA 0xC6040180: enable=1, mode=6)
— unused by the Linux bring-up so far.

### 3.2 Fast-counter environment (function 0x800B8750, CONFIRMED)

The Symbian fast-counter read checks two capability flags before using
the +0x3644 counter:

```
if ([0xC6033090] & 0x10) and ([0xC6033130] & 0x4000):
    r1 = [0xC6033644]              ; read #1
    r0 = [0xC6033644]              ; read #2 (latch semantics?)
    use r1 bits [20:16] and r0 bits [4:0], scaled by 0x0249F000
    (38399488 ≈ 38.4 MHz) via umull/mla — unit conversion
else:
    fallback path reads [0xC6033120]  (0x800B87C4, not fully decoded)
```

Both the double-read-latch protocol and the 38.4 MHz scale factor are
why the Linux/QEMU +0x44 "simple 32-bit up-counter" reading is flagged
TODO-FOR-HARDWARE. The clock-class attribute queries live at
0x800B8828 ([0xC6033004] bits 7:6 → {1,3,4,6}) and 0x800B88C4
(bits 12:11 → {1,6,8,12}); results are published as HAL attributes
0x88002A/0x88002B/0x88002C via the 0x880814 group.

### 3.3 Clock rate (INFERRED, arithmetically pinned)

- Live system reports `ENanoTickPeriod = 192000` ns.
- Tick reload = 38400 = 0x9600 ⇒ input clock = 38400 / 192000 ns = 200 MHz.
- Corroboration: [0xC6033004] bits 7:6 select a multiplier from {1,3,4,6}
  and bits 12:11 from {1,6,8,12} (HAL attribute queries 0x800B8828 /
  0x800B88C4) — consistent with a 33.33 MHz × {1,3,4,6} tree hitting
  200 MHz at the top setting.
- The Linux DTS/QEMU model use 200 MHz (`clock-frequency` property).

## 4. Kernel early mapping table (ROM 0x800020A0, dup at 0x80001DF4)

Static (PTE|attrs, VA) word pairs parsed from the bootstrap data:

| VA | PA | note |
|----|----|------|
| 0xC6010000 | 0x09000000 | |
| 0xC6011000 | 0x08000000 | |
| 0xC6012000 | 0x20010000 | |
| 0xC6018000 | 0x30000000 | PDRAM |
| 0xC6020000 | 0x20020000 | |
| **0xC6060000** | **0x20400000** | **ICU** |
| 0xC6E00000 | 0x20500000 | (IFC domain per the §0.4 caveat table at 0x80001DF4) |
| 0xC6050000 | 0x40000000 | handshake regs (bootstrap polls +0x14) |
| 0xC60A0000 | 0x20600000 | |
| 0xC60A1000 | 0x20700000 | |
| 0xC60AF000 | 0x20680000 | |
| 0xC6070000 | 0xFFFD0000 | wrap-around alias? |
| 0xC6F00000 | 0x40100000 | **XTI/STI trace engine** (task 095: mailbox SRAM + doorbell 0x401FD800; used by SWBL/NLoader/kernel — real hardware, docs/bcm2727-xti-trace.md) |

The system page VA 0xC6030000 is NOT in this table (mapped later by the
resource manager); its linear PA assignment **0x20200000+off** stays as
established in docs/gpio-static-pinmux.md §1.1.

**Bootstrap handshake observations (0x80001E60-0x80001F90, CONFIRMED
stores/polls):** at reset the kernel writes [0xC6033114] = 1, then = 2
(sys-page +0x3114), polls [0xC6050014] (PA 0x40000000+0x14) for bit 1
with a 0xFA0-iteration timeout, and cache-flushes around it — a
secure-side/PSL handshake using the PA 0x40000000 block. The scattered
blob at ROM 0x80001DF4 reads, in order:
`0x30003810, 0x30003f00, 0x30003804` (PDRAM config-block addresses —
matches NLoader's 0x30003e00 config block region),
`0x20033340` (handler table PA), `0x20030300` (security regs PA),
`0x20500100, 0x708, 0x718, 0x20500104, 0, 0, 0, 0x738, 0, 0x73c, 0, 0,
0x20500000, 0, 0x2ff000, 0x09080004, 0x71c` (IFC references, §2.1).
Evidence that PA 0x2003xxxx is an SRAM block aliasing kernel VA
0xC8003xxx/0xC6030xxx contents (INFERRED).

## 5. GPIO mux (the level-3 path)

- Pending read: **[0xC6033350]**; ack: **[0xC6033354] = 0** (GPIO driver
  fn 0x800CB2E8 — CONFIRMED; same pair in the kernel idle path
  0x8005C41C and the bootstrap 0x80002640 via the PA alias).
- Kernel idle path (0x8005C41C) also writes 0xC6033354 (wake ack) and
  reads 0xC6033350 (wake reason).
- The GPIO driver keeps a priority-ordered linked list of pin handlers
  (0x800CB3C8 insert-by-key).
- **DECODED 2026-08-22 (task 099, `docs/bcm2727-gpio-irq.md`):** the
  mux does NOT map into the ICU id space — GPIO events assert the CPU
  IRQ directly with the bank bitmap at ICU +0x40, dispatched chained
  off the level-3 tail (every IRQ). Linux gpio irqchip shipped and
  chain-validated; the register semantics beyond the Symbian-confirmed
  ones are a documented model contract (§3 there). This also retires
  the "+0x13C-0x193 pin resource ids are ICU ids" hypothesis — kernel
  resource ids, not ICU ids.

### 5.1 Undecoded sites recorded as leads (verified literal xrefs, semantics open)

| register (VA) | writer sites | guess |
|---|---|---|
| 0xC60336C4 | 0x80103BE8, 0x801470F0, 0x80147364, 0x80148658 (pools 0x80103CBC/0x80147524/0x80148A3C) | watchdog / second timer ack? |
| 0xC6033630 | 0x80149DB0 (dynamic store) | timer-adjacent |
| 0xC6033600 | 0x8014A378 (pool) + 0x800B8DC8 | same timer block, other accessor (variant DLL) |
| 0xC6033358 | 0x80146F98 (dynamic) | GPIO-mux adjacent (idle/wake domain?) |
| 0xC6033354/0xC6033350 | 0x8014A618/0x8014A64C, 0x80002654 (pool 0x800026B4) | GPIO mux + idle path (§5) |
| 0xC6033340 | 0x800CB2E8 (GPIO driver) | GPIO-mux base |

The 0x8014xxxx/0x80103xxx cluster is a variant/ASSP DLL that repeats
parts of the timer+GPIO-mux setup — likely the next thing to decode for
the mux→ICU wiring and the watchdog. Also unexplored: the driver's
export/veneer table (29 entries decoded in 0x800B7C00-0x800B8400, Thumb
`mov/b` pairs at 2-mod-4 offsets — how the function inventory was
enumerated this session).

## 6. Implementations built from this decode (2026-08-22)

| piece | path |
|---|---|
| Linux irqchip | `kernel/linux/drivers/irqchip/irq-bcm2727-icu.c` (421 linear irqs, select/prio mask+unmask, +0x48/+0x0C dispatch+EOI, fasteoi flow) |
| Linux timer | `kernel/linux/drivers/clocksource/timer-bcm2727.c` (clockevent on reload pair, clocksource+`sched_clock` on +0x44) |
| DTS | `icu@20400000`, `timer@20203600` (+interrupts = 0xF2, clock-frequency 200 MHz) in `bcm2727.dtsi` |
| QEMU models | `emulation/qemu/hw/arm/nokia_e7.c`: `bcm2727_icu` + `bcm2727_timer` at the same PAs (all boot modes) |

QEMU chain-boot validation (no `lpj=` on the command line):

```
bcm2727-icu: /soc/interrupt-controller@20400000: 421 irq ids
bcm2727-timer: /soc/timer@20203600 tick at 200000000 Hz, irq 16 (ICU id 0xf2)
Calibrating delay loop... 1506.50 BogoMIPS (lpj=7532544)
clocksource: Switched to clocksource bcm2727-frc
... full boot to the expected VFS panic (no rootfs by design)
```

The self-calibrated lpj is the end-to-end proof: delay calibration
requires sustained timer interrupts (timer expiry → ICU pending → CPU IRQ
→ Linux handler → EOI → next period) for the whole boot.

**TODO-FOR-HARDWARE:**
- read-back semantics of +0x37A8 (assumed readable down-counter in the
  QEMU model; Linux never reads it) and of +0x44 (assumed simple 32-bit
  up-counter; Symbian's double-read latch dance suggests more).
- The true input-clock tree (200 MHz is arithmetically pinned but the
  source PLL is unidentified).
- EOI write of an id that is not in service: ignored (model) vs undefined
  (hardware).
- GPIO mux → ICU wiring (§5) before a hardware pinctrl irqchip.

## 7. Method / reproducibility

- `python3 tools/gpio_static.py --hits firmware-re/dumps/romdumpplus.dmp`
  — literal pool scan (sys-page/ICU window hits, §1-§3 above were picked
  out of its output).
- `python3 tools/gpio_static.py --track VA --start VA --len N --mode arm`
  — disassembly + dataflow of the functions cited above.
- Raw pool-word dumps (python struct) around 0x80001DF4 / 0x800020A0 for
  the mapping table; `0x800BC268` for the IFC descriptor table.
- **Tracker pitfall:** the ROM compiler folds constants with
  `orr rX, rX, rX, lsr #N` (e.g. 0xC6033600 → 0xC6033780,
  0xC6060004 → 0xC606001C). Literal trackers that don't model ORR
  attribute the store to the wrong address — always recompute when
  capstone shows an ORR on a register carrying an address.
- **Disassembly pitfall:** 0x800B7C00-0x800B8400 is a veneer/jump table
  (Thumb `mov/b` pairs at 2-mod-4 halfword offsets), so `--func` on any
  address in that range yields garbage — scan for the `b` halfwords and
  jump-table targets instead.
