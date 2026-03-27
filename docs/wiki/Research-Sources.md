# Research Sources

## Community Confirmations

### CPU Discovery (2026-03-23)

**zaiood** (Telegram): Stated "RAPUYAMA and OMAP are unrelated" -- this was
confirmed correct by our SuperPage CPUID analysis showing the app processor
is ARM1176JZF-S (0x410fb764), not Cortex-A8.

**karaba abdu** (community): Confirmed the ARM1176 in the Nokia E7 is
Samsung-manufactured.

**General community consensus**: "Nobody has cracked the bootloader yet" for
the Nokia E7/N8 family. The Symbian bootloader chain (SWBL -> NLoader -> SOS)
is non-standard and uses TrustZone-based encryption.

### Symbian Internals

**symbuzzer**: CapsSwitch/PlatSec tool author -- runtime platsec disable via
RomPatcher LDD.

**Max Bondarchenko (iCrazyDoctor)**: Key expert on Symbian kernel internals.

**Symbian World chat**: Analysis of community knowledge on kernel access,
RomPatcher LDDs, and capability bypass. See `memory/symbian-world-chat.md`.

## Key References

### Nokia E7 / N8 Hardware
- SuperPage dump: `docs/superpage.dmp` -- contains CPUID, memory layout
- Critical CPU discovery: `docs/critical-cpu-discovery.md`
- Hardware lore: `docs/hardware-lore.md` (partially outdated -- SoC field incorrect)
- Knowledge graph: `docs/knowledge-graph/` (9 JSON files, 4 Mermaid diagrams)
- Service manual: `docs/service-manual/` (7 files including Service Hints v1.0)
- DCP factory calibration: `docs/dcp-analysis.md`

### Firmware Analysis
- NLoader analysis: `docs/nloader-analysis.md`
- Flash memory map: `docs/flash-memory-map.md`
- Boot modes: `docs/boot-modes.md`
- Nokia Cooker keys: `docs/nokia-cooker-analysis.md`
- BCM2727 firmware: `docs/bcm2727-firmware-analysis.md`
- VideoCore reference: `docs/videocore-reference-analysis.md`

### Symbian SDK / Tools
- Symbian^3 SDK: Archive.org
- Kernel source: github.com/SymbianSource
- FShell v5.00: installed on Unit B
- Docker `symbian-sdk` image: `arm-none-symbianelf-gcc` 12.1.0 + full SDK

### Related Devices
- Nokia N8: same BCM2763 application processor family
- Nokia N950/N9: use TI OMAP3630 (different platform, useful DTS reference for QEMU only)

## Session Logs

Chronological session notes in `lore/daily/`. Key entries:
- `2026-03-08-nloader-full-boot.md` -- NLoader QEMU boot reaching GENIO config
- `2026-03-23` session -- CPU discovery and community confirmation
