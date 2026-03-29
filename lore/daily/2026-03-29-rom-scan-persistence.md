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

## ROM Scan Results (In Progress)

First run found **6 hits** before crash at ~30.5MB:
- 0x80885750
- 0x80886E20
- 0x80889554
- 0x8088E4D4
- 0x8088F274
- 0x80BAFEC0

These are the ROM locations where the GPIO physical address 0x20200000
appears in the decompressed runtime image. The code surrounding these
addresses configures GPIO — the GPFSEL values we need are nearby.

Crash at ~30.5MB is reproducible — same address each time. Script needs
to resume past the crash point. Fixed with multi-file approach: each run
creates `E:\rscaXXXXX.txt`, reads all previous files to find resume point.

### Issues Encountered
- `os.mkdir('E:\\romscan')` failed silently — FAT32 issue, switched to E:\ root
- Progress file locked after crash — `Permission denied` on rewrite
- Fixed: unique files per run, never overwrite, read all to find max address

### What Still Might Work
1. **ROM scan** — 6 hits found, need to finish scan + trace surrounding code
2. **ROM function redirect** — use poke to modify Python object m_ml pointer
   to an existing ROM function that reads from address we control
3. **UART capture** — hardware approach, guaranteed to work
4. **RomPatcher+ trick** — if we can make it dump GPIO instead of SuperPage

## Incidents Log

### git add -A disaster (2026-03-27)
Used `git add -A` during merge which staged 171K files including Nokia
proprietary content. Push failed (HTTP 500). GitHub rolled back. Rebuilt
with clean-push branch and curated 78 safe files.

### git rm deleted local files (2026-03-29)
Used `git rm` to remove noki_python/ from GitHub which also deleted from
local disk. Should have used `git rm --cached`. Files restored from master
branch. Lesson: NEVER use `git rm` without `--cached` on files we want to keep locally.

### General issues
- Repeatedly forgot information from earlier in session (compaction)
- Re-asked questions user already answered
- Didn't check lore/memory files before acting
- Sloppy with paths and file operations
- Need to be more methodical and verify before destructive operations
