# Nokia E7 ROM File Inventory

**Source**: `romdumpplus.dmp` (31,793,152 bytes, 30.3 MB)
**Date parsed**: 2026-03-24
**ROM base address**: 0x80000000
**ROM size (header)**: 0x01F00000 (31.0 MB)

## TRomHeader

| Offset | Field | Value |
|--------|-------|-------|
| +0x00 | iVersion | 0.14.5945 |
| +0x04 | iTime | 0x00E1A39400E1A394 |
| +0x0C | iRomBase | 0x80000000 |
| +0x10 | iRomSize | 0x01F00000 (31.0 MB) |
| +0x14 | iKernDataAddress | 0x80044700 |
| +0x18 | iKernelLimit | 0xC8000000 |
| +0x1C | iPrimaryFile | 0xCA100000 |
| +0x20 | iSecondaryFile | 0x80044710 |
| +0x24 | iCheckSum | 0x80044C74 |
| +0x28 | iHardware | 0x1BF3C4A7 |
| +0x2C | iLanguage | 0x0000000000000000 |
| +0x34 | iKernelConfigFlags | 0x00000000 |
| +0x38 | iRomExceptionSearchTable | 0x0000067E |
| +0x3C | iRomHeaderSize | 0x80042D88 (2147757448) |
| +0x40 | iRomSectionHeader | 0x00000200 |
| +0x44 | iTotalSvDataSize | 0x00000000 (0) |
| +0x48 | iVariantFile | 0x00087E10 |
| +0x4C | iExtensionFile | 0x80044764 |
| +0x50 | iRelocInfo | 0x8004472C |
| +0x54 | iOldTraceMask | 0x00000000 |
| +0x58 | iUserDataAddress | 0x00000000 |
| +0x5C | iTotalUserDataSize | 0x7F000000 (2130706432) |
| +0x60 | iDebugPort | 0x00044000 (278528) |
| +0x68 | field_68 | 0x00000100 (256) |
| +0x70 | field_70 | 0x017BC000 (24887296) |
| +0x74 | field_74 | 0x01E52000 (31793152) |
| +0x80 | field_80 | 0x80000000 (2147483648) |
| +0xC8 | field_C8 | 0x00AA4000 (11157504) |
| +0xCC | field_CC | 0x013ADA50 (20634192) |
| +0xD0 | field_D0 | 0x00002D80 (11648) |
| +0xD4 | field_D4 | 0x00002000 (8192) |
| +0xD8 | field_D8 | 0x00000003 (3) |
| +0xE0 | field_E0 | 0x00042D88 (273800) |
| +0xE4 | field_E4 | 0x017BC000 (24887296) |
| +0xE8 | field_E8 | 0x01E52000 (31793152) |

## Summary

- **Total files cataloged**: 2
  - DLL: 2

## EXE Files

| Name | UID3 | SecureID | Capabilities | CodeSize | Stack | Exports |
|------|------|----------|-------------|----------|-------|---------|

## DLL Files

| Name | UID3 | SecureID | Capabilities | CodeSize | Exports |
|------|------|----------|-------------|----------|---------|
| KeyPublisherPlugin.dll | 0x1020724B | 0x00000000 | 0x40000000008000 | 270498556 | 2097152 |
| pipelib.ldd | 0x00000000 | 0x00000000 | 0x40000000008000 | 270498556 | 2097152 |

## LDD Files (Logical Device Drivers)

| Name | UID2 | UID3 | SecureID | VendorID | Capabilities | CodeSize | DataSize | Exports |
|------|------|------|----------|----------|-------------|----------|----------|---------|

## PDD Files (Physical Device Drivers)

| Name | UID2 | UID3 | SecureID | VendorID | Capabilities | CodeSize | DataSize | Exports |
|------|------|------|----------|----------|-------------|----------|----------|---------|

## Files of Interest

### Kernel and Boot


### Variant/ASSP (SoC-specific)

- None found with these name patterns

### Patcher/MemoryAccess/Security

- None found in ROM (these may only exist as SIS-installed files)
