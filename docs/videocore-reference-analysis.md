# VideoCore Reference Firmware Analysis

> **Note (2026-03-23):** Real HW is BCM2727B1 (ARM1176), not OMAP3630. See docs/critical-cpu-discovery.md

*Task 050 — 2026-03-03*

---

## Sources Collected

### 1. RPi Firmware (boot/start.elf)
- **Location:** `firmware-re/videocore-ref/rpi-firmware/boot/`
- **Files:** start.elf (2.9MB), start_cd.elf (831KB), start_db.elf (4.7MB), start_x.elf (3.7MB)
- **Also:** start4*.elf variants for RPi 4 (BCM2711, different architecture)
- **Format:** ELF32, EM_VIDEOCORE3, ET_EXEC (monolithic executable)
- **RTOS:** ThreadX (confirmed: `_tx_thread_created_next_offset` in strings)
- **Toolchain:** MetaWare Linker (same as BCM2727 VLLs)

### 2. rpi-open-firmware
- **Location:** `firmware-re/videocore-ref/rpi-open-firmware/`
- **Contents:** Open-source VPU bootloader + Broadcom BCM2708 register headers
- **Key value:** 100+ register header files with complete MMIO maps
- **VPU boot code:** `start.s` — real VC4 assembly with exception handlers

---

## BCM2708 Register Address Map

Extracted from `broadcom/bcm2708_chip/*.h`. BCM2708 is the RPi VC4 SoC;
BCM2727 is the Nokia E7's VC3 SoC. Both share the VPU core, so peripheral
register layouts are expected to be similar (possibly different base addresses).

| Base Address | Block | Regs | APB ID (ASCII) | BCM2727 Relevance |
|-------------|-------|------|----------------|-------------------|
| 0x7e001000 | CCP2TX (transmitter) | 11 | "ccp2" | **HIGH** — firmware upload path |
| 0x7e002000 | IC0 (interrupt ctrl) | 21 | "INTE" | Reference for VC interrupt model |
| 0x7e002800 | IC1 (interrupt ctrl) | 21 | "INTE" | Reference for VC interrupt model |
| 0x7e003000 | ST (system timer) | 7 | — | Reference |
| 0x7e006000 | MPHI (host interface) | 28 | "mphi" | **CRITICAL** — ARM↔VC communication |
| 0x7e007000 | DMA0 (channel 0) | 9 | — | Reference for VCHI DMA |
| 0x7e100000 | PM (power manager) | 34 | "pm" | Power sequencing |
| 0x7e101000 | CM (clock manager) | 99 | "cm" | Clock configuration |
| 0x7e200000 | GPIO | 48 | "gpio" | VC-side GPIO control |
| 0x7e205000 | I2C0 | 8 | — | Sensor I2C (tabasco1 vtable) |
| 0x7e209000 | DSI0 (display serial) | 29 | "dsi" | Display output path |
| 0x7e20f000 | OTP | 10 | "otp" | Factory calibration |
| 0x7e400000 | HVS (video scaler) | 36 | "ddrv" | Display composition |
| 0x7e800000 | CAM0 (camera IF 0) | 49 | "ucam" | **HIGH** — camera input pipeline |
| 0x7e801000 | CAM1 (camera IF 1) | 49 | "ucam" | **HIGH** — second camera port |
| 0x7e801000 | CCP (receiver) | 27 | "ccp2" | **HIGH** — CCP2 receive side |
| 0x7e802000 | CSI2 (MIPI receiver) | 37 | "CSI2" | **HIGH** — CSI-2 camera input |
| 0x7e804000 | I2C1 | 8 | — | Reference |
| 0x7e805000 | I2C2 | 8 | — | Reference |
| 0x7e808000 | HDMI | 16 | "HDMI" | HDMI audio/video output |
| 0x7ea00000 | ISP | 1 (stub) | "isp" | Stub only — ISP regs not documented |
| 0x7ee00000 | SDRAM controller | 73 | "SDCO" | Memory timing reference |
| 0x7f000000 | H264 / VCODEC | 1 (stub) | "h264" | Full codec register map in vcodec.h |

### Notes on ISP/H264 stubs
The ISP (0x7ea00000) and H264 (0x7f000000) headers contain only the base address
and APB ID — no register definitions. The ISP registers are likely proprietary and
not included in the leaked header set. However:
- `vcodec.h` contains the **full H264/MPEG/VC-1 hardware register map** (793 lines)
  including deblocking, motion compensation, DMA, symbol decode, and CABAC registers
- The vcodec base at 0x7f000000 matches the H264 base — they're the same block

---

## MPHI Host Interface Registers (Critical for BCM2727)

The MPHI block at 0x7e006000 is the **host processor interface** — this is how
the ARM communicates with the VideoCore. On BCM2727, the equivalent is the
MeSSI-16 + CCP2 transport used by DVCDriver.

Key MPHI registers (from `mphi.h`, 28 registers):

| Offset | Register | Description |
|--------|----------|-------------|
| 0x000 | C0INDDA | Channel 0 inbound data DMA address |
| 0x004 | C0INDDB | Ch0 inbound control (MORUN, MENDINT, LEN) |
| 0x008 | C0OUTDDA | Ch0 outbound data DMA address |
| 0x00C | C0OUTDDB | Ch0 outbound control (MORUN, MENDINT, LEN) |
| 0x010 | C0INDDC | Ch0 inbound DMA count |
| 0x014 | C0OUTDDC | Ch0 outbound DMA count |
| 0x018 | C1INDDA | Channel 1 inbound data DMA address |
| ... | ... | (mirrored for channel 1) |
| 0x050 | INTSTAT | Interrupt status (per-channel) |
| 0x054 | INTCFG | Interrupt configuration |
| 0x058 | INTMSET | Interrupt mask set |
| 0x05C | INTMCLR | Interrupt mask clear |
| 0x060 | CTRL | Global control (ENABLE, RESET) |
| 0x080 | HCW | Host control word |
| 0x084 | OUTDDA | Output DMA direct access |
| 0x088 | OUTDDB | Output DMA direct access B |
| 0x090 | SWAP | Byte swap control |
| 0x0F0 | DEBUG | Debug registers |

### MPHI Protocol Model
- Two DMA channels (0, 1) for inbound and outbound data
- Each channel has: DMA address, control word (length + flags), DMA count
- MENDINT bit = "message end interrupt" — signals transfer completion
- MORUN bit = "message overrun" — error condition
- This maps to VCHI's bulk message transport

---

## CCP2TX Transmitter Registers

CCP2TX at 0x7e001000 — the CCP2 transmitter used for camera data output.
On BCM2727, CCP2 is the link from OMAP3630 (host) to BCM2727 (device).

Key registers (from `ccp2tx.h`):

| Offset | Register | Key Fields |
|--------|----------|------------|
| 0x000 | TC | SWR (reset), TIP (inter-packet), CLKM, MEN, TEN |
| 0x004 | TS | TQI/TEI/TII (interrupts), TQL, TFP/TFF/TFE, TUE, ARE, IEB, TXB |
| 0x008 | TAC | CTATADJ/PTATADJ (PHY trim), CLAC/DLAC, TPC, BPD, APD, ARST |
| 0x00C | TPC | TPT (threshold), TPP, TNP |
| 0x010 | TSC | TSM (speed mode, 4 bits) |
| 0x014 | TIC | TQIT (interrupt threshold), TQIE/TEIE/TIIE |
| 0x018 | TTC | ATX, BI, FSP, LEC, LSC, LCN |
| 0x01C | TBA | Base address (30-bit) |
| 0x020 | TDL | Data length (30-bit) |
| 0x024 | TD | IES, TCS (channel select, 5-bit) |

### CCP2 Protocol Mapping
- TBA + TDL = DMA descriptor (base + length) for bulk data transfer
- TEN = transmit enable, MEN = master enable
- TSM = speed mode (selects LVDS bit rate)
- This is how DVCDriver sends firmware to BCM2727: set TBA → firmware address,
  TDL → firmware length, TEN=1 → hardware streams data over CCP2

---

## Video Codec Hardware Register Map

From `vcodec.h` (793 lines) — the most complete hardware block documented.
Base: 0x7f000000. Access via `RegRd(addr)` / `RegWt(addr, value)`.

### Codec Status (0x110)
```
STATUS_MOCOMP_RDY  (bit 2)   — Motion compensation ready
STATUS_MOCOMP_DONE (bit 3)   — Motion compensation done
STATUS_SPAT_RDY    (bit 4)   — Spatial prediction ready
STATUS_SPAT_DONE   (bit 5)   — Spatial prediction done
STATUS_XFM_RDY     (bit 6)   — Transform ready
STATUS_XFM_DONE    (bit 7)   — Transform done
STATUS_RECON_RDY   (bit 8)   — Reconstruction ready
STATUS_RECON_DONE  (bit 9)   — Reconstruction done
STATUS_DBLK_RDY    (bit 10)  — Deblocking ready
STATUS_DBLK_DONE   (bit 11)  — Deblocking done
STATUS_RESET       (bit 16)  — Reset
STATUS_FLUSHCTX    (bit 17)  — Flush context
```

### Hardware Pipeline Stages
1. **Symbol Interpreter** (0xC00-0xCFF) — Bitstream parsing
   - GET_N_BITS, GET_EXP_GOL, GET_H264_INFO/COEF, GET_SPATIAL
   - CABAC + CAVLC hardware support
   - VC-1 decode tables
2. **Motion Compensation** (0x300-0x31F) — Prediction from reference frames
3. **Spatial Prediction** (0x320-0x32F) — Intra prediction modes
4. **Inverse Transform** (0x700-0x70F) — IDCT/hadamard
5. **Deblocking Filter** (0x720-0x73F) — Loop filter with QP control
6. **DMA Engine** (0x1800-0x184F) — 4 channels, SDRAM↔local memory
7. **Host Mailbox** (0xF00-0xF0F) — CPU↔codec communication
8. **Vector Generator** (0xC30-0xCFF) — Motion vector computation

### Cross-Reference with BCM2727 VLLs
| vcodec.h Register | VLL Export (Task 048) | Match? |
|---|---|---|
| RegRd/RegWt | RegRdSim/RegWtSim (vchwdec.vll) | **YES** — same API, "Sim" suffix for VLL |
| STATUS_DBLK_* | DBLK_CTL_* (vchwdec.vll) | **YES** — same bit definitions |
| DMA0-3 channels | Dma_Read/Write, DmaMemCopy (vchwdec.vll) | **YES** — same 4-channel model |
| HOST2CPU_MBX | vc_ReadMailbox (vchwdec.vll) | **YES** — inter-processor mailbox |
| CABAC/CAVLC tables | C2BCmdWrite, CABAC functions | **YES** — same hardware block |
| Motion compensation | CME grab/start/wait/release | **Partial** — CME is encoder-side |

---

## CAM1 Camera Interface Registers

CAM1 at 0x7e801000 — camera capture interface (49 registers).
This is likely the "tabasco" ISP front-end referenced in BCM2727 VLLs.

| Offset | Register | Purpose |
|--------|----------|---------|
| 0x000 | CAMCTL | Camera control (enable, mode) |
| 0x004 | CAMSTA | Camera status |
| 0x008 | CAMANA | Analog control (reset 0x777) |
| 0x00C | CAMPRI | Priority |
| 0x010 | CAMCLK | Clock control |
| 0x014 | CAMCLT | Clock timing |
| 0x018-024 | CAMDAT0-3 | Data lane configuration |
| 0x028 | CAMDLT | Data lane timing |
| 0x02C-030 | CAMCMP0-1 | Comparators |
| 0x034-038 | CAMCAP0-1 | Capture control |
| 0x100 | CAMICTL | Image capture control |
| 0x104 | CAMISTA | Image interrupt status |
| 0x108 | CAMIDI0 | Image data identifier 0 |
| 0x10C | CAMIPIPE | Image pipeline control |
| 0x110-114 | CAMIBSA0/EA0 | Image buffer start/end addr |
| 0x118 | CAMIBLS | Image buffer line stride |
| 0x11C | CAMIBWP | Image buffer write pointer |
| 0x120-124 | CAMIHWIN/STA | Horizontal window |
| 0x128-12C | CAMIVWIN/STA | Vertical window |
| 0x130 | CAMICC | Image crop control |
| 0x134 | CAMICS | Image crop status |
| 0x138 | CAMIDC | Image data control |
| 0x13C | CAMIDPO | Image data packing order |
| 0x140-144 | CAMIDCA/CD | Image DMA channel A/data |
| 0x148 | CAMIDS | Image DMA status |
| 0x200 | CAMDCS | Data capture start |
| 0x204-208 | CAMDBSA0/EA0 | Data buffer start/end |
| 0x20C | CAMDBWP | Data buffer write pointer |
| 0x300 | CAMDBCTL | Data buffer control |
| 0x400 | CAMMISC | Miscellaneous |

---

## CSI-2 Receiver Registers

CSI2 at 0x7e802000 — MIPI CSI-2 camera serial interface (37 registers).

Key registers:
- `CS_RC` (0x000): PHY control, lane enables (LEN1-4), sync, F16B mode
- `CS_RS` (0x004): Status (IS0/IS1 interrupts, overflow)
- Per-channel control: RC0/RS0/RSA0/REA0/RWP0/RBC0 (ch0 at 0x100-0x128)
- Channel 1 at 0x200-0x228 (same layout)

This is the MIPI CSI-2 receiver — on BCM2727, the CSI-2 input comes from
the camera sensor (tcm8590md) via the `csi2_get_func_table()` vtable.

---

## RPi start.elf Analysis

### ELF Structure
- **Machine:** EM_VIDEOCORE3 (137) — same as BCM2727 VLLs
- **Type:** ET_EXEC (monolithic, not shared library)
- **Entry:** 0xcec00200 → `.crypto` section (cached IO region)
- **Stripped:** No .symtab — all symbols removed in production
- **.text:** 2,541,208 bytes (2.4MB of VPU code)
- **.rdata:** 258,832 bytes (string constants, tables)
- **.bss:** 4,248,028 bytes (~4MB runtime data)

### Special Sections
| Section | Size | Purpose |
|---------|------|---------|
| .crypto | 6,496 | Boot crypto + entry point (cached IO) |
| .secfns | 168 | Security functions |
| .drivers | 240 | 60 driver registration pointers |
| .drivers_base | 12 | 3 driver base pointers |
| .init.vchiq_services | 36 | 3 VCHIQ service registrations |
| .init_mmal_ril_parameter_table | 3,840 | 160 MMAL camera parameters |
| .init.vc_debug_sym | 1,200 | 100 debug symbol entries |

### Key Strings Found
```
OpenMAX IL Components:
  OMX.broadcom.camera
  OMX.broadcom.video_decode
  OMX.broadcom.video_encode
  OMX.broadcom.image_decode
  OMX.broadcom.image_encode
  OMX.broadcom.audio_decode
  OMX.broadcom.audio_encode

Hardware Function Tables:
  get_isp_func_table          ← SAME pattern as BCM2727 VLLs!

DMA/Hardware Channels:
  SYS_DMA_L2, SYS_DMA_UC
  SYS_CCP2TX                  ← CCP2 transmitter DMA
  SYS_MPHI_RX, SYS_MPHI_TX   ← Host interface DMA
  SYS_H264                    ← H.264 hardware DMA
  VIDEO_CME                   ← Content Motion Estimator

MMAL Camera:
  mmal_ril_ic                  ← MMAL Render Interface Layer
  cameraRIL                    ← Camera RIL string
  camera_auto_detect

RTOS:
  _tx_thread_created_next_offset  ← ThreadX (not Nucleus)
  _tx_thread_name_offset
  SERVICE_CLOSED

Hardware Configuration:
  h264_freq, h264_freq_min
  isp_freq, isp_freq_min, isp_use_vpu0
  display_auto_detect, display_default_lcd
  enable_jtag_gpio
```

### BCM2727 Pattern Cross-Reference

| Pattern | RPi start.elf | BCM2727 VLLs | Match |
|---------|--------------|--------------|-------|
| Function vtables | `get_isp_func_table` | `tabasco1_get_func_table` | **Same pattern** |
| OpenMAX IL | OMX.broadcom.camera/video | camera_ilc, video_*_ilc | **Same API** |
| Codec hardware | SYS_H264, VIDEO_CME | CABAC/CME in h264enc.vll | **Same HW** |
| CCP2 DMA | SYS_CCP2TX | ccp2_get_func_table | **Same HW** |
| MPHI host | SYS_MPHI_RX/TX | (firmware-internal) | Same transport |
| MMAL camera | mmal_ril_ic, cameraRIL | (not in VLLs) | RPi-specific layer |
| RTOS | ThreadX | Nucleus PLUS | **Different** |
| Symbol tables | Stripped | Full (MetaWare) | RPi stripped |

---

## Key Findings

### 1. Register Map Applicability
The BCM2708 register headers are **directly applicable** to BCM2727 analysis:
- Same APB IDs (ASCII identifiers in hardware)
- Same block organization (MPHI, CCP2TX, CAM, CSI2, H264, ISP, HVS)
- BCM2727 is VC3 (older), BCM2708 is VC4 (newer) — register compatibility expected
  for shared IP blocks, but addresses may differ

### 2. MPHI = Host Communication
The MPHI block (0x7e006000, 28 registers) documents the ARM↔VideoCore
communication interface. This is the **most critical reference** for writing
a Linux VCHI driver for BCM2727. On E7, the equivalent transport uses
MeSSI-16 (control) + CCP2 (bulk data), but the VCHI protocol layer above
MPHI should be identical.

### 3. Codec Registers are Complete
`vcodec.h` provides the **full register map** for the H.264/MPEG/VC-1 hardware
codec (0x7f000000). Cross-referencing with BCM2727's `vchwdec.vll` exports confirms:
- `RegRdSim`/`RegWtSim` = simulation wrappers around `RegRd`/`RegWt`
- Same DMA engine (4 channels, SDRAM↔local)
- Same deblocking filter register layout
- Same mailbox mechanism

### 4. ISP Registers NOT Documented
The ISP at 0x7ea00000 has only a stub header (base + APB ID).
The ISP pipeline is **software-defined in VLLs** (17 stages in isp_tuner_brcm.vll),
not in fixed hardware registers. The "tabasco1" vtable provides the register
access layer, but the actual register addresses are inside the firmware.

### 5. VPU Boot Confirmation
The `.crypto` entry point at 0xcec00200 uses the cached IO region (0xC0000000+),
confirming the boot model described in rpi-open-firmware: ROM loads firmware to
L1/L2 cached memory, jumps to it. BCM2727 follows a similar pattern via CCP2.

### 6. `get_isp_func_table` in RPi
The string `get_isp_func_table` appears in RPi start.elf .rdata — this is the
**exact same vtable accessor pattern** found in BCM2727 VLLs. Confirms that
the ISP hardware abstraction is shared across VC3 and VC4 platforms.

---

## Implications for BCM2727 Linux Driver (Task 065)

1. **MPHI register map** → defines the host communication DMA interface.
   Linux driver can use these register definitions for VCHI transport init.

2. **CCP2TX registers** → defines the CCP2 transmit DMA for firmware upload.
   TBA (base addr) + TDL (length) + TEN (enable) = firmware upload sequence.

3. **Codec registers** → complete deblocking, motion compensation, DMA register
   map directly usable for V4L2 M2M codec driver.

4. **Camera registers** → CAM0/CAM1 + CSI2 define the capture pipeline front-end.
   Combined with ISP vtable pattern, this maps the full camera capture path.

5. **RPi VCHI driver** (`drivers/staging/vc04_services/`) uses MPHI registers
   for transport — its register access code can be adapted for BCM2727's
   MeSSI-16/CCP2 transport variant.
