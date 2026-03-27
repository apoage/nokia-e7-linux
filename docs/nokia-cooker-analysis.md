# Nokia Cooker 3.4 — .NET Decompilation Analysis

## Overview

Nokia Cooker 3.4 is a .NET (C#) application for parsing, modifying, and
repacking Nokia BB5 firmware packages in FPSX format. Decompiled via
`dnfile` metadata analysis and IL bytecode tracing (no execution).

Binary: `archive-dump/Nokia-E7/NokiaCooker_3.4/NokiaCooker.exe`
Size: 1,197,568 bytes | Format: PE32 Mono/.Net assembly

## Key Findings

### 1. FPSX Encryption Uses DES with Hardcoded Global Key

Nokia Cooker contains **two symmetric ciphers** for FPSX block content:

| Cipher | Enc Type | Key | IV |
|--------|----------|-----|-----|
| DES (CBC) | 0x02 | `41 6c e8 71 c3 bd 0a 28` | `24 ee a7 d4 ce bf f3 63` |
| AES-128 (CBC) | 0x03 | `01×16` (all 0x01) | `02×16` (all 0x02) |

**Both keys are hardcoded FieldRva static byte arrays** — not per-device,
not derived from IMEI or other device-specific data.

The AES key (`01 01 01...`) is trivially weak — likely a placeholder from
development that was never replaced in production.

The encryption type is determined by byte offset 3 in the FPSX file header
(`cipher_type = header_word >> 24`). If neither type 2 nor 3, Nokia Cooker
throws "Unknown Header".

### 2. CORE Partition is DES-Encrypted; ROFS is Not

Actual RM-626 firmware FPSX files:

| File | Enc Byte | Encryption |
|------|----------|------------|
| `core.fpsx` | 0x02 | **DES** |
| `rofs2.fpsx` | 0x00 | None |
| `rofs3.fpsx` | 0x00 | None |
| `uda.fpsx` | 0x00 | None |
| `emmc.fpsx` | 0x01 | Unknown (possibly signed-only) |
| `ape_only.fpsx` | 0x01 | Unknown |

### 3. No IVE3 Support

Nokia Cooker has **no IVE3 block type** and no code path for IVE3 partitions.
The supported block types are: 0x17, 0x27 (ROFS_Hash), 0x28 (CORE_Cert),
0x2E, 0x30, 0x3A, 0x49. IVE3 uses BB5 per-device RSA authentication —
completely separate from the FPSX transport encryption.

### 4. SHA1 Used for Hash Verification

`SHA1CryptoServiceProvider` computes hashes — likely for verifying
`cmt_root_key_hash20_sha1` (20-byte SHA1 CMT root key hash) and
`rap_papub_keys_maybe_hash20_sha1` (RAPUYAMA PA public key hash).

## FPSX Format Structure

### File Header

```
Offset  Size  Field
0x00    1     Marker (always 0xB2)
0x01    2     Flags/version
0x03    1     Encryption type (0=none, 1=signed?, 2=DES, 3=AES)
0x04    4     Block count or header size
```

### BB5 TLV Header Section (offset 0x08+)

Tag-Length-Value entries using BB5 flash protocol tags. Identical
across partitions from the same firmware build:

| Tag | Name | Example Value |
|-----|------|---------------|
| 0xC2 | CMT_Type | "BB5" |
| 0xC3 | CMT_Algo | "XSR 1.6" |
| 0xCD | SecondarySendingSpeed | baud rate |
| 0xCE | AlgoSendingSpeed | baud rate |
| 0xCF | ProgramSendingSpeed | baud rate |
| 0xD1 | MessageReadingSpeed | baud rate |
| 0xD4 | CMT_SupportedHW | 81-byte HW compat table |
| 0xE1 | APE_SupportedHW | phone models |
| 0xE6 | DateTime | build timestamp |
| 0xE7 | APE_Phone_Type | phone identifier |
| 0xE8 | APE_Algorithm | flashing algorithm |
| 0xF4 | Descr | tool version string |

### Block Content

After TLV headers, one or more TBlock structures:

```csharp
class TBlock {
    long   _offset;
    byte   contentType;     // 0x54='T'(Code), 0x56='V', 0x5D(DataCert)
    byte   const01;
    byte   blockType;       // see TBlockType enum
    ushort blockHeaderSize;
    TBlockHeader blockHeader;
    byte   blockChecksum8;
    byte[] content;         // DES/AES encrypted if enc_type != 0
}
```

Encryption applies to block `content` only (not headers), starting at
byte 4 of the content array:
```
TransformFinalBlock(content, inputOffset=4, inputCount=content.Length-4)
```

## Type Definitions

### TBlockType (7 values)
```
0x17 = BlockType17           — general data block
0x27 = BlockType27_ROFS_Hash — ROFS hash + CMT root key hash (SHA1)
0x28 = BlockType28_CORE_Cert — CORE certificate + RAP PA pub key hash
0x2E = BlockType2E           — unknown
0x30 = BlockType30           — unknown
0x3A = BlockType3A           — description block
0x49 = BlockType49           — unknown
```

### TBlockHeader Variants

```
TBlockHeader:
  flashMemory, contentLength, contentChecksum16, location

TBlockHeader27_ROFS_Hash:
  cmt_root_key_hash20_sha1   — 20-byte SHA1 of CMT root key
  description, unkn2

TBlockHeader28_CORE_Cert:
  rap_papub_keys_maybe_hash20_sha1  — 20-byte SHA1 of RAP PA pub keys
  unkn2b
```

### TDeviceType
```
NOR=0, NAND=1, MuxOneNAND=3, MMC=4, RAM=5
```

### TASICType
```
CMT=0 (modem), APE=1 (application), BOTH=2
```

### TTLVType (29 tags)
```
0x0D = MORE_RootKeyHash_MORE    0xC2 = CMT_Type
0x12 = ERASE_AREA_BB5           0xC3 = CMT_Algo
0x13 = ONENAND_SubType_Unkn2    0xC8 = ERASE_DCT5
0x19 = FORMAT_PARTITION_BB5     0xCD = SecondarySendingSpeed
0x2F = PARTITION_INFO_BB5       0xCE = AlgoSendingSpeed
0xCF = ProgramSendingSpeed      0xD1 = MessageReadingSpeed
0xD4 = CMT_SupportedHW          0xE1 = APE_SupportedHW
0xE4 = UnknE4_Imp               0xE5 = UnknE5
0xE6 = DateTime                 0xE7 = APE_Phone_Type
0xE8 = APE_Algorithm            0xEA = UnknEA
0xEC = UnknEC                   0xED = UnknED
0xEE = Array                    0xF3 = UnknF3_Imp
0xF4 = Descr                    0xF6 = UnknF6
0xF7 = UnknF7                   0xFA = UnknFA_Imp
```

## Other File Formats (from archive)

| File | Magic | Format |
|------|-------|--------|
| `*_signature.bin` | `<?xml` | XML certificate/signature |
| `*_size_hwc_*.bin` | `CHWC` | Hardware Calibration |
| `*_size_ccc_*.bin` | `CCCC` | Country/Carrier Config |
| `ive3_otp_template_production.bin` | `OTP0` | OTP template (40 bytes) |
| `simlock_3gstandard_bb5.bin` | random | Encrypted SIM lock data |

## Embedded Data

### FieldRva Static Arrays

| # | Content | Purpose |
|---|---------|---------|
| 0 | `NC34` + 16-bit table | Nokia Cooker 3.4 magic + supported block types |
| 1 | uint16 table [1-21] | Valid block type enumeration |
| 2 | DES key (8 bytes) | `41 6c e8 71 c3 bd 0a 28` |
| 3 | DES IV (8 bytes) | `24 ee a7 d4 ce bf f3 63` |
| 4 | AES key (16 bytes) | `01×16` |
| 5 | AES IV (16 bytes) | `02×16` |
| 14 | 4-bit reversal LUT | Image processing (nibble swap) |
| 15-18 | Float arrays | sRGB gamma coefficients (UI rendering) |

### Crypto Method IL Structure

Two static helper methods:
```csharp
// MethodDef 0x31C — Rijndael/AES path
static ICryptoTransform CreateAESTransform(byte[] key, byte[] iv, bool decrypt) {
    using (var c = new RijndaelManaged())
        return decrypt ? c.CreateDecryptor(key, iv) : c.CreateEncryptor(key, iv);
}

// MethodDef 0x31D — DES path
static ICryptoTransform CreateDESTransform(byte[] key, byte[] iv, bool decrypt) {
    using (var c = new DESCryptoServiceProvider())
        return decrypt ? c.CreateDecryptor(key, iv) : c.CreateEncryptor(key, iv);
}
```

Dispatcher (MethodDef at RVA 0x1B290, 629 bytes IL):
```
cipher_type = (block_header_word >> 24)
if cipher_type == 2:
    transform = CreateDESTransform(key_des, iv_des, decrypt=true)
    plaintext = transform.TransformFinalBlock(content, 4, content.Length - 4)
elif cipher_type == 3:
    transform = CreateAESTransform(key_aes, iv_aes, decrypt=true)
    plaintext = transform.TransformFinalBlock(content, 4, content.Length - 4)
else:
    throw new Exception("Unknown Header")
```

## Verdict

| Question | Answer |
|----------|--------|
| Are FPSX keys extractable? | **Yes** — hardcoded DES + AES keys in binary |
| Are keys per-device? | **No** — global, same key for all devices |
| Does Cooker handle IVE3? | **No** — no IVE3 block type or partition handling |
| Does Cooker help with sos-ive3a.bin? | **No** — IVE3 uses BB5 RSA, not FPSX DES |
| Is CORE FPSX decryptable? | **Yes** — DES CBC with extracted key/IV |
| Is ROFS FPSX encrypted? | **No** — ROFS2/3 have enc_type=0 |

### Implications for Nokia E7 Linux Project

1. **CORE partition content is already extracted** from phone eMMC — we don't
   need to decrypt the FPSX package version. But if needed, the DES key works.

2. **IVE3 remains inaccessible** via Nokia Cooker. The BCM2727 camera firmware
   (sos-ive3a.bin) uses per-device RSA authentication (PA_RSAOBC from the ARM
   kernel extension), not the simple DES transport encryption.

3. **FPSX TLV tags map to BB5 flash protocol** — useful for understanding the
   USB flash chain (Task 033) and BB5 security bypass (Task 031).

4. **CMT_SupportedHW** table in FPSX headers contains 81 bytes of hardware
   compatibility data — could help identify RAPUYAMA silicon revisions.

## Analysis Method

Decompiled without executing: `strings` + `dnfile` (Python .NET metadata
parser) + `pefile` + manual IL bytecode tracing. No mono/dotnet runtime
needed. Key material extracted from FieldRva table entries with RVA-to-offset
translation via PE section headers.
