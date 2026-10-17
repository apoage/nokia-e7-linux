# BCM2727 OneNAND — device model, SWBL-role driver blobs, bypass ledger

**Date:** 2026-08-24 (task 104)
**Sources:** static disassembly of `firmware-re/analysis/nloader.bin`
(Thumb, base 0x10020000), the OneNAND dump
`firmware-re/nokia-e7-onenand.img`, runtime validation in the QEMU chain
(XTI log + QMP memory dumps).
**Confidence legend:** CONFIRMED = decoded from binaries and seen
executing in the chain; CONTRACT = our model's pinned choice where the
dump can't decide; OPEN = unresolved.

---

## 0. Executive summary

The QEMU chain no longer preloads the SDRAM TOC or NOPs NLoader's flash
calls.  A register-level OneNAND model (`hw/arm/bcm2727.c`, base
0x20010000, backed by `-drive id=onenand`) plus two SWBL-role guest
blobs (`emulation/qemu-e7sw/`) make NLoader read the **real flash TOC
and real flash images through the real BML stack**: the chain now runs
NLoader's genuine boot flow (Root-TOC/SWBL/SUSD searches, Build TOC,
Copy images, full GENIO config build) for the first time in emulation.
The OMAP3630 scaffolding is gone — the board file drives a BCM2727 SoC
skeleton.

**Task 122 (2026-09-05): the entry is now genuine.**  The mirror +
primary double-load is retired for a single SWBL-shape copy (file
0-0x1C000 @ 0x10010000); after the shim, an args-restore stub enters
NLoader's real BB5 ARM entry 0x100103D0, and the phone's own
prologue, scatter-relocation loader (staging the genuine command
words, 33-record device-ID table, and USB descriptors), init tree
(including the EM-ASIC detect over the modeled CBUS bridge —
"Pearl-J : v1.0"), and record curation/allocator all run before
main.  The chain still boots to Linux `e7:~#` under TCG and icount.
Full shape + validation: §5.3.1; the rewritten patch ledger with the
0x10000 VA-convention note: §4.

**Open gate (RESOLVED 2026-08-25, task 106):** cleanup's MCUSW search
reported "SOS image not found" because NLoader's copy-images prep
REBUILDS the SDRAM TOC from its own curated record table and, with the
record curation starved in emulation (the shadow image-list at
0x10024178 is never staged and the dump's kernel containers are
auth-walled), stamped its 0xFF end marker ON SLOT 0 — blanking the
shim's synthetic MCUSW entry.  The writer was caught with a logging
overlay (E7SW_TOCWATCH), decoded in full, and the marker store NOPed as
a §4 ledger entry (emulation artifact: on hardware the curation finds
the root TOC).  The chain now boots NLoader's genuine flow end to end
through to Linux `e7:~#` (§5.2 item 2).  Task 104 closed
DONE+VALIDATED on 2026-08-26 (task 107) — final chain shape in §5.3,
baseline log `emulation/qemu-e7sw/boot-baseline-task104.log`.

---

## 1. The NLoader flash stack (decoded)

### 1.1 Layering

```
NLoader boot flow (main 0x1003259a)
  └─ BML_Init 0x100288e0 ─ BML_Open 0x10028c66 ─ bml_read 0x10029d9c
       └─ op table @ 0x10007dd8 (22 entries, installer 0x1002f1a4)
            └─ driver-gap code, mirror offsets 0x1E000-0x1F000
                 = SWBL's payload on hardware; ERASED in our dump
```

The installer (0x1002f1a4) writes literal-pool addresses into
`0x10007dd8` — every slot points into the gap.  Slot→address map
(decoded straight from the installer; the mirror-offset notation is
historical — the real file range is 0xE000-0xF000 under SWBL's base
0x10010000):

| slot | address (Thumb) | role (decoded) |
|------|-----------------|----------------|
| +0x00 | 0x1001E5F3 | init — **return 0** or BML_Init fails 0x80090000 |
| +0x04 | 0x1001E6DB | partition select (r0 = index*4), return 0 |
| +0x08 | 0x1001E903 | release/close (frees the op contexts) |
| +0x0C | 0x1001E97D | **page read** (devinfo+0x68): r0 pbn, r1 LSN, r2 nsec, r3 dst, [sp]=spare, [sp+4]=flag; FSR-style return |
| +0x10 | 0x1001E533 | **GetDevInfo** (idx, &info 0x2C bytes) — validated by 0x100283e4 |
| +0x14 | 0x1001EAB1 | block-class query (BMI scan), FSR return |
| +0x34 | 0x1001E9BD | spare-field variant (BMI deep check) |
| +0x40 | 0x1001E9FF | read-with-spare — the BMI path actually calls a **mirror** at devinfo+0xC0 |
| others | 0x1001E52F … 0x1001EF75 | unused on the boot-read path |

**[Task 115 correction]** the gap payload is NOT erased in our dump:
0x1001E000-0x1001F000 = SWBL's load base 0x10010000 + **nloader.bin
file 0xE000-0xF000**, which carries the genuine FSR-style LLD (code
at every slot target above, OneNAND register constants 0x1E440/
0x1E480 in its pools).  The zero region is file 0x1E000-0x1F000 —
beyond SWBL's 0x1C000 copy cap and never mapped to the slot
addresses.  Full story in §7.5.

**[Task 118 — the LLD decoded and running, trampolines retired]**
The genuine driver spans **file 0xB000-0x10000** (position-linked for
0x1001B000-0x10020000): the 22 installer slots live in the top 4 KB
(file 0xE000-0xF000); the shared layers below are the class-driver
op tables (0x1001bf80/0x1001c7da/0x1001bee2…), the register-level
core read 0x1001bb90, the geometry decoder 0x1001de40, the XTI trace
logger 0x1001dde8/0x1001f26e, and the registry/config init 0x1001f124
(byte-twin of the main body's 0x1002f124).  Structure:

- **Per-unit context** at 0x10007ec8, 8 × 0x34 bytes: ctx[0] = the
  device base handle (BML_Init's param cell 0x10008188 is built by
  0x1002f124 with handle[0] = 0 — **the phone's boot flash at PA 0**,
  matching §7.3's cs<<26), ctx+4 open flag, ctx+0x30 → matched
  device-table record.
- **Open** (slot+0x04) clears SA2 (+0x1E202), reads the three ID
  halfwords at **+0x1E000/+0x1E002/+0x1E004** (the mainline ID
  registers — `0xF<<13`, NOT the SWBL driver's +0x7800 window),
  matches the **device-ID table at 0x100071fc** (0x14-stride records
  {mfr, dev, rev&0x700, u16, u16≤0x2FA, bytes…}, mfr==0 terminator),
  installs the class op table at 0x10008068 (record +0xE == 4 → the
  OneNAND family), mallocs the op contexts via the wrapper family
  0x1001b1b2-0x1001b1d0, then runs the geometry decoder 0x1001de40:
  with the model's post-reset SYS_CFG2 = 0 the "ip=0" path takes the
  record's geometry (64 pages/block, 4 sectors/page) and fills
  ctx+0x1A/+0x24/+0x26 and the shift cells (ctx+0x28-0x2F).
- **Read** (slot+0x0C) validates (0x1001e16c: unit < 8, open, LSN ≤
  record+6) then dispatches to the class op table's read (0x1001bee2)
  → the core read 0x1001bb90: SA1 (+0x1E200, block = LSN>>8),
  SA2 (+0x1E202, die bit), SA8 (+0x1E20E, LSN&0xFF), StartBuffer
  (+0x1E400), interrupt clear (+0x1E482 ← 0, a no-op W1C on real
  silicon), command (+0x1E440 ← one of the five command words at
  **0x100071e4**: READ 0x00 / READOOB 0x13 selected per dst/spare),
  poll of +0x1E482 until **0x8080** (MASTER|READ), data from
  +0x400+sector*512 (the two DataRAMs contiguous — matches the
  model), spare from +0x10020+sector*16, error status from
  **+0x1FE00** (& 0xAAAA/0xCCCC must read 0).  dst==0 with spare==0
  is fast-failed by the wrapper itself (`orrs` check) — the task-106
  blob-side guard is inherent here.
- **Return conventions** (§1.1's earlier reading corrected): the real
  LLD returns **0 on success**; 0x80040000 = param/timeout;
  0x8400xxxx = flash-status error bits.  bml_read's check at
  0x10029e9a accepts ret>>16 ∈ {0, 0x1000, 0x1100, 0x1200, 0x1300}
  plus a special 0x8400 path — "ret>>16 == 0x1100" was one of several
  success codes, not the only one (a task-104 decode imprecision the
  trampolines' 0x11000000 returns happened to satisfy).
- **Static cells the image does not carry** (the double-load maps
  code bytes there): the command words at 0x100071e4 and the
  device-ID table at 0x100071fc — the board stages both (§4).  The
  ctx cells +8/+0xC/+0x10 the driver fills itself.
  **[Task 121 correction]** the image DOES carry both — LZ-compressed
  in the scatter blob at file 0x14EA8-0x1503A: §7.10's record 2
  decompresses them to 0x10007000-0x100074DC on hardware.  It is the
  double-load (which never runs the decompressor) that leaves them
  unstaged, so the board staging stands in.  Two byte-level deltas:
  the command words are twelve u16 (the staged five are the
  read-path subset), and the genuine device-ID table has 24 records
  with NO rev-class-0x0100 entry — the model's VerID 0x0121 would
  fail the open's strict matcher (§7.10b); the staged record exists
  to match it.  The ctx cells remain runtime-filled (record 3's
  zero-filled .bss).

The chain's single map (task 122) lands the whole driver at its
linked VAs natively — file 0-0x1C000 at 0x10010000 puts file
0xB000-0x10000 at 0x1001B000-0x10020000; no repoints, no staging.

### 1.2 Geometry contract (identity LSN mapping)

The BML device-info (unit 0 singleton @ **0x10007c30**, 0xC4 bytes)
decomposes an LSN in `bml_read` using cells the LLD fills:

| cell | value | effect |
|------|-------|--------|
| type (+0x10) | 1 | OneNAND class |
| pages-per-block u16 (+0x22) | 64 | N |
| sectors-per-page u8 (+0x26) | 4 | |
| shift C (+0x27) / shift A (+0x32) | 6 | pbn = LSN>>6 |
| shift B (+0x33) | 1 | |
| blocks u16 (+0x52, +0x1c) | 8192 | 1 GiB |

With these, `pbn = LSN>>6`, page-offset = LSN&0x3F, and the read slot's
**r1 IS the linear sector number**: flash byte offset = r1 × 512.
CONFIRMED in the chain: NLoader's "Read TOC : 00020000" issued
`lld_read(lsn=0x100, nsec=4)` = device offset 0x20000×… (LSN 0x100 =
byte 0x20000? no — byte 0x100×512 = 0x20000 ✓).

The GetDevInfo validator (0x100283e4) hard-rejects anything but
64/128 pages-per-block, 4/8 sectors-per-page, 16-bit bus — real Samsung
geometry, no latitude.

### 1.3 The BMI wall (why BML_Open fails on our dump)

BML_Open walks the flash for factory BML pool metadata: blocks whose
first page starts `ULOCKPCH…`/`LOCKPCHD…` (markers at nloader.bin
0x10029A18/0x10029A24), spare bytes == 0xFE, main[0..511] mirrored at
+0x200.  Our dump is main-area only (no spare), and the markers exist
only in device blocks 0-1 (file 0x14A6E8 etc.) while the scan starts
past them — the scan can only fail (`0x801A0000`).  Two runtime
patches carry the chain past it (§4).  This is a DUMP limitation, not a
decode gap.

### 1.4 The flash TOC vs the SDRAM TOC (SWBL's role)

- Flash TOC at device offset 0: **32-byte records
  `{off@+0, size@+4, rsv, rsv, load@+0x10, name[12]@+0x14, rsv}` from
  device 0, no 20-byte header, name[0]==0xFF terminator, 32 entries**
  — CONFIRMED from SWBL's own reader (§7.4, task 115).  The layout
  this doc carried before task 115 ("20-byte header +
  [name:12][off:4][size:4][rsv:12]") paired every name with the
  following record's {off,size}; names sit at the same bytes either
  way, but the pairing — and therefore which image each name owns —
  shifts by one slot (e.g. the loader this doc calls SWBL is
  TOC-named 'ADA'; 'RAP3NAND' = nloader.bin at 0x4000).
- **NLoader's own flash-TOC reader (0x10031834) is an empty `bx lr`
  stub** — on hardware SWBL stages the RAW flash-TOC page at
  **0x1000C000** and publishes the pointer at **[0x30003E78]**; the
  curation and the 0x101ff000 rebuild are NLoader's Build-TOC work
  over that staged raw TOC (fully decoded from the flash copy, §7.2/
  §7.4 — the earlier "SWBL populates the SDRAM TOC at 0x101ff000"
  reading was the shim's compressed shape).  Our shim blob plays
  SWBL (§3).
- The Root-TOC search expects a *second*, 16-byte-record TOC (shape
  `{0, size, class, name[8]}` — "MCUSW"-keyed) at BML logical offset
  0x20000; it is NOT in our dump at that offset (scan found one
  candidate run at device 0x1C0 of unclear extent).  OPEN — and per
  §7.5, not SWBL's business either way: SWBL reads no second TOC;
  this stays a NLoader-side/hardware item.

---

## 2. OneNAND device model (`hw/arm/bcm2727.c`)

Standard Samsung OneNAND register protocol, 16-bit bus:

- Base **0x20010000**, window 0x20000.  Base is a CONTRACT, not a
  decode: the kernel early-map pre-maps PA 0x20010000 at VA 0xC6012000
  with device attributes (icu-timer doc §4) and no decoded block claims
  it; the silicon base is OPEN (phone-probe item).
- Registers at `(reg_addr << 1)`: SA1 0x1E200 (FBA=block), SA8 0x1E20E
  (FPA<<2|FSA), StartBuffer 0x1E400 (BSA<<8 = DataRAM select), Command
  0x1E440, Interrupt 0x1E482 (W1C), IDs 0x1E000+.  DataRAM0 @ +0x400,
  BootRAM @ +0.
- Commands: 0x00 READ, 0x13 READOOB, 0x90 READID (IDs: EC/0068/0121 =
  Samsung 8-Gbit), 0xF0 RESET, PROG/ERASE/LOCK accepted at protocol
  level, no writeback.
- Geometry: 1 GiB, 128-KiB blocks = 64 pages × 2 KiB (+64 B spare).
- Backing: `-drive if=none,id=onenand` raw file, **device offset X =
  file offset X + 0x140000** (the dump carries the rofs container
  header; TOC at device 0 = file 0x140000).  Past-EOF reads = 0xFF.
- **Spare contract: all spare bytes read 0xFE** ("BML-clean") — the
  dump has no spare area; 0xFF made every BMI check fail instantly.
- **Task 118 additions** (for NLoader's genuine LLD, decoded §1.1):
  - **CS0 alias at PA 0**: `bcm2727.onenand-cs0`, an alias of the
    whole 0x20000 window at physical 0 — the LLD's base handle is 0
    (BML_Init's param cell 0x10008188), so the driver drives the
    boot flash at PA 0 exactly as on hardware (§7.3's cs<<26).  The
    0x20010000 mapping stays as the kernel-era view.
  - **+0x1E446** (SYS_CFG3-ish MuxOneNAND config): reads 0x0000 —
    the single-die straight-through shape.
  - **+0x1FE00-0x1FFFF**: error-status window, reads 0x0000 (no
    error bits; the LLD checks & 0xAAAA/& 0xCCCC after flagged
    reads).
  - **+0x10020-0x100A0**: per-sector spare readback (8 × 16 B),
    reads 0xFEFE — same BML-clean contract as READOOB (read by the
    read-with-spare copy helper 0x1001e12a).
  - **+0x7800-0x7806**: Mfr/Dev/Rev window — the **SWBL driver's**
    ID read (§7.3), kept for real-SWBL runs; NLoader's LLD reads the
    mainline +0x1E000 registers instead.
  - With post-reset SYS_CFG1/SYS_CFG2 = 0x40E0/0x0000 the LLD's
    geometry decoder takes its "ip=0" (record-driven) path; the
    SYS_CFG2 bits 12/14 + 15:13 classes that the "ip=1" path would
    decode are not exercised.
- **Task 122 additions** (for the genuine BB5 entry, §5.3.1):
  - **VerID 0x0000** (0x0121 before) at all three ID sites — READID
    BootRAM, the +0x7800 SWBL window, the +0x1E000 mainline
    registers; Mfr/Dev stay 0x00EC/0x0068.  The genuine 33-record
    device-ID table (scatter record 2) matches rev&0x700
    first-wins: class 0x0000 selects the 8192-block EC/0x68 record;
    the old 0x0121 (class 0x0100) would silently select 4096-block
    geometry (§7.10(b)).
  - **CBUS bridge @ 0x20030700** (`bcm2727.cbus`, 0x100-byte window):
    the serial-control engine both bootloaders drive.  [+0x04]/[+0x08]
    read 0 (idle/complete — the waiter at file 0x1100 reads 0 as
    instant-done); a write to [+0x18] (read command: bus = bits[7:6],
    ser = bits[4:0]) latches the register file into [+0x20]; a write
    to [+0x1C] (write command) stores [+0x20] into the file.  4 × 32
    u16 registers, RAM semantics.  EM identity CONTRACT: bus1.reg0 =
    0x1050 (type 0x10 = "Pearl-J", version 5, rev 0) — NLoader's
    init-tree probe hangs deliberately without a valid type
    ("No EM ASIC", file 0x726).  `CBUS_DEBUG=1` traces every access.
- Debug: `ONENAND_DEBUG=1` prints each command and each ID-register
  read.

## 3. The SWBL-role blobs (`emulation/qemu-e7sw/`)

Built by `build.sh` with the task-103 musl.cc ARMv6 cross; embedded
into QEMU as `hw/arm/e7sw_blobs.h` (regenerate + rebuild QEMU after
changing them).

- **lld.bin is RETIRED (task 118)**: the task-104 vtable trampolines
  and their C body existed only because the double-load never mapped
  file 0xB000-0x10000 at the driver's linked base — the board now
  maps that range and the image's own FSR LLD serves all 22 installer
  slots (§1.1).  vt.S, lld.c and lld.ld are deleted.
- **shim.bin** @ 0x40200000 (SRAM bottom), entered first (PC=shim;
  task 122: LR = the board's args-restore stub 0x40202100, which
  zeroes r0-r3 and jumps to NLoader's genuine BB5 ARM entry
  0x100103D0 — SWBL's own launch-trampoline shape, §5.3.1): resets
  the OneNAND, reads IDs, reads the real flash TOC pages, curates
  them into the SDRAM TOC at 0x101ff000 (excludes the BB5-encrypted
  SOS+IVE3* kernel variants and offsets ≥1 MiB), writes the
  synthetic chain-mode MCUSW slot 0, publishes the pointer at
  0x30003e78.  Config block from the board at 0x40200C00 (`'E7WS'`
  magic).  The BML geometry pre-fill is retired (task 118: the real
  GetDevInfo owns those cells and BML_Init's memset had made the
  pre-fill dead weight anyway); the interrupt status is left set
  (not W1C-cleared) after the shim's flash ops — the genuine LLD's
  op-state machine polls MASTER before its first command and a
  cleared status would time it out.
- Logging: the shim runs pre-main and must NOT call NLoader's XTI
  printf (formatter globals are uninitialized code bytes then).  It
  writes bytes to the QEMU **shim-log port 0x40208000**
  ("swbl: …" lines).  `E7SW_DEBUG=1` enables detail; off by default.

## 4. Runtime-patch ledger (chain mode, nokia_e7.c)

> **VA-convention note (task 122, 2026-09-05 — read before using any
> address in this doc).** Before task 122 the docs cited NLoader code
> VAs under the **old 0x10020000 display convention** (old VA =
> 0x10020000 + file offset).  Task 122 replaced the mirror+primary
> double-load with the single SWBL-shape map — file 0-0x1C000 at
> **0x10010000** — so every linked VA is now real at **real VA = old
> VA − 0x10000**.  Historical sections below keep the old convention;
> this ledger cites both.  Addresses below 0x10010000 (the
> 0x10007xxx globals block, 0x3000xxxx handoff cells) are absolute and
> unchanged.

**Task-104 REMOVALS (stay removed — the real paths run):** the
flash-init/call NOPs (old 0x100325f4/0x100325fc/0x10030bb0/0x10030bb4),
the page-read stub (old 0x1002f330), the Build-TOC skip (old
0x100319fc), and the host-side SDRAM-TOC preload.

**Task-093-era patches that REMAIN** (genuine NLoader walls; rebased
−0x10000 in task 122 — every patch is file-relative so the instruction
encodings are unchanged):

| wall | old VA (convention) | real VA |
|------|--------------------|---------|
| BSI check hang NOP (0x748) | 0x10020748 | 0x10010748 |
| HW/SW version-mismatch skip (0x167a→0x1626, 0xe7d4) | 0x1002167a | 0x1001167a |
| "No valid start-up reason" NOP (0x125f2 — the print at 0x100225e4 fires on the genuine reason 0, the NOP keeps the flow alive past it) | 0x100325f2 | 0x100225f2 |
| delay-loop NOP (0x2bfe) | 0x10022bfe | 0x10012bfe |
| SOS+DECOMP decompressor-hang skip (0xf4fe→0xf4f2) | 0x1002f4fe | 0x1001f4fe |
| MMU-bypass triplet (0x172e/0x1732/0x1738) | 0x1002172e/32/38 | 0x1001172e/32/38 |
| crypto stubs mov r0,#0; bx lr ×6 (0x13b84+8k) | 0x10033b84 | 0x10023b84 |
| secure-ROM net (bx-lr RAM @ 0x30000000 — also hosts the prologue's SYS stack sp=0x30003E00) | — | unchanged |
| config-block zeroing + handoff cells @ 0x30003e00 | — | unchanged (see task-122 bank-cells row) |
| handoff set: auth/PMM/LDSP/GENIO bypasses + launch (0x1246a/0x11840/0xfcd6/0x1071a/0x13b58/0x12478) | 0x1003246a etc. | 0x1002246a/0x10021840/0x1001fcd6/0x1002071a/0x10023b58/0x10022478 |

**Task-104 ADDITIONS that REMAIN** (rebased; documented walls):

| wall | old VA | real VA |
|------|--------|---------|
| BML_Open BMI failure beq→b (0xe004) — dump has no spare area (§1.3) | 0x1002f310 | 0x1001f310 |
| BML-close NOP on the error path | 0x10028cf0 | 0x10018cf0 |
| SOS+DECOMP auth gate beq→b (0xe003) | 0x10031a66 | 0x10021a66 |
| class-selection spin NOPs (0x1778/0x177ae/0x177d0) + reason-bit NOP (0x1784) | 0x100317xx | 0x100117xx — **currently dead code**: with the single map the class searches HIT via the in-image shadow list (§5.3 task-122 note), so the miss path no longer runs; kept as belt-and-braces |
| SDRAM-TOC end-marker memset NOP | 0x10031f78 | 0x10021f78 — still load-bearing: the genuine re-emission now writes 4 records (slots 0-3) and its end marker would land on slot 4, blanking the shim's surviving flash entries (slots 4-9) that the copy-images walk consumes |

**Task-106 blob-side (shim.c) — unchanged** (the shim blob survives as
the SWBL-role stand-in): synthetic MCUSW slot 0 flash offset 0xFFFF0000
(deliberately out of range — the copy fails non-fatally, and with the
task-122 shape the record's size field reads 0 so no copy is even
attempted); flash-entry curation (name[3] '*'/'/' skip, SOS+IVE3*
skip, ≥1 MiB skip).

**Task-112:** legacy I2C override — stays REMOVED (real engines at
0x202022xx/0x200322xx).  ~~probe-done flag zeroing (0x100076F9)~~ —
**RETIRED (task 122)**: scatter record 3 zero-fills 0x100074DC-
0x10008E50, covering the flag.

**Task-118 rows — ALL RETIRED (task 122; the single map provides the
mechanism each row stood in for):**

| retired row | what provided it now |
|-------------|----------------------|
| ~~LLD map: file 0xB000-0x10000 @ 0x1001B000~~ | the single map lands that range at its linked VAs natively |
| ~~helper repoints (0x1001b1b4/b1c0/b1d8 → 0x100328xx)~~ | the linked VAs 0x10022860/0x100228c6/0x100232b0 now hold their true bytes |
| ~~even-dst-copy repoint (0x1001e0e0 → 0x10000680)~~ | linked VA 0x10010680 = file 0x680, now true bytes |
| ~~staged command words (5 × u16 @ 0x100071E4)~~ | scatter record 2 LZ-decompresses the genuine twelve words (byte-verified: {0000,0013,0080,001A,0094,0095,000E,000C,007D,007F,007F,0003}) |
| ~~staged device-ID record @ 0x100071FC~~ | record 2's genuine 33-record table (byte-verified, mfr-0 terminator at record 33; EC/0x68/0x0000 = 8192-block at record 4) |
| ~~HAL stub block (0x10000100/0x10000200 stubs + 0x10007040/0x10007000 staging)~~ | record 2 stages the genuine service table — the HAL thunk at file 0xDAC (real VA 0x10010DAC) publishes [0x10007040]=0x10007000 and calls service[0] (the EM probe) / service[11] through the genuine pointers |
| search-dst park trampoline + call retarget | **MOVED** (still load-bearing): old home 0x10015000 = file 0x5000 = live prologue-region code under the single map → new home **0x10026000** (file 0x16000, inside the confirmed all-zero pad 0x15200-0x1BFFF; SRAM was unreachable — a 4-byte Thumb BL retarget spans ±16 MiB and the suggested 0x40201000 is also the shim's pagebuf).  Call site 0x10030ae4 → real 0x10020ae4 (bl 0x10026000, encoding recomputed + capstone-verified). |

**Task-122 ADDITIONS (the faithful BB5-entry chain):**

| where | change | why |
|-------|--------|-----|
| board: image map | **single map** — file 0-0x1C000 @ 0x10010000 (mirror 0x10000000, primary 0x10020000, LLD sub-map all deleted) | §7.10: the SWBL single-copy layout IS the final layout |
| board: entry | LR = board-written **args stub @ 0x40202100** (ARM: mov r0-r3,#0; ldr pc,[pc,#-4]; .word 0x100103D0); PC stays at the shim, sp stays 0x4020FFF0 | the shim is C and clobbers r0-r3; the stub restores zeros exactly as SWBL's own launch trampoline restores the ROM args (§7.2 img+0x904/0x1C54).  The prologue saves them at 0x30003E00-0x0C, sets SVC sp=0x1002F4D0 (mode-stack pool) and SYS sp=0x30003E00, then blx's into the scatter loop |
| board: bank cells | [0x30003e60]/[+4] = 0x10000000/0 (SWBL shape, 256 MB single bank); the 0x20004F00 record-base hack RETIRED | the genuine image-record validator/allocator (0x1002149a, called from the record walk at 0x10021ea0) checks loads against the region map derived from these cells.  The hw_conf records now land at [E60]+0x0FFFF000 = 0x1FFFF000 — unmapped in our model, dropped; nothing in the chain consumes them (kernel boots from the DTB) |
| board: CHAIN_TRAMPOLINE_BASE | 0x80010000 → **0x101FFC30** (zImage stays in SDRAM at 0x80010000; the 0x150-byte trampoline's r2/r3 words point at DTB/zImage) | the validator requires (load+entry)&(size−1)==0 with the MCUSW record's entry 0x3D0/size 0x100000, plus region containment; 0x101FFC30 is the window NLoader's own allocator produces (heap end 0x101FEFFF bumped to 0x10200000, minus 0x3D0).  Preloading the slot's load with it passes validation with no reallocation; the copy is skipped (record size 0) so the trampoline survives to the launch |
| model: **CBUS bridge @ 0x20030700** | new register-level model (bcm2727.c, `bcm2727.cbus`): [+0x04]/[+0x08] read 0 (idle/done), [+0x18] read-command latches data from a 4×32 u16 register file (bus = cmd bits[7:6], ser = bits[4:0]), [+0x1C] write-command consumes [+0x20]; env-gated `CBUS_DEBUG` trace | the init tree's EM-ASIC probe (service[0], file 0x6AA) reads the EM ID over this bridge and **hangs deliberately** (b . at file 0x726, "No EM ASIC") without it.  EM identity CONTRACT: bus1.reg0 = 0x1050 → type 0x10/version 5/rev 0 → "Pearl-J : v1.0" (the phone's EM codename — NLoader string table file 0xABC-0xAE0, ROFS-core strings, hardware-lore §10 "PEARL N2300" PMIC family) |
| model: OneNAND VerID | 0x0121 → **0x0000** at all three ID sites (READID BootRAM, +0x7800 window, +0x1E000 registers; Mfr/Dev stay 0x00EC/0x0068) | the genuine 33-record table's matcher is strict three-way first-match (§7.10(b)): rev-class 0x0000 first-matches the **8192-block** EC/0x68 record (record 4); class 0x0100 (old VerID 0x0121) would silently select 4096-block geometry |

QEMU-side instrumentation kept (env-gated, default off): the
E7SW_TOCWATCH=1 logging overlay on the SDRAM-TOC head at 0x101ff000 —
unaffected by task 122 (the new trampoline at 0x101FFC30 sits outside
its 0x20-byte window); CBUS_DEBUG (task 122) joins E7SW_DEBUG and
ONENAND_DEBUG.

## 5. Validation state and the open gate

**Task 118 re-validation (2026-09-02, genuine LLD):** the chain boots
to `e7:~#` under plain TCG and `-icount` (docs/boot-118-icount
-ttyama0.txt) with the image's own FSR LLD serving every flash call
(evidence log
`emulation/qemu-e7sw/boot-lld-task118.log`, E7SW_DEBUG=1
ONENAND_DEBUG=1: ID-register reads at +0x1E000 where the driver reads
them, the BMI block-class probes over blocks 8186-8191, the three
search reads at LSN 0x100/0x200/0x500 — the driver's own register
sequence, not the trampoline shape; interactive `/proc/interrupts`
tick/uart/mmc counts with Err: 0, `/dev/root` ext4 rw).  The NLoader
print flow is byte-identical to the task-104 baseline except the
zImage size line (newer kernel build).  The task-118 board-side
bring-up artifacts are the §4 ledger rows (helper repoints, staged
cells, park trampoline).  *Independently reproduced by the main
session 2026-09-04 (plain TCG, eMMC root, no debug envs): `e7:~#` +
ROOT-UP banner; the 359-line NLoader flow diffs the task-104 baseline
in exactly the Image-size line.*

**Validated in the chain (2026-08-25, task 106 — end to end; re-validated
2026-08-26 task 107, see §5.3):** shim
reads real flash TOC (11 entries; IDs EC:0068:0121); NLoader prints its
real boot flow — "Read TOC : 00020000 / 00040000 / 000A0000" (Root-TOC,
SWBL, SUSD searches through BML→LLD→model), "Startup mode : Normal",
real "Build TOC in SDRAM" with the per-slot walk ("Copy & Authenticate
SOS+DECOMP … PASS" from the real flash, "IVE3 binary not found"),
record validation, "Copy images to SDRAM" over the full shim-authored
table (10 entries: MCUSW + 9 flash, each "copying failed"
non-fatally by design), PMM-
not-found clean exit, "Disable MMU", "Start SOS at 80010000", the
complete GENIO configuration build (hw_conf records at 0x300040xx,
16-entry fop/OPP table), Linux, ROOT-UP banner, `e7:~#`.  The §5.1
slot-0 blanking is fixed (§5.2 item 2).

**Resolved (was the open gate):** cleanup (0x10032450) said "SOS image
not found" — the chain did not jump to Linux.  Root cause fully decoded
and fixed in task 106 (§5.1/§5.2 item 2).

### 5.1 Checkpoint bisect result (2026-08-24 follow-up — premise corrected)

QMP `pmemsave` 0x101ff000/0x200 at the three checkpoints (right after
"Build TOC in SDRAM", after "Copy images to SDRAM", at the hang): **all
three dumps are byte-identical** (md5 0315986b026af668b330f5df2310a902).
The table is NOT cleared — the earlier "empty by Copy-images time" read
too much into the zero per-image lines:

- slot 0 (the shim's synthetic MCUSW entry) = **0xFF×32** — overwritten
  with exactly the end-marker pattern;
- slots 1-10 = the shim's flash TOC entries, INTACT;
- slot 11 = the shim's 0xFF end marker, INTACT.

So the only mutation is slot 0, and the write-logger places it AFTER
the "IVE3 binary not found" print and before the copy-images table
header — i.e. inside Build TOC's tail, long after the "Build TOC in
SDRAM" print (the three checkpoint dumps of 2026-08-24 were all
end-state snapshots; the "already blank at the Build-TOC print" reading
was an artifact of snapshot timing).  Both consumers start at slot 0
and terminate on name[0]==0xFF, so the blanked slot 0 explains every
symptom at once:

- copy-images walk 0x10031fee: `r4 = [0x1003222c] = 0x101ff000`, and
  the first thing after the print is `ldrb [r4+0x14]; cmp #0xff; beq
  done` ⇒ ZERO per-image lines;
- cleanup's name search = 0x1003277c (called at 0x10032568): walks
  32-byte slots from `[0x30003e78]` (the published TOC pointer — the
  shim's and the orchestrator's publish cell), same 0xFF terminator,
  also starting at slot 0 ⇒ MCUSW miss ⇒ "SOS image not found".

Verified NOT the writer: every flash read in the E7SW_DEBUG trace.
The search-read helper 0x10030ad2 (prints "Read TOC : %08X") reads
4 sectors into the literal 0x101ff000 via the chunked reader
0x10030a88 → bml wrapper 0x1002f330, and the LLD dst-guard parks all
three searches at 0x10201000; the full log contains exactly three
dsts in the 0x1xxxxxxx range (the parked searches) plus the shim's
SRAM pagebuf, and the only "0x101ff" line is the shim's own info
print.  The writer does direct CPU stores.

Decode added along the way: "TOC entry added" 0x10030bdc appends
40-byte records to a separate count-indexed table at 0x100081e8 (count
byte at 0x100074c4) — it never writes the SDRAM TOC; the SDRAM-TOC
name walker 0x1003140a (r4 = literal 0x1003152c) feeds it; build TOC
0x100319fc prints FIRST, then calls the empty stub 0x10031834, then
0x1003174c, then reads `[0x101ff000+0x14]`.  Literal scan for
0x101ff000 across the binary: 0x10030ec4 (search reader), 0x1003152c
(walker), 0x10031d20 (build TOC), 0x1003222c (copy walk) — no other
references.  The gdbstub is unusable with the host gdb (17.1 has no
ARM support: "Undefined item: arm" / truncated g-packet).
[Resolved 2026-08-26: the single-arch distro gdb was the whole problem —
`apt install gdb-multiarch` fixed it.  Verified live against the chain
(`-S -s`, `target remote :1234`): hbreak at NLoader main 0x1003259a
hits after the shim runs, the SDRAM TOC reads back correctly at the
hit, ARM+Thumb disassembly both decode.  QMP pmemsave + E7SW_TOCWATCH
remain the non-interactive tools of choice; gdb is now available for
single-stepping (e.g. the su-console wedge).]

**Window narrowed to the exact writer (2026-08-25, task 106):** the
E7SW_TOCWATCH overlay caught the blanking as 8 word-sized 0xFFFFFFFF
stores from **0x1003337c / 0x10033380** — the double
`stmhs r0!, {r2,r3,ip,lr}` of NLoader's ARM memset (entry 0x10033364
forces fill 0; 0x10033368 takes the fill word in r2; 0x100332b0 is the
byte-arg sibling that expands 0xFF).  The saved LR at [sp] named the
caller: **0x10031f7d → the call site 0x10031f78, the tail of the
copy-images prep inside Build TOC** (`movs r2,#0xff; movs r1,#0x20;
movs r0,r4; blx memset`): after the per-slot walk and the record
validation, NLoader RE-EMITS the SDRAM TOC from its 40-byte record
table (slots at 0x101ff000+i*0x20, strcpy of each record name at
+0x14) and then stamps ONE 0xFF×32 end-marker slot at
0x101ff000 + count*0x20.  With count == 0 the marker lands ON SLOT 0 —
that was the §5.1 blanking.  The full decode of why count is zero on
our chain (shadow image-list, BB5-container gates) is in §5.2 item 2.

### 5.2 Fix menu (ordered)

1. **Blob-side self-heal (cheapest, no new decode needed):** the
   damage is always slot 0 = 0xFF.  Teach `lld_read`/`lld_read_spare`
   to repair the table on any read: if the board cfg block
   (0x40200C00, magic 'E7WS') says chain mode, the TOC pointer is
   published, and slot 0 name[0] == 0xFF — rewrite slot 0 from the cfg
   (mcusw_load/mcusw_size are already fields there; keep the
   deliberately out-of-range flash offset so the copy fails
   non-fatally, task-093 semantics) and stamp a fresh 0xFF end marker
   at slot 1 (hiding the flash entries — exactly the task-093 table
   shape that booted).  At least one flash read is guaranteed between
   the blanking and cleanup (the SOS+DECOMP copy), so the repair
   always lands in time.  Rebuild blobs (build.sh) + QEMU, boot.

   **Outcome (2026-08-25, task 105): implemented as specified — the
   premise is refuted, the heal has no trigger.**  The helper
   (`toc_selfheal()`, inlined by -Os into `lld_read`'s prologue —
   verified in lld.elf) with all four gates and the byte-identical
   slot-0 rewrite is in; blobs + QEMU rebuilt; the board embeds the
   fresh blob.  The boot reproduces the task-104 state exactly
   ("SOS image not found" at cleanup, no `lld: toc slot0 self-heal`
   line).  The load-bearing claim — "the SOS+DECOMP copy guarantees
   at least one LLD read between the blanking and cleanup" — is
   FALSE: the E7SW_DEBUG trace contains exactly THREE lld_read calls
   in the whole boot, the Root-TOC/SWBL/SUSD search reads (lsn
   0x100/0x200/0x500, parked at 0x10201000), all before "Startup
   mode : Normal"; after "Build TOC in SDRAM" there is NO LLD or
   model traffic at all.  "Copy & Authenticate SOS+DECOMP. Addr:
   1e400000 … PASS" copies through a non-BML path (the 0x1e400000
   destination smells like a direct OneNAND-window read, not a
   bml_read dst), and "Copy images to SDRAM" reads nothing because
   slot 0 is already blank.  The heal therefore only ever evaluates
   during the three search reads, where a gate fails: either slot 0
   was not blanked yet (the write lands in the startup-mode pass /
   build-TOC prologue, i.e. AFTER the searches) or 0x30003e78 did not
   yet hold 0x101ff000 when the searches ran.  The log cannot
   discriminate — the item-2 write-logger settles both the write
   moment and the pointer timeline.  The self-heal code stays in
   (harmless; correct if any post-blanking LLD read ever appears),
   and the fix moves to item 2.  Full log: docs/boot-105-selfheal.txt
   (E7SW_DEBUG=1, timeout-killed at the cleanup hang).
2. **Find the writer (the faithful fix):** logging memory-region
   overlay on 0x101ff000-0x101ff020 in bcm2727.c/nokia_e7.c (forward
   to RAM, log value + first_cpu PC on write), one boot → writer PC →
   disassemble → NOP like a §4 ledger entry.  Worth doing even after
   (1) works so the ledger tells the truth.

   **Outcome (2026-08-25, task 106): writer caught, decoded in full,
   chain BOOTS to `e7:~#`.**  The overlay (kept in bcm2727.c, env-gated
   `E7SW_TOCWATCH=1`, off by default; forwards by PDRAM host-pointer
   arithmetic so re-dispatch cannot recurse; logs the guest PC plus
   [sp] — the writer is ARM code whose `push {lr}` parks the caller
   there) caught the blanking as 8 word stores of 0xFFFFFFFF from
   0x1003337c/0x10033380 = the double-STM fill loop of NLoader's ARM
   memset (0x10033368 entry; the byte-arg sibling 0x100332b0 expanded
   the 0xFF).  Saved LR → call site **0x10031f78**, the tail of the
   copy-images prep inside Build TOC: after the per-slot walk and the
   record validation, NLoader **re-emits the SDRAM TOC from its own
   40-byte record table** (toc_entry_add 0x10030bdc) and stamps one
   0xFF×32 end-marker slot at 0x101ff000 + count*0x20.  count == 0 on
   our chain, so the marker landed on slot 0 and blanked the shim's
   synthetic MCUSW entry.  The rebuild itself is genuine, unconditional
   NLoader behavior — on hardware the table NLoader boots from is
   always its own re-emission, never SWBL's original.

   Why count == 0 here (all decoded): the record curation has two
   gates, both starved in emulation.  (a) The class-search walker
   0x1003140a gates every hit on a **shadow image-list at 0x10024178**
   — 0x20-byte records {name@+0x00, key@+0x0c, load@+0x18, class@+0x1c}
   — which on hardware SWBL stages (the RAM-side face of the Root-TOC,
   §1.4's "second TOC"); in our chain that address still holds the
   file's dummy bytes (the "Yama VCore calibration done" string) and
   every lookup misses.  (b) The per-slot collector additionally
   requires the startup-reason top nibble to be exactly 2
   ("Startup mode : Normal" per NLoader's own string table at
   0x100324d6: 1=Update, 2=Normal, 4=Test, 8=Local) AND a BB5 image
   header (magic 0x809795a3 or "ROFP") readable at the slot's flash
   offset — and the mode pass 0x1003174c SCRUBS the nibble
   (`lsls r0,#4; lsrs r0,#4` at 0x1003177e) whenever both class
   searches miss (i.e. always, per (a)).  The nibble is not
   board-configurable either: main re-reads the reason from hardware
   (0x10022d42 — a stubbed HAL call feeds it 0x64 here) and
   overwrites [0x30003e7c] before anything could consume a preloaded
   value; the determiner 0x100318f2 then maps SUSD-not-found to nibble
   2, which the mode pass scrubs right back on the miss path.  On
   hardware the searches hit the shadow list, the scrub is skipped
   (bne at 0x10031760), the builder reads the real kernel container's
   header, and the rebuilt table genuinely contains MCUSW.

   Decision per the task's own branch test — "a search-miss path that
   on hardware would have found the root TOC" → emulation artifact →
   NOP the store site: **0x10031f78 blx memset → 2×NOP** (§4 ledger,
   task-106 row).  The task-105 self-heal stays dead as a mechanism
   (structurally: nothing blob-side executes between the blanking
   inside Build TOC and cleanup — no LLD traffic exists to ride on).
   The genuinely-faithful future path is decoded and parked for
   tasks/107: stage the shadow list (shim) + point the synthetic slot
   at a real magic-bearing header — but NLoader's machinery then takes
   over the launch entirely (the copier rewrites [+0] = load +
   0xefe01000 and copies the container from flash over the load; the
   launcher enters at load + *(u32*)(load+0x148)) — which only works
   with a bootable BB5 container image in flash, i.e. never with the
   stock dump.  Keeping the shim-authored table (SWBL's role) with the
   marker store neutralized is the honest emulation-side cut.

   Two follow-on repairs the now-live table walk forced (both §4
   ledger, blob-side): the synthetic MCUSW slot's flash offset had to
   become deliberately out-of-range (0xFFFF0000 — with offset 0 the
   copier streamed flash-TOC data over the trampoline; the old zero
   offset had been masked by the blanking), and the compressed
   BB5-container entries (name[3] '*'/'/' → the Decompress copy path,
   container magics + real decompressor, secure-side-walled) had to
   leave the shim's curation.  With those, the boot runs the genuine
   flow end to end: Build TOC walk (Copy & Authenticate SOS+DECOMP
   from the real flash) → record validation → copy-images walk over
   the full shim table (10 entries: MCUSW + 9 flash, all "copying failed" non-fatally as
   designed) → "Disable MMU" → "Start SOS at 80010000 (->80010000)"
   (the mode-2 search resolving the load via the copier's [+0]
   rewrite) → Linux → ROOT-UP banner → `e7:~#`.  Logs:
   docs/boot-106-tocwatch.txt (the catch; also boot-106-tocwatch2.txt
   with the [sp] caller line), docs/boot-106-final2.txt
   (E7SW_TOCWATCH=1 E7SW_DEBUG=1 validated boot),
   docs/boot-106-clean.txt (default envs).
3. Moving the MCUSW entry to a later slot does NOT work — both walks
   terminate at slot 0's 0xFF before reaching any later slot (decode
   above).

### 5.3 Final chain shape (2026-08-26, task 107 close-out) + validation

**The decision.** Task 107 had two candidate end states: (a) self-heal
only (writer decoded as genuine, retained) or (b) writer NOP'd + the
shim writing the task-093 minimal shape natively (MCUSW + end marker at
slot 1, flash entries hidden from the start). The landed answer is
**(b)'s writer-NOP half WITHOUT the trim** — i.e. the task-106 state
as-is: **the shim stages the full curated SWBL table and NLoader's
end-marker store stays NOP'd**. Reasons, on the record:

- The heal is structurally dead (§5.2 item 1 outcome) — it cannot be
  the mechanism under any shape, so (a) is off the table factually.
- The trim buys **no robustness**: with count == 0 the re-emission's
  end marker lands ON SLOT 0 in either shape, so the §4 NOP
  (0x10031f78) is load-bearing regardless. What the trim would change
  is only which entries the consumers see.
- The trim is a fidelity **regression**: the staged flash entries are
  what exercise genuine NLoader code today — "Copy & Authenticate
  SOS+DECOMP … PASS" reads the real image through a staged entry's
  flash offset, and the copy-images walk runs over the full 10-entry
  table. It would also make the shim less SWBL-faithful (SWBL stages
  what is in flash). The interactions that once made full-table walks
  dangerous are already fixed pointedly (§4 task-106 rows:
  container-family curation, out-of-range MCUSW offset, dst==0
  fast-fail); hiding the entries would re-solve a solved problem by
  deleting flow.

**Who writes the TOC, in what order (the final shape):**

1. **Shim (SWBL's role), pre-main, from the real flash.** Model RESET +
   READID → reads the real flash TOC pages at device offset 0 → curates
   (drops SOS+IVE3* encrypted variants, compressed-container family
   name[3] '*'/'/', offsets ≥ 1 MiB) → stages the SDRAM TOC at
   0x101ff000: slot 0 = synthetic chain-mode MCUSW (flash offset
   deliberately out of range 0xFFFF0000, size/load from the board cfg),
   slots 1–9 = the real curated entries (SWBL, ADA, R&D, PRIMAPP,
   RAP3NAND, SOS+PMML, SOS+DECOMP, PASUBTOC, PAPUBKEYS), slot 10 =
   0xFF×32 end marker → publishes the pointer at 0x30003e78. This
   mirrors the phone's division of labor: NLoader's own flash-TOC
   reader is an empty stub (§1.4) — SWBL owns staging.
2. **NLoader Build TOC, genuine, over the staged table.** The per-slot
   walk runs real code over all 10 entries (SOS+DECOMP authenticated
   from the real flash); the record collector starves (shadow list
   never staged, mode nibble scrubbed — §5.2 item 2), so the
   re-emission strcpy loop writes nothing (count == 0) and the
   end-marker store is the §4 NOP. Net: the staged table survives to
   the consumers unchanged.
3. **Consumers walk the staged table.** Copy-images over all 10 entries
   (MCUSW copy fails non-fatally on the out-of-range offset with the
   copier still rewriting [+0] = load + 0xefe01000; flash entries fail
   non-fatally on dst==0), then cleanup's MCUSW search hits slot 0 and
   the mode-2 launch resolves the trampoline load → "Start SOS at
   80010000" → Linux.

The genuinely-faithful future path (stage the shadow list + point the
synthetic slot at a real magic-bearing header so NLoader's own record
machinery collects and launches) stays decoded and parked — it needs a
bootable BB5 container in flash, impossible with the stock dump
(§5.2 item 2). The phone-side root-TOC read (§1.4) stays a hardware
item. **No blob or QEMU changes were made in task 107** — the close-out
is this decision record, the validation below, and the bookkeeping.

**Validation (2026-08-26, task 107):**

1. *Primary (plain TCG, ttyAMA0, root=mmcblk0, SD drive attached):*
   full genuine NLoader flow → Linux → rcS banner "ROOT-UP: buildroot
   armv6/musl on the eMMC ext4" → `e7:~#`, then interactively at the
   prompt: `mount` shows `/dev/root` ext4 rw; `/proc/interrupts` shows
   bcm2727-icu counts (tick 0xF2 ≈ 7.7k, uart 0x1a3, mmc 35/36, Err: 0);
   `/dev/mmcblk1` present. Baseline log:
   `emulation/qemu-e7sw/boot-baseline-task104.log` (committed).
2. *icount control (same command + `-icount shift=auto`):* also reaches
   `e7:~#` (docs/boot-107-icount-ttyama0.txt) — icount is healthy on
   the real-flash chain.
3. *Soft-UART variant (`-icount shift=auto` +
   `earlycon=bcm2727su console=ttySU0 loglevel=5 root=/dev/mmcblk0
   rootwait rw`):* the NLoader phase completes (Build TOC → Copy
   images → Start SOS) and the tap decodes the kernel early log
   **byte-exact** — 511 frames, 0 framing errors, every decoded line
   verbatim in the ttyAMA0 boot's printk text (docs/boot-107-su-icount
   .txt) — the same 511-frame extent task 100 validated (softuart doc
   §3 + its task-107 note). The su-console-only boot then goes silent
   after the earlycon-announce line and parks in `cpu_v6_do_idle`
   (WFI, IRQs masked — QMP `info registers` at the wedge, PC
   0xc00190ac); the root is never mounted (eMMC image byte-unchanged;
   md5 before/after). The wedge is console-path-specific (same
   boundary under plain TCG; ttyAMA0 boots complete under both TCG and
   icount), pre-dates task 104, and was queued as a desk item: printk
   flush vs the bit-bang earlycon when ttySU0 is the only console.
**RESOLVED 2026-08-28, task 109**: root cause = the earlycon's
`early_ioremap()` mappings die at ARM's `paging_init()`
(`early_fixmap_shutdown()` keeps only the permanent fixmap slots);
the first post-boundary console write faulted on the FRC mapping
(0xffe3f644 = `bcm2727su_early.frc`) and the Oops recursed forever
(`mem_dump_obj` with `mem_map` NULL, then `show_pte` through the
dead fixmap) — the "idle park" was the storm's end state, not a
printk deadlock.  Fixed driver-side (permanent `FIX_EARLYCON_*`
fixmap slots) plus the two hidden follow-on bugs (console never
registered without `port->mapbase`; console tty baud seeding).
su-only boots now reach `e7:~#` with the su42 decode intact; full
story + validation in softuart doc §3 (task-109 note) and
docs/boot-109-su-full.txt.

### 5.3.1 Task-122 shape: the faithful BB5 entry (2026-09-05)

Task 122 replaced the entry-into-main approximation with the genuine
BB5 chain — everything from entry to `e7:~#` is now phone code
except the documented §4 walls:

1. **Entry.** PC = shim (unchanged, SRAM bottom), LR = board args
   stub 0x40202100 → zeroes r0-r3, jumps to the ARM entry
   **0x100103D0** (= 0x10010000 + the BB5 +0x148 field).  The
   prologue (file 0x3D0-0x4B4: SCTLR/I-cache dance, CPACR/VFP,
   mode stacks from the 0x1002F4C0-0x1002F500 pool, SYS sp =
   0x30003E00) saves the args at 0x30003E00-0x0C and blx's into the
   scatter loop (file 0x4E0).
2. **Scatter loader.** The three §7.10 records run from the image:
   RAM-veneer blob → 0x30001800, the LZ stream → the genuine
   globals 0x10007000-0x100074DC (twelve command words,
   33-record device-ID table, USB descriptors VID 0x0421/PID
   0x0354, HAL/service table), zero-fill → 0x100074DC-0x10008E50.
3. **Init tree.** 0x10010628's call set runs C-runtime setup, then
   the HAL thunk (file 0xDAC) dispatches service[0] = the EM-ASIC
   probe over the new CBUS-bridge model ("Pearl-J : v1.0"), the
   CBUS clock config, XTI enable — then the veneer tails into main
   (file 0x1259A).
4. **Genuine curation — the §5.2 starvation premise retired.**
   Under the single map the shadow-list address 0x10024178 holds
   the image's OWN bytes (file 0x14178 = a real in-image list:
   PASUBTOC, PAPUBKEYS, VARIANT, HWC, NPC, CCC, LDSP, SOS+UPDAPP,
   SOS*UPDAPP, SOS+ENO, SOS*ENO, MCUSW …) — the class searches HIT
   ("TOC entry added" ×4, MCUSW/PRIMAPP/PASUBTOC/PAPUBKEYS), the
   mode-nibble scrub is skipped, and NLoader RE-EMITS its own
   4-record SDRAM TOC over slots 0-3 (the §4 end-marker NOP still
   guards slots 4+ = the shim's surviving flash entries, which the
   copy-images walk consumes exactly as before).
5. **Genuine allocation.** The record walk (0x10021ea0) validates
   each record's load through 0x1002149a — (load+entry) &
   (size−1) == 0 plus region containment (the region map comes
   from the bank cells at 0x30003E60/64: 256 MB at 0x10000000 →
   sandbox 0x1F880000-0x1FFFFFFF).  The board preloads the
   synthetic MCUSW slot's load with the window NLoader itself
   would allocate — **0x101FFC30** (heap end 0x101FEFFF →
   0x10200000, minus the record's entry 0x3D0) — so validation
   passes with no reallocation, the MCUSW copy is skipped (record
   size 0) leaving the trampoline intact, and the launcher's
   "Start SOS at 101FFC30" jumps into it.  The zImage stays in
   SDRAM at 0x80010000 (the trampoline's r3 word points at it).
6. **Launch.** Cleanup's MCUSW search hits the rebuilt slot 0, the
   mode-2 resolution reads back 0x101FFC30, "Disable MMU" (the
   §4-bypassed wrapper), "Start SOS at 101FFC30 (->101FFC30)" →
   the chain trampoline → Linux → ROOT-UP banner → `e7:~#`.

**New pre-main prints, identified (print site → what):**

| line (boot order) | site | what it is |
|---|---|---|
| `HW : RAP3Gv0.00 PA` | 0x10011642 (fmt 0x10011850) | init-tree platform pass; the "%s" variant is now "3G" from the genuine ASIC-ID path (the stub era printed "RAPg") |
| `acrc.MCDCR: 00000000` | 0x1001164C (fmt 0x1001186C) | ASIC debug-register readback, same pass |
| `Pearl-J : v1.0` | 0x100106E4/0x100106FA (fmt "%s : v%d.%d" @0x10010ACC) | EM-ASIC detect, service[0] via the HAL thunk 0x10010DAC; type/version/rev = 0x10/5/0 from the CBUS model |
| `CBUS : 4723 kHz` | 0x10010CF0 (fmt 0x10010D98) | serial-bus clock config (same service family) |
| `BSI out of range` | 0x100107D0 (fmt 0x10010B14) | battery-size-info read via the genuine path (stub era returned 100 = in range; hw_conf later prints BSI 65530) |
| `Startup Reason: 00000000` + `No valid start-up reason -> Power off` | reason re-read 0x10012D42; print 0x100225E4 (fmt 0x10022754) | the genuine hardware read returns 0; the power-off path is entered and the §4 NOP at 0x100225F2 carries the flow past the hang |
| `Set Gazoo VCore (1425000 uV)` | 0x10010CA2 (fmt 0x10010D68) | the genuine EM VCore call; followed by the familiar `Set VCore : 1425 mV, return : 54` (the EM path's return vs the stub's 100) |
| `TOC entry added : …` ×4 | 0x10020C0C (toc_entry_add) | the genuine record curation running (see 4 above) |
| `Load address : 10200000 added for image : …` | 0x10021EF4 (fmt 0x100221EC) | the genuine allocator, pass 2 of the record walk |
| `Check ROFS images` | 0x10021F18 (fmt 0x10022218) | post-record ROFS pass, newly reached |
| `COMA load FAIL` | 0x10022582 (fmt 0x100226D8) | COMA image load attempt, newly reached (non-fatal) |
| `SmartReflex DSP code not found` / `sr_vcore_status_set(0x13)` / `… VCore status updated!` | 0x1001FCD8 / 0x1001325C | SmartReflex paths, newly reached; the status set runs through the genuine EM |
| `hw_conf: SDRAM: 256 MB at 10000000`, `Sandbox : 1F880000 - 1FFFFFFF` | config build | derived from the SWBL-faithful bank cells (was "512 MB"/0x2F884F00 under the retired record-base hack) |
| `1FFFF000: 00000000 …` (~250 lines) | config-build readback | the hw_conf records now land at 0x1FFFF000 — mapped DDR top on the phone, unmapped (dropped) in our model; cosmetic, nothing consumes them |

Lines that DISAPPEARED vs the task-118 flow, with reasons:
`SOS core image not found -> try ENOSW` / `No executable image found`
(the class searches now HIT — the §4 spin NOPs became dead code);
`Image: MCUSW copying failed` (record size 0 → no copy attempt —
this is what keeps the trampoline alive); `Image: SWBL/ADA/R&D
copying failed` (the rebuilt table's slots 0-3 replaced the shim's
first entries; SOS+PMML/SOS+DECOMP/PASUBTOC/PAPUBKEYS/PRIMAPP/
RAP3NAND survive at slots 4+ and still walk).  The
`SOS+PMML…% 0-0-8 %s` garble in the source/target table is the
rebuilt-slot-0 name read running into the next field — cosmetic.

**Validation (2026-09-05, task 122):**

1. *Primary (plain TCG, E7SW_DEBUG=1 ONENAND_DEBUG=1):* full genuine
   chain → `e7:~#`; interactive `mount` shows `/dev/root` ext4 rw,
   `/proc/interrupts` Err: 0, `/dev/mmcblk0` present.  Evidence log:
   `emulation/qemu-e7sw/boot-bb5-task122.log`.
2. *icount control (`-icount shift=auto`, no debug envs):* also
   reaches `e7:~#` with the identical 1202-line NLoader flow
   (docs/boot-122-icount-ttyama0.txt).
3. *Post-scatter globals (QMP pmemsave 0x10007000/0x4DC at the
   prompt):* twelve command words at +0x1E4 byte-match §7.10(b)
   ({0000,0013,0080,001A,0094,0095,000E,000C,007D,007F,007F,0003});
   the 33-record device-ID table at +0x1FC ends at the mfr-0
   terminator (record 33), with the 8192-block EC/0x68/0x0000 record
   at record 4; USB descriptors at +0x74-0xB0 carry VID 0x0421/PID
   0x0354 (device descriptor, MTP config).  The LLD open read
   IDs EC/0068/0000 (ONENAND_DEBUG trace) and first-matched the
   8192-block record: the identity LSN mapping held (search LSN
   0x100/0x200/0x500 → device blocks 1/2/5, byte offsets
   0x20000/0x40000/0xA0000) and the BMI probes ran through block
   8191.  (The devinfo byte cells at §1.2's offsets now read
   {+0x22: 0x100, +0x26: 4, +0x52: 0x1000} — the genuine record's
   field semantics differ from the staged-record cells §1.2
   recorded; the mapping evidence above is the load-bearing proof.)

## 6. Run command

Identical to docs/bcm2727-pinctrl-linux.md §5 (root=/dev/mmcblk0
variant); new env knobs: `E7SW_DEBUG=1` (shim/LLD logging),
`ONENAND_DEBUG=1` (model command trace).  The `-drive id=onenand` was
and remains required — now it is read through the model instead of
being preloaded by the board.

---

## 7. SWBL decoded (task 115, 2026-08-31)

**Sources:** static disassembly of SWBL's real flash copy —
`firmware-re/nokia-e7-onenand.img` file 0x140800-0x142A00, flash-TOC
record `'ADA'` {off 0x800, size 0x2200} under §7.4's reader format.
Recursive disassembly (1950 insns, mixed ARM/Thumb) from the BB5 entry;
tool `tools/arm_recurse_dis.py` (committed).  Confidence legend as
ever: CONFIRMED = decoded from these bytes and cross-checked against
content; INFERRED = decode with one interpretive step; OPEN = not
decidable from this image.

### 7.1 Image, BB5 header, extent (exit criterion 1)

Offsets below are **img+X** = X bytes from the image start (= device
offset − 0x800; the image is position-independent, so it has no VA of
its own — its load site is owned by the secure ROM, OPEN; the
`0x8000` word at +0x160 is the only load-site hint).

| field | value | meaning |
|-------|-------|---------|
| +0x00 | 0x809795A3 | BB5 magic (CONFIRMED, matches §5.2's container family) |
| +0x04/+0x08/+0x0C/+0x10/+0x14 | 2 / 0x98022316 / 0x501318F6 / 0x00076EAD / 0x009AFFF2 | version + four hash/ID-class words (OPEN individually) |
| +0x18 | 0x168 | end of the header directory (the directory does end at 0x168) (INFERRED) |
| +0x1C | 0x3D0 | entry offset — mirrors +0x148; entry is ARM state |
| +0x20 | 0x1D2F | last content offset — matches the bytes: content ends 0x1D2F, 0xFF (erased-flash) fill 0x1D30-0x1DFF, zeros 0x1E00-0x2200 |
| +0x24-0x37 | 20 B | signature (RSA-style truncated) |
| +0x38-0x57 | 28 B | second signature/hash block |
| +0x148 | 0x3D0 | **entry-point field** — the same field NLoader's launcher reads (`load + *(u32*)(load+0x148)`, §5.2); nloader.bin carries 0x3D0 here too |
| +0x14C | 0x84 (SWBL) / 0x5 (nloader) | per-image type/flags (OPEN) |
| +0x158 | `'NALO'` | image-family tag, same in nloader.bin (OPEN what it names) |
| +0x160 | 0x8000 | flags/load hint, same in nloader.bin (OPEN) |

0x350-0x3CF = 0x80 bytes of high-entropy data (certificate/key hash,
INFERRED from position right before the entry).  Code entry
**img+0x3D0, ARM** — `cpsid aif`, SCTLR manipulation, I-cache/branch
predictor invalidate, CPACR VFP enable (0x500000), FPEXC/FPSCR setup,
then `add r4,pc,#1; bx r4` → **Thumb from img+0x448** for everything
after.  The 0x2200 TOC size is the allocation; the effective image is
0x1E00 (0x1D2F content + 0xFF pad), and SWBL itself never reads past
the first staged page.  Entropy 2.6-6.8/KB throughout — no encrypted
or compressed region; the payload disassembles cleanly end to end.

### 7.2 Boot sequence decoded (exit criterion 2)

All addresses img-relative; every print string is observed at the
cited site (PC-relative `adr` loads — the image is fully
position-independent).

| img+ | code | behavior |
|------|------|----------|
| 0x3D0 | entry (ARM) | `cpsid aif`; SCTLR: I-cache off→inv→on, clear MMU/DCache/align/vectors(0x2200); CPACR=0x500000 (VFP on); flush; `bx` to Thumb 0x448 |
| 0x448 | Thumb start | saves ROM args **r0-r3 → 0x30003E00-0x30003E0C**; `sp = 0x30003E00` (stack in the 0x3000xxxx scratch RAM); call main |
| 0xA24 | main | `memset(0x30003E00, 0, 0x90)` — SWBL itself clears the whole handoff block incl. the startup-reason cell 0x30003E7C and the TOC-pointer cell 0x30003E78; `[0x30003700]=0`; platform init (img+0x18D4); XTI enable; `sz = loader_sdram_init()`; if ≤0 → "SDRAM init failed" + hang; banks: bank0=min(sz,0x10000000)→`0x30003E60`, bank1→`0x30003E64`; print "RAMSET SW : (18:41:18 Jul 27 2012)"; boot flow (img+0xA12); launch (img+0x904) |
| 0x18D4 | platform init | timeout cell **[0x30003E80]=0x154**; clock/ID read via [0x20030300]=0xA handshake (platform byte → 0x30003724+0x20, nibble → 0x30003744+0xC); mux/clock gates 0x20033000/0x20033100/0x20033140/0x20033340; peripheral domain enables [0x20040200+4..0x18]=1, [0x20040600+4]=0x1800; NAND-side reset dance via img+0x1290 (0x20032600+0x64/+0x6C, 0x9480/0x9400/0xF400 steps); pinmux via img+0x17B4 (pin table at 0x300027DD, "CS" split >0x20, CBUS write/read helpers img+0xEFC/0xF1A over the CBUS bridge **0x20030700** with the "CBUS Busy!" timeout at img+0xEC8) |
| 0xE76 | XTI enable | [0x40000000+4]=1, +0x10=0x15, +8=3; "XTI pipe cleaner" (0x10000-iteration delay); "XTI enabled" |
| 0xE60 | print() | logs to **XTI RAM**: buffer 0x40100000+ch·0x1000, channel 0xEF → **0x401EF000**, 0xC7 marker byte, done-flag byte at 0x401FD800; full vsnprintf (%d %08X %s %c…) at img+0xEA0.  (The shim's port-0x40208000 log is the emulation analog.) |
| 0x151E | MuxOneNAND/DDR init (img+0x19B8 loader_sdram_init = print ' loader_sdram_init' + this + PHY tail img+0x1766) | DDR controller at **0x09000000** (+0x10=2, +0x40 row/col, +0x44=0x1300, +0x60=0xA, +0x70 flags, +0x80 config 0x4DE01/0x43101 by variant, +0xC0+0x14); PHY at 0x20031800 (+0x14/+0x18/+0x54/+0x58) and 0x200319C0+0x28; second PHY bank 0x08000000/0x08000140; per-chip calibration via img+0x1388 with variant tables (0x090000A8 vs 0x090000D8 base); chip report "CS%d : %2d MB (Row: %2d, Col: %d)"; dual-channel flag → [0x30003E84]=1; "DDR %d MB (32bit) configured (%d kHz)" with kHz 0x24900 (149.76 MHz, dual) or 0x2A300 (172.8 MHz); returns total bytes |
| 0xA12 | boot flow | ① geometry/ID query (img+0x9DE): `[0x30003744+0xE]=3` (mux mode), detect (img+0x1174) with stack args {2,4,4}, then copies the detect struct 0x30003710 into handoff cells 0x30003E10-0x18 = {size-class, **Mfr, Dev, Rev**}; ② flash-TOC read (img+0x984); ③ NLoader copy+auth (img+0x91C) |
| 0x984 | flash-TOC read | block number from boot-select cells: `u16[0x300003FC]==0x5342('BS') ? u16[0x300003FE] : u16[0x300004FE]`; src = block << 17; **dst = 0x1000C000, size = one page (0x800)**; "Reading TOC from block %d"; then **`[0x30003E78] = 0x1000C000`** (img+0x9C4, `str r5,[r0,#0x78]`) — the published TOC pointer; then TOC search (img+0xB80) for `"RAP3NAND"` → record ptr → [0x30003704]; miss → "NLoader image not found" + hang |
| 0xB80 | TOC search | walks the **published pointer** ([0x30003E78]); 32-byte records, **name[12] at record+0x14, off@+0x00, size@+0x04**; terminator name[0]==0xFF; bound toc+0x800; name compare 12 bytes (memcmp img+0x1AC0); modes: 0→record ptr, 1→[rec], 2→[rec]+toc_base (modes 1/2 unused on this path) |
| 0x91C | NLoader copy+auth | src = rec.off + block_base; size = min(roundup(rec.size, 0x800), 0x20000 − rec.off); "Address : %08X, size : %d"; copy → **0x10010000**; "NLoader copied to SDRAM"; **authenticate**: secure-monitor call (img+0x89C → img+0x4A0 → `smc #0`; cache-clean first at img+0x18A2; claim [0x20030300]=0xA; status magics {0x1004,0x2007,0x1104,0x2107,0x2108}; secure-ROM service **0xFFFD120C** (Thumb)) with (0x3000277D, 0x1F, image_ptr); must return 1 else "NLoader authentication failed" + hang |
| 0x904 | launch | `[0x30003E08]=0x30003E00` (self-pointer into the handoff); entry = **0x10010000 + [0x10010148]** (the BB5 +0x148 field = 0x3D0); launch trampoline img+0x1C54: `mov r4,r0; mov r5,r1; r0-r3 = [0x30003E00..0xC]; bx r5` — restores the ROM's original args and jumps |

A whole secure-monitor ecosystem sits at img+0x530-0x824 (ARM,
`smc #0` at img+0x674, mode-switch dance `cps #0x13`/`msr cpsr_fsxc`,
secure cell 0x20400040) — the SWBL-side machinery behind the §4
secure-ROM patches.

### 7.3 The embedded OneNAND driver and the base constant (question e)

SWBL carries a register-level MuxOneNAND driver (the strings' own
family: 'MuxOneNAND (CS%d) detected (Mfr: %04X, Dev: %04X, Rev:
%04X,', 'MM_SYSCFG1 : %4X', 'MuxOneNAND (CS%d) configured (%d kHz)',
'Copying page from %d to %d', 'Page (%d) copying failed'):

- **Base = `cs << 26` — CS0 at PA 0x00000000**, CS stride 0x04000000
  (CONFIRMED, img+0x1096 `lsls r0,r0,#0x1a` on the CS argument; the
  only boot-path call passes cs=0).  **This refutes 0x20010000 as the
  boot-time OneNAND base** — §2's CONTRACT stays valid as the
  kernel-era mapping (the pre-map at VA 0xC6012000), but the phone's
  SWBL talks to the boot flash at PA 0.  Running the real SWBL blob in
  the chain would need the model (or an alias) at PA 0 — not done, not
  needed for the chain's goals.
- Register protocol (page read, img+0x1096), offsets from cs-base:
  SA1 +0x1E200 = block (with dual-die split: SA2 +0x1E202 = die
  select 0x8000·k, from Dev-ID bit 3 — 0x0068 has it set, dual-die
  1 GiB ✓), SA8 +0x1E20E = (page&63)<<2, StartBuffer +0x1E400 =
  0x800, Interrupt +0x1E482 = 0 (clear) then poll bit 15, Command
  +0x1E440 = 0x00 (READ), data 2 KiB from DataRAM0 **+0x400**, error
  check: u16[+0x1FE00] & 0xAAAA == 0.  SA1/SA8/StartBuffer/Command/
  Interrupt/DataRAM0 offsets match the §2 model exactly.
- **Deltas vs the model:** IDs are read at **+0x7800/+0x7802/+0x7804**
  (Mfr/Dev/Rev; model puts IDs at +0x1E000+) and the page-size class
  at **+0x780A** (b6 = 13 − reg>>8 → 11 = 2 KiB pages; b7 = 6 → 64
  pages/block); the model implements neither +0x7800 window nor the
  +0x1FE00 status read.  Matters only if the real SWBL/LLD blobs are
  ever run against the model.
- SystemConfig1 (MM_SYSCFG1) written at **+0x1E442** =
  0x80F2 | width<<12 | half<<2 (16-bit bus), printed as
  'MM_SYSCFG1 : %4X'.
- Mux/clock config (img+0xFAC): bus mux 0x20033000+0x38, timings
  0x20038200+0x48/+0x4C or +0xC/+0x10, mode from [0x30003744+0xE]
  (SWBL sets 3); kHz table: dual-DDR → 49,920 (0xC300) or 74,880
  (0x12480, mode 3); single → 57,600 (0xE100) or 86,400 (0x15180) —
  the mystery "pointer-table-looking" pool words before 'MuxOneNAND'
  were these two kHz values, not pointers.

### 7.4 The flash TOC — the real reader format (question a; §1.4 correction)

CONFIRMED from the reader (img+0xB80) and byte-verified against image
content (nloader.bin md5-matches device 0x4000+0x20000):

- 32-byte records from device 0, **no 20-byte header**:
  `{u32 off@+0x00, u32 size@+0x04, rsv@+0x08, rsv@+0x0C, u32 load@+0x10, char name[12]@+0x14, rsv}` —
  terminated by name[0]==0xFF (**32 entries**, terminator at device
  0x414; the doc's earlier "14 entries" and "20-byte header +
  [name:12][off:4][size:4]" parse paired each name with the
  *following* record's {off,size} — one slot off; the names sit at the
  same bytes either way).
- Consequences, byte-verified: the loader image this doc calls "SWBL"
  is TOC-named **'ADA'** {0x800, 0x2200}; **'RAP3NAND' = {0x4000,
  0x20000} = nloader.bin** (what SWBL loads as NLoader); 'PRIMAPP' =
  {0x3000, 0xC00, load 0x108C4000}; the 2 MB kernel image at
  0xC0000 is **'SOS*UPDAPP'** {0xC0000, 0x1FFD51, load 0x108FF000}
  (not SOS+IVE3B); 'GENIO_INIT' = {0x84000, 0x43A0} (the repo's
  105,692-byte firmware-re/genio_init.bin was extracted under the old
  parse from 0x1200000 — that location is 'SOS+IVE3A' under the
  reader's pairing; the two candidate images' headers should be
  re-checked if genio work resumes).
- **Re-verification pass (2026-09-01, independent, post-landing):**
  the corrected pairing re-parses identically from the bytes (all 32
  records), every image-bearing record's off now lands on a BB5 magic
  in the dump (device 0x800/0x4000/0x24000/0x50000/0x5C000/0x84000/
  0xC0000/0x1200000/0x1260000), and **RAP3NAND at device 0x4000
  (file 0x144000) is byte-identical to nloader.bin** (md5 a8eaa8f7…,
  0 diffs over the full 0x2E000 — the runner's md5 claim reproduces).
  Two precisions: (a) the record's size field (0x20000) is smaller
  than the actual image — nloader.bin spans 0x2E000, its last 0xE000
  bytes running into the NEXT image (nld[0x20000] is the BB5 magic of
  the image at device 0x24000); md5 of "device 0x4000 for 0x20000" is
  9c8fc3b2… — not a mismatch, just the truncation.  (b) the dump also
  carries a NEAR-twin of the NLoader image in the container region at
  file 0x4000, differing from the device copy in exactly 9 bytes
  (+0xFD01/0x1072D/0x113B3/0x11859/0x11A67: d0→e0, and two 2-byte
  pairs at +0x10BC2/+0x1246A) — the DEVICE copy is the authoritative,
  byte-exact one.  The PRIMAPP line above also had its off/size
  digits corrected in this pass (0x3000/0xC00; load was already
  right).  Arithmetic gotcha for anyone re-probing: device X = file
  X **+** 0x140000 (0x4000 → 0x144000) — concatenating
  (0x1404000) probes device 0x12C4000, deep in ROFS.
- **Question (a) answer:** SWBL stages the RAW flash-TOC page (one
  2 KiB page, block from the boot-select cells) at **0x1000C000** and
  publishes **[0x30003E78] = 0x1000C000**.  It applies **no curation**
  — it searches exactly one name ('RAP3NAND').  **0x101ff000 appears
  nowhere in SWBL**: that address is NLoader's own Build-TOC rebuild
  target (§5.2's writer decode).  The shim therefore compresses two
  hardware steps into one (stage+curate at 0x101ff000); the pointer
  cell is faithful, the staged address and content are not — harmless
  for the chain (consumers only follow the pointer) but a fidelity
  delta, and it explains §5.2 item 2 cleanly: on hardware NLoader's
  record curation reads the RAW TOC through the published pointer.

### 7.5 NLoader load base, the LLD gap, the shadow list (questions b, c)

- **Load base CONFIRMED 0x10010000** (img+0x91C literal; copy size
  min(roundup, 0x20000−off) = 0x1C000 for RAP3NAND →
  0x10010000-0x1002C000), entry = base + [+0x148] = 0x100103D0.
  nloader.bin is *linked* at 0x10020000 (its entry stacks pool
  0x1002F4C0…; the whole §1.1/§5 decode is consistent there and the
  chain runs it there), so NLoader's own img+0x3D0 prologue +
  relocation block (file 0x4E0+, classic pc-relative copy loop)
  relocates it after SWBL's launch — the NLoader-side relocation
  semantics are OPEN, but the SWBL side is pinned: **one copy, at
  0x10010000**.  The board's "mirror at 0x10000000 + primary at
  0x10020000" is an emulation approximation of the end state, and its
  comment "the 0x10020000 that SWBL loads on hardware" is wrong by
  0x10000.
  **[Task 118 prologue probe]** entering the chain via the real BB5
  entry (board LR = 0x100103D0 after the shim) walls immediately:
  the prologue (decoded: SCTLR/I-cache dance, CPACR/VFP setup, mode
  stacks from the 0x1002F4C0 pool, handoff save at 0x30003E00, then
  the scatter-relocation loader) is linked for the SWBL single-copy
  layout — its first out-call `blx 0x10016282` (file 0x6282's
  helper) and the loader's call tree (0x10020482 = file 0x10482,
  0x10022806/0x100239d8/0x10023c20) all assume file X at
  0x10010000+X, while the double-load provides the main body at the
  primary base and the mirror puts different bytes at those VAs
  (undefined-opcode cascade into the PA-0 vector page).  Making it
  run needs the whole image at the SWBL base — but the dump's file
  0x18000-0x1C000 (the SWBL-base main body's upper half) is ZEROS:
  on hardware the scatter-relocation loader itself must populate that
  range, which is the OPEN item above.  Decoding the relocation
  table (img+0x51C bounds, the ldm/sub-lr/bx-r3 trampoline loop) is
  the prerequisite for any faithful BB5-entry chain — parked.
  **[Task 121 — decoded, §7.10; both premises resolved]** (1) The
  linked base IS the SWBL base 0x10010000 (scatter fn words point at
  file 0x580/0x524/0x5A8, the init veneer resolves main at file
  0x1259A under 0x10010000+file, the mode-stack pool 0x1002F4C0 only
  works above the 0x1C000 copy): there is no self-relocation to
  0x10020000 — the OPEN item "relocation semantics" closes as "the
  scatter plan only initializes globals (§7.10); the single copy at
  0x10010000 is the final layout".  (2) The zero-range premise is
  REFUTED: the plan's three records write only 0x30001800+ and
  0x10007000-0x10008E50, nothing references 0x10028000-0x1002C000,
  and the image's real content ends at file 0x151FF — the zeros
  (0x15200-0x20000 contiguous) are correct padding, not content the
  loader owes.  The wall's true cause: the chain maps NOTHING at
  0x10010000 (mirror+primary put file X at 0x10000000+X/0x10020000+X,
  never at 0x10010000+X).  Verdict: faithful BB5 entry is BUILDABLE
  from the stock dump — map file 0-0x1C000 at 0x10010000, enter
  0x100103D0; §7.10 lists the two emulation-side knobs that then
  surface (model VerID vs the genuine ID table; retiring the §4
  staged cells).  **[Task 122 — the OPEN is now resolved BY
  EXPERIMENT, §5.3.1]:** the faithful entry ran end to end (`e7:~#`
  under TCG and icount) — the relocation-semantics reading above
  ("the scatter plan only initializes globals; the single copy at
  0x10010000 is the final layout") is exactly what executed.
- **Question (c): the vtable-gap payload is NOT erased.**  §1.1's
  slots 0x1001E5F3/0x1001E6DB/0x1001E903/0x1001E97D/0x1001E533/
  0x1001EAB1/0x1001E9FF = 0x10010000 + file 0xE5F3/0xE6DB/…/0xE9FF —
  **nloader.bin file 0xE000-0xF000 carries real FSR-style LLD code**
  (slot+0x00 = init/open shape, slot+0x0C = the page-read signature,
  slot+0x10 = the 0x34-stride GetDevInfo; pool constants 0x1E440/
  0x1E480 at file 0xE1F4/0xE638/0xEAA4/0xEED0/0xEED4).  The zero
  region is file **0x1E000-0x1F000** → under the board's
  "0x10000000+file_off" pointer convention that became
  0x1001E000-0x1001F000, but under SWBL's real base the slots point
  at file 0xE000-0xF000 where the code sits.  The chain's double-load
  never maps file 0xE000 to 0x1001E000 — which is the actual reason
  the task-104 lld.bin trampolines were needed.  SWBL itself carries
  no separate gap payload and never writes 0x1001Exxx (the gap rides
  the flash image).  Loading NLoader at 0x10010000 (or remapping just
  the gap) would expose the genuine LLD — parked as a future task; no
  changes made here.
- **Question (b): the shadow image-list 0x10024178 is NOT staged by
  SWBL** (constant absent; no write).  The address falls *inside*
  SWBL's NLoader copy range (= nloader.bin file 0x14178), so whatever
  NLoader finds there arrives as flash-image bytes — in our dump the
  'Yama VCore calibration done' string.  §5.2 item 2's "on hardware
  SWBL stages" inference is refuted; whether the phone's flash image
  carries the list in-file or NLoader builds it from the Root-TOC read
  is NLoader-side, OPEN.

### 7.6 The 0x100070xx service block (question d)

SWBL does **not** populate it.  It carries one 3-instruction ARM
accessor (img+0x1C40, unreferenced by the boot path) returning
**{0x10006000, 0x10007000}** in r0/r2 — bounds of a 4 KiB block the
software family knows about.  The HAL table [0x10007040], USB
descriptors 0x10007074-0x100070B0 incl VID/PID, callback table
0x100070ac and [0x100070bc] are NLoader's own doing — noted in
docs/bcm2727-usb-flashmode.md §6 items 1-2.
**[Task 121]** "NLoader's own doing" pinned to the mechanism: the
whole 0x10007000-0x100074DC block is image content, LZ-compressed at
file 0x14EA8-0x1503A and decompressed there by scatter record 2
(§7.10) — descriptors and VID 0x0421/PID 0x0354 included, plus
fourteen Thumb service pointers (→ file 0x6A9-0xCFB, the dispatcher
family behind the loop at file 0x5D4) and the §1.1 command-word/
device-ID cells.

### 7.7 The five questions, verdicts

| # | question | verdict |
|---|----------|---------|
| a | SDRAM TOC at 0x101ff000 + pointer 0x30003e78 + curation | **Pointer cell CONFIRMED** (img+0x9C4); staged address **0x1000C000 raw** (0x101ff000 absent from SWBL); **curation NONE** (one name search).  0x101ff000 = NLoader's rebuild, §5.2 |
| b | shadow list 0x10024178 | **REFUTED for SWBL** (not written; address lands inside the NLoader copy = file 0x14178); NLoader-side OPEN |
| c | embedded LLD for 0x1001E000-0x1F000 | **Gap payload PRESENT in the dump** at nloader.bin file 0xE000-0xF000 (real code, OneNAND constants); SWBL neither embeds nor injects it — it rides the copy; §1.1's "erased" mis-located it (zeros are at file 0x1E000) |
| d | 0x100070xx service block | **NOT populated by SWBL**; only an unreferenced bounds accessor {0x10006000, 0x10007000}; populating is NLoader's job |
| e | OneNAND base constant | **cs<<26 = PA 0 for CS0** (CONFIRMED) — refutes 0x20010000 as the boot-time base; §2's CONTRACT stands as the kernel-era mapping.  Register deltas: IDs at +0x7800, page-class +0x780A, status +0x1FE00 (& 0xAAAA) |

### 7.8 SWBL-vs-shim fidelity table

| SWBL behavior (source of truth) | shim/chain today | gap? | worth fixing? |
|---|---|---|---|
| stages RAW TOC page at 0x1000C000, publishes [0x30003E78] | curates+stages converted table at 0x101ff000, publishes same cell | yes (address + content) | not for the chain's goals; the faithful shape is the parked §5.2-item-2 path (stage raw → NLoader's real Build-TOC curates) |
| no curation; one name search 'RAP3NAND' | §5.3 curation rules (≥1 MiB, SOS+IVE3*, name[3] '*'/'/', synthetic MCUSW slot 0) | the rules are chain-mode inventions standing in for NLoader's Build-TOC | no — they exist to steer NLoader, not to mimic SWBL; document (this table) |
| TOC records {off@0, size@4, load@+0x10, name@+0x14}, 32 entries, 0xFF terminator | shim parses name@entry+0/off@+0xC (one slot shifted) and writes name@+0x14 (output format matches) | yes — every staged name carries the next record's {off,size} | only if a consumer ever keys on names (none does today except the synthetic MCUSW); cheap fix if revisited |
| loads NLoader at 0x10010000, entry via img+0x148 field, args-restore trampoline | **RESOLVED (task 122): single copy file 0-0x1C000 @ 0x10010000, entered through a board args stub (r0-r3 = 0) at the ARM entry 0x100103D0 after the shim — the real prologue, scatter loader, and init tree all run (§5.3.1); boots `e7:~#` under TCG and icount** | closed | — |
| LLD gap present at file 0xE000-0xF000 → 0x1001E000-0x1F000 at SWBL's base | **RESOLVED (task 118): the board maps the full driver (file 0xB000-0x10000) at 0x1001B000 — the genuine FSR LLD serves all 22 installer slots; the lld.bin trampolines are retired.  Boot log `emulation/qemu-e7sw/boot-lld-task118.log` (E7SW_DEBUG=1 ONENAND_DEBUG=1): NLoader flow byte-identical to the task-104 baseline, model traffic = 4× ID-register triples at +0x1E000 (the LLD's own reads), the BMI block-class probes, and the three search reads — all through the real driver** | closed | — |
| IDs/geometry handed to NLoader via 0x30003E10-0x18 (size-class, Mfr, Dev, Rev) | shim pre-fills BML devinfo cells directly | different mechanism, same intent | no |
| OneNAND at PA 0 (cs<<26); IDs +0x7800; status +0x1FE00 | model at 0x20010000; IDs +0x1E000; no +0x1FE00 | yes | only matters for real-SWBL runs; leave the §2 contract |
| authenticates NLoader via secure monitor (SMC → ROM 0xFFFD120C), hangs on failure | crypto stubs (§4) | expected artifact | no |
| full DDR init, bank sizes → 0x30003E60/64 | RAM preconfigured by QEMU | expected artifact | no |
| logs to XTI RAM 0x401EF000 (ch 0xEF, 0xC7 marker), enables XTI 0x40000000 regs | shim logs to port 0x40208000 | different mechanism, same purpose | no |
| zeroes 0x30003E00-0x30003E90 at start (incl 0x30003E7C) | board RAM starts zeroed | behaviorally equivalent | no — and it corroborates §4's task-112 claim that cell-clearing is SWBL's job on hardware |
| never touches 0x100070xx / 0x10024178 | chain also leaves them alone | none | no |

### 7.9 Reproducibility

```
dd if=firmware-re/nokia-e7-onenand.img of=/tmp/swbl.bin bs=1 skip=$((0x140800)) count=$((0x2200))
python3 tools/arm_recurse_dis.py /tmp/swbl.bin 3d0 1a00 0        # recursive, VA0=img+0x3D0
python3 tools/arm_recurse_dis.py /tmp/swbl.bin 3d0 1a00 0 --linear thumb   # gap sweep
```
Strings annotate inline (PC-relative `adr`); literal-pool loads print
resolved values; `blx` mode switches are followed.  Flash-TOC walk in
SWBL semantics: names at 0x140000+0x14+k·0x20, {off,size} at +k·0x20,
load at +0x10+k·0x20, k=0..31, 0xFF terminator at 0x140414.

### 7.10 The scatter-relocation plan (task 121, 2026-09-05)

**Sources:** the loader loop + helpers at nloader.bin file 0x4E0-0x5D4,
the record table at file 0x145BC-0x145EC, the packed blobs at file
0x146FC+ (`firmware-re/analysis/nloader.bin`, md5 a8eaa8f7 — byte-exact
vs the flash device copy, §7.4), all read with SWBL-base semantics
(VA = 0x10010000 + file).  Legend as §7.  Decode-only task: no
QEMU/kernel/blob changes.

**The loop (file 0x4E0-0x523, CONFIRMED):**

```
100104E0  bl   0x100104E8    ; entry veneer: the Thumb prologue's blx
                              ;   (file 0x4BE) lands here and falls into
                              ;   the loop; the bl below is a parallel
100104E4  bl   0x1001064C    ;   init-tree veneer, unreferenced on this path
100104E8  add  r0, pc, #0x2c ; r0 = 0x1001051C = table header (file 0x51C)
100104EC  ldm  r0, {r10,r11} ; {0x000140A0, 0x000140D0} — relative to r0
100104F0  add  r10, r10, r0  ; r10 = 0x100245BC = first record (file 0x145BC)
100104F4  add  r11, r11, r0  ; r11 = 0x100245EC = end of records
100104F8  sub  r7, r10, #1   ; branch base for odd fn words
100104FC  cmp  r10, r11      ; loop head — also the common return address
10010500  bne  0x10010508
10010504  bl   0x1001064C    ; records exhausted: init tree → main
10010508  ldm  r10!, {r0-r3} ; record = {arg0, arg1, arg2, fn}
1001050C  sub  lr, pc, #0x18 ; lr = 0x100104FC (every call returns to head)
10010510  tst  r3, #1        ; fn odd → ARM call at r7−fn, else bx fn:
10010514  subne pc, r7, r3   ; every record IS a call — the loop ends only
10010518  bx   r3            ; because the last call never returns
```

Structure (CONFIRMED): the {start,end} pair is relative to the header's
own runtime VA 0x1001051C — the words' 0x0001xxxx top half lands the
records at file 0x145BC-0x145EC (not 0x100140A0 as the task brief's
parenthetical guessed, and not the primary base either);
0x40D0−0x40A0 = 0x30 = 3 × 16-B records, one 4-word record per ldm —
the consumption evidence for the record shape.  The header's two words
are followed at 0x524 by helper B's ARM→Thumb switch (`add ip,pc,#1;
bx ip`) — code, not table.  Record shape {src(r0), dst(r1), len(r2),
fn(r3)} — the classic scatter convention: the loop calls fn(src, dst,
len) and every fn returns to the loop head.

**The helpers (file 0x524-0x5D4, CONFIRMED):**

| helper | file | semantics |
|---|---|---|
| B | 0x524 (switch) / 0x52C (Thumb) | LZ-family **decompressor**.  Control byte: bits[2:0] = literal count−1 (0 → next byte is the count), bit 3 selects zero-run vs back-reference, bits[7:4] = length field (0 → next byte).  Literals first, then either len zero bytes or (offset byte o → copy len+2 bytes from dst−o).  Stops when dst+len reached. |
| A | 0x580 (ARM) | plain **memcpy**(src=r0 → dst=r1, len=r2): 16-B blocks + 8/4-B tails |
| C | 0x5A8 (ARM) | **zero-fill** of dst for len bytes (r0/src word ignored) |

**The layout table — the complete plan (CONFIRMED):**

| # | source (nloader.bin file) | destination (VA) | size | helper | content |
|---|---|---|---|---|---|
| 1 | 0x146FC-0x14EA8 | 0x30001800-0x30001FAC | 0x7AC | A copy | RAM-veneer blob (Thumb stubs, zeros, an ARM body at +0x40 whose pool calls back into file 0x6205/0x6283); consumed by the init tree — its pool at file 0x13B88-0x13BA8 holds five Thumb pointers into it (0x30001BE9-0x30001EF7) |
| 2 | 0x14EA8-0x1503A (a 0x192-B stream) | 0x10007000-0x100074DC | 0x4DC out | B LZ | the initialized-globals block: fourteen Thumb service pointers (→ file 0x6A9-0xCFB), USB descriptors 0x10007074-0x100070B0 (string+device+MTP config, VID 0x0421 PID 0x0354), callback table 0x100070AC, DSP divider tables, the **twelve command words @0x100071E4**, the **24-record device-ID table @0x100071FC** |
| 3 | — (src word 0x1002503C ignored by C) | 0x100074DC-0x10008E50 | 0x1974 | C zero | .bss — every runtime-written cell §1.1 names: op table 0x10007DD8, per-unit ctx 0x10007EC8, devinfo 0x10007C30, record table 0x100081E8 |

Contiguity (CONFIRMED): record 1's source ends exactly at record 2's
stream start (0x14EA8); the stream ends 0x1503A, two bytes shy of the
0x1503C src word the linker parked in record 3 (pattern data continues
to 0x151FF); record 2's dst ends exactly at record 3's dst start
(0x100074DC).  The post-table word group {0x110, 0x43, 0xFFFFFFF8,
0x40404000} at 0x145EC would be ldm'd only if the records-done call at
0x10010504 returned — it tails into main (file 0x1259A), which never
returns; unreachable on the boot path (intent OPEN, harmlessly).

**(a) The ZERO range — nothing fills it because nothing needs to
(CONFIRMED, four independent ways).**  (1) No scatter destination
touches 0x10028000-0x1002C000 — the three records above ARE the plan.
(2) No aligned or halfword-aligned u32 anywhere in the image points
into 0x10028000-0x1002C000 (or 0x1002C000-0x1002E000) — zero
references.  (3) The image's real content ends at file 0x151FF;
0x15200-0x20000 is one contiguous zero pad (the "zero range"
0x18000-0x1C000 is just its middle) ending at the next flash image's
BB5 magic (0x20000).  (4) The device copy is byte-exact (md5, §7.4)
and the phone boots these very bytes — load-bearing content there
would break the phone too.  Task 118's premise "on hardware the loader
itself must populate that range" is REFUTED: the zeros are correct
final content (padding to the 0x20000 TOC allocation).  The question's
binary ("packed segments vs absent") resolves as neither: the range is
simply unused.

**(b) The 0x10007xxx cells ARE scatter destinations (CONFIRMED).**
0x100071E4 (+0x1E4) and 0x100071FC (+0x1FC) sit inside record 2's
output; the stream decodes (format above) to:

- command words @0x100071E4 — twelve u16: {0x0000, 0x0013, 0x0080,
  0x001A, 0x0094, 0x0095, 0x000E, 0x000C, 0x007D, 0x007F, 0x007F,
  0x0003} — READ/READOOB then the prog/erase/unlock(0x7D)/lock(0x7F)
  family.  The board's staged five {0, 0x13, 0, 0x13, 0x13} are a
  read-path-faithful subset (only words 0/1 feed the boot-read
  sequencer).
- device-ID table @0x100071FC — **33 data records** *(re-verification
  2026-09-05: the initial "24" undercounted — the full walk ends at
  the mfr-0 terminator at record 33)*, 0x14 stride; Samsung 0xEC (dev
  0x78/0x68/0x60/0x58/0x50/0x48/0x40/0x30/0x20), ST 0x20, Toshiba
  0x98.  The open's matcher (file 0xE730-0xE742) is strict three-way
  equality — readMfr == mfr, (readDev & ~7) == dev, (readRev & 0x700)
  == rec+4, no wildcard, **first match wins** (re-verified from the
  instructions: three bne's to the next-record loop, then ctx+0x30 ←
  record).  There are **four** 0x68 records — EC/{0x0200, 4096},
  EC/{0x0000, 8192 — the §1.2 geometry}, **EC/{0x0100, 4096}**, and
  ST/{0x0000, 8192} — so the earlier "NOTHING has class 0x0100 /
  the model's IDs would NOT match" reading was WRONG: the model's
  VerID 0x0121 (class 0x0100) matches the EC/0x68/0x0100 record
  FIRST-MATCH and the open SUCCEEDS — but with **4096-block
  geometry**, silently halving the model's real 8192-block/1 GiB
  device in the BML cells.  The faithful knob conclusion is
  unchanged in direction but different in mechanism: use a
  **0x0000-class VerID** to select EC/0x68/0x0000 (8192 blocks);
  keeping 0x0121 does not fail the open, it mis-geometries it.  Note
  the board's staged record {EC, 0x68, 0x0100, 8192} is a
  combination that exists in NO genuine record (class 0x0100 comes
  only with 4096 blocks).  §1.1's "static cells the image does not
  carry" is corrected in place.

**(c) The chain's mappings vs the plan.**  Base correction first: the
image is linked for **0x10010000** — the SWBL single-copy base — not
0x10020000 (§7.5's claim, corrected there): the scatter table's fn
words (0x10010580/0x10010524/0x100105A8 = file 0x580/0x524/0x5A8,
where the helpers actually sit), the init veneer's pool arithmetic
(ip = 0x11F37+pc = 0x1002259B → main at file 0x1259A Thumb — the
doc's "main 0x1003259a" is those same bytes under the +0x10020000
display convention), and the mode-stack pool 0x1002F4C0-0x1002F500
(stacks growing down into free SDRAM above the 0x1C000 copy; under
0x10020000 they would grow into code at file 0xF4C0) are all emitted
for 0x10010000+file.  There is no self-relocation: the single SWBL
copy IS the final layout, and the "relocation block" is a globals
initializer whose destinations sit BELOW the load image (a separate
execution region at 0x10007000+, plus one blob to 0x30001800).

| chain mapping | approximates | how it silently differs |
|---|---|---|
| LLD map (file 0xB000-0x10000 @ 0x1001B000) | that file range at its own linked VAs — a sub-range of the plan's single 0x10010000 copy | nothing — the only plan-faithful mapping of the three; comes free on the faithful shape |
| primary (file X @ 0x10020000+X) | the main body + packed blobs, +0x10000 above their linked VAs | every absolute linked VA in data resolves 0x10000 high — exactly §4's task-118 repoints (0x10022860/0x100228C6/0x100232B0 = file 0x128xx/0x132xx wrappers; 0x10010680 = file 0x680 even-dst copy); relative bl/b unaffected (why the chain boots); the scatter src VAs 0x10024xxx hold file 0x4xxx bytes, not the blobs |
| mirror (file X @ 0x10000000+X) | nothing — no plan region exists below 0x10007000 | scaffolding only; historically served file-0x1Exxx zeros at 0x1001Exxx (the task-104 "erased LLD" misread); no consumer since task 118 |
| board-staged 0x100071E4/0x100071FC | record 2's output | byte-differs (5-of-12 cmd words; an authored ID record whose rev-class the genuine table lacks — see (b)) |
| — (nothing staged) | record 1 → 0x30001800 and the rest of 0x10007000-0x100074DC | never staged, never missed: their consumers are the prologue/init tree, which the chain skips by entering main directly |
| QEMU zeroed SDRAM | record 3's zero-fill | accidentally faithful |

**Phase-2 wall + faithful-BB5-entry verdict.**  The wall's true cause
(the §7.5 probe re-attribution): the chain maps NOTHING at 0x10010000
— under mirror+primary the prologue's entire absolute world (entry
0x100103D0, helpers 0x100105xx, init 0x100106xx, every pool constant)
lands on wrong or unmapped bytes; the zero range never was the
blocker.  Verdict: **a faithful BB5-entry chain is BUILDABLE from the
stock dump** — map file 0-0x1C000 at 0x10010000, enter 0x100103D0,
and the plan initializes its own globals (the §4 staged cells should
then be retired; the decompressor overwrites them anyway).  Two
emulation-side knobs surface on that path, neither missing dump
content: (1) the LLD open's ID match — the model's VerID 0x0121
  (→ class 0x0100) first-matches the genuine EC/0x68/0x0100 record
  and silently selects 4096-block geometry (see (b)'s re-verification
  note); the faithful knob is a **0x0000-class VerID**, selecting the
  8192-block EC/0x68 record; (2) the entry
must arrive with the ROM/SWBL args r0-r3 (the prologue saves them at
0x30003E00, §7.2's trampoline shape).  §5.2's BB5-container verdict is
untouched: faithful ENTRY is now known-buildable; the faithful kernel
LAUNCH path remains dump-blocked exactly as §5.2 item 2 records.
**[Task 122 — CONFIRMED BY EXPERIMENT, plus two walls the plan could
not see]** §5.3.1 records the full run (both knobs applied, `e7:~#`
under TCG and icount).  The two additions the plan did not list:
(a) the init tree's EM-ASIC probe over the CBUS bridge 0x20030700
hangs deliberately without a valid EM type — satisfied by a small
named-register model with a Pearl-J identity (§2 task-122 additions);
(b) the genuine record curation is NO LONGER starved (the single map
feeds the shadow list at 0x10024178 with the image's own in-image
list, file 0x14178), so NLoader re-emits its own SDRAM TOC and its
record validator/allocator decides the MCUSW load — the chain now
preloads its trampoline at NLoader's own allocation 0x101FFC30
(§5.3.1 items 4-5).

**Reproducibility:**

```
# loop + helpers + matcher (VAs are 0x10010000+file):
python3 tools/arm_recurse_dis.py firmware-re/analysis/nloader.bin 0 700 10010000 --entry 3d0 --max 120
python3 tools/arm_recurse_dis.py firmware-re/analysis/nloader.bin 52c 54 1001052c --linear thumb
python3 tools/arm_recurse_dis.py firmware-re/analysis/nloader.bin e6dc 60 1001e6dc --linear thumb
xxd -s 0x145a0 -l 0xb0 firmware-re/analysis/nloader.bin   # 3 records + post-table group
# record-2 LZ decode + command-word check:
python3 - <<'EOF'
data = open('firmware-re/analysis/nloader.bin','rb').read()
out = bytearray(); i = 0x14EA8
while len(out) < 0x4DC:
    c = data[i]; i += 1
    lit = c & 7
    if not lit: lit = data[i]; i += 1
    ln = c >> 4
    if not ln: ln = data[i]; i += 1
    lit -= 1
    for _ in range(lit):
        out.append(data[i]); i += 1
    if c & 8:
        o = data[i]; i += 1
        for _ in range(ln+2): out.append(out[-o])
    else:
        out += bytes(ln)
import struct
print(struct.unpack_from('<12H', out, 0x1E4))   # command words
print(bytes(out[0x1FC:0x210]).hex())            # device-ID table head
EOF
```
