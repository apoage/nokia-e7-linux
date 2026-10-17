# Task 093: QEMU NLoader→Linux chain boot

## Status
**COMPLETE (2026-08-22).** Linux 6.12 boots through the *real* NLoader boot
chain in QEMU: NLoader runs hw_detect → Build/Copy images (TOC-driven) →
GENIO orchestrator → config build → "Start SOS at 0x80010000" → secure
trampoline → trampoline → zImage → kernel reaches the
`bcm2727-pinctrl` probe and boots to a clean VFS panic (no rootfs by
design). First end-to-end validation of the BCM2727 device tree.

## Boot proof (abridged)

```
NLoader: Copy images to SDRAM ... (33 TOC entries, copies fail non-fatally)
NLoader: PMM Loader not found            (bypass)
NLoader: Disable MMU
NLoader: SmartReflex DSP code not found  (bypass)
NLoader: Beginning configuration build   (hw_conf records → 0x30003f00)
NLoader: Total bootup time ... msec
NLoader: Start SOS at 0x80010000 (->0x80010000)
Booting Linux on physical CPU 0x0
CPU: ARMv6-compatible processor [410fb767] revision 7
OF: fdt: Machine model: Nokia E7-00 (RM-626)
earlycon: pl011 at MMIO 0x40100000
Kernel command line: earlycon=pl011,0x40100000 loglevel=8 keep_bootcon lpj=500000 init=/bin/sh
bcm2727-pinctrl 20200000.pinctrl: BCM2727B1 GENIO: 176 pads, 160 gpios (banks 0-4); banks 5/6 wake-only
Kernel panic - not syncing: VFS: Unable to mount root fs   ← full boot, no disk
```

## How to run

**[Updated by task 095 (2026-08-22): the command below is the current
one — PL011 moved to 0x40210000 (out of the XTI mailbox window) and
`lpj=` was already dropped by task 094. The "UART shim" references in
the historical sections below were an I2C master, not a UART — see
docs/bcm2727-xti-trace.md §2.]**

```sh
cd emulation/qemu/build
GLIB=.../glib-dev/usr   # see "Build environment repair" below
./qemu-system-arm -M nokia-e7 -m 256M -nographic \
  -drive file=../../firmware-re/analysis/nloader.bin,if=none,id=nloader,format=raw \
  -drive file=../../firmware-re/nokia-e7-onenand.img,if=none,id=onenand,format=raw \
  -kernel ../../kernel/linux/arch/arm/boot/zImage \
  -dtb ../../kernel/linux/arch/arm/boot/dts/broadcom/bcm2727-nokia-e7.dtb \
  -append "earlycon=pl011,0x40210000 loglevel=8 keep_bootcon"
```
(oneandand drive optional; nloader + -kernel + -dtb select chain mode)

## QEMU changes (emulation/qemu)

- **CPU → ARM1176** (`omap3630.c` child type + `nokia_e7.c` machine class).
  NLoader is ARMv6 and the kernel is ARMv6K-only; also fixed the SoC
  realize-time MIDR override `0x413fc082` (Cortex-A8!) → `0x410fb767` —
  with the A8 part number `__lookup_processor_type` finds no proc_info
  and head.S spins forever in `__error_p`.
- **Thumb-1 printf stub**: the old stub used Thumb-2 `movw/movt` — illegal
  on ARM1176. Rewritten with a literal-pool `ldr`.
- **Chain mode** (`nloader_e7.c`): nloader drive + `-kernel` →
  - PL011 console at **0x40100000** at the time (QEMU-only artifact;
    task 095 moved it to 0x40210000 — no real APE UART exists, see
    docs/bcm2727-xti-trace.md), wired to serial0.
  - DTB loaded with `/chosen/bootargs` rewritten from `-append`, placed
    at **0x88000000** (r2).
  - Trampoline at **0x80010000**: 0x150-byte Symbian-style header,
    entry-offset field @+0x148 = 0, ARM code setting r0=0, r1=~0, r2=DTB,
    jumping to zImage at **0x80010150**.
  - Synthetic **MCUSW TOC entry at slot 0** ([+0x10]=0x80010000);
    flash-derived entries follow at slot 1+.
  - Config block init: zero 0x30003e00..+0x100 (the secure-ROM stub's
    `bx lr` fill would otherwise set startup-reason bit31 → UPDAPP
    search) and set `[+0x60]=0x20004F00` so config-build records land at
    **0x30003f00** (behind the field struct — overwriting the struct
    clobbers `[+0x78]`, the TOC base the MCUSW search reads).

## NLoader runtime patches (chain mode only)

All in `nokia_e7.c`; addresses are guest VA:

| address | patch | why |
|---|---|---|
| 0x1003246a | NOP `bne` | auth-fail branch in cleanup launcher |
| 0x10031840 | NOP `bne` | PMM-loader path → "PMM Loader not found" early return (the found path would `blx` into the never-copied image at 0x10040000) |
| 0x1002fcd6 | NOP `bne` | LDSP auth in GENIO orchestrator → clean "not found" return (failure path hangs) |
| 0x1003071a | `beq`→`b` | GENIO_INIT TOC-search result → force "not found" (found → auth → hang) |
| 0x10032478 | NOP `beq` | "boot time + 0x14 == 0 → skip launch" (stubbed tick reads 0xFFFFFFEC; without the NOP main() returns instead of jumping!) |
| **0x10033b58** | ARM `bx r1` | secure jump trampoline — **entered in THUMB** (`bl`, not `blx`, from 0x10030436): its first halfword is `bx pc` switching to ARM at +4; patch the ARM side (+4), not the entry halfword (docs §6 said 0x10033b54 — Thumb-decodes 0xE12FFF11 as garbage Thumb-2) |

## Kernel changes (kernel/linux)

- defconfig: `ARM_AMBA` (selected by `ARCH_BCM2727` in mach-bcm/Kconfig —
  it's an invisible bool, defconfig lines alone do nothing) +
  `SERIAL_AMBA_PL011` + `_CONSOLE` (earlycon comes via
  `SERIAL_AMBA_PL011_CONSOLE`'s select). QEMU-validation console only;
  matches nothing on hardware.
- No DTS change needed — memory@0x80000000/256MB matches QEMU RAM, and
  `CONFIG_AUTO_ZRELADDR` picks zreladdr from the DTB.

## Boot-chain knowledge gained (beyond task 092's decode)

1. The secure trampolines are **Thumb-entered**; entry halfword is
   `bx pc`, ARM payload at +4. Any patch must respect the mode switch.
2. The config block at 0x30003e00 is a **field struct** (~0x100 bytes:
   record base +0x60, TOC ptr +0x78, startup reason +0x7c) with records
   appended at +0x100; record dst = `[+0x60] + 0x0FFFF000`.
3. Between Copy images and the launcher sit three image-auth gates
   (PMM loader 0x10031834, LDSP in GENIO orchestrator 0x1002fcc0,
   GENIO_INIT 0x1003070c) — all follow the found→auth→hang pattern;
   the "not found" paths are the clean exits in emulation.
4. The launcher tail checks `boot_ticks + 0x14 != 0` before jumping.
5. Full boot print sequence now on record: SUSD → Startup mode → Copy
   images → PMM → Disable MMU → SmartReflex → vcore/fop tables →
   config build (SMR/TOC/Startup Reason records + 128-word dump) →
   Total bootup time → Start SOS → jump.

## Build environment repair (host, 2026-08-22)

The distro upgrade (python 3.13→3.14, glib/numa dev packages removed)
wedged the QEMU build dir. **All of it is scripted now — rebuild with
`emulation/qemu-rebuild.sh [ninja args]`** (idempotent; safe to re-run
and self-disabling after a clean meson regen). It:
- replaces the broken `build/pyvenv/bin/meson` with a shim running
  meson 1.10.1 (from `~/.local/lib/python3.13/site-packages`) under
  python3.14,
- extracts dev debs under `build/` on first use (`glib-dev/` incl.
  gdbus-codegen, `numa-dev/`, `pixman-dev/`; `apt-get download` as
  user, network needed once),
- patches `build.ninja` (stale `/usr/bin/python3.13` regen input,
  missing gdbus-codegen input, glib/numa/tinfo link inputs repointed
  to runtime sonames, `-Werror` stripped) and builds with the right
  `CPATH`.
The proper fix with root: `apt install libglib2.0-dev libnuma-dev
libpixman-1-dev` + a working meson, then re-add `select PL011` to
NOKIA_E7 in hw/arm/Kconfig (reverted: PL011 is already default-y in
arm-softmmu and the select forces a meson regen) and let meson
regenerate — the script's patches no-op after that.

## Debugging recipes (paid for in hours — reuse)

- **Guest memory at a hang**: the chain-mode PL011 owns serial0, so
  `-serial null` / `-monitor stdio` abort with "chardev 'serial0' is
  already in use". Use plain `-nographic` and switch the stdio mux to the
  monitor with **Ctrl-A C** (`printf '\001c'` when piping), then
  `xp /Nwx <addr>`. HMP disassembly is NOT supported on this build
  ("Asm output not supported on this arch") — `xp` words + capstone
  instead.
- **Slow boot is not a hang**: the zImage inflate phase takes ~35-60 s
  under TCG (observed; hot loop = the bit-reader at the relocated
  decompressor, e.g. 0x80426ed0). `-d exec` multiplies this several-fold.
  Wait it out before diagnosing.
- **Vector-march signature**: PCs stepping +4 through zeros from 0x0/0xc/
  0x10 mean an exception landed in the EMPTY vector table — 0x10 = data
  abort, 0x04 = undefined instruction, 0x0c = prefetch abort. Look for the
  faulting store/fetch just before the march starts.
- **Finding where it spins/stops**: `-d exec -D file` then count Trace-line
  PCs (`Counter(pcs).most_common()`); the tail's top PC is the spin loop,
  and the last "sane" PC before a march is the divergence point.
  `-dfilter <range>` keeps the log affordable when hunting kernel-range
  execution.
- **printf shim reminder**: NLoader prints are format-string LITERALS
  (%s/%d unexpanded) — names and numbers are invisible; dump the guest
  structures instead of trusting the prints.

## Open items / next

- The chain now reproduces the hardware blockers faithfully:
  **interrupt controller** → **timer** → **UART** decode (docs/
  bcm2727-pinctrl-linux.md §4 ranking unchanged, now validated by a
  running kernel: `timer_probe: no matching timers found`, stall at
  calibrate without `lpj=`).
- Rootfs: once any storage is decoded (eMMC), add `-drive`+root= for a
  shell; today the chain ends at the VFS panic by design.
- On-host artifice to retire later: 0x40100000 PL011 and the
  in-memory DTB bootargs rewrite are QEMU-only; nothing leaked into the
  hardware DTS.
