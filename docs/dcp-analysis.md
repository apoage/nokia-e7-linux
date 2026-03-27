# Nokia E7 (RM-626) DCP Factory Configuration Analysis

> **Note (2026-03-23):** Real HW is BCM2727B1 (ARM1176), not OMAP3630. See docs/critical-cpu-discovery.md

Extracted from three DCP variant packages:
- **059D0N6** (czech_qw_silver_white) -- dated 2012-08-11
- **059G138** (euro2_qw1_it_tim_silver_white) -- dated 2013-02-22
- **0591821** (euro_qw_dark_grey) -- dated 2012-08-11

Each package contains:
- `ProductConfiguration/` -- 21 XML files (hardware params, RF calibration, selftests)
- `VariantConfiguration/` -- 1 XML per variant (flash file lists, DRM, identity)
- `Refurbish/` -- 1 XML (product profile feature flags)

**Key finding: All 21 ProductConfiguration XML files are byte-identical across all
three variants.** The only differences are in VariantConfiguration (variant identity,
language pack, eMMC image) and Refurbish (059G138 has an extra feature flag).

---

## Table of Contents

1. [hwconf_RM-626.xml -- LED Channels, Display, Proximity Sensor](#1-hwconf_rm-626xml)
2. [hw_als_conf.xml -- ALS Calibration (Pupumon)](#2-hw_als_confxml)
3. [ALSTuningWizard.xml -- ALS Calibration (Dipro)](#3-alstuningwizardxml)
4. [camera.xml -- Camera Configuration](#4-cameraxml)
5. [RM-626_TouchPad.xml -- Touchscreen Calibration](#5-rm-626_touchpadxml)
6. [RM-626_wlan_bob.xml -- WL1271 WLAN TX Power Tuning](#6-rm-626_wlan_bobxml)
7. [RM-626_ADC_reading.xml -- TWL5031 ADC Channel Map](#7-rm-626_adc_readingxml)
8. [RM-626_Audio_configuration.xml -- Audio Routing](#8-rm-626_audio_configurationxml)
9. [RM-626_selftests_12012012.xml -- Factory Self-Tests](#9-rm-626_selftests_12012012xml)
10. [RM626.xml -- Flash, Security, Energy Management](#10-rm626xml)
11. [Ratbandconfiguration.xml -- RF Band Support](#11-ratbandconfigurationxml)
12. [bluetooth.xml -- Bluetooth Config](#12-bluetoothxml)
13. [Call_control.xml -- Call Control Server](#13-call_controlxml)
14. [rm-626_emcal.xml -- EM Calibration Limits](#14-rm-626_emcalxml)
15. [RM-626_fm_radioconf.xml -- FM Radio](#15-rm-626_fm_radioconfxml)
16. [RM-626_CA158RS_AttenuationConfiguration -- Antenna Attenuation](#16-rm-626_ca158rs_attenuationconfigurationxml)
17. [RM-626_linko_rf_15122010.xml -- RF Tuning Sequence Master](#17-rm-626_linko_rf_15122010xml)
18. [RM-626_linko_rf_gsmrx_23112010.xml -- GSM RX Calibration](#18-rm-626_linko_rf_gsmrx_23112010xml)
19. [RM-626_linko_rf_gsmtx_02112010.xml -- GSM TX Power Tuning](#19-rm-626_linko_rf_gsmtx_02112010xml)
20. [RM-626_linko_rf_wcdmarx_23112010.xml -- WCDMA RX Calibration](#20-rm-626_linko_rf_wcdmarx_23112010xml)
21. [RM-626_linko_rf_wcdmatx_17032011.xml -- WCDMA TX Tuning](#21-rm-626_linko_rf_wcdmatx_17032011xml)
22. [Variant Configuration XMLs](#22-variant-configuration-xmls)
23. [Refurbish Product Profiles](#23-refurbish-product-profiles)
24. [Cross-Variant Diff Summary](#24-cross-variant-diff-summary)
25. [Linux Driver Mapping Summary](#25-linux-driver-mapping-summary)

---

## 1. hwconf_RM-626.xml

### Light Control Configuration

AmbientLightSensor: **True** (ALS present, hardware confirmed)
DisplayAndKeypadPaired: **False** (backlight independently controllable)

| Light Name  | Channel# | Notes |
|-------------|----------|-------|
| HOME        | 2        | Home key LED (probably LP5521 engine) |
| QWERTY      | 3        | Keyboard backlight |
| CHARGINGN   | 4        | Charging indicator (amber/red) |
| CHARGINGG   | 5        | Charging indicator (green) |
| TORCH       | 6        | Camera flash / torch mode |

**Linux mapping:**
- LP5521 LED controller at I2C 0x33 (`leds-lp5521` driver)
- DTS: `led-sources` property maps channel numbers to LP5521 engines
- Channel 2 = HOME LED likely maps to LP5521 engine 0
- Channel 6 (TORCH) is probably the ADPS1653/LM3555 flash LED driver, not LP5521
- Key insight: 5 named channels confirms multi-LED configuration beyond simple RGB

### Display Configuration

- **Resolution: 640x360** (nHD landscape -- CBD display, 4" AMOLED)
- Operating system: **Sos** (Symbian OS -- display controller runs in SOS context)
- 24 display test patterns defined (IDs 0-63, used in factory testing)
  - Includes: all-off (0), all-on (2), checkerboard (11), color bars (24-27), frame (28), cross (62)

**Linux mapping:**
- DTS: `panel` node with `width-mm`, `height-mm`; 640x360 confirms nHD panel
- OMAP DSS driver (`omapfb` or `omapdrm`): native resolution matches
- Test patterns are factory-only, no Linux equivalent needed

### Proximity Sensor Configuration

- Server: ApeTest (runs on application processor)
- **Low threshold: 90**
- **High threshold: 140**
- **Offset: 10**

**Linux mapping:**
- APDS990x driver (`drivers/iio/light/apds9960.c` or `apds990x.c`)
- DTS properties: `avago,proximity-low-threshold = <90>`, `avago,proximity-high-threshold = <140>`
- Offset maps to `avago,proximity-offset = <10>` or calibration sysfs attribute
- These thresholds define the hysteresis for "near" vs "far" detection
- Already probing in QEMU with OF patch

---

## 2. hw_als_conf.xml

### Ambient Light Sensor Calibration (Pupumon variant)

"Pupumon" is Nokia's internal name for one ALS hardware variant.

| Parameter | Value |
|-----------|-------|
| Reference value | **687** (lux ADC count at reference illumination) |
| Default coefficient | **1.0** |
| Minimum coefficient | **0.6** |
| Maximum coefficient | **1.4** |

**Linux mapping:**
- APDS990x ALS section
- Reference value = factory calibration target in ADC counts at known lux
- Coefficients = per-unit gain correction (0.6--1.4 range = +/-40% tolerance)
- DTS: `avago,als-cal-factor = <687>` (custom property, or userspace calibration)
- The `ga` (glass attenuation) parameter in the driver serves a similar purpose

---

## 3. ALSTuningWizard.xml

### ALS Calibration (Dipro variant)

"Dipro" appears to be a newer calibration tool/method.

| Parameter | Value |
|-----------|-------|
| Reference value | **1** (normalized) |
| Default coefficient | **1** |
| Minimum coefficient | **0.1** |
| Maximum coefficient | **3** |

**Linux mapping:**
- Same APDS990x driver, wider tolerance range (0.1--3.0 vs 0.6--1.4)
- Dipro calibration uses normalized reference (1) vs absolute ADC count (687)
- Suggests the Dipro method applies calibration in software post-processing
- The wider range (0.1--3.0) may indicate compensating for more optical assembly variation

---

## 4. camera.xml

### Camera Configuration

**Camera 1 (Main camera):**
- ID: 1
- Type: SOS (Symbian OS controlled -- goes through BCM2727 ISP)
- Size: **8MPIX** (confirms 8MP main sensor -- Toshiba ET8EK8 or similar)
- Format: Test_JBS28 (JBS28 = Nokia internal test image format)
- Save timeout: 100ms
- Supported tests: **Image**, **Flash**
- Sub-tests: DEFECTIVE_PIX_TEST, COUPLET_TEST, BRIGHTNESS_TEST, VIGNETTING_TEST, BLEMISH_TEST

**Camera 2 (Front camera):**
- ID: 2
- Type: SOS
- Size: **VGA_640_480** (confirms VGA front camera -- likely SMIAPP/smia compliant)
- Format: Test_JBS28
- Save timeout: 100ms
- Supported tests: **Image** only (no flash test -- front camera has no flash)
- Sub-tests: same 5 as main camera

**Linux mapping:**
- Main camera: 8MP sensor via CCP2 to BCM2727 ISP (blocked -- see Task 045)
- Front camera: VGA sensor, likely direct CSI-2 to OMAP ISP
  - Could use `smiapp` or `et8ek8` driver family
- The 5 sub-tests are factory QA only, not relevant to Linux driver
- Key data: confirms 8MP + VGA dual camera, both routed via SOS/BCM2727 path

---

## 5. RM-626_TouchPad.xml

### Touchscreen Calibration

| Parameter | Value |
|-----------|-------|
| Layout ID | **254** |
| X resolution | **180 pixels** |
| Y resolution | **320 pixels** |
| Real X size | **40.0 mm** |
| Real Y size | **71.0 mm** |
| Actual X size | **39.8 mm** |
| Actual Y size | **71.4 mm** |
| Shift X | 1 pixel |
| Shift Y | 1 pixel |
| Reset calibration | **False** |

**Linux mapping:**
- Atmel mXT touchscreen controller at I2C 0x4C
- DTS: `touchscreen-size-x = <180>`, `touchscreen-size-y = <320>`
  - NOTE: These are touchpad-native coordinates, NOT display pixels
  - The display is 640x360 -- the touch controller has its own 180x320 resolution
  - Linux `atmel_mxt_ts` driver handles coordinate scaling
- Physical dimensions: 40x71mm active area (4" display diagonal)
- Shift pixel values suggest a 1px dead zone at edges
- Layout ID 254 is a Nokia-internal identifier for this particular panel+controller combo
- The X/Y resolution being 180x320 (portrait) while display is 640x360 (landscape)
  confirms the touch controller reports in portrait orientation

---

## 6. RM-626_wlan_bob.xml

### WL1271 WLAN TX Power Tuning (BOB module)

Version history shows this was copied from RM-596 on 13.09.2010.
"BOB" = Nokia codename for the WL1271/WL1283 combo BT+WLAN+FM module.

**ALL VALUES IN `<WlanBobTxPowerTuningValues>` ARE HEX unless noted.**

#### Tuning Parameters
- Enabled tuning: WlanBobTxPowerTuning
- Sub-bands: **0** (single sub-band, 2.4 GHz only -- no 5 GHz)
- Number of trials: **3**

#### P2G Constant Limits (power-to-gain calibration)
- Min: **-45**
- Max: **45**

#### PD Curve Delta Limits
| Index | Min |
|-------|-----|
| 3     | 1   |
| 9     | 10  |

#### Trace Insertion Losses (Sub-band 0)
- RX trace loss: **0x10** (16 in decimal, = 1.6 dB in 0.1 dB units)
- TX trace loss: **0x00** (0 dB -- TX path loss compensated elsewhere)

#### FEM 0 (Front-End Module configuration 0 -- internal PA path)

**Normal Mode Power Level Limits (hex, units = 0.25 dBm steps):**

| Rate | Hex | Decimal | dBm (approx) |
|------|-----|---------|---------------|
| MCS7 | 0x1D | 29 | ~7.25 dBm |
| 64-QAM | 0x1F | 31 | ~7.75 dBm |
| 16-QAM | 0x24 | 36 | ~9.0 dBm |
| QPSK | 0x25 | 37 | ~9.25 dBm |
| BPSK | 0x25 | 37 | ~9.25 dBm |
| CCK | 0x27 | 39 | ~9.75 dBm |

**CCK Channel Power Level Limits (hex):**

| Channel | Limit (hex) | Decimal |
|---------|-------------|---------|
| 1  | 0x22 | 34 |
| 11 | 0x22 | 34 |
| 13 | 0x22 | 34 |

**OFDM Channel Power Level Limits (hex):**

| Channel | Limit (hex) | Decimal |
|---------|-------------|---------|
| 1  | 0x20 | 32 |
| 11 | 0x20 | 32 |
| 13 | 0x20 | 32 |

**TxBiP (Built-in Power) Values:**
- Sub-band: 0
- Ref point power: **0x80** (128 = 12.8 dBm)
- Ref point PD voltage: **0x2201** (little-endian: 0x0122 = 290 mV)
- Offset: **0**

**TX PD vs Rate Offsets (hex):**

| Rate | Offset |
|------|--------|
| MCS7 | 0x02 |
| 64-QAM | 0x02 |
| 16-QAM | 0x01 |
| QPSK | 0x00 |
| BPSK | 0x00 |
| CCK  | 0x00 |

#### FEM 1 (Front-End Module configuration 1 -- external PA / alternate path)

**Normal Mode Power Level Limits: identical to FEM 0**

**CCK and OFDM Channel Power Level Limits: identical to FEM 0**

**TxBiP Values:**
- Sub-band: 0
- Ref point power: **0x80** (128 = 12.8 dBm)
- Ref point PD voltage: **0xA901** (little-endian: 0x01A9 = 425 mV -- note v0.7 changelog fixed this from 01A9 to A901)
- Offset: **0**

**TX PD vs Rate Offsets (hex) -- FEM 1 has DIFFERENT values:**

| Rate | FEM0 | FEM1 |
|------|------|------|
| MCS7 | 0x02 | 0x04 |
| 64-QAM | 0x02 | 0x07 |
| 16-QAM | 0x01 | 0x06 |
| QPSK | 0x00 | 0x06 |
| BPSK | 0x00 | 0x05 |
| CCK  | 0x00 | 0x06 |

**Linux mapping:**
- WL1271 driver: `drivers/net/wireless/ti/wlcore/` + `wl12xx`
- These values map to the WL1271 NVS (Non-Volatile Store) calibration data
- In Linux, WLAN calibration is stored in `/lib/firmware/ti-connectivity/wl1271-nvs.bin`
- The `wl12xx` driver loads NVS at probe time from firmware file
- Key NVS fields that correspond:
  - `tx_bip_ref_pd_voltage` -> TxBiPRefPointPdVoltage
  - `tx_bip_ref_power` -> TxBiPRefPointPower
  - `tx_per_channel_power_compensation` -> channel limits
  - `tx_pd_vs_rate_offsets` -> TxPdVsRateOffsets
  - `rx_trace_insertion_loss` -> RxTraceInsertionLoss
- FEM 0 vs FEM 1 maps to two front-end module configurations in the NVS
- **Action: these values should be baked into a custom wl1271-nvs.bin for the E7**
- The `calibrator` tool from TI can generate NVS files from these parameters

---

## 7. RM-626_ADC_reading.xml

### TWL5031 ADC Channel Map

| Channel ID | Description | Unit | Notes |
|------------|-------------|------|-------|
| 1 | Battery Voltage | mV | VBAT ADC on TWL5031 MADC |
| 2 | Charger Voltage | mV | USB/wall charger input voltage |
| 3 | Charger Current | mV | Current sense resistor voltage |
| 4 | Battery Size Indicator (BSI) | Ohms | Battery ID resistor |
| 5 | Battery Temperature | K | NTC thermistor via BTEMP ADC |
| 6 | Headset Interconnection | mV | Accessory detection (ACCDET) |
| 7 | Hook Interconnection | mV | Headset button detection |
| 24 | Backup Battery Voltage | mV | RTC backup capacitor/battery |

All channels support both raw (r) and scaled (s) readback.

**Linux mapping:**
- TWL5031 MADC: `drivers/mfd/twl-core.c` + `drivers/iio/adc/twl6030-gpadc.c`
  (TWL5031 is TWL4030 family, so actually `twl4030-madc.c`)
- DTS: `twl4030-madc` node under TWL PMIC
- Channel mapping:
  - Ch1 = MADC_MAIN_BATTERY (GP channel)
  - Ch2 = MADC_VBUS (charger voltage)
  - Ch3 = MADC_ICHG (charge current)
  - Ch4 = MADC_BSI (battery size/type)
  - Ch5 = MADC_BTEMP (battery NTC)
  - Ch6 = typically ADCIN6 (hook/accessory)
  - Ch7 = typically ADCIN7
  - Ch24 = MADC_VBACKUP
- IIO subsystem exposes these as `/sys/bus/iio/devices/iio:deviceN/in_voltageN_raw`
- BSI (Ch4) in Ohms: the TWL4030 MADC measures voltage across pull-up and BSI resistor
- Battery temperature (Ch5) in Kelvin: confirms NTC measurement through TWL BTEMP pin

---

## 8. RM-626_Audio_configuration.xml

### Audio Routing

Product: RM-626

**Fixed Audio Loops:**
- HpInAvOut (headphone in, AV out)
- AvInAvOut (AV in, AV out -- loopback)
- AvInIhfOut (AV in, IHF/speaker out)

**Audio Inputs:**
- Muted, HdMic (handheld mic), HfMic (handsfree mic), HpMic (headphone mic)

**Audio Outputs:**
- Muted, HpEar (headphone earpiece), HdEar (handheld earpiece), HfEar (handsfree earpiece)
- XEar (external earpiece -- HDMI audio?), HdStereo, HfStereo, IhfEar (IHF speaker)

**Supported Tests:**
- ToneGen: **true** (can generate test tones)
- EancMicrophone: **false** (no EANC = Electronic Active Noise Cancellation mic)

### Vibra Configuration
- Low limit: **5** (minimum vibra duty cycle percentage)
- High limit: **95** (maximum vibra duty cycle percentage)
- Direction: **Both** (bidirectional vibra motor -- LRA type)
- Priority: **Ringtone1**

**Linux mapping:**
- TWL5031 audio codec: `drivers/mfd/twl4030-audio.c`, `sound/soc/codecs/twl4030.c`
- Audio routing defines ALSA/ASoC DAPM routes
- IHF amplifier: GPIO105 = IHF_AMP_EN (already in GPIO map)
- Headphone amp: TPA6140A2 on I2C (I2C2), driver `sound/soc/codecs/tpa6130a2.c`
- Vibra: `twl4030-vibra` or `drv2667` -- 5-95% PWM range
  - DTS: motor direction "both" suggests LRA (Linear Resonant Actuator)
  - `vibra-direction = <2>` (bidirectional) in DTS
- No EANC = no secondary noise-cancelling microphone (simpler audio path)

---

## 9. RM-626_selftests_12012012.xml

### Factory Self-Test Map

49 self-tests defined. Format: `[ID] Name -- Purpose` (engine = automated, non-engine = manual)

| # | ID | Name | Category | What It Tests | Engine? |
|---|-----|------|----------|---------------|---------|
| 1 | 0x02 | Current Consumption | BB | PEARL/GAZOO ADC current monitoring via R2200 | No |
| 2 | 0x2E | EM ASIC CBUS | BB | CBUS connection Rapu <-> PEARL/GAZOO (read EM ASIC ID) | Yes |
| 3 | 0x5D | Security Test | BB | Security server status (phone won't boot if fail) | Yes |
| 4 | 0x0C | SIM Clock Loop | BB | SIMCLK + SIMIO signals Rapu <-> PEARL/GAZOO | Yes |
| 5 | 0x21 | SIM Lock | BB | SIM lock data in permanent memory | Yes |
| 6 | 0x03 | Ear Data Loop | BB | EarData/MicData lines Rapu <-> PEARL | Yes |
| 7 | 0x68 | Accelerometer | BB | LIS302DL I2C + 3-axis self-test (X/Y/Z delta) | Yes |
| 8 | 0x31 | VIBRA | BB | Vibra current measurement (on vs off comparison) | Yes |
| 9 | 0x65 | BTEMP | BB | Battery NTC temperature (283K-318K = 10-45C) | Yes |
| 10 | 0x60 | Hook Interrupt | BB | Headset jack hookint interrupt from PEARL/GAZOO | Yes |
| 11 | 0xD1 | Headset Amplifier | BB | TPA6130A2 I2C presence (read silicon revision = 0x0010) | Yes |
| 12 | 0x22 | USB Charging | BB | BQ24153 charger chip I2C communication | Yes |
| 13 | 0x0F | Sleep X Loop | BB | Sleep clock loopback (details TBD) | Yes |
| 14 | 0x13 | Kelvin Vibra | BB | Vibra Kelvin measurement | Yes |
| 15 | 0x17 | Kelvin BatVoltage | BB | Battery voltage Kelvin | Yes |
| 16 | 0x1A | Kelvin Capacitor | BB | Capacitor check | Yes |
| 17 | 0x1B | Kelvin Audio | BB | Audio path Kelvin | Yes |
| 18 | 0x1D | Kelvin Misc | BB | Miscellaneous Kelvin | Yes |
| 19 | 0x61 | Mass Memory IF | BB | eMMC interface test | Yes |
| 20 | 0x74 | Kelvin XEar | BB | External earpiece Kelvin | Yes |
| 21 | 0x75 | DigiMic | BB | Digital microphone test | Yes |
| 22 | 0xEC | DigiMic 1st | BB | Primary digital mic test | Yes |
| 23 | 0xC9 | Magnetometer | BB | AK8974 compass self-test | Yes |
| 24 | 0xDE | LED Controller | BB | LP5521 I2C and LED test | Yes |
| 25 | 0xE8 | USB Charger | BB | BQ2415x extended test | Yes |
| 26 | 0xE9 | Audio PLL | BB | Audio PLL lock test | Yes |
| 27 | 0x40 | Touch Test | BB | Atmel mXT touch panel BIST | Yes |
| 28 | 0x67 | Touch IF | BB | Touch controller interface | Yes |
| 29 | 0xE5 | LED Flash IF | BB | Flash LED driver I2C | Yes |
| 30 | 0x79 | LED Flash | BB | Flash LED firing test | Yes |
| 31 | 0x72 | IO Expander IF | BB | I2C IO expander test (TWL GPIO?) | Yes |
| 32 | 0x05 | Camera Accel Comm | Camera | IVE3 (BCM2727) code download over Messi-16 | Yes |
| 33 | 0x16 | Main Camera Comm | Camera | Main camera I2C via BCM2727 | Yes |
| 34 | 0x25 | Secondary Camera IF | Camera | Front camera I2C via BCM2727 | Yes |
| 35 | 0x07 | Keyboard | UI | Check no keys stuck | No |
| 36 | 0x5A | Power Key | UI | Check power key not stuck | No |
| 37 | 0x63 | Proximity | UI | APDS990x proximity test | Yes |
| 38 | 0x3B | Main LCD IF | UI | Display self-test ("Alexander" display) | No |
| 39 | 0x56 | RF BB IF | RF | RFBUS Rapu <-> Alli (N7512) <-> Aura (N7509) | Yes |
| 40 | 0x53 | RF Supply | RF | Alli + Aura regulator voltages via ADC | Yes |
| 41 | 0x7D | Digi RXTX IF | RF | 4 TX + 4 RX digital lines Rapu <-> Alli loopback | Yes |
| 42 | 0x7C | RF Strobe | RF | RFStrobe signal + EPU data RAM check (0x1212, 0x1B1B) | Yes |
| 43 | 0x7F | PA ID Pin | RF | PA vendor ID (CID pin) + temp diode (V7502) | Yes |
| 44 | 0x4B | WCDMA TX Power | RF | WCDMA TX via IQ mod + Alli ADC det | Yes |
| 45 | 0x15 | BTFM IF | CWS | BT/FM module clocks + power-up | Yes |
| 46 | 0x47 | BT Wakeup | CWS | BT wakeup/sleep signals | Yes |
| 47 | 0x2C | BT PCM IF | CWS | PCM loopback (PCM_IN/OUT/CLK/SYNC) | Yes |
| 48 | 0x48 | WLAN IF | CWS | WL1271 firmware load + power-up + host IRQ | Yes |
| 49 | 0xEF | RAM IF | BB | RAM interface test | Yes |

**Key hardware confirmations from self-test descriptions:**
- "PEARL/GAZOO" = TWL5031 PMIC (Nokia internal names)
- "Rapu"/"RapuYama" = D2800 = RAPUYAMA SoC package
- "Alli" = N7512 = Linko RFIC
- "Aura" = N7509 = RF frontend IC
- "Ukko PA" = N7510 = PA module
- "Alexander" = display controller name
- TPA6130A2 silicon revision check confirms I2C address and chip presence
- BQ24153 confirmed as USB charger IC (matches BQ2415x family in DTS)
- "IVE3" = BCM2727 camera ISP (Nokia internal name)
- "Messi-16" = 16-bit parallel interface to BCM2727

**Linux mapping:**
- Most self-tests map to I2C device probe success in Linux
- The self-test IDs could be useful for writing a hardware diagnostic tool
- BTEMP limits 283K-318K = valid operating range for battery temperature monitoring

---

## 10. RM626.xml

### Flash Configuration

**SW Update Use Cases:**

| Use Case | Init | FactorySet Before | FactorySet After | Mode Before | Mode After | Downgrade |
|----------|------|-------------------|------------------|-------------|------------|-----------|
| Refurbish | BB5 | false | **true** | Test | Normal | true |
| BackupRestore | BB5 | false | false | Normal | Normal | true |
| AlternateSwUpdate | BB5 | false | false | Test/Normal | Normal/Test | true |

Flash media: **USB** and **FBUS** supported
- Both USB and FBUS can flash the phone
- Refurbish switches from Test to Normal mode
- Factory set runs AFTER flashing in Refurbish mode (FactorySetLevel=1)
- AllowSwDowngrade=true for all use cases

### Security Configuration
- SIM lock type: **BB5V2** (BB5 version 2 security)
- Certificate packet type: **PhoNet** (Nokia ISI protocol)
- ESN types: **IMEI**, **BluetoothId**, **WlanId**
  - All three electronic serial numbers are programmed per-unit

### Energy Management
- FixedBattery: **true** (non-removable battery -- E7 has sealed battery)
- EnableIBAT: **false** (battery current monitoring disabled in factory default)

### Product State
- HwResetMethod: **LongPressPower** (8-second power key hold for hard reset)

### Product Data Verification (all in Test mode)
- FtdMode v1.0, ProductProfile v1.1, VariantCertificateVerifier v1.0
- RdCertificationVerifier v1.0, SimLockDataVerifier BB5

**Linux mapping:**
- BB5 flash protocol: relevant for initial boot chain, not Linux runtime
- FixedBattery=true: DTS `battery { compatible = "simple-battery"; }` with no removable flag
- LongPressPower: TWL5031 PMIC long-press detection, `twl4030-pwrbutton` driver
  - DTS: `pwrbutton` node already standard for TWL PMICs
- IMEI/BT/WLAN IDs stored in BB5 secure area (CMT side), not directly accessible from Linux APE

---

## 11. Ratbandconfiguration.xml

### RF Band Support

Module code: **0203474**

**GSM bands:** 850, 900, 1800, 1900 (quad-band)
**WCDMA bands:** 1, 2, 4, 5, 8 (penta-band 3G)

| WCDMA Band | Frequency (DL) | Region |
|------------|---------------|--------|
| Band 1 | 2110-2170 MHz | Europe/Asia |
| Band 2 | 1930-1990 MHz | Americas |
| Band 4 | 2110-2155 MHz | Americas (AWS) |
| Band 5 | 869-894 MHz | Americas (850) |
| Band 8 | 925-960 MHz | Europe/Asia (900) |

**Linux mapping:**
- Modem configuration (CMT side) -- not directly Linux-configurable
- Useful for `ofono` or `libqmi` modem manager configuration
- Module code 0203474 identifies the specific RF module BOM

---

## 12. bluetooth.xml

### Bluetooth Configuration

- Hardware type: **Epoc BC02**
  - "Epoc" = Symbian heritage naming
  - "BC02" does NOT mean Cambridge Silicon Radio BC02 chip
  - The E7 uses TI WL1283 combo chip (BT 3.0 + WLAN + FM)
  - BC02 is likely a software driver classification in Symbian

**Linux mapping:**
- TI WL1283: `drivers/bluetooth/hci_ll.c` (HCILL protocol over UART)
- Or `btwilink` driver for shared transport
- DTS: `bluetooth` node under UART with `compatible = "ti,wl1283-st"`

---

## 13. Call_control.xml

### Call Control Server

- Call server: **ISA4** (ISA = Intelligent Service Architecture, version 4)
- ISI server: **NetServer** (Nokia ISI network server)
- Network ISI version: **014.003**

**Linux mapping:**
- ISA/ISI are Nokia's modem communication protocols
- Linux equivalent: `ofono` with `isimodem` plugin or `phonet` socket protocol
- These run over the HSI/SSI (High-Speed/Serial Synchronous Interface) to the modem
- The HSI driver in Linux: `drivers/hsi/`

---

## 14. rm-626_emcal.xml

### Energy Management Calibration Limits

**ADC calibration (general):**
- Gain: 12000.0 -- 14000.0
- Offset: -49.0 -- +41.0

**BSI (Battery Size Indicator):**
- Gain: 1100.0 -- 1350.0

**VBAT (Battery Voltage):**
- Gain: 14900.0 -- 15900.0
- Offset: 2635.0 -- 2755.0

**IBAT (Battery Current):**
- Gain: 7750.0 -- 12250.0

**SCALE4:**
- Gain: 33000.0 -- 37000.0
- Offset: -100.0 -- +100.0

**Disabled calibrations:**
- EnableICHAR: **false** (charger current calibration)
- EnableVCHAR: **false** (charger voltage calibration)
- EnableTEMP: **false** (temperature calibration)

**Linux mapping:**
- TWL5031 MADC calibration: these are factory calibration acceptance limits
- The actual calibration values are stored per-unit in the phone's CMT NVM
- Linux `twl4030-madc` driver uses raw ADC values; calibration applies in userspace
- VBAT gain/offset: converts raw ADC counts to millivolts
  - Example: Voltage_mV = (ADC_raw * Gain + Offset) / scaling_factor
- BSI gain: converts ADC to resistance for battery identification

---

## 15. RM-626_fm_radioconf.xml

### FM Radio Configuration

- Server: ApeTest
- Timeout: **20000 ms** (20 seconds for FM radio test)

**Linux mapping:**
- TI WL1283 FM: `drivers/media/radio/wl128x/` (FM TX + RX)
- The timeout is factory-test-specific, not a driver parameter
- FM TX antenna uses the wired headset cable as antenna

---

## 16. RM-626_CA158RS_AttenuationConfiguration.xml

### Antenna Point Configuration

Defines 4 antenna connection points and their cable attenuation values for the
CA-158RS test jig/cable.

**Antenna Points:**

| Name | Attenuation Table | Systems |
|------|-------------------|---------|
| RF Low Band | CA-158RS | GSM 850/900, WCDMA 5/8 (Main) |
| RF High Band | CA-158RS | GSM 1800/1900, WCDMA 1/2/4 (Main) |
| FM TX | FMTx Attenuations | FMTX |
| WLAN/Bluetooth | CA-158RS | WLAN |

**CA-158RS Attenuation Table (dB):**

| Frequency (MHz) | Attenuation (dB) |
|------------------|-------------------|
| 835 | 0.14 |
| 836.6 | 0.15 |
| 880 | 0.15 |
| 881.6 | 0.15 |
| 897.4 | 0.15 |
| 897.6 | 0.15 |
| 942.4 | 0.17 |
| 942.6 | 0.17 |
| 1740 | 0.22 |
| 1747.8 | 0.22 |
| 1767.4 | 0.22 |
| 1842.4 | 0.25 |
| 1842.8 | 0.25 |
| 1862.4 | 0.25 |
| 1880 | 0.25 |
| 1950 | 0.30 |
| 1960 | 0.30 |
| 2140 | 0.30 |
| 2442 | 0.30 |

**FM TX Attenuation Table:**

| Frequency (MHz) | Attenuation (dB) |
|------------------|-------------------|
| 70 | 30.0 |
| 110 | 30.0 |

**Linux mapping:**
- These are test cable compensation values, not device parameters
- However, the antenna point split (Low Band vs High Band) confirms:
  - Two-antenna design: low band antenna (sub-1 GHz) and high band antenna (1.7+ GHz)
  - WCDMA bands 5,8 share low band antenna with GSM 850/900
  - WCDMA bands 1,2,4 share high band antenna with GSM 1800/1900
  - WLAN/BT has a separate antenna connection (2.4 GHz)
- FM TX 30 dB attenuation is enormous -- suggests measurement through heavy filtering

---

## 17. RM-626_linko_rf_15122010.xml

### RF Tuning Sequence Master Configuration

Defines the ordered list of enabled factory RF calibration steps.

**Enabled Tunings (in execution order):**
1. WriteWcdmaSupportedBands
2. CommonPaDetection
3. CommonAfcTuning
4. AfcMeasurement
5. GsmTXIQSelfTune
6. GsmRxSequence
7. GsmTxPowerLevelTuning
8. GsmTxPowerLevelMeasurement
9. GsmOrfsMeasurement (Output RF Spectrum)
10. GsmBurstTemplateMeasurement
11. GsmPhaseErrorMeasurement
12. EdgeEvmMeasurement (EDGE EVM)
13. GsmTxPowerReduction
14. WcdmaRxChainGainTuning
15. WcdmaRssiMeasurement
16. WcdmaSnrMeasurement
17. WcdmaTxPowerTuning
18. WcdmaLTxBandResponseTuning
19. WcdmaTxPowerRangeMeasurement
20. WcdmaMaxPowerLimit
21. WcdmaTxMaxPowerMeasurement
22. WcdmaTxAdjacentChannelPowerTest
23. WcdmaTxEvmMeasurement
24. WcdmaTxPowerReduction

**CU-4 Service Adapter Voltage: 3900 mV** (test fixture supply voltage)

**Linux mapping:**
- These are factory calibration steps, not runtime parameters
- The calibration results get stored in phone NVM (non-volatile memory)
- Key info: PA detection runs early (step 2) -- confirms dual PA vendor support (Renesas + Skyworks)
- AFC tuning is done on GSM900 channel 37

---

## 18. RM-626_linko_rf_gsmrx_23112010.xml

### GSM RX Calibration

#### AFC (Automatic Frequency Control) Tuning

| Parameter | Value |
|-----------|-------|
| GSM band | 900 |
| Channel | 37 |
| Default AFC | 9233 |
| Default AFC usage | GivenDefault |
| Default Coarse | 39 |
| Coarse zero level | 0.5 |
| Temp start | -60 C |
| Temp step | 5 C |
| Temp step count | 32 (covers -60 to +95 C) |

**AFC Limits:**

| Parameter | Min | Max |
|-----------|-----|-----|
| CCoarse | 15 | 60 |
| IBiasCore | 0 | 31 |
| AfcFactorA0 | 20000 | 50000 |
| AfcFactorA1 | 0.00006 | 4 |
| AfcFactorA2 | -0.004 | -0.0000001 |
| AfcReading[0-4] | -10 | 10 |
| CtempK | -1000 | 1000 |
| CtempB | 1000 | 2000 |

#### GSM RX Calibration Per Band

| Band | Cal Channel | Signal (dBm) | AGC | SNR Channel | RSSI Channels |
|------|-------------|-------------|-----|-------------|---------------|
| 850 | 190 | -60.0 | 4 | 128 | 190, 128, 251 |
| 900 | 37 | -60.0 | 4 | 975 | 37, 975, 124 |
| 1800 | 700 | -60.0 | 4 | 512 | 700, 512, 885 |
| 1900 | 661 | -60.0 | 4 | 512 | 661, 512, 810 |

**RX Calibration Limits:**

| Band | AGC Min | AGC Max | SNR Min | SNR Max | IQ Diff Range | RSSI Range |
|------|---------|---------|---------|---------|---------------|------------|
| 850 | 96 | 114 | 18 | 40 | +/-0.8 | +/-4.0 dB |
| 900 | 98 | 114 | 18 | 40 | +/-0.8 | +/-4.0 dB |
| 1800 | 96 | 114 | 18 | 40 | +/-0.8 | +/-4.0 dB |
| 1900 | 97 | 114 | 18 | 40 | +/-0.8 | +/-4.0 dB |

Band filter compensation limits: +/-3 dB (all bands, 4 sub-channels each)

**Linux mapping:**
- All stored in CMT (modem) NVM -- not directly accessible from Linux APE
- RSSI accuracy +/-4 dB at -92 dBm is the factory acceptance window
- SNR 18-40 dB is the acceptable receiver quality range

---

## 19. RM-626_linko_rf_gsmtx_02112010.xml

### GSM TX Power Tuning (115KB -- largest single XML)

#### TX IQ Self-Tuning Parameters

| Band | Power Level | Channel | DC Offset I/Q Range | Amplitude Range | Phase Range |
|------|-------------|---------|---------------------|-----------------|-------------|
| 850 | PL5 | 190 | +/-6 | +/-3.0 dB | 81-95 deg |
| 900 | PL5 | 37 | +/-6 | +/-3.0 dB | 80-95 deg |
| 1800 | PL255 | 700 | +/-6 | +/-3.0 dB | 70-105 deg |
| 1900 | PL255 | 661 | +/-6 | +/-3.0 dB | 70-105 deg |

Phase range is wider for high bands (70-105) vs low bands (80-95).

#### GSM TX Power Level Targets (dBm)

**GSM850 (15 power levels, PL5-PL19):**
32.5, 31.0, 29.0, 27.0, 25.0, 23.0, 21.0, 19.0, 17.0, 15.0, 13.0, 11.0, 9.0, 7.0, 5.0

**GSM900 (PL5-PL19):**
33.0, 31.0, 29.0, 27.0, 25.0, 23.0, 21.0, 19.0, 17.0, 15.0, 13.0, 11.0, 9.0, 7.0, 5.0

**GSM1800 (PL0-PL15):**
31.0, 28.5, 26.0, 24.0, 22.0, 20.0, 18.0, 16.0, 14.0, 12.0, 10.0, 8.0, 6.0, 4.0, 2.0, 0.0

**GSM1900 (PL0-PL15):**
31.0, 28.5, 26.0, 24.0, 22.0, 20.0, 18.0, 16.0, 14.0, 12.0, 10.0, 8.0, 6.0, 4.0, 2.0, 0.0

- Max power GSM900: **33.0 dBm** (2W) -- Class 4 mobile station
- Max power GSM1800: **31.0 dBm** (1.26W) -- Class 1 mobile station
- PA compression level: 9.0 dB for all bands, both Renesas and Skyworks PAs
- BB scaler: shift=1.0, coefficient=4.0, gain offset=24.08 dB

#### EDGE TX Power Level Targets (dBm)
- 850/900: 27.0 down to 5.0 (12 levels, PL8-PL19)
- 1800/1900: 26.0 down to 0.0 (16 levels, PL0-PL15)

#### Dual PA Vendor Support
Every power level has separate burst coefficients for:
- **Renesas** PA (MaxCoefficient: 1750)
- **Skyworks** PA (MaxCoefficient: 1730)

This confirms dual-sourcing of the PA module with per-vendor calibration tables.

**Linux mapping:**
- All GSM TX calibration is modem-side (CMT NVM), not Linux-accessible
- Important for understanding the RF BOM: two PA vendors = two calibration curves
- The service manual identifies "Ukko PA" (N7510) -- both Renesas and Skyworks versions exist

---

## 20. RM-626_linko_rf_wcdmarx_23112010.xml

### WCDMA RX Chain Gain Tuning

**Tuning Channels (both Main and Diversity antennas):**

| Band | Channel | Name | Signal Level |
|------|---------|------|-------------|
| 1 | 10700 | WCDMA1_MID | -65.0 dBm |
| 2 | 9800 | WCDMA2_MID | -65.0 dBm |
| 4 | 1638 | WCDMA4_MID | -65.0 dBm |
| 5 | 4408 | WCDMA5_MID | -65.0 dBm |
| 8 | 3013 | WCDMA8_MID | -65.0 dBm |

Frequency offset: 1,000,000 Hz (1 MHz offset for calibration)

**RX Chain Gain Limits:** +/-6.0 dB for all bands, both Main and Diversity antennas.

**Confirms: Diversity RX antenna present** (all 5 bands have diversity RX parameters).

### WCDMA RSSI Measurement

Signal level: -82 dBm, Averaging: 5 samples
RSSI limits: -86 to -78 dBm (all bands, both antennas) = +/-4 dB accuracy

### WCDMA SNR Measurement

Signal level: -88 dBm, Average: 8 samples, TX power: 23.5 dBm
SNR limits: 8-21 dB (all bands, both antennas)

**Linux mapping:**
- Diversity RX = the E7 has a second WCDMA receive antenna (improves reception)
- All calibration is modem-side (CMT NVM)
- 5-band WCDMA with diversity = high-end 2010 radio configuration

---

## 21. RM-626_linko_rf_wcdmatx_17032011.xml

### WCDMA TX AGC Tuning (87KB)

Per-band, per-PA-vendor tuning with 31-step gain sweeps.

**Common parameters (all bands):**
- Step duration: 100 (667 us)
- Power detector high: 23.5 dBm
- Power detector low: 16 dBm
- Gain step count: 31
- Gain step size: -2
- Sweep high power: 24.4 dBm

**Per-band tuning channels and start values:**

| Band | Channel | Renesas GainStart | Renesas BiasV | Sky GainStart | Sky BiasV |
|------|---------|-------------------|---------------|---------------|-----------|
| 1 | 9750 | 318 | 2950 mV | 315 | 2900 mV |
| 2 | 9400 | 318 | 2950 mV | 315 | 2950 mV |
| 4 | 1412 | 315 | 2800 mV | 316 | 2850 mV |
| 5 | 4175 | 318 | 3000 mV | 315 | 2900 mV |
| 8 | 2787 | 318 | 3000 mV | 315 | 3000 mV |

Each band has 31-step bias current sweeps (19 down to 7 for Renesas, 21 down to 10 for Skyworks)
and 31-step bias voltage sweeps (2950 down to ~535 mV for Renesas, 2900 down to ~710 for Skyworks).

### WCDMA TX ACLR Measurement

| Band | Channel | TX Power | MaxPower Flag |
|------|---------|----------|---------------|
| 1 | 9750 | 23.5 dBm | True |
| 2 | 9400 | 23.5 dBm | True |
| 4 | 1413 | 23.5 dBm | True |
| 5 | 4183 | 23.5 dBm | True |
| 8 | 2787 | **24.0 dBm** | True |

ACLR limits: **-32 dBc** (5 MHz adjacent channel), **-50 dBc** (10 MHz alternate)

### WCDMA Max Power Limit Measurement

| Band | MaxPower | Care Channels (5 per band) |
|------|----------|---------------------------|
| 1 | 23.5 dBm | 9612, 9681, 9750, 9819, 9888 |
| 2 | 23.5 dBm | 9262, 9331, 9400, 9469, 9538 |
| 4 | 23.5 dBm | 1312, 1362, 1413, 1463, 1513 |
| 5 | 23.5 dBm | 4132, 4157, 4183, 4208, 4233 |
| 8 | **24.0 dBm** | 2712, 2750, 2787, 2815, 2863 |

Max power limits (dB tolerance):
- Bands 1,2,4,5: Min=-2, Max=+1.5
- Band 8: Min=-2, **Max=+1** (tighter high limit on Band 8)

### WCDMA TX Power Range Measurement

All bands: start at 24 dBm, 39 steps of -2 dBm = dynamic range to -54 dBm
- Highest power level: +/-2 dB
- Common power levels: +/-4 dB
- Linearity drop: +/-1 dB

### WCDMA TX Power Reductions

| Band | HSDPA MPR | HSUPA MPR | Closed/Test/Normal Reductions |
|------|-----------|-----------|-------------------------------|
| 1 | 1.0 dB | 1.5 dB | All 0 (low/mid/high channels) |
| 2 | 1.0 dB | 1.5 dB | All 0 |
| 4 | 1.0 dB | 1.5 dB | All 0 |
| 5 | 1.0 dB | 1.5 dB | All 0 |
| 8 | 1.0 dB | 1.5 dB | All 0 |

### WCDMA TX EVM Measurement

All bands: 10 samples, using max power value
EVM limits: TBD (in the measurement limits section)

**Linux mapping:**
- All WCDMA TX calibration is CMT NVM
- Band 8 having 24.0 dBm max power vs 23.5 dBm for others is notable
  - Band 8 (900 MHz) has slightly higher allowed output
- HSUPA MPR 1.5 dB is the maximum power reduction for HSUPA operation
- HSDPA MPR 1.0 dB for HSDPA

---

## 22. Variant Configuration XMLs

### 059D0N6.xml (Czech QW Silver White)

| Field | Value |
|-------|-------|
| Description | czech_qw_silver_white |
| Type | RM-626 |
| Product code | 059D0N6 |
| SW version | 111.040.1511 |
| CORE | RM-626_111.040.1511_79u_prd.core.fpsx |
| ROFS2 | RM-626_111.040.1511_**15.01_Euro3_QW1**_79u_prd.rofs2.fpsx |
| ROFS3 | RM-626_111.040.1511_**C00.01**_79u_prd.rofs3.fpsx |
| UDA | RM-626_111.040.1511_**U01.01**_79u.uda.fpsx |
| APE | RM626_APE_ONLY_ENO_11w36_v0.037.fpsx |
| eMMC | RM-626_**M005**.23.emmc.fpsx |
| ProductProfile | RM-626_111.040.1511_**euro.01**_productprofile.xml |

DRM: WmDrmPd, HdmiOtpData, OmaDrm20, **HdmiHdcpKeySet** (HDCP for HDMI output!)

### 059G138.xml (Euro2 QW1 IT TIM Silver White)

| Field | Value |
|-------|-------|
| Description | RM-626 VAR EURO2_QW1 IT TIM SILVER WHITE |
| Product code | 059G138 |
| ROFS2 | RM-626_111.040.1511_**09.01_Euro2_QW1**_79u_prd.rofs2.fpsx |
| ROFS3 | RM-626_111.040.1511_**272.03_default**_79u.2012.20_prd.rofs3.fpsx |
| UDA | RM-626_111.040.1511_**272.01_default**_79u.2012.20.uda.fpsx |
| eMMC | RM-626_**M002**.23.emmc.fpsx |
| ProductProfile | RM-626_111.040.1511_**272.03_default**_productprofile.xml |

- "IT TIM" = Italian Telecom Italia Mobile operator variant
- MCC 272 = Republic of Ireland (or 222 for Italy -- 272.03 might be a Nokia variant code, not MCC)

### 0591821.xml (Euro QW Dark Grey)

| Field | Value |
|-------|-------|
| Description | euro_qw_dark_grey |
| Product code | 0591821 |
| ROFS2 | RM-626_111.040.1511_**01.01_Euro1_QW**_79u_prd.rofs2.fpsx |
| ROFS3 | RM-626_111.040.1511_**C00.01**_79u_prd.rofs3.fpsx (same as 059D0N6) |
| UDA | RM-626_111.040.1511_**U01.01**_79u.uda.fpsx (same as 059D0N6) |
| eMMC | RM-626_**M001**.23.emmc.fpsx |
| ProductProfile | RM-626_111.040.1511_**euro.01**_productprofile.xml (same as 059D0N6) |

**Key variant differences:**

| Component | 059D0N6 (Czech) | 059G138 (IT TIM) | 0591821 (Euro DG) |
|-----------|-----------------|-------------------|--------------------|
| Color | Silver White | Silver White | **Dark Grey** |
| Language pack (ROFS2) | Euro3_QW1 (15.01) | Euro2_QW1 (09.01) | Euro1_QW (01.01) |
| Customization (ROFS3) | C00.01 (generic) | 272.03 (operator) | C00.01 (generic) |
| UDA content | U01.01 (generic) | 272.01 (operator) | U01.01 (generic) |
| eMMC image | M005 | M002 | M001 |
| CORE firmware | **Identical** | **Identical** | **Identical** |
| APE firmware | **Identical** | **Identical** | **Identical** |

All three share:
- Same CORE (MCU) firmware: `RM-626_111.040.1511_79u_prd.core.fpsx`
- Same APE firmware: `RM626_APE_ONLY_ENO_11w36_v0.037.fpsx`
- Same DRM config: WmDrmPd, HdmiOtpData, OmaDrm20, HdmiHdcpKeySet
- Same security: RdCert v1.0, SimLock BB5, VariantCert v1.0
- Same SW version: 111.040.1511

**HDMI HDCP confirmation:** All variants include `HdmiHdcpKeySet` DRM solution, confirming
the E7 has HDMI output with HDCP key provisioning.

---

## 23. Refurbish Product Profiles

### 059D0N6 and 0591821 (euro.01)
```xml
Feature Index=56, Value=0
Feature Index=340, Value=0
```

### 059G138 (272.03 -- operator variant)
```xml
Feature Index=56, Value=0
Feature Index=271, Value=4   <-- UNIQUE to this variant
Feature Index=340, Value=0
```

The **only difference** across all three DCPs in the Refurbish directory:
- 059G138 has an extra feature flag: **Index 271 = 4**
- Nokia product profile indices are not publicly documented
- Index 271 likely relates to operator customization (TIM Italy branding/settings)
- Index 56 and 340 (value 0) are common to all variants

---

## 24. Cross-Variant Diff Summary

### ProductConfiguration/ (21 files)

**All 21 ProductConfiguration XML files are IDENTICAL across all three variants.**

This is confirmed by file sizes being byte-identical:
- All files have the same sizes and the same 2012-08-11 or 2013-02-22 timestamps
- The 059G138 variant has 2013-02-22 timestamps (later packaging date) but same content

This means: **hardware calibration is product-level (RM-626), not variant-level.**
All Nokia E7 RM-626 units share the same factory calibration parameters regardless of
color, language, or operator. Per-unit calibration (actual measured values) is stored
in CMT NVM, not in the DCP.

### VariantConfiguration/ (1 file each)

Differences:
- Product code (059D0N6 / 059G138 / 0591821)
- Description (color + region)
- ROFS2 language pack (Euro1/Euro2/Euro3)
- ROFS3 customization (generic C00 vs operator 272)
- UDA content (generic U01 vs operator 272)
- eMMC image code (M001/M002/M005)

### Refurbish/ (1 file each)

- 059D0N6 and 0591821 share the same `euro.01` product profile (2 features)
- 059G138 has `272.03` profile with 3 features (extra Index 271=4)

---

## 25. Linux Driver Mapping Summary

### Parameters Directly Useful for Linux/DTS

| DCP Parameter | Value | Linux Driver | DTS Property |
|---------------|-------|-------------|-------------|
| Display 640x360 | nHD landscape | omapdrm/omapfb | panel resolution |
| Prox low threshold | 90 | apds990x | avago,prox-low-threshold |
| Prox high threshold | 140 | apds990x | avago,prox-high-threshold |
| Prox offset | 10 | apds990x | avago,prox-offset |
| ALS reference | 687 | apds990x | calibration target |
| ALS coeff range | 0.6--1.4 | apds990x | ga parameter bounds |
| Touch X res | 180 | atmel_mxt_ts | touchscreen-size-x |
| Touch Y res | 320 | atmel_mxt_ts | touchscreen-size-y |
| Touch phys X | 40.0mm | atmel_mxt_ts | touchscreen-x-mm |
| Touch phys Y | 71.0mm | atmel_mxt_ts | touchscreen-y-mm |
| WLAN RX trace loss | 0x10 (1.6dB) | wl12xx NVS | rx_trace_loss |
| WLAN TX trace loss | 0x00 | wl12xx NVS | tx_trace_loss |
| WLAN BiP ref power | 0x80 | wl12xx NVS | tx_bip_ref_power |
| WLAN BiP PD voltage | 0x2201/0xA901 | wl12xx NVS | tx_bip_ref_pd_voltage |
| WLAN power limits | See table | wl12xx NVS | per-rate limits |
| LED channels 2-6 | HOME/QWERTY/CHG/TORCH | lp5521 | led-sources |
| Vibra range | 5-95% | twl4030-vibra | duty-cycle limits |
| Vibra direction | Both | twl4030-vibra | bidirectional LRA |
| Battery fixed | true | simple-battery | non-removable |
| ADC channels | 1-7, 24 | twl4030-madc | IIO channels |
| BT type | WL1283 (TI) | hci_ll / btwilink | compatible |
| GSM bands | 850/900/1800/1900 | ofono/isimodem | modem config |
| WCDMA bands | 1/2/4/5/8 | ofono/isimodem | modem config |
| Camera main | 8MP | BCM2727 ISP | (blocked) |
| Camera front | VGA 640x480 | smiapp/CSI-2 | sensor node |
| HDMI HDCP | present | omapdss hdmi | hdcp keys |

### Parameters NOT Directly Useful (Factory Test Only)

- Display test patterns (hwconf)
- RF calibration tables (GSM TX/RX, WCDMA TX/RX)
- Self-test IDs and sequences
- Antenna attenuation compensation (CA-158RS cable)
- EM calibration limits (gain/offset acceptance ranges)
- FM radio test timeout
- Call control server version

### Key Architectural Insights from DCP

1. **Dual PA vendor design**: Renesas and Skyworks PAs with separate calibration curves
   throughout all GSM and WCDMA bands. Factory detects which PA is installed and
   loads the appropriate curve.

2. **Diversity RX**: WCDMA has both Main and Diversity receive chains for all 5 bands.
   This is a second receive antenna, improving 3G reception.

3. **HDMI with HDCP**: DRM configuration confirms HDMI output with content protection.
   OMAP3630 has HDMI via DSS overlay; needs HDCP key provisioning.

4. **WL1271 dual FEM**: Two front-end module configurations (FEM 0 and FEM 1) with
   different PD voltage references and rate offsets. The PD voltage difference
   (FEM0=0x2201 vs FEM1=0xA901) is significant and must match the actual FEM hardware.

5. **BCM2727 is "IVE3"**: Self-test 0x05 confirms the camera ISP is called IVE3 internally,
   communicates via "Messi-16" (16-bit parallel) lines, and requires code download at boot.

6. **TPA6130A2 headphone amp**: Self-test 0xD1 confirms the chip and that silicon
   revision 0x0010 = TPA6130A2 (not TPA6140 as previously speculated).

7. **"Alexander" display**: Self-test 0x3B calls the display controller "Alexander",
   which is Nokia's internal name for the display subsystem.

8. **Fixed battery**: RM626.xml confirms non-removable battery (FixedBattery=true).
   The BQ2415x charger handles all charge management.

9. **No EANC**: Audio config explicitly disables Electronic Active Noise Cancellation,
   meaning there is no dedicated noise-cancelling microphone.

10. **Touch controller native 180x320**: The Atmel mXT reports in portrait coordinates
    (180x320) while the display is landscape (640x360). The driver must handle rotation
    and scaling.
