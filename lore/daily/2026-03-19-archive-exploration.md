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
