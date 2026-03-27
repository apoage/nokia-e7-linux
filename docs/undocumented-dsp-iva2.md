# TMS320C64x+ DSP (IVA2.2) — Undocumented & Overlooked Features

> **Note (2026-03-23):** Real HW is BCM2727B1 (ARM1176), not OMAP3630. IVA2.2 is OMAP3630-specific and does not exist on BCM2727. This document applies only to the synthetic QEMU emulation. See docs/critical-cpu-discovery.md

Target: OMAP 3630 IVA2.2 subsystem for computational photography on Nokia E7.

---

## 1. The iMX Accelerator — Hidden Image Coprocessor

The IVA2.2 contains more than just the C64x+ core. The **iMX (Imaging Extension)**
is a hardware accelerator that runs **concurrently with the DSP**:

- Hardware SAD (Sum of Absolute Differences) — block matching
- DCT/IDCT (8x8) — frequency-domain processing
- Pixel interpolation (half-pel, quarter-pel)
- Loop deblocking filter

Base address from DSP: **0x01CC0000** (approximate)

```c
// iMX SAD for image alignment (HDR stacking, panorama)
*(volatile unsigned int *)IMX_SRC_ADDR = ref_block_addr;
*(volatile unsigned int *)IMX_DST_ADDR = search_window_addr;
*(volatile unsigned int *)IMX_CONFIG =
    (block_width << 0) | (block_height << 8) |
    (search_range << 16) | (0x01 << 24);  // SAD mode
*(volatile unsigned int *)IMX_CMD = 0x01;  // Start

// Read result
unsigned int sad_min = *(volatile unsigned int *)(IMX_BASE + 0x20);
int mv_x = (short)(*(volatile unsigned int *)(IMX_BASE + 0x24));
int mv_y = (short)(*(volatile unsigned int *)(IMX_BASE + 0x28));
```

**Repurposing for computational photography:**
- Image alignment for HDR, panorama, super-resolution
- Noise estimation (SAD between adjacent blocks)
- Edge detection proxy (high SAD = edge)
- DCT-domain noise reduction (transform, threshold, inverse)

**iMX documentation is under NDA** — extract register definitions from TI codec
source packages (H.264 encoder) or gst-ti plugin / tidspbridge kernel driver.

---

## 2. Undocumented Instructions

### DMPY2 — Double Packed Multiply
```c
long long result = _dmpy2(src1, src2);
// Two 16x16→32 multiplies simultaneously
```

### DMPYU4 / DOTPU4 — 4x8-bit Multiply-Accumulate
```c
int result = _dotpu4(pixel_pack_a, pixel_pack_b);
// Four 8x8 unsigned multiplies, summed
```

### GMPY — Galois Field Multiply
```c
GFPGFR = 0x001D; // CRC-8 polynomial
result = _gmpy4(input_byte, poly);
```
Hardware CRC/PRNG for tile cache coherency verification and dithering.

### Compact Instructions (16-bit)
Header-based fetch packets can mix 16/32-bit instructions. A 3x3 convolution
kernel can shrink from 14 to 9 fetch packets — doubles effective I-cache capacity.

```bash
cl6x --generate_16bit_instructions  # enable in compiler
```

**Caveat**: Do NOT use compact instructions inside SPLOOP bodies — unpredictable
behavior on some C64x+ silicon revisions.

---

## 3. SPLOOP Advanced Tricks

### ILC/RILC Auto-Reload
```c
ILC = width;      // inner loop count
RILC = width;     // auto-reload value
// Outer loop just branches back — ILC reloads automatically
```
Eliminates per-row loop count overhead in image processing.

### SPLOOPW for Early Exit
```asm
SPLOOPW 2
LDW     *A4++, A0
NOP     4
CMPGTU  A0, A1, A2
[A2] SPKERNELR     ; early terminate on threshold match
[!A2] SPKERNEL
```

### SPLOOP Buffer: 14 × 8 = 112 Instructions Max
14 execute packets of 8 instructions each. Use only 32-bit instructions inside.

### IERR Register (0x1A) — Silent Error Detection
```c
unsigned int ierr = _MVC_getter(IERR);
if (ierr & 0x40) // SLXE — SPLOOP exception, results invalid
if (ierr & 0x80) // MEXE — missed execution slot
```

---

## 4. EDMA3 — The DMA Engine

### Bayer Pattern Deinterleave in Hardware (Zero DSP Cycles)

```c
// Extract R channel from RGGB Bayer
param_r->A_B_CNT = 1 | ((width/2) << 16);
param_r->SRC_DST_BIDX = (1 << 16) | 2;         // skip every other pixel
param_r->SRC_DST_CIDX = ((width/2) << 16) | (2 * stride);
param_r->CCNT = height / 2;
```

### Autonomous Double-Buffer Ping-Pong via Chaining

```c
// Ch0: load→bufA, chain→ch2
param0->OPT = (2 << 12) | (1 << 22);
// Ch2: store bufA, chain→ch1
param2->OPT = (1 << 12) | (1 << 22);
// Ch1: load→bufB, chain→ch3
param1->OPT = (3 << 12) | (1 << 22);
// Ch3: store bufB, chain→ch0
param3->OPT = (0 << 12) | (1 << 22);
// Kick: manual trigger ch0
*(volatile unsigned int *)EDMA3_ESR = (1 << 0);
```
Four-phase pipeline with ZERO DSP intervention.

### Image Transposition via 2D-to-2D Transfer
```c
param->A_B_CNT = (1) | (width << 16);
param->SRC_DST_BIDX = (height << 16) | stride;  // read columns, write rows
param->CCNT = height;
```

### Undocumented Event Triggers
- Event 12: L1D writeback complete → auto-trigger DMA after cache flush
- Event 13: iMX output ready → chain iMX result to DMA automatically
- Event 14: iLBCd block complete

---

## 5. Memory Architecture

### L1D Bank Conflicts — The #1 Performance Killer

8 banks, 4 bytes wide, interleaved on 4-byte boundaries. Two accesses to
same bank in same cycle = 1-cycle stall.

```c
// BAD: same alignment = guaranteed conflict
short img_a[320] __attribute__((aligned(32)));  // bank 0
short img_b[320] __attribute__((aligned(32)));  // bank 0!

// FIX: offset by one bank
#pragma DATA_BANK_OFFSET(img_a, 0)
#pragma DATA_BANK_OFFSET(img_b, 4)
```

**Critical**: If row width in bytes is a multiple of 32, ALL rows alias to
same bank. Pad: `#define PADDED_STRIDE(w) (((w) + 4) & ~3)`

### L2 as Scratchpad + Cache

```c
// Configure: 32KB cache + rest as SRAM
*(volatile unsigned int *)0x01840000 = 0x00000001;
```
Optimal for image processing: SRAM for DMA double-buffers, cache for code/LUTs.

### Zero-Coherency Shared Memory via L2 SRAM

L2 SRAM is not cached. Map shared ARM↔DSP buffers there:
- DSP address: `0x00800000`
- ARM address: `0x5C800000`
No cache flush/invalidate needed. Eliminates all coherency overhead.

### MAR Registers — Per-Region Cache Control
```c
// Mark frame buffer region as write-through (DMA can read without writeback)
*(volatile unsigned int *)(0x01848000 + (ext_addr >> 24)*4) = 0x03; // PC=1, WTE=1
```

---

## 6. IVA2.2 Power & Clock Control

### OPP Table (from ARM side)

| OPP | Frequency | Voltage |
|-----|-----------|---------|
| OPP50 | 260 MHz | 1012.5 mV |
| OPP100 | 520 MHz | 1200 mV |
| OPP-Turbo | 660 MHz | 1325 mV (disabled by default) |
| OPP-SB | 800 MHz | 1375 mV (disabled by default) |

### DPLL2 Overclocking
```c
// DPLL2: freq = SYS_CLK * M / (N+1) / M2
// SYS_CLK = 26 MHz, default 520 MHz: M=260, N=12, M2=1
// Target 600 MHz: M=300, N=12, M2=1
// REQUIRES voltage increase and ABB Forward Body Bias
```

### No-Idle for Sustained Throughput
```c
*(volatile unsigned int *)0x01C00010 = 0x08; // no-idle mode
```
Prevents clock-gating during brief idle periods between DMA transfers.

---

## 7. Compiler Flags for Maximum Performance

```bash
cl6x -mv64plus \
     --opt_level=3 \
     --opt_for_speed=5 \
     --generate_16bit_instructions \
     --speculate_loads=4 \
     --mem_model:data=far \
     --advice:performance=all \
     -mw  # pipeline info
```

`--speculate_loads=4` — issues loads up to 4 iterations ahead, hides memory latency.

---

## 8. Performance Targets at 520 MHz

| Operation | Peak | With DMA Pipeline |
|-----------|------|--------------------|
| 8-bit pixel ops (packed) | 4160 Mpix/s | ~2000 Mpix/s |
| DOTPU4 | 2080 Mop/s | ~1000 Mop/s |
| 3x3 convolution (8-bit) | ~230 Mpix/s | ~150 Mpix/s |
| 16-bit multiply | 1040 Mmul/s | ~600 Mmul/s |
| EDMA3 throughput | ~2 GB/s | ~1.5 GB/s |

---

## References

- SPRU732 — C64x/C64x+ ISA Reference
- SPRU871 — C64x+ Megamodule Reference (cache, EDMA3)
- SPRUGN4 — OMAP3630 TRM (IVA2.2 subsystem)
- SPRU986 — C64x+ DSPLIB (optimized kernel source)
- SPRAAK2 — Hand-Tuning Loops on C6000
- TI E2E forums (threads from TI engineers on iMX/EDMA3)
