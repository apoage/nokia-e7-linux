# 2026-03-21: ISI Protocol Breakthrough

## Summary
Established working ISI/Phonet communication with the Nokia E7 over USB.
Read IMEI, software version, Bluetooth info, and hardware data directly
from the phone's BB5 platform — bypassing Symbian OS entirely.

## Setup Discovery

### The Address Problem (solved)
`ip address add 0x10 dev usbpn0` adds an **IPv4** address, not Phonet!
Must use RTM_NEWADDR netlink message with `family=AF_PHONET(35)` and `IFA_LOCAL=0x10`.
Without this, Phonet socket bind fails with EADDRNOTAVAIL.

### Working Setup Sequence
```bash
# After phone connects in Nokia Suite mode (PID 0x0335):
echo "3-2:1.14" | sudo tee /sys/bus/usb/drivers/cdc_phonet/bind
sudo ip link set usbpn0 up
sudo ip address flush dev usbpn0  # Remove stale IP addresses
# Then in Python: RTM_NEWADDR with AF_PHONET + SO_BINDTODEVICE=usbpn0
```

### Key Addressing
- Our address: 0x10 (PN_DEV_PC)
- Phone services: device 0x00
- Phone link address: 0x1B (assigned to us by phone)
- SO_BINDTODEVICE required to bypass empty routing table

## Complete ISI Resource Scan (256 resources)

5 phone reboots survived. Auto-reconnect after each crash.

### Responding Resources (9)
| ID | Response | Notes |
|----|----------|-------|
| 0x0E | VERSION FAFA | Unknown Nokia service |
| 0x2C | VERSION 04.00 | Unknown Nokia service |
| 0x5E | VERSION 02.55 | 22B response, FF padding |
| 0x5F | VERSION 00.00 | Paired with 0x5E |
| 0x76 | VERSION 02.55 | Same as 0x5E |
| 0x77 | VERSION 00.00 | Same as 0x5F |
| 0xE2 | msg=0x16 | PRODUCT_INFO_RESP variant |
| 0xE3 | msg=0x16 | PRODUCT_INFO_RESP variant |
| 0xF1 | msg=0xCB | Completely unknown msg type |

### Crasher Resources (5)
0x13, 0xDF, 0xE1, 0xF2, 0xF4 — cause phone reboot

### ERROR 0x01 Resources (34)
Exist but reject version query. May need firewall authorization.

## PHONE_INFO Deep Probe (16+ message types)

| Msg ID | Response | Decoded Content |
|--------|----------|-----------------|
| 0x00 | SERIAL_READ_RESP | IMEI: 354864048650007 |
| 0x02 | PP_READ_RESP | Product Profile (error 0x01) |
| 0x04 | msg=0x05 | Unknown (3B) |
| 0x07 | VERSION_READ_RESP | 220B! See below |
| 0x09 | msg=0x0A | Unknown (3B) |
| 0x0B | msg=0x0C | Unknown (4B) |
| 0x0D | msg=0x0E | Unknown (3B) |
| 0x0F | msg=0x10 | Unknown (3B) |
| 0x11 | msg=0x12 | VERSION_GET_RESP (4B) |
| 0x13 | msg=0x14 | Unknown (4B) |
| 0x15 | PRODUCT_INFO_RESP | Error 0x02 |
| 0x17 | msg=0x18 | Unknown (4B) |
| 0x23 | msg=0x24 | HW ID: 0x414015CF (12B) |
| 0x29 | msg=0x2A | Unknown (4B) |
| 0x2B | msg=0x2C | Unknown (4B) |
| 0x2E | msg=0x2F | Statistics/caps (140B!) |
| 0x30 | msg=0x31 | Unknown (4B, value=0x01) |
| 0x32 | msg=0x33 | Memory? (8B, value=0x4094) |
| 0x34 | msg=0x35 | Unknown (4B, value=0x16) |

### VERSION_READ_RESP Decoded (220 bytes)
- GHCI Version 12 (rev. 64) — Bluetooth HCI version
- LMP Version 36 (rev. 64) — BT Link Manager Protocol
- Manufacturer 2880 — BT chip manufacturer ID
- Firmware: 111.040.1511
- Full build: 111.040.1511.15.01 / 111.040.1511.C00.01
- Component version: 0.037

### Firewall (0x43) Responses
- msg 0x02: `210200000000` — safe, returns config
- msg 0x04: `230400000000` — safe, returns config
- msg 0x06: `25060100ffffffffffffffffe06400` — **15B with permission mask?**
  WARNING: This or a following message may have triggered phone crash

### SIM (0x09) Responses
All msg IDs 0x00-0x2F respond with 4-byte echo pattern.
Even IDs return 0xFA (error?), odd IDs return 0xFB.

## Hardware Data Extracted

### From PHONE_INFO
- IMEI: 354864048650007 (Unit A)
- Model: RM-626
- Software: V 79_sr1_12w18.5, 27-07-12
- Bluetooth: HCI v12 rev64, LMP v36 rev64, Manufacturer 2880
- Hardware ID: 0x414015CF (msg 0x23)
- Memory/address: 0x4094 (msg 0x32)
- Statistics: 140B block with counts 3, 15, 155 (msg 0x2E)

## Tools Created
- `tools/nokia_isi.py` — ISI protocol client library
- `tools/isi_scan.py` — Crash-resilient 256-resource scanner
- `tools/isi_deep_probe.py` — Service-specific message probing
- `tools/isi_info_probe.py` — PHONE_INFO message type scanner

## Open Questions
1. Is PM read behind the firewall? The 34 ERROR 0x01 resources might respond after auth
2. What is msg 0x23 value 0x414015CF? ARM CPUID variant?
3. What are the 140 bytes from msg 0x2E? Memory map? Hardware capabilities?
4. Can we safely explore firewall msg 0x06+ to open access?
5. Does device 0x60 (modem) have separate services with memory access?
6. The time reset during crash — did we hit a clock configuration register?
