/*
 * vchi_smoketest.c — task 125 guest proof: boots the QEMU-side MeSSI
 * "VideoCore" (the model's env-gated responder) through the §9 boot
 * exchange grammar, then drives one §10 DISP/UPDH update cycle, with a
 * CRC-32C negative probe against the §8.4 failure semantics.  Asserts
 * every reply; prints SMOKE PASS.
 *
 * Decode references are docs/bcm2727-display-recon.md: §8.3 register map,
 * §8.4 frame + CRC-32C, §9.2/§9.3/§9.4 boot exchange grammar, §10.5/§10.6
 * service grammar.  Everything data-movement-shaped (the shared ring, the
 * descriptor format, the doorbell) is the task-125 MODEL CONTRACT — see
 * §11's CONTRACT table before assuming hardware agrees.
 *
 * Pure userspace: pokes the engine at PA 0x20204000 through /dev/mem and
 * builds the shared ring in a page-locked anonymous mapping whose PAs are
 * read from /proc/self/pagemap (root at the chain prompt has both).
 * Poll-mode throughout (never writes the pair-enable registers, so the
 * model never raises ICU 201 — the IRQ path belongs to M2's driver).
 *
 * Build (task-103 cross):
 *   toolchain/musl.cc/armv6-linux-musleabi-cross/bin/armv6-linux-musleabi-gcc \
 *     -static -Os -march=armv6 -mtune=arm1176jzf-s \
 *     -o vchi_smoketest tools/vchi_smoketest.c
 */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/sysmacros.h>
#include <sys/types.h>

/* ---- engine map (PA 0x20204000, model window 0x1480) --------------------- */
#define ENG_BASE        0x20204000u
#define ENG_MAP_LEN     0x2000u

#define R_MASTER_ACK    0x010
#define R_GLOBAL_CTRL   0x040
#define R_ENGINE_CONFIG 0x050
#define R_ENGINE_CTRL   0x240
#define R_RING_MODE     0x1444
#define R_RING_COOKIE   0x144C
#define R_RING_ACTIVATE 0x1460
#define R_GR_CLOCK      0x204
#define R_GR_STATUS     0x208
#define R_GR_ST(pair)   (0x210 + (pair) * 8)
#define R_CH_WIN(ch)    (0x250 + (ch) * 0x30)
#define R_CH_CTRL(ch)   (R_CH_WIN(ch) + 0x00)
#define R_CH_TAG(ch)    (R_CH_WIN(ch) + 0x04)
#define R_CH_LASTDESC   0x08

#define CH_CTRL_INIT    0xFF01002Cu   /* §8.3 per-channel init word */
#define CH_TAG(ch)      (((ch) << 16) | 0x3210u)

#define EV_TXDONE       0x100u
#define EV_RXAVAIL      0x400u
#define EV_ERROR        0x800u

/* ---- ring CONTRACT v1 (task-125 model contract) -------------------------- */
#define RING_MAGIC      0x3152534Du   /* "MSR1" */
#define NDESC           4u
#define DESC_ARMED      0x1u
#define DESC_DONE       0x2u
#define TAG_CTRL        0            /* boot upload header framing */
#define TAG_DATA        1            /* bulk image chunk */
#define TAG_MSG         2            /* small message */

/* ---- §8.4 frame ----------------------------------------------------------- */
#define FRAME_HDR       0x1Cu
#define BOOT_MAGIC_A    0xAFE45251u  /* §9.2 */
#define BOOT_MAGIC_B    0xF001BC4Eu
#define TYPE_UPLOAD     0x42u
#define TYPE_BOOTREPLY  0x2u
#define TYPE_CONTROL    0x00100004u
#define FOURCC_DISP     0x50534944u  /* "DISP" */
#define FOURCC_UPDH     0x48445055u  /* "UPDH" */

/* ---- synthetic upload payloads (content is not verified by the model) ---- */
#define LOADER_LEN      0x2050u      /* the real img1/img2 length (§7.3) */
#define FW_LEN          0x4000u
#define CHUNK           0xFE0u       /* keeps one frame inside a page */

/* ---- shared block layout (pages) ------------------------------------------ */
#define SH_PAGES        12
#define PG_RING         0            /* ring header + 8 rings */
#define PG_RX(n)        (1 + (n))    /* RX frame buffers 0..7 */
#define PG_TX           9            /* TX frame build buffer */

static volatile uint32_t *eng;
static uint8_t *sh;
static uint64_t sh_pa[SH_PAGES];

static uint32_t rd(uint32_t off)
{
    return eng[off / 4];
}

static void wr(uint32_t off, uint32_t v)
{
    eng[off / 4] = v;
}

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
 * algorithm the model implements, proved independently here. */
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

/* PA of a shared-block offset (component must not cross a page) */
static uint64_t pa_of(uint32_t off)
{
    return sh_pa[off / 0x1000] + (off & 0xFFF);
}

/* ---- ring plumbing ------------------------------------------------------- */
/* ring header at page 0 +0x00, the 8 rings at +0x40 */
#define RING_HDR_OFF    0x00
#define RINGS_OFF       0x40
#define RING_OFF(ch, tx) (RINGS_OFF + ((ch) * 2 + (tx)) * (NDESC * 16))
#define DESC_OFF(ch, tx, i) (RING_OFF(ch, tx) + (i) * 16)

static void desc_set(uint32_t off, uint32_t ctrl, uint64_t buf_pa,
                     uint32_t len, uint32_t cookie)
{
    stl(sh + off + 0x0, ctrl);
    stl(sh + off + 0x4, (uint32_t)buf_pa);
    stl(sh + off + 0x8, len);
    stl(sh + off + 0xC, cookie);
}

static uint32_t desc_ctrl(uint32_t off)
{
    return ldl(sh + off + 0x0);
}

static unsigned tx_seq[2], rx_seq, rx_read;

static void ring_setup(void)
{
    memset(sh + PG_RING * 0x1000, 0, 0x1000);
    stl(sh + RING_HDR_OFF + 0x0, RING_MAGIC);
    stl(sh + RING_HDR_OFF + 0x4, NDESC);
    for (int ch = 0; ch < 4; ch++) {
        for (int tx = 0; tx < 2; tx++) {
            stl(sh + RING_HDR_OFF + 0x8 + (ch * 2 + tx) * 4,
                (uint32_t)pa_of(RING_OFF(ch, tx)));
        }
    }
}

/* arm one RX buffer on channel 0 (the §9 "stage receive-post") + doorbell */
static void rx_post(const char *name)
{
    uint32_t idx = rx_seq % NDESC;
    uint32_t buf = PG_RX(rx_seq & 7) * 0x1000;

    memset(sh + buf, 0, 0x1000);
    desc_set(DESC_OFF(0, 0, idx), DESC_ARMED | (TAG_MSG << 8),
             pa_of(buf), 0x100, 0xA0000000u + rx_seq);
    rx_seq++;
    wr(R_CH_CTRL(0), CH_CTRL_INIT | 1u);          /* doorbell */
    printf("  rx-post %-6s desc %u armed\n", name, idx);
}

/* wait for an event bit on a channel's half of the pair status */
static int wait_event(int ch, uint32_t bit, uint32_t loops)
{
    uint32_t half = (ch & 1) ? bit << 16 : bit;

    for (uint32_t i = 0; i < loops; i++) {
        if (rd(R_GR_ST(ch >> 1)) & half) {
            return 0;
        }
        usleep(1000);
    }
    return -1;
}

static void ack_events(int pair, uint32_t bits)
{
    wr(R_GR_ST(pair), bits);                       /* W1C */
}

/* build + send one §8.4 frame on a channel; waits for the TX-done event */
static void tx_send(int ch, int tag, uint32_t type, const void *payload,
                    uint32_t plen, int corrupt_crc)
{
    uint8_t *f = sh + PG_TX * 0x1000;
    uint32_t idx = tx_seq[ch] % NDESC;
    uint32_t crc = crc32c(payload, plen);

    CHECK(plen + FRAME_HDR <= 0x1000, "frame too big for the TX page\n");
    memset(f, 0, FRAME_HDR);
    stl(f + 0x00, type);
    stl(f + 0x04, plen);
    if (corrupt_crc) {
        crc ^= 1;
    }
    stl(f + 0x0C, crc);
    memcpy(f + FRAME_HDR, payload, plen);
    desc_set(DESC_OFF(ch, 1, idx), DESC_ARMED | ((uint32_t)tag << 8),
             pa_of(PG_TX * 0x1000), FRAME_HDR + plen, 0xB0000000u + tx_seq[ch]);
    tx_seq[ch]++;
    wr(R_CH_CTRL(ch), CH_CTRL_INIT | 1u);          /* doorbell */
    CHECK(wait_event(ch, EV_TXDONE, 3000) == 0,
          "ch%d TX-done timeout (tag %d type 0x%08x)\n", ch, tag, type);
    ack_events(ch >> 1, EV_TXDONE << ((ch & 1) * 16));
}

/* receive side: the RXAVAIL latch coalesces frames (one bit, many
 * deliveries), so each event drains every completed RX descriptor into
 * a local queue and acks once — the drain shape a real driver uses */
struct rxframe {
    uint32_t type, plen;
    uint8_t payload[0x100];
};

static struct {
    struct rxframe f[8];
    int head, count;
} rxq;

static void rx_drain_ring(void)
{
    while (rx_read < rx_seq && rxq.count < 8) {
        uint32_t idx = rx_read % NDESC;
        uint32_t ctrl = desc_ctrl(DESC_OFF(0, 0, idx));
        uint32_t buf = PG_RX(rx_read & 7) * 0x1000;
        struct rxframe *fr;

        if (!(ctrl & DESC_DONE) || (ctrl & DESC_ARMED)) {
            break;
        }
        fr = &rxq.f[(rxq.head + rxq.count) % 8];
        fr->type = ldl(sh + buf + 0x00);
        fr->plen = ldl(sh + buf + 0x04);
        CHECK(fr->plen <= 0x100 - FRAME_HDR, "RX plen %u insane\n",
              fr->plen);
        memcpy(fr->payload, sh + buf + FRAME_HDR, fr->plen);
        rxq.count++;
        rx_read++;
    }
}

static void rx_wait(struct rxframe *fr)
{
    if (rxq.count == 0) {
        CHECK(wait_event(0, EV_RXAVAIL, 3000) == 0, "RX-available timeout\n");
        ack_events(0, EV_RXAVAIL);
        rx_drain_ring();
        CHECK(rxq.count > 0, "RX event with no completed descriptor\n");
    }
    *fr = rxq.f[rxq.head];
    rxq.head = (rxq.head + 1) % 8;
    rxq.count--;
}

/* ---- the exchanges -------------------------------------------------------- */

static uint32_t handle;   /* the msg-0x10 update handle */

static void boot_stage1(void)
{
    struct rxframe fr;

    puts("stage-1: post cellA, await the boot ROM's first word");
    rx_post("cellA");
    rx_wait(&fr);
    CHECK(fr.type == TYPE_BOOTREPLY, "stage-1 type 0x%08x\n", fr.type);
    CHECK(ldl(fr.payload + 0x00) == BOOT_MAGIC_A, "stage-1 magic A absent\n");
    CHECK(ldl(fr.payload + 0x1C) == BOOT_MAGIC_B, "stage-1 magic B absent\n");
    CHECK(ldl(fr.payload + 0x04) == 0x01000001u,
          "stage-1 reply word 0x%08x (want sentinel count=1/answer=1)\n",
          ldl(fr.payload + 0x04));
    CHECK(ldl(fr.payload + 0x08) == 3,
          "stage-1 image count %u (want 3)\n", ldl(fr.payload + 0x08));
    puts("  ok: sentinel 0x01000001, image count 3 (the VC speaks first)");
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
    struct rxframe fr;
    static const uint8_t loader[LOADER_LEN];       /* zero image: synthetic */

    puts("stage-2: post cellA+cellB, upload the loader, exchange headers");
    rx_post("cellA");
    rx_post("cellB");
    upload_image("loader", loader, LOADER_LEN);

    rx_wait(&fr);
    CHECK(fr.type == TYPE_BOOTREPLY, "replyA type 0x%08x\n", fr.type);
    CHECK(ldl(fr.payload + 0x04) == 0x03000001u,
          "replyA word 0x%08x (want count=3/answer=1)\n",
          ldl(fr.payload + 0x04));
    puts("  replyA: count=3 answer=1 (loader running)");
    rx_wait(&fr);
    CHECK((ldl(fr.payload + 0x04) >> 24) == 1,
          "replyB word 0x%08x (want count=1)\n", ldl(fr.payload + 0x04));
    puts("  replyB: count=1");
}

static void boot_firmware(void)
{
    struct rxframe fr;
    static const uint8_t fw[FW_LEN];

    puts("main firmware: post cellA, upload img3, final exchange");
    rx_post("cellA");
    upload_image("firmware", fw, FW_LEN);
    rx_wait(&fr);
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
    struct rxframe fr;
    uint8_t pl[0x40];
    uint32_t conn[5] = { 0, 1, 0x1010, 0x1234, 0x800 };
    uint32_t args[8];

    puts("connect: 0x14-B control {0,1,0x1010,conn 0x1234,0x800}");
    rx_post("svc");
    tx_send(0, TAG_MSG, TYPE_CONTROL, conn, sizeof(conn), 0);
    rx_wait(&fr);
    CHECK(fr.type == TYPE_CONTROL, "connect echo type 0x%08x\n", fr.type);
    CHECK(ldl(fr.payload + 0x0C) == 0x1234, "connect echo conn id wrong\n");
    puts("  ok: peer echoed the connect message");

    puts("DISP msg 0x10 (update start)");
    rx_post("svc");
    args[0] = 1;                                   /* display handle */
    tx_send(0, TAG_MSG, FOURCC_DISP, pl, svc_msg(pl, 0x10, args, 1), 0);
    rx_wait(&fr);
    CHECK(fr.type == FOURCC_DISP, "0x10 reply type 0x%08x\n", fr.type);
    handle = ldl(fr.payload + 0x10);
    CHECK(handle != 0, "0x10 reply handle is 0\n");
    printf("  ok: update handle %u\n", handle);

    puts("DISP msg 0x17 (element op, 7 base words)");
    rx_post("svc");
    for (int i = 0; i < 7; i++) {
        args[i] = i;                               /* synthetic {update,layer,...} */
    }
    tx_send(0, TAG_MSG, FOURCC_DISP, pl, svc_msg(pl, 0x17, args, 7), 0);
    rx_wait(&fr);
    CHECK(fr.type == FOURCC_DISP, "0x17 ack type 0x%08x\n", fr.type);
    CHECK(ldl(fr.payload + 0x10) == 0, "0x17 ack != 0\n");
    puts("  ok: element op acked");

    puts("DISP bad-CRC probe (the §8.4 failure semantics)");
    args[0] = 0;
    svc_msg(pl, 0x12, args, 1);
    tx_send(0, TAG_MSG, FOURCC_DISP, pl, 0x18, 1);
    CHECK(wait_event(0, EV_ERROR, 3000) == 0,
          "bad-CRC frame did not raise the error event\n");
    for (uint32_t i = 0; i < 200; i++) {
        usleep(1000);
        CHECK((rd(R_GR_ST(0)) & EV_RXAVAIL) == 0,
              "bad-CRC frame was answered (CRC not enforced)\n");
    }
    ack_events(0, EV_ERROR);
    puts("  ok: frame rejected (error event, no reply)");

    puts("DISP msg 0x11 (update submit {handle, flag})");
    rx_post("svc");
    args[0] = handle;
    args[1] = 0;
    tx_send(0, TAG_MSG, FOURCC_DISP, pl, svc_msg(pl, 0x11, args, 2), 0);
    rx_wait(&fr);
    CHECK(fr.type == FOURCC_UPDH, "0x11 completion type 0x%08x "
          "(want UPDH)\n", fr.type);
    CHECK(ldl(fr.payload + 0x10) == handle,
          "UPDH completion handle %u (want %u)\n",
          ldl(fr.payload + 0x10), handle);
    puts("  ok: completion arrived on UPDH with the right handle");
}

int main(void)
{
    int pm;
    struct stat st;

    setbuf(stdout, NULL);
    puts("vchi_smoketest: task 125 — MeSSI model + VC responder proof");
    crc_init();

    if (stat("/dev/mem", &st) != 0) {
        CHECK(mknod("/dev/mem", S_IFCHR | 0600, makedev(1, 1)) == 0,
              "cannot create /dev/mem\n");
    }
    int fd = open("/dev/mem", O_RDWR | O_SYNC);
    CHECK(fd >= 0, "open /dev/mem\n");
    eng = mmap(NULL, ENG_MAP_LEN, PROT_READ | PROT_WRITE, MAP_SHARED,
               fd, ENG_BASE);
    CHECK(eng != MAP_FAILED, "mmap engine @0x%08x\n", ENG_BASE);
    printf("engine mapped @0x%08x\n", ENG_BASE);

    sh = mmap(NULL, SH_PAGES * 0x1000, PROT_READ | PROT_WRITE,
              MAP_ANONYMOUS | MAP_PRIVATE | MAP_POPULATE, -1, 0);
    CHECK(sh != MAP_FAILED, "mmap shared block\n");
    memset(sh, 0, SH_PAGES * 0x1000);
    CHECK(mlock(sh, SH_PAGES * 0x1000) == 0, "mlock shared block\n");
    pm = open("/proc/self/pagemap", O_RDONLY);
    CHECK(pm >= 0, "open /proc/self/pagemap (need root)\n");
    for (int i = 0; i < SH_PAGES; i++) {
        uint64_t e;
        CHECK(pread(pm, &e, 8, ((uintptr_t)sh / 0x1000 + i) * 8) == 8,
              "pagemap read\n");
        CHECK(e & (1ULL << 63), "shared page %d not present\n", i);
        sh_pa[i] = (e & ((1ULL << 55) - 1)) * 0x1000;
        CHECK(sh_pa[i] != 0, "shared page %d PA==0 (pagemap needs root)\n", i);
    }
    close(pm);
    printf("shared block %d pages, PA[0]=0x%09llx\n", SH_PAGES,
           (unsigned long long)sh_pa[0]);

    /* §8.3 bring-up, in the decoded init order */
    ring_setup();
    wr(R_ENGINE_CONFIG, 0x842);                    /* engine config word */
    wr(R_GR_CLOCK, rd(R_GR_CLOCK) | 0x2000);       /* clock/divider bit13 */
    wr(R_ENGINE_CTRL, 0xB5F);                      /* engine control */
    for (int ch = 0; ch < 4; ch++) {
        wr(R_CH_CTRL(ch), CH_CTRL_INIT);
        wr(R_CH_TAG(ch), CH_TAG(ch));
    }
    wr(R_RING_COOKIE, (uint32_t)pa_of(RING_HDR_OFF));
    wr(R_RING_MODE, 6);
    wr(R_RING_ACTIVATE, 1);
    wr(R_ENGINE_CTRL, 0xB5F | 0x40);               /* bit6: start */
    usleep(1000);
    wr(R_ENGINE_CTRL, 0xB5F | 0x40);               /* bit11 released */
    wr(R_GLOBAL_CTRL, 0x80000);                    /* bit19 (start) */
    CHECK((rd(R_GR_STATUS) & 1) == 0, "engine busy after start\n");
    puts("bring-up: cfg 0x842, clk bit13, ctrl 0xB5F, tags, ring v1 "
         "(4 desc/ring), started");

    puts("--- §9 boot exchanges ---");
    boot_stage1();
    boot_stage2();
    boot_firmware();

    puts("--- §10 service cycle ---");
    service_cycle();

    CHECK((rd(R_MASTER_ACK) & (1u << 25)) == 0,
          "HAT asserted with nothing queued (0x%08x)\n", rd(R_MASTER_ACK));
    CHECK(rd(R_GR_ST(0)) == 0 && rd(R_GR_ST(1)) == 0,
          "pair statuses not drained (0x%08x/0x%08x)\n",
          rd(R_GR_ST(0)), rd(R_GR_ST(1)));
    puts("HAT low, all events drained, pair enables never written "
         "(poll mode)");

    puts("SMOKE PASS");
    return 0;
}
