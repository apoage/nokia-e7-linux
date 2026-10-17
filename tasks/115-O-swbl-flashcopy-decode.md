# Task 115: SWBL flash-copy decode — the real flash-TOC reader / OneNAND LLD the shim impersonates

## Status

**DONE — executed 2026-08-31 (decode-only, no QEMU/kernel/blob changes).**

Outcome, in one paragraph: the payload disassembled cleanly end to
end (entry img+0x3D0 ARM → Thumb; 1950 insns recursive, mixed ISA,
position-independent — `adr` string refs throughout).  Full §7 in
docs/bcm2727-onenand.md.  Headlines: **(1)** SWBL stages the RAW
flash-TOC page at **0x1000C000** (not 0x101ff000 — that address is
NLoader's rebuild target) and publishes **[0x30003E78]=0x1000C000**;
the pointer cell was right, the shim's stage+curate compression is
the only fidelity delta there.  **(2)** The flash TOC's real record
format (from SWBL's own reader) is `{off@+0, size@+4, rsv, rsv,
load@+0x10, name[12]@+0x14}` — 32 entries, 0xFF terminator at device
0x414; the doc's old "20-byte header" parse paired each name with the
next record's location (e.g. the loader is TOC-named 'ADA';
'RAP3NAND' = nloader.bin@0x4000, md5-verified).  **(3)** The vtable
gap 0x1001E000-0x1001F000 = nloader.bin file 0xE000-0xF000 under
SWBL's load base 0x10010000 — and the genuine FSR-style LLD is
**PRESENT in the dump** there (§1.1's "erased" had mis-located the
zero region, which is file 0x1E000).  **(4)** OneNAND base = cs<<26
= **PA 0** for CS0 (refutes 0x20010000 as the boot-time base; the §2
contract stands as the kernel-era mapping; register deltas: IDs
+0x7800, page-class +0x780A, status +0x1FE00).  **(5)** 0x100070xx
service block NOT populated by SWBL (unreferenced bounds accessor
{0x10006000, 0x10007000} only); shadow list 0x10024178 NOT staged
(the address lands inside the NLoader copy).  Also: BB5 header
decoded (+0x148 = entry field, family-wide incl. nloader.bin; +0x20
= last-content offset 0x1D2F; 'NALO' tag), boot sequence with all
subsystems (XTI enable + XTI-RAM logging ch 0xEF → 0x401EF000, CBUS
bridge 0x20030700, DDR init 0x09000000 with bank sizes →
0x30003E60/64, secure-monitor auth via SMC → ROM 0xFFFD120C,
args-restore launch trampoline), and the handoff block 0x30003E00-
0x30003E90 map (memset by SWBL itself).  Deliverables: doc §7 +
fidelity table + reproducibility block, §1.1/§1.4 annotations,
usb-flashmode §6 items 1-2 notes, generic tool
tools/arm_recurse_dis.py.  Parking-lot candidate flagged (NOT done
here): load NLoader at 0x10010000 so the genuine LLD slots work
unshimmed.

**IDLE-TASK PROMPT (paste into the queue):**
> Execute tasks/115-O-swbl-flashcopy-decode.md in /home/lukas/ps3/e7
> (read it and docs/bcm2727-onenand.md first — §1.1 the LLD vtable gap,
> §1.4 the flash-TOC reader story, §3 the blobs being reconciled
> against, §5.2 item 2 the shadow-list gate, §5.3 the shim's curation
> rules). One job: decode SWBL's real flash copy — `firmware-re/
> nokia-e7-onenand.img` file 0x140800-0x142A00 (flash-TOC entry 0
> 'SWBL', dev 0x800, size 0x2200, ~0x1E00 effective — tail is zeros;
> BB5 header magic 0x809795A3, 20-byte signature at +0x24, strings
> plaintext). Parse the header, pin the ISA + entry point, disassemble
> everything, and decode the boot logic the QEMU shim impersonates:
> MuxOneNAND detect/config (strings 'MuxOneNAND (CS%d) configured
> (%d kHz)', 'detected (Mfr: %04X …)'), loader_sdram_init / DDR config,
> 'Reading TOC from block %d' (the real flash-TOC read + any curation),
> 'NLoader copied to SDRAM' + authentication + launch, 'Copying page
> from %d to %d', XTI enable ('XTI pipe cleaner'/'XTI enabled'),
> MM_SYSCFG1, 'CBUS Busy!'. Answer the five cross-doc questions in the
> file's exit criteria (SDRAM-TOC staging + pointer 0x30003e78;
> shadow image-list 0x10024178; embedded OneNAND LLD for NLoader's
> 0x1001E000-0x1001F000 vtable gap; the 0x100070xx staged service
> block; the OneNAND base constant). Deliver docs/bcm2727-onenand.md
> new §7 'SWBL decoded' with the house CONFIRMED/INFERRED/OPEN legend
> plus a SWBL-vs-shim fidelity table, and annotate §1.4. NO QEMU,
> kernel, or blob changes — decode only. Stop per the file's clause if
> the code region resists disassembly (encrypted/compressed). Commit
> when done.

## Why

The USB hardware rounds (2026-08-31, docs/usb-obs-2026-08-31-normalboot.
txt) surfaced a by-product bigger than their brief: **SWBL's flash copy
is NOT erased**. Everything our `emulation/qemu-e7sw/` shim fakes — the
flash-TOC read, the SDRAM-TOC staging, the curation, the pointer
publish — is real code sitting 0x800 bytes into the OneNAND dump, plus
what the shim never modeled (SDRAM init, OneNAND mux/config, XTI setup,
NLoader copy + authentication). Decoding it either validates the shim's
§5.3 curation rules from the source of truth or exposes fidelity gaps;
and the strings suggest SWBL may also settle two standing OPENs outside
the onenand doc: the real OneNAND LLD (NLoader's vtable gap payload,
§1.1) and the 0x100070xx staged service block (the USB descriptors +
callback table, usb-flashmode §6 items 1-2).

## Ground truth already verified (2026-08-31, do not re-derive)

- Flash TOC (device 0 = file 0x140000), 14 entries parsed: `SWBL`
  dev 0x800 size 0x2200 (entry 0); the shim's 9 staged entries = TOC
  entries 0,1,3-9; the shim drops `KEYS`@0xFFF40000, `GENIO_INIT`@
  0x1200000, `SOS+IVE3A`@0x1238000, `SOS+IVE3B`@0xC0000,
  `SOS*UPDAPP`@0xFFF41000 (its ≥1 MiB + name[3] '*'/'/' rules).
- SWBL head (file 0x140800): `a3959780 02000000 16230298 f6181350
  ad6e0700 f2ff9a00 68010000 d0030000 | 2f1d0000 | <20 B sig 78c9774a
  … 91245d03>` — note the word 0x1D2F at +0x20 ≈ the effective image
  extent (last nonzero byte at +0x1DFF); candidates for size/load/
  entry fields are the words at +0x18/+0x1C (0x168/0x3D0) — raw
  observation, not decoded.
- String map (file offsets; all plaintext, cluster 0x141293-0x14136C
  then 0x1416A7+, with pointer-table-looking words immediately before
  the string blob, e.g. `80240100 80510100` before 'MuxOneNAND'):
  0x141293 'Address : %08X, size : %d' · 0x1412B0 'NLoader copied to
  SDRAM' · 0x1412C8 'NLoader authentication failed' · 0x1412F3 'Reading
  TOC from block %d' · 0x141314 'RAP3NAND' · 0x141320 'NLoader image
  not found' · 0x14133F 'SDRAM init failed' · 0x141354/0x141360 build
  ts '18:41:18' / 'Jul 27 2012' · 0x14136C 'RAMSET SW : (%s %s)' ·
  0x1416A7 'XTI pipe cleaner' · 0x1416BC 'XTI enabled' · 0x14176B
  'CBUS Busy!' · 0x141A1C 'MuxOneNAND (CS%d) configured (%d kHz)' ·
  0x141A5C 'MM_SYSCFG1 : %4X' · 0x141A73 'MuxOneNAND (CS%d) not found'
  · 0x141EFC 'CS%d : %2d MB (Row: %2d, Col: %d)' · 0x141F84 'DDR %d MB
  (32bit) configured (%d kHz)' · 0x14226F ' loader_sdram_init' ·
  0x142287 'Copying page from %d to %d' · 0x1422A4 'Page (%d) copying
  failed' · 0x142490 'MuxOneNAND (CS%d) detected (Mfr: %04X, Dev:
  %04X, Rev: %04X,'.
- Per-1KB entropy inside the blob: 2.6-6.8 (code-like; an encrypted
  region would sit near 7.9) — the payload is expected plaintext.

## Method

1. Re-extract the blob (`file 0x140800, len 0x2200`), parse the BB5
   header (magic + fields + the 20-B sig at +0x24), pin the payload
   start. Determine ISA + entry (SWBL is APE-side ARM1176 code — ARM
   or Thumb; detect from branch targets/string-reference idiom, don't
   assume). A generic capstone helper may be committed under `tools/`
   (task114_dis.py precedent, but its va base 0x80000000 does NOT fit
   here — parameterize); session-specific scratch stays in /tmp.
2. Disassemble the ~0x1E00 bytes. Build the string xref map first
   (pool/pointer-table words before the string blob; possibly `adr`
   idiom like NLoader — usb-flashmode §7's /tmp xref method).
3. Decode each print-anchored subsystem: MuxOneNAND init (register
   writes — the IDs read, block/page setup, and the BASE CONSTANT),
   MM_SYSCFG1, DDR/SDRAM init (' loader_sdram_init', 'DDR %d MB'),
   the TOC read ('Reading TOC from block %d' — which block, what
   geometry/LSN math), 'Copying page from %d to %d', the NLoader
   locate ('NLoader image not found' / 'RAP3NAND') + copy +
   authentication ('NLoader authentication failed') + launch, XTI
   enable, CBUS.
4. Answer the five cross-doc questions (exit criteria 3).
5. Write §7 + the fidelity table; annotate §1.4 (its "empty bx lr stub"
   story and the second/root-TOC OPEN item).

## Exit criteria

1. Header parsed (field meanings as far as decodable), ISA + entry +
   load/run address pinned or explicitly OPEN; extent explained
   (0x1D2F/0x1E00 vs the 0x2200 TOC size).
2. The print-anchored boot sequence decoded end to end with addresses.
3. The five cross-doc questions, each CONFIRMED/INFERRED/OPEN with
   evidence:
   (a) does SWBL write the SDRAM TOC at 0x101ff000 and publish the
       pointer at 0x30003e78, and WHAT curation does it apply —
       reconcile rule-by-rule against the shim's §5.3 rules;
   (b) does it stage the shadow image-list at 0x10024178 (records
       {name@+0, key@+0xC, load@+0x18, class@+0x1C}, onenand §5.2
       item 2) and from what source;
   (c) does it carry or embed the real OneNAND LLD destined for
       NLoader's 0x1001E000-0x1001F000 vtable gap (§1.1 slot map) —
       if yes, decode the real slot behaviors;
   (d) does it populate the 0x100070xx staged service block (HAL table
       [0x10007040], USB descriptors 0x10007074-0x100070B0 incl the
       VID/PID, callback table 0x100070ac, [0x100070bc]) — if yes,
       record it and drop a pointer into docs/bcm2727-usb-flashmode.md
       §6 items 1-2 (do NOT restructure that doc);
   (e) the OneNAND base constant — the model's 0x20010000 is a
       CONTRACT (onenand §2); SWBL's MuxOneNAND writes can CONFIRM or
       refute it.
4. §7 written with the fidelity table (SWBL behavior | shim today |
   gap | worth fixing?) and a reproducibility block (offsets + tool).
5. §1.4 annotated (what SWBL's reader actually is).

## If it does NOT work

- If the payload (not the strings) resists disassembly — no coherent
  control flow at any entry candidate, or a per-block entropy near 7.9
  appears under closer inspection — decode what IS certain (header,
  string map, TOC cross-check), mark the code OPEN with the evidence,
  stop. Do not publish a guessed disassembly.
- If only parts decode (e.g. the TOC path stays opaque): deliver the
  decoded subsystems + the fidelity table for those rows only; the
  questions not answerable stay OPEN on the record.

## Gotchas

- File↔device mapping: device X = file X + 0x140000; SWBL dev 0x800 =
  file 0x140800. Offsets in this file are FILE offsets unless marked.
- SWBL runs BEFORE NLoader and before the MMU — its constants are
  physical/secure-side views; do not map them through kernel VA rules.
- The bytes just before several strings are pointer-table/pool words,
  not part of the text (verified: `80240100 8051 0100` before
  'MuxOneNAND'); and a length-prefix byte may precede Symbian-idiom
  strings — check before treating offset X as the string start.
- Capstone constant-fold trap (`orr rX,rX,rX,lsr #N`) — known house
  hazard (tasks/103 era, gpio_static.py note).
- 'CBUS Busy!' relates to the secure bus (cf. the chain's secure-ROM
  net patch at 0x30000000) — a finding here is context for §4's
  emulation artifacts, not a new patch.
- Rule 4: prior claims about SWBL (incl. onenand §1.1's "SWBL's
  payload; ERASED in our dump" — true only of the NLoader-embedded
  mirror, NOT this flash copy) are smoke until re-verified from these
  bytes.
- Commit locally (docs + task status + any generic tool); never push.
