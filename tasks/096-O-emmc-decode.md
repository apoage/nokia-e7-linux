# Task 096: eMMC/SD controller decode — Rapu_Memcard register map

## Status
**DECODE COMPLETE (2026-08-22); implementation not started.** The
HANDOFF lead #2 ("eMMC decode for a shell in the chain") phase 1 is
done: full register map, command encoding, IRQ ids, driver code anchors.
Full detail: **docs/bcm2727-memcard.md**.

## What was decoded

1. **Register file** (16-bit regs, 2 channels, 0x100 stride, base
   0xC6032800 = sys+0x2800): command +0x00 (write-triggered; 0x80 = soft
   reset, 0x10 = stop), ARG +0x04/+0x08, control/IRQ +0x0C, transfer
   status/ack +0x10 (0x800 flush / 0x400 ack), divisor +0x14, timeout
   +0x18/+0x1C, **FIFO +0x20**, block size−1 +0x24, count−1 +0x28, mode
   0x8080 +0x2C, control +0x34, **response regs +0x40..+0x5C (9×16-bit
   big-endian)**, power gate +0x64, reset strobe +0x70 (0x17/0x15),
   reset status +0x74.
2. **Command word** = cmd index | resp-type<<8 (0x100..0x900) |
   0x1000/0x2000/0x3000 DMA/mode | 0x4000 stream | 0x8000 no-DMA.
3. **IRQ ids: eMMC (ch0) = ICU 0x23 (35), SD (ch1) = 0x24 (36)** — from
   config table 0x80164BF4 + ctor bind calls.
4. Driver plumbing: "Rapu_MMCSD_Resource_Manager", DFCQ
   "MEMCARD_DFCQ", clocks EHwClkMemCard0/1, rails EVoltage_eMMC*,
   channel names MemoryCard_eMMC/_SD. RMW helper = kernel 0x800B7C1C
   via veneer 0x8015E8CC (set/clear masks, IRQs masked).
5. PIO flows (write/read) and the ISR skeleton (clock stop → status →
   stop-cmd → restore) reconstructed end-to-end.

## Key consequence

The block is **custom, not SDHCI** — Linux needs a small host driver
(`drivers/mmc/host/mmc-bcm2727.c`, PIO polling is enough for a rootfs;
the Symbian DMA descriptor is driver-RAM-resident and undecoded).
QEMU model maps cleanly onto `sd.c` (command → sd cmd engine, +0x20 →
FIFO bridge, +0x40..+0x5C ← response). Plan in the doc §6.

## Next (phase 2 — one session of implementation)

1. QEMU `bcm2727.memcard` model (0x20202800, ICU ids 0x23/0x24,
   eMMC image via -drive).
2. Linux mmc-bcm2727 host driver + DTS node + defconfig.
3. Tiny initramfs first (shell in the chain = VFS panic gone), ext2 on
   eMMC image second.
4. Close the open items: +0x0C bits, +0x10 read semantics (FIFO-ready
   candidate), resp_type enum → MMC response-class map.

## Validation status of this decode

Static-only so far: every register access was read out of the ROM
disassembly (no runtime confirm). The phone could confirm live
(XPEEK 0xC60328xx is supervisor-only — rc=3 — so runtime confirmation
needs the QEMU model, or hardware once the pinctrl/console path lands).
