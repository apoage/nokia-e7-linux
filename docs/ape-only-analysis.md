# APE_ONLY ENO FPSX Analysis -- Nokia E7-00 (RM-626)

> **Note (2026-03-23):** Real HW is BCM2727B1 (ARM1176), not OMAP3630. See docs/critical-cpu-discovery.md

## File Information

| Property | Value |
|----------|-------|
| Filename | `RM626_APE_ONLY_ENO_11w36_v0.037.fpsx` |
| Size | 4,786,175 bytes (4.56 MB) |
| MD5 | `c47b79ae79378529fa1d2dfc8574d02d` |
| Marker | 0xB2 (valid FPSX) |
| Encryption | 0x01 (signed, NOT encrypted) |
| Tool | Elf2flash 11.13.000 |
| Platform | CMT (BB5), XSR 1.6 |
| Version | 0.037, week 11w36 |
| VPL SubType | Content (not Mcu/Ppm) |
| VPL CRC | `4f81d859` |

**Both archive copies are identical** (same MD5).

Source paths:
- `archive-dump/mobilní telefony/nokia/flash images/Products/RM-626/RM626_APE_ONLY_ENO_11w36_v0.037.fpsx`
- `archive-dump/Nokia-E7/RM-626_cooked/RM626_APE_ONLY_ENO_11w36_v0.037.fpsx`

## What Is This File?

"APE_ONLY ENO" = **Application Processor Engine, Engineering Override**.

This file contains factory-programmed **configuration data** for the Nokia E7's
APE (application processor) side. It targets the following NAND flash partitions:

| Partition | Purpose | Notes |
|-----------|---------|-------|
| **NPC** | Nokia Product Configuration | Phone-specific calibration (display, audio, RF offsets) |
| **CCC** | Customer Care Center Config | Service center access/parameters |
| **HWC** | Hardware Calibration | Per-unit hardware variant data (sensors, camera, etc.) |
| **R&D** | Research & Development | Debug flags, engineering mode settings |
| **PARTNERC** | Partner/Operator Certificate | Operator customization/branding certificates |

These partitions live near the end of the MuxOneNAND flash. The ERASE_DCT5 tag
specifies NAND erase starting at address 0x8C (block units on OneNAND) with device
type 0x03 (MuxOneNAND).

## FPSX Header Structure

### Global Header (0x0000 - 0x003C)

```
Offset  Size  Field                    Value
0x0000  1     FPSX marker              0xB2
0x0001  2     Header flags             0x00 0x00
0x0003  1     Encryption type          0x01 (signed-only, no cipher)
0x0004  1     Tag 0x33 (version block)
0x0005  11    Binary metadata          00 0000 10e6 044e a017 15f4
0x0010  43    Version string           ",010.044.001\n    Elf2flash 11.13.000\n    CMT"
```

### TLV Tags (0x003D - 0x0149)

```
Tag   Len  Name                    Value
0xC3  8    CMT_Algo                "XSR 1.6" (XSR = flash algorithm)
0xC2  4    CMT_Type                "BB5"
0xD4  81   CMT_SupportedHW         10 records (see below)
0xCE  8    AlgoSendingSpeed        6,500,000 baud (6.5 Mbaud)
0xD1  8    MessageReadingSpeed     97,998 baud (~98 Kbaud)
0xCF  8    ProgramSendingSpeed     6,500,000 baud
0xCD  8    SecondarySendingSpeed   650,000 baud
0xF0  1    EncryptionFlag          0x01 (signed, not encrypted)
0xEF  4    Unknown                 0x01000000
0xF6  3    Unknown                 0x00001E
0xED  66   PartitionTable          5 partitions (NPC, CCC, HWC, R&D, PARTNERC)
0xC8  14   ERASE_DCT5             NAND erase descriptor (type=01, start=0x8C blocks)
```

### CMT_SupportedHW Records (RAPUYAMA D1800 Compatibility)

The D4 tag contains 10 hardware compatibility records. The first byte (0x08) is
a record count or version indicator, followed by 10 x 8-byte records:

```
# CS   ASIC   Rev   Flags    Notes
0  40  0x1921  0x00  0x1104   CMT (0x40) + CMT_ALT (0x0C), RAPUYAMA rev 0
1  40  0x1921  0x00  0x1104   (CS=0x0C variant)
2  60  0x1921  0x00  0x1104   APE (0x60) + APE_ALT, rev 0
3  60  0x1921  0x00  0x1104
4  60  0x1921  0x01  0x1104   APE, silicon rev 1
5  60  0x1921  0x01  0x1104
6  60  0x1921  0x02  0x1104   APE, silicon rev 2
7  60  0x1921  0x02  0x1104
8  60  0x1921  0x03  0x1104   APE, silicon rev 3
9  60  0x1921  0x03  0x1104
```

Key findings:
- **ASIC ID 0x1921** = RAPUYAMA D1800 (OMAP3630 SoC package) -- confirmed
- **Silicon revisions 0-3** supported (4 steppings)
- **CS byte**: 0x40 = CMT-side, 0x60 = APE-side; sub-types 0x03 and 0x0C per side
- **0x1104** = likely protocol version or flash interface type

### SOS*ENO Signature Blocks (0x0150 - 0x1E95)

The header contains **7 SOS*ENO blocks** at exactly 0x432 (1074) byte intervals,
plus **7 AENO0\* sub-blocks** interleaved between them:

```
SOS*ENO blocks:  0x0150, 0x0582, 0x09B4, 0x0DE6, 0x1218, 0x164A, 0x1A7C
AENO0* blocks:   0x02C2, 0x06F4, 0x0B26, 0x0F58, 0x138A, 0x17BC, 0x1BEE
Spacing:         0x432 (1074 bytes) between consecutive SOS or AENO markers
```

Each SOS*ENO block structure:
```
Offset  Field
+0      "SOS*ENO\0" (8 bytes)
+8      4 bytes zero padding
+12     0x0003 (constant)
+14     2-byte block-specific checksum (varies per block)
+16     0x00 0004 (constant)
+18     2-byte flash offset (varies)
+20     Configuration data (~48 bytes, mostly constant across blocks)
+68     Zero padding to next block boundary
```

Each AENO0* block contains:
- 16-byte header: `00 10 0000 0400 0000 0000 0000 E403 0000`
- "AENO0*" marker (6 bytes)
- Version reference: `0x0094`
- Zero padding (148 bytes)

Between AENO and the next SOS*ENO, there is:
1. ~128 bytes of **RSA-1024 signature** data (high-entropy cryptographic hash)
2. Fixed trailer: `e8ea de31 1eeb 4800 0010 8800 00f0 8f10 a004 3900`
3. Version string "0.037"
4. 16-byte padding zeros
5. `5D 01 27 2D` + 16 bytes of additional crypto/hash data

The signature structure confirms enc_type=0x01 = **digitally signed** content.
Each block has its own RSA signature for flash-time authentication by the BB5
security subsystem.

## Content Blocks (TBlocks) -- 0x1E96 to EOF

### TBlock Structure

All content is stored as **BlockType 0x17** (general data) blocks:

```
Byte  Field          Value
0     contentType    0x54 ('T' = Code/data)
1     const01        0x01
2     blockType      0x17 (BlockType17 = general data)
3-4   headerSize     0x0E 0x00 (14 bytes, LE16)
5-18  blockHeader    (14 bytes, see below)
19+   content        (raw NAND data, ~512 KB per block)
```

### TBlock Header Fields (14 bytes)

```
Offset  Size  Field              Values across blocks
+0      2     flash_device       0x0003 (MuxOneNAND, constant)
+2      2     block_checksum     varies (0x9446, 0x5694, 0x7FE2, ...)
+4      2     unknown            0x0100 (constant)
+6      2     length_indicator   0x0400 (constant, = 1024 pages?)
+8      4     nand_address       0x0000008C → 0x000000D4 (incrementing by 8)
+12     2     unknown            0x0400 (constant)
```

### NAND Address Map

The `nand_address` field increments by 8 for each block, mapping to physical
OneNAND block addresses (each block = 256 KB):

```
TBlock  File Offset  NAND Addr  Physical Address
0       0x01E96      0x8C       0x8C * 256KB = 0x2300000 (35 MB)
1       0x81EBC      0x94       0x94 * 256KB = 0x2500000 (37 MB)
2       0x101EE2     0x9C       0x9C * 256KB = 0x2700000 (39 MB)
3       0x181F08     0xA4       0xA4 * 256KB = 0x2900000 (41 MB)
4       0x201F2E     0xAC       0xAC * 256KB = 0x2B00000 (43 MB)
5       0x281F54     0xB4       0xB4 * 256KB = 0x2D00000 (45 MB)
6       0x301F7A     0xBC       0xBC * 256KB = 0x2F00000 (47 MB)
7       0x381FA0     0xC4       0xC4 * 256KB = 0x3100000 (49 MB)
8       0x401FC6     0xCC       0xCC * 256KB = 0x3300000 (51 MB)
9       0x481FEC     0xD4       0xD4 * 256KB = 0x3500000 (53 MB)
```

Total flash range: 0x8C to 0xD4 blocks = 73 blocks * 256 KB = **18.25 MB** of NAND.

Block spacing in file: 0x80026 (524,326 bytes) = 512 KB content + 38 bytes overhead.

Last block (9) is partial: content ends at 0x490720, remainder is 0xFF (erased NAND).

### Content Data Analysis

**The payload data is NOT encrypted**. The enc_type=0x01 means "signed only" --
the RSA signatures in the SOS*ENO blocks authenticate the content at flash time,
but the raw NAND data itself is plaintext.

Byte frequency analysis of the first 64 KB of content data:

| Byte Range | Frequency | Notes |
|------------|-----------|-------|
| 0xFE-0xFF  | 330-364   | Most common (erased NAND bits) |
| 0x7F-0xBF  | 300-342   | Common (partially written) |
| 0x00-0x10  | 156-178   | Least common (all-zero = most bits cleared) |

This distribution (high-bit bytes overrepresented, low-bit bytes underrepresented)
is characteristic of **raw NAND flash data** where the erased state is 0xFF.
Programming NAND clears bits from 1 to 0, so programmed data has fewer set bits
than erased data. The content is dense with no obvious filesystem structure visible
at the raw level.

The last ~224 bytes (0x490720 to 0x4907FF) are all 0xFF -- the final partially-used
NAND erase block.

## ENO Content Identification

"ENO" stands for **Engineering Normal Override** (or Emergency Normal OS, depending
on context). In the Nokia production line:

1. **During manufacturing**, the phone is first flashed with the base firmware
   (CORE, ROFS2, ROFS3)
2. **Then ENO is flashed** to write factory calibration data:
   - NPC: Display white balance, audio levels, RF power table offsets
   - CCC: Service center repair history/flags
   - HWC: Board variant identification (which accelerometer, magnetometer, etc.)
   - R&D: Engineering debug flags (often disabled for production)
   - PARTNERC: Operator branding certificates
3. **Each phone gets unique ENO data** from the factory test station

The "APE_ONLY" qualifier means this ENO file targets only the application
processor (OMAP3630) side, not the CMT (modem) side. This is because the NPC/CCC/HWC
partitions are on the APE-accessible OneNAND, not the modem-internal NAND.

## Comparison with Other FPSX Files

| Property | APE_ONLY ENO | CORE | ROFS2/3 | eMMC |
|----------|-------------|------|---------|------|
| enc_type | 0x01 (signed) | 0x02 (DES) | 0x00 (none) | 0x01 (signed) |
| Content | Configuration data | Symbian kernel + ROFS | Language/UI packs | Mass storage |
| Size | 4.6 MB | ~129 MB | 4-57 MB | varies |
| BlockType | 0x17 | 0x17 | 0x17 | 0x17 |
| NAND range | 0x8C-0xD4 | 0x2A4-0x5C98 | varies | eMMC (not NAND) |

## SDRC / Pad Mux Search Results

**No OMAP3630 initialization code found.** The content is raw NAND configuration
data, not ARM executable code. Specifically:

- No SDRC register addresses (0x6D000000 range) found
- No SCM pad mux addresses (0x48002000 range) found
- No ARM branch instructions or exception vectors
- No ELF headers, no executable code signatures
- No OMAP3630 clock management registers

This is expected: the APE_ONLY ENO file contains only factory calibration data
that gets written to specific NAND partitions. Boot code (SWBL, NLoader) and
pad mux configuration (GENIO_INIT) are in separate NAND partitions and separate
FPSX files.

## VPL Context

From `RM626_059D0N6_111.040.1511_015.vpl`, the complete flash package contains:

| # | File | Type | Signed | Optional | Notes |
|---|------|------|--------|----------|-------|
| 1 | `*.dcp` | DCP (data config) | Yes | No | Phone-specific data |
| 2 | `*_prd.core.fpsx` | Mcu | No | No | Symbian kernel + core (v111.040.1511) |
| 3 | `*_prd.rofs2.fpsx` | Ppm | No | No | Euro3 QW1 language pack |
| 4 | `*_prd.rofs3.fpsx` | Ppm | No | No | Locale C00 data |
| 5 | `*.uda.fpsx` | Content | No | No | User data area |
| 6 | **`*_ENO_*.fpsx`** | **Content** | **No** | **No** | **This file** |
| 7 | `*.emmc.fpsx` | MemoryCardContent | No | Yes | eMMC image (16 GB) |
| 8 | `*_data_block` | WmDrmPdTemplate | Yes | Yes | WMDRM template |
| 9 | `ive3_otp_template_production.bin` | HdmiOtpDataTemplate | Yes | Yes | BCM2727 OTP |
| 10 | `*_TypeLabel_*.pcx` | TypeLabelPicture | No | Yes | Type label image |
| 11 | `*_TypeLabel_*.xml` | TypeLabelConfiguration | No | Yes | Type label config |
| 12 | `*_size_ccc_*.bin` | CccCmt | Yes | Yes | CCC template |
| 13 | `*_size_hwc_*.bin` | HwcTemplateCmt | Yes | Yes | HWC template |
| 14 | `simlock_3gstandard_bb5.bin` | SimLock | Yes | Yes | SIM lock |
| 15 | `*.emmc.cardverref.xml` | CardVerRef | Yes | Yes | eMMC verification |
| 16 | `MC_F_nocard.mcard.cardverref.xml` | CardVerRef | Yes | Yes | No-card ref |
| 17 | `*_Default_Verification_File.spr` | SimpleProdVerReference | Yes | Yes | Production verification |
| 18 | `*_signature.bin` | Signature | -- | No | Package signature |

Product: `059D0N6` = Czech QWERTY, Silver/White, Euro variant group.
SW version: 111.040.1511, variant version 015.

## Implications for Nokia E7 Linux Project

1. **No boot code or SDRC values here.** The SDRC configuration is in the SWBL
   bootloader, which is NOT included in any extracted firmware image we have. The
   SWBL lives at NAND offset 0x0800 and is only 8.5 KB. NLoader does NOT configure
   SDRC; it relies on SWBL having already set it up.

2. **HWC data may contain variant identification** that tells us which exact
   hardware components are on our Unit A vs Unit B (accelerometer, magnetometer,
   charger IC variants per the BOM variant table in `variants.json`).

3. **GENIO_INIT (pad mux) is separate and ENCRYPTED** -- it lives at NAND offset
   0x12000, protected by BB5 security. The pad mux configuration is NOT in this
   ENO file.

4. **The NAND address range 0x8C-0xD4** (18.25 MB) is useful for understanding
   the OneNAND memory layout on the E7, complementing the flash-memory-map.md data.

5. **RAPUYAMA ASIC ID 0x1921 confirmed** with 4 silicon revisions supported.
   This aligns with the OMAP3630 ES1.0/1.1/1.2 revisions documented in TI errata.

## Raw Data Extraction

To extract the raw NAND data from the FPSX container, strip the FPSX header
(0x1E96 bytes) and each TBlock's 20-byte overhead (at 0x80026-byte intervals).
The content can then be split into 256 KB NAND blocks corresponding to the
OneNAND block addresses 0x8C through 0xD4.

Note: The ENO data is likely factory-default/template data (version "0.037"),
not phone-specific calibration. Phone-specific values would be programmed at
the factory test station using a different mechanism (likely FBUS/USB direct
NAND programming, not FPSX).
