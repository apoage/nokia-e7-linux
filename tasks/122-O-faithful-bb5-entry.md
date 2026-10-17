# Task 122: the faithful BB5-entry chain — run NLoader's real prologue, scatter loader, and init tree

## Status

**DONE + VALIDATED — executed 2026-09-05.**  The chain now enters
NLoader through the genuine BB5 ARM entry 0x100103D0 and boots to
`e7:~#` under plain TCG and `-icount`.  Evidence:
`emulation/qemu-e7sw/boot-bb5-task122.log` (TCG, E7SW_DEBUG=1
ONENAND_DEBUG=1, interactive mount//proc/interrupts at the prompt) and
`docs/boot-122-icount-ttyama0.txt` (icount control, identical
1202-line NLoader flow).  Full shape, the new-print decode table, and
the validation record: docs/bcm2727-onenand.md §5.3.1; rewritten §4
ledger with the VA-convention note.  What the plan could not foresee
(two walls found and satisfied within scope):

1. **The init tree's EM-ASIC probe** (service[0] via the HAL thunk at
   file 0xDAC) reads the EM identity over the CBUS bridge 0x20030700
   and hangs DELIBERATELY without a valid type ("No EM ASIC", b . at
   file 0x726).  Satisfied by a small named-register model
   (`bcm2727.cbus` in bcm2727.c, §2 task-122 additions) with a
   Pearl-J identity (bus1.reg0 = 0x1050: type 0x10/version 5/rev 0) —
   the phone's EM codename per the NLoader string table, ROFS-core
   strings, and hardware-lore's "PEARL N2300" PMIC family.  CONTRACT,
   like the OneNAND base.
2. **The genuine curation is no longer starved** — the single map
   feeds the shadow-list address 0x10024178 with the image's own
   in-image list (file 0x14178; §5.2 item 2's "starved in emulation"
   premise retires), so NLoader re-emits its own 4-record SDRAM TOC
   and its validator/allocator (0x1002149a from the walk at
   0x10021ea0: (load+entry)&(size−1)==0 + region containment)
   decides the MCUSW load.  The chain now preloads its trampoline at
   NLoader's own allocation **0x101FFC30** (heap end 0x101FEFFF →
   0x10200000, minus the record's entry 0x3D0); the zImage stays in
   SDRAM.  The MCUSW copy is skipped (record size 0), which is what
   keeps the trampoline alive to the launch.

Phase-0 deviations from the prompt (both documented in §4/§5.3.1):
the park trampoline moved to the image's confirmed zero pad
(0x10026000 = file 0x16000), NOT SRAM — a 4-byte Thumb BL retarget
spans ±16 MiB and the suggested 0x40201000 is also the shim's
pagebuf; the LR points at a board-written args-restore stub
(0x40202100) rather than the entry directly — the shim is C and
clobbers r0-r3 (the stub mirrors SWBL's own launch trampoline,
§7.2).

**IDLE-TASK PROMPT (paste into the queue):**
> Execute tasks/122-O-faithful-bb5-entry.md in /home/lukas/ps3/e7
> (read it and docs/bcm2727-onenand.md first — §4 the full patch
> ledger, §5.3 the current chain shape, §7.2 SWBL's launch shape,
> §7.5 the load-base story + Phase-2 wall, §7.10 the scatter plan and
> its green-lit verdict). One job: enter NLoader through its genuine
> BB5 entry and let the real prologue + scatter loader + init tree
> run — per §7.10's verdict: map nloader.bin file 0-0x1C000 at
> 0x10010000 as the SINGLE image mapping (retiring the mirror at
> 0x10000000, the primary at 0x10020000, AND the task-118 LLD map —
> file 0xB000-0x10000 lands at its linked 0x1001B000-0x10020000 free
> under the single map), keep the shim as the entered-first SWBL-role
> stand-in but point its LR at the ARM entry 0x100103D0 (no Thumb
> bit) with ROM/SWBL args r0-r3 (zeros unless evidence demands
> otherwise — main re-reads the reason from hardware per §5.2), and
> set the model's OneNAND VerID to a 0x0000-class value (keep
> Mfr/Dev = EC/0068) so the LLD open first-matches the 8192-block
> EC/0x68/0x0000 record — keeping 0x0121 silently selects 4096-block
> geometry (§7.10(b) re-verification note). Phase 0 (mandatory
> pre-flight, before any code): walk EVERY §4 ledger row and classify
> it retire / keep-rebased / move — the staged command words +
> device-ID record retire (the LZ decompressor overwrites them with
> the genuine twelve words + 33-record table), the three helper
> repoints and the even-dst-copy repoint retire (the single map puts
> every linked VA on its true bytes), the probe-done fixup retires
> (record 3 zero-fills it), the hang-point NOPs / MMU triplet / crypto
> stubs / auth gates KEEP (file-relative — recompute their VAs under
> 0x10010000), and the search-dst park trampoline MOVES: its current
> home 0x10015000 = file 0x5000 = LIVE prologue/helper bytes under
> the single map — relocate to free SRAM (e.g. 0x40201000, above the
> shim+config block) and retarget the search helper's call to it.
> Deliver the classification table in the task file BEFORE touching
> the board. Phase 1: implement, rebuild QEMU (qemu-rebuild.sh; no
> blob rebuild needed unless shim.c changes). Phase 2: validate —
> boot plain TCG AND -icount to `e7:~#`; evidence that the real
> prologue ran (boot diff vs boot-baseline-task104.log will NOT be
> one-line this time: expect new pre-main prints from the genuine
> init tree — decode and identify each new line with its address
> instead of suppressing; also verify via QMP/E7SW_DEBUG that
> 0x10007000-0x100074DC post-scatter holds the genuine globals —
> twelve command words, 33-record ID table, USB descriptors
> VID 0x0421/PID 0x0354 — and that the LLD open matched the
> 8192-block record). Exit deliverables: evidence logs
> (boot-bb5-task122.log TCG + docs/boot-122-icount-ttyama0.txt),
> §4 ledger rewritten (retired rows struck with reasons, kept rows at
> their real VAs, ONE prominent convention note: all pre-122 VAs in
> the docs are cited under the old 0x10020000 display convention,
> real VA = cited − 0x10000 — do NOT mass-rewrite historical text),
> §5.3 extended with the task-122 shape, §7.5's OPEN annotated
> resolved-by-experiment, §7.8 fidelity rows updated. NEVER write
> nloader.bin or the OneNAND image. Stop per the file's clauses: any
> wall the model can't satisfy with a SMALL named register addition →
> document precisely, revert cleanly to the task-118 shape (the tree
> must stay bootable), record the wall like §7.5's probe did. Commit
> when done.

## Phase-0 classification table (2026-09-05, before any code change)

Convention: "old VA" = the pre-122 doc/board display base 0x10020000+file;
"new real VA" = the single-map base 0x10010000+file (= old − 0x10000).
Every patch is file-relative, so KEEP rows keep their instruction encodings
verbatim (branch displacements are file-relative); only the write address
moves.

**A. Task-093-era patches that REMAIN (§4) — all KEEP, rebased:**

| row | old VA | file | new real VA | class |
|-----|--------|------|-------------|-------|
| BSI check hang NOP | 0x10020748 | 0x748 | 0x10010748 | KEEP |
| HW/SW version mismatch b→ret (0xe7d4, target file 0x1626) | 0x1002167a | 0x167a | 0x1001167a | KEEP |
| "No valid start-up reason" NOP | 0x100325f2 | 0x125f2 | 0x100225f2 | KEEP |
| delay-loop NOP | 0x10022bfe | 0x2bfe | 0x10012bfe | KEEP |
| SOS+DECOMP decompressor hang b (0xe7f8 → file 0xf4f2) | 0x1002f4fe | 0xf4fe | 0x1001f4fe | KEEP |
| MMU-bypass triplet (NOP×2 each) | 0x1002172e/32/38 | 0x172e/32/38 | 0x1001172e/32/38 | KEEP |
| crypto stubs mov r0,#0; bx lr ×6 | 0x10033b84+8k | 0x13b84 | 0x10023b84+8k | KEEP |
| secure-ROM net (bx-lr RAM 0x30000000, region-level) | — | — | — | KEEP as-is (not file-relative; also serves the prologue's SYS stack sp=0x30003E00) |
| config-block zeroing + record base (chain) | 0x30003e00 | — | — | KEEP as-is (prologue saves r0-r3 at E00-E0C = zeros over zeros; SYS sp=0x30003E00 grows DOWN, below the block — cells E60/E78 survive) |
| handoff set: NOPs at 0x1003246a/0x10031840/0x1002fcd6/0x10032478, b_skip 0xe020 at 0x1003071a, bx r1 at 0x10033b58 | old VAs | 0x1246a/0x11840/0xfcd6/0x12478/0x1071a/0x13b58 | 0x1002246a/0x10021840/0x1001fcd6/0x10022478/0x1002071a/0x10023b58 | KEEP (all genuine-NLoader walls: auth gates, PMM/LDSP/GENIO bypasses, launch jump) |

**B. Task-104 ADDITIONS — KEEP, rebased (documented walls):**

| row | old VA | new real VA | class |
|-----|--------|-------------|-------|
| BML_Open BMI failure beq→b (0xe004) | 0x1002f310 | 0x1001f310 | KEEP (dump lacks spare area, §1.3) |
| BML-close NOP on the error path | 0x10028cf0 | 0x10018cf0 | KEEP (same BMI wall) |
| SOS+DECOMP auth gate beq→b (0xe003) | 0x10031a66 | 0x10021a66 | KEEP (secure-ROM stubs return 0) |
| class-selection spin NOPs | 0x10031778/0x100317ae/0x100317d0 | 0x10011778/0x100117ae/0x100117d0 | KEEP (auth-walled kernel-class images absent) |
| startup-reason bit-31 NOP | 0x10031784 | 0x10011784 | KEEP |
| SDRAM-TOC end-marker memset NOP | 0x10031f78 | 0x10021f78 | KEEP (record curation starved, §5.2 item 2) |
| search-read dst guard = park trampoline (task-118 re-home) | 0x10015000 + call 0x10030ae4 | → see row F1 | **MOVE** |

**C. Task-106 blob-side rows (shim.c) — KEEP unchanged** (the shim blob
survives as the SWBL-role stand-in; not file-relative): synthetic MCUSW
slot 0 offset 0xFFFF0000; flash-entry curation name[3] '*'/'/'.

**D. Task-118 rows:**

| row | old site | class | why |
|-----|----------|-------|-----|
| LLD map (file 0xB000-0x10000 @ 0x1001B000) | board load | **RETIRE** | the single map (file 0-0x1C000 @ 0x10010000) lands that range at its linked 0x1001B000-0x10020000 natively |
| helper repoints (bl 0x10022860→0x10032860, 0x100228c6→0x100328c6, blx 0x100232b0→0x100332b0) | 0x1001b1b4/b1c0/b1d8 | **RETIRE** | the linked VAs 0x10022860/0x100228c6/0x100232b0 (= file 0x128xx/0x132xx) now hold their true bytes under 0x10010000+file |
| even-dst-copy repoint (bl 0x10010680→0x10000680) | 0x1001e0e0 | **RETIRE** | linked VA 0x10010680 = file 0x680, now true bytes |
| staged command words (5 × u16 @ 0x100071E4) | 0x100071e4 | **RETIRE** | scatter record 2 decompresses the genuine twelve words to 0x100071E4 before any consumer reads them |
| staged device-ID record (0x28 B @ 0x100071FC) | 0x100071fc | **RETIRE** | record 2 writes the genuine 33-record table; the staged record was a combination that exists in no genuine table (§7.10(b)) |

**E. Task-112 rows:** legacy I2C override — already removed, no action.
Probe-done flag zero (0x100076F9) — **RETIRE**: scatter record 3
zero-fills 0x100074DC-0x10008E50, which covers the flag (the SWBL-role
".bss clear" the board stood in for).

**F. Moves and board staging (not all in §4's tables, recorded for
honesty):**

| row | old site | new site | why |
|-----|----------|----------|-----|
| F1. park trampoline body (6 Thumb insns + 0x2000 pool word) | 0x10015000 = file 0x5000 (LIVE code under the single map: push-style prologue bytes verified) | **0x10026000** = file 0x16000, inside the confirmed all-zero pad 0x15200-0x1BFFF (verified) | the prompt's SRAM suggestion (0x40201000) is NOT reachable: a 4-byte Thumb BL retarget spans ±16 MiB and 0x40201000-0x10020AE4 ≈ +0x3020'0000; also 0x40201000 is the shim's pagebuf (SHIM_PAGEBUF, e7sw.h). The zero pad is image territory but dead bytes (§7.10(a)) — same artifact class as the old home (which overwrote mirror pattern data). Trampoline's internal bl recomputes to the real reader 0x10020A88. |
| F2. search helper call retarget (bl) | 0x10030AE4 | **0x10020AE4** (bl 0x10026000) | file-relative move + new encoding for the new target |
| F3. HAL stub block: "return 100" stub 0x10000100, halt stub 0x10000110, vtable 0x10000200, [0x10007040] ptr, 0x10007000-0x10007038 ptr table | board staging | **RETIRE (drop all writes)** | 0x10007000-0x100074DC is scatter record 2's output — the genuine fourteen Thumb service pointers (→ file 0x6A9-0xCFB), the genuine HAL table at 0x10007040, USB descriptors VID 0x0421/PID 0x0354. The stubs stood in for exactly this block; the genuine HAL init (file 0xdac) now calls genuine dispatchers. Nothing reads 0x100070xx before record 2 lands (prologue touches only SCTLR/VFP/mode-stacks/0x30003E00 save). |
| F4. CPU entry state | PC=shim, LR=0x1003259b (main\|Thumb), r0-r3 whatever the shim leaves | PC=shim (unchanged), **LR = 0x40202100** (board-written ARM args stub: mov r0-r3,#0; ldr pc,[pc,#-4]; .word 0x100103D0) → BB5 ARM entry 0x100103D0 | the shim is C and clobbers r0-r3; the stub restores zeros exactly as SWBL's own launch trampoline restores the ROM args (§7.2 img+0x904/0x1C54). Prologue saves them at 0x30003E00-0x0C. Free SRAM: shim ends 0x402004BE, cfg 0x40200C00+0x18, pagebuf to 0x40202000, log port 0x40208000 — 0x40202100 is clear. |
| F5. image loads | mirror 0x10000000 (nl_size) + primary 0x10020000 (nl_size) + LLD map | **single map: file 0-0x1C000 @ 0x10010000** | the §7.10 verdict — the SWBL single-copy layout |

**G. Model knob (bcm2727.c):** VerID 0x0121 → **0x0000** (all three ID
sites: READID BootRAM bytes, +0x7800 SWBL window, +0x1E000 mainline
registers; Mfr/Dev stay 0x00EC/0x0068). The genuine table's matcher is
strict three-way equality, first match wins: rev-class 0x0100 would
first-match the EC/0x68/0x0100 record and silently select 4096-block
geometry; class 0x0000 first-matches **EC/0x68/0x0000 = 8192 blocks**
(§7.10(b) re-verification). The shim's `onenand_read_id` only reports
IDs (no validation) — transparent to it.

**H. Unaffected (checked):** E7SW_TOCWATCH overlay (0x101ff000 SDRAM,
forwards into PDRAM by host-pointer arithmetic — no overlap with the
0x1000xxxx/0x1002xxxx loads); chain-mode trampoline/zImage/DTB/initrd
(0x80010000+ SDRAM); shim config block 0x40200C00; mode-stack pool
0x1002F4C0-0x1002F500 (free PDRAM above the 0x1002C000 copy end —
SVC sp = 0x1002F4D0 per the prologue's pool at file 0x4C4-0x4D8);
prologue's SYS sp = 0x30003E00 inside the secure-ROM net RAM.

## Why

Task 121 proved the faithful entry is buildable from the stock dump:
the zeros are final pad, the image is 0x10010000-linked, and the
scatter loader is a globals initializer whose contents are all
in-image. Running it retires the last emulation scaffolding on the
boot chain: after this, everything from entry to `e7:~#` — prologue,
scatter/LZ globals, init tree, BML, genuine LLD — is phone code, and
the §4 ledger shrinks to the genuine NLoader walls (auth gates, BMI,
mode gates) that stand for documented reasons. Bonus that falls out
free: 0x10007074-0x100070B0 gets the REAL USB descriptors (VID/PID
0x0421/0x0354), closing usb-flashmode §6 item 1's "not in the image"
story for the chain context, and §7.5's relocation-semantics OPEN
resolves by experiment.

## Method

1. **Phase 0 — the classification table.** For every §4 row (all
   eras): does its target bytes exist at the right place under the
   single 0x10010000 map? Retire = the real hardware mechanism now
   provides it (scatter records, true linked VAs). Keep = a genuine
   NLoader wall (auth/MMU/hang/mode) — recompute the VA, apply
   file-relative. Move = needed but its current address becomes live
   image (the park trampoline — see prompt). Write the table into
   this task file, then implement.
2. **Phase 1 — board + model.** Single map (file 0-0x1C000 →
   0x10010000); delete mirror/primary/LLD-map loads; shim entry
   unchanged (PC=shim) but LR = 0x100103D0; r0-r3 = 0; relocate the
   park trampoline to SRAM 0x40201000 (verify the shim/config block
   leaves it free; grow nothing into 0x40208000 = the shim-log port)
   and repoint the search helper's call; model VerID → 0x0000-class
   (one constant in bcm2727.c; keep the ID-register debug print
   honest). Check the E7SW_TOCWATCH overlay is unaffected.
3. **Phase 2 — validate + decode the new prints.** Boot TCG (log to
   boot-bb5-task122.log with E7SW_DEBUG=1 ONENAND_DEBUG=1) and
   -icount; expect `e7:~#`. Compare the NLoader print flow against
   the task-104 baseline: pre-main lines may be NEW (genuine init
   tree + RAM-veneer consumers + possibly the XTI-enable path —
   §7.2's SWBL analogues) — identify each with its code address;
   post-main flow should match the 118/121-era flow modulo the
   VerID-driven ID-match record. Verify the globals block content
   post-scatter (QMP pmemsave of 0x10007000/0x4DC against §7.10's
   published decode) and the geometry cells (8192/64/4).
4. Docs per the prompt's exit list; HANDOFF untouched (main session
   folds).

## Exit criteria

1. Phase-0 classification table committed before code changes.
2. Chain boots `e7:~#` under TCG + icount via the genuine entry;
   evidence logs committed.
3. Post-scatter globals verified byte-level (twelve words, 33-record
   table, VID/PID block); LLD open matched the 8192-block record
   (geometry cells = §1.2).
4. Every new pre-main boot line identified (address + what it is).
5. §4 rewritten with the convention note; §5.3 extended; §7.5 OPEN
   annotated; §7.8 rows updated.
6. Tree bootable at every commit (revert path exercised if walled).

## If it does NOT work

- Init tree walls on unmodeled MMIO: a SMALL named register addition
  (the 118 precedent: +0x1FE00 etc.) is in scope; a whole subsystem
  (DDR PHY calibration, clock-pll convergence loops) is NOT —
  document the exact site + what the code polls, revert to the
  task-118 shape, record the wall as §7.5's probe was recorded.
- Prologue misbehaves with zero args (reads r0-r3 as pointers):
  inspect §7.2's save sites — if a nonzero arg is structurally
  required, derive the minimal safe values and document; if
  undecidable, that is a wall (same revert path).
- Boot regresses post-main (e.g. the genuine command words alter a
  code path the staged five did not): bisect with ONENAND_DEBUG +
  E7SW_DEBUG; if a genuine-flow divergence needs a policy call
  (which we did not decode), stop and report rather than patch
  blindly.

## Gotchas

- **0x10015000 is live code under the single map** (file 0x5000,
  prologue/helpers) — the park trampoline MUST move first or the
  boot dies confusingly inside the scatter loader's own bytes.
- Entry is ARM (§7.1) — no Thumb bit in the entry address; the
  prologue switches modes itself.
- Apply patches file-relative; recompute every kept row's VA under
  0x10010000 and cite BOTH (new real VA + old convention VA) in the
  rewritten ledger.
- The mode-stack pool (0x1002F4C0-0x1002F500) means early sp sits at
  0x30003E00 per §7.2's SWBL shape — the board already provides this
  scratch RAM; don't let new mappings collide with 0x3000xxxx.
- The USB-descriptor side effect: with real descriptors staged, any
  future chain run that enters NLoader's USB task would
  self-enumerate differently — note it, don't chase it here.
- Rebuild order: qemu-rebuild.sh only (blobs unchanged unless the
  shim moves — then build.sh → e7sw_blobs.h → QEMU).
- NEVER write nloader.bin / the OneNAND image. Hex arithmetic: ADD,
  never concatenate. Capstone constant-fold trap.
- QEMU tree gitignored by convention; commit outer-repo artifacts
  locally; never push.
