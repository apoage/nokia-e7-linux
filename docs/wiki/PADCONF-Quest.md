# The PADCONF Quest

The ongoing battle to read 584 bytes of OMAP3630 pad mux registers from a
Nokia E7-00 running Symbian Belle FP1.

## Why We Need PADCONF

The OMAP3630's System Control Module at physical address `0x48002030` contains
292 16-bit pad configuration registers that define how every pin on the chip
is configured: GPIO mode, pull-up/down, input enable, mux function.

Without these values, we can't create a correct device tree for Linux boot on
real hardware. The NLoader bootloader (encrypted) configures these registers
at boot, and the values are Nokia-proprietary.

## Methods Tried (All Failed)

### 1. Custom Kernel LDD
**Result: KErrAccessDenied (-20)**

Built `memread.ldd` using DPlatChunkHw::New() to map physical memory.
Kernel rejects loading because the binary isn't manufacturer-signed.
`User::LoadLogicalDevice` enforces signature verification at kernel level.

### 2. MemSpy Driver
**Result: User-space only, no physical memory access**

MemSpy driver channel opens successfully. Mapped all 250 opcodes.
Reverse-engineered the binary: 191 imports, confirmed `DPlatChunkHw::New`
is NOT imported. Only `Kern::ThreadRawRead` (user-space). Dead end.

### 3. FShell memoryAccess LDD
**Result: TCB capability required, kernel panic**

FShell v5.00 installed. `memoryAccess-fshell.ldd` is loaded in kernel
(confirmed via `driver list logical`). But DoCreate panics because our
process lacks TCB capability. TCB is ROM-only on Symbian — no SIS-installed
binary can ever have it. Even rebuilding with capability=All and fshell's
SID doesn't work — the kernel verifies the signature chain, not just header flags.

### 4. FShell readmem Command
**Result: Crashes**

FShell's own `readmem` command crashes on every address including ROM (0x80000000).
Likely the memoryAccess LDD was built for a different kernel version, or the
BB5 platform remaps OMAP addresses.

### 5. Leftup Certificate Signing
**Result: Dead end (confirmed by community)**

The Leftup/OPDA/BiNPDA certificates in the hacked SWI certstore grant all
capabilities for SIS installation EXCEPT TCB. TCB is enforced at the kernel
binary loader level, not the certstore. "You can't have TCB capability on S60,
it is only possible with ROM executables."

### 6. TRK Debug Agent
**Result: Kernel driver stripped from production firmware**

TRK GUI apps exist in ROM (Z:\sys\bin) but `trkdriver.ldd` is missing —
Nokia stripped it from production builds as a security measure.

### 7. RomDumpPlus
**Result: No custom address input**

Successfully dumped the SuperPage (592 bytes), but RomDumpPlus only has
preset dump targets. No way to specify a custom address.

### 8. Sorcerer App
**Result: Game cheat tool, not system debugger**

Despite being described as a "memory debugger", Sorcerer v1.75 is a game
memory modification tool. User-space only.

## Methods Still Open

### A. Phonet/FBUS Protocol (P1)
Reverse-engineer Nokia's USB service protocol. Phoenix Phone Browser accesses
phone memory from the PC, bypassing all Symbian security. The protocol operates
at BB5 platform level, below the OS. Nokia Service Tools (15.3GB) downloading.

### B. CapsSwitch / PlatSec Tool (P1)
Runtime platform security disable via RomPatcher's kernel driver. Developed by
'symbuzzer'. Would allow loading any LDD. Not yet found.

### C. RomPatcher LDD Repurposing (P2)
RomPatcher's own `patcher*.ldd` is loaded and has kernel write access (that's
how it patches ROM). If we reverse-engineer its DoControl API, we could use it
to read PADCONF or patch the platsec check.

### D. UART Capture (P2)
NLoader prints PADCONF values during boot. Connect UART adapter to E7's debug
serial port and capture the output. Most reliable method but requires hardware
modification and finding the UART pins.

### E. Contact Community Experts (P1)
- **Max Bondarchenko** (iCrazyDoctor) — RomPatcher internals expert
- **symbuzzer** — CapsSwitch/PlatSec tool developer
- Both active on t.me/symbian_world

## Key Insight: BB5 Address Remapping

From nokiahacking.pl reverse engineering thread: "OMAP addressing is modified
by Nokia's BB5 platform — addresses differ from standard TI documentation."

This means the standard OMAP3630 PADCONF address (0x48002030) might not be
correct on the E7. This could explain why FShell's readmem crashes on every
address — it's trying to access remapped memory.

## The Scoreboard

| Bytes read | Bytes needed | Crashes caused |
|------------|-------------|----------------|
| 0 | 584 | 12+ |
