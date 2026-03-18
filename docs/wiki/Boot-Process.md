# Nokia E7 Boot Process

## Hardware Boot Sequence

```
ROM Boot (OMAP3630 internal)
  ↓
SWBL (8.5KB @ NAND 0x800)
  → Configures SDRC/LPDDR timings
  ↓
NLoader (@ 0x10020000)
  → Authenticates chain (SHA-1 + RSA)
  → Reads GENIO_INIT (encrypted, TrustZone AES)
  → Configures pad mux (PADCONF registers)
  → Prints pad mux values to UART
  → Loads SOS image
  ↓
Symbian OS (ekern.exe)
  → Platform security enforcement
  → Device drivers load
  → UI starts
```

## NLoader Details (from QEMU boot analysis)

- Entry point: 0x1003259a
- Binary at: 0x10020000 (code) + 0x10000000 (data, mirror)
- PDRAM: 0x10000000-0x101FFFFF (2MB, extended for QEMU)
- TOC: 0x101ff000 (32 entries from OneNAND image)
- 34 unique debug messages observed in QEMU
- 31 in-memory patches needed for QEMU boot path
- Reached: GENIO auth OK → config build → SDRAM reporting → DATA ABORT
- **Blocked**: GENIO_INIT decryption requires TrustZone AES keys

## QEMU Linux Boot

```
QEMU nokia-e7 machine
  → Cortex-A8 @ reset vector
  → zImage self-decompresses
  → DTB parsed (omap3630-nokia-e7.dtb)
  → Kernel initializes OMAP3630 peripherals
  → I2C buses probe (TWL5031, sensors, keyboard)
  → Root filesystem mounts (initramfs or ext4 via virtio)
  → /init runs → shell
  → Boot time: ~1.4 seconds
```

## Key Boot Parameters

```bash
# Initramfs boot
earlycon clk_ignore_unused rdinit=/init

# ext4 root boot
earlycon clk_ignore_unused root=/dev/vda rw init=/init
```

`clk_ignore_unused` is critical — without it, the kernel disables clocks
that the emulated peripherals need, causing probe failures.
