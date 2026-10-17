# Task 105: SDRAM-TOC self-heal in the LLD blob — boot the real-flash chain

## Status

**EXECUTED 2026-08-25 — DID NOT BOOT; premise refuted, stopped per the
"if it does NOT boot" clause.**  The helper was implemented exactly as
specified (all four gates, byte-identical slot-0 rewrite, slot-1 end
marker; inlined into `lld_read`'s prologue), blobs + QEMU rebuilt, and
the boot reproduced the task-104 failure state unchanged ("SOS image
not found", no self-heal line).  Root cause of the miss: the boot
performs exactly three `lld_read` calls total (the TOC searches, all
before "Startup mode : Normal") and ZERO LLD reads after "Build TOC in
SDRAM" — the SOS+DECOMP copy takes a non-BML path, so the "guaranteed
read between blanking and cleanup" this task assumed does not exist.
Full evidence and next-step routing in
docs/bcm2727-onenand.md §5.2 (outcome paragraph under fix item 1);
log: docs/boot-105-selfheal.txt.  The writer hunt stays tasks/106,
close-out tasks/107.

**IDLE-TASK PROMPT (paste into the queue):**
> Execute tasks/105-O-toc-selfheal.md in /home/lukas/ps3/e7 (read it and
> docs/bcm2727-onenand.md §5 first). One job: teach the LLD blob to
> self-heal SDRAM-TOC slot 0, rebuild blobs + QEMU, boot, validate the
> chain reaches `e7:~#`. Do NOT hunt the TOC writer (tasks/106) and do
> not flip tasks/104 to DONE (tasks/107). Commit when done.

## Why

Task 104's chain runs NLoader's genuine boot flow off the real OneNAND
model but dies at "SOS image not found".  Bisect result (onenand doc
§5.1): between the shim's return and the "Build TOC in SDRAM" print,
something blanks **only slot 0** of the SDRAM TOC (0x101ff000) with
0xFF×32 — the end-marker pattern.  Both consumers walk 32-byte slots
**from slot 0** and stop at name[0]==0xFF, so one blank slot produces
every symptom (zero per-image lines + MCUSW miss).  Moving the MCUSW
entry to a later slot cannot work (§5.2 item 3).

The fix (§5.2 item 1): the LLD repairs the table on any flash read.
The SOS+DECOMP copy guarantees at least one LLD read between the
blanking and cleanup, so the repair always lands in time.

## The change (emulation/qemu-e7sw/lld.c)

Add one helper, called at the top of `lld_read()` (covers
`lld_read_spare()` too — it delegates to lld_read).  All four
conditions must hold, else return without touching memory:

1. cfg magic `'E7WS'` at 0x40200C00 (`struct shim_cfg`, e7sw.h),
2. `cfg->chain != 0`,
3. `*(volatile uint32_t *)0x30003e78 == 0x101ff000` (TOC pointer
   published AND at the canonical base — the board cfg keeps
   `toc_base = 0`, so strict equality is right),
4. `toc[0x14] == 0xFF` (slot 0 name[0] blanked — only then).

Action — rewrite slot 0 **exactly the way shim.c writes it** (byte-for-
byte, from the cfg):

```c
uint32_t *e = (uint32_t *)toc;          /* toc = 0x101ff000 */
for (i = 0; i < 8; i++) e[i] = 0;
e[2] = cfg->mcusw_size;                 /* +0x08 size        */
e[4] = cfg->mcusw_load;                 /* +0x10 load        */
memcpy(toc + 0x14, "MCUSW\0\0\0\0\0\0\0", 12);
for (i = 0; i < 32; i++) toc[32 + i] = 0xFF;   /* slot 1 = end marker */
if (shim_debug()) sw_log("lld: toc slot0 self-heal");
```

The slot-1 marker hides the shim's flash entries (slots 1-10) — that is
**exactly the task-093 table shape that booted** (MCUSW + end marker;
the MCUSW copy fails non-fatally by design, the mode-2 search still
resolves the trampoline load address — nloader-boot-mechanism §6).

## Build

```sh
cd /home/lukas/ps3/e7/emulation/qemu-e7sw && ./build.sh
cd /home/lukas/ps3/e7/emulation/qemu && ./qemu-rebuild.sh
```

**Rebuilding QEMU after build.sh is mandatory** — blobs are embedded
via `hw/arm/e7sw_blobs.h`; a forgotten rebuild runs stale blobs and
wastes a debug cycle.  If ld errors with `.text overlaps .vtbl`, the
driver-gap budget (text must stay below 0x1001E22C) is exhausted —
shrink the helper, don't move the vtbl.

## Boot + validate

```sh
cd /home/lukas/ps3/e7/emulation/qemu/build
./qemu-system-arm -M nokia-e7 -m 256M -nographic \
  -drive file=../../../firmware-re/analysis/nloader.bin,if=none,id=nloader,format=raw \
  -drive file=../../../firmware-re/nokia-e7-onenand.img,if=none,id=onenand,format=raw \
  -drive file=../../../emulation/rootfs/emmc.img,if=none,id=emmc,format=raw \
  -drive file=../../../emulation/rootfs/sd.img,if=none,id=sd,format=raw \
  -kernel ../../../kernel/linux/arch/arm/boot/zImage \
  -dtb ../../../kernel/linux/arch/arm/boot/dts/broadcom/bcm2727-nokia-e7.dtb \
  -append "earlycon=pl011,0x40210000 console=ttyAMA0 loglevel=7 root=/dev/mmcblk0 rootwait rw"
```

`E7SW_DEBUG=1` for the swbl:/lld: lines, `ONENAND_DEBUG=1` for the
model trace.  Expected markers in order: "Read TOC : 00020000 /
00040000 / 000A0000" → "Startup mode : Normal" → "Build TOC in SDRAM"
→ "Copy & Authenticate SOS+DECOMP … PASS" → **a `lld: toc slot0
self-heal` line somewhere before/around Copy images** → "Copy images
to SDRAM" → NO "SOS image not found" → "Disable MMU" → kernel log →
rcS banner "ROOT-UP: buildroot armv6/musl on the eMMC ext4" →
**`e7:~#`**.

## If it does NOT boot

Do not improvise past one iteration of diagnosis: reproduce with
`E7SW_DEBUG=1`, save the boot log, and if the failure point is new,
QMP-dump the TOC (`-qmp unix:/tmp/qmp.sock,server,nowait`, then QMP
`pmemsave` 0x101ff000/0x200 — **QMP, not HMP**; HMP mangles piped
multi-arg commands).  Write what you saw into
docs/bcm2727-onenand.md §5.2 (outcome paragraph) and STOP — the
writer hunt is tasks/106, the close-out is tasks/107.

## Gotchas

- gdbstub is a DEAD END on this host (gdb 17.1 lacks ARM — do not retry).
  [Resolved 2026-08-26: `apt install gdb-multiarch` — verified against
  the chain (hbreak at NLoader main, TOC readback, ARM+Thumb disasm).
  The note above was about the bisect's time budget, not physics.]
- /tmp leftovers from the bisect session may be gone; everything needed
  is in the docs.
- Commit locally (never push third-party content; blobs + docs live in
  this repo, the QEMU tree is gitignored by convention).
