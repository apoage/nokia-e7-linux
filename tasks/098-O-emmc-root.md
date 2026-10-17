# Task 098: root= on the eMMC ext4 — the storage story closed

## Status
**DONE (2026-08-22).** Handoff desk-queue lead #1 executed: the kernel
now mounts the eMMC ext4 as the real root (`root=/dev/mmcblk0
rootwait rw`, no initramfs) and runs `/sbin/init` = busybox init from
it (inittab + rcS + `e7:~#` shell). The initramfs is demoted to a
rescue fallback. Validated through the full NLoader chain, both boots,
with cross-boot persistence proven host-side (debugfs).

## What landed

1. **`emulation/rootfs/build-emmc-root.sh`** — stages `emmc-root/`
   (busybox + applets in /bin + /sbin, inittab, rcS/rcK, fstab,
   passwd/group, profile with `PS1='e7:\w# '`, empty `/dev` dir) and
   runs `mke2fs -F -t ext4 -d emmc-root emmc.img 64m` (no partition
   table). Same Debian armel busybox (ARMv5TE static) as the
   initramfs; the fetch/verify logic mirrors build-initramfs.sh.
2. **`build-initramfs.sh` /init is now explicitly RESCUE** — same
   behavior (mount /dev/mmcblk0 at /mnt/emmc if present, shell), new
   banner explaining it is the repair path.
3. **Docs**: pinctrl-linux §5 (primary `root=` command + evidence +
   rescue note, old initramfs command labeled as the rescue path) and
   memcard §6.3 (same, kept consistent).

No kernel or QEMU changes — config already had everything
(DEVTMPFS_MOUNT, EXT4, MMC_BLOCK built in).

## How the no-initramfs root path actually works (verified in source)

- `root=/dev/mmcblk0 rootwait` → `prepare_namespace` → `wait_for_root`
  polls `early_lookup_bdev("/dev/mmcblk0")`, which resolves through the
  **kernel-internal block class** (`devt_from_devname`), not any /dev
  filesystem — so it works before devtmpfs exists.
- `mount_block_root` → `create_dev("/dev/root", ROOT_DEV)` mknods the
  mount node on the initramfs-era rootfs, then mounts that — /dev
  files are never consulted.
- `devtmpfs_mount()` runs AFTER `mount_root`, mounting on the **relative
  path "dev"** — i.e. the ext4's own `/dev` directory (cwd is /root by
  then). This is why the image ships an empty /dev dir. On this path
  CONFIG_DEVTMPFS_MOUNT applies (its initramfs exclusion is exactly why
  the task-097 /init had to mount devtmpfs itself).
- An initramfs `/init` PREEMPTS `root=` entirely — that's what makes
  `-initrd` the rescue switch, no other flag needed.

## Validation (boot-tested, both modes, 2026-08-22)

Primary (no -initrd): `Waiting for root device /dev/mmcblk0...` →
`mmcblk0: mmc0:0001 QEMU!! 64.0 MiB` → `EXT4-fs (mmcblk0): mounted
filesystem ... r/w` → `VFS: Mounted root (ext4 filesystem) on device
179:0` → `Run /sbin/init as init process` → rcS banner +
`-- ROOT-UP: running from the eMMC ext4` → `e7:~#` prompt, commands
executed, `/dev/root / ext4 rw` in /proc/mounts.

Persistence: guest `echo ... > /task098-marker; sync` → host `debugfs
-R "cat /task098-marker" emmc.img` returns it. Rescue boot (with
-initrd) then read the same marker from /mnt/emmc and its own write
persisted identically — cross-boot durability. The rescue mount also
logged `EXT4-fs (mmcblk0): recovery complete` (the primary QEMU was
SIGKILLed after sync) — journal replay works.

## Notes / gotchas

- `mke2fs -d` needs `-F` on a regular file (prompts otherwise); files
  land owned by the building uid — irrelevant for a root-only system.
- Board file **replaces** /chosen/bootargs with -append (no merge), so
  the primary cmdline must carry console+earlycon itself.
- Chain boot to the shell prompt takes ~5s wall clock — the whole
  NLoader → kernel → busybox-init path.
- loglevel=7 for these boots (mmc + VFS lines are KERN_INFO).

## Next

Unchanged from the handoff queue, now starting at lead #2: GPIO mux →
ICU wiring (ROM call sites 0x800B80E8/0x800B80F0), then genio42
soft-UART, SD ch1, IRQ-driven mmc. Phone-bound storage pins stay in
memcard §6.4.
