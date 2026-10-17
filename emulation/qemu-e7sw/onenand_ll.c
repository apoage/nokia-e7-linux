/*
 * onenand_ll.c — OneNAND register-level access, standard Samsung
 * protocol (16-bit registers, command-triggered, interrupt-status
 * completion).  Compiled into the shim; keep it free of statics so it
 * needs no .bss.
 *
 * The interrupt status is polled but NEVER write-cleared: the status
 * keeps MASTER|<op> set after every command, which is what NLoader's
 * genuine LLD expects to find at entry (its op-state machine polls
 * MASTER before issuing the next command — a cleared status would make
 * it time out; task 118).
 */

#include "e7sw.h"

#define ONA_POLL_LIMIT 20000000u

static void onenand_cmd(uint16_t cmd)
{
    mmio_w16(ONA_REG_CMD, cmd);
}

int onenand_reset(void)
{
    uint32_t n;

    onenand_cmd(ONA_CMD_RESET);
    for (n = 0; n < ONA_POLL_LIMIT; n++) {
        uint16_t st = mmio_r16(ONA_REG_INT);
        if ((st & ONA_INT_MASTER) && (st & ONA_INT_RESET)) {
            return 0;
        }
    }
    return -1;
}

int onenand_read_id(uint16_t ids[3])
{
    uint32_t n;

    onenand_cmd(ONA_CMD_READID);
    for (n = 0; n < ONA_POLL_LIMIT; n++) {
        uint16_t st = mmio_r16(ONA_REG_INT);
        if ((st & ONA_INT_MASTER) && (st & ONA_INT_READ)) {
            ids[0] = mmio_r16(ONA_BASE + 0x1E000);
            ids[1] = mmio_r16(ONA_BASE + 0x1E002);
            ids[2] = mmio_r16(ONA_BASE + 0x1E004);
            return 0;
        }
    }
    return -1;
}

int onenand_read_page(uint32_t page, void *dst_2k)
{
    uint32_t block = page / ONA_PAGES_PER_BLK;
    uint32_t fpa = page % ONA_PAGES_PER_BLK;
    volatile uint16_t *src = (volatile uint16_t *)ONA_DATARAM0;
    uint16_t *dst = dst_2k;
    uint32_t n, i;

    if (shim_debug()) {
        sw_log("ona: page ");
        sw_log_hex(page);
        sw_log(" dst ");
        sw_log_hex((uint32_t)dst_2k);
        sw_log_ch('\n');
    }

    mmio_w16(ONA_REG_SA1, (uint16_t)block);
    mmio_w16(ONA_REG_SA8, (uint16_t)(fpa << 2));
    mmio_w16(ONA_REG_SBUF, 0u << 8);        /* BSA: DataRAM0 */
    onenand_cmd(ONA_CMD_READ);

    for (n = 0; n < ONA_POLL_LIMIT; n++) {
        uint16_t st = mmio_r16(ONA_REG_INT);
        if ((st & ONA_INT_MASTER) && (st & ONA_INT_READ)) {
            for (i = 0; i < ONA_PAGE_SIZE / 2; i++) {
                dst[i] = src[i];
            }
            return 0;
        }
    }
    return -1;
}
