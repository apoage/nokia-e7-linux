# Task 099: GPIO mux → ICU wiring decoded, gpio irqchip shipped + validated

## Status
**DONE (2026-08-22).** Handoff desk-queue lead #2 executed end-to-end:
static decode of the GPIO interrupt path (level-3 chained dispatch,
bank register map via the channel structs), Linux gpio irqchip + QEMU
mux model + input simulator, and a chain-boot validation where a
userspace-triggered gpio edge fires a request_irq handler through the
whole path. Full detail: **docs/bcm2727-gpio-irq.md**.

## What the decode established (CONFIRMED, from the ROM)

1. **GPIO events never enter the ICU id space** — they assert the CPU
   IRQ with a pending-bank bitmap at ICU +0x40 (aliased sys +0x3350),
   dispatched at the tail of every IRQ (level 3). This closes the
   icu-timer §5 open item and retires the "0x13C-0x193 are ICU ids"
   hypothesis.
2. The handoff's call sites 0x800B80E8/0x800B80F0 are the ASSP
   self-registration: ICU init publishes {0x800B7C88, dispatcher
   0x800B7E40} into the kernel PSL descriptor 0xC8001084 and memsets
   the live extension struct 0xC8004068 (whose +0x10 is the chained
   GPIO handler slot). Kernel glue at 0x8005A044 dispatches through
   the descriptor.
3. Per-bank channel structs 0xC8004150 + bank·0x68 (7-case init at
   0x800C0E14) map every bank register — confirms gpio-static §3's
   DATA/WAKE/EDGE trios and the IRQ block Q+0x00..0x3C, and adds the
   definitive field→register assignment.

## What landed

1. **Linux**: irqchip hook in `irq-bcm2727-icu.c` (+0x40 → registered
   handler, header `include/linux/irqchip/irq-bcm2727.h`); edge-only
   gpio irqchip in `pinctrl-bcm2727.c` (mask/unmask Q+0x08/Q+0x0C, ack
   = write-1-to-clear Q+0x24, set_type on the edge-A/B pairs,
   handle_edge_irq, IRQCHIP_IMMUTABLE, no parent irq — dispatched from
   the hook); manual `gpiochip_add_pin_range` (same-node pinctrl+gpio);
   DTS genio node + binding gain interrupt-controller.
2. **QEMU** (`nokia_e7.c`): `bcm2727.gpiomux` region (sys +0x33300:
   mux pair + bank blocks, edges evaluated against en/edge_a/edge_b,
   status mirrored into ICU +0x40, CPU asserted without an id),
   `bcm2727.gpiodata` (DATA slots banks 0-4), `bcm2727.gpiosim`
   (QEMU-only input toggle at 0x40220000, next to the chain PL011).
   ICU model gained `gpio_mux_status` (asserts ahead of the id scan).
3. **Test module** `drivers/gpio/gpio-bcm2727-test.ko`
   (CONFIG_GPIO_BCM2727_TEST=m): resolves the chip by label
   (gpio_device_find; dynamic base was 512), requests a line as
   rising-edge, counts events. build-emmc-root.sh stages modules/ into
   /root; busybox `devmem` added to the applet set.

## Validation (chain boot, root=/dev/mmcblk0, 2026-08-22)

```
insmod /root/gpio-bcm2727-test.ko gpio_num=12
  → watching gpio line 12 (global 524) on irq 18 (rising)
devmem 0x40220000 32 0x1000   # rising  → "irq #1 fired"
devmem 0x40220000 32 0x1000   # falling → filtered (correctly silent)
devmem 0x40220000 32 0x1000   # rising  → "irq #2 fired"
/proc/interrupts: 18:  2  bcm2727-genio-irq  12 Edge  gpiotest
```

sim → mux model → CPU line (no ICU id) → ICU level-3 → gpio irqchip →
request_irq — the whole contract path, polarity-selective. Boot
otherwise unchanged (timer/UART/mmc/root all normal, no new warnings).

## Bugs the bring-up caught

1. Same-node pinctrl+gpio without a pin range: every gpio_request()
   deferred forever (-EPROBE_DEFER) — invisible until this task
   actually requested a gpio.
2. Dynamic gpiochip base (512, not 0) — legacy global numbers are
   unstable; the module resolves the base at load time.
3. QEMU region-offset arithmetic bug: sub-offsets computed from the
   page base instead of the region base silently swallowed every
   register write ("irq registered but never fires").
4. API drift: gpiochip_irqchip_add() is gone in 6.12 — set gc->irq
   fields before gpiochip_add_data, mark the chip IRQCHIP_IMMUTABLE.

## Model contract (hardware must confirm — doc §3)

Pending-register width (Symbian's ldrh can't see pins >15!), per-pin
ack semantics, the +0x08/+0x0C pair meaning, and the +0x40 bit layout
(bit N = bank N). Phone path when hardware time comes: bind a pin
edge, watch ICU +0x40/+0x48 and sys+0x3350.

## Next

Queue item #3 (genio42 soft-UART driver + QEMU stub), then SD ch1,
IRQ-driven mmc, ARMv6 toolchain. The gpio irqchip is also the
prerequisite for buttons/keyboard drivers on hardware.
