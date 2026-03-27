# CRITICAL: Nokia E7 CPU is ARM1176, NOT Cortex-A8

## Date: 2026-03-23

## Discovery

SuperPage dump from the phone contains the ARM CPUID register:

```
CPUID: 0x410fb764
  Implementer: 0x41 (ARM)
  Variant: r0
  Architecture: 0xF (defined by CPUID scheme, actually ARMv6)
  Part: 0xB76 (ARM1176JZF-S)
  Revision: p4
```

**ARM1176JZF-S** — this is an ARMv6 core, NOT Cortex-A8 (ARMv7).

## What This Means

| Assumption | Reality |
|-----------|---------|
| OMAP3630 (Cortex-A8, ARMv7) | ARM1176 (ARMv6) — likely Broadcom |
| TI OMAP3 register map | Unknown register map |
| PADCONF at 0x48002030 | May not exist |
| pinctrl-single driver | Wrong SoC |
| OMAP3 Linux kernel config | Wrong architecture variant |

## Likely Application Processor

ARM1176JZF-S **made by Samsung** (confirmed by community member karaba abdu).

Candidates:
- **Broadcom BCM2763** — Nokia N8's application processor (ARM1176 core)
- **Samsung S3C6410 / S5P6440** — Samsung ARM1176 SoCs
- **Custom Nokia/Samsung design** — Samsung-fabbed ARM1176 with Nokia IP

The "made by Samsung" could mean Samsung designed the SoC, or Samsung
fabricated a Broadcom/Nokia design. Nokia N8 teardowns may clarify.

## Bootloader Status

Community confirms: **"nobody has cracked the bootloader yet"**
- Symbian bootloader chain (SWBL → NLoader → SOS) is non-standard
- Our NLoader auth bypass works in QEMU but GENIO_INIT remains encrypted
- Real hardware Linux boot requires either bootloader crack or alternative entry

## Evidence That Was Misleading

1. "RAPUYAMA (OMAP3630)" in service manual notes — this was our interpretation,
   RAPUYAMA is the modem/CMT, not the app processor
2. "Gazoo=OMAP3630" codename — Gazoo may refer to a different product line
3. ASIC ID 0x1921 — this is the RAPUYAMA modem ID, not the app processor
4. Register addresses at 0x80000000 (RAM) — coincidental overlap, many ARMs use this
5. Nokia N950/N9 DO use OMAP3630 — but E7 is a different platform

## What Remains Valid

- QEMU emulation still boots and works (it's a simulation, not real HW match)
- Symbian toolchain and exe building approach is correct
- ISI/Phonet protocol research is valid (it's about the modem, not the CPU)
- RomPatcher+ and kernel access research is valid
- The phone IS ARM-based, just ARMv6 not ARMv7

## Impact on Project

### Must Change
- Real hardware Linux kernel must target ARMv6 / ARM1176, not ARMv7 / Cortex-A8
- Need to identify the actual SoC (BCM2763? BCM2727 dual-role?)
- Register map for pin mux, clocks, GPIO is completely different
- Device tree must be written for the actual SoC, not OMAP3

### Can Keep (for now)
- QEMU machine as a development/test platform (not HW-accurate)
- All Symbian-side research and tools
- ISI protocol tools and findings

## Next Steps

1. Identify the exact application processor (BCM2763? check N8 teardowns)
2. Find register documentation for the Broadcom SoC
3. Check if Linux has any BCM2763 support (unlikely — Broadcom is notoriously closed)
4. Re-evaluate whether Linux on E7 is feasible with a Broadcom AP
5. The N8 community may have more information — same AP family

## Community Input

Zaiood (Telegram) stated "RAPUYAMA and OMAP are unrelated" — confirmed correct
by SuperPage CPUID analysis.
