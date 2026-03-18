# ARM Cortex-A8 + NEON — Undocumented & Overlooked Features

Target: OMAP 3630 Cortex-A8 as found in Nokia E7. Focused on computational
photography optimization.

---

## 1. Microarchitectural Exploits

### 1.1 The 2-Cycle NEON Writeback Hazard

**The single most important Cortex-A8 NEON optimization.**

NEON results written back 2 cycles after instruction completes. Reading
the result within those 2 cycles causes a stall.

```asm
; BAD — 2-cycle stall
    VMUL.I16  q0, q1, q2       ; produces q0
    VADD.I16  q3, q0, q4       ; reads q0 — STALL

; GOOD — interleave independent work
    VMUL.I16  q0, q1, q2       ; produces q0
    VMUL.I16  q5, q6, q7       ; independent — fills bubble
    VADD.I16  q3, q0, q4       ; q0 ready — no stall
    VADD.I16  q8, q5, q9       ; q5 ready — no stall
```

**Rule**: Always process 2+ pixel groups in alternation. 25-40% throughput gain.

### 1.2 Dual-Issue: ARM + NEON for Free

Cortex-A8 can issue one ARM + one NEON instruction per cycle if ARM comes first.
NEON-NEON never dual-issues.

```asm
; Interleaved — ARM instructions execute FREE in NEON shadow
    VLD1.8    {d0}, [r0]!          ; NEON: load
    ADD       r2, r0, r1           ; ARM: next row addr (FREE)
    VLD1.8    {d2}, [r2]           ; NEON: load row 1
    SUB       r3, r3, #1           ; ARM: decrement (FREE)
    VMULL.U8  q1, d0, d4           ; NEON: process
    PLD       [r0, #64]            ; ARM: prefetch (FREE)
    VMULL.U8  q2, d2, d4           ; NEON: process
    CMP       r3, #0               ; ARM: check (FREE)
```

### 1.3 ARM→NEON Transfer Penalty

`VMOV Rd, Sn` (NEON→ARM) has **20-cycle penalty** (pipeline drain).
`VMOV Sn, Rd` (ARM→NEON) has ~4-cycle penalty.
Avoid transfers in inner loops. Use NEON for reductions, extract at the end.

### 1.4 PLD Optimal Distances

L1 fill from L2: ~11 cycles. L2 fill from DDR: ~100+ cycles.
PLD buffer: 4 outstanding prefetches.

```asm
; For image row-stride access:
    PLD  [r0, #64]          ; 1 cache line ahead (L2 hits)
    PLD  [r0, #384]         ; 6 lines ahead (DDR misses)
    PLD  [r0, r1]           ; next row, same column
```

**PLD costs 0 cycles when dual-issued with NEON.** Always pair them.

**DRAM page alignment**: LPDDR1 has 1KB pages. Align image row stride to 1024 bytes:
```c
size_t aligned_stride = (width * bpp + 1023) & ~1023;
```

---

## 2. NEON Tricks

### 2.1 VQMOVUN — Free Clamping

```asm
; Clamp signed 16-bit to [0,255] AND narrow to 8-bit in ONE instruction
    VQMOVUN.S16  d0, q0    ; negative→0, >255→255, 16→8 bit
```
Saves 2 instructions per narrowing operation. In a 3x3 convolution, that's
16 saved instructions per iteration.

### 2.2 VTBL/VTBX — Universal Byte Permutation

```asm
; RGBA → BGRA channel swizzle in one instruction
    VMOV.U8  d2, #2,#1,#0,#3,#6,#5,#4,#7
    VTBL.8   d1, {d0}, d2

; VTBX: out-of-range indices preserve destination = FREE conditional move
    VMOV     d1, d0              ; copy original
    VTBX.8   d1, {d16}, d2      ; replace only where d2[i] < table_len
```

4-bit LUT via VTBL for gamma approximation:
```asm
    VSHR.U8  q2, q0, #4          ; high nibbles
    VAND.U8  q3, q0, q15         ; low nibbles (q15 = 0x0F)
    VTBL.8   d4, {d16}, d4       ; LUT high
    VTBL.8   d6, {d17}, d6       ; LUT low
    VADD.U8  q0, q2, q3          ; combine
```

### 2.3 VMULL.P8 — Polynomial Multiply for Fast Hashing

Carryless (GF(2)) multiplication. ~1 cycle per 8 bytes for tile hashing/CRC:

```asm
    VMULL.P8  q0, d0, d1     ; 8x8→16 byte hash
    VMULL.P8  q1, d0, d2     ; chain with previous state
    VEOR      q0, q0, q1     ; XOR-combine
```

CRC-32 via Barrett reduction: ~2 cycles/byte vs ~8 cycles/byte scalar.

### 2.4 VRECPE + VRECPS — Fast Approximate Division

```asm
; 4 parallel divisions in ~8 cycles (vs ~20+ per scalar VDIV)
    VRECPE.F32   q1, q0           ; initial estimate ~8 bits
    VRECPS.F32   q2, q0, q1       ; Newton-Raphson step
    VMUL.F32     q1, q1, q2       ; refined ~16 bits
    VMUL.F32     q0, q3, q1       ; result = b/a
```

### 2.5 VPADAL — Progressive Accumulate

```asm
; SAD (Sum of Absolute Differences) — accumulates without separate VADD
    VABDL.U8   q0, d0, d1        ; |a-b| widened to 16-bit
    VPADAL.U16  q2, q0            ; pairwise add-accumulate into 32-bit
```

---

## 3. CP15 Registers

### 3.1 PMU Event Counters

Cycle counter + 4 configurable event counters. Critical events for image processing:

| Event | Code | Why It Matters |
|-------|------|----------------|
| L1 D-cache refill | 0x03 | Cache misses |
| L1 D-cache access | 0x04 | Miss rate calculation |
| NEON MRC stall | 0x68 | ARM↔NEON transfer cost |
| NEON full stall | 0x70 | Pipeline hazards |

Enable user-mode PMU access (from kernel):
```c
asm volatile("MCR p15, 0, %0, c9, c14, 0" :: "r"(1));
```

### 3.2 Cache Way Lockdown

Lock convolution coefficients into L1D:
```c
uint32_t lockdown = 0xE; // lock ways 1-3
asm volatile("MCR p15, 0, %0, c9, c0, 0" :: "r"(lockdown));
// load coefficients (go into way 0)
lockdown = 0x1; // lock way 0
asm volatile("MCR p15, 0, %0, c9, c0, 0" :: "r"(lockdown));
```

### 3.3 ACTLR — Auxiliary Control Register

```
MRC p15, 0, Rd, c1, c0, 1    ; Read ACTLR
```

| Bit | Name | Check |
|-----|------|-------|
| 9 | PLDNOP | Must be 0 (PLD active) |
| 11 | DWBST | NEON double-width store — MUST be enabled |
| 28 | L1NEON | NEON L1 cacheability — may be off by default |

### 3.4 On-Chip SRAM (No TCM on OMAP3630)

TCM not wired on OMAP3630. Alternative: **64KB SRAM at 0x40200000**
```c
void *sram = mmap(NULL, 0x10000, PROT_READ|PROT_WRITE,
                  MAP_SHARED, fd_devmem, 0x40200000);
```
~3 cycle access from CPU after L1 miss. Use as scratchpad for intermediates.

---

## 4. Exploitable Errata

### Erratum 725233: VLD Structure Alignment
Forces aligned accesses, which are actually faster. Use `VLD2.8 {d0,d1}, [r0:128]`
— completes in 1 cycle vs 2 unaligned.

### Erratum 714622: VMLAL Extra Precision
Back-to-back VMLAL may retain extra internal precision bits. Free improved
accuracy in convolution accumulation chains.

### Erratum 742230: Barrier Relaxation
For single-core ARM↔NEON ordering, ISB is sufficient instead of DSB/DMB.
Saves ~8-10 cycles per barrier.

---

## 5. Optimization Priority (Ranked by Impact)

1. **Interleave 2 pixel groups** — hide writeback hazard — 25-40%
2. **Dual-issue ARM+NEON** — PLD/address math free — 15-30%
3. **VQMOVUN free clamping** — 10-15% in convolution kernels
4. **PLD optimal distances** — 20-50% on large images
5. **VMULL.P8 tile hashing** — 4-8x faster than scalar CRC
6. **Cache way lockdown** — eliminates coefficient misses
7. **VTBX conditional select** — eliminates branches in LUT ops
8. **ACTLR bit 11 check** — ensure NEON wide stores enabled

---

*Derived from ARM Cortex-A8 TRM, OMAP3630 TRM, errata sheets, community research*
