# Task 118: run the GENUINE OneNAND LLD — retire the lld.bin trampolines

## Status

**DONE + VALIDATED (executed 2026-09-02).** Phase 0 decode, Phase 1
landed (genuine FSR LLD serving all installer slots, trampolines
retired), chain boots `e7:~#` under plain TCG and `-icount`;
evidence `emulation/qemu-e7sw/boot-lld-task118.log` committed.
Phase 2 attempted and walled precisely (prologue linked for the SWBL
single-copy layout; §7.5 prologue note) — Phase 1 entry retained.
Full story in docs/bcm2727-onenand.md §1.1 (task-118 decode block),
§2 (model additions incl. the PA-0 CS0 alias), §3 (blob retirement),
§4 (task-118 ledger rows), §5 (re-validation), §7.5/§7.8.

**IDLE-TASK PROMPT (paste into the queue):**
> Execute tasks/118-O-genuine-lld-unshim.md in /home/lukas/ps3/e7
> (read it and docs/bcm2727-onenand.md first — §1.1 the vtable slots,
> §3 the blobs, §4 the patch ledger, §5.3 the chain shape, and §7.5/
> §7.8 task 115's LLD-gap decode + fidelity table). One job: make the
> chain's NLoader vtable calls land in the GENUINE FSR LLD code that
> has been in nloader.bin all along (file 0xE000-0xF000, linked for
> 0x1001E000-0x1001F000 — pre-verified: all seven installer slots
> resolve to real Thumb prologues there), and retire the task-104
> trampolines. Phase 0 (decode gate, mandatory first): disassemble the
> 4 KB region, decode the seven slots' behaviors and the register
> sequence (which OneNAND offsets it touches — expect the mainline
> +0x1Exxx windows; compare against §7.3's SWBL-driver deltas); extend
> the bcm2727 OneNAND model ONLY if the real LLD reads a window the
> model lacks. Phase 1 (the landed shape): board maps nloader.bin file
> 0xE000-0xF000 at 0x1001E000 (replacing vt.S), shrink/retire
> lld.bin's trampolines accordingly, re-home the search-read dst-guard
> from blob-side to a one-word board patch (the 0x101ff000 literal at
> 0x10030ec4 → 0x10201000 — ONLY that literal; the build-TOC/copy-walk
> literals at 0x1003152c/0x10031d20/0x1003222c stay), keep or drop the
> shim's geometry pre-fill per what the trace shows, fix the board's
> wrong-by-0x10000 SWBL load-base comment. Rebuild blobs (build.sh) +
> QEMU (qemu-rebuild.sh). Exit criteria: chain boots to `e7:~#` under
> plain TCG AND `-icount`, with E7SW_DEBUG/ONENAND_DEBUG evidence that
> the vtable calls execute the real LLD (its own register sequence in
> the trace, IDs read where IT reads them); §4 ledger + §7.8 fidelity
> table updated; boot log committed vs the task-104 baseline. Phase 2
> (stretch, only if Phase 1 is green): enter NLoader via its real BB5
> entry 0x10010000+0x3D0 (prologue/relocation) instead of main — any
> prologue behavior the model can't satisfy → report and keep Phase 1
> as the landed state. NEVER write firmware-re/nokia-e7-onenand.img or
> nloader.bin. Stop per the file's clauses. Commit when done.

## Why

Task 115's biggest correction: §1.1's "SWBL's payload; ERASED in our
dump" was a mis-location — under SWBL's confirmed NLoader load base
0x10010000, the vtable slots (0x1001E5F3 … 0x1001E9FF) resolve to
nloader.bin file 0xE000-0xF000, real FSR-style LLD code. The chain's
lld.bin trampolines exist only because the double-load never mapped
that file range to 0x1001E000. Swapping the trampolines for the real
4 KB removes the last impersonated NLoader subsystem: every flash
read in the chain then runs genuine phone code end to end
(NLoader → BML → real LLD → our register model).

## Ground truth already verified (2026-09-01, do not re-derive)

- All seven slots hit real Thumb prologues (`b5xx` push frames) at
  file 0xE533/0xE5F3/0xE6DB/0xE903/0xE97D/0xE9FF/0xEAB1; the region
  file 0xE000-0xF000 carries 3847 nonzero bytes; file 0x1E000-0x1F000
  (the true zero gap) has ZERO nonzero bytes.
- Pool constants inside the region: 0x1E440 (Command) / 0x1E480 at
  file 0xE1F4/0xE638/0xEAA4/0xEED0/0xEED4 (onenand §7.5).
- The shim's SWBL-role staging (SDRAM TOC at 0x101ff000, pointer at
  0x30003e78, synthetic MCUSW slot) is INDEPENDENT of this swap — it
  stays. Running the real SWBL blob is explicitly out of scope
  (§7.3: needs the model at PA 0).

## Method

1. **Phase 0 — decode gate.** Disassemble file 0xE000-0xF000 (linked
   base 0x1001E000; Thumb; `tools/arm_recurse_dis.py` takes a base
   argument — commit tweaks if generic). Deliver the seven slots'
   protocols: register sequence per operation (RESET/READID/READ
   flows, which offsets, the success/failure return conventions vs
   §1.1's decoded contract: `ret>>16 == 0x1100` = success etc.), the
   devinfo cells slot+0x10 fills, what slot+0x14 (block-class/BMI)
   and slot+0x40 (read-with-spare) do. Check every OneNAND window it
   touches against the model (§2): if it reads something the model
   lacks (candidate families: ID windows — mainline +0x1E000 vs
   SWBL-driver +0x7800; status +0x1FE00; SA2 dual-die split), extend
   `hw/arm/bcm2727.c` minimally behind the existing contract and say
   so in §2. **Gate: if the LLD's protocol needs data the dump cannot
   provide (e.g., real spare-area content — the model's 0xFE spare
   contract is a stand-in) or calls secure-side services, STOP after
   the decode, record findings + a revised plan in §7.8, do not
   force the swap.**
2. **Phase 1 — the swap.**
   - Board (`nokia_e7.c`): replace the vt.S trampoline load with a
     4 KB copy of nloader.bin[0xE000:0xF000] to 0x1001E000; fix the
     SWBL load-base comment (0x10010000, per §7.5 — the comment
     currently claims 0x10020000, wrong by 0x10000).
   - Blobs (`emulation/qemu-e7sw/`): drop the trampolines from vt.S /
     the build; lld.c's body functions retire WITH them unless the
     boot trace proves something still calls them — then shrink, not
     delete, and document. `build.sh`, regenerate `e7sw_blobs.h`,
     rebuild QEMU (`emulation/qemu-rebuild.sh`; no new source files,
     so no build.ninja surgery).
   - Re-home the dst-guard: the blob-side `lld_read` guard (§4
     task-106 row) disappears with the trampolines. The searches must
     still not clobber the staged TOC: patch the search reader's
     literal — the word `0x101ff000` AT 0x10030ec4 → `0x10201000`
     (board patch; the OTHER three 0x101ff000 literals at
     0x1003152c/0x10031d20/0x1003222c must stay). Add the row to §4.
   - Shim: keep the SDRAM-TOC staging + pointer publish; the geometry
     pre-fill becomes conditional on evidence (if the real GetDevInfo
     fills the cells before first use, drop the pre-fill; else keep
     and note why).
3. **Validate.** Boot plain TCG (ttyAMA0, root=mmcblk0) AND
   `-icount`; confirm `e7:~#`; interactive `/proc/interrupts` sanity
   (tick/uart/mmc/i2c ids intact, Err: 0). Evidence that the REAL LLD
   ran: `ONENAND_DEBUG=1 E7SW_DEBUG=1` trace shows the vtable calls'
   register sequence matching the Phase-0 decode (not the trampoline
   shape), incl. wherever it reads IDs. Diff the boot log vs
   `emulation/qemu-e7sw/boot-baseline-task104.log` — expect the
   trampoline-era lines to change, everything else stable; commit the
   new log as `boot-lld-task118.log`.
4. **Phase 2 (stretch)** — only after Phase 1 is green: board enters
   NLoader at 0x10010000+0x3D0 (real BB5 prologue + relocation)
   instead of 0x1003259b. Instrument (E7SW_TOCWATCH precedent,
   gdb-multiarch `-S -s`) and see how far the real prologue gets; the
   §7.5 OPEN item (relocation semantics) is the deliverable either
   way. Any wall (MMU config, security, unsupported ops) → revert to
   the Phase-1 entry, report the wall precisely.

## Exit criteria

1. Phase-0 decode in the doc (§1.1 corrected in place: the gap is
   real code; slot protocols; model-extension list or "none needed").
2. Chain boots to `e7:~#` TCG + icount with the genuine LLD serving
   all seven slots; evidence log committed.
3. Trampolines gone from the build (or shrunk with the reason);
   dst-guard re-homed as the §4 one-word patch; board comment fixed.
4. §4 ledger + §7.8 fidelity table rows updated; §2 model notes if
   extended; HANDOFF untouched (main session folds it).
5. Phase 2 outcome recorded either way (landed or wall + revert).

## If it does NOT work

- Phase-0 gate trips (protocol needs absent data / secure calls):
  decode + revised plan into §7.8, keep the chain unchanged, stop.
- Real LLD runs but the boot wedges at a specific call: bisect with
  the trace + TOCWATCH/gdb; if the wall is a model gap, a minimal
  model fix is in scope — a MODEL REDESIGN is not (stop, report).
- Slot semantics disagree with §1.1's decoded contract (e.g., the
  success convention differs): the REAL code wins — correct §1.1,
  adjust the BML-side expectations only if the chain demands it, and
  document the correction loudly (this would be a §1.1 decode error
  surviving since task 104).

## Gotchas

- The 4 KB must land at EXACTLY 0x1001E000 (the LLD's linked base —
  the installer's slot addresses and the code agree on it).
- 0x1001E000-0x1001F000 currently holds the vt.S trampolines — the
  swap replaces that range wholesale; nothing else may claim it.
- The three OTHER 0x101ff000 literals are load-bearing (§5.1) — patch
  only 0x10030ec4.
- Rebuild order: build.sh → regenerate e7sw_blobs.h → qemu-rebuild.sh
  (blobs are embedded; HANDOFF QEMU-rebuild note).
- Capstone constant-fold trap; Thumb throughout the LLD; va-vs-file
  hex arithmetic is a named hazard (onenand §7.4 re-verification
  note) — ADD, never concatenate.
- NEVER write the OneNAND image or nloader.bin (decode/verify only).
- QEMU tree is gitignored by convention; commit outer-repo artifacts
  (docs, task status, boot log, blob sources under
  emulation/qemu-e7sw/) locally; never push.
