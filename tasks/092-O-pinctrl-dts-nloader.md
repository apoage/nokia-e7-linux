# Task 092: Linux Pinctrl/DTS Skeleton + NLoader Boot Mechanism Decode

## Status
**Offline work COMPLETE (2026-08-22).** Kernel builds clean with the new
BCM2727 platform; NLoader's image-load/jump chain fully decoded.

## Part A — BCM2727 Linux platform (pinctrl + DTS) — LANDED

Everything builds (`zImage` + `bcm2727-nokia-e7.dtb`, driver symbols in
System.map, zero warnings):

- `kernel/linux/drivers/pinctrl/bcm/pinctrl-bcm2727.c` — pinmux (176 pads,
  22-reg nibble grid @+0x1810, codes 0-4 = gpio + alt1-4) + GPIO banks 0-4
  (160 lines, DATIN/SET/CLR trios, Symbian-mirroring direction sequences).
  irqchip/pinconf deliberately deferred (semantics unknown — see doc).
- `kernel/linux/arch/arm/mach-bcm/bcm2727.c` + `ARCH_BCM2727` (Kconfig,
  Makefile; sits inside `if ARCH_BCM` like bcm2835 — needs
  `CONFIG_ARCH_BCM=y` in defconfig, cost one debug cycle).
- `kernel/linux/arch/arm/boot/dts/broadcom/bcm2727.dtsi` +
  `bcm2727-nokia-e7.dts` (ARM1176JZF-S, 256MB @0x80000000, PDRAM SRAM,
  GENIO @0x20200000; no invented peripherals — no UART/ICU/timer yet).
- DT binding YAML + `bcm2727_nokia_e7_defconfig` (ARMv6K single-platform).
- Gotchas found: `pinmux_ops.dbg_show` doesn't exist in 6.12 (it's
  `pinctrl_ops.pin_dbg_show`); `pinctrl_gpio_request` needs
  `linux/pinctrl/consumer.h`.

Design + ranked open items (irq decode → timer → UART → code names):
**docs/bcm2727-pinctrl-linux.md**.

## Part B — pad-grid correlation (genio-padtable §5, second pass)

**Grid pad N = kernel global N = schematic GPIO N: strongly indicated.**
All 7 known pure-input GPIOs at code 0; 3 GPIO-driven outputs at 0;
USB OTG group (EDGE-driven → must be GPIO) all-0; PM group 57-62 solid 4;
I2C pair 28/29 = 1,1; touch pair 122/123 = 2,2. Open anomalies: 46/47
split, 52 vs 49, and MMC EDGE-writes on code-4 pins 58-62. Decisive data =
first bare-metal FSEL dump.

## Part C — NLoader boot mechanism (docs/nloader-boot-mechanism.md)

Decoded from nloader.bin (capstone, Thumb + pool resolution):

1. **SDRAM TOC @0x101ff000**: `[+0]nand [+4]rsv [+8]size [+0xC]compressed
   [+0x10]LOAD [+0x14]name[12]`; name[3] ∈ {*,/} = compressed path.
2. **Copy images (0x11fee)**: copy failure is NON-FATAL; rewrites
   `[+0] = load − 0x101ff000` (the "0xefe01000 constant" = −TOC base).
3. **TOC search (0x1277c) mode 2** = `[+0] + TOC base` → after rewrite
   returns the LOAD ADDRESS of any named image.
4. **Handoff ("cleanup" 0x12450)**: find `MCUSW` (or `UPDAPP` in update
   mode) mode-2 → auth loaded image in place (0x105b8, secure call) →
   `entry = load + *(u32*)(load+0x148)` → "Start SOS at %08X" → jump via
   secure trampoline **0x10033b54(r0=config@0x30003e00, r1=entry)**.
5. **Custom-image recipe (QEMU-ready)**: image preloaded in RAM + `MCUSW`
   TOC entry with [+0x10] set + auth bypass @0x10032468 + patch 0x10033b54
   to `bx r1`. Trampoline sets Linux regs (r0=0, r1=~0, r2=DTB) and jumps
   to an appended zImage; `*(image+0x148)=0` makes NLoader enter at image
   start. HW walls: BB5 flash TOC write-protection + image auth
   (unchanged).

## Next (task 093 candidates)

1. QEMU: extend nokia_e7 board for the NLoader→Linux chain (§C.5) —
   first end-to-end DTB validation.
2. Static decode of the interrupt controller (ASSP int driver, ROM sites
   in gpio-static-pinmux §3.1) — the #1 hardware-boot blocker.
3. Timer decode (XTI/32K leads in bcm2727-pinctrl-linux.md §4.2).
