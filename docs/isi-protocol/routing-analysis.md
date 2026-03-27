# ISI Message Routing Analysis

## Source: modemadaptation Symbian source code (2026-03-22 analysis)

## Architecture

The Nokia E7 ISI routing system has three layers:

### 1. Transport Layer
- **PN_MEDIA_USB (0x23)** — USB Phonet (our connection, `ETrxUSB`)
- **PN_MEDIA_MODEM_HOST_IF (0x26)** — Shared memory to modem (`ETrxSharedMemory`)

### 2. Router (ISCE)
File: `isirouter_dll/src/isicltransceiver.cpp`

`DISICLTransceiver::RouteISIMessage()` (lines 310-436):
1. Validates message
2. Extracts receiver device ID from ISI header byte 1
3. If `PN_DEV_OWN` (0x00): routes locally via `iShRouter.Receive()`
4. If `PN_DEV_MODEM` (0x60): routes to shared memory link
5. If `PN_DEV_PC`: routes to USB link

Registered devices (lines 94-116):
```cpp
DISIDevice(PN_DEV_MODEM, CreateLinkF(this, PN_MEDIA_MODEM_HOST_IF, ETrxSharedMemory));
DISIDevice(PN_DEV_PC, CreateLinkF(this, PN_MEDIA_USB, ETrxUSB));
```

### 3. Name Service (PN_NAMESERVICE, 0xDB)
File: `isinameservice_dll/src/isinameservice.cpp`

The Name Service maintains a **dynamic resource→device mapping**:
- At modem boot, services register via `PNS_NAME_ADD_IND`
- When a message arrives for `PN_DEV_OWN` with `PN_OBJ_ROUTING_REQ`, the
  Name Service looks up the resource ID and **rewrites the ISI header**:

```cpp
// Line 194-200: Dynamic device translation
iNameRecords->LookupPhonetAddress(resource, &phoNetAddress);
msgPtr[ISI_HEADER_OFFSET_RECEIVERDEVICE] = phoNetAddress >> 8;
msgPtr[ISI_HEADER_OFFSET_RECEIVEROBJECT] = phoNetAddress & 0xFF;
```

## Message Flow: USB → Modem

```
PC (USB)
  │ ISI header: rdev=0x00, resource=0x93
  ▼
USB Server (Symbian)
  │ CUsbPnUsbReceiver → CUsbPnIsaSender
  ▼
ISCE Router
  │ rdev == PN_DEV_OWN → local routing
  │ Object == PN_OBJ_ROUTING_REQ (0x00) → Name Service intercepts
  ▼
Name Service
  │ Lookup: resource 0x93 → Phonet address 0x6000
  │ Rewrite: rdev=0x60, robj=0x00
  ▼
ISCE Router (re-route)
  │ rdev == PN_DEV_MODEM → shared memory link
  ▼
Modem (RAPUYAMA)
  │ PN_MODEM_PERM service processes message
  ▼
Response (reverse path)
```

## Key Insight

Our previous scans sent messages with `rdev=0x60` directly. This attempts to
use the shared memory link **without Name Service translation**. If the link
isn't connected or the message format is wrong, we get `ENTITY_NOT_REACHABLE`.

The correct approach may be:
1. Send to `rdev=0x00` (host) with the modem resource ID
2. Let the Name Service do the device translation
3. The router forwards via shared memory

## BUT: The Modem Must Be Registered

The Name Service only routes to the modem if:
1. The modem has completed boot
2. The modem sent `PNS_NAME_ADD_IND` to register its resources
3. The Name Service has an active entry for the resource

If the modem hasn't registered PN_MODEM_PERM, the Name Service won't know
where to route it, and the message will be silently dropped or error'd.

## USB Phonet Bridge Details

File: `usbphonetlink/usbpnserver_exe/src/cusbpnisareceiver.cpp`

- `iForwardFlag` gates ISA→USB forwarding (set by `SetAltSetting(1)`)
- USB receiver opens ISC channel via `EIscNokiaUsbPhonetLink`
- No message filtering — all ISI messages forwarded as-is
- Alt setting 1 must be active for bidirectional routing

## Connection State Gate

From `isicltransceiver.cpp` lines 385-419:
- `link->StateConnected()` must be true for message delivery
- `iDevPcLastSendTrxId` tracks USB connection state
- Connection expires if USB link drops

## Test Plan

1. **Name Service version query** — verify 0xDB responds
2. **PNS_NAME_QUERY_REQ** for modem resources — check if registered
3. **Version query via device 0x00** for modem resources — test Name Service routing
4. **Compare host vs modem resource routing** — host should work, modem may not

Tool: `e7/tools/isi_nameservice_probe.py`
