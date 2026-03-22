# Nokia ISI Resource IDs (from Symbian Source)

Source: `modemadaptation/connectivitylayer/isimessage/symbianisamacroif/include/wgmodem25/`
Extracted from: `code_FCL,sf,incubator,modemadaptation.hg.bundle` (SymbianOpenSource archive)

## Host-Side Resources (Application Processor)

| ID | Name | Description | Our Scan Result |
|----|------|-------------|-----------------|
| 0x02 | PN_SMS | Short Message Services | not scanned |
| 0x06 | PN_SS | Supplementary Services | not scanned |
| 0x10 | PN_COMMGR | Indication subscription | no response |
| 0x16 | PN_CSD | Circuit Switched Data | not scanned |
| 0x1B | PN_PHONE_INFO | Phone Information | **WORKS!** (IMEI, version, 16+ msgs) |
| 0x31 | PN_GPDS | GPRS server | error 0x01 |
| 0x32 | PN_GSS | GSM Stack server | error 0x01 |
| 0x43 | PN_NVD | Non-Volatile Data Server | responds (msg 0x01-0x07) |
| 0x8C | PN_UICC | Universal IC Card | error 0x01 |
| 0xD9 | PN_PIPE | Pipe messages | not scanned |

## Modem-Side Resources (RAPUYAMA) — NEED DEVICE 0x60

| ID | Name | Description | Our Scan Result |
|----|------|-------------|-----------------|
| 0x8E | PN_AT_MODEM | Modem AT server | error 0x01 |
| 0x90 | PN_MODEM_LCS | Modem LCS server | not tested on 0x60 |
| 0x91 | PN_MODEM_TEST | Modem Test server | NOT_REACHABLE (both devs) |
| 0x92 | PN_MODEM_NVD | **Modem Non-Volatile Data** | NOT_REACHABLE |
| 0x93 | PN_MODEM_PERM | **Modem PERManent Data** (PM READ!) | NOT_REACHABLE |
| 0xB7 | PN_RF_HAL | RF control and tuning | NOT_REACHABLE |
| 0xC2 | PN_MODEM_MCE | Modem MCE server | not tested |
| 0xC3 | PN_MODEM_MON | Modem Monitor server | not tested |
| 0xC8 | PN_MODEM_NETWORK | Modem NET server | not tested |
| 0xC9 | PN_MODEM_CALL | Modem CALL server | not tested |
| 0xCB | PN_MODEM_GAN | 3GPP GAN Protocol Stack | not tested |
| 0xEC | PN_COMMON_RF_TEST | Common RF test server | not tested |
| 0xEE | PN_WCDMA_RF_TEST | WCDMA RF test server | not tested |
| 0xF1 | PN_GSM_RF_TEST | GSM test server | responded on 0x00 (msg 0xCB!) |

## Key Discovery: PN_MODEM_PERM (0x93) = PM Read!

The Permanent Memory read service is **PN_MODEM_PERM at resource ID 0x93**.
It runs on the modem (device 0x60). Currently NOT_REACHABLE from our USB connection.

## Phonet Device Addresses

| ID | Name | Description |
|----|------|-------------|
| 0x00 | PN_DEV_HOST | Application processor (Symbian) |
| 0x10 | PN_DEV_PC | PC (us, via USB) |
| 0x60 | PN_DEV_MODEM | Modem (RAPUYAMA) |
| 0x26 | PN_MEDIA_MODEM_HOST_IF | Modem-host interface |

## Routing Problem

All modem resources (0x90+) return ENTITY_NOT_REACHABLE on BOTH device 0x00 and 0x60.
The Phonet routing from USB → modem is not working. Possible causes:
1. USB CDC Phonet only routes to the application processor (host), not the modem
2. A routing setup message is needed to enable modem forwarding
3. Nokia Suite does a handshake that enables modem routing
4. The modem ISI bus is physically separate from the USB Phonet path

## NVD Protocol (resource 0x43 = PN_NVD)

From `nvdisi.h`:
- Version: 000.002
- NVD_SET_DEFAULT_REQ (0x01) / NVD_SET_DEFAULT_RESP (0x02) — only message pair
- Status codes: OK(0), FAIL(1), NONE(2)
- What we thought was a "firewall" is actually the NVD (Non-Volatile Data) reset service

## Unexplored Areas

### Resources We Haven't Tried on Device 0x60 (Modem)
- 0x90 PN_MODEM_LCS (Location services)
- 0xC2 PN_MODEM_MCE (MCE = Modem Control Entity)
- 0xC3 PN_MODEM_MON (Monitor — might have debug access!)
- 0xCB PN_MODEM_GAN (GAN protocol)
- 0xEC PN_COMMON_RF_TEST
- 0xEE PN_WCDMA_RF_TEST

### Unknown Resources from Our Scan (responded but not identified)
- 0x0E — VERSION FAFA (not in any header)
- 0x2C — VERSION 04.00 (not in any header)
- 0x5E/0x76 — VERSION 02.55 (not in any header)
- 0x5F/0x77 — VERSION 00.00 (not in any header)
- 0xE2/0xE3 — PRODUCT_INFO_RESP (not in any header)

### Source Code Not Yet Searched
- `code_FCL,sf,os,commsfw.hg.bundle` — communications framework
- Forum and mailing list archives (not yet extracted)
- The `oss.zip` in `symbian_202201/` — might have newer source

### Kernel Source Findings (for HaRET approach)
- `Kern::Restart` reads `TRomHeader.iRestartVector` and jumps to it
- `iRestartVector` at offset 0x84 in TRomHeader (e32rom.h line 132)
- Can be overwritten via WriteShadowMemory
- Exception vectors at ROM offset 0x00 (B instructions to handlers)
- `DPlatChunkHw::New` examples in kernel test code
