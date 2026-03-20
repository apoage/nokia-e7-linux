# Nokia ISI Protocol Resource Map

Sources: oFono (GPL-2.0), libisi (GPL-2.0), Linux kernel (GPL-2.0)

## ISI Resource IDs (Phonet resource field)

| ID | Name | Description |
|----|------|-------------|
| 0x01 | PN_CALL | Call control |
| 0x02 | PN_SMS | SMS service |
| 0x06 | PN_SS | Supplementary services |
| 0x08 | PN_SECURITY (PN_SIM_AUTH) | SIM authentication |
| 0x09 | PN_SIM | SIM card access |
| 0x0A | PN_NETWORK | Network registration |
| 0x10 | PN_COMMGR | Communication manager |
| 0x15 | PN_MTC | Mobile Terminal Control |
| 0x1B | PN_PHONE_INFO | Phone info (IMEI, version, product) |
| 0x31 | PN_GPDS | GPRS data service |
| 0x32 | PN_GSS | General Stack Service |
| 0x43 | PN_FIREWALL | Firewall |
| 0x54 | PN_GPS | GPS service |
| 0x62 | PN_EPOC_INFO | Symbian/EPOC info service |
| 0x8C | PN_UICC | UICC (SIM card) |
| 0xB4 | PN_WRAN | WRAN (3G) |
| 0xC2 | PN_MODEM_MCE | Modem MCE |
| 0xC5 | PN_MODEM_INFO | Modem info |
| 0xC8 | PN_MODEM_NETWORK | Modem network |
| 0xC9 | PN_MODEM_CALL | Modem call |
| 0xDB | PN_NAMESERVICE | Name service (resource discovery) |

## Device IDs

| ID | Name | Description |
|----|------|-------------|
| 0x00 | PN_DEV_HOST | Host (application processor) |
| 0x10 | PN_DEV_PC | PC (USB connected) |
| 0x60 | PN_DEV_MODEM | Cellular modem |
| 0x6C | PN_DEV_SOS | Symbian or Linux OS |

## Key Message Types for Phone Info (PN_PHONE_INFO = 0x1B)

| ID | Name |
|----|------|
| 0x00 | INFO_SERIAL_NUMBER_READ_REQ |
| 0x01 | INFO_SERIAL_NUMBER_READ_RESP |
| 0x02 | INFO_PP_READ_REQ (Product Profile) |
| 0x03 | INFO_PP_READ_RESP |
| 0x07 | INFO_VERSION_READ_REQ |
| 0x08 | INFO_VERSION_READ_RESP |
| 0x15 | INFO_PRODUCT_INFO_READ_REQ |
| 0x16 | INFO_PRODUCT_INFO_READ_RESP |

## Protocol Stack

```
USB (CDC/Phonet)
  └── Phonet header (8 bytes: rdev, sdev, res, length, robj, sobj)
       └── ISI message (trans_id, msg_id, submsg_id, data)
            └── Service-specific payload (subblocks)
```

## Missing / Unknown

The following are NOT documented in oFono/libisi:
- PM (Permanent Memory) read/write service — what resource ID?
- Hardware register read — does it exist?
- File browser service — PN_EPOC_INFO or separate resource?
- Test mode service — how Phoenix enters test mode
- Memory dump service — what Phoenix Phone Browser uses

These are Nokia-proprietary ISI resources not implemented in open-source code.
The Phonet/FBUS command from nokiahacking.pl simlock thread used raw FBUS,
not ISI — suggesting PM access might use a different protocol layer.
