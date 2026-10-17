# Task 109: the ttySU0-only console wedge — root-cause + fix

## Status

**DONE + VALIDATED 2026-08-28.** Root-caused AND fixed (driver +
one-line arch fixmap slot + an ARM recursion guard); kernel rebuilt
(zImage only, no QEMU change); `console=ttySU0`-only boots now reach
`e7:~#` with the su42 decode intact; ttyAMA0 boots byte-identical to
the task-104 baseline (modulo new build strings + one new informative
uart_report_port line). Full write-up: docs/bcm2727-softuart.md §3
(task-109 note); evidence: docs/boot-109-su-full.txt. One emulator-only
caveat documented: loglevel-5 boots need `-icount shift=3` (auto grows
the poll quantum after long console idles — see §4 item 10).

**QUEUED 2026-08-26 — written, not executed.** Desk item 5 (HANDOFF).
Medium-risk idle pick: likely needs a kernel driver edit + rebuild +
revalidation; a session that ends with a solid root-cause write-up and
no fix is an acceptable outcome.

**IDLE-TASK PROMPT (paste into the queue):**
> Execute tasks/109-O-suconsole-wedge.md in /home/lukas/ps3/e7 (read
> it, docs/bcm2727-softuart.md §3 incl. the task-107 note, and
> docs/bcm2727-onenand.md §5.3 item 3 first). One job: root-cause why
> `console=ttySU0`-only boots park in cpu_v6_do_idle after the early
> log (root never mounts), and fix it if the fix is driver/DTS-level.
> Debug under PLAIN TCG (the wedge reproduces there; icount is slow)
> with gdb-multiarch + kernel/linux/vmlinux symbols. Success = the su
> boot reaches `e7:~#` with su42 decode intact AND the ttyAMA0 boot is
> unchanged. If the root cause demands structural printk/console
> redesign, document it and stop. Commit when done.

## Known facts (all verified 2026-08-26, task 107)

- Command: the standard chain boot with `-icount shift=auto` and
  `-append "earlycon=bcm2727su console=ttySU0 loglevel=5
  root=/dev/mmcblk0 rootwait rw"`. Logs:
  docs/boot-107-su-icount.txt (wedge), docs/boot-107-icount-ttyama0.txt
  (control).
- Last output: the CR of "earlycon: bcm2727su0 at I/O port 0x0
  (options '')". 511 frames decoded, **0 framing errors**, byte-exact
  vs the ttyAMA0 boot's printk text — wire, tap and driver TX are fine
  up to that point.
- At the wedge (QMP `info registers`): PC 0xc00190ac =
  `cpu_v6_do_idle` (WFI), PSR 0x60000193 (IRQs masked, SVC) — idle,
  not a spin. QEMU vCPU halted (3 s CPU in 145 s wall).
- Root is never mounted: eMMC image md5 identical before/after.
- Same boundary under plain TCG (with the usual ~1 char/burst
  jitter) → **not** a timing artifact; icount+ttyAMA0 control boots to
  `e7:~#` → console-path-specific.
- Symbols are in sync: the wedge PC matches System.map exactly
  (cpu_v6_do_idle 0xc00190ac) — use kernel/linux/vmlinux for gdb.

## Hypotheses (ordered)

1. **Real-console registration hang:** the wedge sits right before
   where a non-bootconsole would take over (in ttyAMA0 boots the next
   visible step is the "printk: legacy console …" pair). Suspect the
   bcm2727_softuart driver's registration path (uart_register_driver /
   console registration / serial-core probe) deadlocks or never
   completes when it is the ONLY console.
2. **CON_PRINTBUFFER replay through serial-core:** on registration the
   buffered log replays back-to-back through the bit-bang path; if
   that happens with a lock held (console_lock/logbuf) or before the
   tty/baud is set, the flush may stall with IRQs off → idle with
   nothing scheduled.
3. **Earlycon/bootconsole handoff:** interaction between the raw
   earlycon (bcm2727su0) and the real console while both are
   registered.

## Method

1. **Boot matrix (cheap, plain TCG, ~2 min each):** variants of the
   su command — `keep_bootcon`, `loglevel=7`, extra `console=` order
   — record how far each gets. If `keep_bootcon` progresses far past
   the boundary, hypothesis 1/3 narrows to the handoff/registration.
2. **gdb:** run QEMU with `-s` (no `-S`), attach gdb-multiarch after
   the NLoader phase (`target remote :1234`, `file vmlinux`),
   `hbreak bcm2727su_console_write`, `hbreak uart_add_driver`,
   `hbreak printk` as needed; find the last printk that attempts the
   su console and where the CPU ends up. QMP `info registers` at the
   stall for the PC/PSR pair.
3. **Fix or document.** Driver/DTS-level fix → implement, rebuild the
   kernel (`make zImage` in kernel/linux — the chain loads the zImage
   externally, NO QEMU rebuild), revalidate:
   - su boot (icount) → `e7:~#`, su42 decode intact;
   - ttyAMA0 boot (plain TCG) → `e7:~#`, behavior unchanged vs
     emulation/qemu-e7sw/boot-baseline-task104.log.

## Deliverables

- Root cause written into docs/bcm2727-softuart.md §3 (+ correct the
  task-107 note's "desk item" wording once solved); onenand doc §5.3
  item-3 cross-note; HANDOFF desk item 5 resolved; this task's Status.
- If fixed: new evidence log docs/boot-109-su-full.txt (icount su
  boot to prompt).
- Commit.

## If it does NOT work

One iteration of diagnosis only: save the gdb/boot-matrix evidence,
write the narrowed hypothesis into softuart doc §3, stop. Do NOT
rewrite the driver wholesale unattended, and do NOT "fix" it by
sneaking a second console into the boot (that hides the bug).

## Gotchas

- Plain TCG reproduces the wedge — debug there, validate byte-exact
  claims with icount only.
- Kernel edit ⇒ rebuild zImage only; blobs/QEMU untouched (no
  build.sh, no qemu-rebuild.sh).
- The tap mutes after framing errors until two clean frames — absence
  of `su42:` lines is not by itself proof the guest stopped.
- QMP not HMP; gdb-multiarch is the working gdb (distro gdb lacks
  ARM).
