/*
 * OMAP3630 SoC emulation
 *
 * Implements the TI OMAP3630 as used in the Nokia E7 (RM-626).
 * Phase 1: CPU + INTC + UART3 + GPTIMER1 + 32K sync counter.
 *
 * Follows the allwinner-a10 pattern: SoC object creates and wires
 * all internal peripherals; board object creates the SoC + RAM.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "qemu/osdep.h"
#include "qapi/error.h"
#include "qemu/error-report.h"
#include "qemu/module.h"
#include "hw/arm/omap3630.h"
#include "hw/char/serial-mm.h"
#include "hw/sysbus.h"
#include "hw/irq.h"
#include "hw/misc/unimp.h"
#include "exec/address-spaces.h"
#include "system/system.h"
#include "qemu/log.h"
#include "target/arm/cpu-qom.h"
#include "ui/input.h"

/*
 * 32K Sync Timer — trivial free-running counter at 32768 Hz.
 * Single read-only register at offset 0x010.
 */
static uint64_t omap3630_sync32k_read(void *opaque, hwaddr offset,
                                      unsigned size)
{
    if (offset == 0x000) {
        /* REVISION register */
        return 0x00000031;  /* OMAP3 sync timer revision */
    }
    if (offset == 0x004) {
        /* SYSCONFIG */
        return 0;
    }
    if (offset == 0x010) {
        /* CR: 32-bit counter value */
        int64_t now = qemu_clock_get_ns(QEMU_CLOCK_VIRTUAL);
        return (uint32_t)muldiv64(now, 32768, NANOSECONDS_PER_SECOND);
    }
    qemu_log_mask(LOG_GUEST_ERROR,
                  "omap3630_sync32k: bad read offset 0x%03x\n", (int)offset);
    return 0;
}

static void omap3630_sync32k_write(void *opaque, hwaddr offset,
                                   uint64_t value, unsigned size)
{
    /* Read-only peripheral, ignore writes */
    qemu_log_mask(LOG_GUEST_ERROR,
                  "omap3630_sync32k: write to read-only offset 0x%03x\n",
                  (int)offset);
}

static const MemoryRegionOps omap3630_sync32k_ops = {
    .read = omap3630_sync32k_read,
    .write = omap3630_sync32k_write,
    .endianness = DEVICE_LITTLE_ENDIAN,
    .valid.min_access_size = 4,
    .valid.max_access_size = 4,
};

/*
 * TAP (Test Access Port) — ID registers for chip identification.
 * Base: 0x4830A000, size: 0x1000.
 * Linux reads IDCODE at offset 0x204 to identify the SoC.
 *
 * OMAP3630 ES1.2 IDCODE: hawkeye=0xb891, rev=2, manf=0x017
 */
#define OMAP3630_TAP_IDCODE     0x2B89102F
#define OMAP3630_TAP_PROD_ID    0x00000000

static uint64_t omap3630_tap_read(void *opaque, hwaddr offset, unsigned size)
{
    switch (offset) {
    case 0x204:
        return OMAP3630_TAP_IDCODE;
    case 0x208:
        return OMAP3630_TAP_PROD_ID;
    default:
        return 0;
    }
}

static void omap3630_tap_write(void *opaque, hwaddr offset,
                               uint64_t value, unsigned size)
{
    /* Read-only */
}

static const MemoryRegionOps omap3630_tap_ops = {
    .read = omap3630_tap_read,
    .write = omap3630_tap_write,
    .endianness = DEVICE_LITTLE_ENDIAN,
    .valid.min_access_size = 4,
    .valid.max_access_size = 4,
};

/*
 * CM (Clock Manager) — context-aware stub with DPLL state tracking.
 *
 * Base: 0x48004000, size: 0x4000.
 * Kernel CM base: 0x48004800 (IVA2 at -0x800 = 0x48004000).
 *
 * Domain map (QEMU offset ranges):
 *   0x000-0x0FF  IVA2_MOD  (DPLL2)
 *   0x800-0x8FF  OCP_MOD
 *   0x900-0x9FF  MPU_MOD   (DPLL1)
 *   0xA00-0xBFF  CORE_MOD
 *   0xC00-0xCFF  WKUP_MOD  (GPT1 lives here)
 *   0xD00-0xDFF  PLL_MOD   (DPLL3/4/5)
 *   0xE00-0xEFF  DSS_MOD
 *   0x1000-0x10FF PER_MOD  (GPT2-9, UART3, GPIO2-6)
 *
 * The DPLL IDLEST registers reflect whether each DPLL is locked:
 *   bit = 1 → locked,  bit = 0 → bypassed/stopped.
 * The kernel PM init writes CM_CLKEN_PLL to change DPLL modes, then
 * polls CM_IDLEST waiting for the transition to complete.  We track
 * CM_CLKEN_PLL writes so IDLEST reflects the requested mode.
 *
 * Module IDLEST (non-DPLL domains):
 *   bit = 0 → functional,  bit = 1 → idle/transitioning.
 * Returning 0 means "all modules functional".
 */

/*
 * DPLL enable register state — default: all DPLLs locked (EN_DPLL = 0x7).
 *
 * PLL_MOD CM_CLKEN_PLL  (0xD00): EN_CORE_DPLL[2:0], EN_PERIPH_DPLL[18:16]
 * PLL_MOD CM_CLKEN2_PLL (0xD04): EN_PERIPH2_DPLL[2:0] (DPLL5)
 * MPU_MOD CM_CLKEN_PLL  (0x904): EN_MPU_DPLL[2:0]
 * IVA2_MOD CM_CLKEN_PLL (0x004): EN_IVA2_DPLL[2:0]
 */
static uint32_t cm_clken_pll = 0x00070007;     /* Core+PER locked */
static uint32_t cm_clken2_pll = 0x00000007;    /* DPLL5 locked */
static uint32_t cm_clken_pll_mpu = 0x00000007; /* DPLL1 locked */
static uint32_t cm_clken_pll_iva2 = 0x00000007;/* DPLL2 locked */

/*
 * Per-domain CM register tracking.
 * Indexed by (domain_offset >> 8) & 0x1F — covers domains from
 * IVA2 (0x000) through USBHOST (0x1400).
 *
 * FCLKEN/ICLKEN init to 0xFFFFFFFF (all clocks on) to match the
 * previous all-ones fallback — boot reads these before writing.
 * AUTOIDLE/CLKSTCTRL init to 0 (no auto-idle, no transitions).
 */
#define CM_NUM_DOMAINS 22
static uint32_t cm_fclken[CM_NUM_DOMAINS];
static uint32_t cm_iclken[CM_NUM_DOMAINS];
static uint32_t cm_autoidle[CM_NUM_DOMAINS];
static uint32_t cm_clkstctrl[CM_NUM_DOMAINS];

static void omap3630_cm_init(void)
{
    for (int i = 0; i < CM_NUM_DOMAINS; i++) {
        cm_fclken[i] = 0xFFFFFFFF;
        cm_iclken[i] = 0xFFFFFFFF;
        cm_autoidle[i] = 0;
        /*
         * CLKSTCTRL upper bits = CLKACTIVITY (1=active).
         * Lower 2 bits = CLKTRCTRL (0=NO_SLEEP).
         * Report all domain clocks as active — we don't model gating.
         */
        cm_clkstctrl[i] = 0xFFFFFFFC;
    }
}

/* Helper: is EN_DPLL field requesting locked mode? (0x7 = lock) */
static inline bool dpll_is_locked(uint32_t en_dpll_field)
{
    return (en_dpll_field & 0x7) == 0x7;
}

static uint64_t omap3630_cm_read(void *opaque, hwaddr offset, unsigned size)
{
    uint8_t reg_off = offset & 0xFF;

    /*
     * --- DPLL IDLEST registers (offset 0x20/0x24 in DPLL domains) ---
     * Reflect the EN_DPLL mode written to CM_CLKEN_PLL.
     */

    /* PLL_MOD CM_IDLEST_CKGEN (0xD20): Core DPLL bit 0, PER DPLL bit 1 */
    if (offset == 0xD20) {
        uint32_t val = 0;
        if (dpll_is_locked(cm_clken_pll))        val |= (1 << 0);
        if (dpll_is_locked(cm_clken_pll >> 16))  val |= (1 << 1);
        return val;
    }
    /* PLL_MOD CM_IDLEST2_CKGEN (0xD24): DPLL5 bit 0 */
    if (offset == 0xD24) {
        return dpll_is_locked(cm_clken2_pll) ? 1 : 0;
    }
    /* MPU_MOD CM_IDLEST_PLL (0x924): DPLL1 bit 0 */
    if (offset == 0x924) {
        return dpll_is_locked(cm_clken_pll_mpu) ? 1 : 0;
    }
    /* IVA2_MOD CM_IDLEST_PLL (0x024): DPLL2 bit 0 */
    if (offset == 0x024) {
        return dpll_is_locked(cm_clken_pll_iva2) ? 1 : 0;
    }

    /* --- Module IDLEST (non-DPLL, offset 0x20/0x24/0x28) --- */
    if (reg_off == 0x20 || reg_off == 0x24 || reg_off == 0x28) {
        return 0x00000000;  /* all modules functional */
    }

    /* --- PLL_MOD (0xD00): DPLL3/4/5 clock configuration --- */
    if (offset >= 0xD00 && offset < 0xE00) {
        switch (reg_off) {
        case 0x00:  return cm_clken_pll;   /* CM_CLKEN_PLL (readback) */
        case 0x04:  return cm_clken2_pll;  /* CM_CLKEN2_PLL (readback) */
        case 0x40:  /* CM_CLKSEL1_PLL: DPLL3 config */
            return (1u << 27) | (400u << 16) | (12u << 8) | 1u;
        case 0x44:  /* CM_CLKSEL2_PLL: DPLL4 (PER) M/N */
            return (432u << 8) | 12u;
        case 0x48:  /* CM_CLKSEL3_PLL: DPLL4 M2 */
            return 9;
        case 0x4C:  /* CM_CLKSEL4_PLL: DPLL5 M/N */
            return (120u << 8) | 12u;
        case 0x50:  /* CM_CLKSEL5_PLL: DPLL5 M2 */
            return 2;
        }
    }

    /* --- MPU_MOD (0x900): DPLL1 (MPU clock) --- */
    if (offset >= 0x900 && offset < 0xA00) {
        switch (reg_off) {
        case 0x04:  return cm_clken_pll_mpu;  /* CM_CLKEN_PLL_MPU */
        case 0x40:  /* CM_CLKSEL1_PLL_MPU: DPLL1 M/N + divider */
            return (1u << 19) | (500u << 8) | 12u;
        case 0x44:  /* CM_CLKSEL2_PLL_MPU: DPLL1 M2 */
            return 1;
        }
    }

    /* --- IVA2_MOD (0x000): DPLL2 --- */
    if (offset < 0x100) {
        switch (reg_off) {
        case 0x04:  return cm_clken_pll_iva2;  /* CM_CLKEN_PLL_IVA2 */
        case 0x40:  /* CM_CLKSEL1_PLL_IVA2 */
            return (1u << 19) | (360u << 8) | 12u;
        case 0x44:  /* CM_CLKSEL2_PLL_IVA2 */
            return 1;
        }
    }

    /* --- Generic CM_CLKSEL for non-PLL domains --- */
    if (reg_off >= 0x40 && reg_off <= 0x44) {
        return 0x55555555;
    }

    /* --- Per-domain FCLKEN/ICLKEN/AUTOIDLE/CLKSTCTRL tracking --- */
    {
        unsigned domain = (offset >> 8);
        if (domain >= CM_NUM_DOMAINS) domain = 0;
        switch (reg_off) {
        case 0x00: return cm_fclken[domain];
        case 0x10: return cm_iclken[domain];
        case 0x30: return cm_autoidle[domain];
        case 0x48: return cm_clkstctrl[domain];
        case 0x4C: return 0x1;  /* CLKSTST: domain clocks active */
        default:   return 0;
        }
    }
}

static void omap3630_cm_write(void *opaque, hwaddr offset,
                               uint64_t value, unsigned size)
{
    uint8_t reg_off = offset & 0xFF;

    /* Track DPLL enable mode writes for IDLEST readback */
    switch (offset) {
    case 0xD00: cm_clken_pll = value;      return;  /* PLL_MOD CM_CLKEN_PLL */
    case 0xD04: cm_clken2_pll = value;     return;  /* PLL_MOD CM_CLKEN2_PLL */
    case 0x904: cm_clken_pll_mpu = value;  return;  /* MPU_MOD CM_CLKEN_PLL */
    case 0x004: cm_clken_pll_iva2 = value; return;  /* IVA2_MOD CM_CLKEN_PLL */
    }

    /* Per-domain FCLKEN/ICLKEN/AUTOIDLE/CLKSTCTRL tracking */
    {
        unsigned domain = (offset >> 8);
        if (domain >= CM_NUM_DOMAINS) return;
        switch (reg_off) {
        case 0x00: cm_fclken[domain] = value;   break;
        case 0x10: cm_iclken[domain] = value;   break;
        case 0x30: cm_autoidle[domain] = value;  break;
        case 0x48:
            /* Accept CLKTRCTRL writes but keep CLKACTIVITY bits active */
            cm_clkstctrl[domain] = (value & 0x3) | 0xFFFFFFFC;
            break;
        }
    }
}

static const MemoryRegionOps omap3630_cm_ops = {
    .read = omap3630_cm_read,
    .write = omap3630_cm_write,
    .endianness = DEVICE_LITTLE_ENDIAN,
    .valid.min_access_size = 4,
    .valid.max_access_size = 4,
};

/*
 * PRM (Power & Reset Manager) — context-aware stub.
 *
 * Base: 0x48306000, size: 0x4000.
 * Kernel PRM base: 0x48306800 (IVA2 at -0x800 = 0x48306000).
 *
 * Each power domain is a 0x100-byte block.  Key registers:
 *   0x58  RM_RSTST    — reset status (write-1-to-clear)
 *   0xE0  PM_PWSTCTRL — power state control
 *   0xE4  PM_PWSTST   — power state status
 *
 * PM_PWSTST bit 20 = INTRANSITION.  If set, kernel spins 100ms
 * per domain waiting for it to clear.  Return ON (0x3) with no
 * transition to avoid this.
 *
 * RM_RSTST: return 0 (no pending reset events).  Kernel writes
 * 0xFFFFFFFF to clear all flags; returning 0 means "already clear".
 */
/*
 * Per-domain PRM state.  PM_PWSTCTRL defaults to 0x3 (power ON) so
 * PM_PWSTST reflects ON without transition — matches previous behavior.
 * RM_RSTST tracks reset flags (write-1-to-clear).
 */
#define PRM_NUM_DOMAINS 22
static uint32_t prm_pwstctrl[PRM_NUM_DOMAINS];
static uint32_t prm_rstst[PRM_NUM_DOMAINS];

static void omap3630_prm_init(void)
{
    for (int i = 0; i < PRM_NUM_DOMAINS; i++) {
        prm_pwstctrl[i] = 0x3;  /* power ON */
        prm_rstst[i] = 0;
    }
}

static uint64_t omap3630_prm_read(void *opaque, hwaddr offset, unsigned size)
{
    /*
     * PRM domain layout (offsets from PRM base 0x48306000):
     *   0x000-0x0FF  IVA2_PRM
     *   0x800-0x8FF  OCP_PRM
     *   0x900-0x9FF  MPU_PRM
     *   0xA00-0xBFF  CORE_PRM
     *   0xC00-0xCFF  WKUP_PRM
     *   0xD00-0xDFF  CCR_PRM (clock control)
     *   0xE00-0xEFF  DSS_PRM
     *   0x1000-0x10FF PER_PRM
     *   0x1200-0x12FF GR_PRM (global registers)
     */

    /* --- Absolute-offset registers (not repeated per domain) --- */

    /* CCR_PRM (0xD00): PRM_CLKSEL — oscillator selection */
    if (offset == 0xD40) {
        /*
         * SYS_CLKIN_SEL[2:0] selects the input oscillator:
         *   0=12M, 1=13M, 2=19.2M, 3=26M, 4=38.4M, 5=16.8M
         * We use 26 MHz (index 3) — consistent with DPLL M/N values
         * that assume 26 MHz input (e.g. DPLL3: 26*400/13 = 800 MHz).
         */
        return 3;
    }

    /* GR_PRM (0x1200): PRM_CLKSRC_CTRL — sys_ck divider */
    if (offset == 0x1270) {
        /*
         * SYSCLKDIV[7:6] divides osc_sys_ck to produce sys_ck.
         * With ti,index-starts-at-one: 1=/1, 2=/2, 3=reserved.
         * Value 1 → sys_ck = osc_sys_ck / 1 = 38.4 MHz
         */
        return (1u << 6);
    }

    /* --- Per-domain registers (repeated in each 0x100-byte block) --- */
    {
        uint8_t reg_off = offset & 0xFF;
        unsigned domain = (offset >> 8);
        if (domain >= PRM_NUM_DOMAINS) domain = 0;

        switch (reg_off) {
        case 0xE0:  /* PM_PWSTCTRL — return tracked value */
            return prm_pwstctrl[domain];
        case 0xE4:  /* PM_PWSTST — reflect requested power state, no transition */
            return prm_pwstctrl[domain] & 0x3;
        case 0x58:  /* RM_RSTST — return tracked reset flags */
            return prm_rstst[domain];
        case 0xA0: case 0xA4: case 0xA8:  /* PM_WKEN/WKST/WKDEP */
            return 0;
        default:
            return 0xFFFFFFFF;
        }
    }
}

static void omap3630_prm_write(void *opaque, hwaddr offset,
                                uint64_t value, unsigned size)
{
    uint8_t reg_off = offset & 0xFF;
    unsigned domain = (offset >> 8);
    if (domain >= PRM_NUM_DOMAINS) return;

    switch (reg_off) {
    case 0xE0:  /* PM_PWSTCTRL */
        prm_pwstctrl[domain] = value;
        break;
    case 0x58:  /* RM_RSTST — write-1-to-clear */
        prm_rstst[domain] &= ~value;
        break;
    }
}

static const MemoryRegionOps omap3630_prm_ops = {
    .read = omap3630_prm_read,
    .write = omap3630_prm_write,
    .endianness = DEVICE_LITTLE_ENDIAN,
    .valid.min_access_size = 4,
    .valid.max_access_size = 4,
};

/*
 * ==========================================================================
 * TWL5031 PMIC register model
 *
 * The TWL4030/5031 PMIC occupies 4 I2C slave addresses, each with 256
 * bytes of register space:
 *   0x48 — slave 0: USB
 *   0x49 — slave 1: Audio/Voice, PIH, INTBR, GPIO, Test
 *   0x4A — slave 2: MADC, Interrupts, Keypad, LED, PWM, Precharge
 *   0x4B — slave 3: PM_MASTER, PM_RECEIVER, RTC, Backup, INT, Secured
 *
 * The Linux twl-core driver uses regmap over I2C to access these.
 * We model enough registers for the probe sequence to succeed:
 *   - IDCODE registers (INTBR module, slave 1, offset 0x85-0x88)
 *   - PIH_ISR (slave 1, offset 0x81) — return 0 (no interrupts)
 *   - PM_MASTER PROTECT_KEY (slave 3, offset 0x44)
 *   - PM_MASTER R_CFG_BOOT (slave 3, offset 0x3B)
 *   - Various SIH control/status/mask registers — return 0
 *   - All other registers — return 0 (safe default)
 * ==========================================================================
 */

/* TWL5031 IDCODE: 0x5703_1505 (little-endian in register space)
 * TWL_SIL_TYPE = 0x031505, TWL_SIL_REV = 0x57
 * This identifies as TWL5031 ES1.0
 */
#define TWL5031_IDCODE_7_0      0x05
#define TWL5031_IDCODE_15_8     0x15
#define TWL5031_IDCODE_23_16    0x03
#define TWL5031_IDCODE_31_24    0x57

typedef struct {
    uint8_t regs[4][256];  /* 4 slaves x 256 registers each */
} TWL5031State;

static TWL5031State twl5031_state;

static void twl5031_init(TWL5031State *s)
{
    memset(s->regs, 0, sizeof(s->regs));

    /*
     * Slave 1 (0x49) — INTBR module at base 0x85:
     *   REG_IDCODE_7_0  = INTBR_base + 0x00 = 0x85
     *   REG_IDCODE_15_8 = INTBR_base + 0x01 = 0x86
     *   REG_IDCODE_23_16= INTBR_base + 0x02 = 0x87
     *   REG_IDCODE_31_24= INTBR_base + 0x03 = 0x88
     */
    s->regs[1][0x85] = TWL5031_IDCODE_7_0;
    s->regs[1][0x86] = TWL5031_IDCODE_15_8;
    s->regs[1][0x87] = TWL5031_IDCODE_23_16;
    s->regs[1][0x88] = TWL5031_IDCODE_31_24;

    /*
     * Slave 1 (0x49) — INTBR REG_GPPUPDCTR1 at base+0x0F = 0x94
     * Default pull-up control — kernel reads then writes this.
     */
    s->regs[1][0x94] = 0x55;  /* I2C pull-ups enabled by default */

    /*
     * Slave 3 (0x4B) — PM_MASTER at base 0x36:
     *   R_CFG_BOOT       = PM_MASTER_base + 0x05 = 0x3B
     *   PROTECT_KEY      = PM_MASTER_base + 0x0E = 0x44
     */
    /* R_CFG_BOOT: HFCLK_FREQ=26MHz (bits[1:0]=2), HIGH_PERF_SQ=0 */
    s->regs[3][0x3B] = 0x02;  /* HFCLK_FREQ_26_MHZ */

    /*
     * Slave 3 (0x4B) — PM_RECEIVER at base 0x5B:
     *   DCDC_GLOBAL_CFG = PM_RECEIVER_base + 0x06 = 0x61
     * SmartReflex enable bit (bit 3) — kernel sets this.
     */
    s->regs[3][0x61] = 0x00;

    /*
     * Slave 0 (0x48) — USB PHY module at base 0x00:
     * TWL4030 has integrated ULPI-like USB transceiver.
     * Driver: drivers/phy/ti/phy-twl4030-usb.c
     */
    s->regs[0][0x00] = 0x6A;  /* VENDOR_ID_LO (TI) */
    s->regs[0][0x01] = 0x04;  /* VENDOR_ID_HI */
    s->regs[0][0x02] = 0x23;  /* PRODUCT_ID_LO (TWL4030) */
    s->regs[0][0x03] = 0x40;  /* PRODUCT_ID_HI */
    s->regs[0][0xFE] = 0x00;  /* PHY_CLK_CTRL: no request initially */
    s->regs[0][0xFF] = 0x01;  /* PHY_CLK_CTRL_STS: clock ready */

    /*
     * Slave 3 (0x4B) — RTC module at base 0x1C:
     * Driver: drivers/rtc/rtc-twl.c
     * Probe reads RTC_STATUS (0x2A), clears POWER_UP, stops RTC,
     * disables interrupts.  Time registers return host BCD time
     * dynamically in twl5031_read().
     */
    s->regs[3][0x29] = 0x01;  /* RTC_CTRL: STOP_RTC=1 (running) */
    s->regs[3][0x2A] = 0x82;  /* RTC_STATUS: POWER_UP=1, RUN=1 */
    s->regs[3][0x2B] = 0x00;  /* RTC_INTERRUPTS: all disabled */

    /*
     * Slave 1 (0x49) — Audio codec at base 0x00:
     * Driver: sound/soc/codecs/twl4030.c
     * Just needs register writes accepted (generic model handles this).
     * APLL_CTL (0x3A): no explicit lock polling in driver.
     */

    /*
     * Slave 2 (0x4A) — Watchdog via PM_RECEIVER:
     * Driver: drivers/watchdog/twl4030_wdt.c
     * Writes to PM_RECEIVER base 0x5B + offset 0x03 = 0x5E on slave 3.
     * Generic register model accepts writes — watchdog never fires.
     * No special init needed.
     */
}

/* Read a register from the TWL5031.  addr = I2C address (0x48-0x4B). */
static uint8_t twl5031_read(TWL5031State *s, uint8_t addr, uint8_t reg)
{
    int slave = addr - 0x48;
    if (slave < 0 || slave > 3) {
        return 0;
    }

    /*
     * Dynamic RTC time (slave 3, module base 0x1C, offsets 0x00-0x06).
     * Returns host system time in BCD format.
     * Register 0x1C=seconds, 0x1D=minutes, 0x1E=hours,
     * 0x1F=day, 0x20=month, 0x21=year, 0x22=weekday.
     */
    if (slave == 3 && reg >= 0x1C && reg <= 0x22) {
        time_t now = time(NULL);
        struct tm tm;
        localtime_r(&now, &tm);
        switch (reg) {
        case 0x1C: return ((tm.tm_sec / 10) << 4) | (tm.tm_sec % 10);
        case 0x1D: return ((tm.tm_min / 10) << 4) | (tm.tm_min % 10);
        case 0x1E: return ((tm.tm_hour / 10) << 4) | (tm.tm_hour % 10);
        case 0x1F: return ((tm.tm_mday / 10) << 4) | (tm.tm_mday % 10);
        case 0x20: {
            int mon = tm.tm_mon + 1;
            return ((mon / 10) << 4) | (mon % 10);
        }
        case 0x21: {
            int yr = tm.tm_year % 100;
            return ((yr / 10) << 4) | (yr % 10);
        }
        case 0x22: return tm.tm_wday;  /* 0=Sunday */
        }
    }

    /* USB PHY clock status: mirror request bit from CLK_CTRL */
    if (slave == 0 && reg == 0xFF) {
        return (s->regs[0][0xFE] & 0x01) ? 0x01 : 0x00;
    }

    return s->regs[slave][reg];
}

/* Write a register to the TWL5031. */
static void twl5031_write(TWL5031State *s, uint8_t addr, uint8_t reg,
                           uint8_t val)
{
    int slave = addr - 0x48;
    if (slave < 0 || slave > 3) {
        return;
    }
    s->regs[slave][reg] = val;
}

/* Check if a given I2C address is a TWL5031 slave */
static bool twl5031_is_address(uint8_t addr)
{
    return (addr >= 0x48 && addr <= 0x4B);
}

/*
 * ==========================================================================
 * LM8323 Keypad Controller model
 *
 * National Semiconductor LM8320/LM8323 — QWERTY matrix keypad scanner
 * with I2C interface, 3 PWM LED drivers, and GPIO expander.
 * Used in Nokia E7 for the slide-out QWERTY keyboard on I2C3.
 *
 * Protocol: command-based I2C (not register-addressed like TWL5031).
 *   Write transaction: [cmd_byte] [data0] [data1] ...
 *   Read transaction:  previous write sets cmd, then read returns response.
 *
 * Linux driver: drivers/input/keyboard/lm8323.c
 * ==========================================================================
 */

#define LM8323_FIFO_LEN     15
#define LM8323_I2C_ADDR     0x34  /* Must match DTS reg property */

/* Commands (from lm8323.c) */
#define LM8323_CMD_READ_ID          0x80
#define LM8323_CMD_WRITE_CFG        0x81
#define LM8323_CMD_READ_INT         0x82
#define LM8323_CMD_RESET            0x83
#define LM8323_CMD_WRITE_PORT_SEL   0x85
#define LM8323_CMD_WRITE_PORT_STATE 0x86
#define LM8323_CMD_READ_PORT_SEL    0x87
#define LM8323_CMD_READ_PORT_STATE  0x88
#define LM8323_CMD_READ_FIFO        0x89
#define LM8323_CMD_RPT_READ_FIFO    0x8a
#define LM8323_CMD_SET_ACTIVE       0x8b
#define LM8323_CMD_READ_ERR         0x8c
#define LM8323_CMD_SET_DEBOUNCE     0x8f
#define LM8323_CMD_SET_KEY_SIZE     0x90
#define LM8323_CMD_READ_KEY_SIZE    0x91
#define LM8323_CMD_READ_CFG         0x92
#define LM8323_CMD_WRITE_CLOCK      0x93
#define LM8323_CMD_READ_CLOCK       0x94
#define LM8323_CMD_PWM_WRITE        0x95
#define LM8323_CMD_START_PWM        0x96
#define LM8323_CMD_STOP_PWM         0x97

/* Interrupt status bits */
#define LM8323_INT_KEYPAD    0x01
#define LM8323_INT_ERROR     0x08
#define LM8323_INT_NOINIT    0x10
#define LM8323_INT_PWM1      0x20
#define LM8323_INT_PWM2      0x40
#define LM8323_INT_PWM3      0x80

typedef struct {
    uint8_t  int_status;
    uint8_t  err_status;
    uint8_t  config;
    uint8_t  clock;
    uint8_t  key_size;
    uint8_t  active_time;
    uint8_t  debounce_time;
    uint8_t  port_sel[2];
    uint8_t  port_state[2];

    /* Key event FIFO: byte format = [7]=press [6:0]=scancode */
    uint8_t  fifo[LM8323_FIFO_LEN];
    int      fifo_head;
    int      fifo_tail;
    int      fifo_count;

    /* Interrupt output (wired to INTC) */
    qemu_irq irq;
} LM8323State;

static LM8323State lm8323_state;

static void lm8323_init(LM8323State *s)
{
    memset(s, 0, sizeof(*s));
    /* After power-on, chip signals NOINIT to request configuration */
    s->int_status = LM8323_INT_NOINIT;
}

static bool lm8323_is_address(uint8_t addr)
{
    return addr == LM8323_I2C_ADDR;
}

/*
 * Push a key event into the LM8323 FIFO.
 * Scancode format: [7]=press, [6:0]=(row << 4) | col
 * Asserts the interrupt line when FIFO becomes non-empty.
 */
static void lm8323_push_key(LM8323State *s, int row, int col, bool press)
{
    uint8_t scancode;

    if (s->fifo_count >= LM8323_FIFO_LEN) {
        return;  /* FIFO full — drop key */
    }

    scancode = ((row & 0x07) << 4) | (col & 0x0F);
    if (press) {
        scancode |= 0x80;
    }

    s->fifo[s->fifo_head] = scancode;
    s->fifo_head = (s->fifo_head + 1) % LM8323_FIFO_LEN;
    s->fifo_count++;

    s->int_status |= LM8323_INT_KEYPAD;
    qemu_set_irq(s->irq, 1);
}

/*
 * Process write data byte for a given command.
 * write_pos is the index of this data byte (0 = first byte after command).
 */
static void lm8323_write_byte(LM8323State *s, uint8_t cmd,
                               int write_pos, uint8_t data)
{
    switch (cmd) {
    case LM8323_CMD_RESET:
        if (write_pos == 0 && data == 0xAA) {
            s->int_status = LM8323_INT_NOINIT;
            s->err_status = 0;
            s->fifo_count = 0;
            s->fifo_head = 0;
            s->fifo_tail = 0;
        }
        break;
    case LM8323_CMD_WRITE_CFG:
        if (write_pos == 0) {
            s->config = data;
        }
        break;
    case LM8323_CMD_WRITE_CLOCK:
        if (write_pos == 0) {
            s->clock = data;
        }
        break;
    case LM8323_CMD_SET_KEY_SIZE:
        if (write_pos == 0) {
            s->key_size = data;
        }
        break;
    case LM8323_CMD_SET_ACTIVE:
        if (write_pos == 0) {
            s->active_time = data;
        }
        break;
    case LM8323_CMD_SET_DEBOUNCE:
        if (write_pos == 0) {
            s->debounce_time = data;
        }
        break;
    case LM8323_CMD_WRITE_PORT_SEL:
        if (write_pos < 2) {
            s->port_sel[write_pos] = data;
        }
        break;
    case LM8323_CMD_WRITE_PORT_STATE:
        if (write_pos < 2) {
            s->port_state[write_pos] = data;
        }
        break;
    case LM8323_CMD_PWM_WRITE:
    case LM8323_CMD_START_PWM:
    case LM8323_CMD_STOP_PWM:
        /* PWM stubs: accept silently */
        break;
    default:
        break;
    }
}

/*
 * Fill the I2C read buffer with the response for the given command.
 * Returns number of bytes written to buf.
 */
static int lm8323_fill_read(LM8323State *s, uint8_t cmd,
                             uint8_t *buf, int max_len)
{
    switch (cmd) {
    case LM8323_CMD_READ_ID:
        if (max_len >= 2) {
            buf[0] = 0x84;  /* Chip ID low — non-zero = chip present */
            buf[1] = 0x03;  /* Chip ID high */
        }
        return (max_len >= 2) ? 2 : max_len;

    case LM8323_CMD_READ_INT:
        if (max_len >= 1) {
            buf[0] = s->int_status;
                s->int_status = 0;  /* Reading clears all interrupt bits */
        }
        return (max_len >= 1) ? 1 : 0;

    case LM8323_CMD_READ_ERR:
        if (max_len >= 1) {
            buf[0] = s->err_status;
            s->err_status = 0;
        }
        return (max_len >= 1) ? 1 : 0;

    case LM8323_CMD_READ_FIFO:
    case LM8323_CMD_RPT_READ_FIFO:
    {
        int i;
        for (i = 0; i < max_len; i++) {
            if (s->fifo_count > 0) {
                buf[i] = s->fifo[s->fifo_tail];
                s->fifo_tail = (s->fifo_tail + 1) % LM8323_FIFO_LEN;
                s->fifo_count--;
            } else {
                buf[i] = 0;  /* 0x00 = end of FIFO */
            }
        }
        if (s->fifo_count == 0) {
            s->int_status &= ~LM8323_INT_KEYPAD;
            qemu_set_irq(s->irq, 0);
        }
        return max_len;
    }

    case LM8323_CMD_READ_KEY_SIZE:
        if (max_len >= 1) {
            buf[0] = s->key_size;
        }
        return (max_len >= 1) ? 1 : 0;

    case LM8323_CMD_READ_CFG:
        if (max_len >= 1) {
            buf[0] = s->config;
        }
        return (max_len >= 1) ? 1 : 0;

    case LM8323_CMD_READ_CLOCK:
        if (max_len >= 1) {
            buf[0] = s->clock;
        }
        return (max_len >= 1) ? 1 : 0;

    case LM8323_CMD_READ_PORT_SEL:
        if (max_len >= 2) {
            buf[0] = s->port_sel[0];
            buf[1] = s->port_sel[1];
        }
        return (max_len >= 2) ? 2 : max_len;

    case LM8323_CMD_READ_PORT_STATE:
        if (max_len >= 2) {
            buf[0] = s->port_state[0];
            buf[1] = s->port_state[1];
        }
        return (max_len >= 2) ? 2 : max_len;

    default:
        memset(buf, 0, max_len);
        return max_len;
    }
}

/*
 * ==========================================================================
 * Nokia E7 QWERTY keyboard input handler
 *
 * Maps host QKeyCode events to LM8323 matrix (row, col) scancodes
 * and pushes them into the LM8323 FIFO.  The reverse mapping is
 * derived from the DTS linux,keymap entries.
 *
 * Row/Col layout (from DTS, 8×11 matrix, 41 keys mapped):
 *   Row 0: Q W E R T Y U I O P
 *   Row 1: A S D F G H J K L ?
 *   Row 2: Z X C V B N M , .
 *   Row 3: Fn Shift Space
 *   Row 4: Compose Ctrl Enter Backspace
 *   Row 5: Up Left Down Right
 * ==========================================================================
 */

typedef struct {
    int8_t row;
    int8_t col;
} E7MatrixPos;

#define E7_KEY(r, c)  { .row = (r), .col = (c) }
#define E7_NONE       { .row = -1, .col = -1 }

/*
 * Reverse keymap: QKeyCode -> (row, col).
 * Only populated entries are valid; everything else is E7_NONE.
 * We use a function instead of a huge sparse array.
 */
static E7MatrixPos qcode_to_e7(int qcode)
{
    switch (qcode) {
    /* Row 0: QWERTYUIOP */
    case Q_KEY_CODE_Q:          return (E7MatrixPos)E7_KEY(0, 0);
    case Q_KEY_CODE_W:          return (E7MatrixPos)E7_KEY(0, 1);
    case Q_KEY_CODE_E:          return (E7MatrixPos)E7_KEY(0, 2);
    case Q_KEY_CODE_R:          return (E7MatrixPos)E7_KEY(0, 3);
    case Q_KEY_CODE_T:          return (E7MatrixPos)E7_KEY(0, 4);
    case Q_KEY_CODE_Y:          return (E7MatrixPos)E7_KEY(0, 5);
    case Q_KEY_CODE_U:          return (E7MatrixPos)E7_KEY(0, 6);
    case Q_KEY_CODE_I:          return (E7MatrixPos)E7_KEY(0, 7);
    case Q_KEY_CODE_O:          return (E7MatrixPos)E7_KEY(0, 8);
    case Q_KEY_CODE_P:          return (E7MatrixPos)E7_KEY(0, 9);

    /* Row 1: ASDFGHJKL? */
    case Q_KEY_CODE_A:          return (E7MatrixPos)E7_KEY(1, 0);
    case Q_KEY_CODE_S:          return (E7MatrixPos)E7_KEY(1, 1);
    case Q_KEY_CODE_D:          return (E7MatrixPos)E7_KEY(1, 2);
    case Q_KEY_CODE_F:          return (E7MatrixPos)E7_KEY(1, 3);
    case Q_KEY_CODE_G:          return (E7MatrixPos)E7_KEY(1, 4);
    case Q_KEY_CODE_H:          return (E7MatrixPos)E7_KEY(1, 5);
    case Q_KEY_CODE_J:          return (E7MatrixPos)E7_KEY(1, 6);
    case Q_KEY_CODE_K:          return (E7MatrixPos)E7_KEY(1, 7);
    case Q_KEY_CODE_L:          return (E7MatrixPos)E7_KEY(1, 8);
    case Q_KEY_CODE_SLASH:      return (E7MatrixPos)E7_KEY(1, 9);

    /* Row 2: ZXCVBNM,. */
    case Q_KEY_CODE_Z:          return (E7MatrixPos)E7_KEY(2, 0);
    case Q_KEY_CODE_X:          return (E7MatrixPos)E7_KEY(2, 1);
    case Q_KEY_CODE_C:          return (E7MatrixPos)E7_KEY(2, 2);
    case Q_KEY_CODE_V:          return (E7MatrixPos)E7_KEY(2, 3);
    case Q_KEY_CODE_B:          return (E7MatrixPos)E7_KEY(2, 4);
    case Q_KEY_CODE_N:          return (E7MatrixPos)E7_KEY(2, 5);
    case Q_KEY_CODE_M:          return (E7MatrixPos)E7_KEY(2, 6);
    case Q_KEY_CODE_COMMA:      return (E7MatrixPos)E7_KEY(2, 7);
    case Q_KEY_CODE_DOT:        return (E7MatrixPos)E7_KEY(2, 8);

    /* Row 3: Fn, Shift, Space */
    case Q_KEY_CODE_META_L:     return (E7MatrixPos)E7_KEY(3, 0);  /* Fn */
    case Q_KEY_CODE_SHIFT:      return (E7MatrixPos)E7_KEY(3, 1);
    case Q_KEY_CODE_SHIFT_R:    return (E7MatrixPos)E7_KEY(3, 1);
    case Q_KEY_CODE_SPC:        return (E7MatrixPos)E7_KEY(3, 2);

    /* Row 4: Compose, Ctrl, Enter, Backspace */
    case Q_KEY_CODE_COMPOSE:    return (E7MatrixPos)E7_KEY(4, 0);
    case Q_KEY_CODE_CTRL:       return (E7MatrixPos)E7_KEY(4, 1);
    case Q_KEY_CODE_CTRL_R:     return (E7MatrixPos)E7_KEY(4, 1);
    case Q_KEY_CODE_RET:        return (E7MatrixPos)E7_KEY(4, 2);
    case Q_KEY_CODE_KP_ENTER:   return (E7MatrixPos)E7_KEY(4, 2);
    case Q_KEY_CODE_BACKSPACE:  return (E7MatrixPos)E7_KEY(4, 3);
    case Q_KEY_CODE_DELETE:     return (E7MatrixPos)E7_KEY(4, 3);

    /* Row 5: arrows */
    case Q_KEY_CODE_UP:         return (E7MatrixPos)E7_KEY(5, 0);
    case Q_KEY_CODE_LEFT:       return (E7MatrixPos)E7_KEY(5, 1);
    case Q_KEY_CODE_DOWN:       return (E7MatrixPos)E7_KEY(5, 2);
    case Q_KEY_CODE_RIGHT:      return (E7MatrixPos)E7_KEY(5, 3);

    /* Convenience aliases for host usability */
    case Q_KEY_CODE_TAB:        return (E7MatrixPos)E7_KEY(3, 0);  /* Tab→Fn */
    case Q_KEY_CODE_ESC:        return (E7MatrixPos)E7_KEY(4, 0);  /* Esc→Compose */

    default:                    return (E7MatrixPos)E7_NONE;
    }
}

static void nokia_e7_kbd_event(DeviceState *dev, QemuConsole *src,
                                InputEvent *evt)
{
    InputKeyEvent *key;
    int qcode;
    E7MatrixPos pos;

    if (evt->type != INPUT_EVENT_KIND_KEY) {
        return;
    }

    key = evt->u.key.data;
    qcode = qemu_input_key_value_to_qcode(key->key);
    pos = qcode_to_e7(qcode);


    if (pos.row < 0) {
        return;  /* Unmapped key */
    }

    lm8323_push_key(&lm8323_state, pos.row, pos.col, key->down);
}

static const QemuInputHandler nokia_e7_kbd_handler = {
    .name  = "Nokia E7 QWERTY",
    .mask  = INPUT_EVENT_MASK_KEY,
    .event = nokia_e7_kbd_event,
};

/*
 * ==========================================================================
 * OMAP3630 I2C controller emulation
 *
 * Register map (IP v1, OMAP3, 16-bit regs with reg_shift=2):
 *   0x00  I2C_REV     — revision (read-only, 0x36 for OMAP3630)
 *   0x04  I2C_IE      — interrupt enable
 *   0x08  I2C_STAT    — status (read clears some bits; write-1-to-clear)
 *   0x0C  I2C_WE      — wakeup enable (OMAP3)
 *   0x10  I2C_SYSS    — system status (RDONE=1)
 *   0x14  I2C_BUF     — buffer configuration
 *   0x18  I2C_CNT     — data count
 *   0x1C  I2C_DATA    — data access register (8-bit on OMAP3630)
 *   0x20  I2C_SYSC    — system configuration
 *   0x24  I2C_CON     — control register (STT, STP, TRX, MST, EN)
 *   0x28  I2C_OA      — own address
 *   0x2C  I2C_SA      — slave address
 *   0x30  I2C_PSC     — prescaler
 *   0x34  I2C_SCLL    — SCL low time
 *   0x38  I2C_SCLH    — SCL high time
 *   0x3C  I2C_SYSTEST — system test
 *   0x40  I2C_BUFSTAT — buffer status
 *
 * Transaction flow:
 *   Write: SA -> CNT -> CON(STT+STP+TRX) -> poll STAT for XRDY ->
 *          write DATA bytes -> poll STAT for ARDY
 *   Read:  SA -> CNT -> CON(STT+STP) -> poll STAT for RRDY ->
 *          read DATA bytes -> poll STAT for ARDY
 * ==========================================================================
 */

/* I2C STAT register bits */
#define I2C_STAT_XDR   (1 << 14)
#define I2C_STAT_RDR   (1 << 13)
#define I2C_STAT_BB    (1 << 12)
#define I2C_STAT_ROVR  (1 << 11)
#define I2C_STAT_XUDF  (1 << 10)
#define I2C_STAT_AAS   (1 << 9)
#define I2C_STAT_BF    (1 << 8)
#define I2C_STAT_XRDY  (1 << 4)
#define I2C_STAT_RRDY  (1 << 3)
#define I2C_STAT_ARDY  (1 << 2)
#define I2C_STAT_NACK  (1 << 1)
#define I2C_STAT_AL    (1 << 0)

/* I2C CON register bits */
#define I2C_CON_EN     (1 << 15)
#define I2C_CON_MST    (1 << 10)
#define I2C_CON_TRX    (1 << 9)
#define I2C_CON_STP    (1 << 1)
#define I2C_CON_STT    (1 << 0)

/* SYSTEST bits for bb_valid detection */
#define I2C_SYSTEST_SCL_I_FUNC  (1 << 8)   /* SCL line input (functional) */
#define I2C_SYSTEST_SDA_I_FUNC  (1 << 6)   /* SDA line input (functional) */

/*
 * I2C sensor stubs — minimal register-file models so drivers probe
 * without hitting -121 Remote I/O errors.  Two stub types:
 *   SensorStubState: generic register-addressed (LIS302DL, AK8974, APDS990x)
 *   MXTStubState:    mXT touchscreen (2-byte LE address, block read)
 */
typedef struct {
    uint8_t addr;       /* 7-bit I2C address, 0 = unused */
    uint8_t regs[256];  /* register file */
    uint8_t reg_ptr;    /* current register pointer */
    bool    reg_set;    /* first byte (reg addr) received in this xfer? */
} SensorStubState;

typedef struct {
    uint8_t  info[10];     /* info block: 7 info + 3 CRC bytes */
    uint16_t addr_ptr;     /* 16-bit register address pointer */
    int      addr_pos;     /* 0=waiting lo, 1=waiting hi, 2=ready */
} MXTStubState;

#define MXT_I2C_ADDR 0x4C

static SensorStubState i2c1_sensors[3];
static SensorStubState i2c3_sensors[4];
static MXTStubState    mxt_stub;

static void init_i2c1_sensors(void)
{
    memset(i2c1_sensors, 0, sizeof(i2c1_sensors));

    /* BQ2415x charger at 0x6B — register 0x03 bits[7:5] = revision */
    i2c1_sensors[0].addr = 0x6B;
    i2c1_sensors[0].regs[0x03] = 0x40;  /* rev=2 (BQ24150), vendor=0 */
    i2c1_sensors[0].regs[0x04] = 0x89;  /* battery regulation = 4.2V */

    /* LP5521 LED controller at 0x33 (NJOY-3, Nokia "PEARL" I2C bus) */
    i2c1_sensors[1].addr = 0x33;
    i2c1_sensors[1].regs[0x00] = 0x00;  /* ENABLE: driver writes 0xC0, reads back */
    i2c1_sensors[1].regs[0x05] = 0xAF;  /* R_CURRENT default (probe checks this) */
    i2c1_sensors[1].regs[0x06] = 0xAF;  /* G_CURRENT default */
    i2c1_sensors[1].regs[0x07] = 0xAF;  /* B_CURRENT default */

    /* TPA6130A2 headphone amp at 0x60 (DCP confirms, not TPA6140) */
    i2c1_sensors[2].addr = 0x60;
    i2c1_sensors[2].regs[0x04] = 0x02;  /* VERSION: bits[3:0]=2 (TPA6130A2) */
    i2c1_sensors[2].regs[0x01] = 0x00;  /* CONTROL: SWS=0 (not shutdown) */
}

static void init_i2c3_sensors(void)
{
    memset(i2c3_sensors, 0, sizeof(i2c3_sensors));

    /* LIS302DL accelerometer #1 at 0x1D */
    i2c3_sensors[0].addr = 0x1D;
    i2c3_sensors[0].regs[0x0F] = 0x3B;  /* WHO_AM_I = WAI_8B */
    i2c3_sensors[0].regs[0x20] = 0x07;  /* CTRL_REG1 default */

    /* LIS302DL accelerometer #2 at 0x1C */
    i2c3_sensors[1].addr = 0x1C;
    i2c3_sensors[1].regs[0x0F] = 0x3B;
    i2c3_sensors[1].regs[0x20] = 0x07;

    /* AK8974 magnetometer at 0x0F */
    i2c3_sensors[2].addr = 0x0F;
    i2c3_sensors[2].regs[0x0F] = 0x48;  /* WHOAMI = AK8974 */
    i2c3_sensors[2].regs[0x0D] = 0x04;  /* INFO register */
    i2c3_sensors[2].regs[0x0C] = 0x55;  /* SELFTEST = IDLE */

    /* APDS990x ALS/proximity at 0x39 */
    i2c3_sensors[3].addr = 0x39;
    i2c3_sensors[3].regs[0x12] = 0x29;  /* APDS990X_ID */
    i2c3_sensors[3].regs[0x11] = 0x01;  /* REV */

    /* mXT touchscreen at 0x4C */
    memset(&mxt_stub, 0, sizeof(mxt_stub));
    mxt_stub.info[0] = 0x80;  /* family_id (mXT224) */
    mxt_stub.info[1] = 0x01;  /* variant_id */
    mxt_stub.info[2] = 0x10;  /* version */
    mxt_stub.info[3] = 0x01;  /* build */
    mxt_stub.info[4] = 0x0E;  /* matrix_xsize = 14 */
    mxt_stub.info[5] = 0x0A;  /* matrix_ysize = 10 */
    mxt_stub.info[6] = 0x00;  /* object_num = 0 (driver stops early) */
    /* bytes 7-9 = CRC (zeros — mismatch is just a warning) */
}

typedef struct OMAP3I2CState OMAP3I2CState;

struct OMAP3I2CState {
    MemoryRegion iomem;
    int bus_id;            /* 1, 2, or 3 */
    qemu_irq irq;         /* IRQ output to INTC */

    /* Registers */
    uint16_t rev;
    uint16_t ie;
    uint16_t stat;
    uint16_t we;
    uint16_t buf;
    uint16_t cnt;
    uint16_t sysc;
    uint16_t con;
    uint16_t oa;
    uint16_t sa;
    uint16_t psc;
    uint16_t scll;
    uint16_t sclh;

    /* Transfer state */
    uint8_t  data_buf[256];  /* internal FIFO for the transfer */
    int      data_pos;       /* current position in data_buf */
    int      data_len;       /* number of bytes in this transfer */
    bool     is_tx;          /* true = transmit (master writes to slave) */
    bool     transfer_active;
    bool     device_found;   /* true = slave ACKed the address */

    /* TWL5031 PMIC (on I2C1) */
    TWL5031State *twl;
    uint8_t  twl_reg_ptr;   /* current register pointer within slave */
    bool     twl_reg_set;   /* true = first TX byte set the register pointer */

    /* LM8323 keypad (on I2C3) */
    LM8323State *lm8323;
    uint8_t  lm8323_last_cmd;    /* persists across write→read transactions */
    int      lm8323_write_pos;   /* -1 = next byte is command, 0+ = data index */

    /* Generic sensor stubs (on I2C3) */
    SensorStubState *sensors;
    int              n_sensors;
    MXTStubState    *mxt;
};

/* Update the IRQ output based on STAT & IE */
static void omap3_i2c_update_irq(OMAP3I2CState *s)
{
    int level = (s->stat & s->ie) ? 1 : 0;
    qemu_set_irq(s->irq, level);
}

/* Find a generic sensor stub by address, or NULL */
static SensorStubState *find_sensor(OMAP3I2CState *s, uint8_t addr)
{
    int i;
    for (i = 0; i < s->n_sensors; i++) {
        if (s->sensors[i].addr == addr) {
            return &s->sensors[i];
        }
    }
    return NULL;
}

/* Look up an I2C device on the bus.  Returns true if a device exists. */
static bool omap3_i2c_has_device(OMAP3I2CState *s, uint8_t addr)
{
    if (s->twl && twl5031_is_address(addr)) {
        return true;
    }
    if (s->lm8323 && lm8323_is_address(addr)) {
        return true;
    }
    if (s->sensors && find_sensor(s, addr)) {
        return true;
    }
    if (s->mxt && addr == MXT_I2C_ADDR) {
        return true;
    }
    return false;
}

static void omap3_i2c_start_transfer(OMAP3I2CState *s)
{
    uint8_t addr = s->sa & 0x7F;

    s->transfer_active = true;
    s->data_pos = 0;
    s->data_len = s->cnt;
    s->is_tx = !!(s->con & I2C_CON_TRX);
    s->twl_reg_set = false;
    s->lm8323_write_pos = -1;  /* next TX byte will be the command */

    /* Reset per-transfer state for sensor stubs */
    if (s->sensors) {
        SensorStubState *sen = find_sensor(s, addr);
        if (sen) {
            sen->reg_set = false;
        }
    }
    if (s->mxt) {
        s->mxt->addr_pos = 0;
    }

    if (!omap3_i2c_has_device(s, addr)) {
        /* No device at this address: NACK */
        s->stat |= I2C_STAT_NACK | I2C_STAT_ARDY;
        s->device_found = false;
        s->transfer_active = false;
        /* Clear STT/STP in CON */
        s->con &= ~(I2C_CON_STT | I2C_CON_STP);
        omap3_i2c_update_irq(s);
        return;
    }

    s->device_found = true;

    if (s->is_tx) {
        /* Master transmit: controller needs data from CPU -> set XRDY */
        s->stat |= I2C_STAT_XRDY;
        s->stat |= I2C_STAT_BB;  /* bus busy */
    } else {
        /* Master receive: pre-fill data_buf from device */
        int len = s->data_len < 256 ? s->data_len : 256;

        if (s->lm8323 && lm8323_is_address(addr)) {
            lm8323_fill_read(s->lm8323, s->lm8323_last_cmd,
                             s->data_buf, len);
        } else if (s->twl && twl5031_is_address(addr)) {
            int i;
            for (i = 0; i < len; i++) {
                s->data_buf[i] = twl5031_read(s->twl, addr,
                                               (s->twl_reg_ptr + i) & 0xFF);
            }
        } else if (s->sensors && find_sensor(s, addr)) {
            SensorStubState *sen = find_sensor(s, addr);
            int i;
            for (i = 0; i < len; i++) {
                s->data_buf[i] = sen->regs[(sen->reg_ptr + i) & 0xFF];
            }
        } else if (s->mxt && addr == MXT_I2C_ADDR) {
            int i;
            for (i = 0; i < len; i++) {
                int idx = s->mxt->addr_ptr + i;
                s->data_buf[i] = (idx < 10) ? s->mxt->info[idx] : 0;
            }
        }
        s->stat |= I2C_STAT_RRDY;
        s->stat |= I2C_STAT_BB;
    }
    omap3_i2c_update_irq(s);
}

static uint64_t omap3630_i2c_read(void *opaque, hwaddr offset, unsigned size)
{
    OMAP3I2CState *s = (OMAP3I2CState *)opaque;

    switch (offset) {
    case 0x00:  /* I2C_REV */
        return 0x0040;  /* OMAP3630 I2C revision (IP v1 rev 4.0) */
    case 0x04:  /* I2C_IE */
        return s->ie;
    case 0x08:  /* I2C_STAT */
        return s->stat;
    case 0x0C:  /* I2C_WE */
        return s->we;
    case 0x10:  /* I2C_SYSS: RDONE = 1 */
        return 0x0001;
    case 0x14:  /* I2C_BUF */
        return s->buf;
    case 0x18:  /* I2C_CNT */
        return s->cnt;
    case 0x1C: { /* I2C_DATA */
        if (!s->transfer_active || s->is_tx) {
            return 0;
        }
        if (s->data_pos < s->data_len) {
            uint8_t val = s->data_buf[s->data_pos++];
            s->cnt = s->data_len - s->data_pos;
            if (s->data_pos >= s->data_len) {
                /* Transfer complete */
                s->stat &= ~(I2C_STAT_RRDY | I2C_STAT_RDR | I2C_STAT_BB);
                s->stat |= I2C_STAT_ARDY | I2C_STAT_BF;
                s->transfer_active = false;
                s->con &= ~(I2C_CON_STT | I2C_CON_STP);
                omap3_i2c_update_irq(s);
            }
            return val;
        }
        return 0;
    }
    case 0x20:  /* I2C_SYSC */
        return s->sysc;
    case 0x24:  /* I2C_CON */
        return s->con;
    case 0x28:  /* I2C_OA */
        return s->oa;
    case 0x2C:  /* I2C_SA */
        return s->sa;
    case 0x30:  /* I2C_PSC */
        return s->psc;
    case 0x34:  /* I2C_SCLL */
        return s->scll;
    case 0x38:  /* I2C_SCLH */
        return s->sclh;
    case 0x3C:  /* I2C_SYSTEST: SDA_I=1, SCL_I=1 (bus idle, lines high) */
        return I2C_SYSTEST_SCL_I_FUNC | I2C_SYSTEST_SDA_I_FUNC;
    case 0x40:  /* I2C_BUFSTAT: FIFODEPTH[15:14]=0 -> 8 bytes */
        return 0x0000;
    default:
        return 0;
    }
}

static void omap3630_i2c_write(void *opaque, hwaddr offset,
                                uint64_t value, unsigned size)
{
    OMAP3I2CState *s = (OMAP3I2CState *)opaque;

    switch (offset) {
    case 0x04:  /* I2C_IE */
        s->ie = value & 0xFFFF;
        omap3_i2c_update_irq(s);
        break;
    case 0x08:  /* I2C_STAT — write-1-to-clear */
        s->stat &= ~(value & 0xFFFF);
        /*
         * Re-assert data-ready flags if the transfer is still active.
         * Real hardware re-asserts XRDY/RRDY when FIFO has space/data
         * and bytes remain. Without this, multi-byte transfers stall
         * because the driver acks XRDY after each threshold-sized
         * chunk, then waits for XRDY to re-appear.
         */
        if (s->transfer_active && s->data_pos < s->data_len) {
            if (s->is_tx) {
                s->stat |= I2C_STAT_XRDY;
            } else {
                s->stat |= I2C_STAT_RRDY;
            }
        }
        omap3_i2c_update_irq(s);
        break;
    case 0x0C:  /* I2C_WE */
        s->we = value & 0xFFFF;
        break;
    case 0x14:  /* I2C_BUF */
        s->buf = value & 0xFFFF;
        break;
    case 0x18:  /* I2C_CNT */
        s->cnt = value & 0xFFFF;
        break;
    case 0x1C: { /* I2C_DATA */
        if (!s->transfer_active || !s->is_tx) {
            break;
        }
        if (s->data_pos < s->data_len) {
            uint8_t addr = s->sa & 0x7F;
            uint8_t byte = value & 0xFF;

            /*
             * Dispatch to the appropriate I2C slave device.
             * TWL5031: register-addressed (first byte = reg addr).
             * LM8323: command-based (first byte = command).
             */
            if (s->lm8323 && lm8323_is_address(addr)) {
                if (s->lm8323_write_pos < 0) {
                    /* First byte = command */
                    s->lm8323_last_cmd = byte;
                    s->lm8323_write_pos = 0;
                } else {
                    /* Subsequent bytes = command data */
                    lm8323_write_byte(s->lm8323, s->lm8323_last_cmd,
                                      s->lm8323_write_pos, byte);
                    s->lm8323_write_pos++;
                }
            } else if (s->twl && twl5031_is_address(addr)) {
                if (!s->twl_reg_set) {
                    s->twl_reg_ptr = byte;
                    s->twl_reg_set = true;
                } else {
                    twl5031_write(s->twl, addr, s->twl_reg_ptr, byte);
                    s->twl_reg_ptr++;
                }
            } else if (s->sensors) {
                SensorStubState *sen = find_sensor(s, addr);
                if (sen) {
                    if (!sen->reg_set) {
                        sen->reg_ptr = byte;
                        sen->reg_set = true;
                    } else {
                        sen->regs[sen->reg_ptr] = byte;
                        sen->reg_ptr++;
                    }
                }
            }
            if (s->mxt && addr == MXT_I2C_ADDR) {
                if (s->mxt->addr_pos == 0) {
                    s->mxt->addr_ptr = byte;            /* low byte */
                    s->mxt->addr_pos = 1;
                } else if (s->mxt->addr_pos == 1) {
                    s->mxt->addr_ptr |= (byte << 8);    /* high byte */
                    s->mxt->addr_pos = 2;
                }
                /* addr_pos == 2: additional writes ignored */
            }

            s->data_pos++;
            s->cnt = s->data_len - s->data_pos;

            if (s->data_pos >= s->data_len) {
                /* Transfer complete */
                s->stat &= ~(I2C_STAT_XRDY | I2C_STAT_XDR | I2C_STAT_BB);
                s->stat |= I2C_STAT_ARDY | I2C_STAT_BF;
                s->transfer_active = false;
                s->con &= ~(I2C_CON_STT | I2C_CON_STP);
                omap3_i2c_update_irq(s);
            }
        }
        break;
    }
    case 0x20:  /* I2C_SYSC */
        s->sysc = value & 0xFFFF;
        /* Check for soft reset (bit 1) */
        if (value & 0x02) {
            s->stat = 0;
            s->ie = 0;
            s->con = 0;
            s->cnt = 0;
            s->transfer_active = false;
            omap3_i2c_update_irq(s);
        }
        break;
    case 0x24:  /* I2C_CON */
        s->con = value & 0xFFFF;
        /* If STT (start) is set and EN+MST are set, begin transfer */
        if ((s->con & (I2C_CON_EN | I2C_CON_MST | I2C_CON_STT)) ==
            (I2C_CON_EN | I2C_CON_MST | I2C_CON_STT)) {
            omap3_i2c_start_transfer(s);
        }
        break;
    case 0x28:  /* I2C_OA */
        s->oa = value & 0xFFFF;
        break;
    case 0x2C:  /* I2C_SA */
        s->sa = value & 0xFFFF;
        break;
    case 0x30:  /* I2C_PSC */
        s->psc = value & 0xFFFF;
        break;
    case 0x34:  /* I2C_SCLL */
        s->scll = value & 0xFFFF;
        break;
    case 0x38:  /* I2C_SCLH */
        s->sclh = value & 0xFFFF;
        break;
    default:
        break;
    }
}

static const MemoryRegionOps omap3630_i2c_ops = {
    .read = omap3630_i2c_read,
    .write = omap3630_i2c_write,
    .endianness = DEVICE_LITTLE_ENDIAN,
    .valid.min_access_size = 2,
    .valid.max_access_size = 4,
};

/* I2C stubs for I2C1 and I2C2 (no devices attached — return NACK) */
static uint64_t omap3630_i2c_stub_read(void *opaque, hwaddr offset,
                                        unsigned size)
{
    switch (offset) {
    case 0x00:  /* I2C_REV */
        return 0x0040;
    case 0x08:  /* I2C_STAT: NACK bit (no slave ACK) */
        return I2C_STAT_NACK;
    case 0x10:  /* I2C_SYSS: RDONE = 1 */
        return 0x0001;
    case 0x24:  /* I2C_CON: return with STT cleared */
        return I2C_CON_EN;
    case 0x3C:  /* I2C_SYSTEST: SDA_I=1, SCL_I=1 */
        return I2C_SYSTEST_SCL_I_FUNC | I2C_SYSTEST_SDA_I_FUNC;
    default:
        return 0;
    }
}

static void omap3630_i2c_stub_write(void *opaque, hwaddr offset,
                                     uint64_t value, unsigned size)
{
}

static const MemoryRegionOps omap3630_i2c_stub_ops = {
    .read = omap3630_i2c_stub_read,
    .write = omap3630_i2c_stub_write,
    .endianness = DEVICE_LITTLE_ENDIAN,
    .valid.min_access_size = 2,
    .valid.max_access_size = 4,
};

/*
 * HSMMC stub — enough to stop busy-waits.
 */
static uint64_t omap3630_hsmmc_read(void *opaque, hwaddr offset, unsigned size)
{
    switch (offset) {
    case 0x010: /* MMCHS_SYSSTATUS: RESETDONE bit 0 = 1 */
        return 0x00000001;
    case 0x014: /* SYSSTATUS (alternative offset): RESETDONE */
        return 0x00000001;
    case 0x12C: /* MMCHS_SYSCTL: ICS (internal clock stable) bit 1 */
        return 0x00000002;
    case 0x128: /* MMCHS_PSTATE: no card, command inhibit clear */
        return 0x00000000;
    case 0x130: /* MMCHS_HCTL: return 0 */
        return 0;
    default:
        return 0;
    }
}

static void omap3630_hsmmc_write(void *opaque, hwaddr offset,
                                  uint64_t value, unsigned size)
{
}

static const MemoryRegionOps omap3630_hsmmc_ops = {
    .read = omap3630_hsmmc_read,
    .write = omap3630_hsmmc_write,
    .endianness = DEVICE_LITTLE_ENDIAN,
    .valid.min_access_size = 4,
    .valid.max_access_size = 4,
};

/*
 * ============================================================
 * OMAP3 GPIO Bank Model
 * ============================================================
 * Each bank has 32 GPIO lines.  OMAP24XX register layout.
 * Key behavior: IRQSTATUS is write-1-to-clear (W1C).
 * Returns 0 for IRQSTATUS by default — prevents interrupt storms.
 */

/* OMAP24XX GPIO register offsets (used by kernel gpio-omap.c) */
#define GPIO_REVISION        0x0000
#define GPIO_SYSCONFIG       0x0010
#define GPIO_SYSSTATUS       0x0014
#define GPIO_IRQSTATUS1      0x0018
#define GPIO_IRQENABLE1      0x001C
#define GPIO_WAKE_EN         0x0020
#define GPIO_IRQSTATUS2      0x0028
#define GPIO_IRQENABLE2      0x002C
#define GPIO_CTRL            0x0030
#define GPIO_OE              0x0034
#define GPIO_DATAIN          0x0038
#define GPIO_DATAOUT         0x003C
#define GPIO_LEVELDETECT0    0x0040
#define GPIO_LEVELDETECT1    0x0044
#define GPIO_RISINGDETECT    0x0048
#define GPIO_FALLINGDETECT   0x004C
#define GPIO_DEBOUNCE_EN     0x0050
#define GPIO_DEBOUNCE_VAL    0x0054
#define GPIO_CLEARIRQENABLE1 0x0060
#define GPIO_SETIRQENABLE1   0x0064
#define GPIO_CLEARIRQENABLE2 0x0070
#define GPIO_SETIRQENABLE2   0x0074
#define GPIO_CLEARWKUENA     0x0080
#define GPIO_SETWKUENA       0x0084
#define GPIO_CLEARDATAOUT    0x0090
#define GPIO_SETDATAOUT      0x0094

static void omap3_gpio_update_irq(OMAP3GPIOState *s)
{
    uint32_t active = s->irqstatus1 & s->irqenable1;
    qemu_set_irq(s->irq, active ? 1 : 0);
}

static uint64_t omap3_gpio_read(void *opaque, hwaddr offset, unsigned size)
{
    OMAP3GPIOState *s = opaque;

    switch (offset) {
    case GPIO_REVISION:
        return 0x0025;  /* OMAP3 GPIO revision 2.5 */
    case GPIO_SYSCONFIG:
        return s->sysconfig;
    case GPIO_SYSSTATUS:
        return 0x00000001;  /* RESETDONE = 1 */
    case GPIO_IRQSTATUS1:
        return s->irqstatus1;
    case GPIO_IRQENABLE1:
        return s->irqenable1;
    case GPIO_WAKE_EN:
        return s->wake_en;
    case GPIO_IRQSTATUS2:
        return s->irqstatus2;
    case GPIO_IRQENABLE2:
        return s->irqenable2;
    case GPIO_CTRL:
        return s->ctrl;
    case GPIO_OE:
        return s->oe;
    case GPIO_DATAIN:
        return s->datain;
    case GPIO_DATAOUT:
        return s->dataout;
    case GPIO_LEVELDETECT0:
        return s->leveldetect0;
    case GPIO_LEVELDETECT1:
        return s->leveldetect1;
    case GPIO_RISINGDETECT:
        return s->risingdetect;
    case GPIO_FALLINGDETECT:
        return s->fallingdetect;
    case GPIO_DEBOUNCE_EN:
        return s->debounce_en;
    case GPIO_DEBOUNCE_VAL:
        return s->debounce_val;
    case GPIO_CLEARIRQENABLE1:
    case GPIO_SETIRQENABLE1:
    case GPIO_CLEARIRQENABLE2:
    case GPIO_SETIRQENABLE2:
    case GPIO_CLEARWKUENA:
    case GPIO_SETWKUENA:
    case GPIO_CLEARDATAOUT:
    case GPIO_SETDATAOUT:
        return 0;  /* Write-only atomic registers */
    default:
        return 0;
    }
}

static void omap3_gpio_write(void *opaque, hwaddr offset,
                              uint64_t value, unsigned size)
{
    OMAP3GPIOState *s = opaque;

    switch (offset) {
    case GPIO_SYSCONFIG:
        s->sysconfig = value & 0x1F;
        if (value & 0x02) {
            /* Soft reset */
            s->irqstatus1 = 0;
            s->irqstatus2 = 0;
            s->irqenable1 = 0;
            s->irqenable2 = 0;
            s->oe = 0xFFFFFFFF;
            s->dataout = 0;
            s->ctrl = 0;
            s->wake_en = 0;
            s->leveldetect0 = 0;
            s->leveldetect1 = 0;
            s->risingdetect = 0;
            s->fallingdetect = 0;
            s->debounce_en = 0;
            s->debounce_val = 0;
            omap3_gpio_update_irq(s);
        }
        break;
    case GPIO_IRQSTATUS1:
        s->irqstatus1 &= ~value;  /* W1C */
        omap3_gpio_update_irq(s);
        break;
    case GPIO_IRQENABLE1:
        s->irqenable1 = value;
        omap3_gpio_update_irq(s);
        break;
    case GPIO_WAKE_EN:
        s->wake_en = value;
        break;
    case GPIO_IRQSTATUS2:
        s->irqstatus2 &= ~value;  /* W1C */
        break;
    case GPIO_IRQENABLE2:
        s->irqenable2 = value;
        break;
    case GPIO_CTRL:
        s->ctrl = value & 0x07;
        break;
    case GPIO_OE:
        s->oe = value;
        break;
    case GPIO_DATAOUT:
        s->dataout = value;
        break;
    case GPIO_LEVELDETECT0:
        s->leveldetect0 = value;
        break;
    case GPIO_LEVELDETECT1:
        s->leveldetect1 = value;
        break;
    case GPIO_RISINGDETECT:
        s->risingdetect = value;
        break;
    case GPIO_FALLINGDETECT:
        s->fallingdetect = value;
        break;
    case GPIO_DEBOUNCE_EN:
        s->debounce_en = value;
        break;
    case GPIO_DEBOUNCE_VAL:
        s->debounce_val = value & 0xFF;
        break;
    case GPIO_CLEARIRQENABLE1:
        s->irqenable1 &= ~value;
        omap3_gpio_update_irq(s);
        break;
    case GPIO_SETIRQENABLE1:
        s->irqenable1 |= value;
        omap3_gpio_update_irq(s);
        break;
    case GPIO_CLEARIRQENABLE2:
        s->irqenable2 &= ~value;
        break;
    case GPIO_SETIRQENABLE2:
        s->irqenable2 |= value;
        break;
    case GPIO_CLEARWKUENA:
        s->wake_en &= ~value;
        break;
    case GPIO_SETWKUENA:
        s->wake_en |= value;
        break;
    case GPIO_CLEARDATAOUT:
        s->dataout &= ~value;
        break;
    case GPIO_SETDATAOUT:
        s->dataout |= value;
        break;
    default:
        break;
    }
}

static const MemoryRegionOps omap3_gpio_ops = {
    .read = omap3_gpio_read,
    .write = omap3_gpio_write,
    .endianness = DEVICE_LITTLE_ENDIAN,
    .valid.min_access_size = 1,
    .valid.max_access_size = 4,
};

static void omap3_gpio_init(OMAP3GPIOState *s, Object *owner,
                             int bank_id, qemu_irq irq)
{
    char name[32];

    memset(s, 0, sizeof(*s));
    s->bank_id = bank_id;
    s->irq = irq;
    s->oe = 0xFFFFFFFF;  /* All pins input by default */

    snprintf(name, sizeof(name), "omap3-gpio%d", bank_id);
    memory_region_init_io(&s->iomem, owner, &omap3_gpio_ops, s, name, 0x1000);
}

/*
 * Set an external input pin on a GPIO bank.
 * pin = 0-31 within the bank, level = 0/1.
 * Fires edge/level interrupts as configured.
 * Called by peripheral models to simulate GPIO inputs (e.g., sensor IRQs).
 */
static void __attribute__((unused))
omap3_gpio_set_pin(OMAP3GPIOState *s, int pin, bool level)
{
    uint32_t mask = 1u << pin;
    uint32_t old_datain = s->datain;

    if (level) {
        s->datain |= mask;
    } else {
        s->datain &= ~mask;
    }

    /* Edge detection */
    uint32_t rising  = s->datain & ~old_datain;
    uint32_t falling = ~s->datain & old_datain;

    s->irqstatus1 |= (rising & s->risingdetect);
    s->irqstatus1 |= (falling & s->fallingdetect);

    /* Level detection */
    s->irqstatus1 |= (s->datain & s->leveldetect1);
    s->irqstatus1 |= (~s->datain & s->leveldetect0);

    omap3_gpio_update_irq(s);
}

/*
 * Generic OMAP3 peripheral stub — returns SYSSTATUS bit 0 = 1 (reset done)
 * at offset 0x014.  This prevents omap_hwmod from spinning 10ms per device
 * waiting for softreset completion.  All other reads return 0.
 */
typedef struct {
    MemoryRegion iomem;
    const char *name;
} OMAP3PeriphStub;

static uint64_t omap3_periph_stub_read(void *opaque, hwaddr offset,
                                        unsigned size)
{
    /*
     * Return bit 0 = 1 for all reads.  The omap_hwmod and ti-sysc
     * frameworks poll SYSSTATUS at various offsets (0x014, 0x028,
     * 0x044, 0x04C, 0x408, ...) checking bit 0 = RESETDONE.
     * Returning 1 everywhere makes all softresets complete instantly.
     * This is safe because unused register reads returning 1 are
     * harmless to the kernel — no driver interprets bit 0 = 1 as
     * anything dangerous for status/config registers.
     */
    return 0x00000001;
}

static void omap3_periph_stub_write(void *opaque, hwaddr offset,
                                     uint64_t value, unsigned size)
{
    /* Accept all writes silently */
}

static const MemoryRegionOps omap3_periph_stub_ops = {
    .read = omap3_periph_stub_read,
    .write = omap3_periph_stub_write,
    .endianness = DEVICE_LITTLE_ENDIAN,
    .valid.min_access_size = 1,
    .valid.max_access_size = 4,
};

/*
 * NLoader UART shim — translates NLoader's UART register layout to console output.
 *
 * NLoader's serial driver uses a non-standard register layout:
 *   offset 0x08: status register (bit 4 = TX ready, polled before each byte)
 *   offset 0x1C: TX data register (write byte here to transmit)
 *   offset 0x08: write 0x10 to acknowledge after TX
 *
 * This shim captures writes to offset 0x1C and outputs them via qemu_log,
 * while always reporting TX ready at offset 0x08.
 *
 * Mapped at 0x401F0000 (unused L4 peripheral space).
 */
#define NLOADER_UART_BASE  0x401F0000
#define NLOADER_UART_SIZE  0x100

static uint64_t nloader_uart_read(void *opaque, hwaddr offset, unsigned size)
{
    switch (offset) {
    case 0x08:
        return 0x10;   /* bit 4 = TX ready, always set */
    default:
        return 0;
    }
}

/* Buffer for accumulating NLoader output lines */
static char nl_uart_buf[256];
static int nl_uart_pos;

static void nloader_uart_write(void *opaque, hwaddr offset,
                                uint64_t value, unsigned size)
{
    if (offset == 0x1C) {
        char ch = (char)(value & 0xFF);
        if (ch == '\n' || ch == '\0' ||
            nl_uart_pos >= (int)sizeof(nl_uart_buf) - 1) {
            nl_uart_buf[nl_uart_pos] = '\0';
            if (nl_uart_pos > 0) {
                info_report("NLoader: %s", nl_uart_buf);
            }
            nl_uart_pos = 0;
        } else if (ch >= 0x20 || ch == '\t') {
            nl_uart_buf[nl_uart_pos++] = ch;
        }
    }
}

static const MemoryRegionOps nloader_uart_ops = {
    .read = nloader_uart_read,
    .write = nloader_uart_write,
    .endianness = DEVICE_LITTLE_ENDIAN,
    .valid.min_access_size = 1,
    .valid.max_access_size = 4,
};

/*
 * SCM (System Control Module) — register-backed model.
 *
 * Base: 0x48002000, size: 0x2000 (8KB).
 *
 * Stores all writes in a backing array so pad mux configuration
 * written by NLoader's GENIO_INIT processing can be read back.
 * Pad mux registers are at offsets 0x030-0x264 (PADCONFS).
 *
 * CONTROL_STATUS (offset 0x2F0): bits [10:8] = DEVICETYPE.
 *   0=test, 1=EMU, 2=HS, 3=GP.
 * Must return GP (3) so PM init skips secure-mode SMC calls.
 */
static uint64_t omap3630_scm_read(void *opaque, hwaddr offset, unsigned size)
{
    OMAP3630State *s = opaque;

    if (offset >= OMAP3630_SCM_SIZE) {
        return 0;
    }

    switch (offset) {
    case 0x2F0:  /* CONTROL_STATUS: DEVICETYPE=GP (3 << 8) */
        return 0x00000300;
    default:
        return s->scm_regs[offset / 4];
    }
}

static void omap3630_scm_write(void *opaque, hwaddr offset,
                                uint64_t value, unsigned size)
{
    OMAP3630State *s = opaque;

    if (offset >= OMAP3630_SCM_SIZE) {
        return;
    }

    s->scm_regs[offset / 4] = (uint32_t)value;

    /* Log pad mux writes — NLoader GENIO_INIT writes (addr, value) pairs
     * to PADCONF registers at offsets 0x030-0x264 */
    if (offset >= 0x030 && offset <= 0x264) {
        info_report("SCM PADCONF %08X : %08X",
                     (uint32_t)(OMAP3630_SCM_BASE + offset),
                     (uint32_t)value);
    }
}

static const MemoryRegionOps omap3630_scm_ops = {
    .read = omap3630_scm_read,
    .write = omap3630_scm_write,
    .endianness = DEVICE_LITTLE_ENDIAN,
    .valid.min_access_size = 4,
    .valid.max_access_size = 4,
};

/*
 * SDRC (SDRAM Controller) — register-level model for PM idle path.
 *
 * Base: 0x6D000000, size: 0x1000.
 *
 * The OMAP3 PM idle code (sleep34xx.S) runs from SRAM after WFI and
 * polls SDRC registers to verify the SDRAM is ready:
 *
 *   1. Reads SDRC_DLLA_CTRL (0x060) bit 2:
 *      - If set → DLL is in lock mode, skip DLL wait, exit immediately
 *      - If clear → poll SDRC_DLLA_STATUS (0x064) bit 2 for DLL locked
 *
 *   2. Writes to SDRC_POWER (0x070) to enable/disable self-refresh on
 *      idle (bit 6).
 *
 * Without this model, DLLA_CTRL returns 0 (from unimplemented device),
 * causing an infinite poll on DLLA_STATUS (also 0) — the system hangs.
 *
 * Solution: default DLLA_CTRL to 0x0C (bit 3 = DLL enabled, bit 2 = DLL
 * in lock mode).  This makes the SRAM code take the fast exit path.
 * DLLA_STATUS bit 2 is derived from DLLA_CTRL bit 3 (enabled → locked).
 */
static uint64_t omap3630_sdrc_read(void *opaque, hwaddr offset, unsigned size)
{
    OMAP3630State *s = OMAP3630(opaque);

    switch (offset) {
    case 0x000:  /* SDRC_REVISION */
        return 0x00000040;  /* OMAP3 SDRC revision 4.0 */
    case 0x010:  /* SDRC_SYSCONFIG */
        return s->sdrc_sysconfig;
    case 0x014:  /* SDRC_SYSSTATUS: RESETDONE bit 0 = 1 */
        return 0x00000001;
    case 0x040:  /* SDRC_CS_CFG */
        return s->sdrc_cs_cfg;
    case 0x044:  /* SDRC_SHARING */
        return s->sdrc_sharing;
    case 0x048:  /* SDRC_ERR_ADDR */
        return 0;
    case 0x04C:  /* SDRC_ERR_TYPE */
        return 0;
    case 0x060:  /* SDRC_DLLA_CTRL */
        return s->sdrc_dlla_ctrl;
    case 0x064:  /* SDRC_DLLA_STATUS */
        /*
         * Bit 2 = DLL lock status.  Report locked when DLL is enabled
         * (DLLA_CTRL bit 3).  Also set bit 1 (phase counter).
         */
        if (s->sdrc_dlla_ctrl & (1 << 3)) {
            return 0x00000004;  /* DLL locked */
        }
        return 0;  /* DLL disabled → not locked */
    case 0x068:  /* SDRC_DLLB_CTRL (not used on OMAP3630, mirror A) */
        return s->sdrc_dlla_ctrl;
    case 0x06C:  /* SDRC_DLLB_STATUS */
        if (s->sdrc_dlla_ctrl & (1 << 3)) {
            return 0x00000004;
        }
        return 0;
    case 0x070:  /* SDRC_POWER */
        return s->sdrc_power;
    case 0x080:  /* SDRC_MCFG_0 */
        return s->sdrc_mcfg_0;
    case 0x084:  /* SDRC_MR_0 */
        return s->sdrc_mr_0;
    case 0x09C:  /* SDRC_ACTIM_CTRLA_0 */
        return s->sdrc_actim_ctrla_0;
    case 0x0A0:  /* SDRC_ACTIM_CTRLB_0 */
        return s->sdrc_actim_ctrlb_0;
    case 0x0A4:  /* SDRC_RFR_CTRL_0 */
        return s->sdrc_rfr_ctrl_0;
    default:
        return 0;
    }
}

static void omap3630_sdrc_write(void *opaque, hwaddr offset,
                                 uint64_t value, unsigned size)
{
    OMAP3630State *s = OMAP3630(opaque);

    switch (offset) {
    case 0x010:  /* SDRC_SYSCONFIG */
        s->sdrc_sysconfig = value;
        break;
    case 0x040:  /* SDRC_CS_CFG */
        s->sdrc_cs_cfg = value;
        break;
    case 0x044:  /* SDRC_SHARING */
        s->sdrc_sharing = value;
        break;
    case 0x060:  /* SDRC_DLLA_CTRL */
        s->sdrc_dlla_ctrl = value;
        break;
    case 0x070:  /* SDRC_POWER */
        s->sdrc_power = value;
        break;
    case 0x080:  /* SDRC_MCFG_0 */
        s->sdrc_mcfg_0 = value;
        break;
    case 0x084:  /* SDRC_MR_0 */
        s->sdrc_mr_0 = value;
        break;
    case 0x09C:  /* SDRC_ACTIM_CTRLA_0 */
        s->sdrc_actim_ctrla_0 = value;
        break;
    case 0x0A0:  /* SDRC_ACTIM_CTRLB_0 */
        s->sdrc_actim_ctrlb_0 = value;
        break;
    case 0x0A4:  /* SDRC_RFR_CTRL_0 */
        s->sdrc_rfr_ctrl_0 = value;
        break;
    default:
        break;
    }
}

static const MemoryRegionOps omap3630_sdrc_ops = {
    .read = omap3630_sdrc_read,
    .write = omap3630_sdrc_write,
    .endianness = DEVICE_LITTLE_ENDIAN,
    .valid.min_access_size = 4,
    .valid.max_access_size = 4,
};

/*
 * sDMA Controller (System DMA) — 32-channel instant-completion model.
 *
 * Base: 0x48056000, size: 0x1000.
 * 32 channels, stride 0x60, channel regs start at offset 0x080.
 * 4 IRQ lines: L0=IRQ12 (not wired, virtio), L1=13, L2=14, L3=15.
 *
 * "Instant completion": when CCR.ENABLE is set, the transfer completes
 * immediately — CSR.BLOCK is set, CCR.ENABLE is cleared, IRQSTATUS
 * updated.  No actual data movement occurs.  The Linux omap-dma driver
 * detects completion by polling CCR (ENABLE=0) or via IRQSTATUS_L1 IRQ.
 */

#define SDMA_NUM_CHANNELS    32
#define SDMA_CHANNEL_BASE    0x80
#define SDMA_CHANNEL_STRIDE  0x60

#define SDMA_CCR_ENABLE      (1 << 7)

#define SDMA_CSR_BLOCK       (1 << 5)

typedef struct {
    uint32_t ccr;
    uint32_t clnk_ctrl;
    uint32_t cicr;
    uint32_t csr;
    uint32_t csdp;
    uint32_t cen;
    uint32_t cfn;
    uint32_t cssa;
    uint32_t cdsa;
    uint32_t csei;
    uint32_t csfi;
    uint32_t cdei;
    uint32_t cdfi;
    uint32_t cdac;
    uint32_t ccen;
    uint32_t ccfn;
    uint32_t color;
    uint32_t cdp;
    uint32_t cndp;
    uint32_t ccdn;
} OMAP3SDMAChannel;

static struct {
    uint32_t irqstatus[4];
    uint32_t irqenable[4];
    uint32_t gcr;
    uint32_t ocp_sysconfig;
    OMAP3SDMAChannel ch[SDMA_NUM_CHANNELS];
    qemu_irq irq[4];
} sdma_state;

static void omap3_sdma_update_irq(void)
{
    /* L0 (IRQ 12) not wired — shared with virtio-mmio */
    int i;
    for (i = 1; i < 4; i++) {
        if (sdma_state.irq[i]) {
            qemu_set_irq(sdma_state.irq[i],
                (sdma_state.irqstatus[i] & sdma_state.irqenable[i]) ? 1 : 0);
        }
    }
}

static uint64_t omap3_sdma_read(void *opaque, hwaddr offset, unsigned size)
{
    /* Global registers */
    switch (offset) {
    case 0x00:  return 0x00000040;  /* DMA4_REVISION (OMAP3) */
    case 0x08:  return sdma_state.irqstatus[0];
    case 0x0C:  return sdma_state.irqstatus[1];
    case 0x10:  return sdma_state.irqstatus[2];
    case 0x14:  return sdma_state.irqstatus[3];
    case 0x18:  return sdma_state.irqenable[0];
    case 0x1C:  return sdma_state.irqenable[1];
    case 0x20:  return sdma_state.irqenable[2];
    case 0x24:  return sdma_state.irqenable[3];
    case 0x28:  return 0x00000001;  /* DMA4_SYSSTATUS: RESETDONE */
    case 0x2C:  return sdma_state.ocp_sysconfig;
    case 0x64:  return 0;  /* DMA4_CAPS_0: no LL123 support */
    case 0x6C:  return 0;  /* DMA4_CAPS_2 */
    case 0x70:  return 0;  /* DMA4_CAPS_3 */
    case 0x74:  return 0;  /* DMA4_CAPS_4 */
    case 0x78:  return sdma_state.gcr;
    }

    /* Per-channel registers: base 0x80, stride 0x60 */
    if (offset >= SDMA_CHANNEL_BASE) {
        uint32_t ch_off = offset - SDMA_CHANNEL_BASE;
        uint32_t ch = ch_off / SDMA_CHANNEL_STRIDE;
        uint32_t reg = ch_off % SDMA_CHANNEL_STRIDE;
        OMAP3SDMAChannel *c;

        if (ch >= SDMA_NUM_CHANNELS) {
            return 0;
        }
        c = &sdma_state.ch[ch];

        switch (reg) {
        case 0x00:  return c->ccr;
        case 0x04:  return c->clnk_ctrl;
        case 0x08:  return c->cicr;
        case 0x0C:  return c->csr;
        case 0x10:  return c->csdp;
        case 0x14:  return c->cen;
        case 0x18:  return c->cfn;
        case 0x1C:  return c->cssa;
        case 0x20:  return c->cdsa;
        case 0x24:  return c->csei;
        case 0x28:  return c->csfi;
        case 0x2C:  return c->cdei;
        case 0x30:  return c->cdfi;
        case 0x34:  return 0;  /* CSAC: read-only counter */
        case 0x38:  return c->cdac;
        case 0x3C:  return c->ccen;
        case 0x40:  return c->ccfn;
        case 0x44:  return c->color;
        case 0x50:  return c->cdp;
        case 0x54:  return c->cndp;
        case 0x58:  return c->ccdn;
        default:    return 0;
        }
    }

    return 0;
}

static void omap3_sdma_write(void *opaque, hwaddr offset,
                              uint64_t value, unsigned size)
{
    int i;

    /* Global registers */
    switch (offset) {
    case 0x08:  /* IRQSTATUS_L0: write-1-to-clear */
        sdma_state.irqstatus[0] &= ~value;
        omap3_sdma_update_irq();
        return;
    case 0x0C:
        sdma_state.irqstatus[1] &= ~value;
        omap3_sdma_update_irq();
        return;
    case 0x10:
        sdma_state.irqstatus[2] &= ~value;
        omap3_sdma_update_irq();
        return;
    case 0x14:
        sdma_state.irqstatus[3] &= ~value;
        omap3_sdma_update_irq();
        return;
    case 0x18:
        sdma_state.irqenable[0] = value;
        omap3_sdma_update_irq();
        return;
    case 0x1C:
        sdma_state.irqenable[1] = value;
        omap3_sdma_update_irq();
        return;
    case 0x20:
        sdma_state.irqenable[2] = value;
        omap3_sdma_update_irq();
        return;
    case 0x24:
        sdma_state.irqenable[3] = value;
        omap3_sdma_update_irq();
        return;
    case 0x2C:
        sdma_state.ocp_sysconfig = value;
        return;
    case 0x78:
        sdma_state.gcr = value;
        return;
    }

    /* Per-channel registers */
    if (offset >= SDMA_CHANNEL_BASE) {
        uint32_t ch_off = offset - SDMA_CHANNEL_BASE;
        uint32_t ch = ch_off / SDMA_CHANNEL_STRIDE;
        uint32_t reg = ch_off % SDMA_CHANNEL_STRIDE;
        OMAP3SDMAChannel *c;

        if (ch >= SDMA_NUM_CHANNELS) {
            return;
        }
        c = &sdma_state.ch[ch];

        switch (reg) {
        case 0x00:  /* CCR */
            c->ccr = value;
            if (value & SDMA_CCR_ENABLE) {
                /*
                 * Instant completion: clear ENABLE + activity bits,
                 * set CSR.BLOCK, update IRQSTATUS for all 4 lines.
                 */
                c->ccr &= ~SDMA_CCR_ENABLE;
                c->csr |= SDMA_CSR_BLOCK;
                for (i = 0; i < 4; i++) {
                    sdma_state.irqstatus[i] |= (1u << ch);
                }
                omap3_sdma_update_irq();
            }
            break;
        case 0x04:  c->clnk_ctrl = value; break;
        case 0x08:  c->cicr = value; break;
        case 0x0C:  /* CSR: write-1-to-clear */
            c->csr &= ~value;
            break;
        case 0x10:  c->csdp = value; break;
        case 0x14:  c->cen = value; break;
        case 0x18:  c->cfn = value; break;
        case 0x1C:  c->cssa = value; break;
        case 0x20:  c->cdsa = value; break;
        case 0x24:  c->csei = value; break;
        case 0x28:  c->csfi = value; break;
        case 0x2C:  c->cdei = value; break;
        case 0x30:  c->cdfi = value; break;
        case 0x38:  c->cdac = value; break;
        case 0x3C:  c->ccen = value; break;
        case 0x40:  c->ccfn = value; break;
        case 0x44:  c->color = value; break;
        case 0x50:  c->cdp = value; break;
        case 0x54:  c->cndp = value; break;
        case 0x58:  c->ccdn = value; break;
        default:    break;
        }
    }
}

static const MemoryRegionOps omap3_sdma_ops = {
    .read = omap3_sdma_read,
    .write = omap3_sdma_write,
    .endianness = DEVICE_LITTLE_ENDIAN,
    .valid.min_access_size = 4,
    .valid.max_access_size = 4,
};

/*
 * ==========================================================================
 * GPMC (General-Purpose Memory Controller) — register-level model.
 *
 * Base: 0x6E000000, size: 0x1000.
 * Manages chip-select windows for OneNAND flash, NOR, SRAM.
 * 8 chip-selects with 7 CONFIG registers each.
 *
 * CS0 pre-configured for OneNAND at 0x08000000 (128MB window).
 * ==========================================================================
 */
typedef struct {
    uint32_t sysconfig;
    uint32_t irqstatus;
    uint32_t irqenable;
    uint32_t config;               /* GPMC_CONFIG (0x50) */
    uint32_t cs_config[8][7];      /* CS0-CS7, CONFIG1-CONFIG7 */
} OMAP3GPMCState;

static OMAP3GPMCState gpmc_state;

static void omap3_gpmc_init_state(void)
{
    memset(&gpmc_state, 0, sizeof(gpmc_state));

    /*
     * CS0 CONFIG7: map OneNAND at 0x08000000, 128MB window.
     * Bits [5:0] = BASEADDRESS (0x08 → addr[29:24] = 0x08),
     * Bit 6 = CSVALID (1),
     * Bits [11:8] = MASKADDRESS (0x8 → 128MB).
     */
    gpmc_state.cs_config[0][6] = (0x8 << 8) | (1 << 6) | 0x08;
}

static uint64_t omap3_gpmc_read(void *opaque, hwaddr offset, unsigned size)
{
    switch (offset) {
    case 0x000: return 0x00000050;  /* GPMC_REVISION: 5.0 (OMAP3) */
    case 0x010: return gpmc_state.sysconfig;
    case 0x014: return 0x00000001;  /* GPMC_SYSSTATUS: RESETDONE */
    case 0x018: return gpmc_state.irqstatus;
    case 0x01C: return gpmc_state.irqenable;
    case 0x040: return 0;           /* GPMC_TIMEOUT_CONTROL */
    case 0x044: return 0;           /* GPMC_ERR_ADDRESS */
    case 0x048: return 0;           /* GPMC_ERR_TYPE */
    case 0x050: return gpmc_state.config;
    case 0x054: return 0;           /* GPMC_STATUS: no wait */
    default:
        break;
    }

    /* Per-CS config registers: CS0 at 0x060, stride 0x30 */
    if (offset >= 0x060 && offset < 0x1E0) {
        int cs = (offset - 0x060) / 0x30;
        int reg = ((offset - 0x060) % 0x30) / 4;
        if (cs < 8 && reg < 7) {
            return gpmc_state.cs_config[cs][reg];
        }
    }

    /* Prefetch engine */
    if (offset >= 0x1E0 && offset <= 0x1FC) {
        return 0;
    }

    /* ECC registers */
    if (offset >= 0x200 && offset < 0x300) {
        return 0;
    }

    return 0;
}

static void omap3_gpmc_write(void *opaque, hwaddr offset,
                              uint64_t value, unsigned size)
{
    switch (offset) {
    case 0x010: gpmc_state.sysconfig = value; return;
    case 0x018: gpmc_state.irqstatus &= ~value; return;  /* W1C */
    case 0x01C: gpmc_state.irqenable = value; return;
    case 0x050: gpmc_state.config = value; return;
    default: break;
    }

    /* Per-CS config registers */
    if (offset >= 0x060 && offset < 0x1E0) {
        int cs = (offset - 0x060) / 0x30;
        int reg = ((offset - 0x060) % 0x30) / 4;
        if (cs < 8 && reg < 7) {
            gpmc_state.cs_config[cs][reg] = value;
        }
    }
}

static const MemoryRegionOps omap3_gpmc_ops = {
    .read = omap3_gpmc_read,
    .write = omap3_gpmc_write,
    .endianness = DEVICE_LITTLE_ENDIAN,
    .valid.min_access_size = 4,
    .valid.max_access_size = 4,
};

/*
 * ==========================================================================
 * OneNAND flash — Samsung KFM8G16Q2M (8Gbit MLC).
 * Memory-mapped at GPMC CS0 base (0x08000000).
 *
 * Samsung OneNAND uses WORD addresses internally; the GPMC presents them
 * as byte offsets = word_addr × 2.  Linux ONENAND_MEMORY_MAP(x) = (x)<<1.
 *
 * Geometry (KFM8G16Q2M-DEBx):
 *   Page size:     2 KB  (2048 bytes main + 64 bytes spare)
 *   Pages/block:   64
 *   Block size:    128 KB (64 × 2 KB)
 *   Blocks:        8192  (= 1 GB / 128 KB)
 *   Total:         1 GB main data
 *
 * Memory map (byte offsets from CS0 base):
 *   0x00000 – 0x003FF  BootRAM        (1 KB)
 *   0x00400 – 0x00BFF  DataRAM0 pg0   (2 KB)
 *   0x00C00 – 0x013FF  DataRAM0 pg1   (2 KB)
 *   0x01400 – 0x01BFF  DataRAM1 pg0   (2 KB)
 *   0x01C00 – 0x023FF  DataRAM1 pg1   (2 KB)
 *   0x10020 – 0x1005F  SpareRAM0      (64 B)
 *   0x10060 – 0x1009F  SpareRAM1      (64 B) (not used yet)
 *   0x1E000+           Registers
 *
 * Supports READ (0x00) command with backing-store page fetch.
 * If no backing file provided, returns 0xFF (erased flash pattern).
 * ==========================================================================
 */

/* OneNAND byte offsets = ONENAND_MEMORY_MAP(word_addr) = word_addr << 1 */
#define ONENAND_OFF_BOOTRAM         0x00000
#define ONENAND_OFF_DATARAM0        0x00400  /* 0x0200 << 1 */
#define ONENAND_OFF_DATARAM1        0x01400  /* 0x0A00 << 1 */
#define ONENAND_OFF_SPARERAM0       0x10020  /* 0x8010 << 1 */
#define ONENAND_OFF_SPARERAM1       0x10060  /* 0x8030 << 1 */

/* Register byte offsets (word_addr << 1) */
#define ONENAND_REG_MANUF_ID        0x1E000  /* 0xF000 << 1 */
#define ONENAND_REG_DEVICE_ID       0x1E002  /* 0xF001 << 1 */
#define ONENAND_REG_VERSION_ID      0x1E004  /* 0xF002 << 1 */
#define ONENAND_REG_DATA_BUF_SIZE   0x1E006  /* 0xF003 << 1 */
#define ONENAND_REG_BOOT_BUF_SIZE   0x1E008  /* 0xF004 << 1 */
#define ONENAND_REG_NUM_BUFFERS     0x1E00A  /* 0xF005 << 1 */
#define ONENAND_REG_TECHNOLOGY      0x1E00C  /* 0xF006 << 1 */

#define ONENAND_REG_START_ADDR1     0x1E200  /* 0xF100 << 1 — DFS + FBA */
#define ONENAND_REG_START_ADDR2     0x1E202  /* 0xF101 << 1 — DBS + FBA (buf) */
#define ONENAND_REG_START_ADDR3     0x1E204  /* 0xF102 << 1 */
#define ONENAND_REG_START_ADDR4     0x1E206  /* 0xF103 << 1 */
#define ONENAND_REG_START_ADDR5     0x1E208  /* 0xF104 << 1 */
#define ONENAND_REG_START_ADDR6     0x1E20A  /* 0xF105 << 1 */
#define ONENAND_REG_START_ADDR7     0x1E20C  /* 0xF106 << 1 */
#define ONENAND_REG_START_ADDR8     0x1E20E  /* 0xF107 << 1 — FPA + FSA */

#define ONENAND_REG_START_BUFFER    0x1E400  /* 0xF200 << 1 */
#define ONENAND_REG_COMMAND         0x1E440  /* 0xF220 << 1 */
#define ONENAND_REG_SYS_CFG1       0x1E442  /* 0xF221 << 1 */
#define ONENAND_REG_SYS_CFG2       0x1E444  /* 0xF222 << 1 */
#define ONENAND_REG_CTRL_STATUS     0x1E480  /* 0xF240 << 1 */
#define ONENAND_REG_INTERRUPT       0x1E482  /* 0xF241 << 1 */
#define ONENAND_REG_START_BLOCK     0x1E498  /* 0xF24C << 1 */
#define ONENAND_REG_END_BLOCK       0x1E49A  /* 0xF24D << 1 */
#define ONENAND_REG_WP_STATUS       0x1E49C  /* 0xF24E << 1 */
#define ONENAND_REG_ECC_STATUS      0x1FE00  /* 0xFF00 << 1 */

/* OneNAND commands */
#define ONENAND_CMD_READ            0x0000
#define ONENAND_CMD_READOOB         0x0013
#define ONENAND_CMD_PROG            0x0080
#define ONENAND_CMD_UNLOCK          0x0023
#define ONENAND_CMD_LOCK            0x002A
#define ONENAND_CMD_UNLOCK_ALL      0x0027
#define ONENAND_CMD_ERASE           0x0094
#define ONENAND_CMD_RESET           0x00F0
#define ONENAND_CMD_READID          0x0090

/* Interrupt register bits */
#define ONENAND_INT_MASTER          (1 << 15)
#define ONENAND_INT_READ            (1 << 7)
#define ONENAND_INT_WRITE           (1 << 6)
#define ONENAND_INT_ERASE           (1 << 5)
#define ONENAND_INT_RESET           (1 << 4)

/* Controller status bits */
#define ONENAND_CTRL_ONGO           (1 << 15)
#define ONENAND_CTRL_RSTB           (1 << 7)

/* Device geometry */
#define ONENAND_PAGE_SIZE           2048
#define ONENAND_SPARE_SIZE          64
#define ONENAND_PAGES_PER_BLOCK     64
#define ONENAND_BLOCK_SIZE          (ONENAND_PAGE_SIZE * ONENAND_PAGES_PER_BLOCK)  /* 128KB */
#define ONENAND_NUM_BLOCKS          8192
#define ONENAND_TOTAL_SIZE          ((uint64_t)ONENAND_BLOCK_SIZE * ONENAND_NUM_BLOCKS)

/* Start Address 8 field extraction */
#define ONENAND_FPA_MASK            0x7F   /* page within block (bits 8:2) */
#define ONENAND_FPA_SHIFT           2
#define ONENAND_FSA_MASK            0x03   /* sector within page (bits 1:0) */

/* Start Buffer register */
#define ONENAND_BSA_SHIFT           8
#define ONENAND_BSA_DATARAM1        (3 << 2)

static struct {
    uint8_t *data;          /* backing store (mmap'd or malloc'd from file) */
    size_t   data_size;

    /*
     * BufferRAM: DataRAM0/1 each hold 1 page (2KB main + 64B spare).
     * We model both DataRAM pages (2 × 2KB) + SpareRAM0 (64B).
     * DataRAM1 is at +0x1000 offset from DataRAM0 in the byte map.
     */
    uint8_t  dataram[2][ONENAND_PAGE_SIZE * 2];   /* DataRAM0/1, 2 pages each */
    uint8_t  spareram[2][ONENAND_SPARE_SIZE];      /* SpareRAM0/1 */
    uint8_t  bootram[1024];                        /* BootRAM */

    /* Address registers */
    uint16_t start_addr1;   /* DFS[15] + FBA[14:0]: block address */
    uint16_t start_addr2;   /* DBS[15] + FBA (buf select) */
    uint16_t start_addr8;   /* FPA[8:2] + FSA[1:0]: page/sector in block */
    uint16_t start_buffer;  /* BSA[11:8] + BSC[2:0]: buffer address/count */

    /* Command and status */
    uint16_t command;
    uint16_t sys_cfg1;      /* System configuration 1 */
    uint16_t sys_cfg2;      /* System configuration 2 */
    uint16_t ctrl_status;   /* Controller status (ONENAND_CTRL_RSTB when ready) */
    uint16_t interrupt;     /* Interrupt status */
    uint16_t wp_status;     /* Write protection status */
    uint16_t start_block;   /* Start block for lock/unlock */
    uint16_t end_block;     /* End block for lock/unlock */
} onenand_state;

/*
 * omap3_onenand_init_state — set up initial register values.
 * Called from omap3630_realize() before memory regions are mapped.
 */
static void omap3_onenand_init_state(void)
{
    memset(&onenand_state, 0, sizeof(onenand_state));
    onenand_state.ctrl_status = ONENAND_CTRL_RSTB;  /* ready after reset */
    onenand_state.interrupt = ONENAND_INT_MASTER | ONENAND_INT_RESET;
    onenand_state.wp_status = 0x0001;  /* unlocked */
    onenand_state.sys_cfg1 = 0x40E0;  /* sync read, ECC on, RDY, INT */
    memset(onenand_state.dataram, 0xFF, sizeof(onenand_state.dataram));
    memset(onenand_state.spareram, 0xFF, sizeof(onenand_state.spareram));
    memset(onenand_state.bootram, 0xFF, sizeof(onenand_state.bootram));
}

/*
 * omap3_onenand_load_image — set the backing store for the OneNAND model.
 * The caller owns the buffer; it must remain valid for the machine's lifetime.
 */
void omap3_onenand_load_image(uint8_t *data, size_t size)
{
    onenand_state.data = data;
    onenand_state.data_size = size;
}

/*
 * Execute a OneNAND command.  Currently supports:
 *   0x00 READ       — load page from backing store into DataRAM
 *   0x13 READOOB    — load spare from backing store into SpareRAM
 *   0x23 UNLOCK     — mark blocks unlocked
 *   0x27 UNLOCK_ALL — unlock all
 *   0x90 READID     — populate BootRAM with device ID
 *   0xF0 RESET      — reinitialise state
 */
static void omap3_onenand_execute_cmd(uint16_t cmd)
{
    unsigned block, page, fpa, fsa;
    uint64_t flash_off;
    int buf_idx;

    /* Decode address registers */
    block = onenand_state.start_addr1 & 0x7FFF;  /* FBA field (bits 14:0) */
    fpa = (onenand_state.start_addr8 >> ONENAND_FPA_SHIFT) & ONENAND_FPA_MASK;
    fsa = onenand_state.start_addr8 & ONENAND_FSA_MASK;
    page = fpa;

    /* Which DataRAM buffer to use: BSA field bits [11:8] */
    buf_idx = ((onenand_state.start_buffer >> ONENAND_BSA_SHIFT) & 0x0F)
              >= ONENAND_BSA_DATARAM1 ? 1 : 0;

    switch (cmd) {
    case ONENAND_CMD_READ:
        flash_off = (uint64_t)block * ONENAND_BLOCK_SIZE
                  + (uint64_t)page * ONENAND_PAGE_SIZE;

        if (onenand_state.data && flash_off + ONENAND_PAGE_SIZE <= onenand_state.data_size) {
            memcpy(onenand_state.dataram[buf_idx],
                   onenand_state.data + flash_off,
                   ONENAND_PAGE_SIZE);
        } else {
            /* No backing store or out of range — erased flash */
            memset(onenand_state.dataram[buf_idx], 0xFF, ONENAND_PAGE_SIZE);
        }

        /* Also load spare data.  In our raw image, spare follows main if present.
         * For a main-only image, return erased spare. */
        memset(onenand_state.spareram[buf_idx], 0xFF, ONENAND_SPARE_SIZE);

        onenand_state.interrupt = ONENAND_INT_MASTER | ONENAND_INT_READ;
        onenand_state.ctrl_status = ONENAND_CTRL_RSTB;  /* ready */

        qemu_log_mask(LOG_UNIMP,
                      "onenand: READ block=%u page=%u (fpa=%u fsa=%u) buf%d "
                      "flash_off=0x%lx%s\n",
                      block, page, fpa, fsa, buf_idx,
                      (unsigned long)flash_off,
                      onenand_state.data ? "" : " [no backing]");
        break;

    case ONENAND_CMD_READOOB:
        /* Read spare/OOB only — we don't model real OOB, return erased */
        memset(onenand_state.spareram[buf_idx], 0xFF, ONENAND_SPARE_SIZE);
        onenand_state.interrupt = ONENAND_INT_MASTER | ONENAND_INT_READ;
        onenand_state.ctrl_status = ONENAND_CTRL_RSTB;
        break;

    case ONENAND_CMD_PROG:
        /* Write: accept silently (we don't write back to image) */
        onenand_state.interrupt = ONENAND_INT_MASTER | ONENAND_INT_WRITE;
        onenand_state.ctrl_status = ONENAND_CTRL_RSTB;
        break;

    case ONENAND_CMD_ERASE:
        /* Erase: accept silently */
        onenand_state.interrupt = ONENAND_INT_MASTER | ONENAND_INT_ERASE;
        onenand_state.ctrl_status = ONENAND_CTRL_RSTB;
        break;

    case ONENAND_CMD_UNLOCK:
    case ONENAND_CMD_UNLOCK_ALL:
    case ONENAND_CMD_LOCK:
        onenand_state.wp_status = (cmd == ONENAND_CMD_LOCK) ? 0x0000 : 0x0001;
        onenand_state.interrupt = ONENAND_INT_MASTER;
        onenand_state.ctrl_status = ONENAND_CTRL_RSTB;
        break;

    case ONENAND_CMD_READID:
        /* Place device ID into BootRAM (NLoader reads ID from there) */
        memset(onenand_state.bootram, 0x00, sizeof(onenand_state.bootram));
        onenand_state.bootram[0] = 0xEC; onenand_state.bootram[1] = 0x00; /* Samsung */
        onenand_state.bootram[2] = 0x68; onenand_state.bootram[3] = 0x00; /* 8Gbit */
        onenand_state.bootram[4] = 0x21; onenand_state.bootram[5] = 0x01; /* Version */
        onenand_state.interrupt = ONENAND_INT_MASTER | ONENAND_INT_READ;
        onenand_state.ctrl_status = ONENAND_CTRL_RSTB;
        break;

    case ONENAND_CMD_RESET:
        onenand_state.start_addr1 = 0;
        onenand_state.start_addr2 = 0;
        onenand_state.start_addr8 = 0;
        onenand_state.start_buffer = 0;
        onenand_state.command = 0;
        onenand_state.sys_cfg1 = 0x40E0;
        onenand_state.ctrl_status = ONENAND_CTRL_RSTB;
        onenand_state.interrupt = ONENAND_INT_MASTER | ONENAND_INT_RESET;
        onenand_state.wp_status = 0x0001;
        break;

    default:
        qemu_log_mask(LOG_UNIMP,
                      "onenand: unimplemented command 0x%04x\n", cmd);
        onenand_state.interrupt = ONENAND_INT_MASTER;
        onenand_state.ctrl_status = ONENAND_CTRL_RSTB;
        break;
    }
}

static uint64_t omap3_onenand_read(void *opaque, hwaddr offset, unsigned size)
{
    /*
     * DataRAM0: byte offsets 0x0400–0x13FF (2 pages × 2KB = 4KB)
     * DataRAM1: byte offsets 0x1400–0x23FF
     */
    if (offset >= ONENAND_OFF_DATARAM0 &&
        offset < ONENAND_OFF_DATARAM0 + sizeof(onenand_state.dataram[0])) {
        unsigned idx = offset - ONENAND_OFF_DATARAM0;
        if (size == 2) {
            return *(uint16_t *)&onenand_state.dataram[0][idx];
        }
        return *(uint32_t *)&onenand_state.dataram[0][idx];
    }
    if (offset >= ONENAND_OFF_DATARAM1 &&
        offset < ONENAND_OFF_DATARAM1 + sizeof(onenand_state.dataram[1])) {
        unsigned idx = offset - ONENAND_OFF_DATARAM1;
        if (size == 2) {
            return *(uint16_t *)&onenand_state.dataram[1][idx];
        }
        return *(uint32_t *)&onenand_state.dataram[1][idx];
    }

    /* SpareRAM0: byte offset 0x10020 */
    if (offset >= ONENAND_OFF_SPARERAM0 &&
        offset < ONENAND_OFF_SPARERAM0 + ONENAND_SPARE_SIZE) {
        unsigned idx = offset - ONENAND_OFF_SPARERAM0;
        if (size == 2) {
            return *(uint16_t *)&onenand_state.spareram[0][idx];
        }
        return *(uint32_t *)&onenand_state.spareram[0][idx];
    }
    if (offset >= ONENAND_OFF_SPARERAM1 &&
        offset < ONENAND_OFF_SPARERAM1 + ONENAND_SPARE_SIZE) {
        unsigned idx = offset - ONENAND_OFF_SPARERAM1;
        if (size == 2) {
            return *(uint16_t *)&onenand_state.spareram[1][idx];
        }
        return *(uint32_t *)&onenand_state.spareram[1][idx];
    }

    /* BootRAM: byte offsets 0x0000–0x03FF (below DataRAM0) */
    if (offset < ONENAND_OFF_DATARAM0 &&
        offset < sizeof(onenand_state.bootram)) {
        unsigned idx = offset;
        if (size == 2) {
            return *(uint16_t *)&onenand_state.bootram[idx];
        }
        return *(uint32_t *)&onenand_state.bootram[idx];
    }

    /* Register area (0x1E000+) — 16-bit registers */
    switch (offset) {
    /* ID registers (read-only) */
    case ONENAND_REG_MANUF_ID:      return 0x00EC;  /* Samsung */
    case ONENAND_REG_DEVICE_ID:     return 0x0068;  /* 8Gbit MLC */
    case ONENAND_REG_VERSION_ID:    return 0x0121;
    case ONENAND_REG_DATA_BUF_SIZE: return 0x0800;  /* 2048 bytes */
    case ONENAND_REG_BOOT_BUF_SIZE: return 0x0200;  /* 512 bytes */
    case ONENAND_REG_NUM_BUFFERS:   return 0x0101;  /* 1 data + 1 boot */
    case ONENAND_REG_TECHNOLOGY:    return 0x0001;  /* MLC */

    /* Address registers (R/W) */
    case ONENAND_REG_START_ADDR1:   return onenand_state.start_addr1;
    case ONENAND_REG_START_ADDR2:   return onenand_state.start_addr2;
    case ONENAND_REG_START_ADDR3:   return 0;
    case ONENAND_REG_START_ADDR4:   return 0;
    case ONENAND_REG_START_ADDR5:   return 0;
    case ONENAND_REG_START_ADDR6:   return 0;
    case ONENAND_REG_START_ADDR7:   return 0;
    case ONENAND_REG_START_ADDR8:   return onenand_state.start_addr8;

    /* Buffer/command registers */
    case ONENAND_REG_START_BUFFER:  return onenand_state.start_buffer;
    case ONENAND_REG_COMMAND:       return onenand_state.command;
    case ONENAND_REG_SYS_CFG1:     return onenand_state.sys_cfg1;
    case ONENAND_REG_SYS_CFG2:     return onenand_state.sys_cfg2;

    /* Status registers */
    case ONENAND_REG_CTRL_STATUS:   return onenand_state.ctrl_status;
    case ONENAND_REG_INTERRUPT:     return onenand_state.interrupt;
    case ONENAND_REG_START_BLOCK:   return onenand_state.start_block;
    case ONENAND_REG_END_BLOCK:     return onenand_state.end_block;
    case ONENAND_REG_WP_STATUS:     return onenand_state.wp_status;

    /* ECC — always report no errors */
    case ONENAND_REG_ECC_STATUS:    return 0x0000;

    default:
        break;
    }

    /* Anything else — erased flash pattern */
    return (size == 2) ? 0xFFFF : 0xFFFFFFFF;
}

static void omap3_onenand_write(void *opaque, hwaddr offset,
                                 uint64_t value, unsigned size)
{
    uint16_t val16 = (uint16_t)value;

    /*
     * Writes to DataRAM (for PROG commands — guest loads data before write).
     */
    if (offset >= ONENAND_OFF_DATARAM0 &&
        offset < ONENAND_OFF_DATARAM0 + sizeof(onenand_state.dataram[0])) {
        unsigned idx = offset - ONENAND_OFF_DATARAM0;
        if (size == 2) {
            *(uint16_t *)&onenand_state.dataram[0][idx] = val16;
        } else {
            *(uint32_t *)&onenand_state.dataram[0][idx] = (uint32_t)value;
        }
        return;
    }
    if (offset >= ONENAND_OFF_DATARAM1 &&
        offset < ONENAND_OFF_DATARAM1 + sizeof(onenand_state.dataram[1])) {
        unsigned idx = offset - ONENAND_OFF_DATARAM1;
        if (size == 2) {
            *(uint16_t *)&onenand_state.dataram[1][idx] = val16;
        } else {
            *(uint32_t *)&onenand_state.dataram[1][idx] = (uint32_t)value;
        }
        return;
    }

    /* SpareRAM writes */
    if (offset >= ONENAND_OFF_SPARERAM0 &&
        offset < ONENAND_OFF_SPARERAM0 + ONENAND_SPARE_SIZE) {
        unsigned idx = offset - ONENAND_OFF_SPARERAM0;
        if (size == 2) {
            *(uint16_t *)&onenand_state.spareram[0][idx] = val16;
        } else {
            *(uint32_t *)&onenand_state.spareram[0][idx] = (uint32_t)value;
        }
        return;
    }

    /* Register writes */
    switch (offset) {
    case ONENAND_REG_START_ADDR1:
        onenand_state.start_addr1 = val16;
        return;
    case ONENAND_REG_START_ADDR2:
        onenand_state.start_addr2 = val16;
        return;
    case ONENAND_REG_START_ADDR8:
        onenand_state.start_addr8 = val16;
        return;
    case ONENAND_REG_START_BUFFER:
        onenand_state.start_buffer = val16;
        return;
    case ONENAND_REG_START_BLOCK:
        onenand_state.start_block = val16;
        return;
    case ONENAND_REG_END_BLOCK:
        onenand_state.end_block = val16;
        return;
    case ONENAND_REG_SYS_CFG1:
        onenand_state.sys_cfg1 = val16;
        return;
    case ONENAND_REG_SYS_CFG2:
        onenand_state.sys_cfg2 = val16;
        return;
    case ONENAND_REG_INTERRUPT:
        /* Writing clears interrupt bits (W1C in some implementations) */
        onenand_state.interrupt &= ~val16;
        return;
    case ONENAND_REG_COMMAND:
        onenand_state.command = val16;
        omap3_onenand_execute_cmd(val16);
        return;
    default:
        break;
    }
}

static const MemoryRegionOps omap3_onenand_ops = {
    .read = omap3_onenand_read,
    .write = omap3_onenand_write,
    .endianness = DEVICE_LITTLE_ENDIAN,
    .valid.min_access_size = 2,
    .valid.max_access_size = 4,
};

/*
 * ==========================================================================
 * OMAP3 HSMMC Controller — eMMC on MMC2 (0x480B4000).
 *
 * Samsung 16GB eMMC for user data storage.
 * Implements register model + CMD/response state machine for basic
 * eMMC card detection and enumeration (CMD0/1/2/3/7/9/13).
 * No data transfer yet — stub for Symbian boot prerequisites.
 * ==========================================================================
 */

/* HSMMC STAT bits */
#define HSMMC_STAT_CC    (1 << 0)   /* Command complete */
#define HSMMC_STAT_TC    (1 << 1)   /* Transfer complete */
#define HSMMC_STAT_CTO   (1 << 16)  /* Command timeout */
#define HSMMC_STAT_ERRI  (1 << 15)  /* Error interrupt */

/* PSTATE bits */
#define HSMMC_PSTATE_CINS  (1 << 16)  /* Card inserted */
#define HSMMC_PSTATE_CSS   (1 << 17)  /* Card state stable */

/* SYSCTL bits */
#define HSMMC_SYSCTL_SRA   (1 << 24)  /* Software reset ALL */
#define HSMMC_SYSCTL_SRC   (1 << 25)  /* Software reset CMD */
#define HSMMC_SYSCTL_SRD   (1 << 26)  /* Software reset DAT */
#define HSMMC_SYSCTL_ICE   (1 << 0)   /* Internal clock enable */
#define HSMMC_SYSCTL_ICS   (1 << 1)   /* Internal clock stable */

/* HCTL bits */
#define HSMMC_HCTL_SDBP    (1 << 8)   /* SD bus power */

typedef struct {
    uint32_t sysconfig;
    uint32_t con;
    uint32_t blk;
    uint32_t arg;
    uint32_t cmd;
    uint32_t rsp[4];
    uint32_t pstate;
    uint32_t hctl;
    uint32_t sysctl;
    uint32_t stat;
    uint32_t ie;
    uint32_t ise;
    uint32_t capa;

    /* eMMC card state */
    uint16_t rca;
    uint8_t  card_state;  /* 0=idle, 1=ready, 2=ident, 3=stby, 4=tran */
    bool     card_present;
} OMAP3HSMMCState;

static OMAP3HSMMCState hsmmc2_state;

static void omap3_hsmmc2_init_state(void)
{
    memset(&hsmmc2_state, 0, sizeof(hsmmc2_state));
    hsmmc2_state.card_present = true;
    hsmmc2_state.pstate = HSMMC_PSTATE_CINS | HSMMC_PSTATE_CSS;
    /* Capabilities: 3.3V + 3.0V + 1.8V, high-speed, base clock 48MHz */
    hsmmc2_state.capa = (1 << 26) | (1 << 25) | (1 << 24) |
                        (1 << 21) | 0x30;
    hsmmc2_state.rca = 0x0001;
}

static void hsmmc2_process_cmd(OMAP3HSMMCState *s)
{
    uint32_t cmd_idx = (s->cmd >> 24) & 0x3F;

    memset(s->rsp, 0, sizeof(s->rsp));
    s->stat &= ~(HSMMC_STAT_CC | HSMMC_STAT_CTO | HSMMC_STAT_TC);

    if (!s->card_present) {
        s->stat |= HSMMC_STAT_CTO;
        return;
    }

    switch (cmd_idx) {
    case 0:  /* GO_IDLE_STATE */
        s->card_state = 0;
        break;

    case 1:  /* SEND_OP_COND (eMMC) */
        /* OCR: high capacity, 3.3V range, busy/ready bit set */
        s->rsp[0] = 0xC0FF8080;
        s->card_state = 1;
        break;

    case 2:  /* ALL_SEND_CID */
        /* Samsung 16GB eMMC CID (synthetic) */
        s->rsp[0] = 0x15010044;  /* MID=0x15(Samsung), OID */
        s->rsp[1] = 0x41473136;  /* PNM="AG16" */
        s->rsp[2] = 0x47420800;  /* PNM(cont), PRV */
        s->rsp[3] = 0x12345601;  /* PSN + MDT */
        s->card_state = 2;
        break;

    case 3:  /* SET_RELATIVE_ADDR (eMMC: host assigns RCA) */
        s->rca = (s->arg >> 16) & 0xFFFF;
        if (s->rca == 0) {
            s->rca = 0x0001;
        }
        s->rsp[0] = (s->rca << 16) | 0x0500;
        s->card_state = 3;
        break;

    case 7:  /* SELECT/DESELECT_CARD */
        s->rsp[0] = 0x00000700;  /* READY_FOR_DATA, tran state */
        s->card_state = 4;
        break;

    case 8:  /* SEND_IF_COND (SD) / SEND_EXT_CSD (eMMC tran) */
        s->rsp[0] = s->arg & 0xFFF;  /* Echo back */
        break;

    case 9:  /* SEND_CSD */
        s->rsp[0] = 0xD00F0032;  /* CSD v1.0 for 16GB eMMC */
        s->rsp[1] = 0x0F5903FF;
        s->rsp[2] = 0xFFFFC7FF;
        s->rsp[3] = 0x8A404000;
        break;

    case 12: /* STOP_TRANSMISSION */
        s->rsp[0] = 0x00000900;
        break;

    case 13: /* SEND_STATUS */
        s->rsp[0] = (s->card_state << 9) | 0x0100;  /* READY_FOR_DATA */
        break;

    case 16: /* SET_BLOCKLEN */
        s->rsp[0] = 0x00000900;
        break;

    case 23: /* SET_BLOCK_COUNT (eMMC) */
        s->rsp[0] = 0x00000900;
        break;

    case 55: /* APP_CMD — eMMC doesn't support this */
        s->stat |= HSMMC_STAT_CTO;
        return;  /* Don't set CC */

    default:
        break;
    }

    s->stat |= HSMMC_STAT_CC;
}

static uint64_t omap3_hsmmc2_read(void *opaque, hwaddr offset, unsigned size)
{
    OMAP3HSMMCState *s = (OMAP3HSMMCState *)opaque;

    switch (offset) {
    case 0x000: return 0x00000030;  /* HL_REV: revision 3.0 */
    case 0x010: return s->sysconfig;
    case 0x014: return 0x00000001;  /* SYSSTATUS: RESETDONE */
    case 0x02C: return s->con;
    case 0x104: return s->blk;
    case 0x108: return s->arg;
    case 0x10C: return s->cmd;
    case 0x110: return s->rsp[0];
    case 0x114: return s->rsp[1];
    case 0x118: return s->rsp[2];
    case 0x11C: return s->rsp[3];
    case 0x120: return 0;  /* DATA port: no data transfer yet */
    case 0x124: return s->pstate;
    case 0x128: return s->hctl;
    case 0x12C: return s->sysctl;
    case 0x130: return s->stat;
    case 0x134: return s->ie;
    case 0x138: return s->ise;
    case 0x140: return s->capa;
    default:    return 0;
    }
}

static void omap3_hsmmc2_write(void *opaque, hwaddr offset,
                                uint64_t value, unsigned size)
{
    OMAP3HSMMCState *s = (OMAP3HSMMCState *)opaque;

    switch (offset) {
    case 0x010:
        s->sysconfig = value;
        break;
    case 0x02C:
        s->con = value;
        break;
    case 0x104:
        s->blk = value;
        break;
    case 0x108:
        s->arg = value;
        break;
    case 0x10C:
        s->cmd = value;
        hsmmc2_process_cmd(s);
        break;
    case 0x128:
        s->hctl = value;
        if (value & HSMMC_HCTL_SDBP) {
            s->hctl |= HSMMC_HCTL_SDBP;  /* Power on succeeds */
        }
        break;
    case 0x12C:
        /* SYSCTL: handle resets and clock enable */
        if (value & HSMMC_SYSCTL_SRA) {
            /* Full software reset */
            uint32_t saved_pstate = s->pstate;
            uint32_t saved_capa = s->capa;
            bool saved_present = s->card_present;
            memset(s, 0, sizeof(*s));
            s->pstate = saved_pstate;
            s->capa = saved_capa;
            s->card_present = saved_present;
            s->rca = 0x0001;
            value &= ~HSMMC_SYSCTL_SRA;
        }
        /* CMD/DAT reset bits auto-clear */
        value &= ~(HSMMC_SYSCTL_SRC | HSMMC_SYSCTL_SRD);
        if (value & HSMMC_SYSCTL_ICE) {
            value |= HSMMC_SYSCTL_ICS;  /* Internal clock always stable */
        }
        s->sysctl = value;
        break;
    case 0x130:
        s->stat &= ~value;  /* W1C */
        break;
    case 0x134:
        s->ie = value;
        break;
    case 0x138:
        s->ise = value;
        break;
    default:
        break;
    }
}

static const MemoryRegionOps omap3_hsmmc2_ops = {
    .read = omap3_hsmmc2_read,
    .write = omap3_hsmmc2_write,
    .endianness = DEVICE_LITTLE_ENDIAN,
    .valid.min_access_size = 4,
    .valid.max_access_size = 4,
};

static void omap3630_init(Object *obj)
{
    OMAP3630State *s = OMAP3630(obj);

    object_initialize_child(obj, "cpu", &s->cpu,
                            ARM_CPU_TYPE_NAME("cortex-a8"));

    object_initialize_child(obj, "intc", &s->intc, TYPE_OMAP3_INTC);
    object_initialize_child(obj, "gpt1", &s->gpt1, TYPE_OMAP3_GPTIMER);
    object_initialize_child(obj, "gpt2", &s->gpt2, TYPE_OMAP3_GPTIMER);

    /*
     * SDRC defaults — DLL enabled + lock mode.
     * Bit 3 = ENADLL (DLL enabled), Bit 2 = LOCKDLL (lock mode).
     * This ensures the post-WFI SRAM code takes the fast exit path
     * instead of polling DLLA_STATUS in a tight loop.
     */
    s->sdrc_dlla_ctrl = 0x0000000C;  /* DLL enabled + lock mode */
    s->sdrc_power = 0x00000000;
}

static void omap3630_realize(DeviceState *dev, Error **errp)
{
    OMAP3630State *s = OMAP3630(dev);
    SysBusDevice *sbd;

    /* CPU — OMAP3630 has Cortex-A8 r3p2 (MIDR 0x413fc082) */
    s->cpu.midr = 0x413fc082;
    if (!qdev_realize(DEVICE(&s->cpu), NULL, errp)) {
        return;
    }

    /* INTC */
    if (!sysbus_realize(SYS_BUS_DEVICE(&s->intc), errp)) {
        return;
    }
    sbd = SYS_BUS_DEVICE(&s->intc);
    sysbus_mmio_map(sbd, 0, OMAP3630_INTC_BASE);
    sysbus_connect_irq(sbd, 0,
                       qdev_get_gpio_in(DEVICE(&s->cpu), ARM_CPU_IRQ));
    sysbus_connect_irq(sbd, 1,
                       qdev_get_gpio_in(DEVICE(&s->cpu), ARM_CPU_FIQ));
    qdev_pass_gpios(DEVICE(&s->intc), dev, NULL);

    /* GPTIMER1 (32.768 kHz, IRQ 37) */
    if (!sysbus_realize(SYS_BUS_DEVICE(&s->gpt1), errp)) {
        return;
    }
    sbd = SYS_BUS_DEVICE(&s->gpt1);
    sysbus_mmio_map(sbd, 0, OMAP3630_GPT1_BASE);
    sysbus_connect_irq(sbd, 0, qdev_get_gpio_in(dev, OMAP3630_IRQ_GPT1));

    /* GPTIMER2 (sys_clk, IRQ 38) */
    object_property_set_int(OBJECT(&s->gpt2), "input-freq",
                            38400000, &error_fatal);
    if (!sysbus_realize(SYS_BUS_DEVICE(&s->gpt2), errp)) {
        return;
    }
    sbd = SYS_BUS_DEVICE(&s->gpt2);
    sysbus_mmio_map(sbd, 0, OMAP3630_GPT2_BASE);
    sysbus_connect_irq(sbd, 0, qdev_get_gpio_in(dev, OMAP3630_IRQ_GPT2));

    /* 32K Sync Timer */
    memory_region_init_io(&s->sync32k_iomem, OBJECT(s),
                          &omap3630_sync32k_ops, s,
                          "omap3630-sync32k", OMAP3630_SYNC32K_SIZE);
    memory_region_add_subregion(get_system_memory(),
                                OMAP3630_SYNC32K_BASE, &s->sync32k_iomem);

    /* On-chip SRAM (64KB) */
    memory_region_init_ram(&s->sram, OBJECT(s), "omap3630.sram",
                           OMAP3630_SRAM_SIZE, &error_fatal);
    memory_region_add_subregion(get_system_memory(),
                                OMAP3630_SRAM_BASE, &s->sram);

    /*
     * NLoader RAM (2MB at 0x10000000).
     * NLoader uses 0x10000000 for data/BSS and 0x10020000 for code.
     * Build TOC writes to 0x101ff000 (shared memory / PDRAM area),
     * so we need at least 2MB.  On real hardware this is the RAPUYAMA
     * processor data RAM accessible to both ARM and modem.
     */
    memory_region_init_ram(&s->nloader_ram, OBJECT(s), "omap3630.nloader-ram",
                           2 * 1024 * 1024, &error_fatal);
    memory_region_add_subregion(get_system_memory(),
                                0x10000000, &s->nloader_ram);

    /* NLoader UART shim — captures NLoader serial output */
    memory_region_init_io(&s->nloader_uart_iomem, OBJECT(s),
                          &nloader_uart_ops, s, "nloader-uart",
                          NLOADER_UART_SIZE);
    memory_region_add_subregion(get_system_memory(),
                                NLOADER_UART_BASE, &s->nloader_uart_iomem);

    /*
     * NLoader printf ring buffer (0x40100000, 960KB).
     * NLoader's ARM printf at code offset 0x1e90 writes formatted output
     * to buffers at base + (bufID << 12).  Base is 0x40100000 (from
     * literal pool at code offset 0x2074).  Buffer ID 0xEF (used by
     * the printf wrapper at 0x6204) maps to 0x401EF000.
     * On real hardware this is in the OCM RAM / L4 peripheral space;
     * provide plain RAM so the formatted strings are captured.
     * Size limited to 960KB (0xF0000) to avoid overlapping the NLoader
     * UART shim at 0x401F0000.
     */
    {
        static MemoryRegion printf_buf;
        memory_region_init_ram(&printf_buf, OBJECT(s),
                               "nloader-printf-buf", 0xF0000,
                               &error_fatal);
        memory_region_add_subregion(get_system_memory(),
                                    0x40100000, &printf_buf);
    }

    /* UART3 (serial console, IRQ 74) */
    serial_mm_init(get_system_memory(), OMAP3630_UART3_BASE, 2,
                   qdev_get_gpio_in(dev, OMAP3630_IRQ_UART3),
                   115200, serial_hd(0), DEVICE_LITTLE_ENDIAN);

    /* CM (Clock Manager) — per-domain FCLKEN/ICLKEN/AUTOIDLE/CLKSTCTRL */
    omap3630_cm_init();
    memory_region_init_io(&s->cm_iomem, OBJECT(s), &omap3630_cm_ops, s,
                          "omap3630-cm", OMAP3630_CM_SIZE);
    memory_region_add_subregion(get_system_memory(),
                                OMAP3630_CM_BASE, &s->cm_iomem);

    /* PRM (Power & Reset Manager) — per-domain PWSTCTRL/PWSTST/RSTST */
    omap3630_prm_init();
    memory_region_init_io(&s->prm_iomem, OBJECT(s), &omap3630_prm_ops, s,
                          "omap3630-prm", OMAP3630_PRM_SIZE);
    memory_region_add_subregion(get_system_memory(),
                                OMAP3630_PRM_BASE, &s->prm_iomem);

    /* SCM stub (System Control Module) — returns 0 for safe defaults */
    memory_region_init_io(&s->scm_iomem, OBJECT(s), &omap3630_scm_ops, s,
                          "omap3630-scm", OMAP3630_SCM_SIZE);
    memory_region_add_subregion(get_system_memory(),
                                OMAP3630_SCM_BASE, &s->scm_iomem);

    /* TAP (Test Access Port — chip ID registers) */
    memory_region_init_io(&s->tap_iomem, OBJECT(s), &omap3630_tap_ops, s,
                          "omap3630-tap", OMAP3630_TAP_SIZE);
    memory_region_add_subregion(get_system_memory(),
                                OMAP3630_TAP_BASE, &s->tap_iomem);

    /* virtio-mmio transport for block storage (paravirtualised) */
    sysbus_create_simple("virtio-mmio", 0x4E000000,
                         qdev_get_gpio_in(dev, OMAP3630_IRQ_VIRTIO0));

    /*
     * Peripheral stubs — return SYSSTATUS.RDONE=1 at offset 0x014
     * so omap_hwmod softreset completes instantly instead of timing out.
     */
#define INIT_PERIPH_STUB(field, name, base, sz) do { \
    memory_region_init_io(&s->field, OBJECT(s), &omap3_periph_stub_ops, \
                          s, name, sz); \
    memory_region_add_subregion(get_system_memory(), base, &s->field); \
} while (0)

    /* GPIO banks 1-6: proper register model with W1C IRQSTATUS */
    {
        static const struct { uint32_t base; int irq; } gpio_info[OMAP3_GPIO_BANKS] = {
            { 0x48310000, OMAP3630_IRQ_GPIO1 },
            { 0x49050000, OMAP3630_IRQ_GPIO2 },
            { 0x49052000, OMAP3630_IRQ_GPIO3 },
            { 0x49054000, OMAP3630_IRQ_GPIO4 },
            { 0x49056000, OMAP3630_IRQ_GPIO5 },
            { 0x49058000, OMAP3630_IRQ_GPIO6 },
        };
        int i;
        for (i = 0; i < OMAP3_GPIO_BANKS; i++) {
            omap3_gpio_init(&s->gpio[i], OBJECT(s), i + 1,
                            qdev_get_gpio_in(dev, gpio_info[i].irq));
            memory_region_add_subregion(get_system_memory(),
                                        gpio_info[i].base,
                                        &s->gpio[i].iomem);
        }
    }
    /* I2C1/I2C2 stubs — no devices, return NACK for all transfers */
    /*
     * I2C1 — TWL5031 PMIC bus (IRQ 56).
     * N950/N9 DTS pattern: TWL5031 on OMAP I2C1 (0x48070000).
     * Nokia I2C_2 → OMAP I2C1. Full I2C controller with TWL5031 0x48-0x4B.
     */
    {
        static OMAP3I2CState i2c1_state;
        memset(&i2c1_state, 0, sizeof(i2c1_state));
        i2c1_state.bus_id = 1;
        i2c1_state.rev = 0x0040;
        i2c1_state.con = 0;  /* disabled at reset */
        i2c1_state.irq = qdev_get_gpio_in(dev, OMAP3630_IRQ_I2C1);

        /* Initialize TWL5031 PMIC */
        twl5031_init(&twl5031_state);
        i2c1_state.twl = &twl5031_state;

        /* Initialize I2C1 sensor stubs (charger) */
        init_i2c1_sensors();
        i2c1_state.sensors = i2c1_sensors;
        i2c1_state.n_sensors = ARRAY_SIZE(i2c1_sensors);

        memory_region_init_io(&s->i2c1_iomem, OBJECT(s),
                              &omap3630_i2c_ops, &i2c1_state,
                              "omap3-i2c1", 0x1000);
        memory_region_add_subregion(get_system_memory(), 0x48070000,
                                    &s->i2c1_iomem);
    }
    /* I2C2 — GPS module (no emulated devices yet) */
    memory_region_init_io(&s->i2c2_iomem, OBJECT(s),
                          &omap3630_i2c_stub_ops, s,
                          "omap3-i2c2", 0x1000);
    memory_region_add_subregion(get_system_memory(), 0x48072000,
                                &s->i2c2_iomem);

    /*
     * I2C3 — sensors + keypad bus (IRQ 61).
     * Nokia I2C_0 → OMAP I2C3 (0x48060000).
     * LM8323 keypad controller at address 0x34.
     * LM8323 INT line wired directly to INTC IRQ 30 (bypasses GPIO stubs).
     */
    {
        static OMAP3I2CState i2c3_state;
        memset(&i2c3_state, 0, sizeof(i2c3_state));
        i2c3_state.bus_id = 3;
        i2c3_state.rev = 0x0040;
        i2c3_state.con = 0;
        i2c3_state.irq = qdev_get_gpio_in(dev, OMAP3630_IRQ_I2C3);

        /* Initialize LM8323 keypad controller */
        lm8323_init(&lm8323_state);
        lm8323_state.irq = qdev_get_gpio_in(dev, OMAP3630_IRQ_LM8323);
        i2c3_state.lm8323 = &lm8323_state;
        i2c3_state.lm8323_write_pos = -1;

        /* Initialize I2C3 sensor stubs */
        init_i2c3_sensors();
        i2c3_state.sensors = i2c3_sensors;
        i2c3_state.n_sensors = ARRAY_SIZE(i2c3_sensors);
        i2c3_state.mxt = &mxt_stub;

        memory_region_init_io(&s->i2c3_iomem, OBJECT(s),
                              &omap3630_i2c_ops, &i2c3_state,
                              "omap3-i2c3", 0x1000);
        memory_region_add_subregion(get_system_memory(), 0x48060000,
                                    &s->i2c3_iomem);

        /* Register host keyboard → LM8323 FIFO input handler */
        qemu_input_handler_register(dev, &nokia_e7_kbd_handler);
    }

    create_unimplemented_device("omap3-sms", 0x6C000000, 0x10000);

    /* SDRC — proper model for PM idle DLL lock polling */
    memory_region_init_io(&s->sdrc_iomem, OBJECT(s), &omap3630_sdrc_ops, s,
                          "omap3-sdrc", 0x1000);
    memory_region_add_subregion(get_system_memory(), 0x6D000000,
                                &s->sdrc_iomem);

    /* GPMC — proper register model for external memory (OneNAND) */
    omap3_gpmc_init_state();
    memory_region_init_io(&s->gpmc_iomem, OBJECT(s), &omap3_gpmc_ops, s,
                          "omap3-gpmc", 0x1000);
    memory_region_add_subregion(get_system_memory(), 0x6E000000,
                                &s->gpmc_iomem);

    /* OneNAND flash at GPMC CS0 (0x08000000, 128KB window for registers) */
    omap3_onenand_init_state();
    memory_region_init_io(&s->onenand_iomem, OBJECT(s), &omap3_onenand_ops, s,
                          "omap3-onenand", 0x01000000);
    memory_region_add_subregion(get_system_memory(), 0x08000000,
                                &s->onenand_iomem);

    /* HSMMC1 stub — return SYSSTATUS.RESETDONE=1, SYSCTL.ICS=1 */
    memory_region_init_io(&s->hsmmc1_iomem, OBJECT(s), &omap3630_hsmmc_ops, s,
                          "omap3-hsmmc1", 0x1000);
    memory_region_add_subregion(get_system_memory(), 0x4809C000,
                                &s->hsmmc1_iomem);
    /* DSS/DISPC — real display controller model */
    sysbus_create_simple("omap3-dss", 0x48050000, NULL);
    INIT_PERIPH_STUB(musb_iomem, "omap3-musb", 0x480AB000, 0x1000);
    INIT_PERIPH_STUB(wdt2_iomem, "omap3-wdt2", 0x48314000, 0x1000);

    /* Crypto engines — ti-sysc reads SYSSTATUS at offset 0x4C from base */
    INIT_PERIPH_STUB(aes1_iomem, "omap3-aes1", 0x480A6000, 0x1000);
    INIT_PERIPH_STUB(aes2_iomem, "omap3-aes2", 0x480C5000, 0x1000);
    INIT_PERIPH_STUB(rng_iomem,  "omap3-rng",  0x480A0000, 0x1000);
    INIT_PERIPH_STUB(sham_iomem, "omap3-sham", 0x480C3000, 0x1000);

    /* 1-Wire, McSPI */
    INIT_PERIPH_STUB(hdq_iomem,    "omap3-hdq",    0x480B2000, 0x1000);
    INIT_PERIPH_STUB(mcspi1_iomem, "omap3-mcspi1", 0x48098000, 0x1000);
    INIT_PERIPH_STUB(mcspi2_iomem, "omap3-mcspi2", 0x4809A000, 0x1000);
    INIT_PERIPH_STUB(mcspi3_iomem, "omap3-mcspi3", 0x480B8000, 0x1000);
    INIT_PERIPH_STUB(mcspi4_iomem, "omap3-mcspi4", 0x480BA000, 0x1000);

    /* Timers 3-11 (GPT3-GPT11) */
    INIT_PERIPH_STUB(gpt3_iomem,  "omap3-gpt3",  0x49034000, 0x1000);
    INIT_PERIPH_STUB(gpt4_iomem,  "omap3-gpt4",  0x49036000, 0x1000);
    INIT_PERIPH_STUB(gpt5_iomem,  "omap3-gpt5",  0x49038000, 0x1000);
    INIT_PERIPH_STUB(gpt6_iomem,  "omap3-gpt6",  0x4903A000, 0x1000);
    INIT_PERIPH_STUB(gpt7_iomem,  "omap3-gpt7",  0x4903C000, 0x1000);
    INIT_PERIPH_STUB(gpt8_iomem,  "omap3-gpt8",  0x4903E000, 0x1000);
    INIT_PERIPH_STUB(gpt9_iomem,  "omap3-gpt9",  0x49040000, 0x1000);
    INIT_PERIPH_STUB(gpt10_iomem, "omap3-gpt10", 0x48086000, 0x1000);
    INIT_PERIPH_STUB(gpt11_iomem, "omap3-gpt11", 0x48088000, 0x1000);

    /* UARTs 1, 2, 4 (UART3 is real serial_mm) */
    INIT_PERIPH_STUB(uart1_iomem, "omap3-uart1", 0x4806A000, 0x1000);
    INIT_PERIPH_STUB(uart2_iomem, "omap3-uart2", 0x4806C000, 0x1000);
    INIT_PERIPH_STUB(uart4_iomem, "omap3-uart4", 0x49042000, 0x1000);

    /* HSMMC2/3, USB host, DSI1 */
    /* HSMMC2 — eMMC controller with card detection + CMD state machine */
    omap3_hsmmc2_init_state();
    memory_region_init_io(&s->hsmmc2_iomem, OBJECT(s), &omap3_hsmmc2_ops,
                          &hsmmc2_state, "omap3-hsmmc2", 0x1000);
    memory_region_add_subregion(get_system_memory(), 0x480B4000,
                                &s->hsmmc2_iomem);
    INIT_PERIPH_STUB(hsmmc3_iomem, "omap3-hsmmc3", 0x480AD000, 0x1000);
    INIT_PERIPH_STUB(usbhost_iomem, "omap3-usbhost", 0x48064000, 0x1000);
    INIT_PERIPH_STUB(dsi1_iomem,   "omap3-dsi1",  0x4804FC00, 0x400);
    /* sDMA — 32-channel instant-completion model (IRQ 13-15 for L1-L3) */
    memset(&sdma_state, 0, sizeof(sdma_state));
    sdma_state.irq[1] = qdev_get_gpio_in(dev, OMAP3630_IRQ_SDMA1);
    sdma_state.irq[2] = qdev_get_gpio_in(dev, OMAP3630_IRQ_SDMA2);
    sdma_state.irq[3] = qdev_get_gpio_in(dev, OMAP3630_IRQ_SDMA3);
    memory_region_init_io(&s->sdma_iomem, OBJECT(s), &omap3_sdma_ops,
                          s, "omap3-sdma", 0x1000);
    memory_region_add_subregion(get_system_memory(), 0x48056000,
                                &s->sdma_iomem);
    INIT_PERIPH_STUB(gpt12_iomem,  "omap3-gpt12", 0x48304000, 0x1000);

#undef INIT_PERIPH_STUB
}

static void omap3630_class_init(ObjectClass *oc, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(oc);

    dc->realize = omap3630_realize;
    dc->user_creatable = false;
}

static const TypeInfo omap3630_type_info = {
    .name = TYPE_OMAP3630,
    .parent = TYPE_DEVICE,
    .instance_size = sizeof(OMAP3630State),
    .instance_init = omap3630_init,
    .class_init = omap3630_class_init,
};

static void omap3630_register_types(void)
{
    type_register_static(&omap3630_type_info);
}

type_init(omap3630_register_types)
