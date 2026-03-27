# 2026-03-23: The BCM2727 Revelation

## The Day Everything Changed

What started as "let's read PADCONF" turned into the most important
discovery of the entire project: **the Nokia E7 is NOT OMAP3630.**

## Timeline

### ISI Name Service Routing Probe
- Built `isi_nameservice_probe.py` using AF_PHONET sockets
- Tested modem routing via device 0x00 (Name Service translation)
- Results: modem resources all NOT_REACHABLE or NOT_SUPPORTED
- PN_GSM_RF_TEST (0xF1) crashed the phone
- Conclusion: ISI modem routing is a dead end from USB

### RomPatcher+ LDD Discovery
- Found all 3 patcher LDDs in archive-dump: patcher.ldd, patcherS3.ldd, patcherShadow.ldd
- All have caps=ALL+TCB (0x000FFFFF) -- manufacturer-signed
- RomPatcherPlus.exe has ZERO capabilities but works -- LDD doesn't check caller caps
- RomPatcher+ v3.1 features: Dump ROM, Dump SuperPage, +SuperPage patch, SnR patches

### Exe Execution Broken
- All exes return error 3 (including previously working test.exe)
- Tried: zero caps, TCB caps, docker build, different UIDs
- Root cause unclear -- possibly cert store issue or phone state
- SafeManager cert store trick from EasyHackKit didn't fix it

### The CPU Discovery
- Asked Telegram community about PADCONF
- Zaiood: "RAPUYAMA and OMAP are unrelated"
- Checked SuperPage dump: **CPUID = 0x410fb764 = ARM1176JZF-S (ARMv6)**
- OMAP3630 would show Cortex-A8 (part 0xC08) -- this is ARM1176 (part 0xB76)
- karaba abdu confirmed: "ARM1176JZF-S made by Samsung"
- Matthew: "nobody has cracked the bootloader yet"

### ROM Dump Deep Analysis
- 30.3MB ROM dump analyzed with 3 parallel agents
- **BCM2727B1** string found -- confirms Broadcom SoC revision
- **VideoCore III** GPU/ISP with VCHI host interface
- Nokia platform: RAPU (RapidoVariant), hw79, codename "Gazoo"
- 62 Nokia source file paths from Y:/ncp_sw/ debug strings
- Source tree: base_rapu/assp/rapu/ (ASSP layer)
- Physical device drivers: accelerometer.pdd, magnetometer.pdd, dipro.pdd, LightDrv.pdd
- 34 clock domains from EHwClk enumeration
- GPIO at kernel VA 0xC8004xxx-0xC8006xxx (7 banks)
- No pin mux strings -- BCM2727 may use fixed or VC firmware pin config

### BCM2835 Cross-Reference BREAKTHROUGH
- Compared E7 ROM 0x20xxxxxx addresses with BCM2835 (Raspberry Pi) peripheral map
- **Near-perfect match**: GPIO, INTC, UART, I2C, SPI, USB, Timer, PWM, eMMC all at same addresses
- Raspberry Pi BCM2835 public datasheet is our reference manual
- BCM2727 is direct predecessor to BCM2835 -- shared peripheral IP

### Documentation Overhaul
- Corrected OMAP3630 misinformation across 15+ active files
- Updated CLAUDE.md, wiki, hardware-lore, emulation docs
- 11 commits pushed to GitHub

## Key Artifacts Created
- `docs/critical-cpu-discovery.md` -- full CPU analysis
- `docs/rom-dump-analysis.md` -- 800+ line peripheral map
- `docs/rom-driver-strings.md` -- 2380 lines categorized HW strings
- `docs/rom-file-inventory.md` -- ROM file structure
- `docs/bcm2727-gpio-research.md` -- BCM2708 GPIO header from GitHub
- `docs/bcm2708-gpio.h` -- 735-line register header
- `docs/gpio-register-discovery.md` -- physical GPIO at 0x20200000
- `tools/isi_nameservice_probe.py` -- ISI routing test tool

## Impact
The entire OMAP3630 assumption was wrong. But we gained something better:
a well-documented reference SoC (BCM2835/Raspberry Pi) whose peripheral
map matches our phone. The path to Linux is harder (Broadcom is closed)
but not impossible.
