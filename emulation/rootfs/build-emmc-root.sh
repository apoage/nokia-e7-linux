#!/bin/bash
# Build the eMMC root filesystem image (task 098): a proper busybox root
# on /dev/mmcblk0, replacing the initramfs-as-everything boot.  The
# initramfs stays around as a rescue fallback (build-initramfs.sh).
#
# Same ARM1176-safe userland as the initramfs: Debian armel busybox
# (ARMv5TE soft-float static) — the gnueabihf glibc is ARMv7 and can
# never be linked static for this machine (task 097 lesson).
#
# Usage: ./build-emmc-root.sh [path-to-busybox]
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
cd "$SCRIPT_DIR"

BUSYBOX="${1:-busybox-armel}"
DEB_CACHE=busybox-static_1.37.0-6+b9_armel.deb
IMG=emmc.img
ROOT=emmc-root
SIZE=64m

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

echo "Staging $ROOT/..."
rm -rf "$ROOT"
mkdir -p "$ROOT"/{bin,sbin,usr/bin,usr/sbin,etc/init.d,dev,proc,sys,tmp,run,root,mnt}
chmod 1777 "$ROOT/tmp"
cp "$BUSYBOX" "$ROOT/bin/busybox"

# applet symlinks: user utilities in /bin, admin ones in /sbin
for a in sh ash mount umount mkdir mknod ln ls cat echo printf uname \
         hostname sleep ps top kill dd cp mv rm touch vi less head tail \
         grep sed awk cut sort wc tr xargs env pwd date dmesg hexdump \
         od stat sync blkid fdisk find which clear true false test \
         ping ifconfig ip netstat setserial devmem; do
    ln -sf /bin/busybox "$ROOT/bin/$a"
done
for a in init poweroff halt reboot mdev switch_root swapon fsck \
         insmod rmmod lsmod modprobe sysctl route pivot_root; do
    ln -sf /bin/busybox "$ROOT/sbin/$a"
done
# kernel tries /sbin/init first; also keep the classic fallbacks happy
ln -sf /bin/busybox "$ROOT/linuxrc"

cat > "$ROOT/etc/inittab" << 'EOF'
# Nokia E7-00 eMMC root (task 098) — busybox init
::sysinit:/etc/init.d/rcS
::respawn:-/bin/sh
::ctrlaltdel:/sbin/reboot
::shutdown:/etc/init.d/rcK
EOF

cat > "$ROOT/etc/init.d/rcS" << 'RCS'
#!/bin/sh
mount -t proc proc /proc
mount -t sysfs sysfs /sys
grep -q ' /dev devtmpfs' /proc/mounts || mount -t devtmpfs devtmpfs /dev
[ -c /dev/null ] || mknod /dev/null c 1 3
hostname -F /etc/hostname

echo
echo "============================================"
echo "  Nokia E7-00 — Linux on BCM2727B1 (RAPU)"
echo "  kernel $(uname -r) @ $(uname -m)"
echo "  root: /dev/mmcblk0 (Rapu_Memcard eMMC, ext4)"
echo "  RAM: $(awk '/MemTotal/{print $2 " " $3}' /proc/meminfo)"
echo "============================================"
echo "-- ROOT-UP: running from the eMMC ext4"
echo "-- block devices:"
cat /proc/partitions
echo "-- mounts:"
cat /proc/mounts
RCS
chmod 755 "$ROOT/etc/init.d/rcS"

cat > "$ROOT/etc/init.d/rcK" << 'RCK'
#!/bin/sh
sync
umount -a -r 2>/dev/null
RCK
chmod 755 "$ROOT/etc/init.d/rcK"

echo "Nokia E7-00 Linux — eMMC root" > "$ROOT/etc/hostname"
cat > "$ROOT/etc/fstab" << 'EOF'
# device      mount  type     options  dump pass
/dev/mmcblk0  /      ext4     rw       0    1
EOF
cat > "$ROOT/etc/passwd" << 'EOF'
root:x:0:0:root:/root:/bin/sh
EOF
cat > "$ROOT/etc/group" << 'EOF'
root:x:0:
EOF
cat > "$ROOT/etc/profile" << 'EOF'
export PS1='e7:\w# '
EOF
cat > "$ROOT/README" << 'EOF'
Nokia E7-00 eMMC root filesystem (task 098, 2026-08-22).
Boots with root=/dev/mmcblk0 rootwait through the NLoader chain.
The initramfs is now the rescue fallback, not the primary root.
EOF

# optional kernel modules staged from a build (e.g. the task-099 gpio
# irq test module); dropped into /root for insmod
if [ -d modules ] && ls modules/*.ko >/dev/null 2>&1; then
    mkdir -p "$ROOT/root"
    cp modules/*.ko "$ROOT/root/"
    echo "staged modules: $(ls modules/*.ko | xargs -n1 basename | tr '\n' ' ')"
fi

echo "Creating $IMG ($SIZE, no partition table)..."
mke2fs -q -F -t ext4 -d "$ROOT" "$IMG" "$SIZE"

echo "Done: $IMG  ($(du -h "$IMG" | cut -f1), $(find "$ROOT" | wc -l) staged entries)"
