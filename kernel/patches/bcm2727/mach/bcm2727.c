// SPDX-License-Identifier: GPL-2.0
/*
 * Broadcom BCM2727B1 (RAPU) device tree glue.
 *
 * The BCM2727B1 is the application-engine SoC in the Nokia E7-00 (RM-626):
 * ARM1176JZF-S (ARMv6KZ) + VideoCore III.  Everything on this platform is
 * described by device tree; this file only registers the machine.
 *
 * Phys map (from Symbian ROM static analysis, docs/gpio-static-pinmux.md):
 *   0x10000000  PDRAM (2 MB, NLoader + early boot structures)
 *   0x20200000  "system block": GENIO pad/GPIO register file, clocks,
 *               security, VideoCore interface (up to 0x20400000)
 *   0x80000000  SDRAM 256 MB
 */

#include <asm/mach/arch.h>

static const char * const bcm2727_compat[] = {
	"brcm,bcm2727b1",
	"brcm,bcm2727",
	NULL
};

DT_MACHINE_START(BCM2727, "BCM2727B1 (RAPU)")
	.dt_compat = bcm2727_compat,
	/*
	 * No .map_io, no .init_irq: this SoC's interrupt controller is not
	 * yet decoded (see docs/bcm2727-pinctrl-linux.md "open items").
	 */
MACHINE_END
