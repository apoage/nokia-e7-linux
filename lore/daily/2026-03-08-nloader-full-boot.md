# 2026-03-08 — NLoader Full Boot Path (Task 069)

## Summary
Pushed NLoader from 17 debug messages to 34 unique messages. Reached
GENIO configuration build phase, SDRAM reporting, VCore calibration,
DSP loading, and GENIO pad mux application. Hit the TrustZone crypto
wall: GENIO_INIT data is encrypted, crypto stubs return 0 (auth passes
but decryption produces garbage), pad mux writes cause data abort.

## Session Progress

### Phase 1: TOC Pre-population (DONE)
- Comparison function at file offset 0x13084 is ARM-mode `strncmp(r0, r1, r2)`
  - Called via `blx` from Thumb (ARM mode switch)
  - Stops at null bytes (not memcmp), so 12-byte overlap with adjacent strings isn't a problem
- TOC table address: 0x101ff000 (literal pool at both 0x11d20 and 0x1222c)
  - Build TOC function (0x119fc) and Copy images (0x11fee) share the same table
  - Also sets [0x30003e78] = table base (for TOC search function at 0x1277c)
- Extended NLoader RAM: 512KB → 2MB (0x10000000-0x101FFFFF = "PDRAM")
  - 0x101ff000 is at offset 0x1FF000 from base, requires ≥2MB
- Pre-populated 32 TOC entries from OneNAND image at NAND offset 0x140000
- Entry format: [+0] uint32 offset, [+4] uint32 size, [+20] char[12] name, end=0xFF

### Phase 2: Build TOC Skip (DONE)
- Skipped 0x100319fc (Build TOC in SDRAM) with `bx lr` (0x4770)
- Build TOC reads flash via OneNAND driver (NOP'd) → would crash
- We pre-populate the same data structure from the QEMU side

### Phase 3: Flash Read Stub (DONE)
- Stubbed flash page-read function at 0x1002f330: `movs r0, #0; bx lr`
  - Returns 0 = failure, all callers handle gracefully
  - Fixes crash in SUSD check (0x10c14) which searched TOC, found SUSD1,
    then tried to read from flash via 0x10a88 → 0xf330 → vtable dispatch → abort
- "SUSD loading failed" now printed (graceful failure)

### Phase 4: Decompress Hang Bypass (DONE)
- 0xf4fe: "SOS+DECOMP is not available" infinite loop
- Redirected to 0xf4f2 (return success): `b.n 0xf4f2` (0xe7f8)
- Copy images now processes all 32 TOC entries without hanging

### Current Hang: GENIO Configuration Abort
- PC = 0x00EE2D9C (wild address, unmapped)
- PSR = abort mode, SP = 0 (stack gone)
- LR = 0x1002FEEE (file offset 0xDEEE): `str r1, [r0, #20]` — pad mux write
- Cause: GENIO_INIT data is encrypted, crypto stubs return 0 (auth "passes"),
  but decryption produces garbage → invalid pad mux register addresses
- This is the expected TrustZone wall

## Boot Output (34 unique messages)
```
HW : RAP%sv%d.%d%d %cA
acrc.MCDCR: %08X
init IC2_%d controller done
XTI pipe cleaner
XTI enabled
SW : %s (%s %s)
Startup Reason: %08X
Set MCU/DSP speed: %d kHz/%d kHz
Heap reserved : %08X - %08X
Enable MMU
R&D : ROFS check disabled
SUSD loading failed                      ← NEW
Startup mode : %s
Copy images to SDRAM                     ← NEW
Image name   : %s                        ← NEW (×32 entries)
Reserved     : %d Bytes
Image size   : %d Bytes
Compressed   : %d Bytes
Nand address : 0x%08X
Load address : 0x%08X
Image: %s copying failed                 ← NEW
Decompres: SOS+DECOMP is not available   ← NEW
Authenticate PMM Loader                  ← NEW
Disable MMU                              ← NEW
Authenticates LDSP, toc = %08X, start = %08X  ← NEW
Start RapuYama VCore calibration         ← NEW
LDSP loaded by Nloader                   ← NEW
DSP code started                         ← NEW
No response from DSP                     ← NEW
sr_vcore_status_set(0x%x)                ← NEW
sr_vcore_status_set(): VCore status updated!   ← NEW
Genio authentication OK                  ← NEW
Genio data at offset : %04X              ← NEW
Address  : Value                         ← NEW
Beginning configuration build            ← NEW
hw_conf: SDRAM: %d MB at %08X            ← NEW
```

## All In-Memory Patches (Updated)

| # | Address | Patch | Purpose |
|---|---------|-------|---------|
| 1 | 0x10000000 | binary mirror | OneNAND driver vtable base |
| 2 | 0x10020000 | binary primary | Code execution base |
| 3 | 0x10000100 | movs r0,#100; bx lr | Return-100 stub |
| 4 | 0x10000110 | b.n . | Halt stub |
| 5 | 0x10000120 | char-by-char UART | Printf shim |
| 6 | 0x10000200 | 14×0x10000101 | HAL vtable |
| 7 | 0x10007040 | ptr→0x10000200 | Vtable pointer |
| 8 | 0x10007000 | 14×0x10000101 | HAL context (single indirection) |
| 9 | 0x10007714 | 0x401F0000 | UART base address |
| 10 | 0x10026204 | push{lr}; bl shim; pop{pc} | Printf wrapper |
| 11 | 0x10020748 | NOP | BSI check hang |
| 12 | 0x1002167a | b.n 0x1626 | Version mismatch bypass |
| 13 | 0x100325f2 | NOP | Startup reason hang |
| 14 | 0x10022bfe | NOP | Delay loop (4B iterations) |
| 15 | 0x1002f4fe | b.n 0xf4f2 | SOS+DECOMP hang → return |
| 16 | 0x100325f4 | NOP×2 | Flash init call |
| 17 | 0x100325fc | NOP×2 | Flash setup call |
| 18 | 0x10030bb0 | NOP×2 | Flash init (sub-function) |
| 19 | 0x10030bb4 | NOP×2 | Post-flash setup |
| 20 | 0x1002f330 | movs r0,#0; bx lr | Flash page-read stub |
| 21 | 0x100319fc | bx lr | Skip Build TOC function |
| 22 | 0x1002172e | NOP×2 | MMU page table setup |
| 23 | 0x10021732 | NOP×2 | MMU cache flush |
| 24 | 0x10021738 | NOP×2 | MMU SCTLR write |
| 25 | 0x10033b84 | 6×(mov r0,#0; bx lr) | Crypto trampolines |
| 26 | 0x30000000 | 64KB bx lr stubs | Secure ROM safety net |
| 27 | 0x30001be8 | mov r0,#0; bx lr | Secure ROM known target |
| 28 | 0x30001e06 | mov r0,#0; bx lr | Secure ROM known target |
| 29 | 0x30001d62 | mov r0,#0; bx lr | Secure ROM known target |
| 30 | 0x101ff000 | 32 TOC entries + end marker | Pre-populated SDRAM TOC |
| 31 | 0x30003e78 | ptr→0x101ff000 | TOC search base pointer |

## Key Findings

### TOC Entry Format (NLoader internal)
```
Offset  Size  Field
+0      4     NAND offset (raw flash address)
+4      4     Image size
+8      4     ??? (unused in search, populated by Build TOC)
+12     4     ??? (reserved in Copy images)
+16     4     Load address in SDRAM
+20     12    Name (null-terminated, end marker = 0xFF)
```
Total: 32 bytes per entry, max 64 entries (0x800 / 32)

### Flash TOC Format (OneNAND at 0x140000)
```
20-byte header (mostly zeros)
32-byte entries:
  +0   12  Name (null-terminated)
  +12   4  NAND offset
  +16   4  Image size
  +20  12  Flags/reserved
```

### Copy Images Flow
1. Iterates all TOC entries at 0x101ff000
2. For each: prints name/size/address, tries flash read (fails gracefully)
3. Some images are compressed → needs SOS+DECOMP decompressor (not loaded)
4. Special handling for "SOS" and "MSW" prefixed entries
5. After iteration: PMM loader auth, MMU disable, LDSP/DSP loading

### GENIO Processing Flow
1. "Authenticate PMM Loader" — auth check (crypto stubs → success)
2. "Disable MMU" — runs MMU disable wrapper
3. "Authenticates LDSP" — verifies LDSP partition (stubs → success)
4. "Start RapuYama VCore calibration" — DSP sends calibration commands
5. "No response from DSP" — DSP not running in QEMU (expected)
6. "sr_vcore_status_set" — VCore status updated regardless
7. "Genio authentication OK" — crypto auth passes (stubs return 0)
8. "Genio data at offset" — reads GENIO_INIT data from SDRAM
9. "Address : Value" headers — about to apply pad mux
10. "Beginning configuration build" — processing (addr, value) pairs
11. "hw_conf: SDRAM" — reports SDRAM config from GENIO data
12. DATA ABORT — tries to write garbage to hardware registers

### Why GENIO Fails
- GENIO_INIT is encrypted with TrustZone Secure World keys
- Our crypto stubs return 0 → auth "passes" (R&D flag may also help)
- But decryption doesn't happen → data is still encrypted
- NLoader parses encrypted data as (address, value) pairs
- Writes garbage values to garbage addresses → data abort

## Boot Call Chain (Updated)
```
Main (0x1259a):
  ├─ 0xfd44 — early init
  ├─ 0x1258e → 0x10390 + 0x2ba2
  ├─ 0x106f0 — hardware setup
  ├─ 0x621a — UART init
  ├─ printf("SW : ...")
  ├─ 0x3102 — timer init
  ├─ 0x2d42 — hw_detect
  ├─ 0x10796 — speed/heap setup
  ├─ printf("Startup Reason")
  ├─ R&D/ROFS check
  ├─ 0xf722 — flash init                    ← NOP'd
  ├─ 0x10398 — post_setup (speed config)
  ├─ 0x12558 — pre_genio
  │   ├─ 0x12534 — genio mode check
  │   ├─ 0x124c6 — sub-function
  │   │   ├─ 0x10b94 — flash sub-init       ← driver calls NOP'd
  │   │   ├─ 0x118f2 — SUSD check
  │   │   │   └─ 0x10c14 — SUSD search + read ← read stub returns 0
  │   │   ├─ 0x17b2 — speed setting
  │   │   ├─ 0x119fc — Build TOC             ← bx lr (skipped)
  │   │   └─ 0x11fee — Copy images to SDRAM
  │   │       ├─ iterate 32 TOC entries
  │   │       ├─ 0x10a88 → 0xf330 — read pages ← returns 0 (failure)
  │   │       ├─ 0x117f0 — PMM auth
  │   │       ├─ 0x11836 — LDSP/DSP loading
  │   │       └─ 0x1740 — Disable MMU
  │   └─ TOC search for GENIO_INIT
  ├─ 0x12498 — GENIO orchestrator
  │   ├─ auth/decrypt (crypto stubs → 0)
  │   ├─ "Genio authentication OK"
  │   ├─ parse (addr, value) pairs
  │   ├─ "hw_conf: SDRAM: ..."
  │   └─ apply config → DATA ABORT (encrypted data = garbage)
  └─ 0x12450 — cleanup (never reached)
```

## Next Steps

1. **Real HW UART capture** — #1 priority for getting actual pad mux values
   - NLoader prints "Address : Value" pairs at boot → one UART capture = all pad mux
   - This bypasses TrustZone entirely (NLoader decrypts then prints)
2. **Alternative GENIO source** — check if Nokia Cooker's FPSX decryption
   can decrypt GENIO_INIT (DES key is known: 416ce871c3bd0a28)
3. **Pad mux from N950/N9** — use as starting point, similar OMAP3630
4. **Linux regression test** — verify -kernel zImage still boots
