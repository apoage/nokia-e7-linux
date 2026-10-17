# NLoader boot mechanism — full decode (custom-image path)

**Date:** 2026-08-22
**Source:** static disassembly (capstone, Thumb mode) of
`firmware-re/analysis/nloader.bin` (184 KB, base 0x10020000, entry 0x10020800),
cross-referenced with the QEMU boot harness `emulation/qemu/hw/arm/nokia_e7.c`
and lore 2026-03-07/03-08.
**Status:** all items CONFIRMED = instruction sequences present in the binary;
interpretations marked INFERRED where semantics can't be proven statically.
**Purpose:** answer "what does it take to make NLoader load a custom image"
(HANDOFF "First moves" #2).

---

## 0. Executive summary

NLoader's image loading is **table-driven and name-keyed**. The SDRAM TOC at
0x101ff000 is the single source of truth for *what* gets loaded and *where*.
A TOC entry whose name matches the boot-mode key image (`MCUSW` in normal
boot, `UPDAPP` in update mode) wins control of the machine: after
authentication, NLoader jumps to

```
entry_point = load_address + *(uint32_t *)(load_address + 0x148)
```

via a secure-ROM trampoline. Copy failures are **non-fatal** (print
"copying failed", continue), and the "load address" of an entry is computed
from a rewritten TOC field — which means **any image already resident in RAM
+ a crafted TOC entry = boot**, no flash write needed. This is the custom-
image path, proven decodable today and QEMU-testable immediately; on real
hardware it still has to clear BB5 image authentication and the write-
protected flash TOC (§6).

---

## 1. The two TOC formats (CONFIRMED)

### 1.1 Flash TOC at OneNAND 0x140000

```
20-byte header (mostly zeros), then 32-byte entries:
  +0   12   name, NUL-terminated
  +12   4   NAND offset (bytes; OneNAND page = 512 B, offsets are page-ish)
  +16   4   image size
  +20  12   flags/reserved
```

### 1.2 SDRAM TOC at 0x101ff000 (built by "Build TOC" 0x119fc)

32-byte entries, max 64 (0x800 bytes), end = name[0] == 0xFF:

| off | field | notes |
|-----|-------|-------|
| +0x00 | NAND offset | **rewritten by Copy images**: becomes `load_address − 0x101ff000` (see §2.2) |
| +0x04 | reserved / special size fallback | printed as "Reserved : %d Bytes" |
| +0x08 | image size (bytes) | normal-entry copy length (rounded up to 512) |
| +0x0C | compressed size | used when name[3] is `'*'` or `'/'` |
| +0x10 | **load address** | destination of the copy; "Load address : 0x%08X" |
| +0x14 | name[12] | `name[3] ∈ {'*', '/'}` selects the compressed/special copy path (0x1002f3b0) |

## 2. Copy images (0x11fee) — the load primitive

### 2.1 Loop semantics (CONFIRMED)

For each SDRAM TOC entry until name[0]==0xFF:

1. save `src_page = entry[+0] >> 9`, `dst = entry[+0x10]`
2. rewrite `entry[+0] = dst + 0xefe01000` — and `0xefe01000 = −0x101ff000`
   (mod 2³²), i.e. `entry[+0] = dst − TOC_base` (INFERRED purpose: §2.2)
3. print name / reserved / size / (compressed) / nand address / load address
4. copy:
   - normal: `flash_read(dst, src_page, roundup(size,512)>>9)` @ 0x10030a88
   - special (`*`/`/`): copy+decompress path @ 0x1002f3b0 with [+0x0C]
5. on failure: print "Image: %s copying failed" and **continue to the next
   entry** (branch at 0x10032176). Failure is never fatal.

The real flash page reader is 0x10030a88 → 0x1002f330 (the OneNAND driver
call). In QEMU the driver code is a zero gap (SWBL loads it), so the harness
stubs 0x1002f330 to return failure — all copies "fail" gracefully.

### 2.2 The load-address trick (CONFIRMED + INFERRED purpose)

TOC search 0x1003277c has three return modes (`r2` argument):

| r2 | returns |
|----|---------|
| 0 | pointer to the TOC entry |
| 1 | entry[+0] raw |
| 2 | **entry[+0] + TOC_base** |

Because Copy images rewrote `[+0] = load − TOC_base` (§2.1 step 2), a
mode-2 search after Copy images returns the **load address** of the named
image. Every post-copy consumer uses mode 2:

- pre_genio finds GENIO_INIT's loaded data this way
- cleanup finds the boot image's load address this way (§4)

**Search mechanics (CONFIRMED from disassembly + live memory dumps,
2026-08-22):** with r0 = 0 the base comes from `[0x30003e00 + 0x78]`;
the scan runs from base to base + 0x800 (64 entries × 0x20), stops at
`name[0] == 0xFF`, and a match is a **12-byte memcmp(name, entry+0x14)**
against 0x10033084 — so synthetic entries need the name field
**zero-padded to 12 bytes** ("MCUSW\0…\0"), and a crafted entry must sit
at an earlier slot than any same-named flash entry to win. Worked example
(dst 0x80010000): rewrite → `[+0] = 0x6FE11000`, mode 2 →
`0x6FE11000 + 0x101FF000 = 0x80010000`.

**Consequence (INFERRED, QEMU-testable):** an entry whose copy "fails" (or
never happens) but whose +0x10 is set still yields a valid load address to
all consumers — NLoader will happily authenticate and jump to an image that
was placed in RAM by other means. Copy failure being non-fatal completes
the primitive.

## 3. Hardcoded special loads inside Build TOC (0x119fc)

- `SOS+PMML` → copied to **0x10040000** (PMM loader, PDRAM)
- `SOS+DECOMP` → auth + copy to **0x1E400000** (0x79<<22; the decompressor)
- name-class searches: `ROF*` (ROFS variant), `SOS+IVE3*`, `PRIMAPP`, `COMA`
- boot-mode image selection (0x1174c): class 2 = "SOS core image",
  class 0xC = executable, class 1 = "Update Agent"; messages
  "SOS core image not found -> ", "ENOSW not found -> Normal mode",
  "Update Agent not found -> Normal boot"

## 4. The final handoff (CONFIRMED)

`main` (0x1259a) tail: `pre_genio (0x12558) → GENIO orchestrator (0x12498)
→ cleanup (0x12450)`. "cleanup" is actually the launcher:

```
0x10032452  bl 0x100303f4          ; find boot image
  ;   startup-reason bit31 set? "UPDAPP" : "MCUSW", search mode 2
  ;   → returns LOAD ADDRESS (or 0 → "SOS image not found" + hang)
0x10032464  bl 0x100305b8          ; authenticate loaded image in place
  ;   r0 != 1 → "SOS (at %08X) authentication..." + hang
0x1003247c  "Total bootup time %d msec"
0x10032486  bl 0x10030418          ; JUMP:
  ;   entry = *(u32 *)(load + 0x148) + load
  ;   globals: [0x100074c0] = entry, [0x100074bc] = config base 0x30003e00
  ;   print "Start SOS at %08X (->%08X)"
  ;   bl 0x10033b54                ; secure-ROM trampoline: switch world +
  ;                                 ; jump to entry (r0 = config, r1 = entry)
```

- `+0x148` is the entry-point *offset* field of the loaded image header
  (Symbian OS image format). An image controls its own entry point.
- 0x10033b54 is one of the six ARM trampolines at 0x10033b84−… that vector
  into the secure ROM (0x3000xxxx). In QEMU these are stubbed to
  `mov r0,#0; bx lr` — which *returns* instead of jumping, so a QEMU chain
  must patch `bx r1` (ARM) at **0x10033b58**, the ARM payload behind the
  Thumb `bx pc` entry halfword (see §6 correction), to complete the handoff.
- The config base 0x30003e00 block ("hw_conf" records: SDRAM id 4, PDRAM
  id 5, massmemory id 0x13, built by the GENIO orchestrator's config-build
  at 0x1002feb2) is passed to the booted image via r0 — the Symbian kernel's
  hardware-configuration handoff.

## 5. GENIO apply (context)

GENIO orchestrator (0x12498): `0x1002fcc0` (auth/decrypt via secure ROM),
wait loop 0x10030614, then config build 0x1002feb2 ("Beginning
configuration build", "hw_conf: SDRAM: %d MB at %08X", "hw_conf: PDRAM:
%d kB at %08X", "hw_conf: Massmemory %d: %d MB (Mfr/Dev)"). The
(address, value) pad-mux apply prints "Address : Value" — its plaintext
record is what was recovered from OneNAND 0x1C5000 (docs/genio-padtable.md);
the shipped GENIO_INIT partition itself stays BB5-encrypted (DES key
416ce871c3bd0a28 verified not to apply).

## 6. What it takes to boot a custom image

### Status: DONE in QEMU (2026-08-22, task 093)

The chain booted Linux 6.12 end to end (see tasks/093). The recipe below
was refined during execution — corrections in brackets.

### In QEMU (all pieces known, harness extension only)

1. Board file loads the custom image (e.g. a Linux zImage trampoline) into
   guest SDRAM and writes a TOC entry `MCUSW` with `[+0x10] = image address`.
   Leave `[+0]` alone — Copy images rewrites it; the (failing) copy is
   non-fatal and mode-2 search still resolves the load address.
   [CONFIRMED: entry must be at slot 0 to win against a flash-image MCUSW;
   rewrite = dst + 0xefe01000, search = [+0] + 0x101ff000.]
2. Trampoline layout: `*(u32 *)(image + 0x148) = 0` so NLoader's computed
   entry = image start; trampoline sets r0=0, r1=0xFFFFFFFF, r2=DTB
   address (Linux boot protocol) and branches to a zImage appended behind
   the 0x150-byte mini-header. MMU is already off ("Disable MMU" runs at
   the end of Copy images).
3. Two in-memory patches: cleanup auth bypass at 0x10032468 [the NOP goes
   on the bne at 0x1003246a], and 0x10033b54 → `bx r1` to perform the
   jump. [CORRECTION: the trampoline at 0x10033b54 is entered in THUMB
   (a bl, not blx, from 0x10030436); its entry halfword is `bx pc` and
   the ARM payload starts at +4. The `bx r1` patch belongs at
   **0x10033b58** — patching 0x10033b54 itself Thumb-decodes as garbage
   Thumb-2 and derails the chain.]

Additional gates found between Copy images and the launcher, all
image-auth shaped (found → auth → hang; "not found" is the clean exit):

| gate | site | bypass |
|---|---|---|
| PMM loader (calls into image @0x10040000 after auth) | 0x10031840 | NOP the `bne` → "PMM Loader not found" |
| LDSP auth (GENIO orchestrator 0x1002fcc0) | 0x1002fcd6 | NOP the `bne` → not-found return |
| GENIO_INIT auth (0x1003070c) | 0x1003071a | `beq`→`b` → forced not-found return |
| launcher boot-time check (ticks + 0x14 == 0 → return, no jump) | 0x10032478 | NOP the `beq` |

And the config block at 0x30003e00 must be sane before cleanup: zero the
~0x100-byte field struct (record base +0x60, TOC pointer +0x78 — read by
the MCUSW search — startup reason +0x7c; bit 31 set would switch the boot
search to UPDAPP) and set `[+0x60] = 0x20004F00` so config-build records
land at 0x30003f00 (dst = field + 0x0FFFF000), behind the struct.

This boots Linux through NLoader — the real boot chain — instead of QEMU's
direct `-kernel` path, validating DTB + low-level assumptions end to end.

### On the phone (walls, in order)

1. **Flash TOC is BB5-write-protected**: adding/repointing a `MCUSW` entry
   in OneNAND 0x140000 needs the flashing path (signed packages) or an
   exploit. The UDA/user partitions are writable from Symbian, but NLoader
   only reads images listed in the boot-area TOC.
2. **Image authentication** (0x100305b8 → secure ROM): custom images fail
   BB5 auth on HW. R&D mode does not bypass image auth (only ROFS checks).
3. **USB peripheral boot** (ROM boot, PID 0x0106): loads a *secondary
   bootloader* over USB at power-on reset — but BB5-signed as well.

So the realistic hardware paths stay: (a) find a BB5 signing/auth bypass
(the crypto stubs are in the secure ROM — QEMU can't see them), (b) the
USB flashing path with Nokia R&D certificates (if any surface), or
(c) accept Symbian-boots-Linux (kexec-style second-stage from a Symbian
exe — currently walled by All+TCB LDD caps, see lore 2026-08-21).

## 7. Key addresses (file offset = address − 0x10020000)

| address | function |
|---------|----------|
| 0x1003259a | main (boot orchestrator) |
| 0x10032558 | pre_genio (SUSD, Build TOC, Copy images, GENIO search) |
| 0x100319fc | Build TOC in SDRAM (skipped in QEMU, pre-populated instead) |
| 0x10031876 | copy_image_to_addr(entry, dst) helper |
| 0x10031fee | Copy images loop (§2) |
| 0x10030a88 | flash page-read dispatcher |
| 0x1002f330 | OneNAND driver page read (code gap in binary, SWBL-loaded) |
| 0x1003277c | TOC search (mode 0/1/2, §2.2) |
| 0x10033084 | ARM-lib memcmp (uqsub8 loop) — the TOC name compare |
| 0x10033ac4 | ARM-lib cache/TLB invalidate + SCTLR read-modify-write
              (mcr p15 c7/c8 storm; the "Disable MMU"/flush helper) |
| 0x10033b54 | secure-ROM jump trampoline — **Thumb-entered** (`bl` from
              0x10030436): halfword 0 is `bx pc`, ARM payload at +4; patch
              `bx r1` at **0x10033b58** in QEMU |
| 0x10033b84 | six more secure trampolines, 8 bytes apart (blx-entered,
              ARM `ldr pc,[pc,#-4]` pairs) — stubbed in QEMU |
| 0x10032450 | cleanup = launcher (§4) |
| 0x100303f4 | find boot image ("MCUSW"/"UPDAPP", mode 2) |
| 0x100305b8 | authenticate loaded image (secure call) |
| 0x10030418 | jump-to-image: entry = load + *(load+0x148) |
| 0x10033b54 | (see above — patch site is +4 / 0x10033b58) |
| 0x10032498 | GENIO orchestrator |
| 0x1002feb2 | config build (hw_conf records) |
| 0x101ff000 | SDRAM TOC (64 entries × 0x20) |
| 0x30003e00 | config/handoff block base (r0 of the jumped image) |
| 0x30003e78 | pointer to SDRAM TOC base |

## 8. Reproducibility

The disassembler snippets in this doc regenerate with capstone in Thumb
mode over `firmware-re/analysis/nloader.bin` (pattern:
`md.disasm(data[off:], 0x10020000+off)` with literal-pool resolution of
`ldr [pc, #imm]`).
