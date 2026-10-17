# Task 114: display step 1 — DVCExe image-set decode (the VPU firmware inventory)

## Status

**EXECUTED 2026-08-31 — DONE.** Registry fully decoded (no deadlock —
the walk was static disassembly, not emulation). Findings:
`docs/bcm2727-display-recon.md` **§7** (§5 step 1 marked DONE with
pointer). Headlines:

- The registry is TWO modules: `vc_exe_registry.dll` (ROM 0x22E040,
  0x25C — DVCExe: 4-slot array at RAM 0xC80104A4, RegisterImages
  0x22E1AC, GetImages 0x22E1D8) + `signed_2727b1_v2.dll` (ROM 0x22E320,
  0xFAD34 — the signed image-set CONTAINER; the old "ARM HAL driver"
  reading in bcm2727-firmware-analysis.md is corrected).
- Exactly ONE set registered: "bcm2727b1", 3 images, each
  [payload]["sign"][20-byte sig]: img1 0x22E588+0x2050 (loader variant
  A), img2 0x2305F8+0x2050 (loader variant B — 3,251 bytes differ, same
  128-byte key blob), img3 0x232668+0xF6A2B (main Nucleus firmware,
  md5 84d88307…). Set span 0x22E588-0x3290B7 (1,024,047 B).
- DVCDriver::Boot() (0x8032DCB0) decoded: 2 stage blobs from RAM cells
  0xC801055C/0xC801057C (initial values OPEN — not in the dump), then
  GetImages, then img1-or-img2 (flag-selected) as "2nd stage boot
  loader" WITH its 20-byte sig, then img3 as the main image.
- Provenance verdict: ROM set ≠ IVE3B (distinct ~1.0 MB plaintext
  build; IVE3B = encrypted ~2.1 MB FOTA-side container, entropy 7.98,
  `fota_version` @+0x3E4; IVE3A = completely zero-filled). ROM set is
  the PRIMARY boot source. Flash TOC offsets corrected: 0x1238000 /
  0x0C0000 (device).

**IDLE-TASK PROMPT (paste into the queue):**
> Execute tasks/114-O-display-dvcexe-imageset.md in /home/lukas/ps3/e7
> (read it and docs/bcm2727-display-recon.md — §2 firmware evidence,
> §4 anchor list, §5 plan — first). One job: decode the DVCExe
> image-set registry from the Symbian ROM — walk the registry at
> RAM-semantics 0xC80104C0 from the constructor at ~0x22E4C0-0x22E548,
> enumerate every image entry (id, pointer, length), map each into the
> ROM blob, and reconcile the in-ROM VC firmware copy (0x22E780-0x329000)
> with the IVE3A/IVE3B flash copies (is the ROM copy == IVE3B, a
> fallback set, or a different build?). Deliver the findings as a new
> §7 "DVCExe image set" section of docs/bcm2727-display-recon.md with
> the house CONFIRMED/CONTRACT/OPEN legend, and update §5 step 1 to
> DONE with a pointer. NO implementation, NO QEMU/kernel changes.
> Stop per the file's clause if the registry walk deadlocks. Commit
> when done.

## Why

Display is VideoCore-mediated (task 110's verdict) and the strategic
Linux path is "keep the VC firmware, write a VCHI host". Every VPU-
side follow-up (MeSSI transport, DVCDriver boot protocol, and
eventually the VPU's own ive_dsi.c) needs to know WHICH firmware
image set the phone actually boots and where each piece lives. This
task is the inventory that unblocks them — §5 calls it small and
"unblocks every VPU-side task".

## Method

1. Locate the registry constructor (anchors §4: ~0x22E4C0-0x22E548,
   registry cell 0xC80104C0) — note the 0xC80xxxxx RAM-semantics
   addresses are the ROM image's load-time view; reconcile VAs
   carefully against the flat dump (romdumpplus.dmp is va
   0x80000000; the VC firmware region is 0x22E780-0x329000 in that
   image — the doc's bounds, pinned where ARM literal pools resume).
2. Disassemble the constructor (ARM, ROM) — extract the entry-array
   base and the record shape; enumerate (id, ptr, len) triples.
3. Map each image: bounds within the ROM dump, magic/build strings,
   sizes; note which entries point outside the ROM copy (flash?
   runtime-loaded?).
4. Reconcile with the flash copies: the OneNAND dump's IVE3* entries
   (the shim's curation excludes SOS+IVE3* variants — onenand doc
   §5.3; compare against firmware-re/nokia-e7-onenand.img at the
   TOC's IVE3A/IVE3B offsets). md5 the ROM copy vs the flash copies.
5. Write §7: the table, the provenance verdict (== IVE3B / fallback /
   distinct build), and what step 2 (MeSSI decode) should target.

## If it does NOT work

Registry shape resists decode (obfuscated indirection, compressed
pointers): deliver the constructor disassembly + what IS certain
(even a partial entry list with bounds beats nothing), mark the rest
OPEN, stop. Do not guess pointers into 826 KB of VPU code.

## Gotchas

- ROM va base 0x80000000; VC firmware is ARM-side bytes but VPU code
  — do NOT disassemble VPU payloads as ARM (leave their contents
  alone; only bounds/ids/strings matter here).
- Capstone constant-fold trap (`orr rX,rX,rX,lsr #N`) on the
  constructor walk.
- The 0xC80104C0-style addresses are RAM-semantics — cross-check with
  §4's anchor list before dereferencing.
- Old notes are smoke until verified from binaries (rule 4).
