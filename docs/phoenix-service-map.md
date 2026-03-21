# Nokia Phoenix Service Component Map

Extracted from Tuscon/Phoenix DLLs (product_menu_a.dll, cmntucsonui.dll).

## Memory Access Services

| Service ID | Component Name | Function |
|-----------|---------------|----------|
| ID_MEMTOOL_PERMMEM (30112) | CNMP.Cmn.PermMemFN.1 | **Permanent Memory** read/write |
| (near 30112) | CNMP.Cmn.RegMemFN.1 | **Register Memory** — hardware register access |
| ID_DSPMEM (30098) | CNMP.GsmG.DspMemFN.1 | DSP memory read |
| ID_MEMTOOL_ADSPMEM (30110) | CNMP.Hhk.AdspTestFn.1 | Audio DSP memory |
| ID_GENIOGPIOCTRL (30105) | CNMP.GsmC.GenioToolFN.1 | **GENIO & GPIO Control** — pad mux! |
| - | CNMP.CmnI.ADCReadFN.1 | ADC register reading |
| - | CNMP.Cmn.WlanMemoryDumpFN.1 | WLAN memory dump |
| - | CNMP.Hip.EepromFN.1 | EEPROM access |
| - | CNMP.CmnI.NvdFN.1 | Non-volatile data |

**ALL memory services require R&D dongle authentication** (`FSdongle="1RD"`).

## DK2 Memory Functions (cmntucsonui.dll)
- `DK2ReadMemory` — Read memory via DES Key box
- `DK2WriteMemory` — Write memory via DES Key box
- Uses `DK2WIN32.DLL` for DES Key hardware interface

## Key Implication for PADCONF

**CNMP.Cmn.RegMemFN.1** (Register Memory) is EXACTLY what reads PADCONF.
**CNMP.GsmC.GenioToolFN.1** (GENIO & GPIO Control) is what configures pad mux.

These services exist on the phone but are **blocked by the firewall** (ISI resource
returns ERROR 0x01) because they require R&D dongle authentication.

The ISI firewall (resource 0x43) is the gatekeeper. Firewall msg 0x02/0x04
return config data. The question: can we authenticate without a physical dongle?

## Other Interesting Services

| Component | Function |
|-----------|----------|
| CNMP.Cmn.FileManagerFN.1 | Phone Browser / file access |
| CNMP.CmnI.PhoneInfoFN.1 | Phone info (WORKING over ISI) |
| CNMP.ISI.MobileTerminalControllerFN.1 | MTC (terminal control) |
| CNMP.CmnI.FactorySetFN.1 | Factory settings |
| CNMP.CmnI.SecurityCodeFN.1 | Security code access |
| CNMP.Cmn.ProductProfileFN.1 | Product profile (PP) |
| CNMP.Cmn.SimcardFN.1 | SIM card operations |
| CNMP.Cmn.Dct4SimLockFN.1 | SIM lock (DCT4) |
| CNMP.Cmn.AMS.FlashFN.1 | Flash operations |
| CNMP.UsbFpi.1 | USB flash programming interface |
