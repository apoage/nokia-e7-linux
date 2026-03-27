# Archive Dump Inventory

## Location
`/home/lukas/ps3/e7/archive-dump/` — 10.5GB, 6,051 files

## Directory Structure

### Nokia Suite (1.4GB)
- `Zálohy/E7-00_2018-11-23.nbu` — 1.4GB full phone backup (contacts, SMS, MMS, apps, settings)
- `MMS/` — saved MMS messages with VCF contacts and media attachments
- `copy to Microsoft SkyDrive´2018-11-23T17-44-42E7-00.txt` — backup manifest

### mobilní telefony/nokia/flash images/Products/RM-626/ (1.6GB)
Factory flash package for product code 059D0N6 (Czech QW Silver White):
| File | Size | Description |
|------|------|-------------|
| `RM-626_111.040.1511_79u_prd.core.fpsx` | ~1.2GB | CORE image (already extracted in Task 002) |
| `RM-626_111.040.1511_15.01_Euro3_QW1_79u_prd.rofs2.fpsx` | — | ROFS2 language pack |
| `RM-626_111.040.1511_C00.01_79u_prd.rofs3.fpsx` | — | ROFS3 user customization |
| `RM-626_111.040.1511_U01.01_79u.uda.fpsx` | — | UDA (user data area) |
| `RM-626_M005.23.emmc.fpsx` | — | eMMC factory image |
| `RM626_APE_ONLY_ENO_11w36_v0.037.fpsx` | 4.6MB | **APE-only firmware** (OMAP3630 bootloader, "Elf2flash 11.13", "CMT", "BB5") |
| `RM626_059D0N6_111.040.1511_015.dcp` | ~50KB | **DCP: Device Configuration Package** (ZIP of 23 XML configs) |
| `RM626_059D0N6_111.040.1511_015.vpl` | — | VPL: Variant Packing List (XML manifest of flash files) |
| `RM626_059D0N6_111.040.1511_015_signature.bin` | — | Flash signature |
| `RM626_059D0N6_size_hwc_v3.0.bin` | 212B | HWC: Hardware Configuration ("CHWC" magic, product code 059D0N6) |
| `RM626_059D0N6_size_ccc_v3.0.bin` | 472B | CCC: Country/Carrier Config ("CCCC" magic, signed) |
| `RM626_data_block` | 4KB | Device certificate XML (UNIQUEID, PUBLICKEY, KEYDATA — UTF-16LE) |
| `ive3_otp_template_production.bin` | 40B | BCM2727 IVE3 OTP template ("OTP0" magic, production config) |
| `RM626_Default_Verification_File.spr` | — | SPR verification file |
| `simlock_3gstandard_bb5.bin` | — | SIM lock configuration |
| `ledcontrol.ini` / `nvd.ini` / `wap_app_mappings.ini` | — | Shared product INI files |

### Nokia-E7/ (7.2GB)

#### InfinityBEST/ (160MB)
Phone service/flash tool with data from both phones:
- `Backup/Cert/354864048650007_CRT.rpl` — Unit A certificate backup
- `Backup/Cert/354864049067334_CRT.rpl` — Unit B certificate backup
- `_CRT_2nd.rpl` variants — second certificate backups
- `phdata/pc/` — 110 product code files (.pc) for various Nokia models
- `Best.ini` / `Settings.ini` — model capability flags
- `SelfTest.ini` — selftest configuration

#### RM-626/ (1.2GB), RM-626_cooked/ (1.4GB), RM-626-czsilver/ (1.6GB), RM-626-czsilver-patched/ (1.6GB)
Multiple ROM variants:
- **RM-626/** — base flash images
- **RM-626_cooked/** — modified ROM (product code 059G138, Euro2 QW1 region)
- **RM-626-czsilver/** — Czech silver variant (same as mobilní telefony copy?)
- **RM-626-czsilver-patched/** — patched Czech variant
- Each contains CORE + ROFS2 + ROFS3 + UDA + eMMC FPSX files

#### RM-626_delight/ (413MB)
"E7 Delight v1.0" custom firmware:
- English and Czech UI variants
- Euro3 QW3 keymapping 17
- Separate CORE + ROFS2 + ROFS3 + UDA images

#### Other
- `jaf/` (18MB) — JAF (Just Another Flasher) v1.98.62 installer + model INI
- `jaf ini 2012/` — updated JAF model database
- `jaif++/` — enhanced JAF model database tool
- `phone patches/networking_improvements.SIS` — Symbian networking patch (310KB)
- `prtograms/` — Symbian apps: Google Maps, Opera Mobile 12, QuickMark, profimail, etc.
- `service manualy/` (85MB) — 3 PDFs: service manual parts 1-2/3-4, schematics
- `NokiaCooker_3.4/` (466MB) — Nokia Cooker ROM kitchen (already analyzed)
- `kontakty jana/`, `kontakty kulas/` — contact exports
- `mail/` — email exports

## DCP Contents (extracted to /tmp/e7-dcp/)

The DCP is the most actionable find. 23 XML files:

### Hardware Configuration
| File | Key Data |
|------|----------|
| `hwconf_RM-626.xml` | Light channels: HOME=2, QWERTY=3, CHARGINGN=4, CHARGINGG=5, TORCH=6. Display: 640x360. Proximity: low=90, high=140, offset=10 |
| `hw_als_conf.xml` | ALS reference=687, calibration coeff 0.6-1.4 (Pupumon sensor) |
| `ALSTuningWizard.xml` | ALS Dipro reference=1, coeff 0.1-3.0 |
| `camera.xml` | Camera 1: 8MP (SOS type), Camera 2: VGA. Tests: defective pixel, couplet, brightness, vignetting, blemish |
| `RM-626_TouchPad.xml` | ID=254, resolution 180x320, physical 40.0x71.0mm, actual 39.8x71.4mm |
| `RM-626_ADC_reading.xml` | ADC channels: 1=BattV, 2=ChargerV, 3=ChargerI, 4=BSI(Ohms), 5=BattTemp(K), 6=Headset, 7=Hook, 24=BackupBatt |

### RF / Wireless
| File | Key Data |
|------|----------|
| `RM-626_wlan_bob.xml` | WL1271 TX power tuning: FEM0/FEM1, CCK/OFDM power limits, BiP ref points, PD vs rate offsets, trace insertion losses |
| `RM-626_fm_radioconf.xml` | FM radio timeout=20000ms |
| `Ratbandconfiguration.xml` | RAT band configuration |
| `RM-626_linko_rf_15122010.xml` | RF general config |
| `RM-626_linko_rf_gsmrx_23112010.xml` | GSM RX tuning |
| `RM-626_linko_rf_gsmtx_02112010.xml` | GSM TX tuning |
| `RM-626_linko_rf_wcdmarx_23112010.xml` | WCDMA RX tuning |
| `RM-626_linko_rf_wcdmatx_17032011.xml` | WCDMA TX tuning |

### Audio / Vibra
| File | Key Data |
|------|----------|
| `RM-626_Audio_configuration.xml` | Audio loops: HpInAvOut, AvInAvOut, AvInIhfOut. Inputs: Muted/HdMic/HfMic/HpMic. Outputs: HpEar/HdEar/HfEar/XEar/HdStereo/HfStereo/IhfEar. Vibra: 5-95%, bidirectional |
| `rm-626_emcal.xml` | EMC calibration |

### System
| File | Key Data |
|------|----------|
| `RM626.xml` | Flash: BB5 init, USB+FBUS media, SIM lock type=BB5V2, ESN: IMEI+BT+WLAN, fixed battery, IBAT disabled |
| `RM-626_selftests_12012012.xml` | Factory selftest sequence: 30+ tests (IDs 0x02-0xE5) |
| `bluetooth.xml` | BT configuration |
| `Call_control.xml` | Call control |
| `RM-626_CA158RS_AttenuationConfiguration_22052010.xml` | Attenuation config |

### Variant
| File | Key Data |
|------|----------|
| `Refurbish/RM-626_111.040.1511_euro.01_productprofile.xml` | Product profile |
| `VariantConfiguration/059D0N6.xml` | Variant-specific config |

## Already Analyzed
- CORE/ROFS/eMMC FPSX → Task 002 (firmware extraction)
- Service manuals + schematics → docs/service-manual/ (6 files)
- Nokia Cooker → docs/nokia-cooker-analysis.md (FPSX crypto decoded)
- eMMC image → Task 007 (empty factory FAT32, no useful data)
- Firmware strings → Task 019 (string mining)

## IMEIs (from InfinityBEST cert backups)
- **Unit A:** 354864048650007
- **Unit B:** 354864049067334
