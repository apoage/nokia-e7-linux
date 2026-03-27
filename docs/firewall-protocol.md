# Nokia ISI Firewall Protocol (Resource 0x43)

## Summary
The Nokia E7 has an ISI firewall that blocks all memory/hardware services.
The firewall has only 4 message pairs and NO authentication submit message.
Auth level is set by hardware (DK2 dongle detection), not software.

## Firewall Version
01.11 (from COMM_ISI_VERSION_GET response via COMMON_MESSAGE 0xF0)

## Message Map (Complete — all IDs 0x00-0x10 tested)

| Msg ID | Name | Response | Notes |
|--------|------|----------|-------|
| 0x01 | STATUS_READ | `XX 02 00 00 00 00` | Same as 0x02 |
| 0x02 | STATUS_READ | `XX 02 00 00 00 00` | Auth status |
| 0x03 | CONFIG_READ | `XX 04 00 00 00 00` | Same as 0x04 |
| 0x04 | CONFIG_READ | `XX 04 00 00 00 00` | Config |
| 0x05 | PERM_READ | 15B with mask | Same as 0x06 |
| 0x06 | PERM_READ | 15B with mask | Permissions + changing 2-byte value |
| 0x07 | AUTH_LEVEL | `XX 08 01` | Always returns level=1 |
| 0x08 | — | ERROR 0x01 | Not supported |
| 0x09-0x10 | — | ERROR 0x01 | Not supported |

## Permission Read Response Format (msg 0x05/0x06)
```
XX 06 01 00 FF FF FF FF FF FF FF FF XX XX 00
│  │  │  │  └─────────────────────┘ └──┘ └─ padding
│  │  │  │     permission mask (64 bits)  changing value
│  │  │  └─ padding?
│  │  └─ count? (always 1)
│  └─ response msg ID
└─ transaction ID (echoed)
```

The "changing value" (2 bytes) differs each call:
- Call 1: `e064`
- Call 2: `b862`
- Call 3: `1860`
- Call 4: `6018`
- Call 5: `601800` (3 bytes?)

Note: `1860` and `6018` are byte-swapped. Not crypto nonce — likely a counter.

## Auth Level
Always returns `0x01` via msg 0x07, regardless of:
- Payload content (empty, padded, key data)
- Nokia Cooker DES key (`416ce871c3bd0a28`)
- Nonce + key combination

The auth level appears to be **hardware-determined** — the phone checks
for a DK2 (DES Key 2) dongle on the parallel port at startup.

## Protected Services (34+ resources, ERROR 0x01)
All return ISI error when version-queried. Direct message access crashes phone.
See `phoenix-service-map.md` for the complete service list.

## Conclusions

1. **No software auth bypass via the firewall's ISI interface**
2. **No challenge-response** — the "nonce" is not a crypto challenge
3. **Auth level is hardware-detected** (DK2 dongle on LPT)
4. **Protected resources crash when accessed** without proper auth
5. The firewall is a **read-only status reporter** from the ISI side

## Alternative Approaches
Since the firewall can't be defeated through ISI messaging:
- Find a DK2 dongle (extremely rare Nokia service equipment)
- Emulate the DK2 dongle on USB/LPT (need the DES key)
- Find the firewall check in the phone's firmware and patch it (via RomPatcher)
- Access hardware registers through a completely different path (UART, JTAG)
- Use the working ISI services to extract useful data indirectly
