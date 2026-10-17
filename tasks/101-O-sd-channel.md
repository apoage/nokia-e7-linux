# Task 101: SD channel ch1 — second card in the chain

## Status
**DONE (2026-08-23).** Handoff desk-queue item #1: `mmc@20202900`
flipped to okay + `-drive id=sd` card image, chain-validated with a
full host↔guest data round trip through ch1. The eMMC root boot is
untouched.

## What landed

1. **DTS** (`bcm2727.dtsi`): `mmc1: mmc@20202900` → `status = "okay"`
   (node + ICU 36 interrupt existed since task 097).
2. **SD image** (`emulation/rootfs/build-sd-card.sh`): 64 MiB ext4
   data card with a marker file, same `mke2fs -d` recipe as the eMMC
   root image. NOT a root — the eMMC stays root. Power-of-two size is
   mandatory: QEMU's sd.c truncates CSD capacity to a power of two,
   and the guest would see a device smaller than the filesystem.
3. **QEMU model fix** (`nokia_e7.c`) — "model side is done" turned out
   to be eMMC-only. The SD attach path speaks app commands the eMMC
   path never issues, and the model's data-phase command list didn't
   know them:
   - ACMD51 SEND_SCR (8-byte read) — first failure: no data phase
     armed → FIFO-ready poll times out → `mmc1: error -110`.
   - ACMD13 SD_STATUS (64-byte read) — worse: undrained, it leaves
     the sd.c card stuck in Sending-Data state; the NEXT command
     (CMD6 switch) comes back rlen=0 — this QEMU maps `sd_illegal`
     to response length 0 — the model flags ERROR → `mmc1: error -84`.
   - Fix: app-command pairing in the model (CMD55 → next = ACMD,
     tracked like sd.c does it); ACMD13/ACMD51 arm their data phases,
     ACMD6 (SET_BUS_WIDTH) arms none (treating it like plain CMD6
     SWITCH arms a phantom phase). Plain CMD13/CMD6 on eMMC untouched.

## Validation (chain boot, 2026-08-23)

- `mmc1: new SD card at address a499` → `mmcblk1: mmc1:a499 QEMU!
  64.0 MiB` in `/proc/partitions`, eMMC root mounted from mmcblk0 in
  the same boot.
- Guest: `mount -t ext4 /dev/mmcblk1 /mnt` → `cat
  /mnt/task101-sd-marker` returns the host-staged text; `echo ... >
  /mnt/task101-sd-write; sync` → host-side `debugfs -R "cat
  /task101-sd-write" sd.img` reads it back. Round trip proven both
  directions.
- Boot log note: `host does not support reading read-only switch,
  assuming write-enable` — no WP/CD pins decoded (get_cd hardwired 1);
  hardware bring-up item, doc §6.4.

## Next

Queue item #2 (IRQ-driven mmc), then ARMv6 toolchain.
