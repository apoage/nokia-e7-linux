# Task 102: IRQ-driven mmc — sleep instead of polling

## Status
**DONE (2026-08-23).** Handoff desk-queue item #2: the memcard host
driver requests its channel ICU id (eMMC 0x23 = 35, SD 0x24 = 36) and
sleeps on a completion for command-done instead of udelay-polling.
Every command since boot shows up in `/proc/interrupts`.

## What landed

1. **Linux** `drivers/mmc/host/mmc-bcm2727.c`:
   - probe requests the IRQ (devm) BEFORE `mmc_add_host` — the core's
     first detect sweep can't issue a command before the handler
     exists; ISR completes on status DONE|ERROR, returns IRQ_NONE
     otherwise.
   - `bcm2727_execute_cmd`: `reinit_completion()` (armed before the
     command word — the model raises the ICU id the instant the +0x00
     write retires) → `wait_for_completion_timeout(1s)` → read status
     for the error check. Polling path kept as the no-IRQ-resource
     fallback (`platform_get_irq` failure → warn + poll).
   - The PIO FIFO moves still poll the ready bits (+0x10 bits 10/11):
     the FIFO-threshold interrupt semantics are undecoded (Symbian's
     PIO loops don't poll at all — memcard doc §4); the emulated FIFO
     is always ready, so this costs nothing in the chain.
2. **No QEMU change**: the model has raised ICU 0x23/0x24 on command
   completion since task 097 — the polling driver simply never
   unmasked it. The ICU irqchip (fasteoi + prio-write mask/unmask)
   was already proven live by the console PL011 @0x1a3 (task 097)
   and the gpio irqchain (task 099).

## Validation (chain boot, 2026-08-23)

- Both channels probe as `IRQ-driven`: `mmc-bcm2727 20202800.mmc:
  BCM2727 Rapu_Memcard eMMC channel, IRQ-driven` (+ SD twin).
- Root still mounts from the eMMC (`VFS: Mounted root (ext4)`) and
  the SD card attaches beside it — the whole task-098/101 flow on the
  IRQ path.
- `/proc/interrupts`:
  `18: 454 bcm2727-icu 35 Edge 20202800.mmc` (every command through
  boot + root mount), `19: 27 bcm2727-icu 36 Edge 20202900.mmc`,
  `Err: 0` — zero spurious (the ISR's IRQ_NONE guard never tripped).
- R+W through both cards under the IRQ driver; guest write persisted
  (host-side `debugfs -R "cat /task102-sd-write" sd.img`).

## Next

Desk queue: ARMv6 userland toolchain (musl.cc + buildroot), QEMU
chain fidelity (real OneNAND model, peel the board file off OMAP
scaffolding).
