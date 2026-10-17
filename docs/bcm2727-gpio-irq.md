# BCM2727B1 GPIO interrupt path — static decode + irqchip (task 099)

**Date:** 2026-08-22
**Sources:** static disassembly of `firmware-re/dumps/romdumpplus.dmp`
(ASSP interrupt driver 0x800B7C00-0x800B9E00, GPIO driver
0x800C0xxx-0x800C3xxx, kernel 0x8005xxxx-0x8006xxxx; ARM code),
cross-referenced with `docs/bcm2727-icu-timer.md` (dispatcher §1.1) and
`docs/gpio-static-pinmux.md` (bank register blocks §3).
**Confidence legend:** CONFIRMED = decoded instruction sequence; INFERRED
= interpretation that fits; **MODEL CONTRACT** = defined by the QEMU
model + Linux driver pair (both sides ours) — hardware bring-up must
confirm or override.

---

## 0. Executive summary

1. **GPIO interrupts never enter the ICU's 421-id space.** They assert
   the CPU IRQ line through a GPIO mux whose pending-bank bitmap is
   readable at **ICU +0x40** (and at sys +0x3350); the Symbian IRQ
   dispatcher polls +0x40 at the tail of EVERY hardware IRQ (the
   "level-3" path) and calls a registered chained handler with the
   status as its only argument. CONFIRMED.
2. The kernel-side registration plumbing is a **PSL descriptor at
   0xC8001084** (+0 = 0x800B7C88 simple entry, +4 = 0x800B7E40 the
   dispatcher) published by ICU init through the two call sites the
   handoff flagged (ROM 0x800B8C60/0x800B8C68 → veneers 0x800B80E8/
   0x800B80F0 → kernel 0x800644B0/0x800644BC); a third primitive
   (0x800644C8) writes handler+arg slots. The dispatcher's live
   extension struct at **0xC8004068** (0x18 bytes, zeroed by ICU init
   via memset) carries the level-1 handler at +0x0C and the GPIO-mux
   chained handler at **+0x10**. CONFIRMED (who fills +0x10 at runtime
   is behind the kernel's import-table indirection — not statically
   recoverable, and not needed for Linux).
3. The GPIO driver's per-bank **channel structs at 0xC8004150 +
   bank·0x68** (7 banks, initialized by the 7-case jump table at
   0x800C0E14) are a plain pointer array that definitively maps every
   bank register: DATA trio, WAKE trio, EDGE pair, and the 16-register
   IRQ block Q+0x00..Q+0x3C at sys **+0x3400 + bank·0x40**. CONFIRMED.
4. This resolves the icu-timer §5 open item ("how the mux maps into the
   ICU id space"): **it doesn't** — and it retires the "+0x13C-0x193
   pin resource ids are ICU ids" hypothesis: those are kernel resource
   ids, not ICU ids.

## 1. Dispatch structure (all CONFIRMED)

```
IRQ entry (kernel glue 0x8005A044 area loads the PSL descriptor
0xC8001084 and jumps to +4 = the ASSP dispatcher 0x800B7E40):

  level 1: r1 = [0xC8004068+0x0C]        ; vectored handler, 0 here
           if r1: call r1
           if [ICU+0x9C] != 0: goto level 3
  level 2: r0 = [ICU+0x48]               ; pending id, -1 = none
           while r0 != -1: {handler table 0xC8003340[r0]; re-read}
  level 3: r0 = [ICU+0x40]               ; GPIO-mux bank bitmap
           sb = [0xC8004068+0x10]        ; chained handler
           if sb: r0 = sb(r0)
           if r0 == 2: special exception return (ldm sp!,{pc}^)
```

Level 3 runs on every IRQ tail — even while an id interrupt was just
serviced. The mux read-ack helper (0x800CB2E8, also the kernel idle
path 0x8005C41C and the MMU-off bootstrap at 0x80002640 via the
0x20033340 PA alias) is:

```
[sys+0x3354] = 0      ; global clear-all ack
r0 = [sys+0x3350]     ; status read
```

## 2. Bank register map (CONFIRMED via channel-struct decode)

Channel struct at 0xC8004150 + bank·0x68 (banks 0-4 full GPIO, 5/6
wake-only — jump-table cases at 0x800C1204/0x800C1228):

| struct field | points to | semantics |
|---|---|---|
| +0x00/+0x04/+0x08 | +0x19F4/+0x19F8/+0x19F0 (+0x10·bank) | DATA set / clr / **DATIN read** |
| +0x0C/+0x10/+0x14 | +0x1998/+0x1994/+0x1990 (+0x10·bank) | WAKE clr / set / status |
| +0x18/+0x1C | +0x1A58/+0x1A54 (+0x10·bank) | EDGE set / clr (write-only) |
| +0x20..+0x60 | +0x3400..+0x343C (+0x40·bank) | the IRQ block below |

IRQ block Q (offsets within sys +0x3400 + bank·0x40):

| off | CONFIRMED Symbian usage | semantics |
|---|---|---|
| Q+0x04 | SetPinDirection writes 1/0 | output gate |
| Q+0x08 | 0x800C0BE0 writes 1<<pin | interrupt-enable SET |
| Q+0x0C | 0x800C0BD8 reads | readback (see contract: also enable-CLR on write) |
| Q+0x10 | SetPinDirection(out) writes 1<<pin | direction SET |
| Q+0x14 | SetPinDirection(in) writes 1<<pin; ICU init writes -1 and polls | direction CLR + **irq flush** (dual use) |
| Q+0x24 | 0x800C0E04 reads (**ldrh** — 16-bit!) | pending status |
| Q+0x28/0x2C | SetPinInterruptMode (arg2 1/0) writes 1<<pin | edge-A (rising) SET / CLR |
| Q+0x38/0x34 | SetPinInterruptMode (arg3 1/2) writes 1<<pin | edge-B (falling) SET / CLR |
| Q+0x3C | suspend path polls for -1 | command ack |

## 3. Model contract (task 099, both sides ours)

Hardware bring-up must confirm or override:

- **Pending register width**: Symbian reads Q+0x24 with `ldrh` — which
  can never see bank-1 pins 26-30 (the MMC anchor pins). Contract: the
  register is 32-bit and the ldrh is a driver quirk (or there is a
  second half elsewhere, candidate Q+0x20). Linux reads it 32-bit.
- **Per-pin ack**: contract = **write-1-to-clear at Q+0x24**. The
  alternatives Symbian leaves open: read-to-clear, or the Q+0x14 flush
  (the model also accepts Q+0x14 as an ack — it clears pending bits).
- **Q+0x08/Q+0x0C pair**: contract = int-enable SET / int-enable CLR
  (with readback on Q+0x0C). Alternative reading: plain mask set/status.
- **Mux status layout**: contract = **bit N = bank N pending**
  (banks 0-4). The Symbian consumers return the raw word unmasked; no
  bit-level test was found statically.
- **Global ack**: sys+0x3354 write 0 clears all bank pending + status
  (mirrors the Symbian idle/wake path). ICU +0x40 and sys+0x3350 alias
  the same status word in the model.
- **CPU assertion**: mux pending asserts the CPU IRQ line directly,
  without any id; +0x48 keeps returning -1 while only GPIO is pending.

## 4. Implementations built from this decode (2026-08-22)

| piece | path |
|---|---|
| Linux dispatch hook | `drivers/irqchip/irq-bcm2727-icu.c` (+0x40 → registered handler), `include/linux/irqchip/irq-bcm2727.h` |
| Linux gpio irqchip | `drivers/pinctrl/bcm/pinctrl-bcm2727.c` (edge-only, handle_edge_irq, no parent irq — dispatched from the hook) |
| DTS/binding | genio node `interrupt-controller; #interrupt-cells = <2>` (no interrupt-parent, deliberately) |
| QEMU models | `emulation/qemu/hw/arm/nokia_e7.c`: `bcm2727.gpiomux` (sys +0x33300 region: mux pair + bank blocks), `bcm2727.gpiodata` (DATA slots banks 0-4), `bcm2727.gpiosim` (QEMU-only input simulator at 0x40220000, next to the chain PL011) |
| Test module | `drivers/gpio/gpio-bcm2727-test.ko` (CONFIG_GPIO_BCM2727_TEST=m): requests a line as rising-edge, counts events |

Boot-validated end to end through the NLoader chain (root=/dev/mmcblk0):
`insmod gpio-bcm2727-test.ko gpio_num=12` → `devmem 0x40220000 32
0x1000` (toggle line 12: rising) → `irq #1 fired`; second pulse
(falling) filtered; third pulse (rising) → `irq #2 fired`;
`/proc/interrupts` shows exactly 2 events on `bcm2727-genio-irq`.

## 5. Bugs the bring-up caught (worth keeping)

1. **Same-node pinctrl+gpio needs a manual pin range**: with no
   `gpio-ranges` phandle to parse, every `gpio_request()` returned
   -EPROBE_DEFER forever (pinctrl core line 407) — invisible until
   something actually requested a gpio. Fix: `gpiochip_add_pin_range()`
   in probe.
2. **Dynamic gpiochip base**: gpiochip0 lands at 512, not 0 — legacy
   global numbers are unstable; the test module resolves the base via
   `gpio_device_find()` + `gpio_device_get_base()`.
3. **QEMU region-offset arithmetic**: writing the model against a
   region base (0x20203300) but computing sub-offsets from the page
   base (0x20203000) silently routed every irqchip register write into
   the unhandled-default path — irq registered but never fired. The
   "everything is off by a constant" failure mode; caught by the
   pulse test, not by compilation.
4. `gpiochip_irqchip_add()` no longer exists in 6.12 — set
   `gc->irq.chip/handler/default_type` before `gpiochip_add_data()`,
   and mark the chip `IRQCHIP_IMMUTABLE` or gpiolib warns on every
   boot.

## 6. Open (hardware bring-up)

- Confirm the §3 contract items (ack semantics, pending width, status
  bit layout) with a phone session: bind a pin edge via Symbian (or
  bare metal), watch ICU +0x40 / +0x48 / sys+0x3350.
- The EDGE slot trio (0x1A50+0x10N, write-only) — filter/schmitt
  config hypothesis unchanged from gpio-static-pinmux §8.2; Linux does
  not touch it.
- Who fills [0xC8004068+0x10] at runtime (kernel import-table
  indirection) — Symbian-internal curiosity, no Linux impact.
- Buttons/keyboard on real hardware will hang off this irqchip; pad
  function codes for those pads are still pending the FSEL dump
  (genio-padtable §5).
