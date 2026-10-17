#!/bin/sh
# Apply the BCM2727 (RAPU) platform files onto kernel/linux (6.12).
# Run from the repo root:  sh kernel/patches/bcm2727/apply.sh
# Idempotent: safe to re-run; overwrites project-owned files.
set -e
L=kernel/linux
P=kernel/patches/bcm2727

cp "$P/pinctrl/pinctrl-bcm2727.c"          "$L/drivers/pinctrl/bcm/"
cp "$P/mach/bcm2727.c"                     "$L/arch/arm/mach-bcm/"
cp "$P/dts/bcm2727.dtsi"                   "$L/arch/arm/boot/dts/broadcom/"
cp "$P/dts/bcm2727-nokia-e7.dts"           "$L/arch/arm/boot/dts/broadcom/"
cp "$P/bindings/brcm,bcm2727-pinctrl.yaml" "$L/Documentation/devicetree/bindings/pinctrl/"
cp ../../kernel/configs/bcm2727_nokia_e7_defconfig "$L/arch/arm/configs/" 2>/dev/null || \
cp kernel/configs/bcm2727_nokia_e7_defconfig      "$L/arch/arm/configs/"

# Kconfig/Makefile wiring (obj-/config- lines) as a patch; skip if already applied
cd "$L"
if ! grep -q PINCTRL_BCM2727 drivers/pinctrl/bcm/Kconfig; then
    patch -p1 --forward < "../../$P/001-wiring.patch" ||
    patch -p1 --forward < "../$P/001-wiring.patch"
fi

echo "done. build with:"
echo "  make ARCH=arm CROSS_COMPILE=arm-linux-gnueabihf- bcm2727_nokia_e7_defconfig"
echo "  make ARCH=arm CROSS_COMPILE=arm-linux-gnueabihf- -j\$(nproc) zImage dtbs"
