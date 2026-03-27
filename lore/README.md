# Nokia E7 Linux Revival — Project Lore

> **Note (2026-03-23):** Real HW is BCM2727B1 (ARM1176), not OMAP3630. QEMU uses synthetic OMAP3630. See docs/critical-cpu-discovery.md

> Bring two Nokia E7-00 units back to life running Linux. Full networking
> stack, computational photography exploiting every ASIC on chip, snappy
> handheld terminal experience.

## What Is This

This is the living documentation of the project. Not dry API docs — the
actual story of bringing decade-old hardware back from the dead with
modern Linux and hand-tuned code that squeezes every FLOP out of every
compute unit on the OMAP 3630.

## The Hardware

**Nokia E7-00 (RM-626)** — two units available

A slide-out QWERTY phone from 2011 with serious silicon under the hood:

- **ARM Cortex-A8** — main CPU, NEON SIMD, up to 1 GHz with OC
- **TMS320C64x+ DSP** — 8-way VLIW beast at 520 MHz (800 MHz OPP-SB)
- **PowerVR SGX530 GPU** — GLES 2.0, USSE shaders (partially reverse-engineered)
- **OMAP3 ISP** — hardware image pipeline (demosaic, NR, resize, stats)
- **256 MB RAM** — the constraint that makes everything interesting

Plus: WiFi/BT/FM/GPS combo chip, NFC, 3G modem, 8MP camera, AMOLED display,
HDMI out, and that gorgeous backlit keyboard.

## The Goals

1. **Boot Linux** — mainline kernel, minimal userland, framebuffer console
2. **Full networking** — WiFi, BT, cellular data, GPS — all working
3. **Computational photography** — multi-frame HDR, night mode, super-res,
   exploiting ISP + DSP + NEON + GPU in a zero-copy pipeline
4. **Snappy terminal** — fast boot, responsive UI, useful as a daily tool
5. **Push the hardware** — find and exploit every undocumented feature,
   every hidden register, every cycle that can be saved

## The Approach

**Emulate first, flash safe.** L3 service manuals + firmware dumps →
QEMU emulation → validate everything in software → flash one unit only.
Second unit stays stock as reference.

## Key Resources Available

- L3 service manuals (schematics, test points, signal routing)
- Multiple firmware variants (Euro1/Euro2/Euro3, stock + modded)
- SGX540 reverse engineering work (USSE ISA, instruction encoding, shader hooking)
- InfinityBEST tool data (camera calibration, cert backups)
- Nokia Suite backup

## Documentation Map

- `lore/` — you are here. Project story and daily notes
  - `daily/` — dated entries: findings, successes, failures
- `docs/` — technical deep-dives
  - `hardware-lore.md` — master hardware reference
  - `undocumented-*.md` — per-ASIC deep dives (ARM, DSP, ISP, GPU, system)
  - `emulation-strategy.md` — the emulate-first approach
- `tasks/` — executable work items (see tasks/README.md)
- `prompts/` — reusable agent prompt library (see prompts/README.md)
- `firmware-stock/` — organized firmware archive with checksums
- `archive-dump/` — raw messy warehouse of tools, manuals, utilities

## Project Status

**Phase: Pre-emulation**

Next milestone: QEMU OMAP3 emulator running, firmware extraction started.

---

*Started: 2025-02-25*
