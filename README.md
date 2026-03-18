# Nokia E7-00 Linux Project

> *"Nokia is dead, nobody cares about old Symbian junk"* — some guy on a forum, 2018
>
> *Hold my soldering iron.*

Bringing Linux to the Nokia E7-00 (RM-626) — a 2011 Symbian smartphone with a
full slide-out QWERTY keyboard, 4" AMOLED display, and more silicon than most
people realize. Two irreplaceable units. Zero documentation. Maximum stubbornness.

The phone Nokia killed too soon deserves a proper OS. We're giving it one.

**Status: Work in Progress** — QEMU boots to shell in 1.4 seconds,
the real phone is putting up a fight (TrustZone, platform security, encrypted
bootloader — the usual Nokia hospitality).

## What's Here

### QEMU Machine (`emulation/`)
Custom QEMU machine type `nokia-e7` emulating the OMAP3630 SoC and E7 peripherals.
Currently boots Linux 6.12 to an interactive shell in ~1.4 seconds.

**Emulated hardware:**
- OMAP3630 (Cortex-A8 1GHz) with INTC, GPIO (6 banks), sDMA (32 channels)
- TWL5031 PMIC (I2C, 4-slave register model, RTC, USB PHY, watchdog, audio codec)
- LM8323 keyboard controller (full key→FIFO→IRQ→input pipeline)
- I2C sensors: LIS302DL accelerometer, AK8974 magnetometer, APDS990x proximity/ALS
- Atmel mXT touchscreen, BQ2415x charger, LP5521 LED controller
- GPMC with OneNAND model (Samsung 8Gbit MLC, full command state machine)
- HSMMC2 eMMC model, CM/PRM clock/power domain tracking
- DSS display model, I2C1/2/3 controllers

> **Note:** This is very much a work in progress. Many peripherals are stubs that
> return just enough to keep the kernel happy. The OMAP3630 model (`omap3630.c`)
> is a single ~15K line file that will be refactored as it matures.

### Kernel (`kernel/`)
- `configs/nokia_e7_defconfig` — minimal 98-line defconfig
- `dts/omap3630-nokia-e7.dts` — ~690 line device tree, all I2C devices probe

### Documentation (`docs/`)
- `hardware-lore.md` — master chip inventory (49 ICs)
- `qemu-machine-design.md` — SoC emulation specification
- `emulation-strategy.md` — emulate-first approach
- `undocumented-*.md` — deep dives into undocumented ASICs
- `knowledge-graph/` — structured JSON hardware knowledge base
  - 49 components, 22 buses, 226 signals, 17 power rails, 32 flash partitions
  - Mermaid diagrams: system block, power tree, I2C topology, boot sequence

### Session Notes (`lore/`)
Chronological development log documenting the journey from first boot attempt
to working emulation, including dead ends, debugging techniques, and discoveries.

## The Phone

The Nokia E7-00 is a 2011 Symbian^3 smartphone built around:

| Component | Details |
|-----------|---------|
| SoC | OMAP3630 (ARM Cortex-A8 @ 1GHz) in RAPUYAMA D1800 package |
| RAM | 256MB LPDDR (Samsung K5W8G1GACK) |
| PMIC | TWL5031 (TI) |
| GPU | PowerVR SGX530 |
| ISP | BCM2727 (Broadcom VideoCore) |
| Display | 640x360 nHD AMOLED |
| Keyboard | Full slide-out QWERTY (LM8323 controller) |
| Flash | 1GB OneNAND + 16GB eMMC |
| Sensors | Accelerometer, magnetometer, proximity, ambient light, touch |
| Connectivity | WiFi (SPI), BT, GPS, NFC (PN544), 3G modem |

Two units available — Unit A (development target) and Unit B (stock reference/rescue).

## Quick Start

```bash
# Build kernel
cd kernel/linux  # (not included — use a shallow clone of Linux 6.12)
make ARCH=arm CROSS_COMPILE=arm-linux-gnueabihf- nokia_e7_defconfig
make ARCH=arm CROSS_COMPILE=arm-linux-gnueabihf- -j$(nproc) zImage dtbs

# Build QEMU (apply omap3630.c to qemu source)
cd qemu && ninja -C build

# Boot (initramfs)
qemu-system-arm -M nokia-e7 -m 256M -kernel zImage \
  -dtb omap3630-nokia-e7.dtb -initrd initramfs.cpio.gz \
  -append "earlycon clk_ignore_unused rdinit=/init" -nographic

# Boot (ext4 root)
qemu-system-arm -M nokia-e7 -m 256M -kernel zImage \
  -dtb omap3630-nokia-e7.dtb \
  -drive file=rootfs.ext4,if=none,format=raw,id=hd0,cache.no-flush=on \
  -device virtio-blk-device,drive=hd0 \
  -append "earlycon clk_ignore_unused root=/dev/vda rw init=/init" -nographic
```

## Project Goals

1. Boot a snappy Linux terminal on the E7's hardware
2. Full networking (WiFi, cellular data)
3. Exploit every ASIC (camera ISP, GPU, audio, sensors)
4. Emulation-first development — nothing hits real hardware without QEMU validation

## Current Adventures

We've been through quite a journey trying to read 584 bytes of pad mux
registers from the running phone:

- Built a working Symbian toolchain from scratch (GCC + Docker + CRT startup discovery)
- Reverse-engineered Nokia's MemSpy kernel driver (191 imports, 168 debug strings — no physical memory access)
- Mapped 250 MemSpy opcodes (crashed the phone 6 times in the process)
- Enumerated 200 running threads and 25 kernel drivers on the live phone
- Installed FShell and discovered the `memoryAccess` LDD is loaded but requires TCB capability
- Learned that TCB is ROM-only on Symbian — no amount of certificate hacking can grant it
- Searched through 160,000 Telegram messages from three Symbian communities
- Analyzed a 30MB ROM dump looking for the PlatSec check to NOP out
- Are now reverse-engineering Nokia's USB service protocol because apparently that's what it takes to read a register

The 584 bytes remain unread. The phone remains defiant. We remain stubborn.

## Contributing

This is a personal research project on irreplaceable hardware. If you have
Nokia E7 hardware knowledge, OMAP3630 experience, Symbian kernel internals,
or you know what CapsSwitch actually does — open an issue or reach out.

Especially interested in hearing from anyone who's gotten `readmem` working
on FShell, or knows the Phonet/FBUS protocol for raw memory access.

## License

Documentation and original code: MIT.
Kernel patches follow Linux kernel licensing (GPL-2.0).
QEMU machine code follows QEMU licensing (GPL-2.0).
