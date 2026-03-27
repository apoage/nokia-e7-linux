# PADCONF Quest -- Pin Mux Register Exploration

## IMPORTANT: OMAP3 Assumption May Be Invalid

> **As of 2026-03-23, we know the Nokia E7's application processor is ARM1176JZF-S
> (likely Broadcom BCM2763), NOT TI OMAP3630.** The PADCONF registers documented
> below are OMAP3-specific (base 0x48002030). These addresses may not exist or
> may have completely different meanings on the real hardware.
>
> This page preserves the OMAP3 PADCONF research for reference (it remains
> relevant to the QEMU synthetic emulation), but should NOT be assumed to
> reflect the real Nokia E7 hardware register layout.

## Background

PADCONF (pad configuration) registers control pin multiplexing on TI OMAP
SoCs. Each 32-bit register configures two pads (16 bits each), selecting
the pin function (mux mode 0-7), pull-up/down, and input enable.

On OMAP3630, PADCONF registers live in the System Control Module (SCM)
at base address 0x48002030, extending through 0x48002278.

## OMAP3 PADCONF Layout (for QEMU reference only)

- Base: `0x48002030`
- End: `0x48002278`
- Each register: 32 bits, controls 2 pads (low 16 bits + high 16 bits)
- Bits per pad: `[2:0]` mux mode, `[3]` pull enable, `[4]` pull type, `[8]` input enable

## Status on Real Hardware

The Symbian LDD approach (using `DPlatChunkHw` to map physical memory) was
proposed for reading PADCONF registers from the phone. However:

1. The address 0x48002030 is OMAP3-specific -- if the real SoC is BCM2763,
   this address range likely maps to something else entirely (or nothing).
2. Reading arbitrary physical addresses on unknown hardware is risky.
3. The Broadcom BCM2763 pin mux register layout is not publicly documented.

## What We Know About Real Pin Mux

- GENIO_INIT (encrypted in NAND) configures pad mux during NLoader boot
- The pad mux values are encrypted with TrustZone AES keys we do not have
- GENIO_INIT decryption is blocked (see NLoader analysis)
- The "genio" pins may be Broadcom GPIO, not OMAP GPIO

## Next Steps

- Identify the BCM2763 (or actual SoC) register map
- Check Nokia N8 community for pin mux documentation
- UART capture from real hardware may reveal register writes during boot
- The SuperPage dump may contain additional clues about the memory map
