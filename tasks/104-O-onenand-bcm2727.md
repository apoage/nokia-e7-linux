# Task 104: QEMU chain fidelity — real OneNAND model + BCM2727 SoC skeleton

## Status

**DONE + VALIDATED (2026-08-26, task 107 close-out).** The SDRAM-TOC
preload and the flash-driver NOPs are GONE: NLoader reads the real
flash TOC and real flash images through a register-level OneNAND model
via a SWBL-role driver blob, the board file drives a BCM2727 SoC
skeleton, and **the chain runs NLoader's genuine boot flow all the way
to Linux `e7:~#`** — validated twice (plain TCG and `-icount`) with
interactive post-checks (`mount` ext4-rw root, bcm2727-icu counts in
`/proc/interrupts`, mmcblk1 with the SD drive attached).  Baseline log:
`emulation/qemu-e7sw/boot-baseline-task104.log`.

The slot-0 blanking gate that held validation open was root-caused
across tasks 105–106: NLoader's Build-TOC tail RE-EMITS the SDRAM TOC
from its own record table and stamps a 0xFF×32 end marker at
0x101ff000 + count×0x20 — with the record curation starved in
emulation (count == 0) the marker landed on the shim's MCUSW slot 0.
Neutralized as a §4 ledger NOP (0x10031f78, emulation artifact per the
task-106 branch test); the tasks-105 LLD self-heal stays in the blob,
structurally dead (no LLD traffic post-blanking to ride on).  The
final SDRAM-TOC chain shape and why it is the faithful one:
docs/bcm2727-onenand.md **§5.3** (fix history: §5.1/§5.2).  Soft-UART
note: the su42 tap decodes byte-exact at the task-100 extent (511
frames, 0 framing errors); `console=ttySU0`-only boots wedge after the
early log — pre-existing, console-path-specific, desk item (softuart
doc §3 task-107 note).

## What landed

1. **`emulation/qemu/hw/arm/bcm2727.[ch]`** (gitignored tree, like all
   QEMU work — design lives in docs): BCM2727B1 SoC skeleton. ARM1176 +
   64-KiB SRAM @ 0x40200000 + 4-MiB PDRAM @ 0x10000000 + every decoded
   model moved out of the old board file (ICU, OS timer, XTI mailbox +
   doorbell, Rapu_Memcard, GPIO mux + DATIN + soft-UART tap) **+ the new
   OneNAND model** (0x20010000, drive-backed, spare=0xFE contract) **+
   the shim-log port** 0x40208000 (byte writes → "swbl:" lines; the
   shim runs pre-main and cannot use the XTI printf).
2. **`emulation/qemu/hw/arm/nokia_e7.c`** rewritten as a thin board:
   boots the SoC, loads NLoader + blobs, applies the runtime patches
   (ledger in the onenand doc §4), chain-mode handoff unchanged.
   `hw/arm/Kconfig`: NOKIA_E7 no longer selects OMAP3630.
3. **`emulation/qemu-e7sw/`** (NEW, committed): the SWBL-role guest
   blobs, built by `build.sh` with the task-103 musl.cc ARMv6 cross —
   - `lld.c` + `vt.S` + `lld.ld`: the OneNAND LLD. Vtable trampolines
     at the exact installer addresses (0x1001E22E-0x1001EF76); open /
     select / GetDevInfo / read / read-spare implemented to the decoded
     contracts; fills the BML geometry (identity LSN map) and the
     devinfo+0xC0 read mirror.
   - `shim.c` + `shim.ld`: runs before NLoader main (PC=shim,
     LR=0x1003259b) — reads the real flash TOC through the model,
     curates it into 0x101ff000 (drops the auth-walled SOS+IVE3*
     variants and offsets ≥1 MiB), writes the chain-mode MCUSW slot 0,
     publishes the pointer at 0x30003e78.
   - `onenand_ll.c`: register-level protocol (RESET/READID/READ),
     shared by both blobs, no statics.
   - Output: blobs embedded as `hw/arm/e7sw_blobs.h`; **rebuild QEMU
     after changing the blob sources** (build.sh regenerates it).
4. **Build wiring**: meson/Kconfig updated for bcm2727.c; the surgery
   build dir needed hand-editing (build.ninja stanzas + link edge +
   regen-guard stamp — meson regen is still broken on this host, see
   qemu-rebuild.sh header).

## Decode record (the RE half of this task)

Full record in **docs/bcm2727-onenand.md**: the 22-slot vtable map
(installer 0x1002f1a4 → 0x10007dd8), per-slot calling conventions and
return codes (init-class returns plain 0; read-class returns
FSR 0x11000000), the GetDevInfo validator (0x100283e4: 64/128 pages,
4/8 sectors, 16-bit bus only), the identity-LSN geometry cells, the BMI
bad-block-pool protocol (ULOCKPCH/LOCKPCHD markers + 0xFE spare +
512-byte mirror — impossible on a main-only dump), the flash TOC ↔
SDRAM TOC division of labor (NLoader's 0x10031834 is an empty stub —
SWBL's role confirmed), and the root-TOC 16-byte record shape that the
Root-TOC search expects at BML logical 0x20000 (not present in our
dump — OPEN).

## Gotchas (each cost a debug cycle)

1. **Thumb-1 has no `b.w`** (ARM1176 = ARMv6). Trampolines are
   `ldr r3,[pc,#i]; bx r3` with the literal word at a computed
   4-aligned address; gas's PC arithmetic must match the runtime base
   (section VMA 4-aligned, first slot at +2).
2. **r3 is the 4th argument.** The plain trampoline clobbers it — for
   the READ entries (dst in r3) that meant copying a page over the
   LLD's own code. The read slots use `push {r3,lr}; ldr; blx; pop
   {r3,pc}` wrappers and the C side reads dst as its first stack arg.
   And the FIRST wrapper attempt (`pop {r3}; bx lr`) lost the caller's
   LR to the blx — hence `pop {r3,pc}`.
3. **Return codes are per-slot**: vtable[+0] must return 0 (nonzero →
   "BML Init error: 80090000"); the read slots return FSR codes.  One
   convention for all slots = the other error.
4. **The failed BML_Open's cleanup zeroes the devinfo vtable mirror**
   (read slot +0x68 → NULL): forcing the success branch is not enough,
   the close call (0x10028cf0) must be NOPed too.
5. **The shim must not call NLoader's printf** — pre-main, the
   formatter globals are code bytes (jump through garbage). Use the
   shim-log port.
6. **Spare bytes must read 0xFE**, not the erased 0xFF — every BMI
   check compares against 0xFE ("clean block").
7. **NLoader's TOC search reads land ON the SDRAM TOC** (the read-dst
   literal is the TOC base). On hardware a successful root-TOC read
   replaces the table legitimately; on our dump the misses zero it.
   Current mitigation: the LLD parks reads targeting 0x101ff000 at
   0x10201000. Not sufficient — see the open gate.
8. **HMP mangles multi-arg commands over a pipe** (pmemsave). Use QMP
   (`-qmp unix:…` + JSON) for guest memory dumps.
9. Build-env: new source files need build.ninja surgery (compile
   stanza + static-archive member + link edge), and the meson regen
   rule must stay neutered (regen-guard stamp).

## Env knobs added

`E7SW_DEBUG=1` (shim + LLD logging via the log port), `ONENAND_DEBUG=1`
(model command trace), plus the existing `XTI_DEBUG` / `MEMCARD_DEBUG`
/ `SUUART_*`.

## Next

1. ~~Validation follow-up~~ DONE — tasks 105/106/107 (writer caught,
   decoded, NOP'd; chain validated; final shape documented in
   onenand doc §5.3).
2. Root-TOC shape at BML logical 0x20000 (16-byte records, MCUSW-keyed)
   — not in our dump; a phone-side raw read of blocks 2-5 would pin
   the BML logical→physical offset.
3. Then the rest of the desk queue: display/DSI, I2C masters
   sys+0x22/23/25xx, USB OTG +0x25Axx, memory-map gaps, sgx540 side
   thread, su-console wedge (printk flush vs bit-bang earlycon when
   ttySU0 is the only console — softuart doc §3 task-107 note).
