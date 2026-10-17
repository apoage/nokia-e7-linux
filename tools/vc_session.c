/*
 * vc_session.c — task 127 guest client: display arc M3, the VC-boot +
 * dispman session that completes the display software stack in
 * emulation.  Two layers in one binary, speaking ONLY through
 * /dev/bcm2727-vchi (the task-126 transport driver — no /dev/mem
 * anywhere; the driver owns bring-up, the CONTRACT-v1 ring, doorbells
 * and event servicing).
 *
 *   §9 boot layer    stage exchanges on ch0 (the VC boot ROM speaks
 *                    first: a bare receive; the 0x01000001 sentinel
 *                    reply carries the image count, §9.2), then the
 *                    cmd-0x42 image uploads on ch1 — image bytes as
 *                    bulk-tag chunks, each image closed by the 0x20-B
 *                    magic-framed header {0xAFE45251,
 *                    (len&0x00FFFFFF)|0x02000000, sig 20 B, 0xF001BC4E}
 *                    — ending in the final count==3/answer==1.
 *
 *   §10 service layer    the 0x14-B connection control echo, DISP msg
 *                    0x10 update start (the handle captured), 0x17
 *                    element op (change-mask + dest/src rects), 0x11
 *                    submit {handle, flag}, the UPDH completion drain —
 *                    plus a mid-session bad-CRC probe (§8.4 rejection)
 *                    that the session must survive.
 *
 * Every exchange logs its wire frame (bytes) and its interpretation;
 * any reply that mismatches the §9/§10 grammar aborts with SESSION
 * FAIL.  On a reply timeout the master-ack register is reported (bit 25
 * = HAT — the §9.3 "HAT=%d" timeout diagnostic, §9.6).
 *
 * Image-file convention (documented as-built in §13):
 *   vc_session [-2] <loader-image> <firmware-image>
 *
 *   -2    loader variant B (§9.3 imgSel != 0 selects img2; in emulation
 *         the same file rides — the A/B pair is a ROM-build distinction).
 *
 *   Each file is the image payload.  If it ends with the §7.2 container
 *   tail — "sign" magic + 20 signature bytes, the geometry of the ROM's
 *   signed_2727b1_v2.dll set — the client splits it and rides the REAL
 *   signature.  Otherwise a documented synthetic signature {0x5A^i}
 *   rides: the responder checks lengths, not signatures (§11.4), and
 *   the signature algorithm itself is OPEN (§7.2) — synthetic in
 *   emulation is a documented convention, not a faked real one.  On
 *   hardware the real set comes from the ROM copy per §7.3 (img1
 *   0x22E588/0x2050 and img3 0x232668/0xF6A2B, each followed by its
 *   tail) — carve and pass those files.
 *
 * Chunk granularity: 0xFE0 payload per frame here (the driver's single
 * TX page caps one wire frame at 0x1000, §12.2) versus the §9.1 decode
 * default 0x2000 (8 KB) chunks of the Symbian scatter-gather path; the
 * responder accumulates bytes and checks only the header's length word
 * (§11.4), so the session is equivalent — the mapping is §13's first
 * as-built note.  Timeouts follow §9.7: 500 ms reply waits, 5 s bulk.
 *
 * Build (task-103 cross; the quoted include pulls the committed uapi
 * header — its <linux/types.h> resolves against the musl sysroot):
 *   toolchain/musl.cc/armv6-linux-musleabi-cross/bin/armv6-linux-musleabi-gcc \
 *     -static -Os -march=armv6 -mtune=arm1176jzf-s \
 *     -o vc_session tools/vc_session.c
 */
#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include "../kernel/linux/include/uapi/linux/bcm2727_vchi.h"

/* ---- §8.4 frame ----------------------------------------------------------- */
#define FRAME_HDR       0x1Cu
#define BOOT_MAGIC_A    0xAFE45251u  /* §9.2 */
#define BOOT_MAGIC_B    0xF001BC4Eu
#define TYPE_UPLOAD     0x42u
#define TYPE_BOOTREPLY  0x2u
#define TYPE_CONTROL    0x00100004u
#define FOURCC_DISP     0x50534944u  /* "DISP" */
#define FOURCC_UPDH     0x48445055u  /* "UPDH" */

/* descriptor tags (§11.2 tag nibble) */
#define TAG_CTRL        0
#define TAG_DATA        1
#define TAG_MSG         2

/* emulation chunk = the driver's 0x1000 frame cap minus the §8.4 header
 * (see the file header: the §9.1 hardware default is 0x2000) */
#define CHUNK           0xFE0u

/* §9.7 decode budgets: reply waits 5 ms x 100 = 500 ms, image-upload
 * waits 5 ms x 1000 = 5 s */
#define TMO_REPLY_MS    500u
#define TMO_BULK_MS     5000u

/* the §10.3 connect message's runtime connection id — a host-side value
 * (decode pins only that it is echoed back) */
#define CONN_ID         0x2727u

static int dfd;

#define CHECK(cond, ...)                                                  \
    do {                                                                  \
        if (!(cond)) {                                                    \
            printf("SESSION FAIL: ");                                     \
            printf(__VA_ARGS__);                                          \
            printf("  (line %d)\n", __LINE__);                            \
            exit(1);                                                      \
        }                                                                 \
    } while (0)

/* ---- CRC-32C: the exact ROM algorithm shape (§8.4) — model, smoketest
 * and this client share one code shape ------------------------------------- */
static uint32_t crctab[256];

static void crc_init(void)
{
    for (uint32_t i = 0; i < 256; i++) {
        uint32_t c = i;
        for (int j = 0; j < 8; j++) {
            c = (c >> 1) ^ ((c & 1) ? 0x82F63B78u : 0);
        }
        crctab[i] = c;
    }
}

static uint32_t crc32c(const void *buf, uint32_t len)
{
    const uint8_t *p = buf;
    uint32_t crc = 0xFFFFFFFFu;

    for (uint32_t i = 0; i < len; i++) {
        crc = crctab[(crc ^ p[i]) & 0xFF] ^ (crc >> 8);
    }
    return crc;
}

static uint32_t ldl(const uint8_t *p)
{
    return (uint32_t)p[0] | (uint32_t)p[1] << 8 |
           (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
}

static uint32_t ldu16(const uint8_t *p)
{
    return (uint32_t)p[0] | (uint32_t)p[1] << 8;
}

static void stl(uint8_t *p, uint32_t v)
{
    p[0] = v; p[1] = v >> 8; p[2] = v >> 16; p[3] = v >> 24;
}

/* ---- verbose frame logging ------------------------------------------------ */

static uint32_t n_ex;                 /* exchange counter for the transcript */

static void hex_dump(const uint8_t *b, uint32_t n)
{
    for (uint32_t i = 0; i < n; i += 16) {
        printf("     %04x:", i);
        for (uint32_t j = 0; j < 16 && i + j < n; j++) {
            printf("%s%02x", (j % 4) == 0 ? " " : "", b[i + j]);
        }
        printf("\n");
    }
}

/* full dump for frames up to the largest service exchange (0x6C B);
 * head 0x24 + tail 8 for bulk chunks */
static void hex_dump_cap(const uint8_t *b, uint32_t n)
{
    if (n <= 0x70) {
        hex_dump(b, n);
        return;
    }
    hex_dump(b, 0x24);
    printf("     ...\n");
    hex_dump(b + n - 8, 8);
}

static void log_tx(int ch, int tag, const uint8_t *f, uint32_t flen,
                   const char *note)
{
    printf(">> #%u ch%d tag%d type 0x%08x plen %u crc 0x%08x | %s\n",
           ++n_ex, ch, tag, ldl(f), ldl(f + 4), ldl(f + 0x0C), note);
    hex_dump_cap(f, flen);
}

static void log_rx(const uint8_t *f, uint32_t flen, const char *note)
{
    printf("<< #%u ch0 type 0x%08x plen %u crc 0x%08x | %s\n",
           ++n_ex, ldl(f), ldl(f + 4), ldl(f + 0x0C), note);
    hex_dump_cap(f, flen);
}

/* ---- the driver ABI: one wire frame per call ------------------------------ */

/* the last frame received (single-threaded session) */
struct rxframe {
    uint32_t type, plen, crc, flen;
    uint8_t payload[0x200];
};
static struct rxframe fr;

static void log_driver_mode(void)
{
    char buf[8] = { 0 };
    int fd = open("/sys/module/vchi_bcm2727/parameters/poll", O_RDONLY);

    if (fd >= 0) {
        if (read(fd, buf, sizeof(buf) - 1) < 0) {
            buf[0] = 0;
        }
        close(fd);
    }
    printf("driver mode: %s\n",
           (buf[0] == 'Y' || buf[0] == '1')
               ? "poll (vchi-bcm2727.poll=1 — the pair enables stay clear)"
               : "IRQ-driven (ICU 201 claimed, pair enables on)");
}

/* the §9.3 timeout diagnostic: master-ack snapshot, bit 25 = HAT (§9.6) */
static uint32_t master_ack_snapshot(void)
{
    struct bcm2727_vchi_wait w;

    memset(&w, 0, sizeof(w));
    if (ioctl(dfd, BCM2727_VCHI_IOC_WAIT, &w) != 0) {
        return 0;
    }
    return w.master_ack;
}

/* build + send one §8.4 frame; returns 0 ok / -1 with errno set */
static int tx_frame(int ch, int tag, uint32_t type, const void *payload,
                    uint32_t plen, uint32_t timeout_ms, int corrupt_crc,
                    const char *note)
{
    static uint8_t f[0x1000];
    struct bcm2727_vchi_send s;
    uint32_t crc;
    long rv;

    CHECK(plen + FRAME_HDR <= sizeof(f), "frame too big for the TX buffer\n");
    memset(f, 0, FRAME_HDR);
    stl(f + 0x00, type);
    stl(f + 0x04, plen);
    crc = crc32c(payload, plen);
    if (corrupt_crc) {
        crc ^= 1;                       /* the deliberate §8.4 violation */
    }
    stl(f + 0x0C, crc);
    memcpy(f + FRAME_HDR, payload, plen);
    log_tx(ch, tag, f, FRAME_HDR + plen, note);

    memset(&s, 0, sizeof(s));
    s.buf_ptr = (uintptr_t)f;
    s.len = FRAME_HDR + plen;
    s.channel = ch;
    s.tag = tag;
    s.timeout_ms = timeout_ms;
    rv = ioctl(dfd, BCM2727_VCHI_IOC_SEND, &s);
    return rv == 0 ? 0 : -1;
}

static void tx_ok(int ch, int tag, uint32_t type, const void *payload,
                  uint32_t plen, uint32_t timeout_ms, const char *note)
{
    CHECK(tx_frame(ch, tag, type, payload, plen, timeout_ms, 0, note) == 0,
          "ch%d SEND failed (%s)\n", ch, strerror(errno));
}

/* receive one frame from ch0 (CONTRACT v1 delivers VC->AP there); the
 * RECV doorbell also delivers frames the peer queued while no RX
 * descriptor was armed — the HAT-hold path (§12.4 item 5), which is how
 * every post-boot reply arrives in this synchronous-ABI session */
static int rx_frame(const char *note, uint32_t timeout_ms)
{
    static uint8_t buf[0x200];
    struct bcm2727_vchi_recv r;
    long rv;

    memset(&r, 0, sizeof(r));
    r.buf_ptr = (uintptr_t)buf;
    r.maxlen = sizeof(buf);
    r.channel = 0;
    r.timeout_ms = timeout_ms;
    rv = ioctl(dfd, BCM2727_VCHI_IOC_RECV, &r);
    if (rv != 0) {
        return -1;
    }
    CHECK(r.len >= FRAME_HDR && r.len <= sizeof(buf),
          "RECV %s: frame length %u insane\n", note, r.len);
    fr.flen = r.len;
    fr.type = ldl(buf + 0x00);
    fr.plen = ldl(buf + 0x04);
    fr.crc = ldl(buf + 0x0C);
    CHECK(fr.plen + FRAME_HDR == r.len,
          "RECV %s: payload len %u vs frame %u (mismatch)\n",
          note, fr.plen, r.len);
    CHECK(fr.plen <= sizeof(fr.payload), "RECV %s: payload %u too big\n",
          note, fr.plen);
    CHECK(fr.crc == crc32c(buf + FRAME_HDR, fr.plen),
          "RECV %s: CRC mismatch — the peer sent a corrupt frame\n", note);
    memcpy(fr.payload, buf + FRAME_HDR, fr.plen);
    log_rx(buf, r.len, note);
    return 0;
}

/* receive with the §9.3 timeout diagnostics folded into the failure */
static void rx_must(const char *note)
{
    CHECK(rx_frame(note, TMO_REPLY_MS) == 0,
          "%s: no reply within %u ms — \"boot ROM not responding\" shape "
          "(§9.3); master-ack 0x%08x (bit 25 = HAT, §9.6)\n",
          note, TMO_REPLY_MS, master_ack_snapshot());
}

/* ---- the image-file convention (§7.2 geometry, §13 as-built) -------------- */

struct vcimg {
    const char *name;
    uint8_t *data;
    uint32_t len;                       /* payload length after the split */
    uint8_t sig[20];
    int sig_real;
};

static void load_image(struct vcimg *im, const char *path)
{
    FILE *fp = fopen(path, "rb");
    long sz;

    CHECK(fp != NULL, "open %s (%s)\n", path, strerror(errno));
    CHECK(fseek(fp, 0, SEEK_END) == 0 && (sz = ftell(fp)) >= 0x20 &&
          fseek(fp, 0, SEEK_SET) == 0,
          "size %s unreadable or too small\n", path);
    im->data = malloc(sz);
    CHECK(im->data != NULL, "out of memory for %s\n", path);
    CHECK(fread(im->data, 1, sz, fp) == (uint32_t)sz,
          "short read on %s\n", path);
    fclose(fp);
    im->name = path;

    if (sz > 0x18 && memcmp(im->data + sz - 0x18, "sign", 4) == 0) {
        /* the §7.2 container tail: [payload]["sign"][20-B signature] —
         * ride the REAL signature (the hardware path: carve the set
         * from the ROM copy per §7.3) */
        im->len = sz - 0x18;
        memcpy(im->sig, im->data + im->len + 4, 20);
        im->sig_real = 1;
    } else {
        im->len = sz;
        for (int i = 0; i < 20; i++) {
            im->sig[i] = 0x5A ^ i;      /* documented synthetic (§11.4) */
        }
        im->sig_real = 0;
    }
    printf("image %s: %u B payload, signature %s\n", path, im->len,
           im->sig_real ? "REAL — split from the container tail (§7.2)"
                        : "synthetic {0x5A^i} — responder checks lengths, "
                          "not signatures (§11.4)");
}

/* ---- the §9 boot layer ---------------------------------------------------- */

/* parse a §9.2 magic-framed reply in fr; returns the reply word */
static uint32_t boot_reply(const char *what)
{
    uint32_t w;

    CHECK(fr.type == TYPE_BOOTREPLY, "%s: type 0x%08x (want boot-class 2)\n",
          what, fr.type);
    CHECK(fr.plen == 0x20, "%s: payload %u B (want the 0x20-B frame)\n",
          what, fr.plen);
    CHECK(ldl(fr.payload + 0x00) == BOOT_MAGIC_A,
          "%s: magic A absent (0x%08x)\n", what, ldl(fr.payload + 0x00));
    CHECK(ldl(fr.payload + 0x1C) == BOOT_MAGIC_B,
          "%s: magic B absent (0x%08x)\n", what, ldl(fr.payload + 0x1C));
    w = ldl(fr.payload + 0x04);
    printf("   parse: magics ok; word 0x%08x -> count %u, answer %u; "
           "[cell+8] 0x%08x\n", w, w >> 24, w & 0x00FFFFFFu,
           ldl(fr.payload + 0x08));
    return w;
}

static void boot_stage1(void)
{
    uint32_t w;

    puts("stage-1: post cellA as a receive buffer — the boot ROM speaks "
         "first (§9.3)");
    rx_must("stage-1 cellA reply");
    w = boot_reply("stage-1");
    CHECK(w == 0x01000001u,
          "stage-1: reply word 0x%08x (want the count=1/answer=1 sentinel)\n",
          w);
    puts("   sentinel 0x01000001 -> image-id negotiation (§9.2)");
    CHECK(ldl(fr.payload + 0x08) == 3,
          "stage-1: image count %u (want 3)\n", ldl(fr.payload + 0x08));
    puts("   requested-count adopted: 0x7FFFFFFF -> 3 — the VC tells the "
         "host how many images to feed");
}

static void upload_image(const struct vcimg *im, const char *what)
{
    static char note[128];
    uint8_t hdr[0x20];
    uint32_t sent = 0, n, nchunks;

    nchunks = (im->len + CHUNK - 1) / CHUNK;
    printf("upload %s: %u B in %u chunk(s) of <= 0x%x, then the 0x20-B "
           "magic-framed header (cmd 0x42, §9.4 msgs 3-4/6-7)\n",
           what, im->len, nchunks, CHUNK);
    while (sent < im->len) {
        n = im->len - sent > CHUNK ? CHUNK : im->len - sent;
        snprintf(note, sizeof(note), "cmd-0x42 %s chunk %u/%u "
                  "(file 0x%06x..0x%06x)", what, (unsigned)(sent / CHUNK + 1),
                 nchunks, sent, sent + n - 1);
        tx_ok(1, TAG_DATA, TYPE_UPLOAD, im->data + sent, n, TMO_BULK_MS, note);
        sent += n;
    }

    memset(hdr, 0, sizeof(hdr));
    stl(hdr + 0x00, BOOT_MAGIC_A);
    stl(hdr + 0x04, (im->len & 0x00FFFFFFu) | 0x02000000u);
    memcpy(hdr + 0x08, im->sig, 20);
    stl(hdr + 0x1C, BOOT_MAGIC_B);
    snprintf(note, sizeof(note), "cmd-0x42 %s header: len 0x%x|0x02000000 "
             "+ 20-B signature (%s)", what, im->len,
             im->sig_real ? "real" : "synthetic");
    tx_ok(1, TAG_CTRL, TYPE_UPLOAD, hdr, sizeof(hdr), TMO_BULK_MS, note);
}

static void boot_stage2(const struct vcimg *loader, int variant_b)
{
    uint32_t w;

    puts("stage-2: upload the loader image, then exchange both cell "
         "replies (§9.4 msgs 2-5)");
    printf("   loader variant %s (§9.3 imgSel)\n",
           variant_b ? "B — set+0x10 (img2)" : "A — set+4 (img1)");
    upload_image(loader, "loader");
    /* the cellA/cellB receive-posts of §9.4 msg 2 are realized by the
     * two RECVs below: the driver ABI is synchronous and the queued
     * replies ride CONTRACT v1's HAT-hold (§12.4 item 5) */
    rx_must("cellA reply (loader accepted)");
    w = boot_reply("replyA");
    CHECK(w == 0x03000001u,
          "replyA: word 0x%08x (want count=3/answer=1)\n", w);
    puts("   replyA: count=3 answer=1 — loader running, ready for firmware");
    rx_must("cellB reply");
    w = boot_reply("replyB");
    CHECK((w >> 24) == 1, "replyB: count %u (want 1)\n", w >> 24);
    puts("   replyB: count=1 (answer value unpinned by decode — §11.4)");
}

static void sccs_scan(const struct vcimg *fw)
{
    uint32_t start = fw->len > 0x100 ? fw->len - 0x100 : 0;

    /* §9.3: Boot scans img3's last 0x100 bytes for the SCCS tag "@(#)"
     * and prints the version string — logging only, no protocol effect */
    for (uint32_t i = start; i + 4 <= fw->len; i++) {
        if (memcmp(fw->data + i, "@(#)", 4) == 0) {
            char tag[33];
            uint32_t n = 0;

            while (n < 32 && i + 4 + n < fw->len &&
                   fw->data[i + 4 + n] >= 0x20 && fw->data[i + 4 + n] < 0x7F) {
                tag[n] = (char)fw->data[i + 4 + n];
                n++;
            }
            tag[n] = 0;
            printf("   VideoCore image ID found: %s\n", tag);
            return;
        }
    }
    puts("   VideoCore image ID not found (synthetic image — the ROM img3 "
         "carries the \"@(#)\" tag, §9.3)");
}

static void boot_firmware(const struct vcimg *fw)
{
    uint32_t w;

    puts("main firmware: SCCS scan, upload, final exchange (§9.3)");
    sccs_scan(fw);
    upload_image(fw, "firmware");
    rx_must("final reply");
    w = boot_reply("final");
    CHECK(w == 0x03000001u,
          "final: word 0x%08x (want count=3/answer=1)\n", w);
    puts("   final: count=3 answer=1 -> VC BOOT COMPLETE");
}

/* ---- the §10 service layer ------------------------------------------------ */

/* §10.3 headline 3: per-message 0x10-B record + {id, args} payload */
static uint32_t svc_msg(uint8_t *p, uint32_t id, const uint32_t *args,
                        int nargs)
{
    uint32_t plen = 4 + nargs * 4;
    uint32_t total = 0x10 + plen;

    memset(p, 0, 0x10);
    p[2] = total & 0xFF;
    p[3] = total >> 8;
    stl(p + 0x04, FOURCC_DISP);
    p[0x0E] = plen & 0xFF;
    p[0x0F] = plen >> 8;
    stl(p + 0x10, id);
    for (int i = 0; i < nargs; i++) {
        stl(p + 0x14 + i * 4, args[i]);
    }
    return total;
}

/* verify a §10.6 service reply in fr ({0x10-B record, 4-B word} on the
 * given fourcc); returns the word */
static uint32_t svc_reply_word(uint32_t fourcc, const char *what)
{
    CHECK(fr.type == fourcc, "%s reply type 0x%08x (want fourcc 0x%08x)\n",
          what, fr.type, fourcc);
    CHECK(fr.plen == 0x14, "%s reply payload %u B (want 0x14)\n", what, fr.plen);
    CHECK(ldu16(fr.payload + 0x02) == 0x14 && ldl(fr.payload + 0x04) == fourcc,
          "%s reply record header wrong\n", what);
    CHECK(ldl(fr.payload + 0x08) == 0 && ldu16(fr.payload + 0x0C) == 0,
          "%s reply record reserved words nonzero\n", what);
    CHECK(ldu16(fr.payload + 0x0E) == 4,
          "%s reply record payload len %u (want 4)\n", what,
          ldu16(fr.payload + 0x0E));
    printf("   parse: record {total 0x14, fourcc 0x%08x, plen 4} + word "
           "0x%08x (%u)\n", fourcc, ldl(fr.payload + 0x10),
           ldl(fr.payload + 0x10));
    return ldl(fr.payload + 0x10);
}

static void service_connect(void)
{
    const uint32_t conn[5] = { 0, 1, 0x1010, CONN_ID, 0x800 };

    puts("connect: the 0x14-B control message on the control handle "
         "(§10.3)");
    printf("   {zeros, version 1, ver 0x1010, conn id 0x%x (ours), max "
           "message 0x800} — type 0x00100004\n", CONN_ID);
    tx_ok(0, TAG_MSG, TYPE_CONTROL, conn, sizeof(conn), TMO_REPLY_MS,
          "control connect");
    rx_must("connect echo");
    CHECK(fr.type == TYPE_CONTROL,
          "connect echo type 0x%08x\n", fr.type);
    CHECK(memcmp(fr.payload, conn, sizeof(conn)) == 0,
          "connect echo payload differs from the request\n");
    puts("   ok: peer echoed the connect message verbatim");
}

static uint32_t svc_update_start(void)
{
    uint8_t pl[0x40];
    uint32_t args[1] = { 1 };
    uint32_t handle;

    puts("DISP msg 0x10: update start (§10.5)");
    puts("   arg0 = 1 (the display/context word — §10.5 shape, semantics "
         "CONTRACT)");
    tx_ok(0, TAG_MSG, FOURCC_DISP, pl, svc_msg(pl, 0x10, args, 1),
          TMO_REPLY_MS, "DISP 0x10 update start");
    rx_must("0x10 reply");
    handle = svc_reply_word(FOURCC_DISP, "0x10");
    CHECK(handle != 0, "0x10 reply handle is 0\n");
    printf("   update handle %u captured (kept for 0x17/0x11 — §10.7 "
           "item 6)\n", handle);
    return handle;
}

static void svc_element_op(uint32_t handle)
{
    uint8_t pl[0x40];
    /* §10.5 msg 0x17: 7 base words + optional dest-rect + optional
     * src-rect (4 words each).  Beyond flags (|=4 &~8 at the backend)
     * the base-word semantics are not decoded — the values marked ours
     * are synthetic but shape-faithful; change-mask 0x3 is our
     * dest|src-rects-follow encoding. */
    const uint32_t el[15] = {
        handle,                    /* w0: update handle (rides the open update) */
        0,                         /* w1: element handle (0 = new) */
        0,                         /* w2: layer */
        0x3,                       /* w3: change-mask (ours: dest|src follow) */
        0x4,                       /* w4: flags (§10.5 backend |=4 &~8) */
        0xFF,                      /* w5: opacity */
        0,                         /* w6: transform */
        0, 0, 360, 640,            /* dest rect {x, y, w, h} (values synthetic) */
        0, 0, 360, 640,            /* src rect — a real one needs a VC resource
                                      handle via KHRN/BUFM (§10.5 note) */
    };

    puts("DISP msg 0x17: element op — change-mask + dest/src rects (§10.5)");
    printf("   base {update %u, element 0, layer 0, change-mask 0x3, "
           "flags 0x4, opacity 0xff, transform 0}\n", handle);
    puts("   dest rect {0,0,360,640}, src rect {0,0,360,640} — 360x640 "
         "panel shape, values synthetic");
    tx_ok(0, TAG_MSG, FOURCC_DISP, pl, svc_msg(pl, 0x17, el, 15),
          TMO_REPLY_MS, "DISP 0x17 element op (7 base + dest + src)");
    rx_must("0x17 reply");
    CHECK(svc_reply_word(FOURCC_DISP, "0x17") == 0,
          "0x17 ack != 0\n");
    puts("   ok: element op acked 0");
}

static void svc_badcrc_probe(void)
{
    uint8_t pl[0x40];
    uint32_t args[1] = { 1 };

    puts("bad-CRC probe: a deliberately corrupted frame mid-session "
         "(§8.4)");
    puts("   msg 0x12 {1} with the CRC bit-flipped — a frame the peer "
         "must reject");
    CHECK(tx_frame(0, TAG_MSG, FOURCC_DISP,
                   pl, svc_msg(pl, 0x12, args, 1), TMO_REPLY_MS, 1,
                   "DISP 0x12 with corrupted CRC (probe)") == -1 &&
          errno == EIO,
          "probe: SEND did not fail with EIO (rv -1 expected, errno %d)\n",
          errno);
    puts("   ok: SEND -> -EIO — the driver relayed the engine's error "
         "event (§12.2)");
    CHECK(rx_frame("post-rejection window", TMO_REPLY_MS) == -1 &&
          errno == ETIMEDOUT,
          "probe: a reply arrived — the corrupted frame was not "
          "rejected\n");
    printf("   ok: silent for %u ms — rejected with no reply (§8.4); the "
           "session recovers on the next exchange\n", TMO_REPLY_MS);
}

static void svc_submit(uint32_t handle)
{
    uint8_t pl[0x40];
    uint32_t args[2] = { handle, 0 };

    puts("DISP msg 0x11: update submit {handle, flag} (§10.5)");
    printf("   {handle %u, flag 0}\n", handle);
    tx_ok(0, TAG_MSG, FOURCC_DISP, pl, svc_msg(pl, 0x11, args, 2),
          TMO_REPLY_MS, "DISP 0x11 submit {handle, flag}");
    puts("   0x11 draws no DISP reply — the completion arrives as an "
         "UPDH push (§10.6)");

    for (int i = 0; i < 4; i++) {
        rx_must("UPDH completion drain");
        if (fr.type == FOURCC_UPDH) {
            CHECK(svc_reply_word(FOURCC_UPDH, "completion") == handle,
                  "UPDH completion handle mismatch (want %u)\n", handle);
            puts("   ok: completion drained on UPDH with the right "
                 "handle — update cycle complete");
            return;
        }
        printf("   drained a non-completion frame (type 0x%08x) — "
               "continuing\n", fr.type);
    }
    CHECK(0, "UPDH drain: no completion among the drained frames\n");
}

int main(int argc, char **argv)
{
    struct vcimg loader, fw;
    struct bcm2727_vchi_wait w;
    int variant_b = 0, first = 1;
    uint32_t handle;

    setbuf(stdout, NULL);
    if (argc > 1 && strcmp(argv[1], "-2") == 0) {
        variant_b = 1;
        first = 2;
    }
    if (argc != first + 2) {
        printf("usage: vc_session [-2] <loader-image> <firmware-image>\n"
               "  -2  loader variant B (§9.3 imgSel; emulation: same file)\n"
               "  images: payload, optionally ending with the §7.2 "
               "container tail (\"sign\" + 20-B signature)\n");
        return 1;
    }

    puts("vc_session: task 127 — M3, the VC-boot + dispman client "
         "(display-recon §9 boot layer + §10 service layer, through "
         "/dev/bcm2727-vchi)");
    crc_init();

    dfd = open("/dev/bcm2727-vchi", O_RDWR);
    CHECK(dfd >= 0, "open /dev/bcm2727-vchi (%s)\n", strerror(errno));
    puts("device open — the driver owns bring-up, the ring and the events");
    log_driver_mode();

    load_image(&loader, argv[first]);
    load_image(&fw, argv[first + 1]);

    puts("--- §9 boot layer ---");
    boot_stage1();
    boot_stage2(&loader, variant_b);
    boot_firmware(&fw);

    puts("--- §10 service layer ---");
    service_connect();
    handle = svc_update_start();
    svc_element_op(handle);
    svc_badcrc_probe();
    svc_submit(handle);

    memset(&w, 0, sizeof(w));
    w.timeout_ms = 0;                   /* snapshot */
    CHECK(ioctl(dfd, BCM2727_VCHI_IOC_WAIT, &w) == 0,
          "WAIT failed (%s)\n", strerror(errno));
    CHECK(w.master_ack == 0,
          "master ack 0x%08x with everything drained (HAT stuck?)\n",
          w.master_ack);
    puts("HAT low, master ack quiet — every exchange drained "
         "(§10.7 item 7 diagnostics)");

    puts("SESSION PASS");
    return 0;
}
