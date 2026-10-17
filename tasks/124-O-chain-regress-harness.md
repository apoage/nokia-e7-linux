# Task 124: chain regression harness — protect the fidelity the project just earned

## Status

**DONE + SELF-VERIFIED — executed 2026-09-05.** Deliverables:
`tools/chain-regress.sh` (usage documented in its header; future task
files: "run chain-regress before/after" and paste both summaries) and
`tools/chain-flow-canonical.txt` (12-line policy header + the 1202-line
task-122 flow, md5-verified against the source extraction; header also
records the determinism evidence: TCG/icount, debug envs on/off, two
kernel builds → one byte-identical sequence — so the diff is byte-exact
with an EMPTY tolerance list).  Self-verification transcripts (abridged;
full one-screen summaries are one command away):

1. Run 1 (plain TCG): **PASS exit 0** — boot 27 s, all 14 ordered markers
   ok (board lines 5/6, Pearl-J line 15, TOC ×4 + allocator lines,
   0x101FFC30 window, Start SOS line 1214, ROOT-UP 1307, prompt 1313),
   TOC count == 4, interactive `/proc/interrupts` captured (fifo +
   sentinel): `Err: 0`, ids 242/419/35/36/150/151/152/183 all claimed
   (183 = task-123 musb, present at count 0 even with no USB devices
   attached — the §5 command stays verbatim, no `-device` args), flow
   1202 lines byte-identical.
2. Run 2 (`--icount`): **PASS exit 0** — same summary, tick 1983 vs 1984
   (count drift is expected; only the id claim + Err are asserted, never
   the counts).
3. Forced-FAIL (canonical truncated by one line, then restored):
   **exit 1**, `FIRST DIVERGENCE: flow line 1202: run 'nloader[xti239]:
   Start SOS at 101FFC30 (->101FFC30)' vs canonical '<canonical
   ended>'`; every other check stayed ok — the FAIL names exactly the
   divergence.  Canonical restored byte-exact (md5 match).

Note: task 123 did NOT change the `nloader[xti` flow (122/123 logs
byte-identical) — the canonical needed no deliberate update.

**IDLE-TASK PROMPT (paste into the queue):**
> Execute tasks/124-O-chain-regress-harness.md in /home/lukas/ps3/e7
> (read it and docs/bcm2727-onenand.md §5.3.1 first — the current
> canonical flow + new-print table). One job: a regression harness
> `tools/chain-regress.sh` + the canonical flow file it checks
> against. The script boots the chain (plain TCG; `--icount` for the
> control variant; fixed eMMC-root args per onenand §6/pinctrl §5),
> captures the log, and asserts, in order: the board info line
> shape (single map @ 0x10010000, BB5 entry 0x100103D0, trampoline
> 0x101ffc30), the §5.3.1 pre-main markers (Pearl-J : v1.0, TOC
> entry added ×4 incl. MCUSW, the genuine-allocator lines), "Start
> SOS at 101FFC30", the ROOT-UP banner, `e7:~#`, and — piped
> interactively at the prompt — `/proc/interrupts` containing Err: 0
> and the expected id claims (tick 242, uart 419, mmc 35/36, i2c
> 150-152; more after task 123 lands — make the id set a config
> list); then diffs the `nloader[xti` line sequence against the
> committed canonical file `tools/chain-flow-canonical.txt`
> (extract it from emulation/qemu-e7sw/boot-bb5-task122.log's
> 1202 lines — the independently re-verified flow). Interactive
> input via a fifo with generous settle delays (busybox at the
> prompt; drop interactive checks to warnings if timing flakes —
> never a false FAIL). Exit 0 PASS / 1 FAIL with a one-screen
> summary naming the first divergence. Verify the harness itself:
> run it twice (two PASSes), and run it once with the canonical
> file temporarily truncated by one line to prove it FAILS (then
> restore). Document usage at the top of the script (future task
> files should cite: "run chain-regress before/after"). NO doc
> rewrites beyond a short HANDOFF-free note in the task file; do
> not touch QEMU/kernel. Commit when done.

## Why

The chain just reached full fidelity through 104→122; every future
model/driver/doc-touching task now risks regressing it silently. The
independent re-verification boots have been manual; this makes them
one command. Cheap to write, pays off every run hereafter — and it
is exactly the "verifying task" class that needs no phone.

## Method

1. Extract the canonical nloader[xti flow from
   boot-bb5-task122.log → tools/chain-flow-canonical.txt (commit it
   with a header naming its source + the 1202-line count).
2. chain-regress.sh: mktemp log, timeout-boot (reuse the §5 run
   command verbatim, `-serial stdio` capture), fifo-fed interactive
   block, marker asserts (ordered grep with line numbers), flow
   diff, PASS/FAIL summary, exit code. `--icount` variant,
   `--quick` (skip interactive), `--keep` (preserve log path in the
   summary).
3. Self-verify per the prompt (two PASSes + the forced-FAIL
   truncation test). Note the runtime (~3-4 min TCG) in the header.

## Exit criteria

1. Script + canonical file committed; two clean PASS runs (one with
   --icount) recorded in the task file.
2. Forced-FAIL test demonstrated (truncated canonical → exit 1 with
   the divergence named).
3. Usage documented; task file carries the run transcripts.

## If it does NOT work

- Interactive fifo timing proves flaky across runs: demote that
  block to warning-only (per the prompt) and note it; the marker +
  flow-diff spine is the load-bearing part.
- If the flow diff trips on legitimately nondeterministic lines
  (timestamps/counts): identify them, and make the canonical check
  tolerant (strip those fields) — but keep the tolerance list
  explicit in the script, not silently broad.

## Gotchas

- The §5 run command's relative paths — anchor everything to the
  repo root (the script runs from anywhere).
- grep -a on the logs (CRLF/extended ASCII — the named hazard).
- Do not "fix" a FAIL by editing the canonical file unless the flow
  LEGITIMATELY changed (e.g. task 123 adds USB lines — then the
  canonical update is a deliberate, documented commit, never a
  side effect).
- Commit locally; never push.
