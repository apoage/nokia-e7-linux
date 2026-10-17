# Task 095: console decode — XTI trace mailbox, I2C myth busted, QEMU drain

## Status
**COMPLETE (2026-08-22).** Hardware-boot blocker #3 ("UART console",
docs/bcm2727-pinctrl-linux.md §4) is resolved by decode: **there is no
APE UART in the boot chain** — the platform console is the XTI trace
mailbox at phys 0x40100000. The QEMU chain now models it and prints
NLoader's real, fully-formatted log; the old printf-stub patch is gone.

## What was decoded (full detail: docs/bcm2727-xti-trace.md)

1. **XTI trace mailbox @ 0x40100000** — tagged binary records in
   per-channel 4KB windows (printf channel 0xEF → 0x401EF000), doorbell
   = channel-id byte to **0x401FD800**. Record grammar: tag 0xC7 at
   slot+0, fmt string 4-packed chars/slot with byte-tail + NUL slot,
   args inline one slot each (%s = inline string, %e/%f/%g = two slots,
   %n/%% = none); prefix-tolerant conversion scan; global write-offset
   at NLoader [0x100076FC], 1KB ring, points AT last-written slot.
   Drainer must drain-and-clear (byte-tail stores leave stale upper
   bytes — post-wrap slots are otherwise ambiguous). Used by SWBL,
   NLoader, and the Symbian kernel (XtiRx driver + XTI-tick calib).
2. **"NLoader's UART" is an I2C master** (send loop 0x10022300:
   poll +8 bit4, data +0x1C, ack 0x10→+8, length +0x18) with selectable
   bases 0x20032200/0x20032300/0x20032500 = sys-block +0x2200/+0x2300/
   +0x2500. The kernel's I2C0 driver sits at 0xC6032200 with the same
   register shape. It carries 0xD6 companion-chip commands, not text.
3. **Sys block dual view: 0x2003xxxx ≡ 0x2020xxxx** (bootloader PA vs
   kernel PA of record) — pad grid, clock-capability regs, I2C engines
   all match across both views.
4. Peripheral map additions: SPI1/2/3 @ 0x20020400/0x20020600/
   0x20020A00; security engine @ 0x200243xx; display supply pair @
   0x20024810/0x20025810; USB OTG + BT PDD @ 0x20025Axx; **eMMC/SD @
   sys+0x2800** (rootfs lead for the next task).

## QEMU changes (emulation/qemu/hw/arm/nokia_e7.c)

- New `bcm2727.xti` mailbox RAM (1MB @ 0x40100000) + `xti-doorbell`
  MMIO (0x401FD800) with record reconstruction (tracked ring offset,
  drain-and-clear) printing to stdout as `nloader[xti239]: …`.
- printf-stub patch removed (real formatter runs unpatched).
- **PL011 moved to 0x40210000** (out of the mailbox window); default
  chain cmdline updated to `earlycon=pl011,0x40210000`.
- `XTI_DEBUG=1` env: doorbell trace + window dumps (/tmp/xtiwin-NN.bin).

## Validation

Chain boot prints NLoader's complete real log (463 records: version
banner, XTI init, TOC dump with all sizes/addresses, GENIO, hw_conf
records, DVFS OPP tables, PMM dumps, "Total bootup time", "Start SOS at
80010000") and Linux 6.12 boots through it to the expected VFS panic,
self-calibrating (1522 BogoMIPS, no lpj=). Wrap-around records (which
cross the 1KB ring boundary) drain cleanly.

## Console strategy on hardware (when we get there)

No APE UART exists in-chain (EDebugPort=0 on the live phone). Options
recorded in the doc §5: XTI jig (needs R&D hardware) or **soft-UART TX
bitbang on genio42 "TxCDa"** (factory pad table leaves it GPIO; pinctrl
sequences decoded; pragmatic first console).

## Follow-ups

- eMMC @ sys+0x2800 ("Rapu_Memcard" driver 0x80160xxx) is the decoded
  target for the rootfs/shell-in-chain item.
- The 0xC3 string-record channel(s) beyond 0xEF are parsed by the model
  but no chain-boot caller was observed.
- If the byte-level wire format of XtiRx's FIFO ports (0x40103880 /
  0x40103EC0, tags 0xC3/0xE2/0xFB) is ever needed for STI-jig output,
  that driver is at ROM 0x803CA2xx.
