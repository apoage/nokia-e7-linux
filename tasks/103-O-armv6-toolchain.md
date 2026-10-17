# Task 103: ARMv6 userland toolchain — musl.cc cross + buildroot rootfs

## Status
**DONE (2026-08-23).** Handoff desk-queue item #1: a real cross
toolchain for the phone. The musl.cc `armv6-linux-musleabi` cross
(gcc 11.2, musl 1.2.x, soft-float, kernel headers 5.15) drives a
buildroot 2026.02.3 external-toolchain build; its rootfs **is** the
new `emmc.img` (busybox 1.37 + strace + i2c-tools + musl dynamic
runtime + e7-hello smoke binaries). We can now compile anything for
the phone: `toolchain/musl.cc/armv6-linux-musleabi-cross/bin/` is the
compiler, `-static` just works, and both static and dynamic binaries
were proven on the target through the full NLoader chain boot.

The task-097 rule stands, now with a workaround: gnueabihf glibc is
ARMv7 and can never link static userland for the ARM1176. The musl
cross is ARMv6-safe everywhere (its gcc even defaults to armv5t —
see gotcha 1).

## What landed

1. **`toolchain/`** (gitignored, like kernel/ and qemu/ trees):
   - `musl.cc/armv6-linux-musleabi-cross/` — the cross (~500MB with
     build artifacts); tarball cached beside it
   - `buildroot/` — buildroot 2026.02.3 from the GitHub mirror
     (buildroot.org is unreachable from this network)
   - `buildroot-o7/` — the O= build dir, `dl/` — the download cache
   - `hostbin/` — PATH shim: `install -> /usr/bin/gnuinstall`
     (this system's /usr/bin/install is uutils 0.8.0, which
     buildroot hard-rejects, uutils/coreutils#12166)
2. **`emulation/rootfs/build-buildroot-root.sh`** — one-shot
   orchestrator: fetch/extract toolchain + buildroot if missing,
   apply the sysroot workaround, compile e7-hello (static + dynamic)
   into the overlay, stage modules/*.ko, configure, build, verify
   every shipped binary is ARMv6 (readelf gate), install as
   `emmc.img` (task-098 busybox root kept once as
   `emmc.busybox-root.bak`). Idempotent; `shell` arg drops into the
   build dir for menuconfig.
3. **`emulation/rootfs/buildroot-e7_defconfig`** — machine-independent
   config: external custom musl toolchain, gcc 11 declared, headers
   5.15, arm1176jzf-s, EABI soft-float (matches the Debian armel ABI
   that carried tasks 097-102), busybox init, i2c-tools + strace,
   64 MiB ext4. Host paths (toolchain/overlay/dl) are sed'ed in at
   configure time.
4. **`emulation/rootfs/buildroot-overlay/`** — inittab/rcS/rcK/fstab/
   hostname/profile, same boot shape and banner as the task-098 root
   (`ROOT-UP: buildroot armv6/musl on the eMMC ext4`).
5. **`emulation/rootfs/e7-hello.c`** — the smoke test, shipped twice:
   `/usr/bin/e7-hello` (static) and `/usr/bin/e7-hello-dyn`
   (dynamic, exercises `/lib/ld-musl-arm.so.1`).

Boot command unchanged: `root=/dev/mmcblk0 rootwait rw`, no -initrd
(docs/bcm2727-pinctrl-linux.md §5). Rescue paths unchanged:
`-initrd` initramfs, and build-emmc-root.sh still rebuilds the old
busybox root.

## Validation (chain boot, 2026-08-23)

- Boot through NLoader → `VFS: Mounted root (ext4)` → rcS banner →
  `e7:~#` on the buildroot busybox v1.37.0.
- `/usr/bin/e7-hello` AND `/usr/bin/e7-hello-dyn` both print:
  `uname: Linux nokia-e7 6.12.0-... armv6l`,
  `cpu: ARMv6-compatible processor rev 7 (v6l)`,
  `sqrt(2)=1.414214 pow(2,10)=1024.0 (musl soft-float)` — static
  AND dynamic musl linking live on the target.
- `strace /usr/bin/e7-hello-dyn` traces through `exit_group(0)` —
  ptrace tooling works (also the toolchain's v6KZ strace binary).
- `i2cdetect -l` runs clean (no adapters yet — the sys+0x22/23/25xx
  masters have no driver; the tools are staged for that decode).
- Task-102 regression check: `/proc/interrupts` still counts every
  mmc command on the IRQ path — `18: 3769 ... 35 Edge 20202800.mmc`
  (buildroot init issues far more commands than the old root),
  `19: 13 ... 36 Edge 20202900.mmc`.
- Persistence: guest `echo task103-buildroot > /task103-marker; sync`
  → host `debugfs -R 'cat /task103-marker' emmc.img` returns it.

## Gotchas (each one cost a build cycle)

1. **The musl.cc "armv6" gcc defaults to `-march=armv5t`**
   (`-Q --help=target` says so; binaries readelf as `Tag_CPU_arch:
   v5T`). Safe for the ARM1176 — same class as the Debian armel
   busybox — but not what the name promises. Buildroot's
   BR2_arm1176jzf_s wrapper passes `-mcpu=arm1176jzf-s` for every
   package (→ `Tag_CPU_arch: v6KZ`); for standalone compiles pass
   `-march=armv6 -mtune=arm1176jzf-s` yourself (that's what the
   e7-hello builds do → tag `v6`). Don't layer `-march` onto the
   wrapper's `-mcpu` — gcc warns about the conflict.
2. **musl.cc sysroots are flat** (`include/`, `lib/` at the root, no
   `usr/` level); buildroot's kernel-headers check hardcodes
   `<sysroot>/usr/include`. Fix: `ln -s . usr` inside the sysroot
   (script does it, idempotent).
3. **Kconfig command-line assignment does not persist** —
   `make BR2_TOOLCHAIN_EXTERNAL_PATH=... olddefconfig` silently
   drops the value. The script seds the generated .config instead.
4. **Hidden mirror symbols**: `BR2_TOOLCHAIN_EXTERNAL_PREFIX` is
   derived; the visible option is
   `BR2_TOOLCHAIN_EXTERNAL_CUSTOM_PREFIX`. Setting the mirror in a
   defconfig does nothing (it resolved to `arm-linux` and buildroot
   tried to run `arm-linux-gcc`).
5. **The custom-toolchain gcc-version choice defaults to the newest
   gcc** (15.x in 2026.02) and the check is strict: declare
   `BR2_TOOLCHAIN_EXTERNAL_GCC_11` for this cross.
6. **musl.cc ships a WORKING gfortran + libgomp** — buildroot's
   sanity checks compile-and-link test programs and demand the
   config declares what the toolchain can actually do
   (`BR2_TOOLCHAIN_EXTERNAL_FORTRAN`, `BR2_TOOLCHAIN_EXTERNAL_OPENMP`),
   or they fail with "not selected but is available".
7. **i2c-tools is hidden** behind `BR2_PACKAGE_BUSYBOX_SHOW_OTHERS`
   (busybox has its own i2c applets).
8. `-mcpu=arm1176jzf-s` readelfs as `Tag_CPU_arch: v6KZ` — the arch
   gate in the script accepts the v6 family (v6, v6K, v6KZ, v6T2).

## Next

Desk queue: QEMU chain fidelity (real OneNAND model, peel the board
file off OMAP scaffolding), display/DSI + I2C master decode (the
i2c-tools in the image are staged for exactly that). Phone-bound
unchanged: soft-UART wire, eMMC pins, GPIO-mux contract items.
