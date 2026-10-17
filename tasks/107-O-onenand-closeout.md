# Task 107: task-104 close-out — final TOC shape, validation, DONE, handoff refresh

## Status

**EXECUTED 2026-08-26 — TASK 104 CLOSED DONE+VALIDATED.** Triage boot
passed (`e7:~#`).  Final shape decision: the task's option (b) was
taken by its writer-NOP half only — landed by task 106 — and the
task-093 trim was REJECTED on the record (buys no robustness: the §4
NOP is load-bearing either way since count == 0; costs fidelity: the
staged flash entries are what exercise "Copy & Authenticate
SOS+DECOMP" and the 10-entry copy-images walk).  Full reasoning in
docs/bcm2727-onenand.md §5.3; no blob/QEMU changes were needed.
Validation: (1) plain-TCG ttyAMA0 boot → `e7:~#` + interactive checks
(mount ext4 rw, bcm2727-icu interrupt counts, mmcblk1 present);
baseline `emulation/qemu-e7sw/boot-baseline-task104.log`.  (2) Soft-UART
icount variant: su42 decode byte-exact — 511 frames, 0 framing errors,
every line verbatim in the ttyAMA0 boot's printk text — the exact
task-100 extent (docs/boot-107-su-icount.txt); the su-console-only
boot then parks in `cpu_v6_do_idle` (WFI, root never mounted) —
console-path-specific, pre-dates task 104, desk item (softuart doc §3
note).  Bonus control: icount + ttyAMA0 also boots to `e7:~#`
(docs/boot-107-icount-ttyama0.txt).  Bookkeeping done: tasks/104
flipped, HANDOFF refreshed (chain UP, first moves = desk backlog),
chronicle appended.

**IDLE-TASK PROMPT (paste into the queue):**
> Execute tasks/107-O-onenand-closeout.md in /home/lukas/ps3/e7. One
> job: close task 104 — pick/implement the final SDRAM-TOC chain
> shape, validate the chain end to end twice, save a baseline boot
> log, flip tasks/104 to DONE+VALIDATED, refresh HANDOFF (chain UP,
> first moves = desk backlog), update the chronicle, commit.
> Prerequisites: tasks/105 (self-heal) and tasks/106 (writer decode)
> should have landed — read their outcome notes in
> docs/bcm2727-onenand.md §5.2/§4 first.

## Step 0 — triage

Boot once (tasks/105 command).  If the chain already reaches `e7:~#`,
continue.  If NOT, do not force it: write the failure into
docs/bcm2727-onenand.md §5.2, leave tasks/104 as-is, and make the
commit a "state record" commit — the follow-up task gets written from
your evidence.

## Step 1 — final TOC shape (one coherent end state)

Pick ONE and implement it, based on tasks/106's decode:

- **(a) Self-heal only** (writer decoded as genuine, retained): the
  validated tasks-105 state.  Nothing to change — document it.
- **(b) Writer NOP'd + shim writes the task-093 shape natively**: in
  chain mode the shim appends the 0xFF×32 end marker at slot 1 right
  after the synthetic MCUSW entry (hide the flash entries from the
  start instead of relying on the heal).  Requires a shim.c change +
  blob rebuild + QEMU rebuild.  Choose this only if 106 proved the
  blanking is an emulation artifact — then slot 0 never blanks, the
  heal never fires, and the table NLoader sees is the task-093 shape
  by construction.

Whichever lands: the answer must be written in a new **§5.3 "final
chain shape"** paragraph of docs/bcm2727-onenand.md (who writes the
TOC, in what order, and why that is the faithful shape).

## Step 2 — validation (twice)

1. ttyAMA0 root=mmcblk0 boot (tasks/105 command): must reach `e7:~#`,
   rcS banner "ROOT-UP: buildroot armv6/musl on the eMMC ext4", and
   mount /dev/mmcblk0 (check `mount`, `cat /proc/interrupts` shows
   bcm2727-icu counts, mmcblk1 present with the sd drive attached).
2. Soft-UART variant: same command with `-icount shift=auto` and
   `-append "earlycon=bcm2727su console=ttySU0 loglevel=5
   root=/dev/mmcblk0 rootwait rw"` — su42: lines must stay byte-exact.

Save boot 1's full log as the task-104 baseline:
`emulation/qemu-e7sw/boot-baseline-task104.log` (committed; ~15 KB).

## Step 3 — bookkeeping

- tasks/104-O-onenand-bcm2727.md: Status → **DONE + VALIDATED**, with
  the one-line outcome and pointers to §5.2/§5.3.
- HANDOFF.md: "Current state" QEMU-chain row → chain UP through the
  real flash path (OneNAND model + SWBL blobs + self-heal/§4 ledger);
  "First moves" #0 → REPLACED by the desk backlog (display/DSI, I2C
  masters sys+0x22/23/25xx, USB OTG +0x25Axx, memory-map gaps
  pinctrl doc §4 item 5, sgx540-reversing/ side thread; root-TOC
  shape at BML logical 0x20000 stays phone-side).
- Chronicle: append the validation outcome to
  lore/daily/2026-08-24-onenand-bcm2727.md (its "Where it stands"
  already carries the corrected premise — add the resolution).
- Commit everything.

## Gotchas

- Blob change ⇒ build.sh ⇒ **QEMU rebuild** (embedded header).
- QMP not HMP for dumps; gdbstub dead end; /tmp leftovers unreliable.
- The QEMU tree is gitignored by convention — describe model/board
  changes in the docs, not the commit, unless only docs changed.
