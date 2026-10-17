# BCM2727B1 Linux bring-up — pinctrl/DTS state and open items

**Date:** 2026-08-22 (updated: QEMU NLoader chain boot landed, §5)
**Scope:** what exists in `kernel/linux` (6.12) for the real SoC, what is
decoded vs assumed, and the ranked list of what Linux still needs before it
can boot on hardware.
**Inputs:** `docs/gpio-static-pinmux.md` (register map, CONFIRMED),
`docs/genio-padtable.md` (factory pad table + correlation),
`docs/phone-probes/hal-dump-2026-08-21.txt`, `docs/nloader-boot-mechanism.md`.

---

## 1. What landed (all builds clean, `zImage` + `bcm2727-nokia-e7.dtb`)

| piece | path |
|---|---|
| pinctrl+GPIO driver | `drivers/pinctrl/bcm/pinctrl-bcm2727.c` (+ Kconfig/Makefile) |
| machine glue | `arch/arm/mach-bcm/bcm2727.c` (`DT_MACHINE_START`, ARCH_BCM2727) |
| SoC dtsi + board dts | `arch/arm/boot/dts/broadcom/bcm2727.dtsi`, `bcm2727-nokia-e7.dts` |
| DT binding | `Documentation/devicetree/bindings/pinctrl/brcm,bcm2727-pinctrl.yaml` |
| defconfig | `arch/arm/configs/bcm2727_nokia_e7_defconfig` (mirror in `kernel/configs/`) — ARMv6K, single platform |

Build:
```sh
cd kernel/linux
make ARCH=arm CROSS_COMPILE=arm-linux-gnueabihf- bcm2727_nokia_e7_defconfig
make ARCH=arm CROSS_COMPILE=arm-linux-gnueabihf- -j$(nproc) zImage dtbs
```

The old OMAP-era `omap3630-nokia-e7.dts` / `nokia_e7_defconfig` remain for
the legacy OMAP-flavored QEMU machine only — the real SoC is BCM2727B1
(ARM1176JZF-S), and new work goes here.

CPU config note (verified in the chain boot): `CPU_V6K=y` alone is enough
for ARM1176 — `__v6_proc_info` (value 0x0007B000, mask 0x0007F000, built
by CPU_V6K per arch/arm/mm/Makefile) matches MIDR 0x410FB767, same as
bcm2835. No `CPU_ARM1176` or `CPU_V6` needed. If head.S ever spins in
`__error_p`, check what MIDR the emulated CPU actually reports before
touching configs (an old QEMU MIDR override cost us exactly that).

## 2. Driver scope (pinctrl-bcm2727)

- **pinmux**: 176 pads, 4-bit fields in the 22-register grid at
  +0x1810; functions `gpio`(0) + `alt1..alt4` (names pending §4);
  RMW under raw spinlock; `strict` mode.
- **gpio**: banks 0–4 = 160 lines. get (DATIN +0x19f0+0x10N), set/clr
  (+4/+8), direction via IRQ-block +0x3400+0x40N (+0x10/+0x14 with the
  +0x04 gate write), mirroring the CONFIRMED Symbian sequences.
- **intentionally not implemented**: pinconf (pull/drive registers not
  identified; candidates are the two boolean regs at +0x1970/+0x1980),
  EDGE pair semantics (write-only, deferred to runtime per
  gpio-static-pinmux §8.2), and the gpio irqchip — the root ICU and the
  timer ARE implemented (2026-08-22, docs/bcm2727-icu-timer.md), but the
  GPIO-mux → ICU wiring is still unknown.

## 3. DTS content and honesty rules

In: CPUs, 256 MB SDRAM @0x80000000, PDRAM SRAM @0x10000000, GENIO node
@0x20200000. **Deliberately absent**: interrupt controller, timers, UART,
I2C/SPI/eMMC/USB/display — no register base for any of them is confirmed
from binaries, and a fake node is worse than none (user doctrine: old
notes are smoke until verified).

## 4. Open items, ranked (blockers for a hardware boot)

1. ~~**Interrupt controller.**~~ **DECODED 2026-08-22** —
   `docs/bcm2727-icu-timer.md`: ICU at phys 0x20400000, 421 flat ids,
   select/priority mailbox + pending-id + EOI-by-id; Linux irqchip
   `irq-bcm2727-icu.c`, QEMU model in the chain — **validated by the
   chain boot**. Remaining: the GPIO-mux → ICU wiring (§5 of that doc)
   before a hardware gpio irqchip.
2. ~~**Timer / clocksource.**~~ **DECODED 2026-08-22** — same doc: tick
   timer in the system block +0x3600-0x37FF, reload pair +0x37A4/+0x37A8,
   IRQ id 0xF2, 200 MHz-class input clock; Linux `timer-bcm2727.c` +
   QEMU model — **chain boot now self-calibrates (`lpj=` dropped)**.
   TODO-FOR-HARDWARE: +0x44 read semantics, true clock tree.
3. ~~**UART console.**~~ **RESOLVED 2026-08-22 (task 095) + SHIPPED
   2026-08-23 (task 100)** — there is no APE UART in the boot chain
   (`docs/bcm2727-xti-trace.md`); the hardware console is now the
   **genio42 bit-bang soft-UART**: `drivers/tty/serial/bcm2727_softuart.c`
   (`/dev/ttySU0`, `console=ttySU0`, `earlycon=bcm2727su`), validated
   byte-exact in the chain against the QEMU wire tap
   (`docs/bcm2727-softuart.md`).
4. **Function-code names** (genio-padtable §5): first FSEL dump on
   bare metal settles codes 1–4; also resolves the 46/47 and 52/49
   anomalies and the pins 58–62 EDGE-vs-code-4 question.
5. **Memory map gaps**: where does the ROM sit for Linux (Symbian XIP ROM
   at 0x80000000 VA is not necessarily a PA), is there a second SDRAM
   window (HIGHMEM config ready), and what owns 0x20300000/0x20400000
   (partially answered by the kernel early-mapping table decoded in
   bcm2727-icu-timer.md §4).

## 5. QEMU validation path (no hardware risk)

**Option A is DONE (2026-08-22, task 093):** the board file boots Linux
through NLoader (custom image + `MCUSW` TOC entry + auth/LDSP/PMM/GENIO
bypasses + `bx r1` at 0x10033b58 — see docs/nloader-boot-mechanism.md §6).
The task-093/097-era command below is the **initramfs boot** (now the
rescue path — see the task-098 update under it for the primary
`root=` boot):

```sh
cd emulation/qemu/build   # after the build-env repair in task 093
./qemu-system-arm -M nokia-e7 -m 256M -nographic \
  -drive file=../../../firmware-re/analysis/nloader.bin,if=none,id=nloader,format=raw \
  -drive file=../../../firmware-re/nokia-e7-onenand.img,if=none,id=onenand,format=raw \
  -drive file=../../../emulation/rootfs/emmc.img,if=none,id=emmc,format=raw \
  -kernel ../../../kernel/linux/arch/arm/boot/zImage \
  -dtb ../../../kernel/linux/arch/arm/boot/dts/broadcom/bcm2727-nokia-e7.dtb \
  -initrd ../../../emulation/rootfs/initramfs.cpio.gz \
  -append "earlycon=pl011,0x40210000 console=ttyAMA0 loglevel=5"
```

**Task 097 update (2026-08-22): this command now lands in an
interactive busybox shell** (ttyAMA0, input works — the console PL011's
RX irq is wired into the ICU at id 0x1a3), with the eMMC image mounted
at /mnt/emmc by /init (`mmc-bcm2727` PIO host + `bcm2727.memcard`
model; docs/bcm2727-memcard.md §6). The old relative paths in this
block were one directory short — fixed above.

**Task 098 update (2026-08-22): the eMMC ext4 is now the REAL ROOT.**
Primary boot — drop `-initrd`, add `root=/dev/mmcblk0 rootwait rw`
(no initramfs; the kernel mounts the eMMC itself and runs
`/sbin/init` = busybox init from the ext4, inittab + rcS banner +
interactive shell):

```sh
cd emulation/qemu/build
./qemu-system-arm -M nokia-e7 -m 256M -nographic \
  -drive file=../../../firmware-re/analysis/nloader.bin,if=none,id=nloader,format=raw \
  -drive file=../../../firmware-re/nokia-e7-onenand.img,if=none,id=onenand,format=raw \
  -drive file=../../../emulation/rootfs/emmc.img,if=none,id=emmc,format=raw \
  -kernel ../../../kernel/linux/arch/arm/boot/zImage \
  -dtb ../../../kernel/linux/arch/arm/boot/dts/broadcom/bcm2727-nokia-e7.dtb \
  -append "earlycon=pl011,0x40210000 console=ttyAMA0 loglevel=7 root=/dev/mmcblk0 rootwait rw"
```

Boot evidence: `Waiting for root device /dev/mmcblk0...` → `mmcblk0:
mmc0:0001 QEMU!! 64.0 MiB` → `VFS: Mounted root (ext4 filesystem) on
device 179:0` → `Run /sbin/init as init process` → rcS prints
`ROOT-UP: running from the eMMC ext4` → `e7:~#` prompt. Rootfs image:
`emulation/rootfs/build-emmc-root.sh` (stages `emmc-root/`, same armel
busybox, mke2fs -d into `emmc.img`, 64MB, no partition table). The
kernel auto-mounts devtmpfs on the ext4's own `/dev` directory
(CONFIG_DEVTMPFS_MOUNT applies on the no-initramfs path — the image
ships an empty `/dev` dir for it).

**Rescue fallback (task 098):** add `-initrd
../../../emulation/rootfs/initramfs.cpio.gz` back to the command above
and drop nothing else — `/init` takes over (an initramfs /init
preempts `root=`), mounts the eMMC at /mnt/emmc when it can, and
drops into a shell for repairs. Validated both ways: a marker written
under the primary boot was read back from the rescue boot + host-side
via debugfs, and a rescue-boot write persisted the same way.

**Task 103 update (2026-08-23): the root is now a buildroot image,
same boot command.** `emmc.img` is built by
`emulation/rootfs/build-buildroot-root.sh` (buildroot 2026.02.3 +
the musl.cc `armv6-linux-musleabi` cross as external toolchain):
busybox 1.37, strace, i2c-tools, musl dynamic runtime
(`/lib/ld-musl-arm.so.1`) and the `e7-hello` smoke binaries — every
shipped binary gated ARMv6 by readelf. Boot evidence identical
through `Run /sbin/init as init process`, rcS banner now says
`ROOT-UP: buildroot armv6/musl on the eMMC ext4 (task 103)`; static
AND dynamically-linked binaries proven on target. The task-098
armel-busybox root is still rebuildable (build-emmc-root.sh; a
one-time copy is kept at `emmc.busybox-root.bak`), the initramfs
rescue switch is unchanged, and the compile-anything recipe +
toolchain gotchas live in `tasks/103-O-armv6-toolchain.md`.

**Task 101/102 update (2026-08-23): the SD channel rides along, and
the mmc driver is IRQ-driven.** Add the SD card drive to the command
above:

```sh
  -drive file=../../../emulation/rootfs/sd.img,if=none,id=sd,format=raw
```

→ `mmc1: new SD card at address a499` → `mmcblk1: mmc1:a499 QEMU!
64.0 MiB` (`/dev/mmcblk1`, mountable ext4; image from
`emulation/rootfs/build-sd-card.sh`, power-of-two sized). Both
channels now log "IRQ-driven" and `/proc/interrupts` counts every
command on `bcm2727-icu` ids 35/36 (eMMC 454 events through the root
mount at boot, SD 27, Err: 0). The mmc1 node in bcm2727.dtsi flipped
to `status = "okay"` for this.

Validated by a running kernel: machine model, 256MB@0x80000000,
chosen/bootargs merge, `bcm2727-pinctrl 20200000.pinctrl: 176 pads, 160
gpios (banks 0-4)` probe, boot to a clean VFS panic. The kernel console
is a QEMU-only PL011 at 0x40210000 (unmapped L4 space) — on hardware the
console story is the XTI mailbox (task 095).

**2026-08-22 (task 094): the ICU + OS timer are modeled in the chain
(docs/bcm2727-icu-timer.md) and `lpj=` is GONE from the command line** —
the kernel self-calibrates (`Calibrating delay loop... 1506.50 BogoMIPS`)
on a real timer→ICU→CPU interrupt flow at 100 Hz, and timekeeping runs on
the `bcm2727-frc` clocksource.

**2026-08-22 (task 095): the XTI trace mailbox + doorbell are modeled
(docs/bcm2727-xti-trace.md)** — NLoader's real formatted log prints in
the chain (printf stub removed, PL011 moved to 0x40210000), and blocker
#3 is resolved by decode: no APE UART exists; hardware console plan =
XTI jig or genio42 bitbang.

- **B: keep direct `-kernel` boot** on the legacy machine for driver
  regression, real-SoC work continues offline.

**2026-08-23 (task 100): the genio42 soft-UART console is the
hardware console** — `docs/bcm2727-softuart.md`.  Validation boot
(kernel console decoded off the pin by the board model's wire tap;
add `-icount shift=auto` for deterministic timing):

```sh
cd emulation/qemu/build
./qemu-system-arm -M nokia-e7 -m 256M -nographic -icount shift=auto \
  -drive file=../../../firmware-re/analysis/nloader.bin,if=none,id=nloader,format=raw \
  -drive file=../../../firmware-re/nokia-e7-onenand.img,if=none,id=onenand,format=raw \
  -drive file=../../../emulation/rootfs/emmc.img,if=none,id=emmc,format=raw \
  -kernel ../../../kernel/linux/arch/arm/boot/zImage \
  -dtb ../../../kernel/linux/arch/arm/boot/dts/broadcom/bcm2727-nokia-e7.dtb \
  -append "earlycon=bcm2727su console=ttySU0 loglevel=5 root=/dev/mmcblk0 rootwait rw"
```

Output arrives as `su42: <line>` on stdout (byte-exact kernel log).
Interactive ttySU0 testing uses the ttyAMA0 boot above plus
`stty -F /dev/ttySU0 115200` (n_tty defaults to B9600!) and
`echo … > /dev/ttySU0`.

Next milestones on this path (see HANDOFF queue): SD channel ch1,
IRQ-driven mmc, and the phone-bound storage-pin bring-up (memcard doc
§6.4).
