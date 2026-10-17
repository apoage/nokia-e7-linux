# Nokia E7-00 Linux

Bringing Linux to the Nokia E7-00 (RM-626) — a 2011 Symbian^3 smartphone with a slide-out QWERTY keyboard, 4" nHD AMOLED display, and 8MP camera.

## Hardware

| Component | Details |
|-----------|---------|
| **SoC** | Broadcom BCM2727B1 (ARM1176JZF-S, ARMv6) |
| **GPU/ISP** | VideoCore III (same family as Raspberry Pi's BCM2835) |
| **RAM** | 256MB LPDDR (Samsung K5W8G1GACK) |
| **Display** | 640x360 nHD AMOLED |
| **Storage** | OneNAND flash + eMMC |
| **Modem** | RAPUYAMA D1800 (cellular modem/CMT) |
| **PMIC** | TWL5031 (Nokia codename PEARL) |
| **Platform** | Nokia RAPU (hw79, codename Gazoo) |

> **Note:** The SoC was previously misidentified as TI OMAP3630. SuperPage CPUID analysis (0x410fb764) confirmed ARM1176JZF-S. The peripheral register map at 0x20200000 matches BCM2835 (Raspberry Pi), confirming the Broadcom lineage. See [critical-cpu-discovery.md](docs/critical-cpu-discovery.md).

## Status

### What Works
- **Custom native exes run on the phone** (2026-08): unprotected-range UID + 4 platform deps recipe — see [tasks/090](tasks/090-O-fix-exe-build-armv6.md) and `symbian-sdk/memread/build_exe.sh`
- **WiFi remote bridge** — remote Python exec, file push/pull, crash-isolated memory reads (`XPEEK`) from the PC; agent is PyS60 2.2.2 (Python 2.2 core!) and host-py3 compatible
- **QEMU emulation** boots Linux to shell in ~1.4s (synthetic OMAP3630 model, not HW-accurate)
- **ISI/Phonet USB** — IMEI read, resource scanning, protocol documented
- **ROM dump analyzed** — 31MB Symbian ROM; user-readable live at 0x80000000 (same image as `firmware-re/dumps/romdumpplus.dmp`)
- **GPIO controller decoded statically** — no GPFSEL exists; BCM2727 uses a custom RAPU 7-bank SET/CLR block; full register map in [gpio-static-pinmux.md](docs/gpio-static-pinmux.md)

### The Kernel-Memory Wall (mapped, not mysterious)
Every LDD on the device (file and ROM resident) carries All+TCB; TCB is
ROM-only; opening a channel with lesser caps = kernel panic. Runtime
kernel reads from Symbian user space are closed pending a real exploit.
Safe probing (peek.exe child) covers all of user space.

### Where the pinmux actually lives
Symbian never programs pin functions (SetPinMode is a stub) — they're
set pre-boot (NLoader / GENIO_INIT / TrustZone / VideoCore chain), the
same territory as the boot path for Linux.

### Next Steps
- **VideoCore second-stage loader** — pinmux source + boot mechanism in one target
- **Linux pinctrl driver + DTS** from the decoded RAPU GPIO map
- **NLoader path** (`firmware-re/analysis/nloader-patched.bin`) for the actual Linux boot

## Repository Structure

```
e7/
├── emulation/          # QEMU machine (synthetic OMAP3630) + rootfs
├── kernel/             # Linux 6.12 source + Nokia E7 defconfig
├── docs/               # Hardware analysis, ROM dump findings, research
│   ├── critical-cpu-discovery.md    # BCM2727B1 revelation
│   ├── rom-dump-analysis.md         # 800-line peripheral map
│   ├── bcm2727-gpio-research.md     # GPIO register layout
│   ├── phone-probes/                # Live phone probe outputs
│   └── wiki/                        # Project wiki pages
├── tools/              # ISI/Phonet protocol tools
├── symbian-sdk/        # Symbian cross-compilation + phone scripts
│   └── memread/pys60/  # Python scripts for phone-side probing
├── lore/               # Session notes, debugging journal
│   └── daily/          # Chronological session logs
├── tasks/              # Work items with state tracking
└── firmware-re/        # Firmware reverse engineering
```

## Key Findings

- **BCM2727B1 peripheral base addresses sit in the BCM2835 0x20xxxxxx space, but the blocks are NOT BCM2835-layout** — the GPIO is a custom RAPU design (7 banks, SET/CLR registers, no GPFSEL); see [gpio-static-pinmux.md](docs/gpio-static-pinmux.md)
- **BCM2708 gpio.h** found on GitHub — confirms register layout with 7 GPFSEL registers (70 GPIO pins)
- **Nokia platform**: RAPU (RapidoVariant), hw79, codename Gazoo
- **34 clock domains**, 7 GPIO banks, MUSB OTG USB, Messi+CCP2 bus to VideoCore
- **`_hack.pyd`** Python module provides byte-level memory read/write (user-space VA only)
- **Heap is NX** (ARM1176 XN bit enforced) — no code execution from writable memory
- **Bootloader uncracked** — community confirms no one has cracked Symbian bootloader

## Community

- Telegram: Symbian World group (zaiood, karaba abdu confirmed CPU details)
- GitHub: [apoage/nokia-e7-linux](https://github.com/apoage/nokia-e7-linux)

## License

Research project. QEMU patches and tools are GPL-compatible. Symbian SDK components retain their original licenses.
