# Nokia E7 Linux Revival — Hardware Lore & Project Root

> Two Nokia E7-00 units. Goal: snappy Linux handheld terminal with full
> networking stack and computational photography exploiting every ASIC on chip.

---

## 1. Device Specs — Nokia E7-00 (RM-626)

| Component        | Detail                                         |
|------------------|-------------------------------------------------|
| SoC              | TI OMAP 3630                                    |
| CPU              | ARM Cortex-A8, 680 MHz (OC to ~1 GHz possible)  |
| DSP              | TMS320C64x+ (IVA2.2), ~520 MHz                  |
| GPU              | PowerVR SGX530, ~200 MHz                         |
| ISP              | OMAP3 ISP (CCDC, preview, resizer)              |
| RAM              | 256 MB mobile DDR (PoP)                          |
| Storage          | 16 GB eMMC internal                              |
| Display          | 4.0" AMOLED 640x360, capacitive touch            |
| Camera           | 8 MP AF, EDoF secondary                          |
| Camera sensor    | Toshiba TCM8590MD (main 8MP) — confirmed from VLL |
| Front camera     | ACMEmini sensor (front EDoF) — confirmed from VLL |
| Video co-proc    | Broadcom BCM2727 VideoCore III — 32MB stacked SDRAM |
| WiFi             | TI WL1273 (wl12xx) — 802.11 b/g/n               |
| Bluetooth        | TI WL1273 — BT 3.0 + HS                          |
| Cellular         | Rapuyama CMT modem (confirmed from NLoader strings) |
| GPS              | TI GPS (NaviLink / WL1273 combo chip)             |
| NFC              | NXP PN544                                         |
| HDMI             | mini-HDMI out, 720p                               |
| USB              | micro-USB, OTG                                    |
| Keyboard         | Slide-out 4-row QWERTY, backlit                   |
| Battery          | BP-4L, 1200 mAh                                   |
| Sensors          | ST LIS3xx accel, AKM AK8974 magnetometer, prox, ALS |
| Touch controller | Atmel mXT capacitive touch (I2C) |
| Charger ICs      | TI BQ2415x + Summit SMB338/SMB138 (I2C) |
| PMIC             | TI TPS65950 (TWL4030 family) on I2C1 |
| Audio            | Stereo speakers, 3.5mm, FM TX/RX                  |
| Original FW      | Symbian^3 → Anna → Belle                          |

### Notes
- Two physical units available — one for dev, one stays stock for reference
- Camera sensor confirmed **Toshiba TCM8590MD** from BCM2727 VLL driver files
- Modem is **Rapuyama** (Nokia's 3G CMT, confirmed from NLoader boot strings)
- WL1273 is a combo WiFi/BT/FM/GPS chip, single driver family
- BCM2727 is connected via CCP2/MPHI (not a GPU — it's a multimedia co-processor)
- Base crystal: **38.4 MHz** (from NLoader: "MCU PLL is in ByPass Mode (38.4MHz)")
- Nokia internal codenames: Gazoo=OMAP3630, RAPU/Rapuyama=modem, YAMA=VCore domain

---

## 2. OMAP 3630 — Silicon Breakdown

### 2.1 Compute Units (6 Total)

```
┌─────────────────────────────────────────────────────┐
│                    OMAP 3630 SoC                     │
│                                                      │
│  ┌──────────────┐  ┌──────────────┐  ┌───────────┐  │
│  │  Cortex-A8   │  │  C64x+ DSP   │  │  SGX530   │  │
│  │  ARM + NEON  │  │   (IVA2.2)   │  │  (GPU)    │  │
│  │  ~1 GHz      │  │   ~520 MHz   │  │  ~200 MHz │  │
│  │  SIMD 128b   │  │  VLIW 8-way  │  │  GLES 2.0 │  │
│  └──────┬───────┘  └──────┬───────┘  └─────┬─────┘  │
│         │                  │                │        │
│  ┌──────┴──────────────────┴────────────────┴─────┐  │
│  │              L3/L4 Interconnect                 │  │
│  │              + SDMA (32 ch DMA)                 │  │
│  └──────┬──────────────────┬────────────────┬─────┘  │
│         │                  │                │        │
│  ┌──────┴───────┐  ┌──────┴───────┐  ┌─────┴─────┐  │
│  │   ISP        │  │  Display SS  │  │  mDDR     │  │
│  │  CCDC+PRV+RSZ│  │  DSS+DISPC   │  │  256 MB   │  │
│  └──────────────┘  └──────────────┘  └───────────┘  │
│                                                      │
│  ┌──────────┐  ┌────────┐  ┌──────┐  ┌───────────┐  │
│  │ Camera IF│  │  MMC/SD│  │ USB  │  │ UART/SPI  │  │
│  │CCP2+CSI-2│  │  (eMMC)│  │ OTG  │  │  I2C/GPIO │  │
│  └────┬─────┘  └────────┘  └──────┘  └───────────┘  │
└───────┼─────────────────────────────────────────────┘
        │ CCP2/MPHI (SubLVDS, 650 Mbps)
        │
┌───────┴─────────────────────────────┐
│     Broadcom BCM2727 VideoCore III   │
│     (external co-processor MCM)      │
│                                      │
│  ┌──────────────┐  ┌──────────────┐  │
│  │  Vec16 DSP   │  │  Vec16 DSP   │  │
│  │  (core 0)    │  │  (core 1)    │  │
│  │  16-way SIMD │  │  16-way SIMD │  │
│  └──────┬───────┘  └──────┬───────┘  │
│         │                  │         │
│  ┌──────┴──────────────────┴──────┐  │
│  │   3D pipeline + ISP hardware   │  │
│  └────────────────────────────────┘  │
│                                      │
│  ┌────────────────────────────────┐  │
│  │      32 MB stacked SDRAM       │  │
│  └────────────────────────────────┘  │
│                                      │
│  Capabilities: H.264/H.263/MPEG-4    │
│  encode, VP6/VC1 decode, 12MP ISP,   │
│  JPEG HW, HDMI 720p, face detect     │
└──────────────────────────────────────┘
```

See detailed per-unit docs:
- [ARM Cortex-A8 + NEON deep dive](./undocumented-arm-neon.md)
- [C64x+ DSP (IVA2.2) deep dive](./undocumented-dsp-iva2.md)
- [PowerVR SGX reversing](../sgx540-reversing/) — existing RE work
- [OMAP3 ISP deep dive](./undocumented-isp.md)
- [System-level features](./undocumented-system.md)

---

## 3. Memory Budget — 256 MB Strategy

Total 256 MB mDDR shared between all cores. This is THE constraint.

### 3.1 Estimated Allocation

| Consumer               | Budget   | Notes                              |
|------------------------|----------|------------------------------------|
| Linux kernel + modules | ~8 MB    | stripped, minimal config            |
| Userspace base (musl)  | ~12 MB   | Alpine/Void minimal                |
| DSP firmware + buffers | ~16 MB   | carve-out via CMA/reserved mem     |
| GPU driver + surfaces  | ~16 MB   | PVR heap, framebuffers             |
| Camera buffers (V4L2)  | ~24 MB   | 4x 8MP raw Bayer @ ~6 MB each     |
| Processing workspace   | ~32 MB   | scratch for DSP/NEON pipelines     |
| Application + UI       | ~16 MB   | terminal + photography app         |
| Networking stack + fw  | ~8 MB    | WiFi firmware, WPA supplicant, BT  |
| System headroom        | ~16 MB   | caches, tmpfs, slab                |
| **Free/swap margin**   | ~108 MB  | breathing room, zram swap          |

### 3.2 Memory Tricks

- **zram swap**: compress inactive pages in RAM, ~2:1 ratio
- **CMA**: for DSP/ISP/GPU carve-outs, reclaimable when hardware not active
- **DMABUF sharing**: single buffer visible to ISP, DSP, GPU, ARM — zero-copy
- **mmap I/O**: V4L2 MMAP buffers avoid copy to userspace
- **DSP L1/L2 SRAM**: 80K+64K on-DSP — process tiles there, not in DDR
- **On-chip SRAM**: 64KB at 0x40200000 — ~3 cycle access, software scratchpad
- **Huge kill**: no systemd, no dbus, no polkit — direct init, minimal services

---

## 4. Computational Photography — Target Features

### Phase 1 — Foundations
- [ ] Raw Bayer capture via ISP bypass
- [ ] ISP hardware pipeline configuration
- [ ] NEON-accelerated demosaic (if bypassing ISP)
- [ ] Basic auto-exposure / auto-white-balance in software

### Phase 2 — Core Algorithms (DSP + NEON)
- [ ] Multi-frame HDR (bracket capture + merge on DSP)
- [ ] Temporal noise reduction (align + average on DSP)
- [ ] Edge-aware sharpening (unsharp mask, NEON)
- [ ] Lens distortion correction

### Phase 3 — Advanced
- [ ] Handheld super-resolution (multi-frame, sub-pixel alignment)
- [ ] Night mode (long-exposure simulation via burst alignment)
- [ ] Computational bokeh (depth from defocus or dual-frame)
- [ ] Panorama stitching

### Phase 4 — GPU Post-processing
- [ ] Real-time tone mapping (fragment shader)
- [ ] Color grading / film emulation LUTs
- [ ] Live viewfinder with preview filters
- [ ] HDMI output with processed feed

---

## 5. Wireless & Networking Stack

### WiFi — TI WL1273 (wl12xx/wlcore)
- **Bus: SPI-A (MCSPI), NOT SDIO** (confirmed Task 019: `am_spia.cpp`, `SpiClient` errors)
- Driver: `wlcore` + `wl12xx` on SPI bus (mainline kernel supports both SPI and SDIO)
- Firmware: `wl1271-fw-5.bin` (linux-firmware)
- Features: station, AP mode, monitor mode

### Bluetooth — TI WL1273
- **Bus: UART port 0, 115200 bps, H4+ protocol** (confirmed Task 019: `hci.ini`)
- Driver: `hci_ll` (shared transport)
- BlueZ 5.x, profiles: SPP, FTP, PAN, HID

### Cellular — 3G Modem
- Interface: likely HSI/SSI or USB (needs investigation)
- oFono or ModemManager

### GPS — TI NaviLink (WL1273 combo)
- UART, NMEA protocol, gpsd

### NFC — NXP PN544
- Driver: `pn544` (mainline), neard/libnfc

### FM Radio — WL1273 combo
- V4L2 radio interface

---

## 6. Development Strategy — Emulate First, Flash Safe

See [emulation strategy](./emulation-strategy.md) for full details.

**Core principle**: never flash anything to real hardware that hasn't
booted successfully in emulation.

### Available Resources
- **L3 service manuals** — schematics, test points, signal routing
- **Original Symbian firmware** — reference for hardware init sequences
- **SGX540 reverse engineering** — existing USSE ISA work in `sgx540-reversing/`

### Phases
1. **Firmware archaeology** — dump, extract, analyze
2. **QEMU emulation** — boot Symbian, then custom Linux, all in emulator
3. **Minimal flash** — serial console first, one unit only
4. **Iterative HW validation** — bring up each subsystem

---

## 7. Linux Platform Choices

### Kernel
- Mainline Linux 6.x, `omap2plus_defconfig` starting point
- Nokia E7 device tree (needs writing)
- Key drivers: omap3isp, wlcore, pvrsrvkm, tidspbridge, twl4030

### Userland
- Alpine Linux (musl, ~8 MB base) or Void Linux (musl variant)
- BusyBox, openrc/runit, ash/zsh

### Display / UI
- Start: framebuffer + fbterm
- Evolve: Custom GLES2 UI via EGL

### Build System
- Cross-compile on x86_64, Buildroot preferred
- DSP: TI C6000 CGT cross-compiler

---

## 8. Boot Chain

### Stock Symbian (from NLoader RE)
```
OMAP3630 ROM code (mask ROM, immutable)
  → NLoader / SWBL (flash offset 0x140000, "RAMSET SW")
    → SDRAM init, VCore calibration (SmartReflex)
    → Charger detect, PLL config
    → Security auth (BB5 — R&D mode bypasses ROFS check!)
    → DSP firmware load
    → Decompress SOS+CORE (LZO/ZLIB)
    → Jump to Symbian kernel at entry point
```

### Target Linux
```
OMAP3630 ROM code (mask ROM)
  → NLoader (Nokia, preserved)
    → U-Boot (chainloaded, needs investigation)
      → Linux kernel (zImage + DTB)
        → initramfs → rootfs
```

---

## 9. Known Resources

- **Nokia E7 L3 service manual** — AVAILABLE
- **Extracted firmware** — all 3 ROFS filesystems, eMMC image, NLoader/bootloader
- **saur0n/unpacker** — built, working for FPSX/ROFS extraction
- postmarketOS Nokia device ports
- Maemo Leste (Nokia N900, OMAP3430)
- TI OMAP3 TRM (~4000 pages)
- TI C6000 Programmer's Guide (SPRU187)
- **SGX540 reverse engineering** — in-house, `sgx540-reversing/`
- **RPi VCHIQ driver** — reference for BCM2727 VCHI protocol
- **hermanhermitage/videocoreiv** — VC4 RE tools (partially applicable to VC3)
- QEMU OMAP3: removed from mainline; qemu-linaro (2014.01) was last version with it

---

## 10. Confirmed Hardware Chip List (from Firmware RE)

| Chip | Type | Interface | I2C Bus | I2C Addr | Evidence |
|------|------|-----------|---------|----------|----------|
| **Broadcom BCM2727** | VideoCore III co-proc (32MB SDRAM MCM) | CCP2/MPHI | — | — | `mphi_ccp2.c`, `DVCDriver`, 35 VLL files |
| **Toshiba TCM8590MD** | Main camera sensor (8MP) | CSI-2 via BCM2727 | — | — | `tcm8590md_camera.vll`, `tcm8590md_tuner.vll` |
| **ACMEmini** | Front camera sensor (EDoF) | CSI-2 via BCM2727 | — | — | `acmemini_camera.vll` |
| **Atmel mXT** | Capacitive touch controller | I2C_0 (I2C1) | 0 | 0x4C* | `DAtmelSensor`, `TouchIcAtmel`; schematic: UI flex I2CSDA/SCL |
| **AKM AK8974** | Magnetometer / compass | I2C_0 (I2C1) | 0 | 0x0F* | `DMagnetometerPddSensorAK8974`; schematic: QWERTY flex I2C_0 |
| **ST LIS302DL** | Accelerometer (N1103) | I2C_0 (I2C1) | 0 | 0x1D* | `DAccPddSensorLIS302DL`; schematic: main board via SENSORS flex |
| **TI TPS65950** | PMIC (TWL4030 family, "PEARL") | I2C_0 (I2C1) | 0 | 0x48-0x4B | Schematic: PEARL N2300 on I2C(1-0); NLoader "Set Gazoo VCore" |
| **TI BQ2415x** | Li-Ion charger IC | I2C_2 (I2C3) | 2 | 0x6B | NLoader: probes 0xD6 (8-bit), `bq2415%d found (I2C_%d)` r3=2 |
| **Summit SMB338** | USB charger SMPS | I2C_2 (I2C3) | 2 | 0x6B | NLoader: same addr, rev-detect. `SMB338%c found (I2C_%d)` |
| **Summit SMB138** | USB charger | I2C_2 (I2C3) | 2 | 0x6B | NLoader: same addr, rev-detect. `SMB138%c found (I2C_%d)` |
| **NXP PN544** | NFC controller | I2C_0 (I2C1)? | 0? | 0x28* | `nfc.dll`; datasheet default 0x28; bus unconfirmed |
| **ALS/Prox sensor** | Ambient light / proximity (N8001) | I2C_0 (I2C1)? | 0? | 0x39? | Schematic: UI flex, VAUX1 powered; model TBD |
| **Mentor MUSB** | USB OTG controller | On-chip (OMAP3) | — | — | `MUSBMHDRC`, USB driver classes |
| **TI WL12xx** | WiFi/BT/FM/GPS combo | **SPI-A** + UART | — | — | `WlanScanFsm`, `am_spia.cpp`, `hci.ini` port=0 baud=115200 |
| **Rapuyama** | 3G modem (Nokia CMT) | Internal bus | — | — | NLoader "Start RapuYama VCore calibration" |

*\* = High confidence but needs I2C scan or deeper RE for final confirmation.*
See: [docs/i2c-bus-map.md](./i2c-bus-map.md) for full analysis and DTS template.

## 11. NLoader Boot Sequence (from flash offset 0x140000)

Build date: Jul 27 2012. Internal name: "RAMSET SW".

1. SDRAM Init → 2. OneNAND Detect → 3. Copy to SDRAM → 4. Read TOC →
5. I2C Init → 6. VCore Calibration (SmartReflex) → 7. Charger Detection →
8. MCU/DSP PLL Config → 9. Security Auth → 10. DSP FW Load →
11. Boot Mode Selection → 12. OS Decompression (LZO/ZLIB) → 13. OS Start

**R&D mode disables ROFS signature check!** ("R&D : ROFS check disabled")

See: [lore/daily/2026-02-26-bootloader-re.md](../lore/daily/2026-02-26-bootloader-re.md)

## 12. BCM2727 VideoCore III

See: [docs/videocore-iii-research.md](./videocore-iii-research.md)

Key points:
- Dual Vec16 DSP architecture (NOT a GPU in the traditional sense)
- 32 MB stacked on-package SDRAM — high internal bandwidth
- Connected via CCP2 (SubLVDS, 650 Mbps) — OMAP3 camera port repurposed
- ARM loads VC firmware (opposite of RPi where VC boots ARM)
- VLL files are native VC3 ELF shared libraries (e_machine=137)
- VCHI protocol reusable from RPi, transport must be CCP2 instead of AXI
- No public documentation — everything from firmware RE
- Priority: locate BCM2727 firmware blob via DVCDriver.dll analysis

## 13. Open Questions

- [x] ~~Exact camera sensor model~~ → **Toshiba TCM8590MD** (8MP main)
- [ ] Modem interface (HSI vs USB) and protocol — Rapuyama confirmed, bus TBD
- [ ] NOLO → U-Boot chainload reliability
- [ ] GPU blobs — SGX DDK version for this silicon rev
- [x] ~~Battery management IC variant~~ → **BQ2415x + SMB338 + SMB138**
- [x] ~~Touch controller IC and mainline status~~ → **Atmel mXT** (mainline: `atmel_mxt_ts`)
- [ ] AMOLED panel controller (DSI)
- [ ] Keyboard matrix — GPIO or dedicated controller?
- [x] ~~OneNAND vs eMMC partition layout~~ → **Full TOC extracted from NLoader** (24 partitions)
- [ ] BCM2727 firmware blob location in flash/ROFS
- [ ] GENIO_INIT (GPIO config) contents — encrypted by BB5
- [ ] OMAP3630 GPMC addresses (909 in 0x50xxxxxx) — BCM2727 or OneNAND?

---

*Last updated: 2026-02-26*
*Status: Firmware archaeology phase — extraction complete, analysis ongoing*
