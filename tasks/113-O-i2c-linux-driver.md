# Task 113: Linux i2c-bcm2727 driver + DTS + i2cdetect validation in the chain

## Status

**EXECUTED 2026-08-30 — DONE.** Driver shipped as
`kernel/linux/drivers/i2c/busses/i2c-bcm2727.c` (+ Kconfig/Makefile,
three DTS nodes in the live `bcm2727.dtsi`, defconfig I2C=y /
I2C_CHARDEV=y / I2C_BCM2727=y with the `kernel/configs/` mirror
synced). As-built record: `docs/bcm2727-i2c.md` §6.2 notes.
Validation log: `emulation/qemu-e7sw/boot-i2c-task113.log`.

**Results vs the exit criteria (all five):**

1. Three "bcm2727-i2c: adapter 0/1/2 ..., IRQ-driven" probes; no
   deferrals/errs; eMMC root mounts; boot to `e7:~#`.
2. `i2cdetect -y 0` → exactly `48 49 4a 4b`; `i2cdetect -y 2` →
   exactly `6b` (6a not modeled in 112); all other cells `--`.
3. `i2cget -y 2 0x6b 0x03` → `0x49` (BQ2415x-class id); `i2cset -y 2
   0x6b 0x05 0x5a` → `i2cget` read-back `0x5a` — this pair exercises
   the combined write-read-without-STOP path.
4. Boot diff vs `boot-baseline-task104.log`: the three adapter lines
   + the `i2c_dev:` init line + build strings only (the `softuart:`
   line is a pre-existing since-task-109 delta, confirmed against the
   task-112 log).
5. (optional, skipped — better evidence instead) `/proc/interrupts`
   shows ICU 150/151/152 claimed by the three adapters with **zero
   idle counts** before any i2c traffic (no early-IRQ storm), and
   112/0/128 after the detection sweeps; no id conflicts (tick 242,
   mmc 35/36, uart 419 intact).

**Decisions worth knowing:** `compatible = "brcm,bcm2727b1-i2c"` (house
convention) instead of §6.2's literal "broadcom,bcm2727-i2c" — recorded
as a contract erratum in the doc. Adapter numbers are pinned to the
engine index so i2c-0/2 are the firmware's I2C_0/I2C_2 regardless of
probe order.

**IDLE-TASK PROMPT (paste into the queue):**
> Execute tasks/113-O-i2c-linux-driver.md in /home/lukas/ps3/e7 (read
> it, docs/bcm2727-i2c.md §6.2, and tasks/112's outcome first —
> prerequisite: the bcm2727.i2c model is live and the chain boots
> through the real engines). One job: write
> kernel/linux/drivers/i2c/busses/i2c-bcm2727.c (transfer algorithm
> per §6.2: NLoader-polled shape, Symbian ctrl constants, combined
> write-read without STOP between segments, IRQ-driven with poll
> fallback), add the three DTS nodes (i2c@20202200/2300/2500,
> interrupts 150/151/152 = ICU 0x96/97/98, devm_ioremap NOT
> devm_ioremap_resource), enable in the defconfig. Rebuild zImage.
> Exit criteria per §6.2: three adapters registered at boot,
> `i2cdetect -y 0` lists exactly 0x48-0x4B, `i2cdetect -y 2` lists
> 0x6B, an i2cget/i2cset round trip on the charger stub matches, the
> eMMC root still mounts, and the ttyAMA0 boot diff vs the task-104
> baseline is clean apart from the new adapter lines. Do NOT touch
> the QEMU tree or the blobs. Commit (kernel repo + docs).

## Why

Task 111's contract §6.2 + task 112's model close the loop: i2c-tools
have been in the buildroot root since task 103 — this task makes
`i2cdetect`/`i2cget` work in the chain, the first Linux-side I2C
primitive for hardware bring-up (bus 0's real citizens: PMIC PEARL,
touch/compass/accel/NFC/ALS per hardware-lore §10).

## Implementation notes

- Register constants are all proven (§6.2 init constants: +0x28=504,
  +0x34=0x39, +0x38=0x3B, +0x20=2, +0x30=2).
- The combined write-then-read (I2C_M_RD after a write in one xfer)
  is Symbian's read-phase pattern — write WITHOUT stop (0x8601 base),
  then read with repeated START. The model treats each START as its
  own segment; it must not require a STOP between them (if it does,
  that's a model bug — fix note, not a driver workaround).
- `devm_ioremap` not `devm_ioremap_resource` — the windows sit inside
  the GENIO pinctrl node's exclusive sys-page claim (memcard doc
  §6.1 item 2 precedent; memcard driver solved the same problem).
- ICU interrupt-parent wiring mirrors the mmc/timer nodes in
  bcm2727*.dtsi; ids 0x96/0x97/0x98 → DT ints 150/151/152.
- Keep a poll fallback when the IRQ resource is missing (memcard
  driver's evolution shape) — useful if the id-0x97/0x98 claims turn
  out contested.

## Validation (exit criteria — all five)

1. Chain boot (ttyAMA0): three "bcm2727-i2c ... adapter" probes, no
   deferrals/errs; eMMC root mounts; `e7:~#`.
2. Interactive: `i2cdetect -y 0` → exactly 48-4b; `i2cdetect -y 2` →
   exactly 6b (6a optional if 112 modeled it); other cells `--`.
3. `i2cget -y 2 0x6b 0x03` → the BQ2415x-class id; `i2cset` +
   read-back round trip matches.
4. ttyAMA0 boot log diff vs
   emulation/qemu-e7sw/boot-baseline-task104.log: only adapter lines
   + build strings.
5. su-console spot check (optional): `-icount shift=3` su boot still
   reaches the banner (proves no early-IRQ storm).

## If it does NOT work

Driver probes but transfers time out → check the model's bit9
direction rule vs the driver's constants FIRST (both proven flows
live in §3); record the mismatch in docs/bcm2727-i2c.md §6.2 as a
contract erratum. If adapters grab wrong IRQ ids (conflicts in
/proc/interrupts), fall back to poll mode and document — do not
re-assign ICU ids inside the icu model unattended.

## Gotchas

- Kernel rebuild only (`make zImage` + dtbs); NO QEMU rebuild, NO
  build.sh — the model landed in 112.
- Commit in BOTH repos: kernel/linux (driver+dts+defconfig) and the
  outer repo (docs/task status).
- The defconfig mirror in kernel/configs/ must stay in sync
  (HANDOFF convention).
