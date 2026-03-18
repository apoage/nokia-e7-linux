# Kernel Build Notes

## Defconfig
- File: `arch/arm/configs/nokia_e7_defconfig` (106 lines)
- Copy: `kernel/configs/nokia_e7_defconfig`
- Stats: ~740 =y, ~90 =m, 56s build, 3.8MB zImage

### Usage
```bash
make ARCH=arm CROSS_COMPILE=arm-linux-gnueabihf- nokia_e7_defconfig
make ARCH=arm CROSS_COMPILE=arm-linux-gnueabihf- -j$(nproc) zImage dtbs modules
```

### What's in
- OMAP3 only (no OMAP2/4/5, AM33xx, DRA7)
- TCP/IP, IPv6, DHCP, netfilter, packet, unix sockets
- USB host (EHCI, OHCI, MUSB OTG)
- USB dongles: AX88xxx, RTL8150/8152, CDC-ECM/NCM, RNDIS, RTL8XXXU,
  ATH9K_HTC, RT2800USB, RTL8187, FTDI, CP210x, PL2303, CH341, usb-storage
- Bluetooth: btusb, hci_uart, rfcomm, bnep, hidp
- Filesystems: ext4, VFAT, exFAT, tmpfs, devtmpfs
- I2C, GPIO, watchdog, regulator framework
- TWL4030_CORE, REGULATOR_TWL4030, GPIO_TWL4030, TWL4030_POWER
- DMA: DMADEVICES, DMA_OMAP (omap-dma-engine probes on QEMU sDMA model)
- KEYBOARD_LM8323, INPUT_MATRIXKMAP (for QWERTY keyboard)
- Debug: DWARF, printk timestamps, magic-sysrq, kallsyms

### What's out
- Sound, display/DRM (add when display tested, Task 013 QEMU model done)
- Non-OMAP3 SoC support
- Future: narrow USB dongle list to tested devices

### Critical config & patches
- `CONFIG_CPU_IDLE=y` required — without it, `omap3_do_wfi` stack corruption
  (mismatched ldmfd without stmfd when called from C via `omap3_pm_idle`)
- Kernel patch: `vc.c` NULL guard in `omap3_vc_set_pmic_signaling()` — the
  voltage controller dereferences NULL `vc.timings` when no TWL PMIC is present
  (NOTE: with TWL4030_CORE=y and QEMU TWL5031 model, this path now works)
- Kernel patch: `lm8323.c` DT support — `of_match_table` with
  `"national,lm8323"`, property parsing for rows/cols, optional IRQ
  (upstream has zero DT support). Also `IRQF_ONESHOT` fix for INTC-wired IRQ.
- Kernel patch: `apds990x.c` DT support (~25 lines) — `of_match_table` with
  `"avago,apds990x"`, `apds990x_parse_dt()` allocates pdata via `devm_kzalloc`,
  sets `ga=0` (triggers built-in uncovered-sensor defaults in probe), reads
  optional `avago,pdrive` and `avago,ppcount` from DT. Probe falls through to
  DT parsing when `platform_data` is NULL. Driver now probes fully from DTS.

## Rootfs
- BusyBox 1.36.1 static ARM (1.37.0 fails with GCC 15)
- BusyBox build: disable CONFIG_TC (CBQ structs removed from headers)
- Initramfs: `emulation/rootfs/initramfs.cpio.gz` (1.1MB)
- Ext4: `emulation/rootfs/initramfs/rootfs.ext4` (64MB, BusyBox + /init script)
  - Built with `fakeroot mkfs.ext4 -F -L rootfs -d <tmpdir> rootfs.ext4`
  - /init: mounts proc/sys/devtmpfs, prints banner, execs /bin/sh
  - /etc/init.d/rcS: sets hostname, prints kernel version
- Build script: `emulation/rootfs/build-initramfs.sh`
- NOTE: `emulation/qemu/rootfs.ext4` is CORRUPTED (zero-filled) — use the one in initramfs/

## Gotchas
- Init scripts MUST have LF line endings — the Write tool produces CRLF,
  always verify with `file` command. CRLF → shebang parsed as `#!/bin/sh\r` → ENOENT.
- `rdinit=/init` for initramfs, `init=/init` for block device root
- `mkfs.ext4 -d <dir> image` populates ext4 without needing sudo/mount
