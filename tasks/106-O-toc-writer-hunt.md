# Task 106: find and decode the SDRAM-TOC slot-0 writer (the faithful fix)

## Status

**EXECUTED 2026-08-25 — WRITER CAUGHT, DECODED, CHAIN BOOTS TO `e7:~#`.**
The overlay (kept in bcm2727.c, env-gated `E7SW_TOCWATCH=1`) caught 8
word stores of 0xFFFFFFFF from 0x1003337c/0x10033380 — the double-STM
fill loop of NLoader's ARM memset — with the saved LR naming the call
site **0x10031f78**: the tail of Build TOC's copy-images prep, which
RE-EMITS the SDRAM TOC from NLoader's own 40-byte record table and
stamps one 0xFF×32 end marker at 0x101ff000 + count*0x20.  Zero
collected records (the shadow image-list at 0x10024178 is never staged
and the BB5-container builder gates starve in emulation — full decode
in docs/bcm2727-onenand.md §5.2 item 2) put that marker ON SLOT 0.
Verdict per this task's own branch test: emulation artifact ("a
search-miss path that on hardware would have found the root TOC") →
**0x10031f78 NOPed as a §4 ledger entry**; the tasks-105 self-heal
stays retired (structurally dead — no LLD traffic post-blanking).
Two follow-on blob repairs the now-live table walk forced: synthetic
MCUSW slot offset → out-of-range 0xFFFF0000 (offset 0 let the copier
stream flash-TOC data over the trampoline), and compressed-container
entries (name[3] '*'/'/') curated out of the shim table (their
Decompress copy path is secure-side-walled).  Re-validated: full
genuine flow to "Start SOS at 80010000" → Linux → ROOT-UP → `e7:~#`,
with and without debug envs.  Logs: docs/boot-106-*.txt.

**IDLE-TASK PROMPT (paste into the queue):**
> Execute tasks/106-O-toc-writer-hunt.md in /home/lukas/ps3/e7 (read it
> and docs/bcm2727-onenand.md §5.1/§5.2 first). One job: add a logging
> memory-region overlay on the SDRAM-TOC head, catch the writer PC on a
> boot, disassemble + decode the writer's intent, then either NOP it
> (§4 ledger) or document why the tasks-105 self-heal is the faithful
> choice. Re-validate the boot afterwards. Commit when done.
> Prerequisite: tasks/105 should have landed (chain boots); if it
> hasn't, read its §5.2 outcome note first.

## Why

§5.1 established: slot 0 of the SDRAM TOC (0x101ff000) is blanked with
0xFF×32 between the shim's return and the "Build TOC in SDRAM" print,
by **direct CPU stores** (every E7SW_DEBUG flash-read dst is accounted
for).  The literal scan found only four references to 0x101ff000 in
nloader.bin — 0x10030ec4 (search reader), 0x1003152c (walker),
0x10031d20 (build TOC), 0x1003222c (copy walk) — none of them the
writer, so the writer most likely computes the address from the
**published TOC pointer [0x30003e78]** (same cell cleanup's name search
uses).  Candidate window: orchestrator 0x10030b94 / first startup-mode
pass of 0x1003174c / build-TOC prologue 0x100319fc..0x10031a66.
"TOC entry added" 0x10030bdc is NOT it (it appends to the separate
40-byte table at 0x100081e8, count at 0x100074c4).

## Instrumentation (emulation/qemu/hw/arm/bcm2727.c)

Env-gated (`E7SW_TOCWATCH=1`, default off so normal boots are
untouched): a 0x20-byte io MemoryRegion overlaid at 0x101ff000 with
`memory_region_add_subregion_overlap(..., priority 1)`.  **Forward
accesses by direct host-pointer arithmetic, not by re-dispatching the
address space** (address_space_write on the same address would recurse
into the overlay): grab the PDRAM MemoryRegion (4 MiB @ 0x10000000,
created in bcm2727.c), `host = memory_region_get_ram_ptr(pdram_mr) +
(0x101ff000 - 0x10000000)`, then ops.read/ops.write are memcpys
to/from `host+offset`.  On every write log: offset, size, value, and
the guest PC (`ARM_CPU(first_cpu)->env.regs[15]`, Thumb lsb included)
with a `tocwatch:` prefix.  Rebuild QEMU only (no blob change).

## Procedure

1. Boot (tasks/105 command) with `E7SW_TOCWATCH=1 E7SW_DEBUG=1`.
   Expect ≤ 8 word-sized writes before the "Build TOC in SDRAM"
   print — the writer PC(s) are the deliverable.
2. Disassemble nloader.bin around the writer PC (base 0x10020000,
   Thumb; capstone is already used by tools/gpio_static.py — beware
   its `orr rX,rX,rX,lsr #N` constant-folding trap; /tmp/thdis.py
   from the bisect session may still exist but do not rely on it).
   Identify the routine: entry, what calls it, what it thinks it is
   doing (it walks the published pointer and blanks slot 0 — find the
   loop and its termination condition).
3. **Decode decides the fix** (this is the point of the task):
   - If the blank only happens because of OUR environment (a forced
     branch from the §4 ledger, a gate the shim's synthetic entry
     trips, a search-miss path that on hardware would have found the
     root TOC) → it is an emulation artifact: NOP the store site like
     any §4 ledger entry (site, was, now, wall), re-validate boot.
   - If it is NLoader's genuine behavior (a deliberate "drop stale
     MCUSW slot before rebuild" that would run on hardware too) → do
     NOT NOP — fidelity doctrine (task 104 exists to stop NOPing real
     paths).  On hardware SWBL owns that table, so the blob-side
     self-heal (tasks/105) is then the faithful mechanism; record the
     writer in §4 as "decoded, retained — healed blob-side".
4. Re-validate the full boot regardless of which branch (must still
   reach `e7:~#`).

## Deliverables

- §5.1 window narrowed to the exact PC + routine (+ what calls it).
- §4 ledger row for the outcome (NOP entry or decoded-retained note).
- §5.2 item-2 outcome paragraph.
- Chain still boots.  Commit.

## Gotchas

- QMP not HMP for guest dumps; gdbstub dead end (host gdb lacks ARM).
- If the overlay catches reads through QMP pmemsave too — expected,
  pmemsave dispatches the address space; harmless (reads forward).
- Writer stores may arrive as one 4-byte write per word or a memset-
  shaped burst — log raw, interpret after.
