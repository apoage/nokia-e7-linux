#!/bin/bash
# Build the SD card image for memcard channel 1 (task 101): a small ext4
# data card, NOT a root filesystem — the eMMC (build-emmc-root.sh) stays
# the root.  Boots with -drive file=sd.img,if=none,id=sd,format=raw.
#
# Size must be a power of two (64 MiB): QEMU's sd card truncates CSD
# capacity to a power of two otherwise and the guest sees a device
# smaller than the filesystem claims to be.
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
cd "$SCRIPT_DIR"

IMG=sd.img
SIZE=64m
STAGE=sd-stage

echo "Staging $STAGE/..."
rm -rf "$STAGE"
mkdir -p "$STAGE"
echo "task101: SD channel ch1 marker (2026-08-23)" > "$STAGE/task101-sd-marker"

echo "Creating $IMG ($SIZE, no partition table)..."
mke2fs -q -F -t ext4 -d "$STAGE" "$IMG" "$SIZE"

echo "Done: $IMG ($(du -h "$IMG" | cut -f1))"
