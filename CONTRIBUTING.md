# Contributing

Thanks for your interest in the Nokia E7 Linux project!

## How You Can Help

### Hardware Knowledge
If you have experience with any of these, we'd love to hear from you:
- **OMAP3630** — pad mux configuration, SDRC timings, peripheral details
- **Nokia BB5 platform** — boot process, service protocol, memory mapping
- **BCM2727 / VideoCore** — ISP registers, firmware, VLL interface
- **Symbian kernel** — platform security internals, LDD development
- **Nokia service tools** — Phoenix protocol, FBUS/Phonet, PM field access

### The 584-Byte Challenge
We're trying to read the OMAP3630 PADCONF registers (584 bytes at 0x48002030)
from a running Nokia E7. See the [PADCONF Quest](docs/wiki/PADCONF-Quest.md)
for the full story and 8+ methods we've tried. If you know how to read hardware
registers from a BB5 Symbian phone, you'll be our hero.

### Code Contributions
- **QEMU peripherals** — improve existing stubs, add new peripheral models
- **Device tree** — corrections to pin mappings, peripheral configuration
- **Kernel patches** — driver fixes for Nokia E7 hardware
- **Tools** — Phonet protocol implementation, memory dump utilities

## Getting Started

1. Fork the repo
2. Check the [wiki](docs/wiki/Home.md) for project context
3. Read through `lore/` for development history and known issues
4. Open an issue to discuss what you want to work on

## Reporting Issues

- Hardware corrections — if you spot wrong addresses, pin mappings, or chip IDs
- QEMU bugs — crashes, wrong register behavior, missing stubs
- Documentation errors — typos, outdated info, missing context

## Code Style

- QEMU code follows QEMU coding style
- Kernel code follows Linux kernel coding style
- Documentation in Markdown, keep it readable
- Session notes in `lore/daily/` — be honest about dead ends, they save others time

## Important Notes

- **Don't publish Nokia firmware or proprietary binaries** — keep RE findings
  as documentation, not redistribution
- **Two irreplaceable phones** — all changes are emulation-first, real hardware second
- **This is a research project** — correctness matters more than speed
