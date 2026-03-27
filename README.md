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
- **QEMU emulation** boots Linux to shell in ~1.4s (synthetic OMAP3630 model, not HW-accurate)
- **Symbian toolchain** — Python scripts run on phone via PyS60 1.4.5
- **ISI/Phonet USB** — IMEI read, resource scanning, protocol documented
- **ROM dump analyzed** — 30MB Symbian ROM with 800+ lines of peripheral mapping
- **BCM2835 cross-reference** — GPIO, INTC, UART, I2C, SPI, USB addresses match Raspberry Pi
- **218 kernel SVCs cataloged** — full executive call interface mapped from ROM
- **peek/poke memory access** — `_hack.pyd` reads/writes user-space virtual memory

### Current Challenge
Reading GPIO Function Select registers (GPFSEL0-6, 28 bytes at physical 0x20200000) from the running phone. These contain the pin mux configuration needed for a Linux device tree. The registers are in kernel virtual address space (0xC8004xxx), inaccessible from user mode.

### Approaches Attempted
1. Custom Symbian exe — blocked by TCB capability (ROM-only)
2. FShell readmem — kernel panic (memoryAccess LDD needs TCB)
3. ISI protocol modem routing — modem resources unreachable from USB
4. RomPatcher+ patches — kernel LDDs installed but no programmatic control
5. `_hack.pyd` peek/poke — user-space only, heap is NX (no code execution)
6. ROP via Python function pointer redirect — in progress (218 SVCs cataloged)

### Next Steps
- **ROP exploit** — redirect Python function to ROM gadget that reads kernel memory
- **UART capture** — hardware debug via J2060 test pad (NLoader prints GPIO config at boot)
- **Real hardware DTS** — BCM2835-based device tree once GPIO config obtained

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

- **BCM2727B1 peripheral map matches BCM2835** (Raspberry Pi) — GPIO, INTC, UART, I2C, SPI, USB all at same base addresses in 0x20xxxxxx space
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
