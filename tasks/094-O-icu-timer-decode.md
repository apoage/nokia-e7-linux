# Task 094: BCM2727 interrupt controller + OS timer decode → Linux + QEMU

## Status
**COMPLETE (2026-08-22).** The two top hardware-boot blockers (docs/
bcm2727-pinctrl-linux.md §4 items 1 and 2) are decoded from the Symbian
ROM, implemented in Linux (irqchip + clockevent/clocksource) and modeled
in the QEMU NLoader chain. **The chain boot no longer needs `lpj=`** —
the kernel self-calibrates its delay loop on a real timer→ICU→CPU
interrupt flow and timekeeps on the decoded free-running counter.

## Decode (full detail in docs/bcm2727-icu-timer.md)

- **ICU @ phys 0x20400000** (kernel VA 0xC6060000; phys pinned by the
  bootstrap locked-TLB table at ROM 0x800020A0 — a (PTE|attrs, VA) pair
  list that also gives six other block mappings). 421 flat interrupt
  ids; per-irq select(+0x20)/priority(+0x24, 0=masked, bind writes 2);
  one-at-a-time pending-id read(+0x48, -1=none); **EOI = write the id
  back to +0x0C** (whose read side is a VIC-style vectored-handler port,
  always 0 here). Dispatcher at ROM 0x800B7E40 has 3 levels: vectored /
  table dispatch (0xC8003340, 421×8B) / GPIO-mux chained (status from
  ICU +0x40, handler ptr in driver RAM).
- **Tick timer**: system block +0x3600-0x37FF (phys 0x2020xxxx):
  enable +0x04, mode +0x08 (3=periodic T0), reload pair +0x37A4/+0x37A8
  (ISR writes 0x9600 every tick), free-running counter +0x3644, aux
  0x493/0xE0000000. **Interrupt id 0xF2 (242)**. Reload 38400 with the
  live system's 192000 ns ENanoTickPeriod ⇒ 200 MHz-class input clock.
- **GPIO mux**: pending/ack at 0xC6033350/0xC6033354 — chained via the
  ICU level-3 path, NOT direct ICU ids. Wiring still open (blocked for a
  hardware gpio irqchip, not for bring-up).
- Driver anchors: dispatcher 0x800B7E40, bind 0x800B89B0, ICU init
  0x800B8C00, timer init 0x800B8DC8, tick ISR 0x800B8D5C, PSL init
  0x800B8E54 ("SpuriousInt" @0x800B8BC8).

## Linux (kernel/linux)

- `drivers/irqchip/irq-bcm2727-icu.c` — root controller, linear domain
  of 421, mask=prio 0 / unmask=prio 2, fasteoi flow, EOI writeback;
  init sequence mirrors Symbian's (+0x04=1, +0x1C=0, +0x10=-1, +0x84=5,
  +0x90/+0x9C=0). Kconfig `BCM2727_ICU`, selected by ARCH_BCM2727.
- `drivers/clocksource/timer-bcm2727.c` — clockevent on the reload pair
  (periodic mode 3 / oneshot mode 6), clocksource + sched_clock on
  +0x44, clock-frequency from DT (200 MHz). Kconfig `BCM2727_OSTIMER`.
- DTS: `icu@20400000` + `timer@20203600` (interrupts = <0xF2>) in
  bcm2727.dtsi; soc-level interrupt-parent = &icu.

## QEMU (emulation/qemu/hw/arm/nokia_e7.c)

- `bcm2727_icu` at 0x20400000: pending bitmap + prio array, destructive
  pending read into in-service, EOI at +0x0C **and** +0x10 (timer ISR
  writes both), flush at +0x50, CPU IRQ = gpio-in line ARM_CPU_IRQ.
- `bcm2727_timer` at 0x20203600: QEMUTimer, 200 MHz, periodic mode 3 /
  oneshot otherwise, +0x44 free-running up-counter, raises ICU id 0xF2.
- Both instantiated in ALL boot modes (bare NLoader too).

## Proof (chain boot, no lpj=)

```
bcm2727-icu: /soc/interrupt-controller@20400000: 421 irq ids
bcm2727-timer: /soc/timer@20203600 tick at 200000000 Hz, irq 16 (ICU id 0xf2)
Calibrating delay loop... 1506.50 BogoMIPS (lpj=7532544)
clocksource: Switched to clocksource bcm2727-frc
bcm2727-pinctrl 20200000.pinctrl: 176 pads, 160 gpios (banks 0-4); banks 5/6 wake-only
Kernel panic - not syncing: VFS: Unable to mount root fs   ← by design (no rootfs)
```

Self-calibrated BogoMIPS = sustained interrupt delivery across the whole
boot (delay calibration waits on jiffies). Run command = task 093's with
`lpj=500000` removed (docs/bcm2727-pinctrl-linux.md §5).

## Build-env repair addendum (qemu-rebuild.sh)

- New host breakage this session: `libgmodule-2.0.so` dev symlink gone →
  the repoint to the runtime `.so.0` is now delimiter-agnostic (space,
  comma or EOL) with a double-patch undo.
- The full `ninja` now also fails in qemu-nbd (libselinux headers gone)
  and TCG test plugins (hardcoded -I/usr/include/glib-2.0). The script
  therefore defaults to building **only `qemu-system-arm`**; pass extra
  ninja targets as arguments when needed.

## Open items / next

1. ~~**UART decode**~~ — done same-day as task 095: no APE UART exists;
   the console is the XTI trace mailbox (docs/bcm2727-xti-trace.md),
   modeled and drained in the chain.
2. GPIO mux → ICU wiring (needs runtime or more Symbian decode) before
   the pinctrl irqchip.
3. TODO-FOR-HARDWARE list in bcm2727-icu-timer.md §6 (+0x37A8/+0x44 read
   semantics, clock tree, EOI of not-in-service ids).
4. Rootfs: still ends at the VFS panic by design; eMMC decode is the
   gate for a shell.
