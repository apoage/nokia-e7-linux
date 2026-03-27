# Nokia E7-00 Linux Project Wiki

> Bringing Linux to two Nokia E7-00 (RM-626) phones. Build a snappy handheld
> terminal with full networking, computational photography, and every ASIC
> exploited. Two irreplaceable units -- emulation first, real hardware second.

---

## Hardware: Broadcom BCM2727B1 (ARM1176JZF-S)

The Nokia E7's application processor is **Broadcom BCM2727B1** with an
**ARM1176JZF-S** core (ARMv6). This was confirmed by SuperPage CPUID
(0x410fb764), ROM dump string `bcm2727b1`, and community verification.

**NOT TI OMAP3630** -- the earlier assumption was incorrect. RAPUYAMA (D1800)
is the modem/CMT only. QEMU emulation uses synthetic OMAP3630.

### BCM2835 Compatibility (BREAKTHROUGH)
The peripheral register map at **0x20200000** matches the Raspberry Pi's BCM2835
almost perfectly -- GPIO, INTC, UART, I2C, SPI, USB, Timer, PWM, eMMC all at
the same base addresses. The BCM2835 ARM Peripherals datasheet serves as our
reference manual.

---

## Current Status (2026-03-26)

### Active: GPIO Register Read
Need to read GPFSEL0-6 (28 bytes at physical 0x20200000) from the running phone.
These contain the pin mux configuration set by GENIO_INIT at boot.

**Progress:**
- `_hack.pyd` found -- provides `peek(addr)` / `poke(addr, byte)` for user-space memory
- Heap is writable but NX (no code execution)
- ROM at 0x80000000 is readable (verified against RomPatcher+ dump)
- 218 kernel SVC instructions cataloged from ROM
- HAL::Get call chain traced into ROM Thumb code
- Python function pointer redirection confirmed (m_ml pointer writable)
- **Blocked:** GPIO at kernel VA 0xC8004xxx, not accessible from user mode

**Next:** ROP (return-oriented programming) using ROM gadgets, or UART hardware capture

### Completed
- BCM2727B1 SoC identification + BCM2835 peripheral map match
- 30MB ROM dump: 800-line peripheral analysis, 2380 driver strings
- BCM2708 gpio.h (70 GPIO pins, 7 GPFSEL registers)
- HAL dump: MachineUid, RAM, ROM, display, capabilities
- ISI/Phonet USB protocol: IMEI, resource scan, routing analysis
- Symbian toolchain: PyS60 scripts, exe building (currently broken)
- QEMU boots Linux to shell in ~1.4s

---

## Pages

| Page | Description |
|------|-------------|
| [Hardware Overview](Hardware-Overview.md) | BCM2727B1, VideoCore III, peripheral map |
| [PADCONF Quest](PADCONF-Quest.md) | GPIO/pin mux register exploration |
| [Research Sources](Research-Sources.md) | Community contacts, references |

## Key Files

- `docs/critical-cpu-discovery.md` -- CPU discovery details
- `docs/rom-dump-analysis.md` -- 800-line ROM peripheral map
- `docs/bcm2727-gpio-research.md` -- GPIO register layout
- `docs/phone-probes/svc-catalog.txt` -- 218 kernel SVCs
- `docs/phone-probes/` -- all live phone probe outputs
- `lore/daily/2026-03-26-peek-poke-exploit.md` -- exploit chain documentation
- `tools/nokia_isi.py` -- ISI/Phonet USB protocol tool
- `symbian-sdk/memread/pys60/` -- phone-side Python scripts
