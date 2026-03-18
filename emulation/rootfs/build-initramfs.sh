#!/bin/bash
# Build minimal BusyBox initramfs for Nokia E7 QEMU
# Usage: ./build-initramfs.sh
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
BUSYBOX_VER=1.36.1
CROSS=arm-linux-gnueabihf-

cd "$SCRIPT_DIR"

# Download BusyBox if needed
if [ ! -d "busybox-${BUSYBOX_VER}" ]; then
    echo "Downloading BusyBox ${BUSYBOX_VER}..."
    wget -q "https://busybox.net/downloads/busybox-${BUSYBOX_VER}.tar.bz2"
    tar xf "busybox-${BUSYBOX_VER}.tar.bz2"
fi

# Build static BusyBox
echo "Building BusyBox (static, ARM)..."
cd "busybox-${BUSYBOX_VER}"
make ARCH=arm CROSS_COMPILE="$CROSS" defconfig >/dev/null 2>&1
sed -i 's/# CONFIG_STATIC is not set/CONFIG_STATIC=y/' .config
sed -i 's/CONFIG_TC=y/# CONFIG_TC is not set/' .config
make ARCH=arm CROSS_COMPILE="$CROSS" -j"$(nproc)" >/dev/null 2>&1
make ARCH=arm CROSS_COMPILE="$CROSS" install >/dev/null 2>&1
cd "$SCRIPT_DIR"

# Create initramfs structure
echo "Creating initramfs..."
rm -rf initramfs
mkdir -p initramfs/{dev,proc,sys,tmp,run,etc,var/log,root}
cp -a "busybox-${BUSYBOX_VER}/_install/"* initramfs/

# Init script
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
echo "  Nokia E7 Linux (QEMU OMAP3630)"
echo "  Kernel: $(uname -r) @ $(uname -m)"
echo "  RAM: $(awk '/MemTotal/{print $2 " " $3}' /proc/meminfo)"
echo "============================================"
echo

exec /bin/sh
INIT
chmod +x initramfs/init

# Pack cpio
echo "Packing initramfs.cpio.gz..."
cd initramfs
find . | cpio -o -H newc 2>/dev/null | gzip > ../initramfs.cpio.gz
cd "$SCRIPT_DIR"

SIZE=$(du -h initramfs.cpio.gz | cut -f1)
echo "Done: initramfs.cpio.gz ($SIZE)"
echo
echo "Boot with:"
echo "  qemu-system-arm -M nokia-e7 -m 256M \\"
echo "    -kernel zImage -dtb omap3630-nokia-e7.dtb \\"
echo "    -initrd initramfs.cpio.gz \\"
echo "    -append \"earlycon clk_ignore_unused rdinit=/init\" \\"
echo "    -nographic"
