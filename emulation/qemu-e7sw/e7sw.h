/*
 * e7sw.h — shared definitions for the SWBL-role guest blobs
 * (emulation/qemu-e7sw).
 *
 * On hardware SWBL (the "RAMSET SW" second-stage bootloader, present in
 * the OneNAND image at device offset 0x1200) loads NLoader into PDRAM
 * (base 0x10010000, task 115) and hands over a staged flash TOC before
 * NLoader's Build-TOC post-processing runs.  NLoader's flash driver is
 * its own FSR-style LLD riding the image (file 0xB000-0x10000 → VA
 * 0x1001B000-0x10020000) — the board maps that range directly (task
 * 118), so only the shim blob remains here:
 *
 *   shim blob → runs before NLoader main: reads the real flash TOC
 *               through the real OneNAND model, builds the SDRAM TOC,
 *               publishes the pointer
 *
 * Everything below is decoded from nloader.bin unless marked contract.
 */

#ifndef E7SW_H
#define E7SW_H

#include <stdint.h>

/* ---- OneNAND controller (model contract, task 104) ------------------- */
#define ONA_BASE          0x20010000u
#define ONA_BOOTRAM       (ONA_BASE + 0x00000)
#define ONA_DATARAM0      (ONA_BASE + 0x00400)
#define ONA_DATARAM1      (ONA_BASE + 0x01400)
#define ONA_REG_ID        (ONA_BASE + 0x1E000)  /* 6 ID halfwords        */
#define ONA_REG_SA1       (ONA_BASE + 0x1E200)  /* FBA (block)           */
#define ONA_REG_SA2       (ONA_BASE + 0x1E202)
#define ONA_REG_SA8       (ONA_BASE + 0x1E20E)  /* FPA<<2 | FSA          */
#define ONA_REG_SBUF      (ONA_BASE + 0x1E400)  /* BSA<<8                */
#define ONA_REG_CMD       (ONA_BASE + 0x1E440)
#define ONA_REG_INT       (ONA_BASE + 0x1E482)  /* W1C                   */

#define ONA_CMD_READ      0x0000u
#define ONA_CMD_READID    0x0090u
#define ONA_CMD_RESET     0x00F0u

#define ONA_INT_MASTER    (1u << 15)
#define ONA_INT_READ      (1u << 7)
#define ONA_INT_RESET     (1u << 4)

/* Geometry the LLD presents (matches the model: 1 GiB, 2 KiB pages) */
#define ONA_PAGE_SIZE     2048u
#define ONA_PAGES_PER_BLK 64u
#define ONA_BLK_SIZE      (ONA_PAGE_SIZE * ONA_PAGES_PER_BLK)   /* 128 KiB */
#define ONA_DEV_SECTORS   (512u * 1024 * 1024)  /* 1 GiB / 512 */

/* ---- NLoader RAM layout (decoded) ------------------------------------ */
#define NL_DEVINFO0       0x10007c30u   /* unit-0 device info, 0xC4 bytes      */

#define NL_SDRAM_TOC      0x101ff000u   /* 64 x 32-byte entries */
#define NL_CFG_TOC_PTR    0x30003e78u   /* config block TOC pointer */

/* ---- Shim plumbing (QEMU board contract) ------------------------------
 * The board drops the shim at 0x40200000 (bottom of the 64 KiB SRAM)
 * and leaves its parameters at 0x40200C00. */
#define SHIM_BASE         0x40200000u
#define SHIM_CONFIG       0x40200C00u
#define SHIM_PAGEBUF      0x40201000u   /* 4 KiB scratch */

#define SHIM_MAGIC        0x53573745u   /* 'E7WS' */

struct shim_cfg {
    uint32_t magic;
    uint32_t debug;      /* nonzero: XTI-log the shim + LLD             */
    uint32_t chain;      /* nonzero: add the synthetic MCUSW slot 0     */
    uint32_t mcusw_load; /* trampoline load address (chain mode)        */
    uint32_t mcusw_size; /* trampoline + zImage size (chain mode)       */
    uint32_t toc_base;   /* SDRAM TOC base (chain mode moves it past the
                          * read-scratch literal, see tasks/104)       */
};

/*
 * Shim logging: the shim runs pre-main and must NOT call NLoader's
 * XTI printf (its formatter globals are uninitialized code bytes then).
 * It writes bytes to the QEMU shim-log port instead; newline flushes.
 */
#define SHIMLOG_PORT     0x40208000u

static inline void sw_log_ch(char c)
{
    *(volatile uint8_t *)SHIMLOG_PORT = (uint8_t)c;
}

static inline void sw_log(const char *s)
{
    while (*s) {
        sw_log_ch(*s++);
    }
    sw_log_ch('\n');
}

static inline void sw_log_hex(uint32_t v)
{
    static const char d[] = "0123456789abcdef";
    int i;
    sw_log_ch('0'); sw_log_ch('x');
    for (i = 28; i >= 0; i -= 4) {
        sw_log_ch(d[(v >> i) & 0xF]);
    }
}

/* onenand_ll.c (compiled into both blobs) */
int  onenand_reset(void);
int  onenand_read_id(uint16_t ids[3]);
int  onenand_read_page(uint32_t page, void *dst_2k);   /* 0 = ok */

static inline void mmio_w16(uint32_t addr, uint16_t v)
{
    *(volatile uint16_t *)addr = v;
}

static inline uint16_t mmio_r16(uint32_t addr)
{
    return *(volatile uint16_t *)addr;
}

static inline uint32_t shim_debug(void)
{
    return *(volatile uint32_t *)SHIM_CONFIG;  /* cfg->debug */
}

#endif /* E7SW_H */
