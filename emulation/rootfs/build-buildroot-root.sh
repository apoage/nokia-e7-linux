#!/bin/bash
# Build the buildroot ARMv6 rootfs (task 103): full musl userland for
# the phone, compiled by the musl.cc armv6-linux-musleabi cross as a
# buildroot EXTERNAL toolchain.  Replaces emmc.img (the task-098
# busybox root; that image stays rebuildable via build-emmc-root.sh
# and build-initramfs.sh is still the rescue fallback).
#
# Toolchain + buildroot trees live in toolchain/ (gitignored, ~450MB
# extracted).  Everything tracked that matters lives here: the
# defconfig (buildroot-e7_defconfig), the overlay (buildroot-overlay/)
# and e7-hello.c, compiled into the image in static+dynamic flavors.
#
# Usage: ./build-buildroot-root.sh          (full build, idempotent)
#        ./build-buildroot-root.sh shell    (drop to a shell in the
#                                            build dir for make menuconfig)
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
TOP="$(cd "$SCRIPT_DIR/../.." && pwd)"
TOOLDIR="$TOP/toolchain"
TC="$TOOLDIR/musl.cc/armv6-linux-musleabi-cross"
BR_VER=2026.02.3
BR="$TOOLDIR/buildroot"
OUT="$TOOLDIR/buildroot-o7"
OVERLAY="$SCRIPT_DIR/buildroot-overlay"

mkdir -p "$TOOLDIR/musl.cc" "$TOOLDIR/dl"

# Host quirks (no sudo needed):
# - this system's /usr/bin/install is uutils 0.8.0, which buildroot
#   rejects (uutils/coreutils#12166); /usr/bin/gnuinstall is the real
#   one — shadow it through a PATH shim dir.
# - the environment may force CUDA's gcc as HOSTCC; pin the system one.
mkdir -p "$TOOLDIR/hostbin"
ln -sf /usr/bin/gnuinstall "$TOOLDIR/hostbin/install"
export PATH="$TOOLDIR/hostbin:$PATH"
HOSTCC_BIN="$(command -v gcc)"

# --- 1. musl.cc armv6 cross -------------------------------------------------
if [ ! -x "$TC/bin/armv6-linux-musleabi-gcc" ]; then
    echo "Fetching musl.cc armv6 cross..."
    curl -sL --retry 3 -o "$TOOLDIR/musl.cc/armv6-linux-musleabi-cross.tgz" \
        https://musl.cc/armv6-linux-musleabi-cross.tgz
    tar -C "$TOOLDIR/musl.cc" -xf "$TOOLDIR/musl.cc/armv6-linux-musleabi-cross.tgz"
fi
XC="$TC/bin/armv6-linux-musleabi-gcc"

# musl.cc sysroots are FLAT (include/, lib/ at the root); buildroot's
# kernel-headers sanity check hardcodes <sysroot>/usr/include.  The
# standard workaround: usr -> . symlink (also lands headers in the
# staging dir's usr/include during the sysroot copy).
SYSROOT="$($XC -print-sysroot)"
[ -e "$SYSROOT/usr" ] || ln -s . "$SYSROOT/usr"

# sanity: the toolchain must produce ARMv6 when asked (and default to
# something ARM1176-safe even when not asked)
[ "$($XC -dumpmachine)" = "armv6-linux-musleabi" ] || {
    echo "ERROR: $XC is not armv6-linux-musleabi" >&2; exit 1; }

# --- 2. buildroot tree ------------------------------------------------------
# buildroot.org is not reachable from this network; the GitHub mirror
# carries the same release tarballs.
if [ ! -d "$BR" ]; then
    echo "Fetching buildroot $BR_VER (GitHub mirror)..."
    curl -sL --retry 3 -o "$TOOLDIR/buildroot-$BR_VER.tar.gz" \
        "https://github.com/buildroot/buildroot/archive/refs/tags/$BR_VER.tar.gz"
    tar -C "$TOOLDIR" -xf "$TOOLDIR/buildroot-$BR_VER.tar.gz"
    mv "$TOOLDIR/buildroot-$BR_VER" "$BR"
fi

# --- 3. toolchain smoke test binaries into the overlay ----------------------
echo "Compiling e7-hello (static + dynamic) with the cross..."
mkdir -p "$OVERLAY/usr/bin"
V6FLAGS="-march=armv6 -mtune=arm1176jzf-s"
$XC -static -Os $V6FLAGS -o "$OVERLAY/usr/bin/e7-hello" "$SCRIPT_DIR/e7-hello.c" -lm
$XC -Os $V6FLAGS -o "$OVERLAY/usr/bin/e7-hello-dyn" "$SCRIPT_DIR/e7-hello.c" -lm
$TC/bin/armv6-linux-musleabi-strip "$OVERLAY/usr/bin/e7-hello" "$OVERLAY/usr/bin/e7-hello-dyn"

# --- 4. optional kernel modules (same staging rule as build-emmc-root.sh) ---
rm -rf "$OVERLAY/root"
if [ -d "$SCRIPT_DIR/modules" ] && ls "$SCRIPT_DIR"/modules/*.ko >/dev/null 2>&1; then
    mkdir -p "$OVERLAY/root"
    cp "$SCRIPT_DIR"/modules/*.ko "$OVERLAY/root/"
    echo "staged modules: $(ls "$SCRIPT_DIR"/modules/*.ko | xargs -n1 basename | tr '\n' ' ')"
fi

# --- 5. configure + build ---------------------------------------------------
echo "Configuring buildroot ($OUT)..."
make -C "$BR" O="$OUT" BR2_DEFCONFIG="$SCRIPT_DIR/buildroot-e7_defconfig" defconfig
# host-absolute paths are sed'ed into the generated .config (Kconfig
# command-line assignment does not persist for these), keeping the
# tracked defconfig portable
sed -i \
    -e "s|^BR2_TOOLCHAIN_EXTERNAL_PATH=.*|BR2_TOOLCHAIN_EXTERNAL_PATH=\"$TC\"|" \
    -e "s|^BR2_ROOTFS_OVERLAY=.*|BR2_ROOTFS_OVERLAY=\"$OVERLAY\"|" \
    -e "s|^BR2_DL_DIR=.*|BR2_DL_DIR=\"$TOOLDIR/dl\"|" \
    "$OUT/.config"
make -C "$BR" O="$OUT" olddefconfig

if [ "${1:-}" = "shell" ]; then
    echo "Dropping to a shell in $OUT (make menuconfig / make); exit to finish."
    cd "$OUT" && exec bash
fi

echo "Building (this also builds host-e2fsprogs etc. on first run)..."
make -C "$BR" O="$OUT" HOSTCC="$HOSTCC_BIN" HOSTCXX="$(command -v g++)" \
    -j"$(nproc)"

# --- 6. verify the image is genuinely ARMv6 ---------------------------------
# -mcpu=arm1176jzf-s shows up as Tag_CPU_arch "v6KZ"; accept the v6 family.
RE="$TC/bin/armv6-linux-musleabi-readelf"
for b in "$OUT/target/bin/busybox" "$OUT/target/usr/bin/strace" \
         "$OUT/target/usr/sbin/i2cdetect" \
         "$OVERLAY/usr/bin/e7-hello" "$OVERLAY/usr/bin/e7-hello-dyn"; do
    [ -e "$b" ] || { echo "ERROR: $b missing from the image" >&2; exit 1; }
    arch=$($RE -A "$b" | sed -n 's/.*Tag_CPU_arch: //p' | head -1)
    echo "arch: $(basename "$b") -> $arch"
    case "$arch" in
        v6|v6K|v6KZ|v6T2) ;;
        *) { echo "ERROR: $b is not ARMv6 ($arch)" >&2; exit 1; } ;;
    esac
done

# --- 7. install as the eMMC image -------------------------------------------
if [ -f "$SCRIPT_DIR/emmc.img" ] && [ ! -f "$SCRIPT_DIR/emmc.busybox-root.bak" ]; then
    cp -a "$SCRIPT_DIR/emmc.img" "$SCRIPT_DIR/emmc.busybox-root.bak"
    echo "(previous task-098 busybox root kept as emmc.busybox-root.bak)"
fi
cp "$OUT/images/rootfs.ext4" "$SCRIPT_DIR/emmc.img"

echo "Done: emmc.img ($(du -h "$SCRIPT_DIR/emmc.img" | cut -f1), $(find "$OUT/target" | wc -l) staged entries)"
