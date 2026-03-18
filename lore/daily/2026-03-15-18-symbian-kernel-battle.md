# 2026-03-15 to 2026-03-18: The Symbian Kernel Battle

Multi-day session spanning the breakthrough of exe execution through to the
systematic exploration (and exhaustion) of every kernel memory access path
available on Symbian Belle FP1.

## Major Breakthrough: CRT Startup Fix (Mar 15)

**Root cause of ALL exe failures found**: missing CRT startup code.

- Symbian EXEs must link with `eexe.lib` (provides `_E32Startup`)
- Without it: exe enters E32Main with no heap → instant silent crash
- Boot chain: `_E32Startup` → `RunThread` → `UserHeap::SetupThreadHeap` → `CallThrdProcEntry` → `E32Main`
- Also need `usrt2_2.lib` (C++ runtime) + `crt_stubs.cpp` (exception handling stubs)
- `crt_stubs.cpp` provides: `__cxa_begin_catch`, `__cxa_end_catch`, `__cxa_end_cleanup`,
  `TCppRTExceptionsGlobals`, `XLeaveException` typeinfo, sized `operator delete`
- Link flags: `-Wl,--whole-archive eexe.lib usrt2_2.lib -Wl,--no-whole-archive`
- Entry: `-Wl,--entry,_E32Startup`

**test.exe confirmed working**: writes `E:\test_ok.txt` containing "it works".
`e32.start_exe('test.exe', '', 1)` returns 0.

## MemSpy Driver Investigation (Mar 15)

### Channel Opens Successfully
- `User::LoadLogicalDevice("MEMSPYDRIVER")` returns 0 (success)
- `DoCreate` succeeds — channel open
- Can send DoControl opcodes

### Opcode Mapping (v6-v11, with per-attempt crash recovery)
Complete scan of opcodes 100-350:
- **101**: ReadMemory — user-space only via `Kern::ThreadRawRead`
- **124**: Returns kernel object handles (ASLR, changes each boot)
- **127, 225, 305, 306, 341, 342**: Return success with various params
- **Crashers**: 183, 184, 203, 222, 223, 224 — kernel panic with NULL params
- ALL 200 threads tested with op 101 for 0x48002030 — all return -13

### Reverse Engineering (Mar 15)
- Decompressed MemSpyDriver.ldd: 63,284 bytes (Symbian custom deflate)
- 191 imports from ekern.exe analyzed
- **DPlatChunkHw::New (ordinal 60) NOT IMPORTED** — confirmed dead end
- Only uses: Kern::ThreadRawRead (290), Kern::ThreadRawWrite (298), Kern::SuperPage (360)

### Conclusion
MemSpy cannot read physical/MMIO memory. Dead end for PADCONF.

## Kernel Thread Enumeration (Mar 15)

200 threads enumerated including 76 kernel threads. Notable:
- `Rapu_GDMASS` — RAPUYAMA modem DMA
- `VCDriverDfcQ` (5 instances) — BCM2727 VideoCore
- `Spi1-3 SpiDriver Thread` — 3 SPI buses
- `NfeThread0-2` — NAND flash encryption
- `SecLddMainDfcq` — security LDD
- `I2CThread_0`, `AtmelThread`, `TouchDriverThread`, `MagnetometerDfcQueue`
- `accelerometer`, `Dipro_drv`, `bt_driver`, etc.

## FShell Discovery (Mar 16)

### Installation
FShell v5.00 installed from SIS library (920KB, 78 E32 binaries).
`memoryAccess-fshell` LDD found **already loaded in kernel**.

### Driver Lists
25 LDDs + 9 PDDs listed via `fshell -e "driver list logical"`.
Key driver: `memoryAccess-fshell` — uses DPlatChunkHw::New for physical memory reads.

### FShell Commands
Working: help, meminfo, driver list, drvinfo, e32header, dump, ls, env, date
Crashing: readmem, ps, chunkinfo — all use memoryAccess LDD channel

### System Info
- Total RAM: 257,228,800 (245MB usable)
- Total ROM: 11,206,656 (10.7MB)
- C: 496MB NAND, D: 148MB RAM, E: 16GB SD

### The TCB Problem

E32 header analysis revealed:
- fshell.exe: caps=0x000fffff (All+TCB), SID=0x10282d94
- memoryaccess-fshell.ldd: caps=0x000fffff (All+TCB), SID=0x10273948
- Our exe: caps=0x000ffffe (All-TCB) — **missing TCB bit!**

DoCreate requires caller caps >= LDD caps. Missing TCB → kernel panic (not error return).

Rebuilt with caps=0x000fffff + fshell SID → still crashes.
**TCB is ROM-only** — confirmed by Symbian World community analysis.

## Symbian World Chat Analysis (Mar 18)

Searched 127K messages across 128 HTML files.

### Critical Findings
- "you can't have sis with tcb capability on s60, it is only possible with rom executables"
- RomPatcher LDDs work because they're **manufacturer-signed** (from AV quarantine)
- Leftup cert grants All caps for SIS but NOT TCB — kernel enforces this
- **CapsSwitch/PlatSec tool** by symbuzzer: disables platsec at runtime via RomPatcher LDD
- **Max Bondarchenko** (iCrazyDoctor): key expert on kernel internals

### Key People
- Max Bondarchenko / iCrazyDoctor: RomPatcher internals, CFW developer
- symbuzzer: PlatSec/CapsSwitch, kernel patching, advanced RP+ mods
- Nuru D.: FShell knowledge, cert updates

### Other Chat Searches
- Linux Mobile World (32K msgs): zero hits for our topics, consumer group
- S80 chat: retro Communicator content, irrelevant
- E7 eMMC failure confirmed systemic by multiple sources

## nokiahacking.pl Exploration (Mar 18)

### Reverse Engineering Thread
Trzyzet (Dec 2018) attempted Linux boot on N95-2:
- Used Sorcerer app for memory viewing (later found to be a game cheat tool, not system debugger)
- Found BB5 modified OMAP addressing — **addresses differ from standard TI docs**
- Found BB5 protected memory, modified values disabling BB5
- Got console window running, stalled on u-boot

### Phoenix Phone Browser
- Nokia internal tool, mounts phone filesystem as Windows drives over USB
- Requires Phoenix 2008.4.7 specifically (broken on newer versions)
- Accesses filesystem, NOT raw registers — useful for deploying files
- Full recipe documented (10 required components)

### TRK Debug Agent
- TRK GUI apps ARE in E7 firmware ROM (Z:\sys\bin)
- But trkdriver.ldd is **stripped from production firmware**
- Without kernel driver, TRK apps are useless

### BB5 SIM-Lock Thread
- PM field 308 encrypted with AES-128 key in RAP3G
- FBUS commands can read raw data from RAP3G chip
- "Nokia's TRK application can do EVERYTHING with the phone"
- Detailed protocol analysis of unlock sequence

## Nokia Service Hints (Mar 18)

Found and analyzed `Nokia_E7-00_Service_Hints_v1.0.xls`:
- Test points with voltage measurements
- J3117 = PwrOnx pad (accessible from Top End Cap)
- IVE (BCM2727B): J1425=RUN, B1400=XIN(19.2MHz)
- Camera, audio, GPS, display diagnostic procedures
- USB flashing is default (no FBUS normally)

## Current State of PADCONF Access Attempts

### Dead Ends (Confirmed)
1. Custom LDD → LoadLogicalDevice returns -20 (kernel signature enforcement)
2. MemSpy driver → no DPlatChunkHw import, user-space reads only
3. memoryAccess-fshell via our exe → TCB is ROM-only, DoCreate panics
4. FShell readmem → crashes (LDD channel issue or kernel version mismatch)
5. Leftup cert signing → doesn't grant TCB
6. TRK → kernel driver stripped from production firmware
7. Sorcerer → game cheat tool, user-space only

### Open Paths
1. **Phonet/FBUS protocol RE** (Task 081) — build Linux tool for direct USB register read
2. **CapsSwitch/PlatSec tool** (Task 079) — runtime platsec disable
3. **Phoenix Phone Browser** (Task 080) — PC-side filesystem access
4. **RomPatcher LDD repurposing** — already loaded, has kernel write access
5. **UART capture** — NLoader prints PADCONF at boot
6. **Contact Max Bondarchenko** — knows kernel internals
7. **Nokia Service Tools** — downloading 15.3GB from Archive.org

## Deployment Workflow (Established)

```
1. Build in Docker (arm-none-symbianelf-gcc 12.1.0 + SDK)
2. Copy to E:\memread_tcb.exe via USB
3. Disconnect USB
4. Run E:\Python\run.py from PyS60:
   - Deletes old C:\sys\bin copy
   - Copies new exe from E:\ to C:\sys\bin and E:\sys\bin
   - Launches exe
   - Copies C:\Data logs to E:\ for USB access
5. Reconnect USB, read E:\memread_log.txt
```

Key gotcha: C:\sys\bin takes priority over E:\sys\bin.
Delete-before-copy needed (Symbian may lock loaded binaries).
Version string in exe helps confirm which version actually ran.

## Files Created/Modified
- `memory/symbian-world-chat.md` — 127K message analysis
- `memory/linux-mobile-chat.md` — 32K message analysis
- `memory/nokiahacking-re-thread.md` — RE forum findings + Phoenix + TRK
- `memory/kernel-threads.md` — updated with driver lists + system info
- `memory/symbian-sdk.md` — updated with FShell, TCB findings, LDD/PDD lists
- `tasks/072-P-symbian-padconf-dump.md` — rewritten with all approaches
- `tasks/074-O-superpage-via-memspy.md` — new
- `tasks/075-O-hal-sysinfo-dump.md` — new
- `tasks/076-O-sis-library-audit.md` — new
- `tasks/077-O-explore-phone-live.md` — new
- `tasks/078-O-leftup-cert-signing.md` — CLOSED (TCB is ROM-only)
- `tasks/079-O-capsswitch-platsec-disable.md` — new
- `tasks/080-O-phoenix-phone-browser.md` — new
- `tasks/081-O-phonet-protocol-re.md` — new
- `docs/service-manual/Nokia_E7-00_Service_Hints_v1.0.xls` — new
- `e7/tools/crawl_nokiahacking.py` — forum crawler
- Multiple `memread_simple.cpp` versions (v3-v16)
- Multiple PyS60 scripts in `memread/pys60/`
