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

Everything below is validated in QEMU emulation against the phone's own
firmware images unless marked **(phone-tested)**.

### What Works
- **The genuine BB5 boot chain executes end to end** (2026-09) — single
  SWBL-shaped map, the real NLoader prologue + scatter/LZ loader (real USB
  descriptors, 33-record ID table), init tree with EM-ASIC detection, NLoader's
  own TOC curation and MCUSW launch — through to a Linux shell (`e7:~#`);
  independent boots are byte-identical (1202-line canonical flow, guarded by
  `tools/chain-regress.sh`). See [bcm2727-onenand.md](docs/bcm2727-onenand.md)
  and [nloader-boot-mechanism.md](docs/nloader-boot-mechanism.md)
- **eMMC + SD storage, IRQ-driven** — custom Rapu_Memcard controller decoded
  (2 channels), QEMU model + Linux PIO host; ext4 boots as the real root,
  SD card mounts on channel 1 ([bcm2727-memcard.md](docs/bcm2727-memcard.md))
- **USB host live in the chain** — MUSBHDRC model (register file, host token
  engine, TUSB1210 ULPI, split IRQs) + `musb-bcm2727` Linux glue; usb-storage
  enumerates with vfat md5 round trips, usb-kbd works on the input layer
  ([bcm2727-usb-flashmode.md](docs/bcm2727-usb-flashmode.md))
- **Display (VideoCore) arc closed in emulation** — the whole protocol decoded:
  MeSSI transport (CRC-32C), DVCDriver boot exchange, dispman service grammar;
  MeSSI engine model + VC responder, Linux VCHI transport driver
  (`/dev/bcm2727-vchi`), and the `vc_session` client complete boot + update
  cycles riding real ROM firmware images
  ([bcm2727-display-recon.md](docs/bcm2727-display-recon.md))
- **Soft-UART console** — TX bit-bang 8N1 on genio42 ("TxCDa") timed by the frc
  counter; `console=ttySU0` boots run to `e7:~#` with the full rcS decoded on
  the wire ([bcm2727-softuart.md](docs/bcm2727-softuart.md))
- **Interrupt controller + timer** — ICU (421 ids) and tick timer decoded;
  Linux irqchip + clocksource drivers; chain boot self-calibrates lpj
  ([bcm2727-icu-timer.md](docs/bcm2727-icu-timer.md))
- **GPIO/pinmux recovered + irqchip live** — pad table (grid N = kernel N =
  schematic N), custom RAPU 7-bank SET/CLR design, no GPFSEL exists; edge
  irqchip chain-validated ([genio-padtable.md](docs/genio-padtable.md),
  [bcm2727-gpio-irq.md](docs/bcm2727-gpio-irq.md))
- **ARMv6 toolchain + buildroot rootfs** — own musl cross toolchain (gcc 11.2,
  soft-float) + buildroot; busybox/strace/i2c-tools rootfs built for ARM1176
- **Custom native exes run on the phone** **(phone-tested, 2026-08)** —
  unprotected-range UID + 4 platform deps recipe — see
  [tasks/090](tasks/090-O-fix-exe-build-armv6.md) and
  `symbian-sdk/memread/build_exe.sh`
- **WiFi remote bridge** **(phone-tested)** — remote Python exec, file
  push/pull, crash-isolated memory reads (`XPEEK`) from the PC; agent is
  PyS60 2.2.2 (Python 2.2 core!) and host-py3 compatible
- **ISI/Phonet USB** **(phone-tested)** — IMEI read, resource scanning,
  protocol documented
- **ROM dump analyzed** — 31MB Symbian ROM, user-readable live at 0x80000000;
  800+ line peripheral map ([rom-dump-analysis.md](docs/rom-dump-analysis.md))

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
- **USB probe round 3 on hardware** (passive-first) — full descriptor harvest
  across the 0106/0333/0335 windows, read-only SCSI structure; the 0106 vendor
  engage stays off
- **Hardware bring-up** — one wire on TxCDa for the console; eMMC silicon pins
  ([bcm2727-memcard.md](docs/bcm2727-memcard.md) §6.4)
- **Display on hardware** — the emulation/hardware boundary is mapped exactly
  ([bcm2727-display-recon.md](docs/bcm2727-display-recon.md) §13.4): firmware
  signature verification + execution, runtime captures to reconcile the
  responder contract
- **USB gadget mode** — the contactless console endgame once our Linux runs

## Repository Structure

```
e7/
├── emulation/          # qemu-e7sw: the faithful BB5-chain machine (OneNAND,
│                       #   MUSB, MeSSI/VCHI, ICU, memcard, soft-UART models)
│                       #   + buildroot rootfs build
├── kernel/             # bcm2727 kernel patches (mach, pinctrl, VCHI char
│                       #   driver, MUSB glue), DTS + defconfig
├── docs/               # Reverse-engineering notes: boot chain, OneNAND,
│   │                   #   display/VideoCore, USB, memcard, soft-UART,
│   │                   #   ICU/timer, GPIO/pinmux, ROM analysis, probes
│   ├── phone-probes/   # Live phone probe outputs
│   └── wiki/           # Project wiki pages
├── tools/              # vc_session client, chain-regress harness, bridge_pc,
│                       #   ISI/Phonet tools, ROM/E32 analysis scripts
├── symbian-sdk/        # Symbian cross-compilation + phone-side probing
│   └── memread/        # peek/patcher probes, PyS60 bridge scripts
├── lore/               # Session notes, debugging journal
│   └── daily/          # Chronological session logs
└── tasks/              # Work items with state tracking (090-127)
```

Raw firmware dumps and vendor SDK trees are kept out of the repository by
design; analyses reference them, the binaries are not redistributed.

## Key Findings

- **BCM2727B1 peripheral base addresses sit in the BCM2835 0x20xxxxxx space, but the blocks are NOT BCM2835-layout** — custom RAPU designs throughout (7-bank SET/CLR GPIO with no GPFSEL, custom ICU, custom memcard host); see [gpio-static-pinmux.md](docs/gpio-static-pinmux.md)
- **The boot chain is decoded and executes** — SWBL → NLoader (scatter/LZ loader, TOC curation) → MCUSW → Linux; the "Symbian bootloader uncracked" era is over in emulation, hardware bring-up is the remaining step
- **Display is VC-mediated** — keep-the-firmware + a Linux VCHI host is the viable path; the full protocol stack is decoded and proven in-chain
- **Symbian never programs pin functions** (SetPinMode is a stub) — muxes are set pre-boot by the NLoader / GENIO_INIT / TrustZone / VideoCore chain
- **Nokia platform**: RAPU (RapidoVariant), hw79, codename Gazoo
- **34 clock domains**, 7 GPIO banks, MUSB OTG USB, Messi+CCP2 bus to VideoCore
- **Heap is NX** (ARM1176 XN bit enforced) — no code execution from writable memory

## Community

- Telegram: Symbian World group (zaiood, karaba abdu confirmed CPU details)
- GitHub: [apoage/nokia-e7-linux](https://github.com/apoage/nokia-e7-linux)

## License

Research project. QEMU patches and tools are GPL-compatible. Symbian SDK components retain their original licenses.
