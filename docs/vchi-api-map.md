# VCHI Service → VLL → Function Mapping

> **Note (2026-03-23):** Real HW is BCM2727B1 (ARM1176), not OMAP3630. See docs/critical-cpu-discovery.md

*Derived from Task 048 Phase 2 VLL disassembly — 2026-03-03*

---

## Service Architecture

The BCM2727 firmware provides services via vtable-returning functions.
VLL plugins access hardware through these vtables, never directly.

```
ARM Host (OMAP3630)
    │
    │  CCP2/MPHI Transport
    ▼
BCM2727 Firmware (Nucleus PLUS)
    │
    ├── camera_subsystem      → CAMD service
    ├── tabasco1 (ISP HW)    → ISP registers
    ├── codec (CME/CABAC)    → Video encode HW
    ├── vchwdec              → Video decode HW
    ├── hdmi                 → HDMI output
    ├── bufman               → Buffer management
    ├── dispcontrol          → Display control
    │
    └── VLL Loader (dlopen/dlsym/dlclose)
         ├── camera_cdi.vll       ← master camera control
         ├── tcm8590md_camera.vll ← sensor I2C driver
         ├── isp_tuner_brcm.vll  ← ISP pipeline tuning
         ├── h264enc.vll          ← H.264 encoder
         ├── vchwdec.vll          ← multi-codec decoder
         └── (30 more VLLs)
```

---

## Hardware Vtable Registry

Each hardware block exposes a function table obtained by name:

| Vtable Function | Hardware Block | Used By |
|-----------------|---------------|---------|
| `tabasco1_get_func_table` | ISP registers | isp_tuner_brcm, camera_cdi |
| `tu_get_func_table` | Tuner utility HW | isp_tuner_brcm |
| `systimer_get_func_table` | Hardware timer | isp_tuner_brcm |
| `intctrl_get_func_table` | Interrupt controller | camera_cdi |
| `ccp2_get_func_table` | CCP2 serial link | camera_cdi |
| `csi2_get_func_table` | CSI-2 (MIPI) | camera_cdi |
| `cpi_get_func_table` | Camera Parallel IF | camera_cdi |
| `i2c_get_func_table` | I2C master | tcm8590md, cmnk8en00f, acmemini |
| `pmu_uideck_get_func_table` | Power management | sensor VLLs |
| `hdmi_get_func_table` | HDMI output | aplay_hdmi |
| `generic_led_get_func_table` | LED/flash | camera_cdi |
| `camera_subsystem_get_func_table` | Camera core | camera_cdi, camera_ilc |
| `performance_lockout_get_func_table` | Perf lockout | camera_cdi |

---

## VLL Call Graph

```
camera_cdi.vll
│
├─ Firmware APIs:
│  ├─ camera_subsystem_get_func_table() → camera core vtable
│  ├─ tabasco1_get_func_table()         → ISP HW vtable
│  ├─ ccp2_get_func_table()             → CCP2 interface vtable
│  ├─ csi2_get_func_table()             → CSI-2 interface vtable
│  ├─ cpi_get_func_table()              → parallel camera vtable
│  ├─ intctrl_get_func_table()          → interrupt controller
│  ├─ generic_led_get_func_table()      → LED flash control
│  └─ performance_lockout_get_func_table()
│
├─ Dynamic VLL loading:
│  ├─ dlopen("tcm8590md_camera.vll") → get_camera_tcm8590md()
│  ├─ dlopen("cmnk8en00f_camera.vll") → get_camera_cmnk8en00f()
│  ├─ dlopen("acmemini_camera.vll")  → get_camera_acmemini()
│  └─ isp_tuner_load() → dlopen("isp_tuner_brcm.vll")
│
├─ RTOS:
│  ├─ TCC_Create_Task    (capture task)
│  ├─ TCC_Create_HISR    (frame-complete interrupt handler)
│  ├─ EVC_Create_Event_Group (state change events)
│  └─ rtos_latch_get/put (mutual exclusion)
│
└─ Metadata:
   └─ vc_metadata_add/get/lock/unlock/clear

camera_ilc.vll (OpenMAX IL camera component)
│
├─ DCC calibration:
│  ├─ apply_dynamic_parameter_file()
│  ├─ camera_dcc_obfuscate()           → DCC encryption
│  ├─ filesys_register/deregister()    → VFS mount
│  └─ fopen/fread/fclose               → read DCC files
│
├─ Image management:
│  ├─ vc_pool_create/destroy/image_*   → buffer pools
│  ├─ vc_image_*                       → image manipulation
│  └─ iltransform_*                    → image transforms
│
├─ VLL interaction:
│  └─ cdi_unload()                     → signal CDI shutdown
│
└─ Thread management:
   └─ ca_create_loader_thread()        → DCC loader thread

isp_tuner_brcm.vll (17-stage ISP pipeline)
│
├─ ISP HW access:
│  ├─ tabasco1_get_func_table()        → ISP registers
│  └─ tu_get_func_table()              → tuner utility HW
│
├─ DPF parameter reading:
│  ├─ isp_tuner_brcm_dpf_read_*()     → typed parameter access
│  └─ isp_pwl_*()                      → piecewise-linear curves
│
├─ Pipeline stages (×17):
│  └─ isp_tuner_brcm_<stage>_{open,close,get_ctrl,set_ctrl,
│                               pre_update,post_update,switch_mode}
│
├─ External stage VLLs:
│  ├─ dlopen("depurple_stage.vll")
│  ├─ dlopen("stillcolourdn_stage.vll")
│  ├─ dlopen("videocolourdn_stage.vll")
│  ├─ dlopen("nldn_stage.vll")
│  ├─ dlopen("stab_stage.vll")
│  └─ dlopen("rmi_sw_stage.vll")
│
└─ VRF management:
   └─ vclib_obtain_VRF / vclib_release_VRF

h264enc.vll / mpg4enc.vll / h263enc.vll
│
├─ Hardware:
│  ├─ codec_grab_CME / codec_release_CME  → motion estimator
│  ├─ C2BCmdWrite / C2BCmdQueueCheck      → CABAC hardware
│  └─ vcodec_enableencoderhardware()      → enable HW blocks
│
├─ Pipeline:
│  ├─ venc_alloc → venc_init → venc_firstframe_init
│  ├─ venc_frame3 (per-frame encode)
│  ├─ venc_getbytes (read bitstream)
│  └─ venc_exit
│
└─ VRF for vector operations:
   └─ vc_InitVRF_H264, mbloop_asm, vc_intra4x4_vec

vchwdec.vll (multi-codec hardware decoder)
│
├─ Codec modules:
│  ├─ AVS_*     (Audio Video Standard)
│  ├─ H264_*    (H.264/AVC)
│  ├─ H263_*    (H.263)
│  ├─ MPEG2_*   (MPEG-1/2)
│  ├─ MPEG4_*   (MPEG-4 Visual)
│  ├─ Sorenson_* (Flash video)
│  └─ VC1_*     (VC-1/WMV9)
│
├─ Hardware:
│  ├─ RegRdSim / RegWtSim               → register access
│  ├─ DMA 2D transfers                  → SDRAM ↔ local
│  └─ Deblocking HW                     → in-loop filter
│
└─ Arc debug:
   └─ ArcPrintf, ArcCommandBuffer       → debug console
```

---

## Import Frequency (Top 20 across all VLLs)

| Import | Count | Category |
|--------|------:|----------|
| memset | 26 | libc |
| memcpy | 24 | libc |
| free | 18 | memory |
| sysman_register_user_ext | 17 | system |
| sysman_deregister_user | 17 | system |
| sysman_set_user_request | 17 | system |
| rtos_malloc_priority | 14 | memory |
| sprintf | 14 | libc |
| __lmul | 13 | math |
| strcmp | 12 | libc |
| EVC_Create_Event_Group | 10 | RTOS |
| EVC_Retrieve_Events | 10 | RTOS |
| EVC_Set_Events | 10 | RTOS |
| mem_alloc_ex | 9 | memory |
| mem_lock | 9 | memory |
| mem_unlock | 9 | memory |
| mem_release | 9 | memory |
| dlopen | 8 | VLL |
| dlsym | 8 | VLL |
| dlclose | 8 | VLL |

The `sysman_*` trio appears in almost every VLL — this is the system
manager registration that all plugins must perform.

---

## VCHI Service Name → Implementation Mapping

Based on VLL exports and firmware string analysis:

| VCHI Service | FourCC | Primary VLL | Entry Point |
|--------------|--------|-------------|-------------|
| Camera | CAMD | camera_cdi.vll | get_cdi_camera |
| Camera ILC | ILCS | camera_ilc.vll | camera_ril_create |
| Video Encode | ILCS | h264enc/mpg4enc/h263enc.vll | get_*_enc_func_table |
| Video Decode | ILCS | vchwdec.vll | (codec-specific Init) |
| JPEG | ILCS | jpeg.vll | get_codec_jpeg |
| ISP Tuner | (internal) | isp_tuner_brcm.vll | get_isp_tuner_brcm |
| HDMI Audio | AUDS? | aplay_hdmi.vll | get_play_hdmi_func_table |
| Face Track | (stage) | arcsoft_ft_stage.vll | get_arcsoft_facetrack_interface |
| Red-Eye | (stage) | arcsoft_rer_stage.vll | get_imeffect_arcsoft_rer |
| Still Capture | ILCS | write_still_ilc.vll | get_write_still_ilc |
| Stabilization | (stage) | stab_stage.vll | get_stab_func_table |

Services marked ILCS go through OpenMAX IL Component Service —
the unified multimedia pipeline abstraction.
