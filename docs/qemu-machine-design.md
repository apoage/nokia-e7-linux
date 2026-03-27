# QEMU Machine Definition Design: Nokia E7 (RM-626, OMAP3630)

> **CRITICAL NOTE (2026-03-23):** The real Nokia E7 application processor is
> **NOT TI OMAP3630**. SuperPage CPUID = 0x410fb764 = **ARM1176JZF-S (ARMv6)**,
> likely **Broadcom BCM2763** (same as Nokia N8). The QEMU machine defined here
> is a **synthetic OMAP3630-based emulation** that does NOT match real hardware
> at the SoC level. It remains useful for Linux driver development and peripheral
> bring-up, but register addresses, CPU architecture (ARMv7 vs ARMv6), and
> on-chip peripherals (DSP, GPU, ISP, crypto) diverge from reality.
> See: [critical-cpu-discovery.md](./critical-cpu-discovery.md)

**Document date:** 2026-02-26
**Status:** Design specification — synthetic emulation (real HW is ARM1176/BCM2763)
**Authors:** derived from firmware RE, QEMU source archaeology, TI TRM

---

## Table of Contents

1. [Background and Context](#1-background-and-context)
2. [QEMU OMAP3 History](#2-qemu-omap3-history)
3. [Approach Evaluation](#3-approach-evaluation)
4. [Recommended Approach: New Machine on Linaro Fork Base](#4-recommended-approach)
5. [OMAP3630 Memory Map](#5-omap3630-memory-map)
6. [Machine Definition Architecture](#6-machine-definition-architecture)
7. [Phased Emulation Plan](#7-phased-emulation-plan)
8. [Build and Integration](#8-build-and-integration)
9. [Linux Kernel Configuration](#9-linux-kernel-configuration)
10. [Testing Strategy](#10-testing-strategy)
11. [Source References](#11-source-references)

---

## 1. Background and Context

The Nokia E7 (RM-626, codename Taika) is a Symbian^3 smartphone built on the Texas Instruments OMAP3630 SoC (ARM Cortex-A8, up to 1 GHz). The hardware reverse engineering effort has produced confirmed register maps from firmware traces:

```
Confirmed from firmware RE:
  0x48004000  SCM (System Control Module / CM)
  0x48200000  MPU INTC (96-line interrupt controller)
  0x4809C000  MMC/SD1 (HSMMC)
  0x48306000  PRCM_IVA2 (Power & Reset CM for IVA2 domain)
  0x49020000  UART3 (console port, 115200 8N1)
  0x49032000  GPTIMER2 (GP timer 2)
  0x49042000  UART4
  0x50000000  GPMC CS0 (OneNAND boot flash, 256 MB window)

Peripheral address spaces observed:
  0x48xxxxxx - 0x49xxxxxx : 4288 unique L4 peripheral hits
  0x50xxxxxx              : 909 unique GPMC hits (OneNAND + BCM2727)
```

Hardware inventory:
- CPU: ARM Cortex-A8 @ 1 GHz (OMAP3630 die)
- RAM: 256 MB LPDDR at 0x80000000
- Flash: OneNAND (via GPMC at 0x50000000)
- Storage: eMMC (via HSMMC1 at 0x4809C000)
- Camera ISP: BCM2727 (Broadcom VideoCore III, CCP2 interface)
- PMIC: TWL5031 (I2C bus 1)
- Display: AMOLED via DSI
- Connectivity: WL1273 (WiFi/BT, MMC3), PN544 (NFC, I2C)

---

## 2. QEMU OMAP3 History

### 2.1 OMAP3 Was Never in Upstream QEMU

This is the critical finding: **OMAP3/BeagleBoard emulation was never merged into upstream QEMU mainline.**

The upstream QEMU tree supported OMAP1 (omap1.c, for the Siemens SX1/sx1 machine via OMAP310) and OMAP2 (omap2.c, for the Nokia N800/N810 via OMAP2420). OMAP3 — the generation that includes OMAP3530, OMAP3630, DM3730 — existed only as a downstream fork.

Timeline of OMAP in upstream QEMU:

```
QEMU upstream OMAP lineage:
  OMAP1 (omap1.c)  →  OMAP2 (omap2.c)  →  [OMAP3: never upstreamed]
  SX1 / sx1-v1         N800, N810

QEMU 9.2 (Dec 2024):
  - OMAP2 machines removed: n800, n810, cheetah (OMAP1)
  - Removed: omap2.c, omap_gptimer.c, omap_synctimer.c, TYPE_OMAP2_GPIO
  - Patch series: "[PATCH for-9.2 00/53] arm: Drop deprecated boards"
    https://www.mail-archive.com/qemu-devel@nongnu.org/msg1063507.html

QEMU 10.x (current, as of 2026):
  - OMAP1 remains: hw/arm/omap1.c (sx1, sx1-v1 machines still alive)
  - Remaining OMAP files: hw/char/omap_uart.c, hw/i2c/omap_i2c.c,
    include/hw/arm/omap.h, hw/gpio/omap_gpio.c (OMAP1 GPIO)
  - NO OMAP2, NO OMAP3 in mainline
  - BeagleBoard/beaglexm: was NEVER upstream

Future risk: sx1/sx1-v1 are under active deprecation discussion
  https://www.mail-archive.com/qemu-devel@nongnu.org/msg1071227.html
  If they go, remaining omap_uart/omap_i2c files may follow.
```

### 2.2 The Linaro QEMU Fork (OMAP3's Only Home)

OMAP3 emulation was developed outside mainline by the Linaro Toolchain Working Group and Google Code's qemu-omap3 project:

```
qemu-omap3 (Google Code, archived):
  https://code.google.com/archive/p/qemu-omap3
  Target: BeagleBoard (OMAP3530)
  Status: Archived, last active ~2011

qemu-linaro (Linaro fork):
  Source:  git://git.linaro.org/qemu/qemu-linaro.git  [defunct]
  Mirror:  https://github.com/av86743/qemu-linaro-clone
  Mirror:  https://github.com/dougg3/qemu-linaro-beagleboard
  Last release: 2014.01 (2014-01-16)
  Base: upstream QEMU ~1.7 era
```

### 2.3 Files in the Linaro/qemu-omap3 Fork

From `https://github.com/av86743/qemu-linaro-clone` (tree: master):

```
Board and SoC:
  hw/beagle.c            — machine definition: "beagle" (OMAP3530, 256MB)
                                               "beaglexm" (OMAP3630, 512MB)
  hw/omap3.c             — OMAP3 SoC: L3/L4 interconnect, PRM, CM,
                           12 GP timers, UART 1-4, I2C 1-3, GPIO 1-6,
                           McBSP, SPI (MCSPI 1-4), USB host/OTG, SDRC
  hw/omap3_boot.c        — OMAP3 boot ROM emulation (NAND/OneNAND/MMC)
  hw/omap3_mmc.c         — OMAP3 HSMMC controller

Supporting OMAP peripherals (shared with OMAP1/2):
  hw/omap.h              — common header
  hw/omap1.c             — OMAP1 SoC (reused UART, I2C, GPIO, timer code)
  hw/omap2.c             — OMAP2 SoC (also removed from upstream)
  hw/omap_clk.c          — clock framework
  hw/omap_dma.c          — SDMA (System DMA)
  hw/omap_gpio.c         — GPIO controller
  hw/omap_i2c.c          — I2C controller
  hw/omap_uart.c         — UART (wraps 16C750-compatible core)
```

### 2.4 Key Memory Addresses in omap3_boot.c

Confirmed against our firmware RE (matches exactly):

```c
// From hw/omap3_boot.c in av86743/qemu-linaro-clone:
0x48004a40  CM_CLKSEL1_EMU
0x48004c40  CM_CLKSEL_WKUP
0x48004d00  CM_CLKEN_PLL / CM_CLKEN_PLL_MPU
0x48004d30  CM_AUTOIDLE_PLL
0x48004d40  CM_CLKSEL1_PLL
0x48004d44  CM_CLKSEL2_PLL
0x48307270  PRM_CLKSRC_CTRL
0x48306d40  PRM_CLKSEL
0x6d000040  SDRC config registers (SDRAM controller)
0x6e000060  GPMC config (NAND/flash)
0x40014000  Boot ROM base
0x4020ffc8  SRAM vectors
```

These match our firmware-RE observations precisely, confirming the Linaro OMAP3 model uses **standard, unremapped TI addresses**.

### 2.5 beagle.c Machine Definition Summary

`beaglexm` (OMAP3630, our closest match) initializes:
- OMAP3 MPU via `omap3_mpu_init()` in `omap3.c`
- NAND flash (Micron, MTD-compatible) via GPMC CS0
- SD/MMC card (optional) via HSMMC
- TWL4030 PMIC via I2C bus 1
- GPIO pins 171-173 for board variant identification
- DDC/EDID monitor data via I2C

The E7 differences from beaglexm:
- 256 MB RAM (not 512 MB)
- TWL5031 PMIC instead of TWL4030
- BCM2727 camera ISP (additional hardware)
- eMMC via HSMMC1 (not SD card in slot)
- OneNAND instead of NAND (same GPMC window, different chip type)

---

## 3. Approach Evaluation

### Approach A: Resurrect Linaro QEMU Fork (~QEMU 1.7 base)

**What it provides:**
- Complete OMAP3 SoC model (omap3.c: PRM, CM, 12 GP timers, UART 1-4,
  I2C 1-3, GPIO 1-6, SDRC, GPMC)
- omap3_boot.c: Boot ROM sequence, NAND/OneNAND/MMC boot
- omap3_mmc.c: HSMMC controller model
- beagle.c: Working BeagleXM machine (OMAP3630, our SoC)
- TWL4030 PMIC model (close to E7's TWL5031)
- Proven to boot Linux 3.x kernels on BeagleBoard hardware

**Problems:**
- Base is QEMU ~1.7 (circa 2014), 12 years old
- Does NOT use QOM (QEMU Object Model) — predates or uses early QOM
- Pre-dates modern QEMU device APIs (MemoryRegion, QOMified devices)
- Pre-dates meson build system (uses old Makefile approach)
- Cannot easily build against modern QEMU 9/10 toolchain
- Porting 12 years of API changes = major engineering effort
- QEMU 1.7 has known security issues, cannot use as long-term base
- No KVM support (irrelevant for ARM, but shows age)
- Compilation requires old glibc workarounds (see dougg3 fork fixes)

**Assessment:** Usable as reference and test oracle, NOT as build base.

**Verdict:** Use as reference code only. Read omap3.c for register maps,
  read omap3_boot.c for boot sequence, extract logic for porting.

### Approach B: New Machine on Top of Modern QEMU (virt or realview-pb-a8)

**What it provides:**
- Modern QEMU 10.x base (actively maintained, security-patched)
- Access to existing PL011 UART (pl011.c) — clean, modern QOM device
- Access to existing ARM GIC (arm_gic.c) — could sub for OMAP3 INTC
- Access to existing PL031 RTC, PL061 GPIO stubs
- meson.build + Kconfig build system
- Device tree generation support (via libfdt in QEMU)
- Virtio, network, block devices work out of box
- cubieboard (Allwinner A10, also Cortex-A8) as a structural template

**Problems:**
- PL011 UART ≠ OMAP3 UART: different register layout
  Linux earlycon for OMAP uses `ti,omap3-uart` compatible string,
  not `arm,pl011`. The omap_uart driver is 16C750-based, not PL011.
- ARM GIC ≠ OMAP3 INTC: different register layout, different SIR register,
  different idle/turbo errata handling (i540 errata)
- No OMAP3 clock/power registers (PRCM) — firmware probes these early
- No OneNAND model in modern QEMU (was removed with OMAP2)
- No OMAP3 HSMMC model
- Memory map must match 0x48xxxxxx/0x49xxxxxx — achievable with custom
  MemoryRegion mappings regardless of base machine

**Mitigation for UART mismatch:**
  Use `serial_mm_init()` with 16550/16750 register layout at 0x49020000.
  The omap_uart.c in upstream QEMU (OMAP1, hw/char/omap_uart.c) wraps
  exactly this — it's a 16750-compatible core with OMAP-specific extensions.
  Linux's `omap-serial` driver is 16550A-compatible at the base level.

**Mitigation for INTC mismatch:**
  Write a minimal OMAP3 INTC model (96 lines, SIR register, MIR0-3,
  ILR0-95). This is ~400 lines of C and is the highest-leverage component.
  The upstream OMAP1 INTC model in omap1.c can be adapted.

**Assessment:** Highest maintainability long-term. More upfront work but
  produces code mergeable into QEMU mainline eventually.

**Verdict:** Recommended approach for all new code.

### Approach C: Use Old QEMU with OMAP3 (QEMU 7.x or older)

**Investigation result:** There is no QEMU 7.x or 8.x with OMAP3.

OMAP3 was NEVER in upstream QEMU at any version. Using an old QEMU
buys you nothing — you would still need the Linaro fork.

The Linaro fork's last release (2014.01) is based on QEMU ~1.7.
There is no "QEMU 7.x with OMAP3" — the version numbers suggest
current QEMU (7.x = 2022, 8.x = 2023) which never had OMAP3.

If the goal is "oldest QEMU with OMAP3", the answer is Linaro QEMU 2014.01,
which IS available and compilable (with minor fixes — see dougg3 fork).

**When Approach C makes sense:**
- Quick smoke test: does the E7 firmware do anything on beaglexm?
- Validating the OMAP3 register map against our firmware RE
- Getting a working boot to verify Linux DTS before porting to modern QEMU

**Verdict:** Use for early validation only. Build Linaro QEMU alongside
  the new machine work. Treat it as a reference implementation.

---

## 4. Recommended Approach

**Primary: New machine definition targeting QEMU 10.x, with OMAP3-compatible
  peripheral implementations, modeled after the Allwinner A10 / cubieboard
  SoC+machine split pattern.**

**Parallel: Linaro QEMU 2014.01 built as test oracle.**

### 4.1 Rationale

1. The cubieboard / allwinner-a10 pattern (hw/arm/allwinner-a10.c +
   hw/arm/cubieboard.c) demonstrates the exact structure we need:
   - Separate SoC object (`TYPE_AW_A10`) with child devices
   - Machine file instantiates SoC, maps RAM, loads kernel/DTB
   - UART via `serial_mm_init()` at fixed address
   - Interrupt controller mapped to fixed address, wired to CPU IRQ/FIQ

2. The existing upstream OMAP1 peripheral drivers (omap_uart.c, omap_i2c.c,
   hw/arm/omap1.c INTC logic) can be adapted — they already implement the
   correct register layouts for OMAP-series hardware.

3. A modern QEMU base gives us:
   - Correct Cortex-A8 CPU model (arm-cortex-a8-cpu)
   - Working MMU, caches, VFPv3/NEON
   - virtio block as placeholder for eMMC until real HSMMC model exists
   - Network via virtio-net or e1000 for development convenience
   - Working GDB stub for kernel debugging

### 4.2 Key Decision: OMAP3 INTC

The OMAP3 INTC (at 0x48200000, 96 lines) MUST be modeled correctly because:
- Linux OMAP3 kernel boots with `ti,omap3-intc` compatible driver
- The SIR register protocol (read active IRQ, then write NEWIRQAGR) is
  not compatible with GIC or VIC semantics
- Errata i540 (autoidle can stall INTC) affects driver behavior

We write a minimal `hw/intc/omap3_intc.c`. This is ~500 lines.
Registers to implement for Phase 1 boot:

```
Offset  Register         Function
0x000   INTC_REVISION    Read-only version (return 0x40 for rev 4.0)
0x010   INTC_SYSCONFIG   Softreset / autoidle
0x014   INTC_SYSSTATUS   Reset done status
0x040   INTC_SIR_IRQ     Active IRQ source number
0x044   INTC_SIR_FIQ     Active FIQ source number
0x048   INTC_CONTROL     New IRQ / FIQ acknowledge
0x04C   INTC_PROTECTION  Protection enable
0x050   INTC_IDLE        Idle mode / turbo mode (errata i540 target)
0x060   INTC_IRQ_PRIORITY Active IRQ priority
0x064   INTC_FIQ_PRIORITY Active FIQ priority
0x080   INTC_THRESHOLD   Priority threshold
0x084   INTC_MIR0        Mask IRQs 0-31  (write 1 = mask)
0x088   INTC_MIR_CLEAR0  Clear mask IRQs 0-31
0x08C   INTC_MIR_SET0    Set mask IRQs 0-31
0x090   INTC_ISR_SET0    Set software IRQ 0-31
0x094   INTC_ISR_CLEAR0  Clear software IRQ 0-31
0x098   INTC_PENDING_IRQ0 Pending IRQ 0-31 (after masking)
0x0A0   INTC_ITR0        Raw interrupt input 0-31
0x0A4   INTC_MIR1        Mask IRQs 32-63
... (repeat pattern × 3 for 96 lines)
0x100   INTC_ILR0        IRQ 0: priority [6:2], FIQ/IRQ routing [0]
... (96 entries × 4 bytes)
```

### 4.3 Key Decision: UART

Use `hw/char/omap_uart.c` from upstream QEMU (OMAP1, still present as of
QEMU 10.x). It implements a 16750-compatible UART with OMAP-specific
extension registers (EBLR, SYSCONTROL, WER, CFPS, MDR1/MDR2).

The Linux `omap-serial` driver (`drivers/tty/serial/omap-serial.c`) probes
these extension registers. For Phase 1 (console only), the 16750 core
suffices — Linux's earlycon does not probe extensions.

Alternative: `serial_mm_init()` with `DEVICE_LITTLE_ENDIAN` at 0x49020000,
stride 4 (OMAP registers are 32-bit aligned). This requires zero new code
and will work for U-Boot and Linux earlycon. Upgrade to full omap_uart later.

### 4.4 Key Decision: Timer

Linux OMAP3 requires at least one GP timer for the clockevent source.
The 32K sync timer (OMAP3_32KSYNCNT at 0x48320000) is used as clocksource.

For Phase 1, implement:
1. GPTIMER1 (0x48318000, IRQ 37): clockevent (OMAP3 Linux default)
2. 32K sync counter (0x48320000): free-running 32768 Hz counter

The GPTIMER register set is well-documented (TI TRM chapter 16).
Minimal registers for a working clockevent:

```
0x010  TIOCP_CFG   — softreset, idle mode
0x014  TISTAT      — reset done
0x018  TISR        — interrupt status (OVF, CAPT, MAT bits)
0x01C  TIER        — interrupt enable
0x024  TCLR        — control: start/stop, autoreload, compare enable
0x028  TCRR        — counter register (current value)
0x02C  TLDR        — reload value
0x030  TTGR        — trigger reload
0x034  TWPS        — write posted status
0x038  TMAR        — compare value
0x040  TSICR       — synchronization interface control
0x044  TCAR1       — capture register 1
```

---

## 5. OMAP3630 Memory Map

Complete memory map derived from OMAP3630 TRM + firmware RE + omap3.dtsi.
All addresses confirmed against firmware observations where noted.

```
Address Range       Size    Device                    Confirmed
─────────────────────────────────────────────────────────────────
0x00000000-0x3FFFFFFF  1GB  Reserved / unmapped
0x40000000-0x400FFFFF  1MB  SRAM (internal, 64KB used)
  0x40014000              Boot ROM (Linaro model)
  0x4020ffc8              SRAM exception vectors
0x48000000-0x48FFFFFF  16MB  L4 Core interconnect
  0x48002000    4KB  SCM / PADCONF (pin mux)      [confirmed]
  0x48004000    4KB  CM (Clock Management)        [confirmed]
  0x48005000    4KB  CM continued
  0x48020000    4KB  L4 Core IA
  0x48040000    4KB  USB OTG HS
  0x48056000    4KB  SDMA (System DMA)
  0x48060000    4KB  I2C3 (IRQ 61)
  0x48064000    4KB  USB HSOTG
  0x48070000    4KB  I2C1 (IRQ 56)
  0x48072000    4KB  I2C2 (IRQ 57)
  0x4809C000    4KB  HSMMC1 (eMMC)               [confirmed]
  0x480AC000    4KB  HSMMC2 (external SD)
  0x480B4000    4KB  HSMMC3 (WL1273 WiFi)
  0x48200000    4KB  MPU INTC (96 IRQs)           [confirmed by dtsi]
  0x48306000    4KB  PRM_IVA2                     [confirmed]
  0x48307000    4KB  PRM (Power & Reset CM)
  0x48310000    4KB  GPIO1 (IRQ 29)
  0x48318000    4KB  GPTIMER1 (IRQ 37)
  0x48320000    4KB  32K sync counter
0x49000000-0x490FFFFF  1MB  L4 Peripheral interconnect
  0x49020000    4KB  UART3 (console, IRQ 74)      [confirmed]
  0x49032000    4KB  GPTIMER2 (IRQ 38)            [confirmed]
  0x49042000    4KB  UART4 (IRQ 70)               [confirmed]
  0x49050000    4KB  GPIO2 (IRQ 30)
  0x49052000    4KB  GPIO3 (IRQ 31)
  0x49054000    4KB  GPIO4 (IRQ 32)
  0x49056000    4KB  GPIO5 (IRQ 33)
  0x49058000    4KB  GPIO6 (IRQ 34)
0x50000000-0x5FFFFFFF  256MB  GPMC (General Purpose MC) [confirmed: 909 hits]
  0x50000000          GPMC CS0: OneNAND flash
  (BCM2727 ISP also mapped here — CCP2 interface, specific CS TBD)
0x68000000-0x6FFFFFFF  128MB  SDRC registers + CS regions
  0x6D000000    4KB  SDRC (SDRAM controller)
  0x6E000000    4KB  GPMC registers (config space)
0x80000000-0x8FFFFFFF  256MB  DDR SDRAM (E7: 256MB)     [confirmed]
```

### 5.1 QEMU Machine Memory Regions

For the machine definition, these `memory_region_add_subregion()` calls are needed:

```c
/* Phase 1 minimum */
memory_region_add_subregion(sysmem, 0x80000000, ram);     // 256MB DDR
memory_region_add_subregion(sysmem, 0x48200000, intc);    // OMAP3 INTC
memory_region_add_subregion(sysmem, 0x49020000, uart3);   // UART3
memory_region_add_subregion(sysmem, 0x48318000, gpt1);    // GPTIMER1
memory_region_add_subregion(sysmem, 0x48320000, sync32k); // 32K counter

/* Phase 1 stubs (read-as-zero, ignore writes) */
memory_region_add_subregion(sysmem, 0x48004000, cm_stub);   // CM
memory_region_add_subregion(sysmem, 0x48307000, prm_stub);  // PRM

/* Phase 2 */
memory_region_add_subregion(sysmem, 0x4809C000, hsmmc1);  // eMMC
memory_region_add_subregion(sysmem, 0x50000000, gpmc);    // NAND/OneNAND

/* Phase 3 */
memory_region_add_subregion(sysmem, 0x48070000, i2c1);
memory_region_add_subregion(sysmem, 0x48310000, gpio1);
// ... remaining GPIO, I2C, timers
```

---

## 6. Machine Definition Architecture

### 6.1 File Structure

Following the cubieboard/allwinner-a10 pattern:

```
QEMU source tree additions:

hw/arm/nokia_e7.c          — board machine definition
hw/arm/omap3630.c          — OMAP3630 SoC object
include/hw/arm/omap3630.h  — SoC header

hw/intc/omap3_intc.c       — OMAP3 interrupt controller (NEW)
include/hw/intc/omap3_intc.h

hw/timer/omap3_gptimer.c   — OMAP3 GP timer (NEW, adapted from old omap_gptimer.c)
hw/timer/omap3_32ktimer.c  — OMAP3 32K sync counter (NEW)

hw/sd/omap3_mmc.c          — OMAP3 HSMMC (Phase 2, adapted from omap3_mmc.c)
hw/mtd/omap3_onenand.c     — OneNAND over GPMC (Phase 2)

hw/arm/Kconfig             — add NOKIA_E7 config symbol
hw/arm/meson.build         — add nokia_e7.c and omap3630.c
hw/intc/meson.build        — add omap3_intc.c
hw/timer/meson.build       — add omap3_gptimer.c, omap3_32ktimer.c
docs/system/arm/nokia-e7.rst  — board documentation
```

### 6.2 SoC Object (omap3630.c)

Pattern: identical to `hw/arm/allwinner-a10.c`.

```c
// include/hw/arm/omap3630.h
#define TYPE_OMAP3630  "omap3630"
OBJECT_DECLARE_SIMPLE_TYPE(OMAP3630State, OMAP3630)

#define OMAP3630_UART3_BASE   0x49020000
#define OMAP3630_INTC_BASE    0x48200000
#define OMAP3630_GPT1_BASE    0x48318000
#define OMAP3630_SYNC32K_BASE 0x48320000
#define OMAP3630_I2C1_BASE    0x48070000
#define OMAP3630_I2C2_BASE    0x48072000
#define OMAP3630_I2C3_BASE    0x48060000
#define OMAP3630_HSMMC1_BASE  0x4809C000
#define OMAP3630_CM_BASE      0x48004000
#define OMAP3630_PRM_BASE     0x48307000

typedef struct OMAP3630State {
    DeviceState parent_obj;

    /* CPU */
    ARMCPU          cpu;

    /* Interrupt controller */
    OMAP3INTCState  intc;

    /* Timers */
    OMAP3GPTimerState gpt1;
    OMAP3GPTimerState gpt2;
    MemoryRegion    sync32k_iomem;

    /* Serial */
    /* uart3 via serial_mm_init() in Phase 1 */

    /* I2C */
    OMAPI2CState    i2c[3];

    /* GPIO */
    /* Phase 3 */

    /* Storage */
    /* hsmmc1 in Phase 2 */

    /* Clock/Power stubs */
    MemoryRegion    cm_iomem;
    MemoryRegion    prm_iomem;
} OMAP3630State;
```

### 6.3 Machine Definition (nokia_e7.c)

```c
// hw/arm/nokia_e7.c — abridged structure

#define NOKIA_E7_RAM_SIZE   (256 * MiB)
#define NOKIA_E7_RAM_BASE   0x80000000

static void nokia_e7_init(MachineState *machine)
{
    OMAP3630State *soc;
    MemoryRegion *sysmem = get_system_memory();
    MemoryRegion *ram    = g_new(MemoryRegion, 1);

    /* RAM at 0x80000000 */
    memory_region_init_ram(ram, NULL, "nokia-e7.ram",
                           machine->ram_size, &error_fatal);
    memory_region_add_subregion(sysmem, NOKIA_E7_RAM_BASE, ram);

    /* SoC object */
    soc = OMAP3630(object_new(TYPE_OMAP3630));
    object_property_set_bool(OBJECT(soc), "realize", true, &error_fatal);

    /* Boot: load kernel + DTB, or raw binary */
    nokia_e7_binfo.ram_size         = machine->ram_size;
    nokia_e7_binfo.loader_start     = NOKIA_E7_RAM_BASE;
    nokia_e7_binfo.board_id         = 0xffffffff;  /* DT only */
    nokia_e7_binfo.dtb_filename     = machine->dtb;
    arm_load_kernel(&soc->cpu, machine, &nokia_e7_binfo);
}

static void nokia_e7_machine_init(MachineClass *mc)
{
    mc->desc            = "Nokia E7 (RM-626, OMAP3630 Cortex-A8)";
    mc->init            = nokia_e7_init;
    mc->default_cpu_type = ARM_CPU_TYPE_NAME("cortex-a8");
    mc->default_ram_size = NOKIA_E7_RAM_SIZE;
    mc->ignore_memory_transaction_failures = true;  /* Phase 1 stubs */
}

DEFINE_MACHINE("nokia-e7", nokia_e7_machine_init)
```

### 6.4 OMAP3 INTC (omap3_intc.c) — Skeleton

```c
// hw/intc/omap3_intc.c

#define OMAP3_INTC_NR_IRQS   96
#define INTC_REVISION        0x000
#define INTC_SYSCONFIG       0x010
#define INTC_SYSSTATUS       0x014
#define INTC_SIR_IRQ         0x040
#define INTC_SIR_FIQ         0x044
#define INTC_CONTROL         0x048  /* write 1 to ACK new IRQ, 2 to ACK FIQ */
#define INTC_PROTECTION      0x04C
#define INTC_IDLE            0x050  /* bit 0 = turbo, bit 1 = autoidle */
#define INTC_IRQ_PRIORITY    0x060
#define INTC_FIQ_PRIORITY    0x064
#define INTC_THRESHOLD       0x080
/* Register bank offsets: 0x084 + n*0x20 for bank n (0,1,2) */
#define INTC_MIR(n)          (0x084 + (n)*0x20)
#define INTC_MIR_CLEAR(n)    (0x088 + (n)*0x20)
#define INTC_MIR_SET(n)      (0x08C + (n)*0x20)
#define INTC_ISR_SET(n)      (0x090 + (n)*0x20)
#define INTC_ISR_CLEAR(n)    (0x094 + (n)*0x20)
#define INTC_PENDING_IRQ(n)  (0x098 + (n)*0x20)
#define INTC_ITR(n)          (0x0A0 + (n)*0x20)
/* ILR: 96 entries × 4 bytes starting at 0x100 */
#define INTC_ILR(irq)        (0x100 + (irq)*4)
```

Key protocol the Linux omap-intc driver uses:
1. On IRQ entry: read `INTC_SIR_IRQ` → get active IRQ number
2. Service the IRQ
3. Write 0x1 to `INTC_CONTROL` → signal NEWIRQAGR (new IRQ agree)

The QEMU model must:
- Maintain a priority queue of pending, unmasked IRQs
- Update the `INTC_SIR_IRQ` value when IRQ line goes high
- Pulse `qemu_irq` connected to CPU's `ARM_CPU_IRQ` input
- Clear `ARM_CPU_IRQ` when NEWIRQAGR is written and no more pending IRQs

### 6.5 Kconfig Entry

```kconfig
# hw/arm/Kconfig
config NOKIA_E7
    bool
    default y
    depends on TCG && ARM
    select OMAP3630
    select OMAP3_INTC

config OMAP3630
    bool
    select ARM_V7
    select OMAP3_INTC
    select OMAP_UART      # hw/char/omap_uart.c (OMAP1, still present)
    select OMAP_I2C       # hw/i2c/omap_i2c.c (OMAP1, still present)

config OMAP3_INTC
    bool
```

---

## 7. Phased Emulation Plan

### Phase 1: Boot to Serial Console

**Goal:** Kernel prints boot messages to QEMU's stdio. No storage, no
  network required. Validates CPU, memory, INTC, timer, UART.

**QEMU components required:**
- [x] ARM Cortex-A8 CPU (already in QEMU)
- [ ] 256MB RAM at 0x80000000
- [ ] OMAP3 INTC at 0x48200000 (new: omap3_intc.c, ~500 lines)
- [ ] UART3 at 0x49020000 (reuse omap_uart.c OR serial_mm_init)
- [ ] GPTIMER1 at 0x48318000, IRQ 37 (new: omap3_gptimer.c, ~300 lines)
- [ ] 32K sync counter at 0x48320000 (read-only free-running counter, ~50 lines)
- [ ] CM/PRM stub at 0x48004000/0x48307000 (ignore-write, return 0)

**Linux kernel config:**
```
CONFIG_ARCH_OMAP3=y
CONFIG_OMAP3_EMU=y (optional but useful)
CONFIG_SERIAL_OMAP=y
CONFIG_SERIAL_OMAP_CONSOLE=y
CONFIG_OMAP_32K_TIMER=y
CONFIG_HZ=100         (reduces timer IRQ load)
```

**Boot command line:**
```bash
qemu-system-arm \
  -M nokia-e7 \
  -m 256M \
  -kernel arch/arm/boot/zImage \
  -dtb arch/arm/boot/dts/nokia-e7.dtb \
  -append "console=ttyO2,115200n8 earlyprintk=ttyO2,115200n8 ignore_loglevel" \
  -nographic \
  -serial stdio
```

Note: OMAP UART console is `ttyO2` (capital-O, not zero) for UART3.
For earlycon: `earlycon=omap8250,0x49020000,115200n8`

**Device tree (minimum for Phase 1):**
```dts
/ {
    compatible = "nokia,rm626", "ti,omap3630", "ti,omap3";
    model = "Nokia E7 RM-626";
    #address-cells = <1>;
    #size-cells = <1>;

    memory@80000000 {
        device_type = "memory";
        reg = <0x80000000 0x10000000>;  /* 256MB */
    };

    chosen {
        bootargs = "console=ttyO2,115200n8";
    };

    cpus {
        #address-cells = <1>;
        #size-cells = <0>;
        cpu@0 {
            compatible = "arm,cortex-a8";
            device_type = "cpu";
        };
    };

    intc: interrupt-controller@48200000 {
        compatible = "ti,omap3-intc";
        interrupt-controller;
        #interrupt-cells = <1>;
        reg = <0x48200000 0x1000>;
        ti,intc-size = <96>;
    };

    uart3: serial@49020000 {
        compatible = "ti,omap3-uart";
        reg = <0x49020000 0x400>;
        interrupts = <74>;
        interrupt-parent = <&intc>;
        clocks = <&uart3_fck>;
        clock-names = "fck";
        ti,hwmods = "uart3";
    };

    timer1: timer@48318000 {
        compatible = "ti,omap3430-timer";
        reg = <0x48318000 0x80>;
        interrupts = <37>;
        interrupt-parent = <&intc>;
        ti,timer-alwon;
    };

    counter32k: counter@48320000 {
        compatible = "ti,omap-counter32k";
        reg = <0x48320000 0x20>;
    };
};
```

**Expected boot sequence with Linaro QEMU (validation):**
1. Boot ROM (omap3_boot.c) initializes SDRAM
2. x-loader / MLO loaded from NAND/MMC
3. U-Boot SPL → U-Boot → zImage
4. Linux starts, INTC initialized, UART3 console active
5. Prompt

**Expected Phase 1 with new machine (abbreviated):**
- Skip boot ROM (load zImage directly)
- Linux kernel decompresses at 0x80000000 + offset
- INTC probed (driver reads 0x48200000, expects revision 4.0)
- UART3 probed (omap-serial driver initializes)
- GPTIMER1 probed (clockevent driver)
- 32K counter probed (clocksource driver)
- Boot messages on stdio

**Estimated effort:** 3-5 days for a minimally working Phase 1.

---

### Phase 2: Block Storage (eMMC + OneNAND)

**Goal:** Kernel can mount rootfs from eMMC image. Validates HSMMC and
  optionally OneNAND.

**Option 2A (recommended): virtio-blk placeholder**
Use QEMU's virtio-mmio or a raw SD card model as a proxy for eMMC.
The Linux kernel doesn't care if the block device is "real" HSMMC or
virtio-blk, as long as it can mount a filesystem. This requires:
- Adding `virtio-mmio` transport at an unused address OR
- Using `-drive if=sd,file=rootfs.img` with a placeholder MMC model

This approach lets Phase 2 proceed WITHOUT writing an HSMMC model.

**Option 2B: Full OMAP3 HSMMC model**
Port `omap3_mmc.c` from the Linaro fork to modern QEMU's SD bus API.
The HSMMC register set is documented in the OMAP3630 TRM (Chapter 20).
The `hw/sd/sdhci.c` in modern QEMU implements SDHCI 2.0 — this is
architecturally similar to OMAP3 HSMMC but register-incompatible.

A minimal HSMMC stub that makes Linux happy:
- Responds to OMAP3 HSMMC probing (returns capability registers)
- Implements data transfer at the HSMMC register level
- Delegates actual data to the SD card model (`TYPE_SD_CARD`)

**Option 2C: OneNAND**
The OneNAND model was removed with OMAP2 in QEMU 9.2 (part of the
53-patch series). It must be reimplemented or the Linaro version ported.
The GPMC + OneNAND combination requires:
- GPMC register model (config registers at 0x6E000000)
- OneNAND command/status registers at 0x50000000 (CS0)
- NAND BBT (bad block table) emulation for MTD layer

**Recommendation for Phase 2:** Use virtio-blk proxy first, write HSMMC
  model in parallel.

**QEMU command for Phase 2 with virtio-blk:**
```bash
qemu-system-arm \
  -M nokia-e7 \
  -m 256M \
  -kernel arch/arm/boot/zImage \
  -dtb arch/arm/boot/dts/nokia-e7.dtb \
  -append "console=ttyO2,115200n8 root=/dev/vda rw" \
  -drive file=rootfs.ext4,if=virtio,format=raw \
  -nographic
```

**Phase 2 DTS additions:**
```dts
mmc1: mmc@4809c000 {
    compatible = "ti,omap3-hsmmc";
    reg = <0x4809c000 0x200>;
    interrupts = <83>;
    interrupt-parent = <&intc>;
    ti,hwmods = "mmc1";
    bus-width = <8>;
    ti,non-removable;  /* eMMC: non-removable */
};
```

---

### Phase 3: I2C and GPIO

**Goal:** TWL5031 PMIC accessible on I2C bus 1. GPIO banks probed.
  Keyboard matrix functional.

**OMAP3 I2C:** `hw/i2c/omap_i2c.c` is present in upstream QEMU (OMAP1).
  This driver supports OMAP2/3 protocol. Adaptation needed:
  - Map to correct addresses (I2C1 at 0x48070000, IRQ 56)
  - Wire to I2C bus where TWL5031 stub lives

**TWL5031 PMIC stub:** Minimal I2C device that:
  - ACKs its address (0x48 on I2C bus 1)
  - Returns plausible values for power/clock registers
  - Does NOT need to implement the full PMIC feature set

**GPIO:** 6 banks (0x48310000, 0x49050000-0x49058000).
  The `hw/gpio/omap_gpio.c` in upstream QEMU targets OMAP1.
  OMAP2/3 GPIO is similar but not identical (different revision register).
  Port or rewrite for OMAP3 addressing.

**E7-specific GPIO:**
  From firmware RE, identify which GPIO pins control:
  - Keyboard scan lines (rows/cols)
  - Camera enable/reset
  - Display enable
  - LED control

**Keyboard matrix:**
  Model as a GPIO-scanning keyboard. QEMU's `hw/input/matrix_keymap.c`
  provides infrastructure for keyboard matrix emulation.

**Phase 3 QEMU command:**
```bash
qemu-system-arm \
  -M nokia-e7 \
  -m 256M \
  -kernel arch/arm/boot/zImage \
  -dtb arch/arm/boot/dts/nokia-e7.dtb \
  -append "console=ttyO2,115200n8 root=/dev/mmcblk0p2 rw" \
  -drive file=emmc.img,if=sd,format=raw \
  -nographic
```

---

### Phase 4: BCM2727 Stub (Camera ISP)

**Goal:** BCM2727 present on CCP2/GPMC interface. Firmware can load and
  probe the camera subsystem without kernel panic.

**Background:**
The Broadcom BCM2727 is a VideoCore III-based camera ISP. It connects to
the OMAP3630 via CCP2 (Camera Parallel Port 2) for image data and likely
uses GPMC for control registers or a secondary SPI/I2C for configuration.

Our firmware RE shows 909 unique GPMC hits (0x50xxxxxx), suggesting the
BCM2727 occupies a GPMC chip-select region. The exact CS# and address
range needs further analysis.

**Approach:**
1. Identify BCM2727 GPMC CS from firmware disassembly
2. Create a stub MMIO device at that GPMC window
3. Respond to probe reads with plausible chip-ID values
4. Log all accesses for further RE work
5. Implement minimum command set so the firmware "initializes" it

**Phase 4 stub structure:**
```c
// hw/misc/bcm2727_stub.c
static uint64_t bcm2727_read(void *opaque, hwaddr addr, unsigned size)
{
    BCM2727State *s = opaque;
    qemu_log_mask(LOG_UNIMP, "BCM2727: read @ 0x%04" HWADDR_PRIx "\n", addr);
    switch (addr) {
    case 0x00:  return 0x2727;  /* chip ID */
    case 0x02:  return 0x0001;  /* revision */
    default:    return 0x0000;
    }
}
```

**Phase 4 is non-blocking** — the kernel's camera driver can be built as
a module and not loaded until BCM2727 emulation is ready. Phases 1-3 do
not depend on Phase 4.

---

## 8. Build and Integration

### 8.1 Building Linaro QEMU (Reference/Validation)

```bash
# Clone the maintained mirror with build fixes
git clone https://github.com/dougg3/qemu-linaro-beagleboard.git
cd qemu-linaro-beagleboard

# Ubuntu 22.04 dependencies (from dougg3 README):
sudo apt install build-essential libglib2.0-dev libfdt-dev \
    libpixman-1-dev zlib1g-dev libsdl2-dev git python3 ninja-build

# Configure for ARM only (faster build)
./configure --target-list=arm-softmmu \
    --enable-sdl \
    --disable-werror

make -j$(nproc)

# Run beaglexm (closest to E7):
./arm-softmmu/qemu-system-arm \
  -M beaglexm \
  -m 256 \
  -drive file=e7-mmc.img,if=sd,format=raw \
  -serial stdio

# Run with upstream Linux kernel (closest DTS = omap3-beagle-xm.dts):
./arm-softmmu/qemu-system-arm \
  -M beaglexm \
  -m 256 \
  -kernel path/to/zImage \
  -dtb path/to/omap3-beagle-xm.dtb \
  -append "console=ttyO2,115200n8" \
  -serial stdio
```

Linaro QEMU source for critical files:
```
https://github.com/av86743/qemu-linaro-clone/blob/master/hw/omap3.c
https://github.com/av86743/qemu-linaro-clone/blob/master/hw/omap3_boot.c
https://github.com/av86743/qemu-linaro-clone/blob/master/hw/omap3_mmc.c
https://github.com/av86743/qemu-linaro-clone/blob/master/hw/beagle.c
```

### 8.2 Building Modern QEMU with Nokia E7 Machine

```bash
git clone https://gitlab.com/qemu-project/qemu.git
cd qemu
git submodule update --init --recursive

# Add new files (from this design doc):
# hw/arm/nokia_e7.c
# hw/arm/omap3630.c
# include/hw/arm/omap3630.h
# hw/intc/omap3_intc.c
# hw/timer/omap3_gptimer.c
# hw/timer/omap3_32ktimer.c

# Update build files:
# hw/arm/Kconfig           — add NOKIA_E7 and OMAP3630
# hw/arm/meson.build       — add nokia_e7.c, omap3630.c
# hw/intc/meson.build      — add omap3_intc.c
# hw/timer/meson.build     — add omap3_gptimer.c, omap3_32ktimer.c

mkdir build && cd build
../configure \
  --target-list=arm-softmmu \
  --enable-debug \
  --disable-werror

make -j$(nproc)

# Test:
./qemu-system-arm -M nokia-e7 -m 256M -nographic \
  -kernel path/to/zImage -dtb path/to/nokia-e7.dtb \
  -append "console=ttyO2,115200n8 ignore_loglevel"
```

### 8.3 meson.build Additions

```python
# hw/arm/meson.build — add to arm_ss sources
arm_ss.add(when: 'CONFIG_NOKIA_E7', if_true: files(
    'nokia_e7.c',
    'omap3630.c',
))

# hw/intc/meson.build
softmmu_ss.add(when: 'CONFIG_OMAP3_INTC', if_true: files('omap3_intc.c'))

# hw/timer/meson.build
softmmu_ss.add(when: 'CONFIG_OMAP3630', if_true: files(
    'omap3_gptimer.c',
    'omap3_32ktimer.c',
))
```

---

## 9. Linux Kernel Configuration

### 9.1 Kernel Version Recommendation

Use Linux 6.6 LTS or later. OMAP3 support is in `arch/arm/mach-omap2/`
and has been stable for many years. The `omap2plus_defconfig` provides
a reasonable starting point.

```bash
# Build OMAP3 kernel
make ARCH=arm CROSS_COMPILE=arm-linux-gnueabihf- omap2plus_defconfig
# Enable OMAP3630 and Nokia-specific options:
make ARCH=arm menuconfig
  → System Type → TI OMAP2/3/4 Multiplatform support → Enable OMAP3 support
  → Device Drivers → Character devices → Serial → OMAP serial support
  → Device Drivers → Block → MMC/SDIO → OMAP HSMMC support
make ARCH=arm CROSS_COMPILE=arm-linux-gnueabihf- zImage dtbs -j$(nproc)
```

### 9.2 Device Tree Source Location

```
arch/arm/boot/dts/nokia-e7.dts          (new file, we write this)
arch/arm/boot/dts/omap36xx.dtsi         (exists, OMAP3630 base)
arch/arm/boot/dts/omap3.dtsi            (exists, OMAP3 common)
arch/arm/boot/dts/omap3-beagle-xm.dts  (reference: also OMAP3630+256MB)
```

The omap3-beagle-xm.dts is structurally almost identical to what we need
for Phase 1. Key differences for E7:
- Model string: `"Nokia E7 RM-626"`
- Compatible: `"nokia,rm626", "ti,omap3630", "ti,omap3"`
- RAM: 256MB (same as beagle, not 512MB)
- eMMC in `mmc1`: `ti,non-removable`, `bus-width = <8>`
- No SD card slot in `mmc2`
- TWL5031 on I2C bus 1 instead of TWL4030

### 9.3 Kernel Boot Arguments

```
Phase 1 (UART console only):
  console=ttyO2,115200n8 earlyprintk=ttyO2,115200n8 ignore_loglevel
  earlycon=omap8250,0x49020000,115200n8

Phase 2 (with rootfs):
  console=ttyO2,115200n8 root=/dev/mmcblk0p2 rw rootfstype=ext4
  rootwait init=/bin/sh

Phase 2 with virtio-blk:
  console=ttyO2,115200n8 root=/dev/vda rw rootfstype=ext4 rootwait
```

### 9.4 Useful Kernel Debug Options

```
# Add to kernel config for debugging:
CONFIG_DYNAMIC_DEBUG=y
CONFIG_DEBUG_LL=y           # Low-level printk before UART driver init
CONFIG_DEBUG_OMAP_UART=y    # OMAP UART debug output
CONFIG_KALLSYMS=y
CONFIG_KGDB=y               # Kernel debugger via QEMU GDB stub
CONFIG_KGDB_SERIAL_CONSOLE=y
CONFIG_PROVE_LOCKING=y      # Lock dependency checker
```

---

## 10. Testing Strategy

### 10.1 Validation Against Linaro QEMU

Run the same kernel + DTS on both machines, compare output:

```bash
# Linaro QEMU (reference):
./qemu-linaro/arm-softmmu/qemu-system-arm -M beaglexm -m 256 \
  -kernel zImage -dtb omap3-beagle-xm.dtb \
  -append "console=ttyO2,115200n8" -nographic \
  2>&1 | tee boot-linaro.log

# New machine (target):
./qemu-modern/build/qemu-system-arm -M nokia-e7 -m 256M \
  -kernel zImage -dtb nokia-e7.dtb \
  -append "console=ttyO2,115200n8" -nographic \
  2>&1 | tee boot-modern.log

diff boot-linaro.log boot-modern.log
```

### 10.2 INTC Register Trace Test

Write a minimal ARM bare-metal test that:
1. Programs GPTIMER1 to overflow every 1 ms
2. Enables GPTIMER1 interrupt in INTC
3. Counts IRQ firings, prints via UART3
4. Verifies correct SIR_IRQ values on entry

This tests the INTC model without Linux's driver abstractions.

```c
// test/intc_test.c — bare metal
void irq_handler(void) {
    uint32_t sir = *(volatile uint32_t*)0x48200040;  // INTC_SIR_IRQ
    if ((sir & 0x7F) == 37) {  // IRQ 37 = GPTIMER1
        irq_count++;
        *(volatile uint32_t*)0x48318018 |= 0x2;   // GPTIMER TISR: clear OVF
        *(volatile uint32_t*)0x48200048 = 0x1;    // INTC_CONTROL: NEWIRQAGR
    }
}
```

### 10.3 GDB-Based Debugging

```bash
# Start QEMU with GDB server:
qemu-system-arm -M nokia-e7 -m 256M \
  -kernel zImage -dtb nokia-e7.dtb \
  -nographic -S -gdb tcp::1234

# Attach GDB in another terminal:
arm-linux-gnueabihf-gdb vmlinux
(gdb) target remote :1234
(gdb) b start_kernel
(gdb) c
```

### 10.4 QEMU Tracing for Peripheral Debug

```bash
# Trace OMAP3 INTC register accesses:
qemu-system-arm -M nokia-e7 -m 256M \
  -kernel zImage -dtb nokia-e7.dtb \
  -nographic \
  -trace "omap3_intc_read,omap3_intc_write,omap3_gptimer_*"
```

Add trace events to each new device:
```c
// trace-events for hw/intc/omap3_intc.c:
omap3_intc_read(uint32_t addr, uint32_t val) "addr=0x%04x val=0x%08x"
omap3_intc_write(uint32_t addr, uint32_t val) "addr=0x%04x val=0x%08x"
omap3_intc_irq_set(int irq, int level) "irq=%d level=%d"
omap3_intc_sir_update(int active) "active_irq=%d"
```

---

## 11. Source References

### QEMU Repository and Files

| File | URL | Notes |
|------|-----|-------|
| `hw/arm/cubieboard.c` | https://github.com/qemu/qemu/blob/master/hw/arm/cubieboard.c | Template for machine definition |
| `hw/arm/allwinner-a10.c` | https://github.com/qemu/qemu/blob/master/hw/arm/allwinner-a10.c | Template for SoC object |
| `hw/char/omap_uart.c` | https://github.com/qemu/qemu/blob/master/hw/char/omap_uart.c | Reusable OMAP UART (OMAP1, still upstream) |
| `hw/i2c/omap_i2c.c` | https://github.com/qemu/qemu/blob/master/hw/i2c/omap_i2c.c | Reusable OMAP I2C (OMAP1, still upstream) |
| `include/hw/arm/omap.h` | https://github.com/qemu/qemu/blob/master/include/hw/arm/omap.h | OMAP1 header, still present |
| `hw/arm/realview.c` | https://github.com/qemu/qemu/blob/master/hw/arm/realview.c | realview-pb-a8 reference (Cortex-A8) |

### Linaro QEMU Fork (Primary OMAP3 Reference)

| File | URL | Notes |
|------|-----|-------|
| `hw/omap3.c` | https://github.com/av86743/qemu-linaro-clone/blob/master/hw/omap3.c | Full OMAP3 SoC model |
| `hw/omap3_boot.c` | https://github.com/av86743/qemu-linaro-clone/blob/master/hw/omap3_boot.c | Boot ROM, register map reference |
| `hw/beagle.c` | https://github.com/av86743/qemu-linaro-clone/blob/master/hw/beagle.c | beagle/beaglexm machine (closest to E7) |
| Fork root | https://github.com/av86743/qemu-linaro-clone | Mirror of git.linaro.org/qemu/qemu-linaro |
| Compilable fork | https://github.com/dougg3/qemu-linaro-beagleboard | With glibc + U-Boot SPL fixes |
| Google Code archive | https://code.google.com/archive/p/qemu-omap3 | Original qemu-omap3 project |

### Linux Kernel OMAP3 Drivers

| File | URL | Notes |
|------|-----|-------|
| `drivers/irqchip/irq-omap-intc.c` | https://github.com/torvalds/linux/blob/master/drivers/irqchip/irq-omap-intc.c | OMAP3 INTC driver — compatible strings, register protocol |
| `arch/arm/boot/dts/omap3.dtsi` | https://github.com/beagleboard/devicetree-source/blob/master/arch/arm/boot/dts/omap3.dtsi | All OMAP3 peripheral addresses and IRQs |
| `arch/arm/boot/dts/omap3-beagle-xm.dts` | https://github.com/beagleboard/devicetree-source/blob/master/arch/arm/boot/dts/omap3-beagle-xm.dts | BeagleXM DTS (closest existing DTS to E7) |

### QEMU Deprecation and Removal History

| Reference | URL | Notes |
|-----------|-----|-------|
| Patch: Drop deprecated ARM boards | https://www.mail-archive.com/qemu-devel@nongnu.org/msg1063507.html | QEMU 9.2 removal of OMAP2 machines |
| Docs: Document removal | https://www.mail-archive.com/qemu-devel@nongnu.org/msg1063516.html | Which boards and SoCs were removed |
| Thread: Future deprecations | https://www.mail-archive.com/qemu-devel@nongnu.org/msg1071227.html | sx1/OMAP1 status, ongoing discussions |
| QEMU removed features | https://www.qemu.org/docs/master/about/removed-features.html | Official removal documentation |
| Patch: Remove omap_gptimer | https://www.mail-archive.com/qemu-devel@nongnu.org/msg1068599.html | Confirms omap_gptimer.c was OMAP2-only |
| Patch: Remove omap_synctimer | https://www.mail-archive.com/qemu-devel@nongnu.org/msg1068594.html | Confirms omap_synctimer.c was OMAP2-only |

### TI Documentation

| Document | Notes |
|----------|-------|
| OMAP3630/OMAP3611 Technical Reference Manual (SPRUGZ1) | Register maps for all peripherals. Chapter 14: INTC, Chapter 16: GPTIMER, Chapter 25: UART, Chapter 20: HSMMC |
| OMAP3630 Data Sheet | Package pinout, electrical specs |

### OMAP3 Device Tree Bindings

| Binding | URL |
|---------|-----|
| OMAP UART | https://www.kernel.org/doc/Documentation/devicetree/bindings/serial/omap_serial.txt |
| OMAP INTC compatible strings | `"ti,omap3-intc"`, `"ti,omap2-intc"` (from irq-omap-intc.c) |
| OMAP3 GP Timer | `"ti,omap3430-timer"` compatible string |
| OMAP3 32K counter | `"ti,omap-counter32k"` compatible string |

---

## Appendix A: Summary Comparison Table

| Aspect | Linaro Fork (Approach A) | Modern QEMU (Approach B) |
|--------|--------------------------|--------------------------|
| Base QEMU version | ~1.7 (2014) | 10.x (2025+) |
| OMAP3 completeness | Full (PRM, CM, 12 timers, 4 UARTs, 3 I2C, 6 GPIO, SDRC, GPMC) | Minimal (write from scratch per phase) |
| Boot ROM emulation | Yes (omap3_boot.c) | No (load kernel directly) |
| OneNAND support | Yes | No (removed, rewrite needed) |
| HSMMC support | Yes (omap3_mmc.c) | No (write or use virtio proxy) |
| Modern QOM | Partial / no | Yes |
| Meson build | No | Yes |
| KVM | No | Yes (irrelevant for ARM target) |
| Security | Outdated (QEMU 1.7) | Maintained |
| Compilation | Needs glibc fixes | Standard |
| Effort to boot Linux | Low (already works on beaglexm) | Medium (write INTC, timer first) |
| Long-term viability | Dead end | Active upstream path |
| Nokia E7 specificity | Needs E7 board file on top | Write E7-native from day one |

**Decision: Approach B (Modern QEMU) with Linaro Fork as reference oracle.**

---

## Appendix B: Register Quick Reference

### OMAP3 INTC (0x48200000)

```
+0x000  INTC_REVISION   ro  0x00000040 (rev 4.0)
+0x010  INTC_SYSCONFIG  rw  softreset[1], autoidle[0]
+0x014  INTC_SYSSTATUS  ro  resetdone[0]
+0x040  INTC_SIR_IRQ    ro  active IRQ[6:0], spurious[29]
+0x044  INTC_SIR_FIQ    ro  active FIQ[6:0]
+0x048  INTC_CONTROL    wo  FIQagree[1], IRQagree[0]
+0x050  INTC_IDLE       rw  turbo[1], autoidle[0]
+0x080  INTC_THRESHOLD  rw  priority threshold
+0x084  INTC_MIR0       rw  mask IRQs 0-31 (1=masked)
+0x088  INTC_MIR_CLR0   wo  clear mask (1=unmask)
+0x08C  INTC_MIR_SET0   wo  set mask (1=mask)
+0x098  INTC_PENDING0   ro  pending after masking
+0x100  INTC_ILR[0..95] rw  per-IRQ: priority[6:2], FIQ routing[0]
```

### OMAP3 GPTIMER (0x48318000 for GP1)

```
+0x010  TIOCP_CFG   rw  softreset[1], idlemode[4:3]
+0x014  TISTAT      ro  resetdone[0]
+0x018  TISR        rw  OVF[1], CAPT[1], MAT[0] (write 1 to clear)
+0x01C  TIER        rw  OVF_EN[1], CAPT_EN[1], MAT_EN[0]
+0x024  TCLR        rw  ST[0]=start, AR[1]=autoreload, CE[6]=compare
+0x028  TCRR        rw  current counter value
+0x02C  TLDR        rw  autoreload value
+0x030  TTGR        wo  trigger reload (write any value)
+0x034  TWPS        ro  write pending status
+0x038  TMAR        rw  compare match value
+0x040  TSICR       rw  posted[2], softreset[1]
```

### OMAP3 UART (0x49020000 for UART3)

16750-compatible base (same as 16550A for earlycon purposes):
```
+0x000  RHR/THR     rw  receive/transmit holding (stride 4)
+0x004  IER         rw  interrupt enable
+0x008  IIR/FCR     rw  interrupt ID / FIFO control
+0x00C  LCR         rw  line control (baud divisor latch access bit 7)
+0x010  MCR         rw  modem control
+0x014  LSR         ro  line status (bit 5 = THR empty, bit 0 = data ready)
+0x018  MSR         ro  modem status
+0x01C  SCR         rw  scratch register
OMAP extensions (when LCR != 0xBF):
+0x020  MDR1        rw  mode definition (0=UART16x, 7=disable)
OMAP extensions (when LCR == 0xBF):
+0x010  EFR         rw  enhanced feature register
```

---

*End of document. See also: `/home/lukas/ps3/e7/docs/emulation-strategy.md`
for the overall emulation philosophy and `/home/lukas/ps3/e7/emulation/`
for implementation artifacts.*
