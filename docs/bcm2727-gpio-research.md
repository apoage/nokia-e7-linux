# BCM2727 GPIO Register Research

> **Note (2026-03-23):** Real HW is BCM2727B1 (ARM1176), not OMAP3630. See docs/critical-cpu-discovery.md

**Date**: 2026-03-25
**Purpose**: Collect all publicly available information about BCM2727/BCM2763 GPIO
pin configuration, GPFSEL registers, and pad mux to understand GENIO_INIT behavior.

## Executive Summary

The BCM2727 GPIO register layout is almost certainly **identical to BCM2835** (Raspberry Pi).
The key evidence chain:

1. BCM2708 = VideoCore IV silicon die (no ARM core)
2. BCM2835 = BCM2708 + ARM1176JZF-S (package-on-package)
3. BCM2763 = VideoCore IV mobile multimedia processor (BCM2727 successor)
4. BCM2727 = VideoCore III, predecessor to BCM2763
5. Beyond3D forum: "the 2835 is a VideoCore IV (2763) with an added ARM1176,
   the 2763 is a souped up 2727/VideoCore III (as seen in Nokia phones)"
6. The `gpio.h` from BCM2708 leaked source (VideoCore IV QPU driver) uses
   **bus address 0x7e200000** as GPIO base -- same as BCM2835 documentation

**Critical question**: Does BCM2727 (VideoCore III) use the same GPIO register
layout as BCM2708/BCM2835 (VideoCore IV)? Evidence strongly suggests YES for
the basic GPFSEL/GPSET/GPCLR/GPLEV registers, since these are part of the
VideoCore peripheral block that predates the ARM core addition.

## Architecture Lineage

```
VideoCore II  (BCM2722)   -- early mobile multimedia
    |
VideoCore III (BCM2727)   -- Nokia N8/E7/C7, 720p, ARM11 *external*
    |
VideoCore IV  (BCM2763)   -- Nokia 808/701, 1080p, ARM11 *external*
    |
VideoCore IV  (BCM2708)   -- silicon die, no ARM, same peripherals as BCM2763
    |
VideoCore IV  (BCM2835)   -- BCM2708 + ARM1176 (Raspberry Pi 1)
VideoCore IV  (BCM2836)   -- BCM2709 + quad Cortex-A7 (Raspberry Pi 2)
VideoCore IV  (BCM2837)   -- BCM2710 + quad Cortex-A53 (Raspberry Pi 3)
```

### Key Insight: BCM2708 = BCM2835 without ARM

From Raspberry Pi Stack Exchange: "BCM2708 refers to the main part of the SoC
(VideoCore 4 + peripherals) without an ARM core. The BCM2835/6/7 were then
constructed by adding an ARM subsystem (ARM11, quad-A7, or quad-A53
respectively) to the BCM2708."

This means the peripheral register layout (GPIO, UART, I2C, SPI, etc.) is
defined by the VideoCore die, NOT by the ARM core. The ARM core is a bolt-on.

### BCM2727 vs BCM2835: What Changed?

- VideoCore III -> VideoCore IV: GPU upgraded (4-6x performance increase)
- BCM2727: external ARM11 CPU via TI (Samsung package-on-package)
- BCM2835: integrated ARM1176JZF-S on-die
- Peripherals (GPIO, I2C, SPI, UART): **likely same register layout**
  - The `gpio.h` from BCM2708 source matches BCM2835 documentation exactly
  - Both use 0x7e200000 bus base, 3-bit FSEL fields, same register offsets
  - This peripheral IP block was carried forward from VideoCore III

## BCM2708 GPIO Register Map (from leaked source)

Source: `shacharr/videocoreiv-qpu-driver` on GitHub
File: `brcm_usrlib/dag/vmcsx/vcinclude/bcm2708_chip/gpio.h`

### Base Address
- **VC bus**: 0x7e200000
- **ARM physical (BCM2835)**: 0x20200000
- **ARM physical (BCM2727)**: UNKNOWN -- depends on ARM-side memory map
- **APB ID**: 0x6770696f ("gpio" in ASCII)

### Function Select Registers (GPFSEL)

Each register covers 10 GPIO pins, 3 bits per pin.
FSEL values: 0=INPUT, 1=OUTPUT, 2-7=ALT functions (ALT0-ALT5).

| Register   | Address      | Pins    |
|-----------|-------------|---------|
| GP_FSEL0  | 0x7e200000  | 0-9     |
| GP_FSEL1  | 0x7e200004  | 10-19   |
| GP_FSEL2  | 0x7e200008  | 20-29   |
| GP_FSEL3  | 0x7e20000c  | 30-39   |
| GP_FSEL4  | 0x7e200010  | 40-49   |
| GP_FSEL5  | 0x7e200014  | 50-59   |
| GP_FSEL6  | 0x7e200018  | 60-69   |

**Note**: BCM2708 has FSEL0-FSEL6 (70 pins), BCM2835 doc shows FSEL0-FSEL5 (54 pins).
The extra FSEL6 register suggests the VideoCore die supports up to 70 GPIOs.
BCM2727 may expose a different subset than BCM2835.

### Output Set/Clear/Level Registers

| Register   | Address      | Width  | Function                    |
|-----------|-------------|--------|----------------------------|
| GP_SET0   | 0x7e20001c  | 32-bit | Set output pins 0-31       |
| GP_SET1   | 0x7e200020  | 32-bit | Set output pins 32-63      |
| GP_SET2   | 0x7e200024  | 6-bit  | Set output pins 64-69      |
| GP_CLR0   | 0x7e200028  | 32-bit | Clear output pins 0-31     |
| GP_CLR1   | 0x7e20002c  | 32-bit | Clear output pins 32-63    |
| GP_CLR2   | 0x7e200030  | 6-bit  | Clear output pins 64-69    |
| GP_LEV0   | 0x7e200034  | 32-bit | Read level pins 0-31 (RO)  |
| GP_LEV1   | 0x7e200038  | 32-bit | Read level pins 32-63 (RO) |
| GP_LEV2   | 0x7e20003c  | 6-bit  | Read level pins 64-69 (RO) |

### Event Detect / Edge Detect Registers

| Register   | Address      | Function                         |
|-----------|-------------|----------------------------------|
| GP_EDS0   | 0x7e200040  | Event detect status 0-31 (W1C)   |
| GP_EDS1   | 0x7e200044  | Event detect status 32-63        |
| GP_EDS2   | 0x7e200048  | Event detect status 64-69        |
| GP_REN0   | 0x7e20004c  | Rising edge enable 0-31          |
| GP_REN1   | 0x7e200050  | Rising edge enable 32-63         |
| GP_REN2   | 0x7e200054  | Rising edge enable 64-69         |
| GP_FEN0   | 0x7e200058  | Falling edge enable 0-31         |
| GP_FEN1   | 0x7e20005c  | Falling edge enable 32-63        |
| GP_FEN2   | 0x7e200060  | Falling edge enable 64-69        |
| GP_HEN0   | 0x7e200064  | High level detect enable 0-31    |
| GP_HEN1   | 0x7e200068  | High level detect enable 32-63   |
| GP_HEN2   | 0x7e20006c  | High level detect enable 64-69   |
| GP_LEN0   | 0x7e200070  | Low level detect enable 0-31     |
| GP_LEN1   | 0x7e200074  | Low level detect enable 32-63    |
| GP_LEN2   | 0x7e200078  | Low level detect enable 64-69    |
| GP_AREN0  | 0x7e20007c  | Async rising edge enable 0-31    |
| GP_AREN1  | 0x7e200080  | Async rising edge enable 32-63   |
| GP_AREN2  | 0x7e200084  | Async rising edge enable 64-69   |
| GP_AFEN0  | 0x7e200088  | Async falling edge enable 0-31   |
| GP_AFEN1  | 0x7e20008c  | Async falling edge enable 32-63  |
| GP_AFEN2  | 0x7e200090  | Async falling edge enable 64-69  |

### Pull-Up/Down Control

| Register     | Address      | Function                    |
|-------------|-------------|----------------------------|
| GP_PUD      | 0x7e200094  | Pull-up/down control (2-bit)|
| GP_PUDCLK0  | 0x7e200098  | PUD clock pins 0-31         |
| GP_PUDCLK1  | 0x7e20009c  | PUD clock pins 32-63        |
| GP_PUDCLK2  | 0x7e2000a0  | PUD clock pins 64-69        |

PUD values: 0=off, 1=pull-down, 2=pull-up.
Programming sequence: write PUD, wait 150 cycles, write PUDCLK, wait, clear.

### Schmitt Enable / Test / JTAG

| Register     | Address      | Function                         |
|-------------|-------------|----------------------------------|
| GP_SEN0     | 0x7e2000a4  | Schmitt enable 0-31 (reset=0xFFFFFFFF) |
| GP_SEN1     | 0x7e2000a8  | Schmitt enable 32-53 (22-bit)    |
| GP_GPTEST   | 0x7e2000b0  | Test register (4-bit)            |
| GP_AJBCONF  | 0x7e2000c0  | AJB (ARM JTAG bridge) config     |
| GP_AJBTMS   | 0x7e2000c4  | AJB TMS                          |
| GP_AJBTDI   | 0x7e2000c8  | AJB TDI                          |
| GP_AJBTDO   | 0x7e2000cc  | AJB TDO                          |

**GP_SEN (Schmitt enable)**: Not in BCM2835 public docs! This register enables
Schmitt trigger inputs per GPIO pin. Reset value 0xFFFFFFFF = all enabled.
This may be relevant to GENIO_INIT pad configuration.

**GP_AJBCONF**: ARM JTAG Bridge -- allows GPIO pins to be used for JTAG.
Not in BCM2835 public docs either. Potentially useful for debug.

## BCM2708 Peripheral Map (VideoCore bus addresses)

From `hermanhermitage/videocoreiv` wiki and `register_map.h`:

| Address      | Peripheral              |
|-------------|------------------------|
| 0x7e000000  | Multicore Sync Block    |
| 0x7e001000  | CCP2 TX                 |
| 0x7e002000  | VC Interrupt Controller |
| 0x7e003000  | System Timers           |
| 0x7e006000  | MPHI (Host Interface)   |
| 0x7e007000  | DMA 0-14                |
| 0x7e00b000  | ARM Control Block       |
| 0x7e100000  | Power Management        |
| 0x7e101000  | Clock Management        |
| 0x7e200000  | **GPIO**                |
| 0x7e201000  | UART0 (PL011)           |
| 0x7e202000  | Legacy MMC              |
| 0x7e203000  | PCM/I2S                 |
| 0x7e204000  | SPI Master              |
| 0x7e205000  | BSC0 (I2C0)             |
| 0x7e215000  | Aux (UART1/SPI1/SPI2)   |
| 0x7e300000  | eMMC                    |
| 0x7e804000  | BSC1 (I2C1)             |
| 0x7e805000  | BSC2 (I2C2/HDMI)        |
| 0x7e980000  | USB                     |
| 0x7ee00000  | SDRAM Controller        |

### Interrupt Map (from `hardware.h`)

GPIO has 4 interrupt lines in the VideoCore interrupt controller:
- INTERRUPT_GPIO0 = HW offset + 49
- INTERRUPT_GPIO1 = HW offset + 50
- INTERRUPT_GPIO2 = HW offset + 51
- INTERRUPT_GPIO3 = HW offset + 52

These correspond to GPIO banks (pins 0-15, 16-31, 32-47, 48-63).

## Nokia E7 Specifics

### Hardware Architecture
- **Application processor**: BCM2727IFBG (Broadcom, VideoCore III)
  - Contains ARM1176JZF-S core (CPUID 0x410fb764) -- confirmed by SuperPage dump
  - **OR** the ARM11 is in a separate TI die (Samsung PoP package)
  - AnandTech N8 teardown: "Samsung memory package integrates DDR memory (256MB),
    NAND (512MB) and a CPU (TI ARM11 applications processor)"
  - This creates ambiguity: is the ARM11 in the BCM2727 or in the TI package?
- **Modem**: RAPUYAMA D1800 (baseband/CMT only)
- **Memory**: 256MB LPDDR (Samsung K5W8G1GACK-AL56)

### GENIO_INIT and GPIO

Nokia's "GENIO" pins are the BCM2727's GPIO pins. GENIO_INIT is the firmware
blob that configures pad mux (GPFSEL values) during early boot.

From previous project analysis:
- GENIO_INIT is stored encrypted in NAND (at 0x01200000)
- Decryption requires TrustZone AES keys (blocked)
- When decrypted, it writes GPFSEL values to configure each pin's function
- Without correct GENIO_INIT, all pins are in default (input) mode

### What GENIO_INIT Likely Does

Based on BCM2835 GPIO documentation and the E7 schematic signals:

1. Sets GPFSEL for I2C pins (SDA/SCL) to ALT0 (BSC function)
2. Sets GPFSEL for SPI pins (MOSI/MISO/CLK/CS) to ALT0 (SPI function)
3. Sets GPFSEL for UART pins (TX/RX) to ALT0 (UART function)
4. Sets GPFSEL for camera interface pins to appropriate ALT function
5. Sets GPFSEL for display interface (DSI) pins
6. Sets GPFSEL for HDMI interface pins
7. Configures pull-up/down for each pin via GP_PUD/GP_PUDCLK
8. May configure Schmitt enable via GP_SEN registers

### Known Nokia E7 GPIO Signals (from schematic/service manual)

These are the signal names; the actual BCM2727 GPIO pin numbers are unknown
without GENIO_INIT or schematic detail:

- GPIO2=vol_up, GPIO3=prox_int, GPIO12=AccInt
- GPIO26=Hall, GPIO36=Capture, GPIO40=mag_DRDY
- GPIO41=mag_INT, GPIO44=AccInt2, GPIO46=GPS_CLK_REQ
- GPIO65=vol_down, GPIO105=IHF_AMP_EN
- GPIO122=touch_reset, GPIO123=touch_int

**WARNING**: These GPIO numbers may be OMAP3630 numbering from earlier (wrong)
assumption. They need re-validation against BCM2727 pin numbering.

## Comparison: BCM2835 vs BCM2727

| Feature              | BCM2835              | BCM2727 (probable)      |
|---------------------|----------------------|------------------------|
| VideoCore gen       | IV                   | III                    |
| ARM core            | ARM1176 (on-die)     | ARM1176 (on-die or PoP)|
| GPIO base (VC bus)  | 0x7e200000           | 0x7e200000 (assumed)   |
| GPIO pin count      | 54 (FSEL0-5)         | Up to 70 (FSEL0-6)?   |
| FSEL encoding       | 3-bit per pin        | 3-bit per pin (same)   |
| FSEL values         | 0=IN,1=OUT,2-7=ALT   | Same (assumed)         |
| SET/CLR/LEV         | Same offsets          | Same offsets (assumed) |
| PUD control         | Sequence-based        | Same (assumed)         |
| Peripheral base ARM | 0x20000000           | UNKNOWN                |
| ALT function map    | Documented            | NOT documented         |

The **ALT function mapping** (which ALT number maps to which peripheral on
which pin) is the critical unknown. On BCM2835, ALT0 on GPIO14/15 = UART,
ALT0 on GPIO2/3 = I2C. BCM2727 will have DIFFERENT pin-to-function mappings
even if the register layout is the same.

## Potential Paths to GPIO Configuration

### Path 1: Dump GPFSEL from Running Symbian
Use a Symbian app to read the GPIO GPFSEL registers at runtime. This would
reveal the current pin mux configuration set by GENIO_INIT.

**Challenge**: Need to know the ARM physical base address for GPIO on BCM2727.
On BCM2835 it's 0x20200000. Could try common bases:
- 0x20200000 (BCM2835 style)
- 0x7e200000 (direct VC bus, may work if no remapping)
- Need to check what addresses NLoader/GENIO_INIT write to

### Path 2: Disassemble GENIO_INIT
Already attempted (Task 069). Blocked by TrustZone encryption.
Crypto stubs pass auth but produce garbage values.

### Path 3: UART Capture During Boot
Capture NLoader UART output during real hardware boot. NLoader prints
GENIO configuration messages. This would reveal pad mux values.

### Path 4: Read BCM2835 Peripherals Doc + N8 Schematic
Cross-reference BCM2835 GPIO ALT function table with N8/E7 schematic
signal names. If signal routing matches, can infer GPFSEL values.

### Path 5: Linux GPIO Driver
Use `pinctrl-bcm2835` Linux driver as starting point. The register
layout is compatible. Only the ALT function mapping differs.

## Source Files Downloaded

The full BCM2708 `gpio.h` (735 lines) has been saved to:
`/tmp/bcm2708_gpio.h`

This file contains every register definition for the BCM2708 GPIO block.
It was generated by Broadcom's internal `create_regs` script and leaked
via the VideoCore IV QPU driver source code.

Repository: https://github.com/shacharr/videocoreiv-qpu-driver
Path: `brcm_usrlib/dag/vmcsx/vcinclude/bcm2708_chip/gpio.h`

Other useful files in the same directory:
- `hardware.h` -- interrupt definitions, DMA sources
- `register_map.h` -- includes all peripheral register headers
- 113 total header files covering all BCM2708 peripherals

## Nokia N8/E7 on postmarketOS

The Nokia N8 (nokia-vasco) is listed on postmarketOS wiki but marked as
"not booting" with "pmOS kernel: none". No Linux port exists for any
BCM2727-based Nokia device. The armhf architecture may be dropped by
Alpine Linux.

## BCM2727 Datasheet

A partial datasheet exists at:
- http://datasheet.elcodis.com/pdf2/106/8/1060897/bcm2727.pdf
- https://elcodis.com/parts/6168465/bcm2727.html

Features listed: GPIO, SPI, Power Supply interfaces, VideoCore III
Multimedia Engine, 32MB stacked SDRAM, 12MP ISP.

Full register-level documentation is NOT publicly available.
Broadcom never published a BCM2727 equivalent of the BCM2835 ARM
Peripherals document.

## Key Sources

- BCM2835 ARM Peripherals: https://www.raspberrypi.org/app/uploads/2012/02/BCM2835-ARM-Peripherals.pdf
- BCM2708 GPIO header: https://github.com/shacharr/videocoreiv-qpu-driver/blob/master/brcm_usrlib/dag/vmcsx/vcinclude/bcm2708_chip/gpio.h
- BCM2708 register map: https://github.com/hermanhermitage/videocoreiv/wiki/MMIO-Register-map
- VideoCore IV register docs: https://github.com/hermanhermitage/videocoreiv/wiki/Register-Documentation
- Beyond3D BCM2727 thread: https://forum.beyond3d.com/threads/broadcom-gpu-in-nokia-n8.50248/
- Beyond3D BCM2835 thread: https://forum.beyond3d.com/threads/raspberry-pi-and-bcm2835.51889/
- BCM2708/BCM2835 naming: https://raspberrypi.stackexchange.com/questions/840/why-is-the-cpu-sometimes-referred-to-as-bcm2708-sometimes-bcm2835
- AnandTech N8 review (SoC): https://www.anandtech.com/show/4126/nokia-n8-review-/3
- N8FanClub BCM2727 review: https://www.n8fanclub.com/2011/02/review-of-nokia-n8-broadcom-bcm2727-gpu.html
- Nokia N8 schematics: https://bbs.aw-ol.com/assets/uploads/files/1631158689295-nokia-n8.pdf
- Nokia E7 schematics: https://elektrotanya.com/nokia_e7-00_rm-626_schematics_v1.0_sch.pdf/download.html
- postmarketOS Nokia N8: https://wiki.postmarketos.org/wiki/Nokia_N8_(nokia-vasco)
- Broadcom BCM2763 product: https://www.broadcom.com/products/broadband/reference-design/bcm2763
- BCM2727 datasheet stub: http://www.datasheetdir.com/BCM2727+Multimedia
- Reverse costing teardown: https://www.reverse-costing.com/teardowns/components/broadcom_bcm2727/

## Conclusions

1. **GPIO register layout is almost certainly BCM2835-compatible**. The VideoCore
   peripheral block (including GPIO) is shared across the BCM27xx family. The
   BCM2708 leaked source confirms identical register offsets.

2. **The ARM physical base address is the critical unknown**. On BCM2835 it's
   0x20200000. On BCM2727 it could be different. Reading GENIO_INIT writes or
   dumping from Symbian would reveal this.

3. **ALT function mapping is chip-specific**. Even with the same GPFSEL register
   layout, which ALT function number maps to which peripheral on which pin
   differs between BCM2727 and BCM2835. This can only be determined by:
   - Reading the GENIO_INIT configuration
   - UART capture during boot
   - Reading GPFSEL values from running Symbian
   - Cross-referencing with the N8/E7 schematic

4. **The `pinctrl-bcm2835` Linux driver should work** for basic GPIO on BCM2727,
   once the ARM physical base address is known. The register protocol (3-bit
   FSEL, W1S SET, W1C CLR, RO LEV) is identical.

5. **No public documentation exists for BCM2727 register-level details**.
   Broadcom never released a BCM2727 ARM Peripherals document. The only
   register information comes from the BCM2708 leaked source (VideoCore IV)
   and the BCM2835 public documentation, both of which are for the successor.

6. **BCM2727 has up to 70 GPIO pins** (FSEL0-FSEL6), compared to BCM2835's 54.
   The extra pins (54-69) may be used for Nokia-specific GENIO signals.
