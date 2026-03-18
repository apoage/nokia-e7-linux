# Hardware-Software Symbiosis Plan -- Nokia E7 Linux

**Date:** 2026-02-27
**Goal:** Select modern, efficient software components that best utilise the E7's
6 compute units, 256MB RAM, and dual-storage (OneNAND + eMMC) architecture.
**Principle:** Every choice must earn its bytes. Prefer solutions that exploit
the hardware we have, not fight against what we lack.

---

## 1. Hardware Inventory at a Glance

| Unit | Clock | Capability | Linux Status |
|------|-------|-----------|-------------|
| ARM Cortex-A8 | 1 GHz | General compute, NEON SIMD | Mainline, fully working |
| C64x+ DSP (IVA2.2) | ~520 MHz | 8-way VLIW signal processing | **Dead** -- kernel driver removed 2014 |
| PowerVR SGX530 | ~200 MHz | OpenGL ES 2.0 | **Dead** -- no open driver, stale blobs |
| BCM2727 VideoCore III | ~250 MHz | H.264/JPEG/ISP, dual Vec16 DSP | **Unexplored** -- needs RE of CCP2/VCHI |
| OMAP3 ISP | — | Camera pipeline (CCDC, preview, resize) | Mainline |
| sDMA | — | 32-channel DMA engine | Mainline, transparent |
| HW Crypto | — | AES, SHA1/256, MD5, RNG | Mainline (`omap-aes`, `omap-sham`) |

**Storage:** 256MB OneNAND (GPMC, MTD) + ~16GB eMMC (HSMMC, block device)
**RAM:** 256MB mDDR (shared across all cores)
**Display:** 640x360 AMOLED (CBD4441, DSI command mode)

---

## 2. Compute Strategy: What to Use, What to Skip

### Use: Cortex-A8 + NEON (primary workhorse)

The NEON unit is the only reliably usable accelerator. Every software choice
should be compiled with `-march=armv7-a -mfpu=neon -mfloat-abi=hard` to
exploit auto-vectorisation and hand-tuned NEON assembly.

**In-kernel NEON crypto** (all mainline, enable in .config):

| Config | Algorithm | Why it matters |
|--------|-----------|---------------|
| `CRYPTO_CHACHA20_NEON` | ChaCha20 | WireGuard VPN (~1 Gbps on Cortex-A8) |
| `CRYPTO_POLY1305_ARM` | Poly1305 | WireGuard MAC |
| `CRYPTO_AES_ARM_BS` | AES bit-sliced | Disk/network encryption |
| `CRYPTO_SHA256_ARM` | SHA-256 | dm-verity, TLS |
| `CRYPTO_NHPOLY1305_NEON` | NHPoly1305 | Adiantum encryption (fast on no-AES-HW) |
| `CRYPTO_CURVE25519_NEON` | Curve25519 | ECDH key exchange |

**Userspace libraries** (require NEON-aware builds):

| Library | Replaces | NEON benefit |
|---------|----------|-------------|
| libjpeg-turbo | libjpeg | 2-5x JPEG encode/decode |
| zlib-ng | zlib | NEON Adler-32, inflate |
| pixman | — | NEON compositing (critical for Wayland) |
| libsodium | OpenSSL (for NaCl) | NEON ChaCha20/Curve25519 |
| Ne10 | — | FFT, FIR, image processing |

### Use: OMAP3 HW Crypto + sDMA

The OMAP3630 has dedicated AES/SHA/MD5/RNG accelerators with DMA. These are
mainline and work transparently through the kernel crypto API:

```
CONFIG_CRYPTO_DEV_OMAP_AES=y    # HW AES via sDMA
CONFIG_CRYPTO_DEV_OMAP_SHAM=y   # HW SHA/MD5 via sDMA
CONFIG_HW_RANDOM_OMAP3_ROM=y    # HW random number generator
CONFIG_DMA_OMAP=y               # sDMA engine (also used by MMC, UART, display)
```

This offloads crypto from the CPU entirely -- critical for disk encryption
(fscrypt/dm-crypt) and WireGuard. The sDMA handles data movement between
memory and the crypto accelerators with zero CPU involvement.

### Use: BCM2727 VideoCore III (future -- highest value target)

The BCM2727 has its own 32MB RAM and dual Vec16 DSP cores. It can handle:
- H.264 encode/decode up to 720p
- JPEG hardware encode/decode
- 8MP camera ISP (auto-exposure, AWB, autofocus)
- HDMI 720p output

**Interface:** CCP2/MPHI (SubLVDS, 81 MB/s). ARM-boots-VC model (we control
firmware loading). Nokia's VCHI protocol uses slot-based circular buffers
(128 slots x 4KB) for service communication.

**Status:** Needs reverse engineering of MPHI framing protocol from `mphi_ccp2.c`
binary and VCHI service interface. The Raspberry Pi kernel's `vchiq` driver and
Broadcom's open-source VideoCore documentation provide reference material for
the VCHI protocol, though the CCP2 transport layer is Nokia-specific.

**Value:** This is the single highest-value compute unit to unlock. It would
give us hardware video encode/decode and camera processing without touching the
Cortex-A8 at all, and it has its own dedicated 32MB RAM.

### Skip: C64x+ DSP

Kernel driver (`tidspbridge`) removed from mainline in 2014. No open toolchain
(C64x+ requires TI's proprietary Code Generation Tools). The `remoteproc`
framework only supports OMAP4+. Reviving this for modern kernels would be a
multi-month effort for diminishing returns -- the BCM2727 covers the same
multimedia workloads with better hardware.

### Skip: PowerVR SGX530 3D

Imagination's 2024 open-source driver initiative covers only Rogue (Series 6+)
GPUs. The SGX530 is Series 5 -- completely different architecture, no open
driver path. TI's binary blobs are frozen and target ancient kernels. The
omapdrm KMS driver (mainline) provides display output without 3D. At 640x360
resolution, software rendering via pixman+NEON is viable for a UI compositor.

---

## 3. Filesystem Architecture

### Design Principles
1. **Read-only compressed root** -- minimise eMMC writes, maximise space efficiency
2. **Flash-aware writable layer** -- F2FS understands FTL garbage collection
3. **LZ4 everywhere** -- fastest decompression on Cortex-A8, good enough ratio
4. **Factory reset = erase overlay** -- clean separation of system and state
5. **zram swap** -- trade CPU cycles for effective RAM expansion

### Storage Layout

```
OneNAND (256MB, MTD via GPMC)
  mtd0: bootloader       256KB   raw
  mtd1: bootloader-env   128KB   raw
  mtd2: kernel+dtb       6MB     raw (direct load by bootloader)
  mtd3: recovery-rootfs  ~240MB  UBIFS on UBI (LZO compressed)

eMMC (16GB, block device via HSMMC)
  mmcblk0p1: rootfs      512MB   EROFS (LZ4 compressed, dm-verity)
  mmcblk0p2: overlay     2GB     F2FS (LZ4 compression, discard, fscrypt)
  mmcblk0p3: data        ~13GB   F2FS (LZ4 compression, discard, fscrypt)
```

### Mount Hierarchy

```
/           overlayfs (lower=/rom, upper=/overlay/upper, work=/overlay/work)
/rom        EROFS rootfs (read-only, verified)
/overlay    F2FS (writable state, packages, configs)
/data       F2FS (user data, media, logs)
/tmp        tmpfs (lost on reboot)
/run        tmpfs (runtime state)
```

### Filesystem Choices Explained

**EROFS over SquashFS** for read-only rootfs:
- Fixed-output compression aligns to 4KB device blocks (every read is useful data)
- SquashFS's variable-output compression wastes I/O on unaligned reads
- EROFS: ~57-63 MB/s sequential with LZ4 vs SquashFS: measurably slower
- EROFS has inline xattrs/data and compact metadata -- less RAM overhead
- Mainline since 5.4, actively developed (Android, Huawei containers)
- Used by modern Android for system partitions

**F2FS over ext4** for writable partitions:
- Designed for FTL-based flash (exactly what eMMC is)
- Write amplification ~1.02 vs ext4's journal-induced amplification
- Transparent LZ4 compression reduces write volume further
- Multi-stream writes (6 segments by data temperature) reduce GC overhead
- Async discard/TRIM for eMMC longevity
- fscrypt inline encryption supported

**UBIFS on UBI** for OneNAND:
- Only proper filesystem for raw NAND on modern Linux
- B+ tree index on flash (logarithmic RAM scaling, ~6MB for 256MB)
- LZO transparent compression
- Wear leveling and bad block management via UBI layer
- Nokia N900 used identical UBIFS parameters: `-m 2048 -e 129024 -c 2047`

**LZ4 over ZSTD/LZO** as compression algorithm:
- ~400-600 MB/s decompression on ARMv7 (2x faster than ZSTD)
- Good compression ratio (~2.1:1, vs ZSTD's ~2.8:1 but at 2x speed cost)
- On a 1GHz single-core, decompression speed matters more than ratio
- zram, EROFS, F2FS all support LZ4 -- one algorithm everywhere

---

## 4. Memory Strategy (256MB)

### RAM Budget

| Component | Allocation | Notes |
|-----------|-----------|-------|
| Kernel + modules | ~8 MB | Stripped, minimal config |
| EROFS rootfs cache | ~5 MB | Decompression buffers, page cache |
| F2FS metadata | ~4 MB | Overlay + data partitions |
| UBI/UBIFS (if mounted) | ~6 MB | OneNAND management |
| Userspace (musl + BusyBox + init) | ~4 MB | Minimal footprint |
| System services | ~10 MB | wpa_supplicant, udev, etc. |
| zram overhead | ~2 MB | Metadata for compressed pages |
| **Available for apps + cache** | **~217 MB** | |
| zram swap (effective) | +192-384 MB | 192MB device at 2-3:1 compression |

### zram Configuration

```bash
echo lz4 > /sys/block/zram0/comp_algorithm
echo 192M > /sys/block/zram0/disksize    # 75% of physical RAM
mkswap /dev/zram0 && swapon -p 100 /dev/zram0
```

LZ4 is the right choice for zram on this hardware -- highest throughput and
lowest latency under memory pressure. zram swap is dramatically faster than
eMMC swap and causes zero flash wear.

### DMABUF Zero-Copy Pipeline (future)

When camera/video is working, use DMABUF to share buffers between:
ISP -> BCM2727 -> Display (via omapdrm) -- no CPU copies needed.
This keeps the Cortex-A8 out of the video data path entirely.

---

## 5. Userspace Stack

### Recommended

| Component | Choice | Rationale |
|-----------|--------|-----------|
| **C library** | musl | 9x smaller than glibc, ~4MB base image, proven (Alpine) |
| **Core utils** | BusyBox | ~400 applets in 1MB, includes vi/udhcpc/ash |
| **Init** | dinit | Dependency-based (like systemd), tiny C++ binary, fast boot |
| **Networking** | iwd + WireGuard | iwd replaces wpa_supplicant (smaller, D-Bus-less mode available) |
| **Display** | cage (Wayland) | Single-app kiosk compositor on omapdrm, uses pixman+NEON |
| **VPN** | WireGuard (kernel) | NEON ChaCha20-Poly1305, minimal overhead |
| **Shell** | ash (BusyBox) | Fast, small, POSIX-compliant |
| **Crypto userspace** | libsodium | NEON-optimised NaCl primitives |
| **Compression** | zlib-ng | Drop-in zlib replacement with NEON optimisation |

### Why Not These

| Rejected | Reason |
|----------|--------|
| glibc | Too large for 256MB; NSS/iconv dlopen complexity |
| systemd | ~40MB RAM overhead; overkill for single-purpose device |
| OpenRC | Shell-script overhead; dinit or runit are lighter |
| wpa_supplicant | iwd is smaller, faster, modern replacement |
| X11 | Dead weight; Wayland on omapdrm is the modern path |
| toybox | Only ~230 commands vs BusyBox's 400; still maturing |

### Compiler Flags (all userspace)

```
CFLAGS="-march=armv7-a -mfpu=neon -mfloat-abi=hard -O2 -pipe"
LDFLAGS="-Wl,-O1,--sort-common,--as-needed,-z,relro,-z,now"
```

This ensures all code uses hard-float and NEON auto-vectorisation. The linker
flags reduce binary size and improve security (RELRO, NOW binding).

---

## 6. Kernel Configuration Essentials

```kconfig
# Compute
CONFIG_NEON=y
CONFIG_KERNEL_MODE_NEON=y
CONFIG_VFP=y

# sDMA + HW Crypto
CONFIG_DMA_OMAP=y
CONFIG_CRYPTO_DEV_OMAP_AES=y
CONFIG_CRYPTO_DEV_OMAP_SHAM=y
CONFIG_HW_RANDOM_OMAP3_ROM=y

# NEON Crypto
CONFIG_CRYPTO_CHACHA20_NEON=y
CONFIG_CRYPTO_POLY1305_ARM=y
CONFIG_CRYPTO_AES_ARM_BS=y
CONFIG_CRYPTO_SHA256_ARM=y
CONFIG_CRYPTO_NHPOLY1305_NEON=y
CONFIG_CRYPTO_CURVE25519_NEON=y

# Storage
CONFIG_EROFS_FS=y
CONFIG_EROFS_FS_LZ4=y
CONFIG_F2FS_FS=y
CONFIG_F2FS_FS_LZ4=y
CONFIG_UBIFS_FS=y
CONFIG_OVERLAY_FS=y

# Memory
CONFIG_ZRAM=y
CONFIG_ZRAM_DEF_COMP_LZ4=y
CONFIG_CMA=y

# Integrity
CONFIG_DM_VERITY=y

# Display
CONFIG_DRM_OMAP=y

# Networking
CONFIG_WIREGUARD=y

# Strip bloat
# CONFIG_SOUND is not set        (until audio driver is ready)
# CONFIG_USB_GADGET is not set   (until USB is needed)
# CONFIG_BT is not set           (until BT driver is ready)
```

---

## 7. Data Flow Architecture

```
                          256MB DDR
                    +-----------------------+
                    |  Kernel + drivers     |
                    |  Userspace (musl)     |
                    |  zram swap pool       |
                    |  Page cache (EROFS)   |
                    +-----------+-----------+
                                |
        +-----------+-----------+-----------+-----------+
        |           |           |           |           |
    Cortex-A8    sDMA        OMAP3       omapdrm    HW Crypto
    + NEON     (32 ch)       ISP         (DSS)     AES/SHA/RNG
        |           |           |           |           |
    [App logic] [Peripheral  [Camera     [640x360   [Disk/VPN
     pixman     I/O: MMC,    pipeline]   AMOLED     encryption
     libjpeg    UART, etc]               display]    offload]
     zlib-ng]
        |
        +------ CCP2/MPHI (81 MB/s) ------+
                                           |
                                    BCM2727 VC III
                                    (32MB own RAM)
                                    [H.264, JPEG,
                                     Camera ISP,
                                     HDMI 720p]
```

The key insight: the Cortex-A8 handles logic and UI, the BCM2727 handles
multimedia, HW crypto handles encryption, and sDMA moves data between them
all without CPU involvement. The 256MB DDR is preserved for application
use by keeping the rootfs compressed and read-only, using zram for swap,
and eventually offloading video to the BCM2727's dedicated 32MB.

---

## 8. Priority Order for Implementation

1. **Rootfs with shell** (Task 010) -- musl + BusyBox + EROFS image, boot to sh
2. **zram swap** -- immediate RAM relief
3. **F2FS overlay** -- persistent writable layer on eMMC
4. **HW crypto + WireGuard** -- network security stack
5. **omapdrm + display** -- framebuffer on AMOLED
6. **BCM2727 VCHI driver** -- unlock hardware multimedia (highest long-term value)
7. **Camera pipeline** -- ISP -> BCM2727 -> DMABUF -> display
