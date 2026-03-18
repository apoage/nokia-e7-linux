# OMAP 3630 System-Level — Undocumented & Overlooked Features

Target: system-wide optimization for Nokia E7 Linux platform.

---

## 1. SDMA (System DMA) — 32 Channels

### Constant Fill Mode (Hardware memset)

`CCR bit 16` (`CONSTANT_FILL`): DMA sources from `DMA4_COLOR` register.

```c
// Zero-fill 1MB buffer — zero CPU cycles after setup
writel(fill_value, DMA4_COLOR(ch));
// CCR: constant fill, dst post-increment, burst 64
```

### Transparent Copy (Hardware Chroma-Key)

`CCR bit 17` (`TRANSPARENT_COPY`): skips write if element matches `DMA4_COLOR`.
Hardware sprite blitting with transparency.

### Endianness Conversion During Transfer

`CSDP` bits 18-21: source/dest endianness + lock bits.
Free byte-swap during DMA — useful for big-endian image format headers.

### 2D Block Transfers

Double-indexed addressing (`CCR_*_AMODE = 3`):
- CSEI/CSFI: source element/frame index (stride)
- CDEI/CDFI: dest element/frame index

Extract rectangular ROI from larger framebuffer in single DMA op.

---

## 2. L3 Interconnect Priority

Base: **0x68000000** (SMX_APE)

### Camera Pipeline Priority Boost

```c
// Boost camera ISP priority over other L3 traffic
// Camera agent at 0x68005820
writel(priority_value, 0x68005820);
```

| Initiator | Agent Offset |
|-----------|-------------|
| MPU | 0x1400 |
| IVA2.2 | 0x1800 |
| Camera ISP | 0x5800 |

### SDRC Memory Priority

Separate priority layer at the SDRAM controller. `MREQPRIO` registers at
`0x6D000000` control memory request priority independently of L3.

---

## 3. Display Subsystem (DSS/DISPC)

### Free YUV→RGB Conversion

VID overlays do hardware color space conversion during scanout.
Write YUV directly — save CPU from conversion entirely.

Configurable via `DISPC_OVL_CONV_COEF` (BT.601, BT.709, custom coefficients).

### Free 5-Tap Scaling

VID overlays include programmable 5-tap polyphase FIR for H+V scaling.
Scale video during display — not in software.

### Free Alpha Compositing

- Per-overlay global alpha (8-bit)
- Per-pixel alpha (ARGB8888/4444/1555)
- Pre-multiplied alpha
- Z-ordering
- Source/destination color keying

Put video on VID1, UI on GFX with alpha. Zero CPU compositing.

### Limitation: No Writeback

OMAP3 DSS cannot render to memory (no writeback pipeline unlike OMAP4/5).
Output goes to display encoder only. Use SDMA for 2D blitting.

---

## 4. SmartReflex & Overclocking

### OPP Table — VDD1 (MPU)

| OPP | Frequency | Voltage | Default |
|-----|-----------|---------|---------|
| OPP50 | 300 MHz | 1012.5 mV | Enabled |
| OPP100 | 600 MHz | 1200 mV | Enabled |
| OPP-Turbo | 800 MHz | 1325 mV | **Disabled** |
| OPP-SB | 1000 MHz | 1375 mV | **Disabled** |

### Adaptive Body Bias (ABB) — MANDATORY for High Clocks

The OMAP3630 has an on-chip ABB LDO. **Forward Body Bias required for
reliable operation above 800 MHz (ARM) / 660 MHz (DSP).**

Sequence:
- **Upscale**: raise voltage → set ABB to FBB → raise clock
- **Downscale**: lower clock → set ABB to bypass → lower voltage

Without ABB, OPP-Turbo and OPP-SB **will be unreliable** even at spec voltage.

### Speed Bin eFuse

`CONTROL_OMAP_STATUS` — factory-tested maximum safe frequency.
Read before enabling high OPPs.

### SmartReflex Class 1.5 (Preferred on 3630)

Detects and stores per-OPP calibrated voltage on first transition, replays
stored value on subsequent transitions. Less I2C traffic than Class 3.

### Community OC Results (N9/N950/BeagleBoard-xM)

- 1.2 GHz: "guaranteed stable" on most silicon
- 1.3 GHz: achievable but risky long-term
- Disable SmartReflex above 1 GHz (causes reboot loops)

---

## 5. Secure ROM Services

On GP (General Purpose) devices, 3 ROM services via SMC:

| ID | Constant | Purpose |
|----|----------|---------|
| 1 | `OMAP3_GP_ROMCODE_API_L2_INVAL` | Invalidate L2 cache |
| 2 | `OMAP3_GP_ROMCODE_API_WRITE_L2ACR` | Write L2 Auxiliary Control Register |
| 3 | `OMAP3_GP_ROMCODE_API_WRITE_ACR` | Write ARM Auxiliary Control Register |

**Only way to configure L2 cache and speculative access on GP devices.**
Crypto (AES/SHA) in secure ROM is NOT accessible on GP devices.

---

## 6. eFuse / Device Identification

### Registers at 0x4830A200

| Register | Address | Content |
|----------|---------|---------|
| CONTROL_ID_CODE | 0x4830A204 | Silicon revision, part number |
| DIE_ID_0-3 | 0x4830A218+ | 128-bit unique die ID |

CONTROL_ID_CODE: bits [27:12] = part number (OMAP3630 vs 3430 vs DM3730).

### SmartReflex eFuse (NVALUE)

`CONTROL_FUSE_OPPn_VDDx` at `0x48002270+` — factory-programmed optimal voltage
sensor setpoints. Blank on engineering samples.

---

## 7. ARM↔DSP Shared Memory

### No Hardware Coherency

**There is no snoop control unit between ARM and C64x+ DSP.**

Options:
1. **Non-cacheable on ARM side** (`ioremap_wc()`) — simplest
2. **Explicit cache maintenance** — flush/invalidate before/after exchange
3. **L2 SRAM** (DSP: `0x00800000`, ARM: `0x5C800000`) — not cached, zero overhead

Option 3 is best for small control/mailbox buffers.
Option 1 for large image buffers.

---

## 8. OMAP3630 vs OMAP3430 Key Differences

| Feature | 3430 (65nm) | 3630 (45nm) |
|---------|-------------|-------------|
| ARM clock | 600 MHz max | 720 MHz stock, **1 GHz OPP-SB** |
| SGX530 | ~110 MHz | **200 MHz** (~80% faster) |
| IVA2 DSP | 430 MHz | **520 MHz** stock, **800 MHz OPP-SB** |
| LPDDR | 166 MHz | **200 MHz** (20% more bandwidth) |
| L3 interconnect | 166 MHz | **200 MHz** |
| SmartReflex | V1 | **V2** (improved) |
| ABB LDO | None | **Yes** (enables high OPPs) |

Same peripheral set (GPIO, McBSP, I2C, SPI, UART, MMC).

---

## 9. Temperature Monitoring

```c
// On-die sensor at 0x4800232C
unsigned int temp_raw = readl(0x4800232C);
// temp_C ≈ (raw - 0x1B8) * 0.45 + 25
```

Use for thermal throttling decisions during sustained DSP + camera processing.

---

## 10. Performance Monitoring

### Cortex-A8 PMU
- CCNT (cycle counter) + 4 event counters
- Enable user-mode access: set PMUSERENR bit 0
- Events: L1/L2 misses, branch mispredictions, TLB misses, stall cycles

### 32K Sync Timer
- `0x48320000` + offset 0x10
- 32.768 kHz, survives sleep states
- ~30.5 us resolution, cheaper than ktime_get()

### GPMC Prefetch Engine
- Hardware prefetch/write-posting for NAND/external memory
- FIFO threshold DMA triggering
- Can interface FPGAs/external SRAM

---

*Derived from OMAP3630 TRM, Linux kernel sources (omap-dma, omapdrm, SmartReflex),
TI E2E forums, community overclocking results*
