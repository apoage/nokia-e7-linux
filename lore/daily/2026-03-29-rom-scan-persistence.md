# 2026-03-29: ROM Scan Persistence

## The Approach

Scanning 31MB of ROM from the running phone via `_hack.peek()`, 4 bytes at
a time, looking for the GPIO physical address 0x20200000. The ROM in memory
is the decompressed runtime image — different from the RomPatcher+ flash dump.

## The Script: rom_scan.py

A crash-resilient scanner that:
1. Saves position **BEFORE** each 4KB block read
2. On crash: position file = crash address
3. On resume: logs crash to `E:\rom_crash.txt`, skips 4KB ahead
4. Hits logged to `E:\rom_hits.txt`
5. Shows progress every 256KB

Run it. If it crashes, run again. And again. And again. And again. And again.
It'll chew through the whole 31MB. True resiliency.

## SVC Hunt Summary (March 26-29)

### What We Traced
- `_hack.peek` uses plain `LDRB r3,[r3]` — user-mode byte load, MMU-enforced
- `_hack.poke` — same, user-mode byte store
- HAL::Get at 0x80AAA632 (Thumb, ROM) — dispatch table in kernel RAM, unreachable
- 218 SVC instructions cataloged across 1MB of euser.dll ROM
- _misty.pyd: 43 functions confirmed, no hidden ones
- Import stubs use `LDR pc, [pc, #-4]` + IAT pattern
- DLL loads at different address each Python session (ASLR)

### Memory Map (confirmed via peek)
| Range | Access | Contents |
|-------|--------|----------|
| 0x006xxxxx | RW, NX | User heap |
| 0x1AExxxxx | RW, NX | Python objects |
| 0x7B1xxxxx | RO, X | _misty.pyd code+data (ASLR) |
| 0x7EF7xxxx | RO, X | _hack.pyd code (ASLR) |
| 0x80000000-0x81EFFFFF | RO, X | ROM (31MB, stable) |
| 0x20200000 | CRASH | Physical GPIO (unmapped) |
| 0xC8xxxxxx | CRASH | Kernel VA (supervisor) |

### Why the Exploit is Hard
- Heap is NX — can't execute injected code
- ROM is read-only — can't patch kernel code
- DLL code is read-only — can't modify import stubs
- peek uses plain LDRB — MMU enforces user-mode permissions
- No hidden _misty functions — 43 is all there is
- HAL dispatch table in kernel RAM — can't read it
- SVC calls go through kernel which checks capabilities
- Process has only 5 of 20 capabilities (no TCB/AllFiles/ReadDeviceData)

### What Still Might Work
1. **ROM scan** — find 0x20200000 in the runtime ROM, trace the code that uses it
2. **ROM function redirect** — use poke to modify Python object m_ml pointer
   to an existing ROM function that reads from address we control
3. **UART capture** — hardware approach, guaranteed to work
4. **RomPatcher+ trick** — if we can make it dump GPIO instead of SuperPage
