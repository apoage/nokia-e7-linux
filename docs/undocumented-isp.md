# OMAP3 ISP — Undocumented & Overlooked Features

> **Note (2026-03-23):** Real HW is BCM2727B1 (ARM1176), not OMAP3630. OMAP3 ISP does not exist on BCM2727. This document applies only to the synthetic QEMU emulation. See docs/critical-cpu-discovery.md

Target: OMAP 3630 ISP subsystem for computational photography on Nokia E7.
ISP base address: **0x480BC000**

---

## 1. Pipeline Overview

```
CSI-2 → CCDC → [Preview Engine] → [Resizer] → Memory
                                 ↘ H3A / Histogram
```

All submodules can operate standalone (memory-to-memory) — not just as camera pipeline.

---

## 2. Preview Engine — Programmable Filters

Base: ISP + 0x0600

### CFA Interpolation — 192 Custom Coefficients

```
ISPPREV_CFA_TBL_0 through ISPPREV_CFA_TBL_191
Offsets: 0x0644 — 0x0940
Format: 2x 13-bit signed coefficients (1.12 fixed point) per 32-bit register
```

The Linux driver loads standard Bayer demosaicing. The hardware accepts **arbitrary
4x4 FIR coefficients**. Load custom kernels for:
- Adaptive color plane interpolation
- Edge-directed demosaicing
- Combined demosaic + sharpen in single pass

### Noise Filter — Programmable LUT (Bilateral Approximation)

```
PREV_NF (0x0668): spread value + strength scaling
NF_TBL[0..7] (0x066C–0x0688): 8-entry weight curve
```

By programming custom NF_TBL values:
- Bilateral filter approximation (weights drop with intensity difference)
- Edge-preserving smoothing (sharp cutoff)
- Frequency-dependent NR

Exposed via `V4L2_CID_OMAP3ISP_PRV_NF_TABLE` but flexibility far exceeds defaults.

### Gamma LUTs — Per-Channel, 1024 Entries

```
PREV_REDGAMMA_TABLE:   0x06D4, 1024 entries (10-bit in → 10-bit out)
PREV_GREENGAMMA_TABLE: 0x0AD4
PREV_BLUEGAMMA_TABLE:  0x0ED4
```

Use for: per-channel tone mapping, inverse gamma for linear processing,
arbitrary transfer functions, film emulation curves.

### White Balance — Extended Range

`PREV_WB_DGAIN` (0x0628): Hardware range supports up to ~256x digital gain
(driver typically limits to 4x). Useful for extreme low-light compensation.

### Color Correction Matrix

`PREV_RGB_MAT` (0x064C–0x0664): 3x3 matrix, 12-bit signed S4.8 format.
Driver loads sRGB. Load custom for: Adobe RGB, ProPhoto RGB, false color, spectral analysis.

### Standalone Operation

```
PREV_PCR (0x0604):
  Bit 2 — SOURCE: 0=CCDC, 1=Memory (standalone)
  Bit 3 — ONESHOT: process one frame then stop
```

Set input: `PREV_RSDR_ADDR` (0x0630), output: `PREV_WSDR_ADDR` (0x0634).
**Hardware image processor for any buffer in memory.** Process multiple raw
frames with different parameters, blend results on DSP.

---

## 3. H3A Engine — Hardware Edge Detection

Base: ISP + 0x0C00

### AF Engine as Edge Detector

Two independent **11-coefficient IIR filters**. Compute per-paxel statistics
every frame at wire speed, zero CPU.

```
H3A_AFCOEF010–AFCOEF0110: IIR filter 0 (11 coefficients, 12-bit signed)
H3A_AFCOEF110–AFCOEF1110: IIR filter 1 (11 coefficients, 12-bit signed)

H3A_AFPAX1 (0x0C08): paxel size (2-256 pixels)
H3A_AFPAX2 (0x0C0C): paxel grid (up to 36×128)
```

**Load edge-detection IIR kernel**: `[0, 0, 0, 0, -1, 2, -1, 0, 0, 0, 0]`

Dense 8x8-pixel paxels across frame → real-time sharpness/edge map for:
- Focus peaking overlay
- Depth-from-focus
- Scene complexity analysis
- Adaptive processing decisions

AF output per paxel (32 bytes):
```c
struct isph3a_af_paxel {
    u32 iir0_h_sum, iir0_v_sum;  // filter 0
    u32 iir1_h_sum, iir1_v_sum;  // filter 1
    u32 pix_count;                // for normalization
    u32 reserved[3];
};
```

### AEWB Engine — Per-Region Color Analysis

Dense window grid → mean luminance, R/G B/G ratios per region.
Unsaturated pixel count → highlight/shadow detection.

---

## 4. Histogram Module — 4 Independent ROIs

Base: ISP + 0x0A00

### Multi-Region Configuration

```
HIST_R0_HORZ/VERT through HIST_R3_HORZ/VERT
Each: bits [13:0] = start, bits [29:16] = end
```

4 independent overlapping rectangles. **Per-region HDR bracket analysis:**
- Center-weighted vs surround metering
- Independent highlight/shadow detection
- Scene segmentation

### Multi-Channel + Custom Weights

```
HIST_CNT (0x0A08):
  Bits [5:3] — Channel: R, Gr, Gb, B, All(Y), R+G, G+B, Custom
  Bits [10:7] — Bins: 32, 64, 128, 256

HIST_WB_GAIN (0x0A0C): per-channel gain (U8Q5)
```

Zero-gain unwanted channels → pure single-channel histogram.
Custom weights → arbitrary luminance definition.

---

## 5. Resizer — Hardware Image Pyramid

Base: ISP + 0x0800

### Standalone Memory-to-Memory

```
RSZ_CNT (0x0808):
  Bit 0 — INPSRC: 0=preview, 1=memory (standalone)
  Bits [7:0] — vertical ratio (64=1:1, 128=2x down, 32=2x up)
  Bits [17:8] — horizontal ratio
```

### Custom 4-Tap Polyphase Coefficients

32 phases × 4 taps, 10-bit signed coefficients:
```
RSZ_HFILT10/32 (0x0828+): horizontal, 64 registers
RSZ_VFILT10/32 (0x0868+): vertical, 64 registers
```

Load: Lanczos-2, windowed sinc, Gaussian, nearest-neighbor, custom kernels.

### Hardware Pyramid Generation

Capture full-res → resizer 1/2x → resizer 1/2x → ... repeat.
Multi-resolution pyramid for Laplacian blending, focus stacking, exposure fusion.

---

## 6. CCDC — Advanced Raw Capture

Base: ISP + 0x0400

### A-Law Compression — 37% Bandwidth Savings

```
CCDC_ALAW (0x042C):
  Bit 0 — Enable A-law companding
  Bits [3:1] — Input width (10-13 bit)
```

Logarithmic 12→8 bit encoding. Preserves shadow detail better than linear
truncation. Critical for high-speed burst capture where memory bandwidth
is the bottleneck.

### Culling Patterns — Hardware Subsampling

```
CCDC_CULLING (0x044C):
  Bits [7:0] — Even-row horizontal pattern (8-bit bitmask)
  Bits [15:8] — Odd-row horizontal pattern
  Bits [23:16] — Vertical pattern
```

Each bit enables/disables pixel in 8-pixel repeating group.
Hardware 2x, 4x, 8x subsampling. Arbitrary patterns for compressed sensing.

### Fault Pixel Correction

```
CCDC_FPC_ADDR (0x0450): table address in memory
CCDC_FPC (0x0454): enable + count (up to 65535 entries)
```

Correction method bits [31:30]: 00=prev, 01=next, 10=avg, **11=2D median (undocumented on 3630)**.

### Black Level Subtraction

```
CCDC_DCSUB (0x043C): global pedestal subtraction
CCDC_BLKCMP (0x0440): per-Bayer-channel offset (4x 8-bit signed)
```

Hardware dark frame subtraction — driver initializes DCSUB to 0 but it accepts
arbitrary values.

### Video Port Cropping

```
CCDC_VP_OUT (0x0460):
  Bits [31:17] — HORZ_ST: horizontal start position
```

Hardware ROI extraction at the video port level.

---

## 7. CSI-2 Receiver

### 4 Virtual Channels Simultaneously

```
CSI2_CTX0-3_CTRL1: each maps to a virtual channel
```

Capture main image + metadata + thumbnail + PD AF data simultaneously.

### Embedded Data Extraction

Configure separate context with data type 0x12 (embedded non-image data).
Sensor register dumps, timestamps, statistics — per frame.

### D-PHY Timing Tuning

```
CSI2_DPHY_CTRL (0x0148):
  Bits [23:16] — THS_TERM
  Bits [15:8]  — THS_SETTLE
```

Reducing settle times can overclock the sensor interface.

---

## 8. ISP MMU — Zero-Copy Cross-Unit Sharing

32-entry TLB, supports 4KB/64KB/1MB/16MB pages. TLB locking available.

### ISP→DSP→GPU Zero-Copy Pipeline

```c
// Same physical pages mapped into all three MMUs
iommu_map(isp_domain, isp_va, phys, size, IOMMU_READ|IOMMU_WRITE);
iommu_map(dsp_domain, dsp_va, phys, size, IOMMU_READ|IOMMU_WRITE);
// GPU via PVR services PVRSRVMapDeviceMemory
```

Entire capture→process→display pipeline with zero memory copies.

---

## 9. Register Summary

| Module | ISP Offset | Absolute |
|--------|-----------|----------|
| ISP Core | 0x0000 | 0x480BC000 |
| CSI-2A | 0x0100 | 0x480BC100 |
| CCDC | 0x0400 | 0x480BC400 |
| Preview | 0x0600 | 0x480BC600 |
| Resizer | 0x0800 | 0x480BC800 |
| Histogram | 0x0A00 | 0x480BCA00 |
| H3A | 0x0C00 | 0x480BCC00 |
| ISP MMU | — | 0x480BD000 |

---

## 10. Computational Photography Recipes

### HDR Capture
1. H3A AEWB analyzes scene → 4 regions
2. Histogram determines bracket exposures
3. Capture 3 frames with different sensor exposure (I2C during VBLANK)
4. Preview Engine standalone → process each with different gamma LUT
5. Blend on DSP via shared DMABUF — zero copy

### Focus Peaking
1. H3A AF with dense 8x8 paxels, edge-detection IIR kernel
2. Read statistics every frame (DMA automatic)
3. Overlay via DSS graphics plane with alpha

### Multi-Resolution Pyramid
1. CCDC → full-res raw to memory
2. Preview Engine standalone → processed frame
3. Resizer standalone 1/2x → level 1
4. Resizer standalone 1/2x → level 2
5. Use for Laplacian blend / focus stack / exposure fusion

---

*Derived from TI OMAP3630 TRM (SPRUF98), Linux omap3isp driver source, TI E2E forums*
