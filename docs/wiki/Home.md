# Nokia E7-00 Linux Project Wiki

> Bringing Linux to the Nokia E7-00 (RM-626). Build a snappy handheld
> terminal with full networking, computational photography, and every ASIC
> exploited. Emulation first, real hardware second.

Current status lives in the [README](../../README.md) — this page is the map.

---

## Hardware: Broadcom BCM2727B1 (ARM1176JZF-S)

The Nokia E7's application processor is **Broadcom BCM2727B1** with an
**ARM1176JZF-S** core (ARMv6). This was confirmed by SuperPage CPUID
(0x410fb764), ROM dump string `bcm2727b1`, and community verification.

**NOT TI OMAP3630** -- the earlier assumption was incorrect. RAPUYAMA (D1800)
is the modem/CMT only.

### BCM2835 adjacency (with a big caveat)
The peripheral register map at **0x20200000** sits in the same 0x20xxxxxx
space as the Raspberry Pi's BCM2835, and the BCM2835 ARM Peripherals
datasheet is a useful reference -- but the blocks themselves are custom RAPU
designs, not BCM2835 layouts (the GPIO has 7 SET/CLR banks and no GPFSEL,
the ICU and memcard host are custom too). Details:
[gpio-static-pinmux.md](../gpio-static-pinmux.md).

---

## The story so far

1. **CPU re-identified** -- BCM2727B1, not OMAP3630 --
   [critical-cpu-discovery.md](../critical-cpu-discovery.md)
2. **31MB ROM dump decoded** -- 800+ line peripheral map, driver strings,
   file inventory -- [rom-dump-analysis.md](../rom-dump-analysis.md)
3. **Phone-side probing** **(phone-tested)** -- custom native exes (the
   unprotected-UID recipe), WiFi remote bridge, crash-isolated memory reads,
   ISI/Phonet over USB -- [tasks/090](../../tasks/090-O-fix-exe-build-armv6.md),
   `symbian-sdk/memread/`
4. **Kernel-memory wall mapped** -- TCB is ROM-only, caps shortfall at channel
   open = kernel panic; user-space probing covers everything else
5. **Platform decoded block by block** --
   GPIO/pinmux ([genio-padtable.md](../genio-padtable.md)),
   interrupts + timer ([bcm2727-icu-timer.md](../bcm2727-icu-timer.md)),
   eMMC/SD ([bcm2727-memcard.md](../bcm2727-memcard.md)),
   USB ([bcm2727-usb-flashmode.md](../bcm2727-usb-flashmode.md)),
   OneNAND boot chain ([bcm2727-onenand.md](../bcm2727-onenand.md)),
   display/VideoCore ([bcm2727-display-recon.md](../bcm2727-display-recon.md))
6. **QEMU runs the genuine boot chain** -- SWBL → NLoader → MCUSW → Linux
   `e7:~#`, byte-identical across boots, with USB host and the display
   service live on top; every change guarded by `tools/chain-regress.sh`

## What's next

Hardware: passive USB probing (round 3), one-wire console on TxCDa, then the
first hardware boot. See the README's "Next Steps" section.

---

## Pages

| Page | Description |
|------|-------------|
| [Hardware Overview](Hardware-Overview.md) | BCM2727B1, VideoCore III, peripheral map |
| [Boot Process](Boot-Process.md) | The Symbian boot chain as decoded |
| [QEMU Machine](QEMU-Machine.md) | The qemu-e7sw emulation machine |
| [PADCONF Quest](PADCONF-Quest.md) | GPIO/pin mux register exploration |
| [Symbian Toolchain](Symbian-Toolchain.md) | Building native code for the phone |
| [Research Sources](Research-Sources.md) | Community contacts, references |

## Key Files

- `docs/` -- the reverse-engineering notes (onenand, display-recon,
  usb-flashmode, memcard, softuart, icu-timer are the load-bearing ones)
- `docs/phone-probes/` -- all live phone probe outputs
- `docs/rom-dump-analysis.md` -- 800-line ROM peripheral map
- `tasks/` -- work items with state tracking (090-127)
- `tools/` -- vc_session client, chain-regress harness, ISI/Phonet tools
- `symbian-sdk/memread/` -- phone-side probes + PyS60 bridge scripts
- `lore/daily/` -- chronological session logs
