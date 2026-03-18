# 2026-03-07 — NLoader QEMU Boot (Task 067)

## Summary
Got NLoader (patched, 188KB) executing in QEMU bare-metal mode. Captured
first UART output: **"BSI out of range"**. hw_detect function runs to
completion. GENIO_INIT decryption confirmed impossible without TrustZone keys.

## What Was Built

### QEMU Changes (omap3630.c + nokia_e7.c)

1. **NLoader boot mode** in nokia_e7.c:
   - Detects `-drive ...,id=nloader` without `-kernel` → bare-metal boot
   - Loads binary at 0x10020000, sets Thumb entry at 0x100207c0 (hw_detect)
   - HAL vtable stubs at 0x10000100/0x10000200 (return 100)
   - VFP/NEON enabled (CPACR + FPEXC + hflags rebuild)
   - Printf shim: replaces wrapper at 0x6204 with direct string output
   - BSI hang patch: NOP at 0x10020748 (b.n . → nop)
   - LR = 0x10000111 (halt stub), SP = 0x4020FFF0 (SRAM top)

2. **NLoader UART shim** at 0x401F0000:
   - Offset 0x08 reads: always returns 0x10 (TX ready)
   - Offset 0x1C writes: accumulates chars, info_report() on newline/null
   - NLoader's UART driver struct at 0x10007714 pointed here

3. **Printf ring buffer RAM** at 0x40100000 (960KB):
   - NLoader's ARM printf at 0x1e90 writes to base + (bufID << 12)
   - Base = 0x40100000 (from literal pool at code offset 0x2074)
   - Buffer ID 0xEF → address 0x401EF000
   - Must NOT overlap UART shim at 0x401F0000

4. **SCM register-backed model**:
   - Stores all writes, logs PADCONF writes (0x030-0x264)
   - CONTROL_STATUS returns GP device type

5. **No Linux boot regression** — shell prompt still reached

## Execution Trace

```
0x100207c0  hw_detect entry (push {r3-r7,lr})
0x1002154e  bus_read_wrapper
0x10021152  bus_read (control struct at literal pool)
0x10021100  bus_poll (polls control struct, exits immediately — zeros)
0x10026204  printf("BSI out of range") via patched wrapper
0x10000120  printf shim → writes chars to 0x401F001C
0x10020748  BSI hang (patched to NOP, falls through)
0x1002075e  error handler returns 0
0x10020818+ more bus reads (all return 0, r4 accumulates 0)
0x1002089c  hw_detect returns (pop {r3-r7,pc})
0x10000110  halt stub (b.n .)
```

Total: ~36 unique translation blocks, ~150 trace entries.

## Key Finding: GENIO_INIT Crypto is Blocked

GENIO_INIT at NAND offset 0x01200000 (105,692 bytes) has entropy 7.99/8.0
— fully encrypted. Attempted decryption with known FPSX DES key
(`416ce871c3bd0a28`) in both CBC and ECB modes: output still random,
no OMAP3 peripheral addresses (0x48xxxxxx) found.

**Why QEMU can't decrypt it:**
- Auth function chain: 0x105b8 → 0x10762 → `blx 0x13b94` (HAL crypto)
- HAL crypto (0x13b94) is a vtable function pointer, stubbed to return 100
- Real implementation uses SMC #0 (TrustZone Secure World)
- Confirmed in Task 045: "Software-only RE dead end"
- The auth BYPASS (patched at 0x1072c: beq→b.n) skips the CHECK but
  doesn't perform the DECRYPTION — data remains encrypted

**What WOULD work:**
- UART capture on real hardware (NLoader prints every pad mux write)
- ATF box with JTAG access to Secure World
- CCP2 bus capture during BCM2727 boot (separate topic)

## NLoader Binary Layout Confirmed

| Region | File Offset | Memory Address | Content |
|--------|------------|----------------|---------|
| BB5 header | 0x000-0x3CF | 0x10020000 | NALO at 0x158 |
| Strings | 0x350-0x7FF | 0x10020350 | Format strings |
| Thumb code | 0x800-0x2D020 | 0x10020800 | Main code |
| ARM code | 0x1e90-0x2074 | 0x10021e90 | printf (uses VFP) |
| Data area | — | 0x10000000 | BSS/globals (512KB) |

Key data structures in data area:
- 0x10007040: HAL vtable pointer → 0x10000200 (our stub vtable)
- 0x10007714: UART base pointer → 0x401F0000 (our shim)
- 0x100074c4+3: R&D flag byte

## Printf Architecture

```
printf wrapper (0x6204, Thumb)
  → ARM printf (0x1e90, uses VFP vldr/vstr)
    → string formatter (0x1dfc, Thumb) — writes to ring buffer
      → buffer at 0x40100000 + (bufID << 12)
      → bufID 0xEF → 0x401EF000
  → [buffer never flushed — UART TX (0x2300) not called by hw_detect]
```

Our printf shim bypasses the entire chain: replaces 0x6204 with a
direct character-by-character output to the UART shim at 0x401F001C.
Loses printf formatting (%d/%x appear as literals) but captures which
code paths execute and what strings are printed.

## Bus Read Architecture

Function 0x1152 uses a hardware control structure (address from literal
pool at 0x119c). Sets transaction parameters (bus/register address),
calls 0x1100 to poll for completion, returns value from struct offset 0x20.
All data area is zeros → polls exit immediately, reads return 0.

BSI register address: 0x00220800 (passed via 0x154e → 0x1152).
BSI value 0 → "BSI out of range" → hang at 0x748 (now patched to NOP).

## Task 067 Status

**Partial success.** NLoader executes in QEMU, hw_detect runs to completion,
UART output is captured. But the primary goal (GENIO_INIT pad mux recovery)
is blocked by BB5 encryption that requires TrustZone keys.

Next options:
1. Run more NLoader code paths (find main boot function, patch more hangs)
   — diminishing returns without crypto
2. UART capture on real Unit A — the definitive solution
3. Use N900/N950 pad mux as starting point, adjust per schematic
