# 2026-03-23: ARM1176 CPU Discovery

## The Bombshell

SuperPage CPUID = 0x410fb764 = **ARM1176JZF-S**, NOT Cortex-A8.

The Nokia E7 does NOT have an OMAP3630. The application processor is
ARM1176-based, likely Broadcom BCM2763 (same as Nokia N8).

## How We Found Out

1. Asked Telegram community about PADCONF dump
2. Zaiood said "RAPUYAMA and OMAP are unrelated"
3. Checked SuperPage dump (already had it from March 14)
4. Offset 0x40 = ARM CPUID = 0x410fb764 = ARM1176 part 0xB76

## What Was Wrong

Our entire OMAP3630 assumption came from:
- Misreading service manual — RAPUYAMA is the modem, not app processor
- N950/N9 DO use OMAP3630, but E7 is different platform
- Register addresses seemed to match (RAM at 0x80000000 is common)
- Nobody questioned it until now

## What Still Works

- QEMU emulation (synthetic, not HW-accurate anyway)
- Symbian toolchain
- ISI/Phonet protocol (modem-side, CPU-independent)
- RomPatcher+ research
- All documentation about the phone's software/firmware

## What's Broken

- Linux kernel target architecture (need ARMv6, not ARMv7)
- All OMAP3-specific register assumptions
- PADCONF at 0x48002030 — this address is OMAP3-specific
- Device tree — completely wrong SoC
- Pin mux approach — need Broadcom equivalent

## Session Also Included

- ISI Name Service routing probe — modem resources unreachable
- Patcher LDD discovery — all three .ldd files on phone with ALL+TCB caps
- RomPatcher+ confirmed working, custom .rmp patches appear in menu
- Symbian exe execution broken (all return 3) — needs phone reboot
- GitHub push with 120 files of findings
