/*
 * vchi_smoketest_drv.c — task 126 guest proof: the same §9-boot +
 * §10-update + bad-CRC sequences as tools/vchi_smoketest.c (task 125),
 * but through the M2 Linux VCHI transport driver /dev/bcm2727-vchi
 * instead of raw /dev/mem pokes.  The kernel driver owns the §11.6
 * bring-up, the CONTRACT-v1 ring, the doorbells and the event
 * servicing (poll or IRQ mode, vchi-bcm2727.poll=); this test is a
 * pure frame-level client: one complete §8.4 wire frame per
 * ioctl(SEND/RECV).  Asserts every reply; prints SMOKE PASS.
 *
 * Decode references are docs/bcm2727-display-recon.md: §8.4 frame +
 * CRC-32C, §9.2/§9.3/§9.4 boot exchange grammar, §10.5/§10.6 service
 * grammar, §11.2 ring CONTRACT v1, §12 the M2 as-built (the ABI).
 *
 * Ordering note vs the task-125 test: the driver ABI is synchronous
 * (RECV blocks), so the userspace probe's "post RX before send" shape
 * becomes send-then-receive everywhere except stage 1 — the VC boot
 * ROM speaks first once an RX post's doorbell rings, so stage 1 is a
 * bare RECV.  Peer frames queued while no descriptor is armed (the
 * two stage-2 replies, every service reply) are held for the next
 * RECV's doorbell — CONTRACT v1's HAT-delivery path.
 *
 * Build (task-103 cross; the quoted include pulls the committed uapi
 * header — its <linux/types.h> resolves against the musl sysroot):
 *   toolchain/musl.cc/armv6-linux-musleabi-cross/bin/armv6-linux-musleabi-gcc \
 *     -static -Os -march=armv6 -mtune=arm1176jzf-s \
 *     -o vchi_smoketest_drv tools/vchi_smoketest_drv.c
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

/* ---- synthetic upload payloads (content is not verified by the model) ---- */
#define LOADER_LEN      0x2050u      /* the real img1/img2 length (§7.3) */
#define FW_LEN          0x4000u
#define CHUNK           0xFE0u       /* keeps one frame inside the TX page */

static int dfd;

#define CHECK(cond, ...)                                                  \
    do {                                                                  \
        if (!(cond)) {                                                    \
            printf("SMOKE FAIL: ");                                       \
            printf(__VA_ARGS__);                                          \
            printf("  (line %d)\n", __LINE__);                            \
            exit(1);                                                      \
        }                                                                 \
    } while (0)

/* CRC-32C, table generated exactly as the ROM's boot-time generator
 * (§8.4: init 0xFFFFFFFF, reflected 0x82F63B78, no final xor) — the same
 * algorithm the model enforces on every received frame. */
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

static void stl(uint8_t *p, uint32_t v)
{
    p[0] = v; p[1] = v >> 8; p[2] = v >> 16; p[3] = v >> 24;
}

/* ---- the driver ABI: one wire frame per call ------------------------------ */

/* build + send one §8.4 frame on a channel; blocks for the TX-done */
static void tx_send(int ch, int tag, uint32_t type, const void *payload,
                    uint32_t plen, int corrupt_crc)
{
    static uint8_t f[0x1000];
    struct bcm2727_vchi_send s;
    uint32_t crc = crc32c(payload, plen);
    long rv;

    CHECK(plen + FRAME_HDR <= sizeof(f), "frame too big for the TX buffer\n");
    memset(f, 0, FRAME_HDR);
    stl(f + 0x00, type);
    stl(f + 0x04, plen);
    if (corrupt_crc) {
        crc ^= 1;
    }
    stl(f + 0x0C, crc);
    memcpy(f + FRAME_HDR, payload, plen);

    memset(&s, 0, sizeof(s));
    s.buf_ptr = (uintptr_t)f;
    s.len = FRAME_HDR + plen;
    s.channel = ch;
    s.tag = tag;
    rv = ioctl(dfd, BCM2727_VCHI_IOC_SEND, &s);
    CHECK(rv == 0, "ch%d SEND failed (%s, tag %d type 0x%08x)\n",
          ch, strerror(errno), tag, type);
}

/* the last frame delivered by rx_recv / rx_recv_rv (single-threaded) */
struct rxframe {
    uint32_t type, plen;
    uint8_t payload[0x200];
};
static struct rxframe fr;

/* receive one frame; rv = the ioctl return (0 ok, -1 errno set) */
static int rx_recv_rv(const char *name, uint32_t timeout_ms)
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
          "RECV %s length %u insane\n", name, r.len);
    fr.type = ldl(buf + 0x00);
    fr.plen = ldl(buf + 0x04);
    CHECK(fr.plen + FRAME_HDR == r.len,
          "RECV %s payload len %u vs frame %u (mismatch)\n",
          name, fr.plen, r.len);
    memcpy(fr.payload, buf + FRAME_HDR, fr.plen);
    return 0;
}

static void rx_recv(const char *name)
{
    CHECK(rx_recv_rv(name, 0) == 0, "RECV %s failed (%s)\n",
          name, strerror(errno));
}

/* ---- the exchanges -------------------------------------------------------- */

static void boot_stage1(void)
{
    puts("stage-1: bare receive — the VC boot ROM speaks first");
    rx_recv("cellA");
    CHECK(fr.type == TYPE_BOOTREPLY, "stage-1 type 0x%08x\n", fr.type);
    CHECK(ldl(fr.payload + 0x00) == BOOT_MAGIC_A, "stage-1 magic A absent\n");
    CHECK(ldl(fr.payload + 0x1C) == BOOT_MAGIC_B, "stage-1 magic B absent\n");
    CHECK(ldl(fr.payload + 0x04) == 0x01000001u,
          "stage-1 reply word 0x%08x (want sentinel count=1/answer=1)\n",
          ldl(fr.payload + 0x04));
    CHECK(ldl(fr.payload + 0x08) == 3,
          "stage-1 image count %u (want 3)\n", ldl(fr.payload + 0x08));
    puts("  ok: sentinel 0x01000001, image count 3");
}

static void upload_image(const char *what, const uint8_t *img, uint32_t len)
{
    uint8_t hdr[0x20];
    uint32_t sent = 0, n, chunks = 0;

    while (sent < len) {
        n = len - sent > CHUNK ? CHUNK : len - sent;
        tx_send(1, TAG_DATA, TYPE_UPLOAD, img + sent, n, 0);
        sent += n;
        chunks++;
    }
    printf("  upload %s: %u B in %u chunks, header {len 0x%x|0x02000000, sig}\n",
           what, len, chunks, len);
    memset(hdr, 0, sizeof(hdr));
    stl(hdr + 0x00, BOOT_MAGIC_A);
    stl(hdr + 0x04, (len & 0x00FFFFFFu) | 0x02000000u);
    for (int i = 0; i < 20; i++) {                 /* synthetic signature */
        hdr[0x08 + i] = 0x5A ^ i;
    }
    stl(hdr + 0x1C, BOOT_MAGIC_B);
    tx_send(1, TAG_CTRL, TYPE_UPLOAD, hdr, sizeof(hdr), 0);
}

static void boot_stage2(void)
{
    static const uint8_t loader[LOADER_LEN];       /* zero image: synthetic */

    puts("stage-2: upload the loader, then exchange both cell replies");
    upload_image("loader", loader, LOADER_LEN);

    rx_recv("replyA");
    CHECK(fr.type == TYPE_BOOTREPLY, "replyA type 0x%08x\n", fr.type);
    CHECK(ldl(fr.payload + 0x04) == 0x03000001u,
          "replyA word 0x%08x (want count=3/answer=1)\n",
          ldl(fr.payload + 0x04));
    puts("  replyA: count=3 answer=1 (loader running)");
    rx_recv("replyB");
    CHECK((ldl(fr.payload + 0x04) >> 24) == 1,
          "replyB word 0x%08x (want count=1)\n", ldl(fr.payload + 0x04));
    puts("  replyB: count=1");
}

static void boot_firmware(void)
{
    static const uint8_t fw[FW_LEN];

    puts("main firmware: upload img3, final exchange");
    upload_image("firmware", fw, FW_LEN);
    rx_recv("final");
    CHECK(fr.type == TYPE_BOOTREPLY, "final type 0x%08x\n", fr.type);
    CHECK(ldl(fr.payload + 0x04) == 0x03000001u,
          "final word 0x%08x (want count=3/answer=1)\n",
          ldl(fr.payload + 0x04));
    puts("  final: count=3 answer=1 -> VC BOOT COMPLETE");
}

/* §10 per-message record + {id, args} payload (record u16s are LE) */
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

static void service_cycle(void)
{
    uint8_t pl[0x40];
    uint32_t conn[5] = { 0, 1, 0x1010, 0x1234, 0x800 };
    uint32_t args[8], handle = 0;
    long rv;

    puts("connect: 0x14-B control {0,1,0x1010,conn 0x1234,0x800}");
    tx_send(0, TAG_MSG, TYPE_CONTROL, conn, sizeof(conn), 0);
    rx_recv("svc");
    CHECK(fr.type == TYPE_CONTROL, "connect echo type 0x%08x\n", fr.type);
    CHECK(ldl(fr.payload + 0x0C) == 0x1234, "connect echo conn id wrong\n");
    puts("  ok: peer echoed the connect message");

    puts("DISP msg 0x10 (update start)");
    args[0] = 1;                                   /* display handle */
    tx_send(0, TAG_MSG, FOURCC_DISP, pl, svc_msg(pl, 0x10, args, 1), 0);
    rx_recv("svc");
    CHECK(fr.type == FOURCC_DISP, "0x10 reply type 0x%08x\n", fr.type);
    handle = ldl(fr.payload + 0x10);
    CHECK(handle != 0, "0x10 reply handle is 0\n");
    printf("  ok: update handle %u\n", handle);

    puts("DISP msg 0x17 (element op, 7 base words)");
    for (int i = 0; i < 7; i++) {
        args[i] = i;                               /* synthetic {update,layer,...} */
    }
    tx_send(0, TAG_MSG, FOURCC_DISP, pl, svc_msg(pl, 0x17, args, 7), 0);
    rx_recv("svc");
    CHECK(fr.type == FOURCC_DISP, "0x17 ack type 0x%08x\n", fr.type);
    CHECK(ldl(fr.payload + 0x10) == 0, "0x17 ack != 0\n");
    puts("  ok: element op acked");

    puts("DISP bad-CRC probe (the §8.4 failure semantics)");
    args[0] = 0;
    svc_msg(pl, 0x12, args, 1);
    {
        /* corrupt the CRC, expect the driver to hand back the engine's
         * error event as SEND -EIO, and NO frame within 300 ms after */
        static uint8_t f[0x100];
        struct bcm2727_vchi_send s;
        uint32_t plen = 0x18, crc = crc32c(pl, plen);

        memset(f, 0, FRAME_HDR);
        stl(f + 0x00, FOURCC_DISP);
        stl(f + 0x04, plen);
        stl(f + 0x0C, crc ^ 1);
        memcpy(f + FRAME_HDR, pl, plen);
        memset(&s, 0, sizeof(s));
        s.buf_ptr = (uintptr_t)f;
        s.len = FRAME_HDR + plen;
        s.channel = 0;
        s.tag = TAG_MSG;
        rv = ioctl(dfd, BCM2727_VCHI_IOC_SEND, &s);
        CHECK(rv == -1 && errno == EIO,
              "bad-CRC SEND did not fail with EIO (rv %ld errno %d)\n",
              rv, errno);
        printf("  ok: SEND -> EIO (driver relayed the error event)\n");
    }
    CHECK(rx_recv_rv("no-reply", 300) == -1 && errno == ETIMEDOUT,
          "bad-CRC frame was answered (CRC not enforced)\n");
    puts("  ok: no reply within 300 ms (frame rejected)");

    puts("DISP msg 0x11 (update submit {handle, flag})");
    args[0] = handle;
    args[1] = 0;
    tx_send(0, TAG_MSG, FOURCC_DISP, pl, svc_msg(pl, 0x11, args, 2), 0);
    rx_recv("svc");
    CHECK(fr.type == FOURCC_UPDH, "0x11 completion type 0x%08x "
          "(want UPDH)\n", fr.type);
    CHECK(ldl(fr.payload + 0x10) == handle,
          "UPDH completion handle %u (want %u)\n",
          ldl(fr.payload + 0x10), handle);
    puts("  ok: completion arrived on UPDH with the right handle");
}

int main(void)
{
    struct bcm2727_vchi_wait w;
    long rv;

    setbuf(stdout, NULL);
    puts("vchi_smoketest_drv: task 126 — M2 transport driver proof");
    crc_init();

    dfd = open("/dev/bcm2727-vchi", O_RDWR);
    CHECK(dfd >= 0, "open /dev/bcm2727-vchi (%s)\n", strerror(errno));
    puts("device open: the driver owns bring-up, ring and events");

    puts("--- §9 boot exchanges ---");
    boot_stage1();
    boot_stage2();
    boot_firmware();

    puts("--- §10 service cycle ---");
    service_cycle();

    memset(&w, 0, sizeof(w));
    w.timeout_ms = 0;                              /* snapshot */
    rv = ioctl(dfd, BCM2727_VCHI_IOC_WAIT, &w);
    CHECK(rv == 0, "WAIT failed (%s)\n", strerror(errno));
    CHECK(w.master_ack == 0,
          "master ack 0x%08x with everything drained (HAT stuck?)\n",
          w.master_ack);
    puts("HAT low, master ack quiet, all exchanges drained");

    puts("SMOKE PASS");
    return 0;
}
