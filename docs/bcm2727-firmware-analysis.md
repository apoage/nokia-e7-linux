# BCM2727 VideoCore III Firmware Analysis
# Nokia E7 Linux Project

> **Note (2026-03-23):** Real HW is BCM2727B1 (ARM1176), not OMAP3630. See docs/critical-cpu-discovery.md

*Generated: 2026-02-28, Task 005*

---

## 1. Executive Summary

The BCM2727 VideoCore III firmware exists in **two flash partitions** (`SOS+IVE3A`
and `SOS+IVE3B`) rather than as a standalone file in the ROFS filesystem. The
ROFS contains the **ARM-side driver stack** (`DVCDriver`, `VCHI`, etc.) and **35
loadable VLL plugins**, but the main VC3 firmware binary is stored in a dedicated
flash region loaded by NLoader and the Symbian kernel's DVCDriver at boot.

**Key findings:**
- Main firmware blob: `SOS+IVE3A` (160KB) + `SOS+IVE3B` (~2MB) in NAND flash
- 35 VLL plugin files in `/Z:/vlls/bcm2727b1/` (ROFS), loaded on-demand by firmware
- ~15 additional VLLs are built into the firmware image (VCHI services, display, etc.)
- RTOS: Nucleus PLUS (not ThreadX as initially assumed)
- Transport: CCP2/MPHI dual-channel to OMAP3630, VCHI protocol layer
- ARM driver: `vcdriver.dll` + `vchi.dll` + `signed_2727b1_v2.dll` (in Symbian ROM image)
- 20 DCC (Device Configuration Certificate) calibration files for two silicon variants
- **CORRECTION (2026-03-01): Firmware IS BB5-ENCRYPTED (entropy 7.99/8.0)**
- SOS+IVE3B not included in firmware update FPSXs (factory-programmed only)
- `signed_2727b1_v2.dll` extracted (1MB) — is ARM HAL driver, NOT VC firmware
- 14 VideoCore DLLs found in Symbian ROM extension table (base 0x01278200)
- Firmware extraction blocked: requires real hardware (UART capture or JTAG dump)

---

## 2. Firmware Location and Structure

### 2.1 Flash Partitions (SOS+IVE3A / SOS+IVE3B)

From the NAND flash TOC (source: `flash-partitions.json`):

| Partition   | Flash Offset | Size      | Description |
|-------------|-------------|-----------|-------------|
| SOS+IVE3A   | 0x12380     | 160 KB    | BCM2727 firmware image A (boot stage) |
| SOS+IVE3B   | 0x0C000     | ~2.0 MB   | BCM2727 firmware image B (main OS + services) |

**SOS+IVE3A** (163,840 bytes): Likely the initial bootloader / second-stage loader
that the ARM sends to the VC3 boot ROM via CCP2.

**SOS+IVE3B** (2,096,465 bytes): The main Nucleus RTOS firmware image containing:
- Core OS (Nucleus PLUS RTOS kernel)
- Built-in VCHI services (display, camera, graphics, TV-out, etc.)
- VLL dynamic loader infrastructure
- ISP pipeline, video codec engines, display controller
- Symbian ROFS path reference: `/Z:/vlls/bcm2727b1/` for external VLLs

Both partitions are:
- **BB5-authenticated** (signature verification at boot)
- **Not encrypted** (unlike GENIO_INIT which is encrypted)
- **Mandatory** (`SOS+` prefix = mandatory OS partition in Nokia TOC convention)

### 2.2 Boot Sequence (ARM -> VC3)

Reconstructed from DVCDriver strings in the ROFS core image:

```
1. NLoader (PRIMAPP) authenticates SOS+IVE3A and SOS+IVE3B via BB5
2. NLoader copies IVE3 images to SDRAM (among other SOS images)
3. Symbian kernel boots, loads DVCDriver extension
4. DVCDriver acquires HAT pin (Host Attention), RUN pin, torch enable pin
5. DVCDriver initializes CCP2 driver and MeSSI interface
6. DVCDriver powers up BCM2727:
   - Activates VAUX, VOUT, VIO power rails via TWL5031 PMIC
   - Releases RUN pin to start VC3 boot ROM
7. DVCDriver sends firmware stage 1 via CCP2 to VC3 boot ROM
   - "DVCDriver::Boot() VideoCore boot ROM not responding (stage 1)"
   - "DVCDriver VideoCore image ID found/not found"
8. DVCDriver sends firmware stage 2 (main image)
   - "DVCDriver::Boot() Failed(%d) to start 2nd stage boot loader"
9. VC3 firmware boots Nucleus RTOS, initializes VCHI
10. DVCHI layer establishes message channels over CCP2/MeSSI
11. VCHI services come online (display, camera, bufman, etc.)
12. VC3 firmware dynamically loads VLLs from /Z:/vlls/bcm2727b1/ via VCHI filesystem
```

Error strings revealing the boot protocol:
```
DVCDriver::Boot() VideoCore boot ROM not responding (stage 1); HAT=%d, iMessiReadDfc.Queued()=%d
DVCDriver::Boot() VideoCore boot ROM not responding (stage 2); HAT=%d, iMessiReadDfc.Queued()=%d
DVCDriver::Boot() Unexpected response from boot ROM %u %u
DVCDriver::Boot() no images returned from DVCExe
DVCDriver::Boot() BootParseMessage failed %d
DVCDriver::Boot() BootParseMessage returned an error (stage 2)
DVCDriver::Boot() Failed(%d) to start 2nd stage boot loader
DVCDriver::Boot() Unexpected response after sending 2nd stage %u %u
DVCDriver::BootParseMessage Unrecognised VideoCore response
```

### 2.3 Firmware Not Extracted Yet

The main firmware blob has **not yet been extracted** from the raw flash image.
The ROFS extraction yielded the filesystem contents (VLLs, DLLs, DCC files) but
the IVE3 partitions sit in the NAND at separate offsets. To extract:

1. Need raw NAND image (in `firmware-re/core/rofs.img` at 142MB, this may contain
   the full flash, or just the ROFS region)
2. Apply TOC offsets to locate SOS+IVE3A at 0x12380 and SOS+IVE3B at 0x0C000
3. The offsets in the TOC JSON may be NAND block addresses rather than byte
   offsets (need to account for MuxOneNAND page/block structure)

---

## 3. ARM-Side Driver Stack

The following Symbian DLLs form the ARM-side BCM2727 driver stack (from ROFS ROM
extension loading table, lines 489058-489092 of strings-rofs-core.txt):

### 3.1 Core Drivers

| DLL                              | Purpose |
|----------------------------------|---------|
| `signed_2727b1_v2.dll`           | **BCM2727 firmware container** - likely embeds or references the VC3 firmware blob for loading |
| `vcdriver.dll`                   | DVCDriver - manages VC3 boot, CCP2/MeSSI transport, power |
| `vchi.dll`                       | VCHI protocol layer - message passing over CCP2 |
| `dvcrmiservice.dll`              | RMI (Remote Method Invocation) service layer |
| `vc_logging.dll`                 | VC trace/logging bridge |
| `vcos.dll`                       | VideoCore OS abstraction layer (ARM side) |
| `vc_exe_registry.dll`            | VC executable registry |

### 3.2 Display Stack

| DLL                              | Purpose |
|----------------------------------|---------|
| `vcdispmanvchi.dll`              | DispmanX VCHI client - display manager over VCHI |
| `vcdispman.dll`                  | Display manager core |
| `vcdispmanupdatehandler.dll`     | Display update handler |
| `vcdispmantypes.dll`             | Display type definitions |
| `dispcommanddrv.dll`             | Display command driver |
| `display_chipset_drv_ive_bc.dll` | IVE-BC display chipset driver |
| `display_tools.dll`              | Display utility functions |
| `display_modules.dll`            | Display module registry |
| `display_updater.dll`            | Display frame updater |
| `r_display_kernel.dll`           | Display kernel extension |
| `r_display.dll`                  | Display resource |

### 3.3 Buffer/Stream Management

| DLL                              | Purpose |
|----------------------------------|---------|
| `vcbufmanx.dll`                  | Buffer manager client (ARM side) |
| `vcstreamio.dll`                 | Stream I/O client |

### 3.4 TV/HDMI Output

| DLL                              | Purpose |
|----------------------------------|---------|
| `tv_out.dll`                     | TV-out driver |
| `tvoutconfig.dll`                | TV-out configuration |
| `tvoutbehaviour.dll`             | TV-out behavior |
| `cec_access.dll` / `.ldd`       | HDMI-CEC control |

### 3.5 Graphics

| DLL                              | Purpose |
|----------------------------------|---------|
| `sgresource.dll`                 | Graphics resource management |
| `sgextension.dll`                | Graphics extension |
| `sgdevice.ldd`                   | Graphics device driver |

### 3.6 Key Source Path

The DVCDriver source path in the firmware strings:
```
Y:/ncp_sw/core7.0/IVE3_Engine/IVE3_rapu_drivers/interface/vcdriver/src/messi/controller_messi.cpp
```

This confirms:
- Nokia Core Platform 7.0 source tree
- IVE3 = the Nokia project name for BCM2727 integration
- Drivers are in `IVE3_Engine/IVE3_rapu_drivers/`
- MeSSI (Message System Interface) is one of the transport channels

---

## 4. VCHI Services

VCHI (VideoCore Host Interface) is the message-passing protocol between the ARM
host and VC3. The firmware registers these services at boot:

### 4.1 Built-in VLL Services (Inside Firmware Image)

These VLLs are embedded in SOS+IVE3B and loaded internally by the VC3 RTOS at
boot, not from the ROFS filesystem:

| VLL Name             | Init Function                   | 4CC Service | Purpose |
|----------------------|---------------------------------|-------------|---------|
| `audiopipe.vll`      | `vchi_audiopipe_init`           | AUDIOPIPE   | Audio pipe (HDMI audio) |
| `bufman.vll`         | `bufman_server_init`            | BUFM_AV     | Buffer manager server |
| `dispcontrol.vll`    | `vchi_dispcontrol_server_init`  | DISPCONT/D  | Display control |
| `endpoint_screen.vll`| `endpoint_screen_init`          | -           | Screen endpoint |
| `gce_rpc.vll`        | `vchi_dispservice_x_init`       | GCE_RPC     | Graphics Composition Engine RPC |
| `host_rmi.vll`       | `vchi_hostrmi_init`             | -           | Remote Method Invocation |
| `khronos.vll`        | `khronos_get_func_table`        | KHRN_S      | OpenGL ES / OpenVG |
| `server_khrn.vll`    | `vchi_egl_gce_server_init`      | -           | EGL/GCE Khronos server |
| `logging_vchi.vll`   | `logging_vchi_listener_init`    | -           | VC logging via VCHI |
| `otpburn.vll`        | `vchi_otpburn_init`             | OTPBURN     | OTP (one-time-programmable) fuse |
| `streamio.vll`       | `vc_streamio_server_init`       | -           | Stream I/O server |
| `tvout.vll`          | `vchi_tvout_server_init`        | -           | TV/HDMI output server |
| `vcam.vll`           | `vchi_vcamservice_server_init`  | -           | Camera service (main) |
| `vcam_shared_vid.vll`| `vcam_shared_vid_get_fns`       | -           | Shared video buffer mgmt |
| `vcamdecode.vll`     | `vcamservice_get_fns`           | -           | Camera decode service |
| `vcamencode.vll`     | `vcamservice_get_fns`           | -           | Camera encode service |
| `vcamera.vll`        | `vcamservice_get_fns`           | -           | Camera driver service |
| `vcamfunc.vll`       | `vcamservice_get_fns`           | -           | Camera functional test |
| `vcamtest.vll`       | `vcamservice_get_fns`           | -           | Camera self-test |
| (filesystem)         | `fsysvchi_server_init`          | -           | Host filesystem bridge |

### 4.2 VCHI Transport Architecture

```
ARM (OMAP3630)                              BCM2727 (VC3)
  vcdriver.dll                                Nucleus RTOS
  vchi.dll         <---- CCP2/MeSSI ---->    VCHI server
  DVCHIService                                DVCHIService
    |                                            |
    +-- BULX (bulk transfer)                     +-- BULX
    +-- Control messages                         +-- Control messages
    +-- Service 4CC routing                      +-- Service 4CC routing
```

Transport channels:
- **MeSSI-16**: Message System Interface (16-bit), primary control channel
- **CCP2**: Camera/Compact Camera Port 2, bulk data transfer (sensor data, framebuffers)
- **MPHI**: Mobile Parallel Host Interface (slave mode on BCM2727)

VCHI message format (from error strings):
- Sync word validation
- CRC per-channel (MeSSI-16 CRC, CCP2 CRC)
- Slot-based receive buffers
- Bulk Auxiliary (BULX) for large data transfers
- Power-aware service tracking

### 4.3 VCHI Callback Types

```
VCHI_CALLBACK_MSG_AVAILABLE
VCHI_CALLBACK_MSG_SENT
VCHI_CALLBACK_MSG_SPACE_AVAILABLE
VCHI_CALLBACK_BULK_RECEIVED
VCHI_CALLBACK_BULK_SENT
```

---

## 5. VLL Plugin Catalog (ROFS: /Z:/vlls/bcm2727b1/)

35 VLL files on the ROFS filesystem, loaded dynamically by the VC3 firmware.
All are **ELF format** (VideoCore III native, e_machine=137), compiled with
**MetaWare Linker v5.6.19** (ARC/VC toolchain).

### 5.1 Camera Sensor Drivers

| VLL File                    | Size    | Purpose |
|-----------------------------|---------|---------|
| `tcm8590md_camera.vll`     | 21,348  | Toshiba TCM8590MD main 8MP sensor driver |
| `tcm8590md_camera_test.vll`| 2,952   | TCM8590MD test/calibration |
| `tcm8590md_tuner.vll`      | 137,356 | TCM8590MD ISP tuner parameters |
| `cmnk8en00f_camera.vll`    | 23,776  | Samsung CMNK8EN00F front sensor driver |
| `cmnk8en00f_camera_test.vll`| 2,952  | CMNK8EN00F test/calibration |
| `cmnk8en00f_tuner.vll`     | 136,564 | CMNK8EN00F ISP tuner parameters |
| `acmemini_camera.vll`      | 7,288   | ACMEmini front EDoF camera driver |
| `acmemini_camera_test.vll` | 2,880   | ACMEmini test/calibration |
| `acmemini_tuner.vll`       | 135,752 | ACMEmini ISP tuner parameters |
| `acmeminist_camera.vll`    | 7,652   | ACMEminiST variant driver |
| `acmeminist_camera_test.vll`| 2,884  | ACMEminiST test/calibration |
| `acmeminist_tuner.vll`     | 135,712 | ACMEminiST ISP tuner parameters |
| `camera_cdi.vll`           | 19,804  | Camera Device Interface |
| `camera_ilc.vll`           | 48,248  | Camera IL Component |

### 5.2 ISP/Image Pipeline

| VLL File                    | Size    | Purpose |
|-----------------------------|---------|---------|
| `isp_tuner_brcm.vll`       | 221,884 | Broadcom ISP tuner (AWB, AGC, AF, color correction, defect pixel, etc.) |
| `depurple_stage.vll`       | 5,280   | Purple fringe reduction |
| `nldn_stage.vll`           | 5,152   | Noise/lens distortion correction |
| `stab_stage.vll`           | 13,876  | Video stabilization |
| `stillcolourdn_stage.vll`  | 5,848   | Still image color denoise |
| `videocolourdn_stage.vll`  | 9,432   | Video color denoise |
| `fx_legacy_stage.vll`      | 4,344   | Legacy effects stage |
| `rmi_sw_stage.vll`         | 22,220  | RMI software stage |
| `write_still_ilc.vll`      | 12,792  | Still image writer IL component |
| `invert_imeffect.vll`      | 2,212   | Invert image effect |

### 5.3 Video Codec

| VLL File                    | Size    | Purpose |
|-----------------------------|---------|---------|
| `h264enc.vll`              | 131,212 | H.264 encoder |
| `h263enc.vll`              | 98,324  | H.263 encoder |
| `mpg4enc.vll`              | 126,344 | MPEG-4 encoder |
| `vchwdec.vll`              | 327,372 | VideoCore hardware decoder (multi-codec) |
| `vp6dec.vll`               | 34,876  | VP6 decoder |
| `video_decode_ilc.vll`     | 16,416  | Video decode IL component |
| `video_encode_ilc.vll`     | 21,364  | Video encode IL component |
| `jpeg.vll`                 | 38,240  | JPEG encoder/decoder |

### 5.4 ArcSoft (3rd Party)

| VLL File                    | Size    | Purpose |
|-----------------------------|---------|---------|
| `arcsoft_ft_stage.vll`     | 389,604 | ArcSoft face tracking stage |
| `arcsoft_rer_stage.vll`    | 183,256 | ArcSoft red-eye reduction stage |

### 5.5 HDMI Audio

| VLL File                    | Size    | Purpose |
|-----------------------------|---------|---------|
| `aplay_hdmi.vll`           | 5,536   | HDMI audio playback |

### 5.6 VLL File Format

ELF characteristics (from firmware VLL loader error strings and binary headers):
```
- Magic: \x7fELF (confirmed by reading binary header)
- e_machine: 137 (VideoCore III)
- Endianness: checked at load time ("has the wrong endian")
- Sections: .rdata, .rsdata, .sdata, .bss_internal
- Relocation: .rela.text, .rela.data, .rela.rdata, .rela.sdata
- Dynamic linking: .dynamic, .dynstr, .dynsym
- Toolchain: MetaWare Linker v5.6.19
```

VLL loader error messages:
```
%s is not a VLL.
%s has the wrong endian.
Can't read ELF header of %s
%s is not a valid ELF file.
%s does not contain %s code.
%s: section header size appears to be garbage.
```

### 5.7 VLL Exported Symbols

Key function table entries that VLLs export:
```
get_camera_tcm8590md       - Main sensor driver entry
get_camera_cmnk8en00f      - Front sensor driver entry
get_camera_func_table      - Generic camera function table
i2c_get_func_table         - I2C bus access functions
ccp2_get_func_table        - CCP2 interface functions
camera_subsystem_get_func_table  - Camera subsystem
pmu_uideck_get_func_table  - PMU power control
platform_gpio_control      - GPIO pin control
```

---

## 6. RTOS Analysis: Nucleus PLUS (Not ThreadX)

The BCM2727 firmware runs **Nucleus PLUS RTOS** (Express Logic), not ThreadX
as initially hypothesized. Evidence:

### 6.1 Nucleus API Symbols

The firmware exports standard Nucleus PLUS kernel API functions:
```
TCC_Create_Task           - Create a task
TCC_Create_HISR           - Create a High-level ISR
TCC_Delete_Task           - Delete a task
TCC_Delete_HISR           - Delete a HISR
TCC_Current_Task_Pointer  - Get current running task
TCC_Relinquish            - Yield CPU
TCC_Resume_Service        - Resume a suspended service
TCC_Task_Sleep            - Sleep current task
TCC_Terminate_Task        - Terminate a task
TCS_Change_Priority       - Change task priority
TCT_Activate_HISR         - Activate HISR from LISR
TCT_In_LISR               - Check if in Low-level ISR
TCF_Task_Affinity_Information - Task core affinity (dual-core)
TCF_Task_Information       - Query task info
SMC_Create_Semaphore      - Create a semaphore
SMC_Delete_Semaphore      - Delete a semaphore
SMC_Obtain_Semaphore      - Acquire semaphore
SMC_Release_Semaphore     - Release semaphore
QUC_Create_Queue          - Create message queue
QUC_Delete_Queue          - Delete queue
QUC_Receive_From_Queue    - Receive from queue
QUC_Send_To_Queue         - Send to queue
QUS_Reset_Queue           - Reset queue
EVC_Create_Event_Group    - Create event group
EVC_Delete_Event_Group    - Delete event group
EVC_Retrieve_Events       - Wait for events
EVC_Set_Events            - Signal events
EVCE_Set_Events           - Signal events (extended)
```

### 6.2 RTOS Source Path

```
../../../../vcfw/rtos/nucleus/rtos_nucleus.c
../../../../interface/vcos/nucleus/vcos_nucleus.c
```

This shows the VCFW (VideoCore Firmware) framework wraps Nucleus via a VCOS
(VideoCore OS) abstraction layer -- the same pattern used in Raspberry Pi
firmware (which uses ThreadX on VC4).

### 6.3 Named Tasks/HISRs

From firmware strings:
```
MPHI HISR0, MPHI HISR1, MPHI HISR2   - MPHI interrupt handlers
Rosberg Thread                         - "Rosberg" subsystem (ISP?)
Rosberg_HISR                           - Rosberg ISR handler
TABASCO1                               - LED/flash controller
TABASCO1_HISR                          - Flash HISR
HDMI HISR                              - HDMI interrupt handler
IIRQHISR                               - Internal IRQ HISR
VCamService_Task                       - Camera service task
VCamFuncTest_Task                      - Camera functional test
DispmanX                               - Display manager task
GCE_RPC                                - Graphics composition task
ISPTASK                                - ISP pipeline task
khronos server stack                   - OpenGL ES server
```

### 6.4 ThreadX Reference

The single `[ThreadX7H4Ch` occurrence at line 424705 of strings-rofs-core.txt
appears to be in a Symbian channel status enumeration (likely `HCI` Bluetooth
related, not BCM2727). The BCM2727 firmware definitively uses Nucleus PLUS.

---

## 7. DCC (Device Configuration Certificates)

20 DCC files in `/Z:/dcc/` on the ROFS, used for BCM2727 ISP calibration:

### 7.1 DCC Naming Convention

```
IVppBCBsssMMMMMMMMNN.dcc
  ^  ^  ^   ^        ^
  |  |  |   |        +-- Sequence number
  |  |  |   +----------- Camera module part number
  |  |  +--------------- BCM2727 silicon variant (07A or 179)
  |  +------------------ "BCB" = BCM Camera Board?
  +---------------------- "IV" = IVE, pp = level/version (00, L2, L4)
```

### 7.2 BCM2727 Silicon Variants

| Variant | DCC Prefix  | Description |
|---------|-------------|-------------|
| 07A     | IV00BCB07A  | First silicon variant (BCM2727B1 rev A?) |
| 179     | IV00BCB179  | Second silicon variant (BCM2727B1 rev 79?) |

### 7.3 DCC File List

**Level 00 (base calibration):**
| File | Size | Camera Module |
|------|------|---------------|
| IV00BCB07A0301022E05.dcc | 25,341 | 0301022E rev 05 |
| IV00BCB07A0301022E07.dcc | 25,341 | 0301022E rev 07 |
| IV00BCB07A0307769802.dcc | 30,906 | 0307769802 |
| IV00BCB07A030C213402.dcc | 31,804 | 030C213402 |
| IV00BCB07A030F769804.dcc | 30,906 | 030F769804 |
| IV00BCB1790301022E05.dcc | 20,152 | 0301022E rev 05 |
| IV00BCB1790301022E07.dcc | 20,152 | 0301022E rev 07 |
| IV00BCB179030C213402.dcc | 20,006 | 030C213402 |

**Level L2 (camera tuning level 2):**
| File | Size | Camera Module |
|------|------|---------------|
| IVL2BCB07A030C218E06.dcc | 41,431 | 030C218E06 |
| IVL2BCB07A030C218E07.dcc | 42,375 | 030C218E07 |
| IVL2BCB07A030C218E08.dcc | 42,383 | 030C218E08 |
| IVL2BCB07A030F008803.dcc | 42,945 | 030F008803 |
| IVL2BCB179030C218E06.dcc | 32,083 | 030C218E06 |
| IVL2BCB179030C218E07.dcc | 32,074 | 030C218E07 |
| IVL2BCB179030C218E08.dcc | 32,074 | 030C218E08 |
| IVL2BCB179030F008803.dcc | 33,739 | 030F008803 |

**Level L4 (variant 179 only):**
| File | Size | Camera Module |
|------|------|---------------|
| IVL4BCB179030C218E06.dcc | 32,082 | 030C218E06 |
| IVL4BCB179030C218E07.dcc | 32,073 | 030C218E07 |
| IVL4BCB179030C218E08.dcc | 32,073 | 030C218E08 |
| IVL4BCB179030F008803.dcc | 33,739 | 030F008803 |

---

## 8. BCM2727 Chip Variants

From firmware strings (line 18426-18429):
```
2727A0    - BCM2727 revision A0 (initial silicon)
2727B0    - BCM2727 revision B0 (first metal spin)
2727B1    - BCM2727 revision B1 (production silicon, used in E7)
2708A0    - BCM2708 revision A0 (different chip -- RPi BCM2835 predecessor?)
```

The Nokia E7 uses **BCM2727B1** (confirmed by VLL directory name `bcm2727b1`
and the firmware DLL name `signed_2727b1_v2.dll`).

---

## 9. Power and Voltage Control

BCM2727 power rails (from DVCDriver init strings):

| Rail | Control | Purpose |
|------|---------|---------|
| VAUX | TWL5031 LDO | Camera analog supply |
| VOUT | TWL5031 programmable | BCM2727 core or I/O supply |
| VIO  | TWL5031 LDO | BCM2727 I/O interface supply |

Power-up sequence (from strings):
```
1. "Failed to activate Vaux"   -> Enable VAUX first
2. "Failed to activate Vout"   -> Then VOUT
3. "Failed to activate Vio"    -> Then VIO
4. "Failed to set Vout voltage" -> Program VOUT to correct level
```

Power-down:
```
1. "Failed to deactivate Vaux"
2. "Failed to deactivate Vout"
3. "Failed to deactivate Vio"
```

GPIO pins used by DVCDriver:
- **HAT pin** (Host Attention) - interrupt from VC3 to ARM
- **RUN pin** - enables/resets BCM2727
- **Torch enable pin** - camera flash LED control

---

## 10. Display Subsystem

The BCM2727 drives the E7's AMOLED panel via DSI:

```
Display path: BCM2727 DispmanX -> DSI Command Mode -> AMOLED panel
              BCM2727 TV-out  -> HDMI encoder -> mini-HDMI connector
```

Key findings:
- **DSI command mode** (not video mode) - `DSI_CMD`, `DSI_TE` (tearing effect sync)
- **IVE_DSI** source: `../../../../applications/ive/display/ive_dsi.c`
- **DispmanX** composition engine handles multi-layer display
- **HDMI** supports: NTSC, PAL, 480p, 576p, 720p, 1080i@50/60, 1080p@24/25/30
- **HDCP** authentication implemented (full state machine with KSV exchange)
- **Display data handles**: DISPDATA, IMGDATA0, IMGDATA1

---

## 11. `signed_2727b1_v2.dll` - The Firmware Container

This Symbian DLL appears in the ROM extension loading table at line 489059 of
the ROFS core strings. It is loaded before `vcdriver.dll` and `vchi.dll`.

Based on the naming convention:
- `signed` = BB5 digital signature applied
- `2727b1` = BCM2727 revision B1
- `v2` = firmware version 2

This DLL likely **contains the BCM2727 firmware binary as an embedded resource
section**, or provides the firmware image to DVCDriver for loading. It is NOT
currently present in the extracted `core-rofs0/sys/bin/` directory -- it must
be in the portions of the ROFS image that were not fully extracted, or in the
raw ROFS binary data.

**This is the primary extraction target.** Recovering this DLL from the raw ROFS
image (`firmware-re/core/rofs0.rofs` or `rofs0_pure.rofs`) would give us the
complete VC3 firmware blob.

---

## 12. Comparison: Nokia E7 BCM2727 vs Raspberry Pi BCM2835

| Aspect | Nokia E7 (BCM2727/VC3) | RPi (BCM2835/VC4) |
|--------|----------------------|---------------------|
| VC generation | VideoCore III | VideoCore IV |
| RTOS | Nucleus PLUS | ThreadX |
| Firmware container | `signed_2727b1_v2.dll` | `start.elf` |
| Firmware size | ~2.2 MB (IVE3A+B) | ~2.5 MB |
| VLL plugins | 35 files (~2.4 MB total) | N/A (monolithic) |
| Host interface | CCP2/MeSSI/MPHI | Mailbox/VCHIQ |
| Boot direction | ARM loads -> VC | VC loads -> ARM |
| VCHI transport | CCP2 serial link | Shared memory |
| Display | DSI command mode | HDMI/DSI |
| Camera ISP | Built-in (BCM2727) | Separate (ISP on SoC) |
| 3D graphics | OpenGL ES 1.1/2.0 | OpenGL ES 2.0 |

---

## 13. Next Steps

### 13.1 Critical Path: Extract Firmware Blob

1. **Method A: Extract from raw ROFS image**
   - Parse `firmware-re/core/rofs0_pure.rofs` (97 MB) to find `signed_2727b1_v2.dll`
   - Or parse the raw `rofs.img` which may contain the full flash layout
   - The file is in the ROM extension table so it is definitely in the ROFS

2. **Method B: Extract SOS+IVE3 from NAND image**
   - If a raw NAND dump exists, extract at TOC offsets
   - Need to handle MuxOneNAND block/page alignment

3. **Method C: Look in binwalk-extracted artifacts**
   - The `core/_rofs.img.extracted/` directory has many extracted blobs
   - Two YAFFS images (~87 MB each) and numerous zlib-compressed segments
   - The firmware may be in one of the YAFFS images

### 13.2 Analysis Once Extracted

1. Identify ELF header (e_machine=137 for VideoCore III)
2. Determine load address and entry point
3. Map memory layout (code, data, BSS, VLL loader region, VCHI buffers)
4. Catalog all built-in VCHI services
5. Identify VLL filesystem client (how it reads from host ROFS)

### 13.3 Linux Driver Implications

For Linux on the Nokia E7, the BCM2727 requires:
- A kernel driver equivalent to DVCDriver (CCP2/MeSSI transport)
- VCHI message layer (similar to `drivers/staging/vc04_services/` in mainline Linux)
- The same firmware blob loaded via request_firmware()
- DCC calibration data loaded for camera ISP tuning
- Power management via TWL5031 PMIC

The Raspberry Pi's VCHI/VCHIQ driver in Linux (`drivers/staging/vc04_services/`)
is architecturally similar and could serve as a reference, though the transport
layer differs (shared memory on RPi vs CCP2/MeSSI serial link on Nokia E7).

---

## 14. Source File References

Source paths embedded in the firmware (from strings analysis):

```
../../../../interface/vchi/message_drivers/videocore/mphi_ccp2.c
../../../../applications/vmcs/vchi/otpburn.c
../../../../applications/vmcs/vchi/gce_rpc/resource_handlers.c
../../../../applications/vmcs/vchi/tvout.c
../../../../applications/ive/display/ive_dsi.c
../../../../applications/ive/vcam/vcamservice_vchi.c
../../../../applications/ive/vcam/vcamdecode.c
../../../../applications/ive/vcam/vcamencode.c
../../../../applications/ive/functionaltest/vcamfunc.c
../../../../applications/ive/selftest/vcamtest.c
../../../../vcfw/rtos/nucleus/rtos_nucleus.c
../../../../interface/vcos/nucleus/vcos_nucleus.c
../../../../helpers/vcsuspend/vcsuspend.c
../../../../middleware/vec/vec.c
../../../../middleware/ISP/tuner/isp_tuner_brcm_awb.c
Y:/ncp_sw/core7.0/IVE3_Engine/IVE3_rapu_drivers/interface/vcdriver/src/messi/controller_messi.cpp
```

---

## Appendix A: Key String Locations in strings-rofs-core.txt

| Line Range   | Content |
|-------------|---------|
| 9-10        | SOS+IVE3A, SOS+IVE3B in NLoader TOC |
| 519-524     | IVE3 boot status messages |
| 12260-12290 | DSI display, IVE_DSI, MPHI slave |
| 15887       | CCP2 TX |
| 16310-16340 | FSysVchi, CCP2, ISP, tabasco |
| 17100-17250 | MPHI, VLL loader, GCE_RPC |
| 17290-17310 | Nucleus RTOS source path |
| 17500-17570 | LED flash (tabasco1), HDMI |
| 17730-17830 | VCam service, VCOS, OpenVG |
| 18340-18430 | VLL list, camera sensors, chip revisions |
| 18690-18970 | Exported symbol table, VLL init functions |
| 19000-19100 | DVCDriver boot, CCP2 init, power rails |
| 19100-19280 | VCHI protocol, DispmanX, buffer manager |
| 350815      | "VideoCore III" identifier |
| 424705      | ThreadX reference (Bluetooth, not BCM2727) |
| 489058-489092| ROM extension DLL loading order |
