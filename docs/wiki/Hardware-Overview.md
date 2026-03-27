# Hardware Overview -- Nokia E7-00 (RM-626)

## CRITICAL: CPU Correction (2026-03-23)

> **The application processor is ARM1176JZF-S (ARMv6), NOT TI OMAP3630 (Cortex-A8, ARMv7).**
>
> SuperPage CPUID: `0x410fb764`
> - Implementer: 0x41 (ARM)
> - Part: 0xB76 (ARM1176JZF-S)
> - Variant: r0p4
> - Architecture: ARMv6 (with Jazelle, VFP, TrustZone)
>
> The chip is Samsung-manufactured, likely a **Broadcom BCM2763** -- the same
> application processor family used in the Nokia N8. Confirmed by community
> member zaiood and CPUID analysis. See [critical-cpu-discovery.md](../critical-cpu-discovery.md).

## Application Processor (REAL HARDWARE)

| Property | Value |
|----------|-------|
| CPU core | ARM1176JZF-S (ARMv6) |
| CPUID | 0x410fb764 |
| Manufacturer | Samsung (fabrication) |
| Likely SoC | Broadcom BCM2763 |
| Register map | **Unknown** -- not OMAP3 |
| TrustZone | Yes (confirmed by BCM2727 analysis) |

## QEMU Emulation SoC (SYNTHETIC -- NOT REAL HARDWARE)

The QEMU machine uses TI OMAP3630 as a **synthetic development platform**.
This was built before the real CPU was identified and remains useful for
driver development and rootfs testing, but does NOT match real hardware.

| Property | Value |
|----------|-------|
| Emulated SoC | TI OMAP3630 |
| Emulated CPU | ARM Cortex-A8 (ARMv7) |
| Status | Boots to shell, functional |
| Accuracy | NOT hardware-accurate |

## Modem / CMT

| Property | Value |
|----------|-------|
| Chip | RAPUYAMA (D1800) |
| Role | Cellular modem (CMT) only |
| ASIC ID | 0x1921 |
| Note | Previously misidentified as app processor |

## Other Key Components

| Component | Detail |
|-----------|--------|
| RAM | 256 MB LPDDR (Samsung K5W8G1GACK-AL56, x32, ~178 MHz) |
| Storage | 16 GB eMMC + OneNAND flash (boot) |
| Display | 4.0" AMOLED 640x360 nHD, capacitive touch |
| Camera | 8 MP AF main (Toshiba TCM8590MD), VGA front |
| Video co-proc | Broadcom BCM2727 VideoCore III (32 MB stacked SDRAM) |
| WiFi | TI WL1273 (SPI-A, NOT SDIO) -- 802.11 b/g/n |
| Bluetooth | TI WL1273 -- BT 3.0 + HS |
| NFC | NXP PN544 |
| PMIC | TI TWL5031 (TWL4030 family) |
| Keyboard | Slide-out 4-row QWERTY, LM8323 controller (I2C, 0x34) |
| Charger | TI BQ2415x (I2C, 0x6B) |
| Sensors | LIS302DL accel (0x1C/0x1D), AK8974 mag (0x0F), APDS-99xx ALS/prox (0x39) |
| Touch | Atmel mXT (I2C, 0x4C) |
| Audio amp | TPA6130A2 headphone amp |
| LED driver | LP5521 (I2C, 0x33) |
| Base crystal | 38.4 MHz |
| Battery | BP-4L, 1200 mAh |

## What Changed With the CPU Discovery

| Previous Assumption | Reality |
|---------------------|---------|
| TI OMAP3630 (Cortex-A8, ARMv7) | ARM1176JZF-S (ARMv6), likely BCM2763 |
| OMAP3 register map | Unknown (Broadcom, undocumented) |
| PADCONF at 0x48002030 | May not exist on real HW |
| pinctrl-single driver | Wrong SoC |
| RAPUYAMA = app processor | RAPUYAMA = modem only |
| "Gazoo = OMAP3630" codename | Gazoo may refer to different product line |
| ASIC ID 0x1921 = app processor | 0x1921 = RAPUYAMA modem |

## What Remains Valid

- QEMU emulation boots and works (synthetic platform, not HW match)
- TWL5031 PMIC, LPDDR specs, eMMC -- these are real
- Symbian toolchain and exe building -- correct
- ISI/Phonet modem protocol research -- valid (about modem, not CPU)
- I2C device addresses and sensor models -- confirmed from DCP/schematics
- BCM2727 VideoCore analysis -- valid
