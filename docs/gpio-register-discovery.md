# GPIO Register Discovery — BCM2727B1

## Physical GPIO Base: 0x20200000
Confirmed by 300+ references in ROM dump.

## Kernel Virtual Mapping: 0xC8004000-0xC8006FFF  
7 GPIO banks mapped at 0x1000 intervals.

## BCM2835 Comparison
The base address matches BCM2835 (Raspberry Pi) but register offsets differ.
BCM2835 uses 4-byte aligned registers (GPFSEL at 0x00/0x04/0x08...).
BCM2727 uses byte-granularity offsets (0x31, 0x32, 0x33, 0x34, 0x35, 0x36).

## GPIO Register Offsets Found in ROM Code (0x14C800-0x14E200)

### Standard BCM2835-compatible
| Offset | BCM2835 Name | Refs |
|--------|-------------|------|
| 0x00 | GPFSEL0 (base, computed) | 80+ |
| 0x20 | GPSET1 | 1 |
| 0x48 | (GPEDS1+4?) | 1 |

### BCM2727-specific (NOT in BCM2835)
| Offset | Notes |
|--------|-------|
| 0x31-0x36 | Byte-addressed GPIO config? |
| 0x43 | Unknown |
| 0x48 | Unknown |
| 0x54 | Unknown |
| 0x64 | Unknown |
| 0x6B | Unknown |
| 0x6C | Unknown |
| 0x6E | Unknown |
| 0x72 | Unknown |
| 0x74 | Unknown (4 refs) |
| 0x78 | Unknown (3 refs) |

## Key Code Locations in ROM
- **GPIO init cluster**: 0x14C800-0x14E200 (6.5KB, 100+ GPIO base refs)
- **GPIO driver**: 0xC0C00-0xC1900 (virtual 0xC8004xxx-0xC8006xxx)
- **ASSP layer**: 0xB9000-0xD0000 (base_rapu/assp/rapu/)

## Next Steps
1. Full disassembly of GPIO init cluster (0x14C800-0x14E200)
2. Read GPFSEL registers from running phone (need kernel access)
3. Cross-reference byte offsets with BCM2727 datasheet (if obtainable)
4. Compare with BCM2835 GPIO driver source in Linux kernel
