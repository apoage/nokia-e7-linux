# Nokia E7 NBU Backup File Analysis

**File:** `archive-dump/Nokia Suite/Zálohy/E7-00_2018-11-23.nbu`
**Size:** 1,397,471,066 bytes (1.4 GB)
**Date:** 2018-11-23 (Nokia Suite backup)

## File Header

The NBU file begins with a 16-byte magic signature followed by metadata:

```
Offset  Hex                              Notes
------  ---                              -----
0x00    cc52 33fc e92c 1848 afe3 3630    Magic signature (12 bytes)
0x0c    1a39 4006                        (4 bytes, purpose unknown)
0x10    0400 0000                        Version or type (LE uint32 = 4)
0x14    8cb4 4b53                        Possibly timestamp or checksum
0x18    (zero-padded to 0x73)
0x74    0829 4b2b 0e89 174b 9773 17c2   16-byte GUID — appears in footer too
        4c1a dbc8                        (backup session identifier)
0x84    2387 7b00 0000 0000              Offset: 0x7b8723 (file section directory)
0x8c    6000 0000                        0x60 = 96 (section count or flags)
0x9c    0110 0000                        Section type 0x1001
0xa2    (first data section begins — ASCII text)
```

The 16-byte GUID at offset 0x74 (`0829 4b2b 0e89 174b 9773 17c2 4c1a dbc8`)
reappears in the footer directory at offset 0x534bb51a, linking header to footer.

## File Footer (Device Identity + Section Directory)

The file ends with a structured footer starting at approximately offset 0x534bb49a:

### Device Identification Block

| Field | Value | Notes |
|-------|-------|-------|
| IMEI | `354864048650007` | UTF-16LE, 15-digit IMEI |
| Type Designation | `RM-626` | Nokia E7-00 hardware type |
| Product Name | `E7-00` | Marketing name |
| Firmware Version | `111.040.1511` | Symbian Anna final release |
| Language | `cs` | Czech locale |

**Firmware 111.040.1511** confirms this is the Symbian Anna release for the E7
(production build 40, week 15 of 2011).

### Section Directory

The footer contains a section directory with entries structured as:
- 16-byte GUID (section type identifier)
- 8-byte offset (LE uint64, absolute file position)
- 8-byte size (LE uint64, section data length)
- 4-byte count (LE uint32, number of items)
- 4-byte flags/type

Identified section GUIDs and their approximate offsets:

| GUID | Offset | Size | Items | Likely Content |
|------|--------|------|-------|---------------|
| `0829 4b2b 0e89 174b...` | 0x74 | 0x7b8723 | 96 | Files/settings section |
| `0e3d 5f65 af22 7848...` | 0x7b87a7 | ~0x51641ab7 | 475 | Application data + installed apps |
| `efd4 2ed0 a351 3847...` | 0x51dfa388 | 0x7c78 | 98 | Calendar/situations |
| `16cd f8e8 235e 5a4e...` | 0x51e02010 | 0x4fb | 3 | Small metadata section |
| `5c62 973b dca7 5441...` | 0x51e0251b | 0x44 | 1 | Single-entry section |
| `617a efd1 aabe a149...` | 0x51e0256f | 0x7f51a | 787 | Contacts/messages (large) |
| `471d d465 efe3 3240...` | 0x51e81a99 | 0x180686 | 14 | Media content (photos) |
| `7f77 9056 31f9 5749...` | 0x5200212f | 0xaf3 | 10 | Bookmarks |
| `2df5 686b 1f4b 224a...` | 0x52002e9c | 0x14b85e0 | 0 | Bulk data (~21MB, filesystem) |

## Content Sections

### Section 1: eZSpot WiFi Hotspot Log (0xa2 - ~0x7b5c00)

The first data section is an ASCII debug log from the `eZSpot` WiFi hotspot
application (JoikuSpot). This is the application's runtime log backed up from
`C:\data\ezspot_log_app.txt`.

**Technical findings from log:**
- **Bluetooth tethering interface:** `Bttint4` — used as public uplink
- **WLAN interface:** `wlan` at 192.168.2.1 (private hotspot network)
- **IPv6:** Supported, link-local `fe80::92cf:15ff:fe40:4100`
- **WLAN MAC:** `90:cf:15:40:41:00` (derived from IPv6 SLAAC address)
- **ESOCK protocols:** 25-26 installed (BTLinkManager, L2CAP, RFCOMM, SDP, eZSpotNAPT, etc.)
- **GPRS tethering:** Public IAP ID 2, using Cloudflare DNS (1.1.1.1 / 1.0.0.1)
- **WLAN hotspot:** IAP name `NcBB1`, WPA passphrase `Qweas`, gateway 192.168.2.1
- **DHCP service:** `!eZSpotDHCPServ_0x20037520`
- **Premium version** of eZSpot confirmed

### Section 2: Helix Media Engine Configuration (~0x10000)

Contains Helix/RealPlayer engine configuration for video playback:
- YUV coefficients: `0x1000002`
- YUV sampling pattern: `0x8` (4:2:0)
- Payload format ID: `0x10283354`
- Video data unit encapsulation: `0x10000`

### Section 3: Media Player Debug Log (~0x8000)

`CMPEngine` / `CMPFFHandler` debug trace data from the Helix media player:
- Shows FFState transitions (FFState_Ready)
- Seek operations and packet supply chains
- DTDR (Debug Trace Data Record) format with timestamps

### Section 4: File Backup Entries (~0x7b5c30)

Individual file backup entries with the format:
```
[2 bytes: path length] [UTF-16LE: \\C:\path\to\]
[2 bytes: filename length] [UTF-16LE: filename.ext]
[metadata: 7 bytes type + size + timestamp]
[file data or compressed data]
```

**Selected backed-up files (C:\data\):**
- `DDISCVRY.DPS` — Bluetooth device discovery database
- `ezspot_log_app.txt` — eZSpot hotspot log (24,445 bytes, see Section 1)
- `hxlog.txt` — Helix media player log
- `HXMDEngine_3_2.cfg` — Helix multimedia engine configuration
- `hxmetadata_archive.txt` — Media metadata archive
- `hxthumbnail_archive.txt` — Thumbnail archive
- `MdfPluginArchive.txt` — MDF (Mobile Device Framework) plugin archive
- `MtkEngine_3_2.cfg` — MTK engine configuration
- `plugin_archive.txt` — Plugin listing
- `powlite_fm.mbm` — FM radio UI bitmap resource
- `prozkoumat - excel_ceatina.xlsx` — User spreadsheet (Czech: "to explore")

### Section 5: Symbian Situations (Context Profiles) (~0x7b5000)

Symbian "Situations" system — context-aware profile automation:
- Profile types: `meeting`, `work`, `group`
- Properties: `uid`, `conditions`, `actions`, `icon`, `payload`
- Actions include: `beep`, `manual`
- UI resources: `qrc:/gfx/uilib/situation_meeting.svg`
- Start/stop conditions with `negative` flag support

### Section 6: Installed Applications List (~0x7b87d3)

Complete application registry with UID, name (Czech locale), and executable path.
Format: `0xUID:  Name                    Drive:\sys\bin\executable.exe`

#### ROM Applications (Z:\sys\bin\) — Symbian Built-in

| UID | Name | Executable |
|-----|------|-----------|
| 0x10207a28 | Nastaveni prislusenstvi | cfmwecaseserviceui.exe |
| 0x10005234 | Uzivatelska prirucka | Cshelp.exe |
| 0x10207218 | akncapserver | akncapserver.exe |
| 0x102819bc | Predvolene aplikace | defaultappserver.exe |
| 0x20012c9c | Exchange ActiveSync | EasUiProvider.exe |
| 0x200022ebd | Signal boost | ganapp.exe |
| 0x101f4cd5 | Protokol (Log) | Logs.exe |
| 0x101f4cd6 | E-mail (editor) | MsgMailEditor.exe |
| 0x101f4ce4 | E-mail (viewer) | MsgMailViewer.exe |
| 0x1000599d | Poznamky | NpdViewer.exe |
| 0x101f6de5 | Spravce pristroje | NSmlDMSync.exe |
| 0x2000fdc3 | PopupClock | PopupClock.exe |
| 0x101f874a | Prezentace | SVGTViewerApp.exe |
| 0x10207839 | UISettingsSrv | UISettingsSrv.exe |
| 0x20034935 | Zalosky (Bookmarks) | vbselector.exe |
| 0x200122a4 | Nast. telef. | Welcome2.exe |
| 0x102081ef2 | aknnfysrv | aknnfysrv.exe |
| 0x101f875a | Instalator | SWInstSvrUI.exe |
| 0x101f8512 | Instalace | AppMngr2.exe |
| 0x100059b5 | Autolock | Autolock.exe |
| 0x2001fd3d | bttoggle | bttoggle.exe |
| 0x2001fe5c | paintcursor | paintcursor.exe |
| 0x10005951 | Bluetooth | btui.exe |
| 0x100059d2 | Ulozit certifikat | CertSaver.exe |
| 0x10204375 | CfmwLauncher | cfmwlauncher.exe |
| 0x102072c4 | Stahnout (FOTA) | fotaserver.exe |
| 0x10283322 | !LockApp | !LockApp.exe |
| 0x2001e25e | Predictive Setting | peninputpredictivesetting.exe |
| 0x10005a32 | Motivy | Psln.exe |
| 0x100056cf | screensaver | screensaver.exe |
| 0x100058f4 | Startup | Startup.exe |
| 0x100058f3 | SysApp | SysApp.exe |
| 0x102068e2 | USB | usbclasschangeui.exe |
| 0x2000023d | Zip manager | ZipManager.exe |
| 0x10201b00 | Poslech zprav | messagereaderapplication.exe |

#### E:\sys\bin\ — User-installed to Mass Storage

| UID | Name | Executable |
|-----|------|-----------|
| 0x2006e99c | FExplorer Pro | FExplorer.exe |
| 0x200040c7f | Task Manager | Prj_0x200040C7F.exe |
| 0x2000b1a5 | Python | Python.exe |

#### C:\sys\bin\ — User-installed to Internal Memory

| UID | Name | Executable |
|-----|------|-----------|
| 0x2005b021 | JoikuSpot 2012 Edition | JoikuSpot_0x2005B021.exe |
| 0x200044233b | SwipeUnlock | SwipeUnlock.exe |
| 0x20029b71 | Pocasi (Weather) | Sweat.exe |
| 0x2003fd1a | pwpublisher | pwpublisher.exe |
| 0x2003fd23 | Detaily mista | pdcservice.exe |
| 0x200261c9 | storeinstaller | storeinstaller.exe |
| 0x20022d07f | Obchod (Store) | ovistore_2002D07F.exe |
| 0x200261c5 | nokiastoreclient | nokiastoreclient.exe |
| 0x20026140 | nokiaaserver | nokiaaserver.exe |
| 0x200261c1 | noasecurestorageserver | noasecurestorageserver.exe |
| 0x2002615c | crossenablerserver | crossenablerserver.exe |
| 0x2002699f | Social | Launcher_0x200269F.exe |
| 0x2002f954 | Social (contacts) | contactlauncher_0x2002F954.exe |
| 0x200269a2 | Socialni site | mediabridge_0x200269A2.exe |
| 0x20030c90 | Shazam | Shazam.exe |
| 0x102829b8 | wrtserviceresolver | wrtserviceresolver.exe |
| 0x102829f1 | wrtsfwbackup | wrtsfwbackup.exe |
| 0x200002c0 | Quickoffice | QO_FileManager.exe |
| 0x200002c2 | Quicksheet | Quicksheet.exe |
| 0x200002c1 | Quickword | Quickword.exe |
| 0x200002c3 | Quickpoint | Quickpoint.exe |
| 0x2000745e | Quickoffice (PDF) | QuickPDFStub.exe |
| 0x20034686 | textparserserviceinstalle | textparserserviceinstaller.exe |
| 0x2003ddc3 | Microsoft Konf... | (truncated in scan) |

### Section 7: File Path Registry

Backed-up files include various Symbian data directories:

**C:\data\ files:**
- `DDISCVRY.DPS` (Bluetooth discovery)
- `ezspot_log_app.txt` (WiFi hotspot log)
- `hxlog.txt` (Helix log)
- `HXMDEngine_3_2.cfg` (multimedia config)
- `hxmetadata_archive.txt`
- `hxthumbnail_archive.txt`
- `MdfPluginArchive.txt`
- `MtkEngine_3_2.cfg`
- `plugin_archive.txt`
- `powlite_fm.mbm` (FM radio bitmaps)

**C:\data\situations2\ files:**
- `runtime` — Situations engine runtime state
- `situations` — Situation profile definitions

**C:\data\pyfonts\ files:**
- `small.fnt` — Python font file (confirms Python for S60 installed)

## SkyDrive Sync Report

**File:** `copy to Microsoft SkyDrive´2018-11-23T17-44-42E7-00.txt`
**Size:** 516 lines, Czech language

This is a log from Nokia Suite's "copy to SkyDrive" feature, documenting what
was synced to Microsoft's cloud.

**Content categories synced:**
1. **Contacts** (KONTAKTY): ~150 entries (names only, personal data)
2. **Calendar** (KALENDAR): 3 entries attempted; API error for events endpoint
3. **Photos** (FOTOGRAFIE): ~350 JPEG files, date range 2018-03-31 to 2018-10-04
   - `MeeGo.png` — interesting: suggests MeeGo/Harmattan interest
   - `AppList.png` — screenshot of installed applications
   - Several `Resized_*` files suggest photo editing on device

**Error noted:** Live API error when syncing calendar events:
```
Live code = request_url_invalid
URL: https://apis.live.net/V5.0//me/events/?access_token=XX
```

## Technically Interesting Findings

### WLAN MAC Address
Derived from IPv6 SLAAC in eZSpot log: **90:CF:15:40:41:00**
- This is the BCM4329 WiFi chip's MAC address
- Useful for cross-referencing with real hardware

### Firmware Build
**111.040.1511** — This is the final Symbian Anna release for the E7
- Build 40, internal version 1511
- Language: Czech (cs)

### Network Stack Protocols (25-26 installed)
```
00: (unnamed)
01: BTLinkManager
02: L2CAP
03: RFCOMM
04: SDP
05: eZSpotNAPT
(plus ~20 more standard protocols)
```

### Python for S60
Python runtime installed at `E:\sys\bin\Python.exe` with font files at
`C:\data\pyfonts\`. UID `0x2000b1a5` matches Python for S60 v2.0.

### JoikuSpot Premium
WiFi hotspot app at `C:\sys\bin\JoikuSpot_0x2005B021.exe` — Premium version
confirmed by log message "Premium version" at startup.

### Nokia Store Client Stack
Full Nokia/Ovi Store infrastructure present:
- `nokiastoreclient.exe` — Store UI
- `nokiaaserver.exe` — Authentication server
- `noasecurestorageserver.exe` — Secure storage
- `crossenablerserver.exe` — Cross-app enabler
- `storeinstaller.exe` — SIS installation handler

### QuickOffice Suite
Full office suite installed (C:\sys\bin\):
- Quickword, Quicksheet, Quickpoint, QuickPDF

## NBU Format Summary

The Nokia NBU format is a proprietary container with:

1. **16-byte magic** at offset 0x00 (not documented publicly)
2. **Session GUID** at offset 0x74 (16 bytes, links header to footer)
3. **Data sections** starting at ~0xa2, containing:
   - Raw file backups (path + metadata + data)
   - Application data (situations, settings)
   - Application list (UID registry)
   - Compressed content sections (zlib/deflate)
4. **Footer directory** in the last ~2KB with:
   - Device identity (IMEI, type, firmware, language)
   - Section index with GUIDs, offsets, sizes, and item counts
   - Session GUID cross-reference

File entries use a consistent structure:
```
[2B path_dir_len] [UTF-16LE path directory, UNC format \\C:\...]
[2B filename_len] [UTF-16LE filename]
[7B metadata: type + size fields]
[variable: file data or compressed stream]
```

Path format uses UNC-style notation: `\\C:\data\filename.ext` where the drive
letters are standard Symbian: C=internal, E=mass storage, Z=ROM.

## Relevance to Linux Porting

1. **WLAN MAC** confirms BCM4329 chip identity (matches `90:CF:15:*` OUI)
2. **Firmware 111.040.1511** identifies exact Symbian build for driver RE
3. **ESOCK protocol list** documents the network stack we need to replace
4. **Installed apps list** shows what services the device was actively using
5. **Situations/context profiles** document sensor integration patterns
6. **HXMDEngine config** reveals multimedia pipeline configuration

---
*Analysis date: 2026-03-02. Only structural/technical content documented.
Personal data (contacts, messages, photos) intentionally excluded.*
