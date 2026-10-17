/*
 * shim.c — the SWBL-role boot shim (task 104).
 *
 * On hardware SWBL reads the flash TOC ("Reading TOC") and populates the
 * SDRAM TOC at 0x101ff000 before NLoader's Build-TOC post-processing
 * runs (NLoader's own flash-TOC read step at 0x10031834 is an empty
 * stub in this firmware).  This shim plays that role through the real
 * OneNAND model: it runs before NLoader main (board sets PC here, LR to
 * 0x1003259b), reads the TOC pages at flash device offset 0 through the
 * register-level protocol, and writes the converted table.
 *
 * Also SWBL-shaped housekeeping:
 *   - publish the TOC pointer at 0x30003e78 for NLoader's TOC search
 *
 * The BML geometry pre-fill is RETIRED (task 118): the genuine LLD's
 * GetDevInfo owns those cells now (the board stages its device-ID table
 * and command words instead), and BML_Init's memset had made the old
 * pre-fill dead weight anyway.
 *
 * Chain mode (QEMU custom-image path, docs/nloader-boot-mechanism.md
 * §6): slot 0 is the synthetic MCUSW entry pointing at the trampoline;
 * its flash offset is out of device range ON PURPOSE — the LLD's
 * parameter check makes the copy fail, which is non-fatal, and the
 * mode-2 search still resolves the trampoline load address.
 */

#include "e7sw.h"

void *memcpy(void *d, const void *s, unsigned n);

void *memcpy(void *d, const void *s, unsigned n)
{
    uint8_t *dd = d;
    const uint8_t *ss = s;
    while (n--) {
        *dd++ = *ss++;
    }
    return d;
}

#define FLASH_TOC_HDR  20u
#define FLASH_TOC_MAX  32u
/* include only the boot-area images (device offsets < 1 MiB); the
 * encrypted kernel variants (SOS+IVE3*) stay out — their auth path is
 * walled separately (see tasks/104) */
#define TOC_OFF_LIMIT  0x100000u

void shim_entry(void)
{
    volatile struct shim_cfg *cfg = (volatile struct shim_cfg *)SHIM_CONFIG;
    uint8_t *pagebuf = (uint8_t *)SHIM_PAGEBUF;
    uint8_t *toc = (uint8_t *)NL_SDRAM_TOC;
    if (cfg->magic == SHIM_MAGIC && cfg->toc_base) {
        toc = (uint8_t *)cfg->toc_base;
    }
    uint16_t ids[3];
    uint32_t dbg = 0, chain = 0;
    int n_entries = 0;
    int i;

    if (cfg->magic == SHIM_MAGIC) {
        dbg = cfg->debug;
        chain = cfg->chain;
    }

    if (onenand_reset() != 0 || onenand_read_id(ids) != 0) {
        /* no usable flash model: leave the TOC empty (single end marker
         * is what NLoader sees on a blank device) */
        *(volatile uint32_t *)(NL_CFG_TOC_PTR) = (uint32_t)toc;
        if (dbg) {
            sw_log("swbl: NO ONENAND");
        }
        return;
    }
    if (dbg) {
        sw_log("swbl: onenand ids ");
        sw_log_hex(ids[0]); sw_log_ch(':');
        sw_log_hex(ids[1]); sw_log_ch(':');
        sw_log_hex(ids[2]);
        sw_log(", reading TOC");
    }

    /* flash TOC: 20-byte header + 32-byte entries at device offset 0 */
    if (onenand_read_page(0, pagebuf) != 0 ||
        onenand_read_page(1, pagebuf + ONA_PAGE_SIZE) != 0) {
        if (dbg) {
            sw_log("swbl: TOC read FAILED");
        }
        return;
    }

    if (chain) {
        /* synthetic MCUSW slot 0 — the §6 custom-image primitive */
        uint32_t *e = (uint32_t *)(toc + 0);
        for (i = 0; i < 8; i++) {
            e[i] = 0;
        }
        /*
         * +0x00 flash offset: deliberately OUT OF DEVICE RANGE.  Copy
         * images reads the image from here (LSN = off>>9) into the load
         * address — with a valid offset that copy would stream flash
         * data over the preloaded trampoline.  Out of range the LLD's
         * parameter check fails the read and the copy is non-fatal
         * (task-093 semantics).  The slot is still launchable: Copy
         * images rewrites [+0] = load + 0xefe01000 and the mode-2
         * search resolves the load back as [+0] + 0x101ff000.
         */
        e[0] = 0xFFFF0000u;                   /* +0x00 flash offset     */
        e[2] = cfg->mcusw_size;               /* +0x08 size             */
        e[4] = cfg->mcusw_load;               /* +0x10 load address     */
        memcpy(toc + 0x14, "MCUSW\0\0\0\0\0\0\0", 12);
        n_entries = 1;
    }

    for (i = 0; i < FLASH_TOC_MAX; i++) {
        const uint8_t *fe = pagebuf + FLASH_TOC_HDR + i * 32;
        uint32_t off, size;
        uint8_t *ne;
        int k;

        if (fe[0] == 0x00 || fe[0] == 0xFF) {
            break;
        }
        off = *(const uint32_t *)(fe + 12);
        size = *(const uint32_t *)(fe + 16);
        if (off >= TOC_OFF_LIMIT) {
            continue;
        }
        /* encrypted kernel variants: excluded (auth walled) */
        if (fe[0] == 'S' && fe[1] == 'O' && fe[2] == 'S' && fe[3] == '+' &&
            fe[4] == 'I') {
            continue;
        }
        /*
         * compressed-container family (name[3] '*' or '/' — the exact
         * marker NLoader's Copy images switches on at 0x1003206c):
         * these go through the Decompress path (BB5 container magics
         * 0x809795a3/0x31deeae8 header checks, then the real
         * decompressor + crypto), which is secure-side-walled in
         * emulation and derails the copy walk.  On hardware SWBL
         * stages them for that path; we curate them out instead.
         */
        if (fe[3] == '*' || fe[3] == '/') {
            continue;
        }
        if (n_entries >= 60) {
            break;
        }

        ne = toc + n_entries * 32;
        for (k = 0; k < 8; k++) {
            *(uint32_t *)(ne + k * 4) = 0;
        }
        *(uint32_t *)(ne + 0x00) = off;
        *(uint32_t *)(ne + 0x04) = *(const uint32_t *)(fe + 20); /* flags */
        *(uint32_t *)(ne + 0x08) = size;
        memcpy(ne + 0x14, fe, 12);
        n_entries++;

        if (dbg) {
            sw_log("swbl: toc '");
            sw_log((const char *)fe);
            sw_log("' off ");
            sw_log_hex(off);
            sw_log(" sz ");
            sw_log_hex(size);
            sw_log_ch('\n');
        }
    }

    /* end marker: name[0] = 0xFF */
    {
        uint8_t *ne = toc + n_entries * 32;
        for (i = 0; i < 32; i++) {
            ne[i] = 0xFF;
        }
    }

    /*
     * Geometry pre-fill: RETIRED (task 118).  BML_Init memsets the
     * device-info area before the LLD fills it, so the old pre-fill was
     * dead weight — and now the genuine LLD's GetDevInfo (slot +0x10)
     * is the sole geometry source; a pre-fill would only mask its
     * failure.  The board stages the LLD's device-ID table + command
     * words (nokia_e7.c) instead.
     */

    *(volatile uint32_t *)(NL_CFG_TOC_PTR) = (uint32_t)toc;

    if (dbg) {
        sw_log("swbl: SDRAM TOC ");
        sw_log_hex((uint32_t)n_entries);
        sw_log(" entries at ");
        sw_log_hex(NL_SDRAM_TOC);
        sw_log_ch('\n');
    }
}
