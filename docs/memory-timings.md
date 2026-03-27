# Nokia E7 — SDRC Memory Timing Analysis

> **Note (2026-03-23):** This document references OMAP3630 SDRC registers. Real HW
> AP is ARM1176JZF-S (ARMv6), likely BCM2763 — the memory controller register map
> may differ. These timings remain valid for QEMU emulation (OMAP3630-based).
> See docs/critical-cpu-discovery.md.

## Summary

**NLoader does NOT configure SDRC.** The SWBL (Secondary Boot Loader, 8.5KB at
NAND offset 0x800) initializes SDRC before passing control to NLoader. NLoader
only reports the memory size that ROM/SWBL already configured:
```
hw_conf: SDRAM: %d MB at %08X
```

The exact Nokia timing values are locked inside the SWBL binary, which is not
present in any extracted partition image. This document provides reference
timing values from OMAP3630 reference boards and JEDEC LPDDR1 specifications.

## Memory Identification

| Property | Value |
|----------|-------|
| Part number | Samsung K5W8G1GACK-AL56 |
| Type | LPDDR1 (JESD209) |
| Density | 2 Gbit (256 MB) total |
| Per-die SDRAM | 1 Gbit (128 MB) x16 |
| NAND flash | 8 Gbit (1 GB) in same MCP |
| Bus width | x32 (two x16 dies on CS0 + CS1) |
| Package | POP (stacked on OMAP3630/RAPUYAMA) |
| Speed grade | -AL56 (5.6ns tCK min, ~178MHz max rated) |
| Target clock | **166MHz** (safe within speed grade; 200MHz is marginal) |
| Configuration | 2 × 128MB chip-selects (CS0 + CS1) |
| Source | Board markings, part number decode |

**Part number decode:** K5W = MCP (NAND+mSDRAM), 8G = 8Gbit NAND, 1G = 1Gbit
mSDRAM, ACK = package/config, AL56 = speed grade 5.6ns.

**Dual chip-select:** OMAP3630 SDRC has a 32-bit data bus composed of two x16
lanes. Each 128MB die occupies one chip-select. ACTIM registers must be
configured for **both** CS0 (offsets 0x09C/0x0A0) and CS1 (offsets 0x0C4/0x0C8).
SDRC_MCFG must also be set per-CS.

**Note:** Task 029 originally assumed Elpida LPDDR. Part number confirms
Samsung. The 8Gbit NAND in the same MCP is likely the OneNAND connected via GPMC.

## SDRC Register Reference

Base address: `0x6D000000`

| Register | Offset (CS0) | Offset (CS1) | Purpose |
|----------|-------------|-------------|---------|
| SDRC_MCFG | 0x080 | 0x0B0 | Memory config (size, type, width) |
| SDRC_MR | 0x084 | 0x0B4 | Mode register (CAS, burst length) |
| SDRC_ACTIM_CTRLA | 0x09C | 0x0C4 | Active timing A (tRFC, tRC, tRAS, tRP, tRCD, tRRD, tWR, tDAL) |
| SDRC_ACTIM_CTRLB | 0x0A0 | 0x0C8 | Active timing B (tWTR, tCKE, tXP, tXSR) |
| SDRC_RFR_CTRL | 0x0A4 | 0x0D4 | Refresh control (rate + auto-refresh) |
| SDRC_DLLA_CTRL | 0x060 | — | DLL control (shared) |
| SDRC_POWER | 0x070 | — | Power management (shared) |

### ACTIM_CTRLA Bit Fields

```
[31:27] TRFC  — tRFC: Refresh-to-activate (5 bits)
[26:22] TRC   — tRC:  Row cycle time (5 bits)
[21:18] TRAS  — tRAS: Row active time (4 bits)
[17:15] TRP   — tRP:  Row precharge (3 bits)
[14:12] TRCD  — tRCD: RAS-to-CAS delay (3 bits)
[11:9]  TRRD  — tRRD: Row-to-row delay (3 bits)
[8:6]   TDPL  — tWR:  Write recovery (3 bits)
[4:0]   TDAL  — tDAL: Auto-precharge (5 bits)
```

### ACTIM_CTRLB Bit Fields

```
[17:16] TWTR  — tWTR: Write-to-read turnaround (2 bits)
[14:12] TCKE  — tCKE: CKE minimum pulse width (3 bits)
[10:8]  TXP   — tXP:  Exit power-down to command (3 bits)
[7:0]   TXSR  — tXSR: Self-refresh exit to command (8 bits)
```

## Reference Timing Values

### Source: u-boot OMAP3 mem.h + Zoom3 board

Three reference configurations, all validated on OMAP3 silicon:

#### Zoom3 Board (OMAP3630 + Hynix H8MBX00U0MER-0EM, 200MHz)

The only publicly available OMAP3630-specific timing set. Conservative margins.

| Register | Value | Decoded Fields |
|----------|-------|----------------|
| ACTIM_CTRLA | `0xa2e1b4c6` | TRFC=20, TRC=11, TRAS=8, TRP=3, TRCD=3, TRRD=2, TDPL=3, TDAL=6 |
| ACTIM_CTRLB | `0x0002131c` | TWTR=2, TCKE=1, TXP=3, TXSR=28 |
| RFR_CTRL | `0x0005e601` | 200MHz refresh rate, ARE=1 |
| MR | `0x00000032` | BL=4-word burst, CAS=3, sequential |

#### Micron/JEDEC LPDDR1 Standard (200MHz)

JEDEC-minimum timing at 200MHz (5ns cycle). Matches Micron datasheet values.

| Register | Value | Decoded Fields |
|----------|-------|----------------|
| ACTIM_CTRLA | `0x7ae1b4c6` | TRFC=15, TRC=11, TRAS=8, TRP=3, TRCD=3, TRRD=2, TDPL=3, TDAL=6 |
| ACTIM_CTRLB | `0x00024217` | TWTR=2, TCKE=4, TXP=2, TXSR=23 |
| RFR_CTRL | `0x0005e601` | 200MHz refresh rate, ARE=1 |
| MR | `0x00000032` | BL=4-word burst, CAS=3, sequential |

#### Hynix Generic (200MHz)

From u-boot mem.h, used on several OMAP3 boards.

| Register | Value | Decoded Fields |
|----------|-------|----------------|
| ACTIM_CTRLA | `0x92e1c4c6` | TRFC=18, TRC=11, TRAS=8, TRP=3, TRCD=4, TRRD=2, TDPL=3, TDAL=6 |
| ACTIM_CTRLB | `0x0002111c` | TWTR=2, TCKE=1, TXP=1, TXSR=28 |
| RFR_CTRL | `0x0005e601` | 200MHz refresh rate, ARE=1 |
| MR | `0x00000032` | BL=4-word burst, CAS=3, sequential |

#### Samsung K5W8G1GACK JEDEC-Computed (166MHz) — RECOMMENDED

Computed from JEDEC LPDDR1 timing parameters for Samsung 1Gbit mDDR at
166MHz (tCK = 6.02ns). This is the correct target frequency for the -5.6
speed grade. **Both CS0 and CS1 must be programmed identically.**

| Parameter | ns | Clocks @166MHz |
|-----------|-----|----------------|
| tRFC | 110 | 19 |
| tRC | 60 | 10 |
| tRAS | 42 | 7 |
| tRP | 18 | 3 |
| tRCD | 18 | 3 |
| tRRD | 12 | 2 |
| tWR (tDPL) | 15 | 3 |
| tDAL | 33 | 6 |
| tXSR | 120 | 20 |
| tXP | 25 | 5 |
| tWTR | 1 clk | 1 |
| tCKE | 1 clk | 1 |
| CL | — | 3 |

| Register | Value | Decoded Fields |
|----------|-------|----------------|
| ACTIM_CTRLA | `0x9A9DB4C6` | TRFC=19, TRC=10, TRAS=7, TRP=3, TRCD=3, TRRD=2, TDPL=3, TDAL=6 |
| ACTIM_CTRLB | `0x00011214` | TWTR=1, TCKE=1, TXP=2, TXSR=20 |
| RFR_CTRL | `0x0004DC01` | 166MHz refresh (7.8µs standard), ARE=1 |
| MR | `0x00000032` | BL=4-word burst, CAS=3, sequential |

### RFR_CTRL by Frequency

| Clock Rate | RFR_CTRL | Source |
|------------|----------|--------|
| 200 MHz | `0x0005e601` | u-boot SDP_3430 |
| 165 MHz | `0x0004e201` | u-boot SDP_3430 |
| 133 MHz | `0x0003de01` | u-boot SDP_3430 |
| 100 MHz | `0x0002da01` | u-boot SDP_3430 |

## Recommended Strategy for Nokia E7

### For QEMU (current)
No changes needed. QEMU's SDRC model stores whatever the kernel writes
and returns it on read. Actual timing accuracy is irrelevant in emulation.

### For Real Hardware (Task 014)

**Priority order of approaches to get exact timings:**

1. **UART capture at boot** (best, non-invasive)
   Connect to UART3 during Symbian boot. NLoader prints `hw_conf: SDRAM`.
   Capture the SWBL output too — it may print SDRC config. If SWBL is
   silent, use approach 2 or 3.

2. **Runtime register dump** (good, requires root on Symbian)
   Read SDRC registers from a running Symbian system:
   ```
   Read 0x6D00009C → ACTIM_CTRLA_0
   Read 0x6D0000A0 → ACTIM_CTRLB_0
   Read 0x6D0000A4 → RFR_CTRL_0
   Read 0x6D000084 → MR_0
   Read 0x6D000080 → MCFG_0
   ```
   Requires a kernel driver or /dev/mem access on Symbian.

3. **SWBL binary extraction** (definitive)
   SWBL is at NAND offset 0x800 (8.5KB). If we can read raw NAND (e.g.
   via JTAG, ATF box, or OpenOCD), disassembly will reveal exact timing
   tables. SWBL is NOT in any extracted FPSX or rofs image.

4. **Use Zoom3 reference values** (fallback, conservative)
   The Zoom3 ACTIM_CTRLA `0xa2e1b4c6` has the most conservative margins
   (TRFC=20 vs JEDEC-minimum 15). For initial bring-up on real hardware,
   these are the safest starting point. They may waste ~2% bandwidth but
   will not cause corruption.

### Why Not Micron/JEDEC Minimum?

Samsung K5W8G1GACK-AL56 speed grade is "-5.6" (5.6ns). At 200MHz (5.0ns
cycle), the part runs slightly above its rated frequency. Nokia's SWBL
almost certainly adds margin. The Zoom3 values already include margin
(TRFC=20 vs minimum 15) and are the safest bet.

However, if the Nokia SWBL runs SDRC at 166MHz instead of 200MHz (which
the "-5.6" speed grade supports natively), entirely different timing
tables apply. The UART capture will resolve this.

## Investigation Log

### What We Searched

1. **NLoader binary** (188KB at firmware-re/analysis/nloader.bin):
   Searched for all SDRC register addresses (0x6D0000xx). Found exactly
   ONE reference: `0x6D000000` (SDRC base) at offset 0x13F0D — used only
   for reading memory size. Zero writes to timing registers.

2. **CORE FPSX** (129MB encrypted): Searched for 0x6D00 pattern in TLV
   headers. Found "SWBL" as a partition name string only.

3. **rofs.img**: SWBL at NAND offset 0x800 is below the rofs partition
   (which starts at 0x140000). Offset 0x800 within rofs.img is zeros.

4. **Kernel sources** (Linux 6.12): No pre-computed ACTIM tables for any
   OMAP3 board. The old board-omap3*.c files with timing tables were
   removed during the device-tree migration. Only RFR_CTRL reference
   values remain in `arch/arm/mach-omap2/sdrc.h`.

5. **u-boot sources**: `arch/arm/include/asm/arch-omap3/mem.h` has
   Micron and Hynix timing tables. Zoom3 board (OMAP3630) has the only
   known 3630-specific configuration.

### Confirmed: NLoader Does Not Configure SDRC

From `docs/nloader-analysis.md` line 285:
> No SDRC register strings found. "hw_conf: SDRAM" only reports what ROM set up.

NLoader reads SDRC_MCFG to determine memory size, then prints it. The
actual SDRC initialization sequence is:

```
ROM bootloader → SWBL (8.5KB) → configures SDRC → NLoader (184KB) → kernel
                  ↑ this is where timing is set
```

## Sources

- u-boot `arch/arm/include/asm/arch-omap3/mem.h` — ACTIM macro definitions
- Zoom3 board: `sdram-hynix-h8mbx00u0mer-0em.h` — OMAP3630-validated timings
- Linux kernel `arch/arm/mach-omap2/sdrc.h` — register offsets, RFR_CTRL values
- OMAP3630 TRM Chapter 12 — SDRC register documentation
- JEDEC JESD209 (LPDDR1) — standard timing specifications
- `docs/flash-memory-map.md` — SWBL location and size
- `docs/nloader-analysis.md` — confirmation that NLoader does not configure SDRC

## Open Questions

1. **SDRC_SHARING register** — configures CS0/CS1 interleaving and bus width
   mapping. Board-specific, not in JEDEC specs. UART capture or SWBL disassembly
   will reveal Nokia's configuration.

2. **DPLL3 M/N divisors** — 166MHz SDRC clock requires specific DPLL3 settings.
   SWBL or NLoader configures this. Our QEMU CM model tracks DPLL state but
   hasn't confirmed the actual divider values Nokia uses.

3. **OneNAND in same MCP** — the 8Gbit NAND half of K5W8G1GACK is likely the
   OneNAND connected via GPMC. If so, OneNAND timing parameters may also be
   derivable from this part's datasheet (if found).

4. **Temperature-compensated refresh** — JEDEC specifies 7.8µs at normal temp,
   3.9µs at high temp. Nokia may use auto-temperature-compensated refresh
   (SDRC supports this). The RFR_CTRL value above assumes standard 7.8µs.

5. **Actual Nokia clock frequency** — the -5.6 speed grade strongly suggests
   166MHz, but Nokia could run at 133MHz (extra conservative) or attempt
   178MHz (rated max). Only UART capture or SWBL disassembly will confirm.
