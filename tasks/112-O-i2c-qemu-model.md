# Task 112: QEMU I2C model ("bcm2727.i2c") + retire the legacy XTI shim

## Status

**EXECUTED 2026-08-30 — DONE.** Model shipped in
`emulation/qemu/hw/arm/bcm2727.c` (`bcm2727.i2c`), shim retired, chain
re-validated to `e7:~#` with the marker flow identical to the task-104
baseline. Full as-built record: `docs/bcm2727-i2c.md` §6.1 notes.
Validation log: `emulation/qemu-e7sw/boot-i2c-task112.log`.

**Results vs the exit criteria:**

- Chain boots to `e7:~#` with the shim deleted — no restore needed
  (the fallback clause never fired).
- "init IC2_2 controller done" goes through the REAL engine:
  NLoader's selector programs the model at the 0x20032500 alias view
  and the full §3.1 init sequence is traced under I2C_DEBUG=1
  (mode 2, SCL 0x39/0x3B, divisor 0x1F8, ctrl 0x80→0x8080).  Bonus
  sanity: `devmem 0x20032528` from the Linux shell reads 0x000001F8.
- Charger probe traffic (bus 2, readreg at 0x6B) is visible under
  I2C_DEBUG=1 — **with a decode correction**: NLoader's probe path
  NEVER RUNS in the chain boot.  The call sites 0x10020966/78 live in
  0x10020902, which has no reference anywhere in the image (no BL /
  literal / pointer byte — it is reached via a runtime SWBL-side
  dispatch we don't reconstruct) and never executes (gdb breakpoint +
  zero engine writes prove it).  Task 111's "called from early boot"
  was a static-location claim only.  The traffic instead comes from
  the model's probe exerciser zone @0x40221000 (QEMU-only scaffolding,
  gpio-sim family): `devmem 0x40221000 32 0x6b` →
  `START TX 0x6b / TX 0x03 / Ardy / START RX / RX 0x49 / Ardy`,
  `i2c-probe: readreg(0x6b, 3) = 0x49`; absent addresses NAK.

**Model decisions worth knowing for tasks/113** (all in doc §6.1
as-built):

- START latch is arm-edge based (bit0+bit15+bit9/bit10, composite
  newly-armed OR bit9 changed), NOT any-write — NLoader's epilogues
  leave bit0 set and any-write would wedge the bit12 busy gate.  A
  driver doing combined write-then-read must pass through an unarmed
  ctrl value (Symbian's Configure ctrl=0 shape) or flip bit9.
- TxData/RxData re-fire on their W1C ack while bytes remain (the ack
  is the "byte consumed" edge — matches both drivers' poll/IRQ flows).
- Stub regs: charger 0x6B reg3 = 0x49 (BQ2415x, "24150 rev 1"), reg0
  0x10; PMIC echo 0x48-0x4B on bus 0; IRQ ids 0x96/0x97/0x98 level on
  (status & enable).
- Board fixup kept in nokia_e7.c: a one-byte SWBL-role .bss zeroing of
  the probe-done flag at 0x100076F9 (stale code byte 0x98 under the
  flat load would read as "already probed") — no NLoader instructions
  are patched anywhere.

**IDLE-TASK PROMPT (paste into the queue):**
> Execute tasks/112-O-i2c-qemu-model.md in /home/lukas/ps3/e7 (read
> it and docs/bcm2727-i2c.md — §2 register map, §3 transfer
> sequences, §6.1 the model contract — first). One job: implement the
> three-engine I2C model in emulation/qemu/hw/arm/bcm2727.c per the
> §6.1 contract (state machine, W1C status, level IRQs to ICU
> 0x96/0x97/0x98, charger responder at 0x6B on bus 2 + PMIC echoes at
> 0x48-0x4B on bus 0, dual-mapped at BOTH the 0x202022xx and
> 0x200322xx views), add the env-gated I2C_DEBUG trace, and RETIRE the
> legacy globals+0x24 XTI shim in nokia_e7.c (delete the
> i2c_base=0x401F0000 write at 0x10007714, ~line 234). Rebuild QEMU.
> Exit criteria: chain still boots to `e7:~#`, NLoader's
> "init IC2_2 controller done" now goes through the REAL engine, and
> with I2C_DEBUG=1 the charger probe traffic (bus 2, readreg) is
> visible. If retiring the shim breaks the boot, restore it and stop
> per the file's clause. Do NOT touch the kernel tree. Commit docs +
> task status; QEMU tree is gitignored by convention.

## Why

Task 111 decoded the masters from both sides and left the §6.1
contract. Today NLoader's I2C pokes land harmlessly in XTI RAM via a
board-file override (the task-093 "NLoader UART" workaround) — with a
real model, NLoader's own selector (0x100220D6 → base 0x200322xx)
drives genuine MMIO, and the stage is set for the Linux driver
(tasks/113) whose validation is i2cdetect against the stubs.

## Implementation notes

- **Implement inside bcm2727.c** (one MemoryRegion with the sys-page
  aliasing the other models already use) — avoids build.ninja surgery
  for new source files (task 104 gotcha 9).
- Dual view: 0x200322xx ≡ 0x202022xx per xti-trace §3 — map the
  region once at the 0x2020 base and alias the 0x2003 view, or vice
  versa; NLoader writes the 0x2003 form at boot.
- State machine per contract §6.1 verbatim: START latch (ctrl bit0 +
  bit15), direction = ctrl bit9, bit12 = transaction-open (NLoader's
  busy gate), NAK on empty address (never Arbit — single master),
  +0x10 bit0 reads 1 after any +0x20 write (the init poll).
- Charger stub reg3 must satisfy NLoader's classifier
  (`id>>5 == 2` = BQ2415x class) so the probe prints its "found" line.
- ICU wiring: reuse the icu model's existing claim mechanism; ids
  0x97/0x98 are model-chosen — re-check no other model claims them
  when wiring (icu-timer doc decode).
- Shim retirement: delete the whole `{ uint32_t i2c_base =
  0x401F0000; ... }` block (nokia_e7.c ~line 234) with its comment;
  note the removal in the commit message body (the ledger lives in
  docs — update docs/bcm2727-i2c.md §6 intro to say "shim retired",
  and bcm2727-xti-trace.md §2 gets a one-line pointer if it mentions
  the workaround as current).

## Validation

1. `emulation/qemu-rebuild.sh` (defaults to qemu-system-arm).
2. Boot the standard ttyAMA0 chain → `e7:~#`, with `I2C_DEBUG=1`:
   expect NLoader's engine-init sequence, the charger probe
   (write-index/read-back at 0x6B), "init IC2_2 controller done", and
   NO regression vs emulation/qemu-e7sw/boot-baseline-task104.log
   (same flow markers).
3. Sanity: a deliberate i2cget-less check is fine — QMP readback of
   one engine's divisor (504 after init) is enough.

## If it does NOT work

Boot breaks with the shim retired → restore the block, record in
docs/bcm2727-i2c.md §6 exactly what misbehaved (engine semantics
guess vs NLoader reality), and stop — the diagnosis is the deliverable,
do not force-fit by patching NLoader.

## Gotchas

- Blob changes: NONE in this task (no build.sh).
- QEMU rebuild required after bcm2727.c/nokia_e7.c edits.
- The engines sit in the GENIO/memcard sys-page neighborhood — watch
  for region-overlap regressions (memcard doc §6.1).
- W1C gotcha: status writes clear ONLY the bits written-set; the
  Symbian ISR reads +0x08 then writes back the read value.
