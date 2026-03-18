# Nokia E7-00 (RM-626) Device Specification

## Overview

| Property | Value |
|----------|-------|
| Model | Nokia E7-00 |
| Type Number | RM-626 |
| Release | February 2011 |
| OS (stock) | Symbian^3 / Nokia Belle |
| Form Factor | Side-slider QWERTY |
| Dimensions | 123.7 × 62.4 × 13.6 mm |
| Weight | 176g |
| Display | 4" nHD AMOLED, 640×360, capacitive touch |
| Battery | BL-4D, 1200 mAh |

## System on Chip

| Component | Details |
|-----------|---------|
| Package | RAPUYAMA D1800 (combined SoC + cellular modem) |
| Application Processor | OMAP3630 (ARM Cortex-A8 @ 1GHz) |
| GPU | PowerVR SGX530 @ 200MHz |
| DSP | TMS320C64x+ IVA2.2 (video/audio acceleration) |
| ISP | Integrated in OMAP3630 |
| Modem | Integrated in RAPUYAMA (3G/WCDMA + GSM) |
| Platform | BB5 (Base Band 5) |

## Memory

| Type | Details |
|------|---------|
| RAM | 256MB LPDDR1 (Samsung K5W8G1GACK-AL56, x32, ~178MHz) |
| OneNAND | 1GB (Samsung 8Gbit MLC) — OS, boot, ROFS |
| eMMC | 16GB — user data, apps |
| RAM base | 0x80000000 (physical) |

## PMIC

| Component | Details |
|-----------|---------|
| Chip | TWL5031 (Texas Instruments) |
| I2C | 4 slave addresses: 0x48-0x4B |
| Features | RTC, USB PHY, watchdog, audio codec, GPIO expander, regulators |

## Video / Imaging Engine

| Component | Details |
|-----------|---------|
| Chip | BCM2727B (Broadcom VideoCore) |
| Function | Computational photography, H.264 encode/decode, display processing |
| Interface | Proprietary to RAPUYAMA |
| Power | VIVE_1V8, VIVE_2V5, VIVE_2V8 |
| Clock | XIN 19.2 MHz (test point B1400) |
| Run pin | J1425 |

## I2C Bus Map

### I2C1 (Nokia I2C_2)
| Address | Device | Function |
|---------|--------|----------|
| 0x48-0x4B | TWL5031 | PMIC |
| 0x6B | BQ2415x | Battery charger |
| 0x33 | LP5521 | LED controller (Home/QWERTY/Charging) |
| 0x60 | TPA6130A2 | Headphone amplifier |

### I2C3 (Nokia I2C_0)
| Address | Device | Function |
|---------|--------|----------|
| 0x1C | LIS302DL #1 | Accelerometer (disabled — multi-instance bug) |
| 0x1D | LIS302DL #2 | Accelerometer (active) |
| 0x0F | AK8974 | Magnetometer |
| 0x39 | APDS990x | Proximity + ambient light sensor |
| 0x4C | Atmel mXT | Touchscreen controller |
| 0x34 | LM8323 | Keyboard controller (IRQ: INTC line 35) |
| 0x28 | PN544 | NFC (disabled — GPIOs unknown) |

## Key GPIOs (cross-validated: schematic + service manual)

| GPIO | Function |
|------|----------|
| 2 | Volume Up |
| 3 | Proximity interrupt |
| 12 | Accelerometer interrupt |
| 26 | Hall sensor (lid) |
| 36 | Camera capture button |
| 40 | Magnetometer DRDY |
| 41 | Magnetometer INT |
| 44 | Accelerometer interrupt 2 |
| 65 | Volume Down |
| 105 | IHF amplifier enable |
| 122 | Touch reset |
| 123 | Touch interrupt |

## Connectivity

| Interface | Details |
|-----------|---------|
| WiFi | SPI-A (MCSPI), NOT SDIO |
| Bluetooth | Integrated |
| GPS | OMAP3630 internal |
| NFC | PN544 (I2C, disabled) |
| USB | Micro-USB, OTG capable |
| HDMI | Micro-HDMI output |
| Audio jack | 3.5mm (or Micro-USB adapter) |

## Storage Layout

| Region | Content |
|--------|---------|
| OneNAND TOC | 32 entries at offset 0x140000 |
| SWBL | 8.5KB at NAND 0x800 (SDRC config) |
| NLoader | Boot stage 2, auth + GENIO |
| GENIO_INIT | Encrypted pad mux config at 0x01200000 |
| SOS+CORE | OS image at 0x02A40000 |
| eMMC | Empty FAT32 (user data) |

## Power Test Points (from Service Hints)

| Test Point | Signal | Nominal Value |
|------------|--------|---------------|
| C2070 | Vbat | ~3.8V |
| C2818 | Vcore | ~1.2V |
| L2504 | Vio | 1.8V |
| C2802 | VRFC | 1.8V |
| J2832 | SleepClk | 32.768 KHz |
| J3300 | VBUS | >3V |
| J3117 | PwrOnx | Power key (Top End Cap) |
| C1466 | VBAT_IVE | ~3.8V |
| C1405 | VIVE_1V8 | 1.8V |
| C1419 | VIVE_2V5 | 2.5V |
| C1455 | VIVE_2V8 | 2.8V |
| J1425 | IVE RUN | 1.8V |
| B1400 | IVE XIN | 19.2 MHz |

## Units

| Unit | Role | State |
|------|------|-------|
| Unit A | Development target (will be modified) | Stock, reserved for Linux |
| Unit B | Reference / rescue (stays stock forever) | Jailbroken, running Symbian Belle FP1 |

Unit B IMEI: 354864049067334
Firmware: Belle Refresh 111.040.1511 (2012-07-27)
Product code: 059G138
