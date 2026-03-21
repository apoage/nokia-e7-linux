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

## Live Scan Results (Nokia E7 RM-626, 2026-03-21)

Full scan of all 256 ISI resource IDs on device 0x00 (phone host).
Sent COMM_ISI_VERSION_GET_REQ (0x12) to each.

### Responding Resources (9)
| ID | Version/Response | Notes |
|----|-----------------|-------|
| 0x0E | VERSION FAFA | Unknown, not in oFono |
| 0x2C | VERSION 04.00 | Unknown, not in oFono |
| 0x5E | VERSION 02.55 | 22B with FF padding |
| 0x5F | VERSION 00.00 | Paired with 0x5E? |
| 0x76 | VERSION 02.55 | Same response as 0x5E |
| 0x77 | VERSION 00.00 | Same response as 0x5F |
| 0xE2 | msg=0x16 (PRODUCT_INFO_RESP) | |
| 0xE3 | msg=0x16 (PRODUCT_INFO_RESP) | |
| 0xF1 | msg=0xCB, data=27efed12 | Completely unknown |

### Crasher Resources (5)
0x13, 0xDF, 0xE1, 0xF2, 0xF4

### ERROR 0x01 Resources (34, exist but reject version query)
0x04, 0x0A, 0x0D, 0x11, 0x12, 0x17-0x20, 0x42, 0x43, 0x47, 0x4C,
0x4F, 0x5C, 0x61, 0x62, 0x8E, 0x9D, 0xA6, 0xB5, 0xBB, 0xBE,
0xD3, 0xD6, 0xE0(0xFF), 0xE9, 0xEA-0xEC, 0xF0, 0xF3

## Next Steps
- Probe each responding resource with different message types
- Resource 0x2C is most promising unknown (VERSION 04.00)
- Resource 0xF1 returned completely unknown msg type 0xCB
- ERROR 0x01 resources exist but may need different message format
- PM read service might be among the ERROR 0x01 group
