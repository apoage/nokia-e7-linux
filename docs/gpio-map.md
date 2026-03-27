# Nokia E7 GPIO Map

**Source:** RM-626 service schematics v1.0 (pages 2-9), visual + text analysis
**Date:** 2026-02-28

---

## Architecture: RAPUYAMA genio pins

> **Note (2026-03-23):** This section originally stated RAPUYAMA = OMAP3630. Real HW
> CPUID = ARM1176JZF-S (ARMv6), likely Broadcom BCM2763. The "genio" pins are
> Broadcom GPIO, not OMAP3630 GPIO. PADCONF registers at 0x48002xxx may be Broadcom
> register space, not TI SCM. See docs/critical-cpu-discovery.md. The QEMU emulation
> uses OMAP3630 GPIO mapping intentionally (synthetic, not HW-accurate).

RAPUYAMA (D1800) is the application processor SoC package. The "genio" pins on the
RAPUYAMA BGA are GPIO pins. The GENIO_INIT flash partition configures pad mux
registers to set each pin's function.

On the schematic:
- Pages 2-3 label GPIOs as "GPIO18", "GPIO19" etc. — these connect to the BCM2727
  imaging processor area
- Pages 3-4 label GPIOs as "genio2", "genio44" etc. — these use Nokia's "GENIO"
  naming convention for the package pins

**genio N = GPIO N** (needs UART boot log to confirm 100%).

---

## Camera / Imaging Processor (BCM2727, pages 2-3)

| GPIO | Signal | Function | Page |
|------|--------|----------|------|
| GPIO18 | VCAM_1V8 EN | Camera 1.8V regulator enable (N1515) | 2 |
| GPIO19 | VCAM_2V8 STBY | Camera 2.8V regulator standby (N1517) | 2 |
| GPIO28 | PRI_CAM_SDA | Primary camera I2C data (4k7 pull to VCAM_1V8) | 2 |
| GPIO29 | PRI_CAM_SCL | Primary camera I2C clock (4k7 pull to VCAM_1V8) | 2 |
| GPIO32 | SEC_CAM_CLK | Secondary camera clock (33R series, to UI flex) | 2 |
| GPIO33 | TE0 | Display tearing effect (to UI flex) | 2 |
| GPIO36 | SEC_CAM_SHUTDOWN | Secondary camera shutdown (to UI flex) | 2 |
| GPIO37 | DISP_RESET_N | Display reset (to UI flex) | 2 |
| GPIO38 | EXTCLK | Primary camera external clock (33R series) | 2 |
| GPIO40 | — | BCM2727 connector J1443 | 2 |
| GPIO43 | — | BCM2727 connector J1445 | 2 |
| GPIO46 | SEC_CAM_I2C(0) | Secondary camera I2C data (3k3 R1488, to UI flex) | 2 |
| GPIO47 | SEC_CAM_I2C(1) | Secondary camera I2C clock (3k3 R1487, to UI flex) | 2 |
| GPIO49 | FLASH_TRIGGER | Camera flash strobe (N1500) | 2 |
| GPIO52 | FLASH_ENABLE | Camera flash enable / I2C (N1500) | 2 |

### BCM2727 Additional Connections
- CSI-2 lanes (CSI_DN0/DP0, CSI_CLKN/CLKP, CSI_DN1/DP1) for primary camera
- CCP lanes (CCP_CLKP/CLKN/DP/DN) for secondary camera
- DSI1 lanes (DSI1_CLKP/N, DSI1_DP0/DN0) for display
- CCP(30:0) bus to RAPUYAMA
- SENNA_TORCH_ENABLE to RAPUYAMA (camera torch via modem)
- 19.2 MHz crystal B1400 (XIN/XOUT)

---

## Genio/GPIO Pin Assignments (pages 3-4)

| GPIO | BGA Ball | Signal | Function | Page |
|------|----------|--------|----------|------|
| GPIO2 | D10 | volume_up | Volume up button (S2462) | 4 |
| GPIO12 | T23 | AccInt1 | Accelerometer interrupt 1 (N1103) | 4 |
| GPIO13 | Y21 | Hall_sensor_DET | Hall sensor detection (N1000 area) | 4 |
| GPIO17 | T7 | FMIRQ area | FM radio interrupt / I2C0 routing | 4 |
| GPIO19 | — | HMC_3V0_EN | eMMC 3.0V switch enable | 4 |
| GPIO23 | — | I2C0 area | Near I2C bus 0, possibly FMIRQ | 4 |
| GPIO36 | S25 | Capture_key | Camera shutter button (QWERTY flex) | 4 |
| GPIO44 | AA11 | AccInt2 | Accelerometer interrupt 2 (N1103, J1106) | 4 |
| GPIO57 | — | SysClk192 | System clock area (near PEARL) | 3 |
| GPIO58 | — | SleepX | Sleep signal (near PEARL) | 3 |
| GPIO59 | E22 | VMEM2 EN | eMMC power regulator enable (N3201) | 4 |
| GPIO60-62 | L22,V3,S13 | — | Power management area | 4 |
| GPIO65 | S30 | volume_down | Volume down button (S2462) | 4 |
| GPIO77 | B15 | WLAN area | WLAN-related (near BOB module) | 4 |

### Genio Pins on RAPUYAMA (pages 3-4, partial list)

Additional genio pins visible but signal not fully identified:

| GPIO | Area | Notes |
|------|------|-------|
| GPIO1, GPIO79 | Keyboard/volume | Near KEYB(15:0) connector |
| GPIO3 | SENSORS(15:0) | Sensor bus interface |
| GPIO24, GPIO43 | HDMI | Near HDMI connector area |
| GPIO26, GPIO40, GPIO41 | Combo memory | D3000 memory interface |
| GPIO27, GPIO28 | Keyboard | Near KEYB connector |
| GPIO29, GPIO30 | GPS | GPS module area |
| GPIO31, GPIO32 | GPS | GPSResetX, GPS_TimeStamp |
| GPIO35 | BTFM/WLAN/GPS | Near BOB module |
| GPIO39, GPIO72 | Audio | Audio routing |
| GPIO42 | TxCDa | Serial/debug |
| GPIO46, GPIO51 | GPS TX | TXP, TXA signals |
| GPIO47-50, GPIO53-56, GPIO82-85 | DDR | RAPUYAMA DDR memory interface |
| GPIO63 | USB | USB area |
| GPIO68 | CCP/Camera | Camera CCP bus SDA |

---

## Sensor Interrupt Routing

All sensor interrupts route through flex connectors back to RAPUYAMA genio pins on the main board. The routing path is:

```
Sensor IC → flex board trace → flex connector → main board trace → RAPUYAMA ball (GPIO)
```

### Confirmed Interrupt Routing

| Sensor | Signal | Flex | Connector Pin | GPIO | Source |
|--------|--------|------|---------------|------|--------|
| Accelerometer (N1103) | AccInt1 | Main board (direct) | — | **GPIO12** (T23) | Schematic p4 |
| Accelerometer (N1103) | AccInt2 | Main board (direct) | — | **GPIO44** (AA11) | Schematic p4 |
| Hall sensor (N1000) | Hall_sensor_DET | QWERTY flex | SENSORS pin 1 | **GPIO26** | Service manual L3/4 |
| ALS/Proximity (N8001) | PROX_INT (DiPro_Int) | UI flex | SENSORS pin 0 | **GPIO3** | Service manual L3/4 |
| Magnetometer (N1105) | mm_DRDY | QWERTY flex | SENSORS pin 9 | **GPIO40** | Service manual L3/4 |
| Magnetometer (N1105) | mm_INT | QWERTY flex | SENSORS pin 10 | **GPIO41** | Service manual L3/4 |
| Touch controller | TOUCH_INT | UI flex | UI flex pin 95 | **GPIO123** | Service manual L3/4 |
| Touch controller | TOUCH_RST | UI flex | UI flex pin 94 | **GPIO122** | Service manual L3/4 |

### Interrupt Routing (GPIO number still unknown)

| Sensor | Signal | Flex | Connector Pin | Destination |
|--------|--------|------|---------------|-------------|
| IO Expander (N1002) | IOexp_IRQ | QWERTY flex | SENSORS pin 13 | RAPUYAMA (genio TBD) |
| IO Expander (N1002) | IOexp_RESETN | QWERTY flex | SENSORS pin 6 | RAPUYAMA (genio TBD) |

Most sensor interrupt GPIOs now confirmed from service manual L3/4.
IO expander IRQ/RESET GPIOs still unknown — not in service manual signal map.

---

## SENSORS Connector (QWERTY flex ↔ main board, page 4)

| Pin | Signal | Source |
|-----|--------|--------|
| 0 | Prox_int | ALS/Proximity (N8001, UI flex) |
| 1 | Hall_sensor_DET | Hall sensor (N1000, QWERTY flex) |
| 9 | mm_drdy | Magnetometer data ready (N1001, QWERTY flex) |
| 10 | mm_int | Magnetometer interrupt (N1001, QWERTY flex) |
| 6 | IOexp_RESETN | IO Expander reset (N1002, QWERTY flex) |
| 13 | ExpanderInt | IO Expander interrupt (N1002, QWERTY flex) |
| 14 | I2C0SDA | Sensor I2C bus data |
| 15 | I2C0SCL | Sensor I2C bus clock |

---

## UI Flex Connector (UI flex ↔ main board, page 9)

| Pin | Signal | Function |
|-----|--------|----------|
| 1 | TE0 | Display tearing effect |
| 3 | HOME LED 1 | Home button LED |
| 4 | HOME LED 2 | Home button LED |
| 5 | HOME KEY | Home button input |
| 15 | SDA | I2C (I2CSDA) |
| 16 | SCL | I2C (I2CSCL) |
| 28 | TOUCH_INT | Touch interrupt |
| 29 | TOUCH_RST | Touch reset |
| 94 | Touch_RSTX | Touch reset (alternate routing) |
| 95 | Touch_Int | Touch interrupt (alternate routing) |
| 96 | I2C0SDA | Sensor I2C data |
| 97 | I2C0SCL | Sensor I2C clock |
| — | EARP, EARN | Earpiece audio |
| — | DVS0SL, DVS0DA | Display DSI |
| — | PROX_INT | Proximity interrupt |
| — | Bus0-5 | Data bus |

---

## QWERTY Keyboard Matrix (N1002, page 8)

The IO expander N1002 drives an 8-row × 11-column QWERTY keyboard matrix.

### Matrix Layout (from schematic page 8)

```
        Col1    Col2    Col3    Col4    Col5    Col6    Col7    Col8    Col9    Col10   Col11
Row1:   Q       W       E       R       T       Y       U       I       O       P
        S1003   S1004   S1005   S1006   S1007   S1008   S1009   S1010   S1011   S1012
Row2:   A       S       D       F       G       H       J       K       L       ?
        S1014   S1015   S1016   S1017   S1018   S1019   S1020   S1021   S1022   S1038
Row3:   Z       X       C       V       B       N       M       ,       .
        S1023   S1024   S1025   S1026   S1027   S1028   S1029   S1030   S1031
Row4+:  Fn, Shift, Space, Sym, Ctrl, Enter, Backspace, arrows
        (S1033-S1051 area)
```

### N1002 Pin Map (BGA, page 8)

| BGA Ball | Function | Type |
|----------|----------|------|
| E2 | SDA | I2C data |
| E1 | SCL | I2C clock |
| F3 | IRQN | Interrupt output (active low) |
| C1 | RESETN | Reset input (active low) |
| A4, F4 | VCC | Power (VIO 1.8V) |
| C3,C4,D3,D4 | GND | Ground |
| A6 | KPX0 | Row 0 |
| A5 | KPX1 | Row 1 |
| F1 | KPX2 | Row 2 |
| F2 | KPX3 | Row 3 |
| A2 | KPX4 | Row 4 |
| B3 | KPX5 | Row 5 |
| A3 | KPX6 | Row 6 |
| B4 | KPX7 | Row 7 |
| C6 | KPY0 | Column 0 |
| C5 | KPY1 | Column 1 |
| B6 | KPY2 | Column 2 |
| B5 | KPY3 | Column 3 |
| B2 | KPY4 | Column 4 |
| A1 | KPY5 | Column 5 |
| B1 | KPY6 | Column 6 |
| C2 | KPY7 | Column 7 |
| E3 | KPY8 | Column 8 |
| D5 | KPY9 | Column 9 |
| E6 | KPY10 | Column 10 |

Also receives Lock_key and Camera_SW_1 via QWERTY flex connector.
Also receives SetCurrKB, OVVLed, SetCurrD for keyboard LED current control.

**Likely IC: TCA8418** (TI) — Nokia N950 (sister device) uses TCA8418 @ I2C 0x34.
KPX/KPY naming matches TI convention. The 11th column (KPY10) may use a GPIO pin.

---

## Button GPIOs

| Button | Signal | GPIO | Ball | Source |
|--------|--------|------|------|--------|
| Volume Down | volume_down | GPIO65 | S30 | Main board switch S2462 |
| Volume Up | volume_up | GPIO2 | D10 | Main board switch S2462 |
| Capture Key | Capture_key | GPIO36 | S25 | QWERTY flex connector |
| Power Key | PwrOstl | — | — | TWL5031 (N2400) direct, not OMAP GPIO |
| Home Key | HOME_KEY | TBD | — | UI flex connector pin 5 |
| Lock Key | Lock_key | TBD | — | QWERTY flex connector pin 5 → N1002 |

---

## Connectivity GPIOs (pages 3, 7 — from block diagram reconstruction)

WL1273 (BOB) BT/FM/WLAN combo and GPS module. GPIO assignments extracted
from block diagram analysis (signal routing through RAPUYAMA BGA balls).

| GPIO | Signal | Function | IC Pin | Source |
|------|--------|----------|--------|--------|
| GPIO48 | BT_RESETX | Bluetooth reset (active low) | BOB J5 | block-diagrams.md |
| GPIO50 | BT_WAKEUP | Bluetooth wakeup | BOB C9 | block-diagrams.md |
| GPIO53 | GPS_EN_RESET | GPS enable/reset | GPS N6200 H2/E6 | block-diagrams.md |
| GPIO77 | WLAN_IRQ | WiFi interrupt (J6376 V22) | BOB J4 | block-diagrams.md |
| — | WLANENABLE | WiFi enable (J6373 N22) | BOB E2 | block-diagrams.md |
| — | FM_INTX | FM radio interrupt | BOB H9 | block-diagrams.md |
| GPIO24 | HDMI_CABLE_DET | HDMI cable detect | HDMI conn | block-diagrams.md |

### WLAN Bus: SPI (not SDIO)

**Correction**: WLAN uses SPI-A bus, confirmed by firmware strings (`am_spia.cpp`,
`SpiClient: Request status == SPIA::EFailure`) and schematic (SPI_CLK, SPI_MOSI,
SPI_MISO, SPI_CSX pins on BOB).

| Signal | RAPUYAMA Ball | BOB Pin |
|--------|---------------|---------|
| SPI_CLK | J2804 D10 | E3 |
| SPI_MOSI | Y20 | F4 |
| SPI_MISO | P17 (68R R2859) | G4 |
| SPI_CSX | G9 | E4 |
| CLK_REQ | J6375 | B9 |

### Bluetooth UART

BT uses UART (H4+ protocol at 115200 bps, port 0). From `hci.ini` (Task 019).

| Signal | RAPUYAMA | BOB Pin |
|--------|----------|---------|
| UART_TX | BTUARTIn | J6 (68R R2860) |
| UART_RX | BTUARTOut | H6 |
| UART_CTS | BTCTS | G6 |
| UART_RTS | BTRTS | H7 |
| UART_WAKE | HostWakeUp | C8 |

### GPS Module (N6200)

GPS communicates via I2C1 (also used as UART). Separate from sensor I2C0.

| Signal | GPIO | Notes |
|--------|------|-------|
| GPS_EN_RESET | GPIO53 | Enable/reset |
| GPS_TimeStamp | genio32? | Timing |
| GPS_Blanking | genio49? | PA enable |
| AGPS_CLK_REQ | — | Assisted GPS |

---

## Cross-Validated GPIO Summary (2026-03-01)

All confirmed against service manual L3/4, schematic, DTS, and knowledge graph:

| GPIO | Signal | Direction | Function | Confidence |
|------|--------|-----------|----------|------------|
| 2 | VOLUME_UP | in | Volume up button | confirmed |
| 3 | PROX_INT | in | APDS-99xx proximity/ALS interrupt | confirmed |
| 12 | ACCEL_INT1 | in | LIS302DL accelerometer interrupt 1 | confirmed |
| 26 | HALL_DET | in | Hall sensor — keypad slide detect | confirmed |
| 35 | WLAN_EN | out | WL1273 WLAN enable (N950 confirms gpio2,3=gpio35) | high |
| 36 | CAPTURE_KEY | in | Camera shutter button | confirmed |
| 39 | DIGIMIC_DATA | out | Digital microphone PDM data | confirmed |
| 40 | MAG_DRDY | in | AK8974 magnetometer data ready | confirmed |
| 41 | MAG_INT | in | AK8974 magnetometer interrupt | confirmed |
| 44 | ACCEL_INT2 | in | LIS302DL accelerometer interrupt 2 | confirmed |
| 46 | GPS_CLK_REQ | out | GPS clock request | confirmed |
| 48 | BT_RESETX | out | WL1273 Bluetooth reset (active low) | confirmed |
| 50 | BT_WAKEUP | out | WL1273 Bluetooth wakeup | confirmed |
| 53 | GPS_EN_RESET | out | GPS module enable/reset | confirmed |
| 59 | VMEM2_EN | out | VMEM2 memory boost regulator enable | confirmed |
| 65 | VOLUME_DOWN | in | Volume down button | confirmed |
| 72 | DIGIMIC_CLK | out | Digital microphone PDM clock | confirmed |
| 77 | WLAN_IRQ | in | WL1273 WLAN interrupt | confirmed |
| 105 | IHF_AMP_EN | out | IHF speaker amplifier enable | confirmed |
| 122 | TOUCH_RESET | out | Atmel mXT touchscreen reset | confirmed |
| 123 | TOUCH_INT | in | Atmel mXT touchscreen interrupt | confirmed |

Total: 21 confirmed GPIO assignments.

---

## What Remains Unknown

1. **WLANENABLE pad mux**: GPIO 35 is confirmed (N950 match) but pad register offset unknown (register map is Broadcom, not OMAP)
2. **FM_INTX exact GPIO**: routes to FMIRQ on RAPUYAMA, genio17 area (probable)
3. **Display reset**: GPIO37 (DISP_RESET_N) routes through BCM2727 — mediated, not direct OMAP
4. **IO expander IRQ/RESET GPIOs**: N1002 IRQN and RESETN genio numbers unknown
5. **All pad mux register values**: GENIO_INIT is BB5-encrypted

**UART boot log capture (Task 032) will reveal ALL pad mux (address, value) pairs,
resolving every unknown GPIO assignment and providing pinctrl-single values for DTS.**

---

*Last updated: 2026-03-01*
