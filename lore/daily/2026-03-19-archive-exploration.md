# 2026-03-19: Archive Exploration & FShell Source Discovery

## Downloads Completed
- Nokia Service Tools archive from Archive.org (28GB, partially corrupted — last 10GB recovering)
- Nokia SDKs & Dev Tools (separate collection)
- Both at `/home/lukas/ps3/e7/archive/`

## FShell memoryAccess Source Code — EXTRACTED AND ANALYZED

### Discovery
Found inside `source.zip` (6.7GB Symbian OS source) as a Mercurial bundle:
`source/oss+FCL+sf+os+fshell.hg` (2.3MB, 115 changesets, 1463 files)

Latest commit: "Implemented RObjectIx-based memoryaccess APIs"

Extracted to: `e7/symbian-sdk/fshell-memoryaccess-src/`

### Key Files
- `memoryaccess.h` — full API header with 57 DoControl opcodes
- `MemoryAccess.cpp` — kernel driver implementation
- `fdebuggerkernel.cpp` — kernel-side debugger (breakpoints, registers)
- `MemoryAccess.mmp` — build configuration
- EPL-licensed (Eclipse Public License v1.0)

### Complete Opcode Map (from source)
```
0:  EControlGetThreadMem        — Kern::ThreadRawRead (user-space only!)
1:  EControlGetAllocatorAddress
2:  EControlResetTimer
3:  EControlGetContainerCount
4:  EControlAcquireContainerMutex
5:  EControlReleaseContainerMutex
6:  EControlGetObjectType
7:  EControlGetObjectInfo
8:  EControlGetObjectInfoByPtr
9:  EControlGetObjectInfoByHandle
10: EControlAcquireCodeSegMutex
11: EControlReleaseCodeSegMutex
12: EControlGetNextCodeSegInfo
13: EControlObjectDie
14: EControlGetObjectInfoByName
15: EControlGetObjectAddresses
16: EControlGetChunkAddresses
17: EControlGetCurrentAllocatorAddress
18: EControlFindPtrInCodeSegments
19: EControlGetHandleOwners
20: EControlForceCrash           — Kern::Fault() !
21: EControlGetPropertyInt
22: EControlGetPropertyDesc
23: EControlSetPropertyInt
24: EControlSetPropertyDesc
25: EControlDeleteProperty
26: EControlSetThreadPriority
27: EControlNotifyThreadCreation
28: EControlCancelNotifyThreadCreation
29: EControlSetPriorityOverride
30: EControlSetCriticalFlags
31: EControlOpenThread
32: EControlGetThreadHandles
33: EControlGetProcessHandles
34: EControlInPlaceThreadRename
35: (removed)
36: EControlInPlaceObjectRename
37: EControlInPlaceSetProcessFileName
38-41: (removed)
42: EControlEnableHeapTracing
43: EControlDefragRam
44: EControlEmptyRamZone
45: EControlGetRamZoneInfo
46: EControlSetProcessProperties
47: EControlWriteShadowMemory    — Epoc::AllocShadowPage (ROM patching)
48: EControlFreeShadowMemory
49: EControlSetZombieDebugMode
50: EControlWriteMem             — Kern::ThreadRawWrite (user-space only)
51: EControlSetTextTraceMode
52: EControlGetRegisters         — CPU register dump
53-56: Zombie/debug controls
57+: Breakpoint controls
```

### GetThreadMem Implementation (THE critical function)
```cpp
TInt DMemoryAccess::GetThreadMem(TAny* aParams, TAny* aBuf)
{
    // Read params from user-space
    TThreadMemoryAccessParams params;
    Kern::ThreadDesRead(iClient, aParams, paramsBuf, 0);

    // Open thread by ID
    DThread* thread = Kern::ThreadFromId(params.iId);

    // Read memory in chunks using Kern::ThreadRawRead
    while (bytesCopied < params.iSize)
    {
        err = Kern::ThreadRawRead(thread, params.iAddr + bytesCopied, localPtr, toCopy);
        // Copy to user-space
        Kern::ThreadDesWrite(iClient, aBuf, *localBuf, bytesCopied, 0, NULL);
    }
}
```

Uses `Kern::ThreadRawRead` which reads from a thread's virtual address space.
MMIO at 0x48002030 is in kernel-only address space — not accessible through any thread.

### WriteShadowMemory Implementation
```cpp
TInt DMemoryAccess::WriteShadowMemory(TLinAddr aAddress, TAny* aNewContents)
{
    // Shadow the ROM page
    Epoc::AllocShadowPage(pageForStart);
    // Copy new bytes to shadow
    Epoc::CopyToShadowMemory(aAddress, (TLinAddr)newMem.Ptr(), newMem.Size());
}
```
This is how RomPatcher works — creates RAM shadow copies of ROM pages.
Only works for ROM addresses, not MMIO.

### CONFIRMED: No DPlatChunkHw::New() Anywhere
Searched entire source: zero references to DPlatChunkHw.
The memoryAccess LDD was NEVER designed for physical/MMIO memory access.
It's purely a user-space process memory debugger + ROM patcher.

## Nokia Service Tools Archive Inventory

### Phoenix Service Software (62 versions)
- `phoenix_service_sw_2008_4_7_32837.exe` (124MB) — **Phone Browser version**
- `phoenix_internal_rd_a04_11_000.zip` (58MB) — **R&D internal**
- 3 more internal versions (2007-2008)
- Versions span 2003 (41MB) through 2012 (151MB)
- Several patches for 2006-2007 versions

### WinTesla (10 versions + 34 modules)
- Versions 4.3 through 6.50
- Notable modules:
  - Nokia Service Driver pack — core FBUS/Phonet drivers
  - DES Key drivers — encrypted FBUS communication
  - FLS-4 drivers — flash cable protocol
  - HD98X functionality — flash operations
  - DTX-3 DLL — debug/test equipment
  - GPIB drivers — IEEE 488 instrument interface

### Other Service Tools
- Diego (8 versions, 2.14-3.09) — flash/update tool
- Care Suite (45+ files) — diagnostics with documentation
- WinLock (3 versions) — device unlock
- Data Package Manager (9 versions)
- Software Updater for Retail (7 versions)
- Service Application Manager

### Nokia SDKs & Dev Tools (91 files)
- `source.zip` (6.7GB) — **Full Symbian OS source**
- `Symbian^3 (Compiled).zip` (27.3MB) — compiled SDK tools
- `documentation.zip` (249MB) — massive doc archive
- Carbide.c++ v2.7 (246MB) and v3.2 (227MB) — Nokia IDE
- Qt SDK 1.2.1 (1.8GB) — full offline Qt
- 3 Nokia CD ISOs (1.57GB total)
- S60v3/v5 SDKs, Belle plugins, NFC SDKs
- Raptor/SBS build system

## Implications

### For Phonet Protocol RE (Task 081)
Now have all the tools to analyze:
1. Install Phoenix 2008.4.7 in Windows VM
2. Capture USB traffic (Wireshark + USBPcap)
3. Extract and reverse protocol DLLs from Phoenix installer
4. Cross-reference with Nokia Service Driver pack
5. Care Suite docs (`ASB_NCS_004.doc`) may describe protocol
6. Internal RD Phoenix may have additional debug commands

### For PADCONF Quest
- FShell memoryaccess path is DEFINITIVELY CLOSED (source code proves it)
- Phoenix Phone Browser accesses filesystem only (not MMIO)
- Best remaining paths:
  1. Phonet protocol raw memory read command (if it exists)
  2. CapsSwitch/PlatSec runtime disable (Task 079)
  3. UART capture from real hardware
  4. Modify memoryaccess source to add DPlatChunkHw (blocked by TCB signing)

## Phonet/ISI USB Communication (Late Night 2026-03-19/20)

### What Works
- Phone detected at PID 0x0335 (Nokia Suite CDC mode) with 18 USB interfaces
- **Interface 14/15**: UsbPnComm/UsbPnData = Phonet interface
- `cdc_phonet` kernel module loaded and bound to interface 14
- `usbpn0` network interface created (link/phonet 0x1b peer 0x00)
- Phonet socket (AF_PHONET=35) created and bound via ctypes
- **Phone RESPONDED**: dev=0x10 returned 0xF0/0x14 (COMM_ISA_ENTITY_NOT_REACHABLE_RESP)

### What Doesn't Work Yet
- Packets to dev 0x00/0x60/0x6C: no response (routing issue)
- Phonet route table empty — rtnetlink route add for AF_PHONET not working
- Raw packet socket (AF_PACKET): no response at all
- Object ID scan on dev 0x10: stopped responding (connection state?)
- Phone's PHONE_INFO service not reachable at any device address

### Key Insight
The phone's Phonet router IS processing our packets (confirmed by 0xF0/0x14 response).
But it returns "entity not reachable" — meaning it can't find the target ISI service.
Either:
1. The phone's services are at a device address we haven't tried
2. The phone needs a handshake/activation before exposing services to the PC
3. Nokia Suite does something special during connection that we're not doing

### Tools Created
- `e7/tools/nokia_isi.py` — ISI protocol tool (Phonet socket based)
- `/tmp/phonet_test.py`, `phonet_scan.py`, `phonet_route.py`, `phonet_direct.py`, `phonet_fix.py` — iterative test scripts
- `/tmp/phonet_route_setup.py` — rtnetlink route setup (didn't work for Phonet)

### Next Steps
1. Capture what Nokia Suite/PC Suite ACTUALLY sends during connection (Wireshark USB capture in Windows VM)
2. Check if the NCCD (Nokia Communication Controller) Symbian app needs to be explicitly started
3. Try using the OBEX/PC Suite Services interface (intf 8/9) instead of Phonet
4. Check if phonet_route_add in the kernel needs different parameters
5. Try the oFono test programs directly (gisi library)

### Protocol State of Knowledge
- Phonet header: 7 bytes (rdev, sdev, res, len_BE, robj, sobj)
- ISI message: utid, msg_id, data
- 21 ISI resources mapped (from oFono/libisi)
- Phone's link address: 0x1b (assigned to us), peer: 0x00
- Our Phonet address: 0x10 (set via ip addr add)
- CDC Phonet uses cdc_phonet kernel module
- Phone responded from dev=0x10 res=0x1B with ENTITY_NOT_REACHABLE

## ISI COMMUNICATION BREAKTHROUGH (2026-03-21 ~09:00)

### WORKING! Phone responds to ISI messages over Phonet/USB!

**Setup sequence (must be done after each reboot):**
1. Phone in Nokia Suite USB mode (PID 0x0335)
2. `sudo sh -c 'echo 3-2:1.14 > /sys/bus/usb/drivers/cdc_phonet/bind'`
3. `sudo ip link set usbpn0 up`
4. `sudo ip address flush dev usbpn0` (remove any stale IP addresses)
5. Add Phonet address 0x10 via RTM_NEWADDR with AF_PHONET family
6. Bind socket with SO_BINDTODEVICE=usbpn0
7. Send to device 0x00 (phone's service host)

**Key discovery: phone's address setup was the problem.**
`ip address add 0x10 dev usbpn0` adds an IP address, NOT a Phonet address!
Must use RTM_NEWADDR netlink message with family=AF_PHONET(35) and IFA_LOCAL=0x10.

### Confirmed Working
- **IMEI read**: 354864048650007 (Unit A confirmed!)
- **Software version**: V 79_sr1_12w18.5, 27-07-12, RM-626, (c) Nokia
- **Resource 0x0E**: responds to ISI version query (unknown service, version 0xFAFA)
- **Device 0x00**: phone's ISI service host address

### Crashed During Resource Scan
- Scan got to resource 0x0E before phone rebooted
- Resource 0x0E responded but scan continued to higher IDs
- Some resource ID between 0x0F and 0xFF crashed the phone
- Need per-resource logging (same technique as MemSpy opcode scan)

### Protocol Stack (confirmed working)
```
Linux PC (us)               Nokia E7
  dev=0x10                    dev=0x00
     |                           |
  AF_PHONET socket              ISI services
  SO_BINDTODEVICE=usbpn0        PN_PHONE_INFO (0x1B) ✓
     |                           PN_??? (0x0E) ✓
  cdc_phonet driver              ...more to discover
     |                           |
  USB CDC Phonet              USB CDC Phonet
  (interface 14/15)           (UsbPnComm/UsbPnData)
```
