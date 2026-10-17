#!/bin/bash
# Build the ARM1176-safe BusyBox initramfs for the Nokia E7 chain boot.
#
# RESCUE IMAGE since task 098: the primary boot is root=/dev/mmcblk0 on
# the eMMC ext4 (build-emmc-root.sh); this initramfs only ships when
# -initrd is passed, to get a shell when the eMMC root is broken.
#
# Why a Debian armel busybox (task 097): the local cross toolchain is
# arm-linux-gnueabihf, and its glibc (crt/libc objects) is built for
# ARMv7-A — a static link against it faults on the E7's ARM1176
# (ARMv6/VFPv2) no matter what -march the app itself uses.  Debian's
# armel busybox-static is a self-contained ARMv5TE soft-float static
# binary: runs on every ARMv5TE+ core including ARM1176.  The earlier
# from-source build (busybox-1.36.1/ in this dir) was ARMv7 and only
# ever ran under the old OMAP3630 Cortex-A8 board model.
#
# Usage: ./build-initramfs.sh [path-to-busybox]
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
cd "$SCRIPT_DIR"

BUSYBOX="${1:-busybox-armel}"
DEB_CACHE=busybox-static_1.37.0-6+b9_armel.deb

if [ ! -x "$BUSYBOX" ]; then
    if [ ! -f "$DEB_CACHE" ]; then
        echo "Downloading Debian armel busybox-static..."
        wget -q "http://deb.debian.org/debian/pool/main/b/busybox/$DEB_CACHE"
    fi
    rm -rf busybox-armel-pkg
    dpkg-deb -x "$DEB_CACHE" busybox-armel-pkg
    cp busybox-armel-pkg/usr/bin/busybox "$BUSYBOX"
fi

readelf -A "$BUSYBOX" | grep -q 'Tag_CPU_arch: v5' || {
    echo "ERROR: $BUSYBOX is not ARMv5TE (wrong arch?)" >&2; exit 1;
}

echo "Staging initramfs..."
rm -rf initramfs
mkdir -p initramfs/{bin,sbin,dev,proc,sys,tmp,run,etc,root,mnt/emmc}
cp "$BUSYBOX" initramfs/bin/busybox

# applet symlinks (same set the old from-source build provided)
for a in sh ash mount umount mkdir mknod ln ls cat echo printf uname \
         hostname sleep ps top kill dd cp mv rm touch vi less head tail \
         grep sed awk cut sort wc tr xargs env pwd date dmesg hexdump \
         od stat sync blkid fdisk swapon fsck find which clear reboot \
         poweroff halt ping ifconfig ip route netstat insmod rmmod \
         lsmod modprobe switch_root setsid cttyhack stty true false test; do
    ln -sf busybox "initramfs/bin/$a"
done

cat > initramfs/init << 'INIT'
#!/bin/sh

mount -t proc proc /proc
mount -t sysfs sysfs /sys
mount -t devtmpfs devtmpfs /dev

[ -c /dev/console ] || mknod /dev/console c 5 1
[ -c /dev/null ]    || mknod /dev/null c 1 3

hostname nokia-e7

echo
echo "============================================"
echo "  Nokia E7-00 — Linux on BCM2727B1 (RAPU)"
echo "  kernel $(uname -r) @ $(uname -m)"
echo "  RAM: $(awk '/MemTotal/{print $2 " " $3}' /proc/meminfo)"
echo "============================================"
echo "-- RESCUE shell (initramfs). Primary boot is"
echo "-- root=/dev/mmcblk0 on the eMMC ext4; boot"
echo "-- this image only to repair it."
echo
echo "-- block devices:"
cat /proc/partitions

# mount the eMMC rootfs image if the block device showed up
i=0
while [ $i -lt 50 ] && [ ! -b /dev/mmcblk0 ]; do
    sleep 0.1; i=$((i + 1))
done
if [ -b /dev/mmcblk0 ]; then
    if mount -t ext4 /dev/mmcblk0 /mnt/emmc 2>/dev/null; then
        echo "-- /dev/mmcblk0 (Rapu_Memcard eMMC) mounted on /mnt/emmc:"
        ls /mnt/emmc
        [ -f /mnt/emmc/README ] && cat /mnt/emmc/README
    else
        echo "-- /dev/mmcblk0 present, ext4 mount failed"
    fi
fi

exec setsid cttyhack /bin/sh
INIT
chmod +x initramfs/init

echo "Packing initramfs.cpio.gz..."
cd initramfs
find . | cpio -o -H newc 2>/dev/null | gzip > ../initramfs.cpio.gz
cd "$SCRIPT_DIR"

SIZE=$(du -h initramfs.cpio.gz | cut -f1)
echo "Done: initramfs.cpio.gz ($SIZE)"
