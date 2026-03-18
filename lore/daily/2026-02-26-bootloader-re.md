# Bootloader Reverse Engineering — 2026-02-26

## Flash Layout (from NLoader TOC at 0x140000)

The Nokia E7 OneNAND flash contains these partitions (reconstructed from NLoader
TOC table):

```
Partition       Description                         Notes
─────────────────────────────────────────────────────────────
SWBL            Software BootLoader (NLoader)        Nokia proprietary
ADA             Application Data Area?               Small
KEYS            Security keys (BB5 fuses)            Crypto material
R&D             R&D mode flags                       Disables ROFS check!
PRIMAPP         Primary Application (kernel)         Symbian EKA2 kernel
RAP3NAND        Rapuyama 3G modem firmware            Baseband processor code
SOS+PMML        Power Management Microcode           PMIC init sequences
SOS+DECOMP      Decompressor (LZO/ZLIB)              For compressed images
PASUBTOC        PA Sub Table-of-Contents             Sub-partitioning
PAPUBKEYS       PA Public Keys                       Authentication certs
GENIO_INIT      GPIO / HW Init Data                  Board-level config!
SOS+IVE3A       IVA2.2 Engine firmware (part A)      DSP codec firmware
SOS+IVE3B       IVA2.2 Engine firmware (part B)      DSP codec firmware
SOS*UPDAPP      Update Application                   FOTA updater
SOS*DSP0        DSP firmware image                   TMS320C64x+ code
SOS*ISASW       ISA Software (?)                     Unknown
SOS+CORE        Core OS (BB5 header + ROFS0)         Main Symbian image
SOS+ROFS1       Read-only filesystem 1               Core FS layer
SOS+ROFS2       Language pack                        Localization
SOS+ROFS3       Operator variant                     Carrier customization
SOS-USER        User data                            R/W partition
SOS-SWAP        Swap partition                       Virtual memory
SOS-PMM         Power Management data                Runtime PM state
SOS-CRASH       Crash dump area                      Debug/diagnostics
NPC             Nokia Product Code                   Device variant ID
CCC             Customer Configuration Code          Operator config
HWC             Hardware Configuration               Board HW config
PARTNERC        Partner Certificate                  Partner auth
```

## NLoader Boot Sequence

### Stage 1: OMAP3630 ROM Boot
- OMAP3630 ROM bootloader (immutable, in SoC) reads first-stage from OneNAND
- Validates and loads SWBL (Software BootLoader / NLoader) into SRAM

### Stage 2: NLoader (at 0x140000 in flash)
Build date: **Jul 27 2012**
Internal name: RAMSET SW

NLoader performs this sequence (reconstructed from debug strings):

1. **SDRAM Init**
   - "SDRAM init failed" / "DDR %d MB (32bit) configured (%d kHz)"
   - Configures DDR memory controller

2. **OneNAND Detection**
   - "MuxOneNAND (CS%d) detected (Mfr: %04X, Dev: %04X, Rev: %04X)"
   - Identifies flash chip manufacturer/device/revision

3. **Copy Self to SDRAM**
   - "NLoader copied to SDRAM"

4. **Read TOC (Table of Contents)**
   - "Read TOC : %08X"
   - Locates all partitions in flash

5. **I2C Init**
   - "init IC2_%d controller done"
   - Enables I2C buses for PMIC communication

6. **VCore Calibration (SmartReflex)**
   - "Set Gazoo VCore (%d uV)" — "Gazoo" = OMAP3630 codename at Nokia!
   - "Start RapuYama VCore calibration"
   - "SR calibration failed -> OPs are limited to 450MHz"
   - Reads eFuse values for RAPU and YAMA voltage targets
   - **RAPU** = Rapuyama = CMT/cellular modem processor
   - **YAMA** = Application processor (OMAP3630)
   - "fop %d mcu  : mul %d, div %d, freq %d" — frequency operating points
   - "fop %d dsp  : mul %d, div %d, freq %d"
   - "fop %d yama : mul %d, div %d, freq %d"
   - "fop %d vcore raw  : %d" / "fop %d vcore final: %d"

7. **Charger Detection**
   - "bq2415%d found, rev:%x, (I2C_%d)" — **TI BQ24150/BQ24153 charger IC**
   - "SMB338%c found, rev:%x, (I2C_%d)" — **Summit SMB338 USB charger**
   - "SMB138%c found, rev:%x, (I2C_%d)" — **Summit SMB138**
   - Battery voltage checking: "Vbat : %d (limits with charger %d) / Temp: %d"

8. **MCU/DSP PLL Configuration**
   - "MCU PLL is in ByPass Mode (38.4MHz)" — base crystal frequency
   - "MCU PLL at full frequency"
   - "Set MCU speed: %d kHz" / "Set DSP speed: %d kHz"
   - "MessI bus speed = %d MHz" — internal interconnect bus

9. **Security Authentication**
   - "NLoader authentication failed"
   - "Genio authentication OK/failed"
   - "Hash check failed"
   - "Authentication fail : %s (reason: %d)"
   - "R&D : ROFS check disabled" — **R&D mode skips verification!**

10. **DSP Firmware Load**
    - "LDSP loaded by Nloader"
    - "DSP code started"
    - "DSP version : %s"
    - Loads SmartReflex DSP code for voltage calibration

11. **Boot Mode Selection**
    - Normal Mode / Local Mode / Test Mode / Update Mode / FOTA Mode
    - "USB -> Update Mode" / "ADL -> Local Mode"
    - "Forced Normal Mode" / "Massmemory boot"

12. **OS Image Decompression & Loading**
    - "COZLLZO compression detected" (LZO) / "ZLIB compression detected"
    - "SOS core image not found -> try ENOSW"
    - "Image: %s decompression failed"
    - "PartnerC prosessing failed for %s"

13. **OS Start**
    - "Start SOS at %08X (->%08X)" — **OS entry point address!**
    - Jumps to Symbian kernel

### USB Boot (NLoader Mode)
NLoader includes a full MUSB OTG driver for USB flashing:
- "Nokia Mobile Device (NLoader mode)" — USB device descriptor
- Cold/warm flash protocol
- EP0 control transfers, bulk transfers
- DMA channel management

## Key Hardware Details from NLoader

### Nokia Internal Codenames
| Codename | Meaning |
|----------|---------|
| Gazoo | OMAP3630 (application processor) |
| RAPU / Rapuyama | CMT (cellular modem terminal) |
| YAMA | Related to app processor voltage domain |
| Messi | Internal interconnect bus |

### Clock Configuration
- Base crystal: **38.4 MHz** (confirmed from "MCU PLL is in ByPass Mode (38.4MHz)")
- MCU PLL configurable via mul/div
- DSP PLL separate
- Frequency Operating Points (FOPs) for MCU, DSP, YAMA
- If SmartReflex calibration fails: limited to 450MHz

### Charger / Power ICs
| IC | Type | Bus |
|----|------|-----|
| BQ2415x | TI Li-Ion charger | I2C |
| SMB338x | Summit USB charger SMPS | I2C |
| SMB138x | Summit USB charger | I2C |
| TPS65950 (Gazoo VCore) | PMIC | I2C1 |

### Memory Configuration Strings
```
hw_conf: SDRAM: %d MB at %08X
hw_conf: PDRAM: %d kB at %08X
hw_conf: MCU speed: %d kHz
hw_conf: DSP speed: %d kHz
hw_conf: Non-MCU area: %d kB at %08X
hw_conf: MDI buffer: %d kB at %08X
hw_conf: M2D offset: %08X
hw_conf: ROM jump table: %08X
hw_conf: BSI value: %d
hw_conf: TOC location: %08X
hw_conf: Massmemory %d: %d MB
```

## Pre-ROFS Flash Map (Byte Offsets in rofs.img)

| Offset | Size | Content |
|--------|------|---------|
| 0x000000 | 0x40 | Partition table header (ADA, 3RD entries) |
| 0x000040 | ~0x13FFBF | 0xFF fill (erased flash) |
| 0x040000 | small | 8 bytes of data, rest empty |
| 0x140000 | ~0x20000 | **NLoader (SWBL)** — bootloader + TOC |
| 0x190000 | ~0x30000 | Code/data region (continuation) |
| 0x1C0000 | ~0x5000 | Small code/data region |
| 0x200000 | ~0x2980000 | **BB5 signed CORE image** (Symbian kernel + SOS) |
| 0x2B80000 | - | BB5 header for ROFS0 |
| 0x2B81000 | ~92MB | **ROFS0 filesystem** |

## GENIO_INIT — Critical for Emulation

The `GENIO_INIT` partition contains GPIO initialization data — board-level
hardware configuration. This is the key to understanding the E7's specific
GPIO pin assignments, mux settings, and peripheral enables.

Location in TOC: offset 0x012000, size 0x19CDC bytes (~105KB)

This needs to be extracted and analyzed — it likely contains:
- GPIO mux register values (which pins connect to which peripherals)
- I2C device addresses for all chips
- Power-on sequencing for BCM2727, WiFi, etc.

## eMMC Extraction Result

Successfully extracted eMMC from BLOCK_TYPE_H30 in FPSX:
- Format: FAT32 (OEM "fsim5.74")
- Boot signature: 0x55AA
- Bytes/sector: 512, Sectors/cluster: 32
- Total: 30,515,200 sectors = **14.9 GB** (full eMMC capacity)
- Contains factory default content (wallpapers, default apps, etc.)
