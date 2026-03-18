# Emulation-First Development Strategy

> **Core principle**: never flash anything to real hardware that hasn't
> booted successfully in emulation. The two E7 units are irreplaceable.

---

## Phase 0 — Firmware Archaeology

```
Original FW image
     │
     ├─→ Extract bootloader (NOLO stages)
     ├─→ Extract kernel / core OS
     ├─→ Extract peripheral firmware blobs (WiFi, DSP, GPU, modem)
     ├─→ Extract device tree / board init (Symbian HAL)
     ├─→ Map I/O register accesses (hardware init sequence)
     └─→ Identify memory map, partition layout, boot params
```

Tools:
- `0xFFFF` — Nokia flasher, dump partitions
- `binwalk` — firmware image analysis
- `strings` / `radare2` / `ghidra` — RE bootloader and HAL
- L3 service manual — cross-reference GPIO/register with schematics

---

## Phase 1 — QEMU OMAP3 Emulation

### Step 1: Boot Original Firmware

**UPDATE (2026-02-26):** QEMU 10.x has NO beagleboard/OMAP3 machine type —
removed from mainline. **RESOLVED**: Built custom OMAP3630 machine from scratch.

Standard OMAP3 L4 peripheral addresses (0x48xxxxxx) confirmed intact in
firmware — register remapping claim from postmarketOS appears overstated.

See: [qemu-machine-design.md](./qemu-machine-design.md) for full analysis.

### Step 1b: Custom OMAP3630 Machine (DONE — 2026-02-26)

Built from scratch on QEMU 10.x following allwinner-a10/cubieboard pattern:

**Working:**
- Machine `nokia-e7` registered in QEMU, boots without crash
- INTC (96 IRQ lines, SIR protocol, priority sorting)
- UART3 serial console (bare-metal test: "Hello from OMAP3630!")
- 32K sync timer clocksource
- 64KB on-chip SRAM
- TAP chip ID (OMAP3630 ES1.2)
- CM/PRM/SCM stub registers
- Linux 6.12 earlycon output — identifies as OMAP3630/DM3730 ES1.2

**Blocked:**
- GPT1 timer init fails (-ENODEV) due to hwmod clock dependency
- Need CM module enable/idle register emulation (Task 009)
- Boot stalls at "Console: colour dummy device" — no tick interrupt

**Source:** `emulation/qemu/hw/arm/{omap3630.c,nokia_e7.c}`, `hw/intc/omap3_intc.c`,
`hw/timer/omap3_gptimer.c`

### Step 2: Custom Device Model

Write QEMU device models for E7-specific peripherals:
- AMOLED panel (DSI) <- service manual
- Keyboard matrix <- service manual
- Camera sensor (CSI-2 stub) <- FW trace
- WL1273 WiFi/BT stub <- FW trace
- PN544 NFC stub <- FW trace
- TWL/TPS PMIC <- service manual
- eMMC model <- FW dump

### Step 3: Boot Custom Linux

- Mainline kernel + omap2plus_defconfig
- ~~Write Nokia E7 device tree (DTS) from phases 0-1~~ DONE (uses omap36xx.dtsi)
- Iterate: boot -> fix -> boot -> fix, all in QEMU
- **Current state**: booting but stalled at timer init (see Step 1b)

### Step 4: Peripheral Bring-Up in Emulator

- WiFi (wlcore) -- probe + firmware load
- Display -- framebuffer output
- Keyboard -- key events
- Camera -- synthetic test frames

---

## Phase 2 — Minimal Flash to Real Hardware

Only after emulator shows clean boot:

1. Full backup of both units (eMMC + OneNAND dump)
2. Identify UART test points from L3 service manual
3. Solder serial console (blind boot is unacceptable)
4. Boot from external source first if possible (USB/UART boot)
5. Flash minimal kernel + initramfs to ONE unit only
6. Second unit stays stock as reference/rescue

---

## Phase 3 — Iterative Hardware Validation

Bring up subsystems in order (each depends on previous):

```
Serial console (UART)          ← first, always
  → eMMC / storage             ← can we read/write?
  → Display (framebuffer)      ← visual feedback
  → Keyboard                   ← input
  → USB gadget                 ← file transfer / networking
  → WiFi                       ← wireless connectivity
  → Bluetooth                  ← secondary wireless
  → Camera (ISP pipeline)      ← image capture
  → GPU (SGX530)               ← accelerated display
  → DSP (C64x+)               ← compute offload
  → Audio                      ← speakers, headphone
  → GPS, NFC, FM               ← nice to have
  → Cellular modem             ← last, most complex
```

---

## Emulator Architecture

```
┌─────────────────────────────────────────────────┐
│                QEMU (host x86_64)                │
│                                                  │
│  ┌────────────┐  ┌─────────────────────────────┐ │
│  │ OMAP3630   │  │ Nokia E7 Board Model        │ │
│  │ CPU model  │  │                              │ │
│  │ (Cortex-A8)│  │  TWL/TPS PMIC ←── svc manual│ │
│  │            │  │  DSI panel    ←── svc manual│ │
│  │  + MMU     │  │  KB matrix   ←── svc manual│ │
│  │  + L3/L4   │  │  Camera stub ←── FW trace  │ │
│  │  + SDMA    │  │  WL1273 stub ←── FW trace  │ │
│  │  + ISP stub│  │  PN544 stub  ←── FW trace  │ │
│  │  + DSS     │  │  eMMC model  ←── FW dump   │ │
│  └────────────┘  └─────────────────────────────┘ │
│                                                  │
│  FW dump loaded ──→ eMMC backing file            │
│  Serial out    ──→ host terminal (stdio)         │
│  Display       ──→ SDL/GTK window                │
│  Network       ──→ host TAP/user networking      │
└─────────────────────────────────────────────────┘
```
