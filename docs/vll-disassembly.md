# VLL Plugin Disassembly — VideoCore III (BCM2727)

*Task 048 Phase 2 — 2026-03-03*
*Disassembly using custom VC3/VC4 VPU scalar decoder*

---

## 1. Toolchain

Custom Python disassembler (`/tmp/vc3dis.py`) built from hermanhermitage's
VideoCore IV ISA documentation. The VC3 and VC4 share the same VPU scalar
instruction set (confirmed: `e_machine = EM_VIDEOCORE3 = 137`).

Handles 16/32/48-bit variable-length instructions. Some 48-bit vector
instructions decode as raw data — these are Vec16 SIMD operations unique
to VC3 (no public documentation exists).

All 35 VLLs have full symbol tables (MetaWare Linker v5.6.19 preserved them).
This gives us function boundaries, names, imports, and exports without
needing perfect instruction decode.

---

## 2. VPU Calling Convention (Confirmed by Disassembly)

```
r0-r5     Arguments / scratch (caller-saved)
r6-r11    Callee-saved
r24       Global pointer (gp) — used by ld/st (r24+offset)
r25       Frame pointer (optional)
r26       Link register (lr)
sp        Stack pointer (alias for r30 on VC3/VC4?)

Prologue:  stm r6-rN, lr, (--sp)    ; N = highest callee-saved used
Epilogue:  ldm r6-rN, pc, (sp++)    ; pops saved regs + return
           OR: ret  (b r26)         ; leaf function, no stack frame
```

Function arguments in r0-r5 (up to 6 register args), return value in r0.
The `addcmpb` instruction is used heavily for loop control — it adds,
compares, and conditionally branches in a single instruction (unique to VC).

---

## 3. VLL Inventory (35 files, 2.4 MB total)

### 3.1 Summary Table

| Category | VLL | .text | Exports | Imports | Key Feature |
|----------|-----|------:|--------:|--------:|-------------|
| **Camera CDI** | camera_cdi.vll | 13,456 | 5 | 44 | Master camera controller, VLL loader |
| **Camera ILC** | camera_ilc.vll | 38,568 | 6 | 78 | OpenMAX IL camera component, DCC |
| **ISP Tuner** | isp_tuner_brcm.vll | 106,892 | 212 | 116 | 17-stage ISP pipeline, Bayesian AWB |
| **HW Decoder** | vchwdec.vll | 254,882 | 781 | 66 | Multi-codec: H.264/H.263/AVS/VC-1/MPEG |
| **H.264 Enc** | h264enc.vll | 113,808 | 96 | 41 | CME + CABAC hardware, rate control |
| **MPEG-4 Enc** | mpg4enc.vll | 108,948 | 82 | 38 | MPEG-4 ASP encoder |
| **H.263 Enc** | h263enc.vll | 82,112 | 70 | 37 | H.263 baseline encoder |
| **VP6 Dec** | vp6dec.vll | 23,996 | 7 | 14 | On2 VP6 software decoder |
| **JPEG** | jpeg.vll | 22,804 | 6 | 25 | HW zigzag + quantization |
| **Face Track** | arcsoft_ft_stage.vll | 85,896 | 85 | 35 | ArcSoft face detection (Haar cascade) |
| **Red-Eye** | arcsoft_rer_stage.vll | 123,928 | 321 | 14 | ArcSoft red-eye removal |
| **Stabilization** | stab_stage.vll | 10,062 | 5 | 8 | Video stabilization |
| **Depurple** | depurple_stage.vll | 2,264 | 2 | 5 | Chromatic aberration correction |
| **Still Denoise** | stillcolourdn_stage.vll | 2,976 | 2 | 5 | Still image color denoise |
| **Video Denoise** | videocolourdn_stage.vll | 6,174 | 2 | 5 | Video color denoise |
| **NLDN** | nldn_stage.vll | 2,362 | 2 | 3 | Noise-level-dependent noise reduction |
| **Invert** | invert_imeffect.vll | 306 | 3 | 3 | Image inversion (YUV byte XOR) |
| **Legacy FX** | fx_legacy_stage.vll | 1,306 | 2 | 8 | Legacy image effects |
| **RMI SW** | rmi_sw_stage.vll | 12,896 | 2 | 22 | Remote Metering Interface (exposure) |
| **HDMI Audio** | aplay_hdmi.vll | 2,386 | 4 | 17 | SPDIF format + DMA audio |
| **Video Dec ILC** | video_decode_ilc.vll | 9,584 | 3 | 27 | OpenMAX IL video decode wrapper |
| **Video Enc ILC** | video_encode_ilc.vll | 13,628 | 3 | 33 | OpenMAX IL video encode wrapper |
| **Still Writer** | write_still_ilc.vll | 7,396 | 5 | 27 | Still capture IL component |
| **TCM8590 Cam** | tcm8590md_camera.vll | 8,676 | 4 | 10 | Toshiba main sensor driver, OTP |
| **TCM8590 Tuner** | tcm8590md_tuner.vll | 842 | 2 | 15 | Main sensor ISP tuning params |
| **TCM8590 Test** | tcm8590md_camera_test.vll | 624 | 2 | 7 | Factory self-test |
| **CMNK8 Cam** | cmnk8en00f_camera.vll | 11,060 | 4 | 11 | CMN sensor driver |
| **CMNK8 Tuner** | cmnk8en00f_tuner.vll | 842 | 2 | 15 | CMN sensor ISP tuning |
| **CMNK8 Test** | cmnk8en00f_camera_test.vll | 624 | 2 | 7 | Factory self-test |
| **ACMEmini Cam** | acmemini_camera.vll | 3,918 | 2 | 8 | Front camera driver |
| **ACMEmini Tuner** | acmemini_tuner.vll | 838 | 2 | 15 | Front camera ISP tuning |
| **ACMEmini Test** | acmemini_camera_test.vll | 624 | 2 | 7 | Factory self-test |
| **ACMEminist Cam** | acmeminist_camera.vll | 3,706 | 2 | 9 | Alt front camera driver |
| **ACMEminist Tuner** | acmeminist_tuner.vll | 842 | 2 | 15 | Alt front camera ISP tuning |
| **ACMEminist Test** | acmeminist_camera_test.vll | 624 | 2 | 7 | Factory self-test |

### 3.2 Plugin Registration Pattern

Every VLL exports a pair of entry points:
```
get_<type>_func_table        ; weak symbol (generic name)
get_<specific>_func_table    ; global symbol (specific name)
```
Both point to the same address. The function returns a pointer to a vtable
struct (8+ bytes), loaded by the CDI or ILC layer via `dlsym`.

Examples:
- `get_cdi_func_table` = `get_cdi_camera` (camera_cdi.vll)
- `get_vid_enc_func_table` = `get_h264_enc_func_table` (h264enc.vll)
- `get_isp_tuner_func_table` = `get_isp_tuner_brcm` (isp_tuner_brcm.vll)
- `get_camera_func_table` = `get_camera_tcm8590md` (tcm8590md_camera.vll)
- `get_sw_func_table` = `get_arcsoft_facetrack_interface` (arcsoft_ft_stage.vll)

---

## 4. Hardware Architecture (from Import/Export Analysis)

### 4.1 ISP Hardware — "Tabasco1"

The ISP hardware block is codenamed **tabasco1**. Access is via
`tabasco1_get_func_table()` which returns a vtable of register access
functions. This is the same pattern used for all hardware blocks.

Related APIs:
- `tu_get_func_table` — tuner utility hardware interface
- `systimer_get_func_table` — hardware timer
- `intctrl_get_func_table` — interrupt controller
- `ccp2_get_func_table` — CCP2 camera serial interface
- `csi2_get_func_table` — CSI-2 (MIPI) camera interface
- `cpi_get_func_table` — Camera Parallel Interface

### 4.2 CABAC Hardware (H.264)

The H.264 encoder uses a dedicated CABAC (Context-Adaptive Binary Arithmetic
Coding) hardware accelerator:

```
C2BCmdQueueCheck    — Check if HW command queue has space
C2BCmdWrite         — Write a CABAC command to hardware
C2B_bits_space_available — Query bitstream buffer space
cabac_convert       — Start CABAC conversion
cabac_deferred_queue_issue_pending_commands
cabac_deferred_queue_reset
cabac_init          — Initialize CABAC engine state
```

The "C2B" prefix = CABAC-to-Bitstream hardware pipeline.

### 4.3 CME Hardware (Motion Estimation)

The Content Motion Estimator is a dedicated hardware block:

```
cme_action          — Execute motion estimation operation
cme_start_frame     — Initialize CME for new frame
cme_wait            — Wait for CME completion
codec_grab_CME      — Acquire exclusive CME access
codec_release_CME   — Release CME
```

Used by all three encoders (H.264, MPEG-4, H.263). The CME searches for
motion vectors — this is the most computationally expensive part of video
encoding, hence dedicated hardware.

### 4.4 Deblocking Filter (H.264 Decode)

The hardware decoder has extensive deblocking support:
```
RDblkCtlDefault     — Default deblocking control
RDblkCtlTopRow      — Top-row specific deblocking
RDblkCtl            — Current deblocking control register
Rdeblockparam       — Deblocking parameters
RDeblockQP          — Deblocking quantization parameter
```

### 4.5 DMA Engine

Multiple VLLs use the DMA subsystem:
```
dma_callback_status — Check DMA completion
dma_post_callback   — Register completion callback
dma_status_wait     — Block until DMA complete
dma_unset_callback  — Remove DMA callback
```

2D DMA is used for block transfers between local SRAM and stacked SDRAM.

### 4.6 Vector Register File (VRF)

The Vec16 SIMD unit is managed via:
```
vclib_obtain_VRF    — Lock VRF for exclusive use (prevents preemption)
vclib_release_VRF   — Release VRF
vclib_check_VRF     — Verify VRF state
vc_InitVRF          — Initialize VRF state
vc_InitVRF_H264     — H.264-specific VRF setup
```

VRF locking is critical because the vector register file is shared between
tasks — preemption while using VRF would corrupt another task's SIMD state.

---

## 5. ISP Pipeline (isp_tuner_brcm.vll — 212 exports)

The ISP tuner implements a 17-stage pipeline. Each stage follows a uniform
interface pattern with 7 entry points:

```c
// For each stage X:
isp_tuner_brcm_X_open(...)           // Allocate resources
isp_tuner_brcm_X_close(...)          // Free resources
isp_tuner_brcm_X_get_ctrl(...)       // Read current parameters
isp_tuner_brcm_X_set_ctrl(...)       // Write new parameters
isp_tuner_brcm_X_pre_update(...)     // Pre-frame processing
isp_tuner_brcm_X_post_update(...)    // Post-frame processing
isp_tuner_brcm_X_switch_mode(...)    // Mode change (video↔still)
```

### 5.1 Pipeline Stages (in processing order)

| Stage | Largest Function | Purpose |
|-------|-----------------|---------|
| agc | pre_update (1666B) | Auto gain/exposure control |
| awb | post_update (492B) | Auto white balance (Bayesian) |
| black_level | pre_update (848B) | Black level subtraction |
| lens_shading | pre_update (712B) | Lens vignetting correction |
| defpix | pre_update (248B) | Defective pixel correction |
| denoise | pre_update (570B) | Bayer domain noise reduction |
| sharpen | pre_update (228B) | Edge enhancement |
| chrom | pre_update (564B) | Chrominance processing |
| correction | pre_update (1074B) | Color correction matrix |
| contrast | pre_update (390B) | Tone mapping |
| custom | pre_update (128B) | Saturation/UV adjustment |
| distortion | pre_update (92B) | Lens distortion correction |
| motion_detection | post_update (144B) | Scene change detection |
| focus | pre_update (1530B) | Continuous auto-focus |
| depurple | pre_update (206B) | Purple fringing removal |
| statistics | pre_update (374B) | 3A statistics collection |
| debug | pre_update (108B) | Debug/diagnostic output |

### 5.2 DPF (Dynamic Parameter File) System

The tuner reads calibration from DCC files via typed accessors:

```
isp_tuner_brcm_dpf_read_int32           — 32-bit integer
isp_tuner_brcm_dpf_read_float           — IEEE 754 float
isp_tuner_brcm_dpf_read_sint10p6        — Signed 10.6 fixed-point
isp_tuner_brcm_dpf_read_sint24p8        — Signed 24.8 fixed-point
isp_tuner_brcm_dpf_read_uint16p16       — Unsigned 16.16 fixed-point
isp_tuner_brcm_dpf_read_uint0p16        — Unsigned 0.16 fraction
isp_tuner_brcm_dpf_read_pwl_func        — Piecewise-linear curve
isp_tuner_brcm_dpf_read_enum            — Enumerated value
isp_tuner_brcm_dpf_read_bitfield        — Bitfield
isp_tuner_brcm_dpf_read_syntax          — Structured syntax block
```

The piecewise-linear (PWL) function library is extensive:
```
isp_pwl_create, isp_pwl_copy, isp_pwl_compose
isp_pwl_evaluate, isp_pwl_evaluate_ext
isp_pwl_domain, isp_pwl_intersect
isp_pwl_max, isp_pwl_min, isp_pwl_maxval
isp_pwl_read_us, isp_pwl_write_us       — Unsigned short format
isp_pwl_read_sl_us, isp_pwl_write_sl_us — Signed long + unsigned short
isp_pwl_simplify, isp_pwl_validate
```

PWL functions map one continuous variable to another (e.g., exposure →
gain, color temperature → white balance correction). The DCC calibration
files encode per-sensor PWL curves.

### 5.3 Math Library Usage

The ISP tuner imports trig and math functions:
```
sin, cos, tan, acos, atan    — Lens distortion, rotation
sqrt                          — Various calculations
exp, exp_fast                 — Exposure computations
```

These are firmware-provided soft-float functions running on the VPU scalar
core (no FPU on VC3).

---

## 6. Camera Driver Interface (camera_cdi.vll)

### 6.1 Architecture

CDI is the master camera controller. It:
1. Loads sensor-specific VLLs via `dlopen`/`dlsym`
2. Loads ISP tuner VLLs
3. Manages camera state machine
4. Handles interrupt-driven capture flow (HISR/LISR model)
5. Controls LED (flash/torch) via `generic_led_get_func_table`

### 6.2 State Machine

The main export `cdi_camera_change_state` (1064 bytes) implements:
```
IDLE → VIEWFINDER → CAPTURE → IDLE
                  → RECORD  → IDLE
```

Uses Nucleus PLUS event groups (`EVC_*`) for synchronization:
- Create event group for camera state changes
- Set events to signal state transitions
- Retrieve events with timeout for blocking waits

### 6.3 Sensor Interface Hierarchy

```
camera_cdi.vll
    ├── dlopen("tcm8590md_camera.vll")    — Toshiba main sensor
    │       └── i2c_get_func_table()      — I2C register access
    │       └── pmu_uideck_get_func_table() — Power management
    │       └── read_nvm_reg_series()     — OTP calibration data
    │       └── tcm8590md_read_ls_table() — Lens shading table
    │
    ├── dlopen("acmemini_camera.vll")     — Front camera
    │
    ├── dlopen("isp_tuner_brcm.vll")     — ISP tuning
    │
    └── camera_subsystem_get_func_table() — Firmware camera core
```

### 6.4 Dynamic Loading

```c
handle = dlopen("sensor_name_camera.vll", 0);
func   = dlsym(handle, "get_camera_func_table");
vtable = func();
dldone(handle);  // signal that symbol lookup is done
// ... use vtable ...
dlclose(handle); // on shutdown
```

This is a standard POSIX-like dynamic linking API running on the VC3 VPU,
implemented by the Nucleus PLUS firmware's VLL loader.

---

## 7. Video Codec Hardware (vchwdec.vll + h264enc.vll)

### 7.1 Hardware Decoder — Multi-Codec Engine

vchwdec.vll (255KB) is the largest VLL, supporting 6+ codecs:

**Codec modules (confirmed by export prefixes):**
- `AVS_*` — Audio Video Standard (Chinese H.264 variant)
- `H264_*` — H.264/AVC baseline/main/high
- `H263_*` — H.263 baseline
- `MPEG2_*` — MPEG-1/2
- `MPEG4_*` — MPEG-4 Visual
- `Sorenson_*` — Sorenson Spark (Flash video)
- `VC1_*` — VC-1/WMV9

Each codec has a standardized interface:
```
XXX_Activate        — Initialize decoder state
XXX_Deactivate      — Clean up
XXX_DecodePicture   — Decode one picture
XXX_Init_LocalPictureCntx — Set up per-picture context
XXX_MBDecode_*      — Macroblock-level decoding
XXX_ParseSequence   — Parse sequence header
XXX_SliceLoop       — Main decode loop
XXX_StartChannel    — Open decode channel
XXX_DPB_Flush       — Flush decoded picture buffer
XXX_ResetToRAP      — Seek to random access point
```

### 7.2 Register State Model (H.264 Decode)

The H.264 decoder exports 100+ VPU register-file addresses (R-prefix):
```
RMBPosX, RMBPosY        — Current macroblock position
RMBType, RMBCtl          — MB type and control
Rqp                      — Quantization parameter
RSpatialAvail             — Spatial prediction availability
RDblkCtl                  — Deblocking control
RVecAX/Y, RVecBX/Y       — Motion vectors A and B
RColVecX/Y                — Co-located vectors (B-frames)
RRefId, RPicId            — Reference picture IDs
RSkipCount                — Skip run length
Rfinished                 — Decode completion flag
```

These are essentially hardware register definitions exposed as ELF symbols
for the VPU code to reference. The dense packing (4-byte stride from
0x100000) suggests they map to the VPU's data scratchpad or hardware
register file.

### 7.3 H.264 Encoder Pipeline

```
venc_alloc                           — Allocate encoder context
venc_init                            — Initialize encoder state
venc_firstframe_init                 — First-frame setup
  └── sequence_param_encode (1086B)  — Write SPS NAL
  └── picture_param_encode (268B)    — Write PPS NAL

venc_frame3 (1300B)                  — Main encode loop
  ├── prepare_cme                    — Set up motion estimator
  ├── cme_start_frame                — HW CME start
  ├── cme_wait                       — Wait for CME
  ├── picture_encode (1640B)         — Encode all MBs
  │   ├── mbloop_asm                 — MB encode loop (vector)
  │   ├── vc_intra4x4_vec/asm       — Intra prediction
  │   ├── vc_intra8x8_vec           — 8x8 intra prediction
  │   ├── vc_h264_iterate_CME       — Process CME results
  │   └── cabac_* / C2B*            — CABAC HW pipeline
  ├── slice_header_encode            — Write slice header
  └── delimiter_encode               — Write AU delimiter

venc_getbytes (616B)                 — Read encoded bitstream
venc_exit                            — Shutdown
```

### 7.4 Rate Control

```
venc_rc_preslice_calcquant (862B)   — Pre-slice QP selection
venc_rc_postslice_log (432B)        — Post-slice statistics
venc_rc_get_qp_for_iframe (254B)    — I-frame QP
venc_rc_target_change (88B)         — Target bitrate update
venc_rc_process_cabac (284B)        — CABAC-aware RC
venc_rc_update_from_cabac (232B)    — Update RC from CABAC stats
venc_asm_mb_updatequantmodel (1006B) — Per-MB QP model (ASM)
```

The "_asm" suffix functions contain Vec16 SIMD code running on the VPU
vector unit — these are the performance-critical inner loops.

---

## 8. ArcSoft Computer Vision (arcsoft_ft_stage.vll + arcsoft_rer_stage.vll)

### 8.1 Face Tracking (86KB .text, 85 exports)

Implements a Haar-cascade face detector (similar to Viola-Jones):
```
fpaf_afInitialCascade (10468B)       — Load cascade classifier
fpaf_afHaarDetectObjects_TwoScale    — Two-scale detection
fpaf_afHaarDetectObjects_FineScale   — Fine-scale detection
fpaf_afExpSmooth (770B)              — Exponential smoothing (tracking)
fpaf_afIntegral                      — Integral image computation
runShortClassifier                   — Cascade stage evaluation
```

Image processing ops on YUV420 data:
```
ZoomAndToGrayYUV420/MONOYUV420/NEGYUV420  — Scale + grayscale
ZoomMideScaleImiageYUV420                  — Mid-scale zoom
IntegralTwoImage_v_asm                     — Integral image (ASM)
```

Memory management wraps firmware allocator:
```
MMemAlloc, MMemFree, MMemAllocStatic, MMemFreeStatic
MMemMgrCreate, MMemMgrDestroy
MMemCpy (= memcpy), MMemSet (= memset)
```

### 8.2 Red-Eye Removal (124KB .text, 321 exports!)

Most function-rich VLL. Sophisticated eye detection + correction:

**Eye detection pipeline:**
```
ARER_DetectEyes (3978B)              — Main eye detector
ARER_EigenEye_1 (870B)               — Eigenface-based verification
ARER_EyeSeedDetect                   — Initial eye candidate detection
ARER_CascadeValidation (812B)        — Haar cascade eye validation
ARER_PupilValidation (992B)          — Pupil shape verification
ARER_EyeValidate (888B)              — Final eye validation
```

**Eye pairing:**
```
EyePair_GetPairs                     — Find eye pairs
EyePair_CalcFaceConfidence           — Face confidence from eye pairs
EyePair_GetSkinCountWithRotation     — Skin detection around eyes
EyeMatch_PairWithLowSimilarity      — Asymmetric eye matching
EyeLocation / EyeLocationK          — Sub-pixel eye localization
```

**Correction:**
```
ARER_FixRedeye                       — Main correction entry
ARER_FixRedeye_YCBCR420_P           — Fix in YCbCr 4:2:0 planar
ARER_FixRedeye_YCBCR422_P           — Fix in YCbCr 4:2:2 planar
ARER_ColorDiffusion_YCBCR420_P      — Smooth correction edges
ARER_SegementationByOTSU_INT        — Otsu thresholding
ARER_AggressivePupilMoving           — Handle pupil movement
ARER_Lock_GreenIris (1144B)         — Green iris correction
ARER_TryBrownIris_K (622B)          — Brown iris handling
```

---

## 9. Firmware API Surface (Consolidated Imports)

### 9.1 RTOS (Nucleus PLUS)

```
TCC_Create_Task, TCC_Delete_Task, TCC_Terminate_Task
TCC_Create_HISR, TCC_Delete_HISR      — Hardware ISR handlers
TCC_Task_Sleep, TCC_Current_Task_Pointer
TCT_Activate_HISR, TCT_In_LISR        — ISR activation
TCS_Change_Priority                     — Dynamic priority
TCF_Task_Affinity_Information           — Multi-core affinity
SMC_Create_Semaphore, SMC_Obtain_Semaphore, SMC_Release_Semaphore
EVC_Create_Event_Group, EVC_Set_Events, EVC_Retrieve_Events
```

### 9.2 Memory Management

```
rtos_malloc_priority                   — Priority-aware malloc
rtos_mempool_create/destroy            — Memory pool management
mem_alloc_ex                           — Extended allocation
mem_lock/unlock                        — Pin physical memory
mem_release                            — Free locked memory
mem_register_callback                  — Allocation notifications
```

### 9.3 Hardware Vtable Accessors

```
tabasco1_get_func_table                — ISP hardware registers
tu_get_func_table                      — Tuner utility hardware
systimer_get_func_table                — Hardware timer
intctrl_get_func_table                 — Interrupt controller
hdmi_get_func_table                    — HDMI output hardware
generic_led_get_func_table             — LED/flash control
ccp2_get_func_table                    — CCP2 serial interface
csi2_get_func_table                    — CSI-2 (MIPI) interface
cpi_get_func_table                     — Camera Parallel Interface
i2c_get_func_table                     — I2C master
pmu_uideck_get_func_table              — Power management
camera_subsystem_get_func_table        — Camera core
performance_lockout_get_func_table     — Performance lockout
```

### 9.4 Image/Buffer Framework

```
vc_image_initialise, vc_image_set_type/dimensions/pitch
vc_image_set_image_data, vc_image_lock/unlock
vc_image_blt, vc_image_get_u/v, vc_image_required_size
vc_pool_create/destroy                 — Image buffer pool
vc_pool_image_create/acquire/release   — Pool-based images
vc_metadata_add/get/lock/unlock/clear  — Image metadata
vclib_crc32, vclib_crc32_init          — Data integrity
```

### 9.5 System Services

```
sysman_register_user_ext               — Register system client
sysman_deregister_user                 — Deregister
sysman_set_user_request                — Request system state change
rtos_delay                             — Blocking delay
rtos_getmicrosecs                      — Microsecond timer
rtos_latch_get/put/try                 — Spinlock/latch primitives
```

### 9.6 Dynamic Linking

```
dlopen(path, flags)                    — Load VLL
dlsym(handle, symbol)                  — Lookup symbol
dldone(handle)                         — Signal lookup complete
dlclose(handle)                        — Unload VLL
```

### 9.7 File I/O

```
fopen, fclose, fread, fwrite           — Standard file ops
_fileno, _filelength                   — File descriptors
filesys_register, filesys_deregister   — Mount filesystem
```

The firmware provides a VFS layer — VLLs can read DCC calibration files
from the ROFS filesystem via standard POSIX-like file I/O.

---

## 10. Camera Sensor Drivers

### 10.1 Toshiba TCM8590MD (Main Camera, 8MP)

```
Exports:
  get_camera_tcm8590md              — Registration entry
  read_nvm_reg_series               — Read NVM (OTP) via I2C
  tcm8590md_read_ls_table (904B)    — Read lens shading calibration

Imports:
  i2c_get_func_table                — I2C bus access
  pmu_uideck_get_func_table         — Power management
  rtos_delay                        — Sensor power-up timing
  vcamtrace_record_measurement_point — Performance tracing

.rdata: 6892 bytes (I2C register tables)
.data:  1582 bytes (sensor configuration)
.bss:   1152 bytes (runtime state)
```

The 6.8KB .rdata section contains I2C register initialization sequences —
this is the sensor's power-on register programming.

### 10.2 Sensor VLL Triad Pattern

Each sensor has three VLLs:
1. `*_camera.vll` — Hardware driver (I2C, power, timing)
2. `*_tuner.vll` — ISP tuning parameters for this sensor
3. `*_camera_test.vll` — Factory self-test routines

The tuner VLLs are small (842 bytes .text) and primarily return pointers
to static tuning parameter tables.

---

## 11. Key Codenames Decoded

| Codename | Meaning |
|----------|---------|
| tabasco1 | ISP hardware block (image signal processor) |
| acmemini / acmeminist | Front camera sensor variants |
| tcm8590md | Toshiba main camera sensor (8MP) |
| cmnk8en00f | Alternative main camera sensor (CMN/OmniVision?) |
| C2B | CABAC-to-Bitstream hardware pipeline |
| CME | Content Motion Estimator hardware |
| FPAF | Face Presence Auto-Focus (ArcSoft subsystem) |
| ARER | ArcSoft Red-Eye Removal |
| RMI | Remote Metering Interface (exposure control) |
| DPF | Dynamic Parameter File (calibration format) |
| PWL | Piecewise-Linear function (tuning curves) |
| VRF | Vector Register File (Vec16 SIMD state) |

---

## 12. Implications for Linux Driver Development

### 12.1 What We Now Know

1. **VCHI service model is confirmed** — all hardware access goes through
   vtable-returning `*_get_func_table()` functions. A Linux driver needs
   to provide these vtables.

2. **ISP pipeline is self-contained** — the tuner VLL manages all 17 stages
   internally. A Linux driver only needs to provide the tuner with DCC
   calibration data and frame buffers.

3. **Camera sensor drivers are simple** — just I2C register sequences and
   power management. These could be reimplemented in Linux (or the VLLs
   could be loaded by a future Linux BCM2727 driver).

4. **Hardware accelerators need register maps** — CABAC, CME, and deblocking
   registers are accessed through the firmware's vtable layer. We need to
   map these to physical MMIO addresses.

5. **DCC calibration is essential** — the ISP tuner reads sensor-specific
   calibration from DCC files. We have these files extracted and analyzed
   (see `docs/dcp-analysis.md`).

### 12.2 Minimum Viable Camera Path

```
1. Power up BCM2727 (GPIO + TWL5031 rails)
2. Load firmware via CCP2
3. Open VCHI session (CONNECT handshake)
4. Open "CAMD" service
5. Load tcm8590md_camera.vll + tcm8590md_tuner.vll
6. Load isp_tuner_brcm.vll + DCC files
7. Set viewfinder mode → receive preview frames via BULK transfer
```

Steps 1-3 require reverse engineering DVCDriver (Task 045/046).
Steps 4-7 use VCHI protocol, which is architecturally same as RPi VCHIQ.

---

## 13. Disassembler Notes

### 13.1 Instruction Coverage

- **16-bit**: ~95% decoded (ALU, branch, ld/st, push/pop)
- **32-bit**: ~90% decoded (addcmpb, branches, memory, ALU triadic, float)
- **48-bit**: ~60% decoded (large-immediate ALU/MOV work, vector ops unknown)

### 13.2 Known Limitations

- Vec16 vector instructions (48-bit with 0xF8-0xFB prefix) show as raw data
- VC3-specific vector encoding differs from VC4 QPU — no public documentation
- Some 48-bit branch/memory forms may have incorrect offset calculation
- Push/pop with bank 0 (r0-based) untested

### 13.3 Tool Location

```
/tmp/vc3dis.py                  — Python disassembler
/tmp/vc3-tools/                 — Python venv with pyelftools
/tmp/videocoreiv/videocoreiv.arch — ISA encoding reference
```

---

## References

- hermanhermitage/videocoreiv — ISA documentation, [GitHub](https://github.com/hermanhermitage/videocoreiv)
- itszor/vc4-toolchain — GCC/binutils for VideoCore, [GitHub](https://github.com/itszor/vc4-toolchain)
- VideoCore IV Programmers Manual — [wiki](https://github.com/hermanhermitage/videocoreiv/wiki/VideoCore-IV-Programmers-Manual)
- Nokia E7 VLL extraction — `firmware-re/core-rofs0/vlls/bcm2727b1/`
- Phase 1 analysis — `docs/vll-api-analysis.md`
