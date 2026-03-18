# QEMU Nokia E7 Machine

## Overview

Custom QEMU machine type `nokia-e7` implementing the OMAP3630 SoC and
Nokia E7 board peripherals. Single source file (`omap3630.c`).

## Emulated Peripherals

### Fully Functional
| Peripheral | Notes |
|------------|-------|
| OMAP3630 INTC | Interrupt controller, 96 IRQ lines |
| GPIO (6 banks) | OMAP24XX layout, W1C IRQSTATUS, `omap3_gpio_set_pin()` API |
| sDMA (32 channels) | Instant-completion model |
| I2C1/2/3 | Full controller model, IRQ 56/57/61 |
| TWL5031 PMIC | 4-slave I2C, RTC (host time), USB PHY, watchdog, audio codec |
| LM8323 Keyboard | Full pipeline: sendkey→FIFO→IRQ→input subsystem, 41 keys |
| LIS302DL | Accelerometer, probe OK |
| AK8974 | Magnetometer, probe OK |
| APDS990x | Proximity/ALS, probe OK |
| BQ2415x | Battery charger, probe OK |
| LP5521 | LED controller (3 channels), probe OK |
| CM/PRM | Clock/power domain tracking (22 domains) |

### Partial / Stubs
| Peripheral | State |
|------------|-------|
| GPMC + OneNAND | Full command state machine, Samsung 8Gbit |
| HSMMC2 (eMMC) | Card detect + CMD state machine |
| DSS Display | Register model, no pixel output |
| mXT Touch | Info block reads, fails at bootloader recovery |
| SDRC | Basic register model |
| UART1/2/3 | Via QEMU serial |
| 25+ SYSSTATUS stubs | Return "ready" to unblock probes |

### Not Yet Modeled
| Peripheral | Blocker |
|------------|---------|
| BCM2727 ISP | Proprietary VideoCore, TrustZone keys |
| PowerVR SGX530 | Binary driver needed |
| WiFi (SPI) | MCSPI model needed |
| Cellular modem | RAPUYAMA proprietary |
| PN544 NFC | GPIO mapping unknown |
| USB OTG | MUSB model needed |

## Key Design Decisions

1. **Single file**: All peripherals in `omap3630.c` — will refactor later
2. **Stub-first**: Return minimum values to keep kernel happy, add fidelity later
3. **GPIO W1C**: IRQSTATUS uses write-1-to-clear (old stubs returned 1 for all reads → interrupt storms)
4. **CLKSTCTRL init**: Must be 0xFFFFFFFC (CLKACTIVITY=active) or pm_runtime breaks drivers
5. **LM8323 on INTC**: Wired to INTC line 35 directly, not via GPIO (simpler, works)

## Build

```bash
# Apply omap3630.c to QEMU v10.0.0 source tree
cp omap3630.c qemu/hw/arm/
# Add to meson.build, configure, build
cd qemu && ninja -C build
```
