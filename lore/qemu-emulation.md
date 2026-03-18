# QEMU Emulation Notes

## Machine
- Name: `nokia-e7`, SoC type: `omap3630`
- Source: `emulation/qemu/hw/arm/omap3630.c` (SoC) + `nokia_e7.c` (board)
- Build: `cd emulation/qemu && ninja -C build`
- Boot: **~0.82s** to shell (ext4 root via virtio-blk)

## Register Stubs (omap3630.c)

### Full models (track state)
| Block | Address | Key behavior |
|-------|---------|-------------|
| CM | 0x48004000 | DPLL state tracking, IDLEST derived from CLKEN_PLL |
| PRM | 0x48306000 | PRM_CLKSEL=3 (26MHz), PM_PWSTST=ON, RM_RSTST=0 |
| SCM | 0x48002000 | CONTROL_STATUS=0x300 (GP device type) |
| TAP | 0x4830A000 | IDCODE=0x2B89102F (OMAP3630 ES1.2) |
| I2C1 | 0x48070000 | Full controller: REV/IE/STAT/CON/SA/CNT/DATA + TWL5031 PMIC |
| I2C2 | 0x48072000 | Full controller: stub (no devices wired) |
| I2C3 | 0x48060000 | Full controller: LM8323 (0x34) + 5 sensor stubs + INTC IRQ 61 |
| HSMMC1 | 0x4809C000 | SYSSTATUS.RDONE=1, SYSCTL.ICS=1 |
| Sync32K | 0x48320000 | Free-running 32768 Hz counter |
| GPT1/GPT2 | 0x48318/49032000 | Full timer with IRQ support |

| SDRC | 0x6D000000 | DLLA_CTRL/STATUS tracking, DLL lock mode, POWER reg |
| DSS/DISPC | 0x48050000 | Full display model: GFX_BA0, pixel format, QEMU console |
| TWL5031 | on I2C1 | 4 slaves (0x48-0x4B), 256-byte regmaps, IDCODE, R_CFG_BOOT |
| sDMA | 0x48056000 | 32-ch instant-completion, CCR/CSR/IRQSTATUS, IRQ L1-L3 (13-15) |
| GPIO1-6 | 0x48310000+ | OMAP24XX register model, W1C IRQSTATUS, OE=0xFFFFFFFF, REV=0x0025 |
| LM8323 | on I2C3 | Command-based I2C, 15-entry FIFO, INTC IRQ 35, sendkey pipeline |
| I2C Sensor Stubs | on I2C3 | 5 sensors: LIS302DL x2, AK8974, APDS990x, mXT (see below) |

### I2C Sensor Stubs (Task 043)
Added `SensorStubState` (generic) and `MXTStubState` (touchscreen) data structures
in `omap3630.c`. All wired to I2C3 controller via `has_device`, `read fill`,
`write dispatch`, and `state reset` integration points.

| Sensor | Address | Type | Probe Result |
|--------|---------|------|-------------|
| LIS302DL #1 | 0x1C | SensorStubState (generic) | OK — "8 bits sensor found" |
| LIS302DL #2 | 0x1D | SensorStubState (generic) | OK — "8 bits sensor found" |
| AK8974 | 0x0F | SensorStubState (generic) | OK — probes, selftest warning only |
| APDS-990x | 0x39 | SensorStubState (generic) | OK — probes fully (OF patch, ga=0 defaults; dummy regulator warnings expected in QEMU) |
| mXT touch | 0x4C | MXTStubState (2-byte LE addr, 10-byte info block) | Reads info block, then fails at bootloader recovery (-121 at 0x26/0x27, not 0x4C) |

- Generic sensors: register-addressed I2C (first TX byte = register pointer, auto-increment reads)
- mXT: 2-byte little-endian address protocol, returns 10-byte information block
- Boot time still ~1s (no regression)

### Periph stubs (return SYSSTATUS=1 for all reads)
GPT3-12, UART1/2/4, McSPI1-4, AES1/2, RNG, SHAM, HDQ,
DSI1, MUSB, WDT2, GPMC, HSMMC2/3, USB host

### Still unimplemented (return 0)
- SMS (0x6C000000)

## TWL5031 PMIC Model
- 4 I2C slave addresses: 0x48 (USB/power), 0x49 (audio/GPIO), 0x4A (PMU), 0x4B (MADC)
- Each slave has 256-byte register array (read/write)
- Pre-initialized: IDCODE=0x57031505 (slave 1, 0x85-0x88), R_CFG_BOOT=0x02, GPPUPDCTR1=0x55
- Transfer state machine: first byte = register pointer, subsequent = data (auto-increment)
- Kernel probes: twl-core, twl4030-gpio (18 GPIOs + IRQ cascade), twl4030-power, 8 regulators

## I2C Controller Model
- Full register set: REV(0x00), IE(0x04), STAT(0x08), WE(0x0C), BUF(0x14), CNT(0x18),
  DATA(0x1C), SYSC(0x20), CON(0x24), OA(0x28), SA(0x2C), PSC(0x30), SCLL(0x34), SCLH(0x38)
- Revision 0x0040 (matches OMAP_I2C_REV_ON_3630 — avoids b_hw workaround path)
- Transfer: CON.STT triggers start, XRDY/RRDY for data exchange, ARDY on completion
- NACK for unknown slave addresses, write-1-to-clear STAT with XRDY/RRDY re-assertion
- IRQ wired to INTC (I2C1=56, I2C2=57, I2C3=61)

## DTS Overrides (omap3630-nokia-e7.dts)
- UART1/2/4: disabled (saves hwmod probe time)
- MMC1/2/3: disabled (no DMA controller)
- bandgap: disabled
- virtio_mmio@4e000000: QEMU block storage, IRQ 12

## Boot Flow
1. Kernel decompresses, finds DTB, sets up memory
2. INTC, clock tree init (ssi_ssr_fck warning — harmless)
3. Calibrating delay loop (125ms)
4. UART3 console enabled at ttyS0
5. I2C buses probe (3 controllers at 400/100/400 kHz)
6. TWL5031 probe: PIH IRQ cascade, power IRQ, GPIO expander
7. GPIO banks probe: 6 banks, OMAP GPIO hardware version 2.5
8. LM8323 keyboard probes on I2C3 at 0x34, input device created
9. Sensor probes: LIS302DL x2 OK, AK8974 OK, APDS990x OK, mXT info block read
10. Regulators come up: vaux1/2, vdac, vpll1/2, vsim, vintana2, VMMC2
11. virtio-blk detects vda (64MB)
12. Mount ext4, devtmpfs, Run /init → shell (~1s)

## virtio-blk
- virtio-mmio at 0x4E000000, IRQ 12
- FLUSH command hangs without `cache.no-flush=on`
- Reads and regular writes work, only REQ_PREFLUSH/sync() hangs
