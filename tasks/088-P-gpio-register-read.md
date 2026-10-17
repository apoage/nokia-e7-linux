# Task 088: Read GPFSEL Registers — The Demigod Boss Fight

## Status (2026-08-21)
**Runtime paths CLOSED; static path ACTIVE.**

All Tier 1/2 runtime kernel-memory routes are definitively closed by the
LDD caps wall (see lore 2026-08-21): every driver on the device — file
and ROM resident — carries All+TCB; TCB is ROM-only; caps shortfall at
channel-open is a kernel panic (3 reboots this week prove it). This
includes RomPatcher LDDs (never loaded), fshell memoryAccess (DoCreate
crashes), MemSpy (uninstalled + All+TCB), and our own LDDs (-20).

**The remaining road to the pinmux:**
1. **Static ROM analysis** (running): docs/gpio-static-pinmux.md will
   hold the CONFIRMED-vs-INFERRED pin table extracted from the GPIO init
   cluster (ROM 0x14C800-0x14E200 file offsets) + 5 mapped functions.
   If Symbian's pinmux is compiled-in, no runtime read is ever needed.
2. **VideoCore fly-over** (untested): BCM2835 lineage puts GPIO under
   VideoCore control; VCHI from user space (camera.pyd/gles.pyd) has no
   Symbian capability gate. Tier-2A in the list below.
3. **UART boot log** (hardware): NLoader prints config at boot on J2060.
4. If runtime confirmation is ever needed for a specific address,
   XPEEK/peek.exe does crash-isolated reads (user VA only — GPIO KVA
   0xC8004000 is supervisor-only).

## The Situation (original brief, kept for context)

We need to read 7 registers (28 bytes) from physical address 0x20200000.
These GPFSEL registers contain the pin mux configuration for all 70 GPIO
pins on the BCM2727B1 SoC — set by encrypted GENIO_INIT at boot.

The irony: Symbian malware authors get kernel code execution in a weekend.
We're trying to read 28 bytes and the phone's security architecture is
treating us like we're trying to launch nukes. It's like entering a long
death dungeon to slay the main never-defeated demigod boss — except the
boss is Nokia's platform security and we're armed with Python 2.5.

## What We've Confirmed

- GPIO physical base: 0x20200000 (BCM2835-compatible)
- GPIO kernel VA: 0xC8004150 (from ROM constant pool)
- Heap is NX (no execute)
- ROM is read-only
- DLL code is read-only + executable
- peek/poke: user-space byte read/write only (plain LDRB)
- 218 SVCs cataloged, HAL::Get fully traced
- 5 ROM functions that map GPIO found and disassembled
- Mapped VA stored in kernel heap (inaccessible from user mode)
- Process capabilities: 5 of 20 (no TCB/AllFiles/ReadDeviceData)
- Custom exe execution broken (error 3)
- FShell readmem crashes (memoryAccess LDD needs TCB)

## Attack Surface Map

### Tier 1: Quick Wins (hours)

#### 1A. RomPatcher+ Custom Dump
RomPatcher+ "Dump SuperPage" reads kernel memory and writes to file.
The dump address might be configurable or patchable.
- Examine how RP+ dump works internally
- The patcherShadow.ldd has ALL+TCB — it CAN read any address
- If we can pass a custom address (0xC8004150) to the dump function...
- Write a custom .rmp patch that triggers a dump from GPIO VA
- **Risk:** low (read-only operation)

#### 1B. Python Object Corruption via poke
We can write to Python object metadata in heap memory.
- Corrupt a string object's internal buffer pointer to point to ROM
- When Python reads the "string", it reads from our target address
- PyStringObject: ob_refcnt(4), ob_type(4), ob_size(4), ob_shash(4),
  ob_sstate(4), then ob_sval[] (the actual string data)
- But ob_sval is inline (not a pointer) in CPython 2.5
- Alternative: corrupt a buffer/memoryview object if available
- Alternative: corrupt the ob_type pointer to a fake type with custom tp_str
- **Risk:** medium (crash on wrong corruption)

#### 1C. HAL Variant Group Call
Symbian HAL has group 1 = EHalGroupVariant which dispatches to the
BCM2727 ASSP driver. This driver knows about GPIO.
- Need to call UserSvr::HalFunction(1, func, a1, a2)
- misty.get_hal_attr only calls group 0
- Could we modify misty's get_hal_attr code path to use group 1?
- Writable Python object m_ml pointer → redirect to ROM function
  that calls HalFunction with different group
- **Risk:** medium (wrong group/func = kernel panic)

### Tier 2: Deeper Exploits (days)

#### 2A. VideoCore/VCHI Shared Memory
DVCDriver communicates with VideoCore III via Messi/CCP2 bus.
VCHI (VideoCore Host Interface) uses shared memory buffers.
- If VCHI buffers are user-mapped, we might read/write VC commands
- VideoCore has full hardware access (it's the ISP/GPU)
- Send a VC command to read GPIO and return the value
- 20+ VCHI services identified in ROM (audio, display, camera, EGL)
- camera.pyd and gles.pyd available in Python — might expose VCHI
- **Risk:** medium-high (wrong VC command = hang)

#### 2B. Display Framebuffer Pivot
dipro.pdd (display driver) maps framebuffer into user space for rendering.
- Find the framebuffer virtual address (user-accessible!)
- Check if GPIO mapping is in adjacent memory pages
- The kernel's memory allocator might place DPlatChunkHw mappings nearby
- If framebuffer is at 0xC800Dxxx and GPIO at 0xC8004xxx,
  a negative offset from framebuffer reaches GPIO
- **Risk:** low (just reading, but might not be adjacent)

#### 2C. Audio DMA Buffer
DAudioPipeChannel has shared memory for audio streaming.
Audio DMA buffers are often user-mapped for low-latency playback.
- audio.pyd is available in Python
- Open an audio stream, find the DMA buffer address
- Check if DMA buffer mapping extends to GPIO pages
- **Risk:** low

#### 2D. Buffer Overflow in _hack.pyd
_hack.peek/poke take integer arguments. What happens with:
- Very large addresses (> 0xFFFFFFFF)?
- Negative values?
- If the C code uses signed int, negative value → kernel VA!
- peek(-939524096) = peek(0xC8004000) if signed→unsigned conversion
- **Risk:** low-medium (might just crash)

### Tier 3: Research-Heavy (weeks)

#### 3A. Symbian Malware Analysis
Known Symbian malware achieved kernel code execution.
- Find and decompile known Symbian^3 exploits
- CommWarrior, Cabir, Skulls used different vectors
- More relevant: jailbreak exploits (CapsSwitch, PlatSec bypass)
- RomPatcher's own patcher.ldd IS a kernel exploit — it's manufacturer-signed
  but the technique (shadow page patching) is the same
- **Risk:** research only

#### 3B. Mailing List / Forum Mining
27K mailing list files, 134K forum files already extracted.
- Search for "DPlatChunkHw" user-space access techniques
- Search for "physical memory" read from user process
- Search for Symbian kernel vulnerabilities
- **Risk:** none (research)

#### 3C. OpenGL ES GPU Command Buffer
khegl_ldd.ldd is in ROM. GPU drivers map command buffers.
- gles.pyd available in Python
- GPU command buffers might be in a shared memory region
- The GPU (VideoCore III) has full hardware access
- Craft a GPU command that reads from GPIO physical address
- Return result via GPU → CPU shared memory
- **Risk:** high (wrong GPU command = system hang)

### Tier 4: Nuclear Options

#### 4A. Kernel Panic Dump
Trigger a controlled kernel panic that dumps register state.
- misty.reset_phone() available
- If kernel crash dump includes hardware register state...
- Nokia might log GPFSEL values in crash dump
- Check if crash dump exists after reboot
- **Risk:** phone reboot, but no permanent damage

#### 4B. Timing Side Channel
Read GPIO state by measuring instruction timing.
- GPIO output levels affect bus timing
- Execute tight loops and measure cycle counts
- Different GPFSEL configs = different bus contention patterns
- Extremely slow and unreliable but theoretically possible
- **Risk:** none (just slow)

#### 4C. Cold Boot / JTAG
- J2060 has JTAG pins — direct hardware debug access
- OpenOCD with JTAG adapter reads any address
- Requires soldering but gives god-mode access
- **Risk:** hardware modification required

## Methodology: Map First, Then Probe

**CRITICAL**: Kernel panics and crashes during probing are NOT random failures.
They're signs we touched LIVE kernel memory the WRONG way. Each crash is data.

**Correct approach:**
1. MAP the target address space by reading ROM code and tracing mappings
2. UNDERSTAND what's at each address before touching it
3. WRITE the exact address we intend to test
4. TEST it with a known-safe read pattern
5. RECORD the crash point if it fails — that's information about the mapping

Do NOT blindly scan address ranges. Each peek to an unmapped address kills
the Python process. Map first, then surgically probe known-valid addresses.

## Priority Order

1. **0. Network bridge** (Task 089) — unblocks rapid iteration
2. **2D. Negative value in peek** (5 minutes to test!)
3. 1A. RomPatcher+ custom dump
4. 1B. Python object corruption
5. 2B. Display framebuffer pivot
6. 1C. HAL variant group
7. 2A. VideoCore shared memory
8. 2C. Audio DMA buffer
9. 3A. Malware analysis
10. 3B. Mailing list mining
11. Everything else

## Notes

The fact that we've gotten this far — live ROM disassembly via Python,
218 kernel syscalls mapped, GPIO mapping code traced through 5 functions,
function pointer redirection confirmed — all from a 2011 phone running
Python 2.5 with 5 out of 20 capabilities — is already unprecedented.

Symbian malware authors had it easy: they targeted user data, SMS, calls.
We're trying to read hardware registers through the most locked-down
security architecture Nokia ever built. Even the jailbreak community
never needed to read MMIO registers — they just needed to patch
InstallServer to bypass certificate checks.

We're deeper than any documented Symbian exploit has gone.

28 bytes. The demigod awaits.

## RESOLUTION (2026-08-21, static analysis agent)

The task's premise was wrong — productively wrong. **BCM2727 has no GPFSEL
registers at all.** Full report: docs/gpio-static-pinmux.md (CONFIRMED vs
INFERRED, tool: tools/gpio_static.py, capstone). Verified independently:
the SetPinMode hardware stub at 0x800C0B24 is a literal `bx lr`
(0xE12FFF1E), and the old "init cluster" references were 0x20202020
ASCII space-word false positives (464 in that range).

What we actually learned (better than the 28 bytes):
- GPIO controller = custom Broadcom RAPU block: 7 banks x 32 pins,
  per-bank DATIN/SET/CLR trios, EDGE/OUT set-clr pairs, IRQ/direction/
  pull blocks (map in report §3). Real MMIO VA window 0xC603xxxx
  (phys 0x20200000). NOT BCM2835 layout.
- Symbian never programs pin FUNCTIONS: SetPinMode is stubbed out.
  Direction/output/irq/wake/edge ARE runtime-programmable via SET/CLR.
  Confirmed literal stores: USB pins 129-132, MMC 58-62 (validated vs
  schematic GPIO59=VMEM2_EN), VideoCore wake 7/8.
- Pin functions are set PRE-SYMBIAN (NLoader/GENIO_INIT/SWBL/TrustZone/
  VideoCore) or fixed in hardware.

For Linux: no pinmux table to port — write a pinctrl driver from the
decoded register map; treat function as fixed-by-bootloader. The pre-boot
chain (where muxing actually lives) is the same NLoader/VideoCore
territory as the boot path — the two quests have converged.
