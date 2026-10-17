# Task 123: BCM2727 MUSBHDRC — QEMU model + Linux musb glue, live USB in the chain

## Status

**DONE + VALIDATED — executed 2026-09-05.**  All six exit criteria met;
as-built record: docs/bcm2727-usb-flashmode.md §12.  Evidence:
`emulation/qemu-e7sw/boot-usb-task123.log` (icount, usb-storage +
usb-kbd attached, interactive mount/md5//proc/interrupts at the
prompt); plain TCG boots the same flow to `e7:~#` + `Attached SCSI
disk`.

Results vs the exit criteria:

1. Chain boots `e7:~#` (TCG + icount); `musb-hdrc.1.auto: MUSB HDRC
   host driver`, `new USB bus ... number 1`, `musb-bcm2727 20208800.usb
   ... (host, PIO, IRQ 18)`; **zero deferrals**.
2. usb-storage round trip: QEMU MSD enumerates (full-speed, behind
   QEMU's auto-chained hub — see below), SCSI INQUIRY/CAPACITY over
   the bulk pipes, `sda: sda1`, `Attached SCSI disk`;
   `mount -t vfat /dev/sda1 /mnt` + `md5sum /mnt/task123.txt` =
   `d8b87d577ac6ebe82bb68431724bb65b` — byte-identical to the host
   file.
3. usb-kbd enumerates in the same boot (`input: QEMU QEMU USB
   Keyboard`, hid-generic).
4. Boot diff vs task-122: 1202-line NLoader flow identical; kernel
   side adds only USB/SCSI lines + build-string/memory artifacts.
5. /proc/interrupts: ICU **183 claimed by musb-hdrc.1.auto** (1181
   services at the prompt check); no 215 entry — idle by design.

Walls found and fixed (all model-side, per the task's "driver is the
more trustworthy side" rule; full stories in §12):

- `-device` could not find the USB bus — the model now rides its own
  sysbus carrier (TYPE_BCM2727_MUSB), because the SoC is an unattached
  plain DeviceState and bus search walks the main system bus.
- The device-side endpoint for a token comes from the low nibble of
  TXTYPE/RXTYPE, not the hw ep number (musb_schedule muxes non-control
  transfers to any free hw ep; the hub's status endpoint on
  device-ep1 landed on hw ep2 and STALLED when addressed as ep2).
- usb_port_reset's internal detach+attach must not raise
  CONNECT/DISCONNECT (hub_port_wait_reset reads mid-reset C_CONNECTION
  as a disconnect → "Cannot enable. Maybe the USB cable is bad?").
- Interrupt endpoints speak once per programmed RXINTERVAL — instant
  answers to every resubmitted request let the hub driver's
  hub_irq→resubmit loop spin a device-IRQ storm that starved the whole
  system (even init).

Topology note: QEMU auto-chains an 8-port usb-hub onto a single-port
bus (usb_claim_port), so every `-device` enumerates behind it at full
speed — that hub's status interrupt endpoint and class requests are
therefore part of the validated surface.

**IDLE-TASK PROMPT (paste into the queue):**
> Execute tasks/123-O-usb-musb-model-glue.md in /home/lukas/ps3/e7
> (read it and docs/bcm2727-usb-flashmode.md first — §2.1/§2.2 the
> base + full register layout vs musb_regs.h, §2.3 ULPI/PHY block,
> §2.4 HSDMA + the BCM channel order, §5 glue prerequisites, §10 the
> decoded interrupt shape: split ids 0xB7/183 core + 0xD7/215 HSDMA,
> ULPI PHY TI 0x0451:0x1508). One job, two sides, validated against
> each other in the chain. QEMU side: a sysbus MUSBHDRC host model at
> PA 0x20208800 (dual view with the 0x20038800 pre-MMU alias — the
> task-112 i2c precedent), core window +0x000-0x1FF, HSDMA block
> +0x200-0x2FF with the BCM2727 channel order CONTROL@+4/ADDR@+8/
> COUNT@+0xC (§2.4), ULPI viewport 0x74-0x76 answering reads as a
> TUSB1210-class PHY (vendor 0x0451 product 0x1508, §10.5) so PHY
> probes succeed, the 0x20038c00 PHY-control block as a benign stub,
> IRQs to the ICU as TWO lines (183 core: INTRUSB/INTRTX/INTRRX
> set on events, clear-on-read like the decoded ISR expects; 215
> HSDMA: raise only if DMA is ever modeled — start PIO-only, line
> registered but idle). The register map is §2.2 (mainline MUSBHDRC,
> byte-addressed on a 16-bit bus — RMW semantics on odd bytes).
> Linux side: a small musb glue platform driver under
> drivers/usb/musb/ (use jz4740.c/da8xx.c as templates — musb_core
> is already in the tree), DTS node musb@20208800 with
> interrupts <183> <215> per the house DT-int=ICU-id convention,
> PHY-less bring-up (TUSB1210 via ULPI is auto-probed; keep
> NOP_XCEIV fallback), defconfig USB=y USB_MUSB_HOST=y + the glue;
> mirror the defconfig in kernel/configs/. NO HSDMA in the driver
> (PIO mode — musb works without DMA; keep 215 in the DTS for
> documentation). Exit criteria: (1) the chain boots to `e7:~#` with
> the musb glue probing cleanly (no deferrals beyond PHY detect);
> (2) with `-device usb-storage,drive=sdfile` attached (FAT image via
> -drive), Linux enumerates it (dmesg usb-storage + scsi lines),
> /dev/sda mounts, a file reads back correctly; (3) a second device
> check: `-device usb-kbd` enumerates (input layer); (4) boot diff vs
> the task-122 flow = the new USB lines only; (5) evidence log
> committed (boot-usb-task123.log); (6) docs: usb-flashmode.md gains
> an as-built §12 (model contract: which registers are real, which
> stubs, the ULPI/PHY answers, IRQ shapes) and HANDOFF is NOT touched
> (main session folds). Stop per the file's clauses. NEVER write the
> OneNAND image or nloader.bin. Commit when done.

## Why

Task 117 closed the decode (ids, layout, PHY, HSDMA order); the glue
was parked only for lack of a validation target. The chain IS that
target: a QEMU model + the Linux driver validated against each other
gives USB in emulation — real enumeration, real storage — without
touching the phone. Bonus that falls out: with `dev-serial` available
in QEMU's USB stack, a future chain boot can carry a USB serial
console, previewing exactly the hardware endgame (console as a USB
gadget — the reason UART bring-up can stay shelved).

## Method

1. Model first: hw/arm/bcm2727.c (or a new source file — then
   build.ninja surgery per the task-104 gotcha; prefer an existing
   file if it fits cleanly). Register semantics per §2.2's table +
   §10.3's ISR expectations (the decoded Symbian ISR is the
   behavioral spec: INTRUSB 0x0A/INTRTX 0x02/INTRRX 0x04
   clear-on-read + enables, POWER bits, INDEXed per-EP windows,
   dynamic FIFO sizing TXFIFOSZ/RXFIFOSZ/TXFIFOADD/RXFIFOADD).
   Reuse QEMU's USB core (usb_bus_create, usb_find_device,
   usb_handle_packet — hcd-ohci-sysbus.c is the sysbus template;
   the tree has NO hcd-musb.c, so write from §2.2 + musb_regs.h,
   which agree). Start minimal: EP0 control + one bulk IN/OUT pair,
   then add per-EP windows as the driver demands.
2. Glue: DTS + defconfig + the small glue file; boot; iterate on
   probe failures with the model's trace (add an env-gated
   MUSB_DEBUG like ONENAND_DEBUG).
3. Validate per exit criteria; commit evidence.

## Exit criteria

1-6 per the prompt (probe clean, usb-storage round trip, usb-kbd,
boot diff, log, §12 as-built + CONTRACT legend for stubs).

## If it does NOT work

- Driver probes but no device answers: bisect with MUSB_DEBUG —
  expect the classic first walls (FIFO sizing semantics, EP0 data
  toggles, VBUS/session bits in POWER/DEVCTL); fix the MODEL per the
  musb_core expectations (the driver is the more trustworthy side —
  it is mainline).
- If usb-storage needs features too deep to finish in one run (e.g.
  interrupt EPs / scheduling): land what enumerates (even just EP0 +
  device descriptors in dmesg), document the remaining gap precisely
  with the trace, stop — partial USB is still a validation rail.
- IRQ storms/misrouting: check the ICU level semantics (the I2C/
  memcard precedents) before anything else.

## Gotchas

- Byte-addressed registers on a 16-bit bus: odd-register RMW (§2.2
  usb_write_reg8) — decide model-side whether to model halfword
  interleave or accept byte ops directly; the kernel driver uses
  readb/writeb, so byte ops suffice for validation.
- HSDMA channel order is BCM-specific (CONTROL@+4) — do NOT copy
  mainline musbhsdma's ADDR@+0; we're not using DMA anyway, but the
  model's register FILE should still decode per §2.4 so a future
  DMA task finds it sane.
- The 0x2003xxxx≡0x2020xxxx dual view (xti-trace §3): map the alias
  like the i2c engines did.
- New kernel file → both defconfigs synced (kernel/configs/ mirror,
  house convention); new QEMU file → build.ninja surgery + rebuild.
- icount control boot is part of the evidence (task-107/118/122
  convention).
- NEVER write the phone-side binaries; commit locally; never push.
