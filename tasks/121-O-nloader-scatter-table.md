# Task 121: NLoader scatter-relocation table — the hardware memory-layout plan behind the Phase-2 wall

## Status

**DONE — executed 2026-09-05, decode-only (no QEMU/kernel/blob
changes).**  Full deliverable: docs/bcm2727-onenand.md §7.10 (the
plan: 3 records — memcpy 0x7AC B → 0x30001800, LZ 0x192→0x4DC B →
0x10007000-0x100074DC, zero-fill 0x1974 B → 0x100074DC-0x10008E50),
plus corrections in §1.1/§7.5/§7.6/§7.8.  Headline results: (a) the
zero range file 0x18000-0x1C000 is correct-as-zeros padding — nothing
fills it because nothing references it (task 118's premise refuted;
the Phase-2 wall was the missing 0x10010000 mapping — the image is
LINKED for 0x10010000, not 0x10020000); (b) the 0x10007xxx cells are
scatter destinations, LZ-decompressed from file 0x14EA8 — the genuine
device-ID table (24 records) has no rev-class-0x0100 entry, so the
model's VerID 0x0121 would fail the open's strict matcher (the board
staging masks this today); (c) only the LLD map is plan-faithful —
the primary base is +0x10000 off (the exact cause of §4's task-118
repoints), the mirror approximates nothing.  Verdict: faithful BB5
entry is BUILDABLE from the stock dump (map file 0-0x1C000 at
0x10010000, enter 0x100103D0); the stop-clause did not trigger (all
sources are in-image).

**IDLE-TASK PROMPT (paste into the queue):**
> Execute tasks/121-O-nloader-scatter-table.md in /home/lukas/ps3/e7
> (read it and docs/bcm2727-onenand.md first — §1.1 the LLD and its
> linked base, §4 the task-118 board rows (staged cells, helper
> repoints), §7.5 the NLoader load base + the Phase-2 prologue wall).
> One job: decode NLoader's scatter-relocation plan — the table at
> nloader.bin file 0x51C driven by the loader loop at file
> 0x4E0-0x528 (pre-verified: `add r0,pc,#0x2C` @0x4E8 resolves to
> 0x51C; `ldm r0,{r10,r11}` loads {start,end} offset pairs relative
> to the 0x10010000 load base; first entry {0x40A0,0x40D0} sits at
> 0x51C/0x520; the copy helper is the `bl` @0x504; the tail
> 0x510-0x518 is tst r3,#1 / bx r3 with a Thumb switch at
> 0x524/0x528). Walk the table to its terminator, decode the copy
> helper's semantics (plain copy? entry-point calls per region? the
> bx-r3 tail), and produce the authoritative hardware memory-layout
> table: every region {source file offset → destination VA, size}.
> Answer the three questions: (a) where does the content of the ZERO
> range file 0x18000-0x1C000 (VA 0x10028000-0x1002C000 at the SWBL
> base) come from — another file region (packed segments) or content
> absent from the dump (= faithful BB5 entry impossible with the
> stock dump, cf. §5.2's BB5-container verdict)? (b) does the plan
> fill the 0x10007xxx data cells (command words 0x100071e4, device-ID
> table 0x100071fc) that the board currently stages per §4 — i.e. are
> they scatter destinations, or runtime-initialized? (c) which of the
> chain's mappings (mirror 0x10000000, primary 0x10020000, LLD
> 0x1001B000) approximate which scatter destinations — and where does
> the double-load approximation silently DIFFER from the plan?
> Deliver onenand new §7.10 'The scatter-relocation plan' with the
> house CONFIRMED/INFERRED/OPEN legend + the layout table, annotate
> §7.5's OPEN item and the Phase-2 wall, and state the verdict for
> the faithful-BB5-entry follow-up (possible now / needs dump content
> we lack). NO code or blob changes. Stop per the file's clause if
> the table's sources decode to addresses outside the image. Commit
> when done.

## Why

Task 118's Phase-2 probe walled at a precisely-located place: the
prologue and its scatter-relocation loader are linked for the SWBL
single-copy layout, and the dump's file 0x18000-0x1C000 is zeros —
on hardware the loader itself must fill that range. The loader's
table at img+0x51C is therefore the single artifact that decides
whether a faithful BB5-entry chain (entering at 0x10010000+0x3D0
instead of main, running NLoader's real prologue) is buildable from
the stock dump or blocked on content we don't have. It also settles
the emulation's biggest standing approximation: the double-load
(mirror + primary + the task-118 LLD map) is a guess at the loader's
end state — this table IS the ground truth, and every place the
guess differs is a fidelity bug waiting to be found.

## Ground truth already verified (2026-09-04, do not re-derive)

- File 0x4C4-0x4D4: the mode-stack pool (0x1002F4C0-F500, five
  words); 0x4D8/0x4DC: 0x30003E00 handoff cells — matches §7.5's
  prologue decode.
- The loader loop (file 0x4E0-0x528): `bl`×2 → `add r0,pc,#0x2C`
  (r0 = 0x51C table) → `ldm r0,{r10,r11}` → `add r10,r10,r0` /
  `add r11,r11,r0`-style base-relative math → `sub r7,r10,#1` →
  `cmp r10,r11` loop → `bl` (copy helper) → `ldm r10!,{r0-r3}` /
  `sub lr,pc,#0x18` / `tst r3,#1` / `bx r3` trampoline tail → Thumb
  switch (`add r12,pc,#1; bx r12`) at 0x524/0x528.
- First table entry: {0x000140A0, 0x000140D0} at 0x51C/0x520 (a
  [start,end) pair relative to the load base = a 0x30-byte region at
  0x100140A0).
- The prologue/loader are SWBL-base-linked (all call targets assume
  file X at 0x10010000+X; first out-call `blx 0x10016282` = file
  0x6282, call tree 0x10020482/0x10022806/0x100239d8/0x10023c20 per
  §7.5).
- The zero range: file 0x18000-0x1C000 (0 nonzero bytes).

## Method

1. Re-extract and disassemble with the SWBL base semantics
   (`tools/arm_recurse_dis.py` takes a base; the loop is ARM, the
   post-switch code at 0x52C+ is Thumb — follow the mode switch).
2. Walk the table from 0x51C: determine the record shape from the
   loop's consumption rate (the ldm {r10,r11} suggests 2-word pairs;
   verify whether the helper @0x504 consumes more per record, e.g. a
   destination or flags word), find the terminator, enumerate all
   records.
3. Decode the copy helper (target of the `bl` @0x504) and the
   bx-r3 tail: is each region plain-copied to a destination computed
   from the pair, or is {start,end} a [dst range) with the source
   elsewhere (e.g. a second table, or src = records streamed from
   the table's tail)? Pin which.
4. Build the layout table; answer questions (a)/(b)/(c) with byte
   evidence (e.g. for (a): if a record's source covers real code
   bytes destined for VA 0x10028000+, name the file range; if no
   record covers it, the content is absent).
5. Cross-check against the chain: for each scatter destination,
   what does the current emulation map there (mirror/primary/LLD
   map/board-staged)? Every mismatch is a row for the report (fixing
   them is a FUTURE task, not this one).
6. Write §7.10 + the §7.5 annotation + the faithful-entry verdict.

## Exit criteria

1. Table format + full record list (CONFIRMED, with the loop's
   consumption evidence).
2. The layout table: {source → destination, size} for every record.
3. Answers to (a), (b), (c), each CONFIRMED/INFERRED/OPEN with
   bytes.
4. §7.10 written; §7.5's OPEN + Phase-2 wall annotated; the
   faithful-BB5-entry verdict stated (buildable now / blocked on X).
5. Reproducibility block (offsets + tool invocation).

## If it does NOT work

- If records reference absolute source addresses OUTSIDE the image
  (SDRAM written by SWBL/secure side, or compressed streams needing
  runtime state): publish the record shape + every decodable record,
  mark the rest OPEN with what a phone probe would need to read,
  stop — that itself answers question (a) negatively and settles the
  Phase-2 verdict.
- If the loop turns out to be a decompressor rather than a scatter
  copier (e.g. the helper inflates regions), same shape: decode the
  container header, record what is decodable, mark the algorithm
  OPEN if it needs tables absent from the dump.

## Gotchas

- SWBL-base linking throughout the prologue/loader — disassemble
  with 0x10010000 semantics; the main body at 0x1002xxxx is the
  PRIMARY base view (§7.5's OPEN: the two views coexist in one
  image; don't mix them in one disassembly pass).
- ARM→Thumb switch at 0x524/0x528; code before is ARM, after is
  Thumb.
- The `bl` @0x504's target arithmetic: ARM BL target = PC+8+
  (imm24<<2) — compute, don't eyeball.
- Hex arithmetic: ADD, never concatenate (onenand §7.4's named
  hazard).
- Capstone constant-fold trap (`orr rX,rX,rX,lsr #N`).
- NEVER write nloader.bin or the OneNAND image (decode/verify only).
- Commit locally (docs + task status); never push.
