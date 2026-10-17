# Task 097: eMMC phase 2 — shell in the chain (model + driver + initramfs)

## Status
**DONE (2026-08-22).** Handoff lead #2 executed end-to-end: NLoader
chain boot → Linux 6.12 → interactive busybox shell on ttyAMA0 → ext4
mounted from the eMMC image → file written + persisted (verified
host-side via debugfs). The VFS panic is gone. Full detail:
**docs/bcm2727-memcard.md §6**.

## What landed

1. **QEMU `bcm2727.memcard`** (`emulation/qemu/hw/arm/nokia_e7.c`):
   0x20202800, 2×0x100 channels bridged onto QEMU sd.c (`emmc` card on
   ch0 via `-drive id=emmc`, `sd-card` on ch1 via `id=sd`); command
   write → `sdbus_do_command`, FIFO +0x20 ↔ `sdbus_read/write_byte`,
   done → ICU 0x23/0x24. MEMCARD_DEBUG=1 traces it.
2. **Chain-mode `-initrd`**: image at 0x87000000 (below DTB at
   0x88000000, ~100MB clear of zreladdr), linux,initrd-start/end in
   /chosen.
3. **Console RX**: PL011 irq → qemu_irq adapter → ICU id 0x1a3 (unused,
   GPIO ids end 0x193); DT node serial@40210000 with BOTH clock names
   (uartclk + apb_pclk — the AMBA core fetches apb_pclk before the
   periphid match; missing it = silent eternal probe deferral).
4. **Linux `mmc-bcm2727`** (`drivers/mmc/host/mmc-bcm2727.c`): PIO
   polling host; executes mrq->sbc/mrq->stop itself (core hands them
   to the host); devm_ioremap to coexist with the GENIO node's
   region claim. Kconfig MMC_BCM2727, DTS mmc@20202800/2900,
   defconfig += MMC, MMC_BLOCK, EXT4.
5. **Initramfs**: Debian armel busybox-static (ARMv5TE soft-float
   static). The local arm-linux-gnueabihf glibc is ARMv7 — a static
   link faults on ARM1176 regardless of app -march; the Feb busybox
   only ever ran on the OMAP3630-era board. build-initramfs.sh
   rewritten (downloads the deb once, verifies Tag_CPU_arch v5).
6. **eMMC image**: 64MB ext4, no partition table (mke2fs -d), content
   /README + /etc/hostname.

## Bugs the bring-up caught

- **ICU handler `__init`**: bcm2727_icu_handle executed from
  freed+poisoned initmem (0xe7fddef0 trap) once /init reached
  free_initmem — latent since task 094, all earlier boots panicked
  first. Fixed (handler not __init).
- **Missing CMD12**: the mmc core puts sbc/stop INSIDE the host
  request; the first driver ignored them → open-ended CMD18 left the
  card in Sending-Data forever → every later data read = card-dummy
  zeros.
- **Model error-flag logic**: `(rspf!=0 || rlen==-2) && rlen!=0`
  excluded the main case (expected response, rlen 0) → "successful"
  reads off a wedged card returned zeros. Also: data phase no longer
  armed on failed commands.
- **Userland arch**: first rebuild set busybox to ARMv6 but the static
  glibc objects were still v7 → replaced by the armel binary.

## Validation

Boot log (task-run 2026-08-22): `mmc0: new MMC card at address 0001`,
`mmcblk0: mmc0:0001 QEMU!! 64.0 MiB`, `/dev/mmcblk0 mounted on
/mnt/emmc`, guest `echo > write-test.txt; sync` → host `debugfs -R
"cat /write-test.txt" emmc.img` prints the content. Shell prompt
interactive (commands piped via stdin, cursor-position query `[6n`
visible = real tty).

## Next

- Hardware bring-up items in docs/bcm2727-memcard.md §6.4 (payload vs
  raw-token response layout, FIFO byte order, +0x0C bits, divisor).
- GPIO mux → ICU wiring (handoff lead #3) unchanged.
- Optional: root= switch to the eMMC ext4 (real rootfs), interrupt-
  driven mmc variant, SD channel with a card image.
