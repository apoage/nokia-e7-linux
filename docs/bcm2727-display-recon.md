# BCM2727 display path reconnaissance — who owns the panel

**Date:** 2026-08-28 (task 110, desk item 1)
**Sources:** static analysis of `firmware-re/dumps/romdumpplus.dmp` (Symbian
ROM, va 0x80000000, file offset = va − 0x80000000), `docs/schematics/
schematics_decrypted.pdf` (E7-00 RM-626, Board 3EQ_08 v1.0), `docs/schematics/
service_manual_34_decrypted.pdf` (RM-626 BB troubleshooting guide),
`firmware-re/videocore-ref/` (BCM2708 register headers, precedent only).
**Confidence legend:** CONFIRMED = string/instruction/nets observed in the
artifact cited; INFERRED = interpretation.

---

## 0. Verdict

**The display is VideoCore-mediated. There is no APE-side DSI/LCD
controller.** The panel link (DSI), the composition/scaler pipeline, TV-out
(SDTV) and HDMI are all owned by the **BCM2727B "IVE" companion processor**
(the VideoCore subsystem, board ref D1400), which runs its own Nucleus-RTOS
firmware. The ARM1176 APE (RAPUYAMA) boots that firmware over MeSSI/CCP2 and
then controls display exclusively through VCHI RPC; the only display-adjacent
things the APE touches are power rails, reset/clock pins, and cable-detect
GPIOs. CONFIRMED from three independent directions (hardware, firmware
content, code behavior), below.

This is the Raspberry-Pi shape, not the OMAP shape: display is a
firmware service, so the follow-up work is a **mailbox/protocol decode**
(MeSSI transport + VCHI dispcontrol/dispmanx grammar), not a register map.

## 1. Hardware evidence (schematic + service manual)

- Schematic **page 2** ("Imaging processor, Primary camera, Camera flash"):
  the DSI pair **DSI1_CLKP/CLKN + DSI1_DP0/DN0**, the **DSI(5:0)** group and
  **TE0** leave the **Imaging processor** (ball refs V21/U21/V20/U20, L5) and
  run to the **UI flex connector (p9)** → display connector **X8001**. The
  same chip page carries the **CCP(30:0)** bus cross-referenced
  "RAPUYAMA (p3)" — i.e. the imaging processor is the RAPUYAMA companion,
  linked by the CCP bus, and it — not RAPUYAMA — owns the panel wires.
  CONFIRMED (net names on the page).
- Schematic **page 3** ("RAPUYAMA, Imaging processor, HDMI connector"):
  HDMI connector circuitry sits with the imaging processor. CONFIRMED.
- Service manual §3 ("IVE troubleshooting"): *"The IVE engine is a next
  generation imaging and video engine based on BCM2727B. The BCM2727B acts
  as imaging, video, **display**, and HDTV and SDTV hardware accelerator."*
  Same section names **RUN** (J1425, "Enable signal to IVE … High for IVE to
  be Up"), the **19.2 MHz XIN crystal**, and supplies VIVE_1V8/2V5/2V8/VBAT.
  HDMI section: HDMI 5V regulator N1651, HPD buffer D1653, connector X1650,
  plus **D1400 (BCM2727B)** on the HDMI signal list. CONFIRMED (quotes).
- These hardware names map 1:1 onto APE ROM strings (§2.2): DVCDriver's
  "Unable to acquire **RUN** pin", Vaux/Vout/Vio (the VIVE rails),
  TvOutKernelIveHdmi's "Failed to enable **HDMI 5v regulator**" /
  "unable to acquire **HPD** pin". The APE is the power/handshake butler;
  the BCM2727B is the display engine.

## 2. Firmware evidence (Symbian ROM)

### 2.1 The ROM embeds the VideoCore firmware — and the DSI driver is in it

ROM region **~0x22E780–0x329000** (~826 KB, VPU code — ARM literal pools
resume exactly at 0x329000) is the VideoCore firmware image set (same
software family as the OneNAND `SOS+IVE3A/IVE3B` partitions,
docs/bcm2727-firmware-analysis.md). *(Task 114 refinement, §7: the set is
registry-decoded to the exact span 0x22E588-0x3290B7 = 1,024,047 B — the
0x22E780 bound was img1's first nonzero byte, and the ~826 KB figure
undercounted.)* It contains the whole display stack on
the **VPU side** (offsets = file offsets in the ROM dump):

| off | string | meaning |
|---|---|---|
| 0x2b3554 | `IVE_DSI` | DSI driver trace tag |
| 0x2b355c | `../../../../applications/ive/display/ive_dsi.c` | **DSI panel driver source path** |
| 0x2b2a7c | `../../../../applications/ive/display/ive_cdp.c` | display/"cdp" companion module |
| 0x276aa0 | `DSI_TE` | panel tearing-effect handling (matches TE0 net) |
| 0x27131c | `../../../../middleware/dispmanx/dispmanx.c` | display composition API |
| 0x2d70f4 | `../../../../applications/vmcs/vchi/tvout.c` | TV-out VCHI service |
| 0x29df48 | `../../../../middleware/hdmi/hdmi.c` | HDMI middleware |
| 0x2fde68+ | `HDMI_VideoFmt_e*` enum table | TV/HDMI format list |
| 0x31a3d8/0x31a3e8 | `dispcontrol.vll` / `vchi_dispcontrol_server_init` | **display-control VCHI service (VC side)** |
| 0x31a45b | `vchi_dispservice_x_init` | display service |
| 0x31a405 | `endpoint_screen.vll` | screen endpoint service |
| 0x304adb | `tvout.vll` | TV-out service plugin |
| 0x2e7efc | `dispc_q` | display-controller queue trace name |
| 0x2baaa0 | `../../../../interface/vchi/message_drivers/videocore/mphi_ccp2.c` | VCHI transport (VC side) |
| 0x2c504c | `../../../../vcfw/rtos/nucleus/rtos_nucleus.c` | Nucleus RTOS (= VPU resident) |
| 0x2b5848 | `khronos.vll`, plus GLSL builtins 0x303xxx | GLES server (VC side) |
| 0x328a48 | `/Z:/vlls/bcm2727b1/` | VLL load path (VC firmware reads the ROM fs) |

The VC firmware loads its plugins from `/Z:/vlls/bcm2727b1/` — matching the
extracted `firmware-re/core-rofs0/vlls/bcm2727b1/` (35 VLLs; note: no
`dispcontrol.vll`/`tvout.vll` files there — they are among the ~15 VLLs
**built into** the firmware image, per bcm2727-firmware-analysis.md §1).

The image provider is the Symbian-side **DVCExe** (ROM 0x22E250:
`DVCExe::RegisterImages() more image sets than slots available`,
`DVCExe::GetImages() no suitables images found`, set name `bcm2727b1` at
0x22E540, registry pointer pool → RAM 0xC80104C0/0xC80104C4). DVCDriver::Boot()
asks DVCExe for images and uploads them (§2.2).

### 2.2 The APE display stack is VCHI clients all the way down

Kernel-side components in the ROM (all ARM code):

- **DVCDriver PDD** (`IVE3_Vcd_Drv`, strings 0x3298D0–0x331F00, code from
  0x329000): two-stage VideoCore boot over the VC boot-ROM protocol —
  `DVCDriver::Boot() VideoCore boot ROM not responding (stage 1/2); HAT=…`,
  `VideoCore image ID found/not found`, `Failed(%d) to start 2nd stage boot
  loader`, `no images returned from DVCExe`; MeSSI + CCP2 channels
  (`TControllerMessi`, `DVCMessiRx/TxChannel`, `DVCCcpRxChannel`, transport
  source path `Y:/ncp_sw/core7.0/IVE3_Engine/IVE3_rapu_drivers/interface/
  vcdriver/src/messi/controller_messi.cpp` @0x330DC4). Supply management:
  `Failed to activate/deactivate Vaux/Vout/Vio`, `Failed to set Vout
  voltage`. **Its complete APE MMIO footprint** (literal-pool scan of
  0x310000–0x350000, all peripheral windows): the supply pair
  **0xC6024810-80 / 0xC6025810-64** (= PA 0x200248xx/0x200258xx, the XTI
  doc's "display supply pair"), pad-mux rows 0xC60318E0-0xC60319E8,
  clock regs 0xC6033008/3038/3178, and 0xC6032664 / 0xC6034000 /
  0xC6034200 / 0xC6034240 (unattributed — MeSSI/GPIO-bank candidates for
  the follow-up decode). **No scanout/DSI-style register cluster exists.**
  CONFIRMED.
- **VCHI host library** (strings 0x333DBC–0x33BD18): `VCHI: MeSSI-16 CRC
  failure` (0x335214), `VCHI: CCP2 CRC failure` (0x335230) — the two
  transports; trace points `DISPLAY_TRANSFER_START/STOP`, `DISPLAY_READY`,
  `COMPOSITION_START/END` (0x33A75D+) exist as names only.
- **vcdispman LDD** (0x429808-0x4298F4): `DISPMAN_LDD_UPDATE_MUX`,
  `DISPMAN_LDD_FRAMECOUNT_MUX`, `DDispmanLddChannel/Factory`, name
  `vcdispman` — the user-space handle to composition.
- **VCDispman kernel client** (0x346540-0x347EF1):
  `DVCDispmanVCHI: VCHI Host Lock`, `VCHI Message Available Semaphore`,
  update-callback handler, update lists — composition requests become VCHI
  messages here.
- **TvOutKernelIve extension** (0x343748-0x345B2B): HDMI variant (cable-detect
  pin ISR, HPD pin ISR, "Failed to enable/disable HDMI 5v regulator",
  Vaux, copy-protect listener = HDCP notification) and SDTV variant (Vaux).
  Power/pins only — encoding is VC-side (`tvout.vll`, `hdmi.c`).
- **EGL/Khronos bridges** (0x42A2C4-0x4316B0): `KHRSendCtrl/KHRSendBulk/
  KHRRecv/KHRStartVCHI` (0x42E660+), `DEGLServer::VCHIDriverCB` (0x430A2C),
  `khegl_ldd.ldd` (0x431258) — GLES rides VCHI too.
- **DDC access LDD** (0x42A29C: `DDC_ACCESS_MUX`, `DdcAccessChannel`) —
  HDMI EDID/HDCP DDC, via the VC (paired with `DataServiceIveBc` HDCP-key
  service, 0x42BDC4+: "Error initialising VCHI", `GetHdcpKey`,
  `VerifyHdcpKey`).
- **User-side composition backend** (namespace `Gce`, 0xD4DCA8-0xD5AF78):
  `CVCCompositionBackendGlobal`, scene/layer managers, TV-out clone manager
  `CVCGceTvOutManager` — and the smoking gun for the plumbing:
  `CVCCompositionBackendServer::ConstructL() - error loading **dispman
  logical device**` (0xD59D44) / `error loading **EGL logical device**`
  (0xD59D98). The window server composes through the dispman/EGL LDDs.
- **Variant HAL channel names** (`PriDisp`, `SecDisp`, `DisplayDriver`,
  `PanicScreen`, `BootFigure0/1`, `PowerTouareg` — tables at ROM 0xBCE4C+
  and repeated in `firmware-re/bcm2727/signed_2727b1_v2.dll`) are HAL
  *channel name tables* only; the variant DLL contains no display PDD
  implementation and no display MMIO. `DisplayDriver` = the on/off/state
  HAL channel; `SecDisp` = the TV/HDMI clone target; `BootFigure0/1` /
  `PanicScreen` = boot and panic images (which themselves must be composed
  via the VC after boot).
- **Power-resource registry names** (ROM 0xCC238-0xCC5EC) show the clock
  handles the APE gates for the links and external display:
  `EHwClkMeSSI` (0xCC328), `EHwClkViSSI` (0xCC348), `EHwClkLoSSI`,
  `EHwClkEdAvsync`/`EHwClkEdCommon` (0xCC238/0xCC24C), `EHwClkVGALite`
  (0xCC38C), `EHwClkRFBI` (0xCC338), `EHwRegHdmi` (0xCC4D8). Names only —
  whether all are live on this board vs OMAP-era inherited names is an
  open question (§5).

### 2.3 Negative results (what was excluded)

- **No DSI/LCD/panel/MIPI/AMOLED strings and no scanout MMIO cluster exist
  anywhere in the APE ROM code.** The only `DSI`/`LCD` string hits are in
  the VPU firmware blob (`IVE_DSI`, `DSI_TE`) or unrelated (a typeinfo
  `LCD` substring in an XML DLL; ADS audio-codec names). CONFIRMED.
- **Kernel early-map census** (table at ROM 0x800020A0, docs/bcm2727-icu-
  timer.md §4): the unnamed windows — VA 0xC6010000←0x09000000,
  0xC6011000←0x08000000, 0xC6012000←0x20010000, 0xC60A0000←0x20600000,
  0xC60A1000←0x20700000, 0xC60AF000←0x20680000 — are referenced **only
  from the bootstrap mapping table itself**; no driver in the ROM touches
  them. None can be a CPU-driven display block. CONFIRMED (literal scan of
  the kernel-side ROM 0x0-0x460000).
- `VCDriver: Gfx Fifo Underflow` (0x32B268) and `SyncLostDigit Interrupt
  Happened` (0x32B28C) look display-ish but have **zero references** (no
  literal-pool word anywhere in the ROM points at them) — OMAP-port
  leftovers, not evidence of an APE display interrupt. CONFIRMED
  (unreferenced); interpretation INFERRED.
- `nloader.bin` has zero display strings (task-file premise, re-verified
  during the XTI work): display bring-up is entirely post-NLoader. The
  panel is dark until the Symbian kernel boots the VideoCore.

## 3. Architecture picture (as evidenced)

```
            AMOLED panel (640x360, CBD)          TV / HDMI
                 │ DSI1_CLK±, DSI1_D0±, TE0       │ SDTV DAC, HDMI TMDS,
                 │ (schematic p2/p9 → X8001)      │ HPD, DDC, 5V
                 ▼                                ▼
   ┌────────────────────────── BCM2727B "IVE" (D1400) ────────────────┐
   │  VideoCore VPU(s), Nucleus firmware (in-ROM set "bcm2727b1", §7;     │
   │   flash IVE3A/B = FOTA-side, not the boot source)                    │
   │  ive_dsi.c · DSI_TE · dispmanx · dispcontrol.vll · tvout.vll     │
   │  hdmi.c · khronos (GLES) · gce_rpc · VLL loader (/Z:/vlls/...)   │
   └────────────▲──────────────────────────────▲──────────────────────┘
        MeSSI-16│ (msgs)                CCP2/CCP│ (bulk: firmware, bufs)
                │ HAT (VC→AP attn GPIO)  RUN pin │
   ┌────────────┴──────────────────────────────┴──────────────────────┐
   │ RAPUYAMA APE (ARM1176, runs SWBL→NLoader→Symbian)                │
   │  DVCDriver PDD: boot protocol, Vaux/Vout/Vio rails @0x20024810/  │
   │    0x20025810, padmux, clocks                                     │
   │  VCHI host lib → vcdispman LDD / VCDispman client / TvOutKernelIve│
   │  / DDC access / DataServiceIveBc (HDCP) / EGL+KHR bridges         │
   │  user side: Gce composition backend (window server)              │
   │  panel housekeeping GPIOs: DISP_RESET=37, EXTCLK=38, TE0=33       │
   └──────────────────────────────────────────────────────────────────┘
```

## 4. Anchor list for the follow-up decode

APE side (ARM, ROM va = 0x80000000+off):

| anchor | where | what |
|---|---|---|
| DVCDriver PDD `IVE3_Vcd_Drv` | ROM 0x329000-0x332200 (code+strings) | VC boot state machine; start here |
| DVCDriver boot strings | 0x32DBC0-0x32E226 | two-stage boot + image negotiation log lines |
| DVCExe image registry | `vc_exe_registry.dll` ROM 0x22E040 (0x25C); container `signed_2727b1_v2.dll` 0x22E320 (0xFAD34); RAM 0xC80104A4 (4 slots) / 0xC80104C4 (set record) | image-set provider ("bcm2727b1") — fully decoded, §7 |
| VCHI host library | ROM 0x333000-0x33C000 | transport framing; MeSSI-16 + CCP2 CRC paths |
| vcdispman LDD | 0x429808-0x4298F4 (strings), code around | user↔kernel composition handle |
| VCDispman VCHI client | 0x346540-0x347EF1 | **dispmanx message grammar, ARM side** |
| TvOutKernelIve ext | 0x343748-0x345B2B | HPD/cable GPIO + rails choreography |
| DDC/HDCP services | 0x42A29C-0x42C9CC | EDID/HDCP over VCHI |
| EGL/KHR bridges | 0x42E660-0x4316B0 | GLES transport (GPU thread boundary) |
| Gce composition backend | 0xD4DCA8-0xD5AF78 | who calls whom (user side) |
| DVCDriver MMIO (complete) | VA 0xC6024810-80, 0xC6025810-64, 0xC60318E0-0xC60319E8, 0xC6032664, 0xC6033008/3038/3178, 0xC6034000/4200/4240 | supplies/padmux/clocks + 4 unattributed regs |
| Power-resource names | 0xCC238-0xCC5EC | EHwClkMeSSI/ViSSI/Ed*/VGALite/RFBI, EHwRegHdmi |

VideoCore side (VPU code, in-ROM copy):

| anchor | where | what |
|---|---|---|
| Embedded VC firmware | ROM ~0x22E780-0x329000 (~826 KB) | the decode target itself |
| `IVE_DSI` / `ive_dsi.c` | 0x2B3554 / 0x2B355C | panel driver entry points (trace-tagged) |
| `ive_cdp.c` | 0x2B2A7C | display companion module |
| `DSI_TE` | 0x276AA0 | tearing-effect path |
| `dispmanx.c` middleware | 0x27131C | composition engine |
| `vchi_dispcontrol_server_init` | 0x31A3E8 | display-control service loop |
| `gce_rpc/resource_handlers.c` | 0x2BFB48 | composition RPC handlers |
| `tvout.c` / `hdmi.c` | 0x2D70F4 / 0x29DF48 | TV/HDMI services |
| VLL name tables | 0x304A90-0x304B1A, 0x31A398-0x31A683 | built-in vs file VLLs |
| flash copies | OneNAND `SOS+IVE3A` (160 KB, **blank** in this dump), `SOS+IVE3B` (~2 MB, encrypted) | FOTA-side variants, NOT the boot source — see §7.5 |

Reference (BCM2708/VC4 precedent, `firmware-re/videocore-ref/rpi-open-firmware/
broadcom/bcm2708_chip/`): `dsi.h` (DSI0 @ VC 0x7E209000), `hvs.h` (scaler @
0x7E400000), `pixel_valve*.h`, `vec.h`, `hdmi.h`/`hdmicore.h` — register
*shapes* to test against the BCM2727B (VC3) once VPU decode starts; addresses
will differ and `ive_dsi.c` behavior wins over the VC4 headers.

## 5. Proposed follow-up plan (sized, no implementation yet)

1. **DVCExe image-set decode** — **DONE 2026-08-31 (task 114), findings in
   §7**: registry = `vc_exe_registry.dll` 4-slot array at RAM 0xC80104A4 +
   one registered set `bcm2727b1` (container `signed_2727b1_v2.dll` @ ROM
   0x22E320) with three images — 8 KB loader ×2 (A/B variants) + 1,010,219 B
   main firmware, payload+"sign"+20-byte-signature containers, set span
   0x22E588-0x3290B7; flash reconciliation: ROM set ≠ IVE3B (distinct ~1.0 MB
   plaintext build, the primary boot source), IVE3A blank in this dump.
   Unblocked downstream: steps 2, 3, 5.
2. **MeSSI-16 transport decode** — **DONE 2026-09-01 (task 116), findings in
   §8**: engine base CONFIRMED VA 0xC6034000 (PA 0x20204000) via
   `[TControllerMessi+4]`; channel-group window 0xC6034200 (per-channel
   0xC6034250+0x30×ch, tag `(ch<<16)|0x3210`); engine control 0xC6034240;
   0xC6032664 REFUTED (GENIO pad-bank). VCHI frame = 0x1C-byte header +
   payload, **CRC-32C (Castagnoli, init 0xFFFFFFFF)** over payload at
   header+0xC — "MeSSI-16" names the link width, not the CRC. Clock:
   EHwClkMeSSI = resource id 0xE; physical gate = [0xC6033178] bit 5 +
   companion [0xC6033158]=0x80, staged 0xC6033008 bring-up. Register map,
   frame diagram and the step-3 hand-off anchors in §8.
3. **DVCDriver::Boot() protocol** — **DONE 2026-09-03 (task 119), findings in
   §9**: the boot exchanges are 0x20-byte magic-framed slots
   (0xAFE45251 … 0xF001BC4E) posted as receive buffers on the MeSSI message
   channel; all APE→VC payload traffic is the two 0x42-command image uploads
   (8 KB loader + ~1 MB firmware, scatter-gather) each followed by a
   0x20-byte header carrying length+signature in the same magic framing;
   replies surface via MeSSI IRQ 0xC9 → completion counter → 5 ms-poll wait;
   final success = reply word {count=3, answer=1}. Stage-blob OPEN resolved:
   the cells are BSS-zero (pure runtime exchange buffers). HAT = GPIO
   attention line latched at [MeSSI-channel +0x20]. State machine, message
   tables and the step-4 hand-off anchors in §9.
4. **VCHI dispcontrol/dispmanx grammar from the ARM side** — **DONE
   2026-09-04 (task 120), findings in §10**: the post-boot chain is
   Gce/rdispman (user) → vcdispmanldd.ldd → vcdispmanvchi.dll → vchi.dll →
   vcdriver.dll; the §9.11 Start-consumer is vchi.dll's dispatcher
   0x803370B0/0x80336FB0; services are fourcc-addressed ("DISP" dispmanx +
   "UPDH" update-callbacks opened by vcdispmanvchi; "DSPC" dispcommand,
   "BUFM"/"XMFB" buffer manager, "CRMI" rmi beside; VC-side server names
   DISPCONT/DISPCOND at dispc_q); every dispmanx payload = {u32 static
   msgId 2-0x26, args} inside a 0x10-B per-message record
   {0, len, fourcc, 0, 0, plen} inside §8.4's 0x1C-B frame; update cycle =
   0x10 start → element op 0x13/0x17 → 0x11 submit, completions on UPDH;
   all composition traffic is short-message (≤0x1000, flags 0) — bulk is
   the buffer-manager's job. Minimal-client contract + message tables +
   step-5 hand-off in §10. Unblocked downstream: 5.
5. **VPU-side `ive_dsi.c` decode** (large; hardware-session-adjacent):
   needs 1; disassemble around the `IVE_DSI`/`DSI_TE` tags for the DSI
   DCS init sequence and lane setup; compare against `dsi.h` shapes.

Implementation arc on top of steps 1-4 (the "keep the VC firmware,
write a Linux VCHI host" strategy): **M1 DONE 2026-09-06 (task 125)** —
QEMU MeSSI engine model + scripted VC responder + in-chain proof binary,
as-built §11; M2 = the Linux VCHI transport driver (binds to §11.6);
M3 = the VC-boot + dispman client.

Strategy note for Linux bring-up (decision, not work yet): the RPi
precedent + this verdict say the sane path is **keep the VideoCore
firmware and write a Linux VCHI host + dispman client** (framebuffer +
KMS later). Direct CPU drive of DSI/HVS would mean re-implementing the
VPU's entire power/clock/boot choreography — poor trade.

Open questions left deliberately: ~~provenance of the in-ROM VC copy vs
IVE3B (task 1)~~ **answered 2026-08-31 — §7: distinct ~1.0 MB plaintext
build, primary boot source; IVE3B is an encrypted ~2.1 MB FOTA-side variant,
IVE3A blank**; ~~initial values of Boot's stage-1/2 blobs at
0xC801055C/0xC801057C~~ **answered 2026-09-03 — §9.8: vcdriver BSS, i.e.
statically ZERO — pure runtime exchange buffers, refuting the
relocated-constants hypothesis**; whether `EHwClkRFBI`/`VGALite`/`Ed*` clocks
are live or OMAP-inherited names; `SecDisp` ↔ HDMI-clone mapping
confirmation; pad mux code-3 group (pads 154-161) owner; TE0 routing (panel →
BCM2727 TE ball and/or RAP GPIO33 — schematic shows TE0 in the DSI group on
p2, padtable has TE0 as RAP GPIO33 input 33; possibly both receive it).

## 6. Reproducibility

- String hunts: `strings`-style scan of `firmware-re/dumps/romdumpplus.dmp`
  with offsets (all offsets in this doc are file offsets; va = +0x80000000).
- MMIO scans: 4-aligned literal-pool word scan per region, windows
  0x20000000-0x207FFFFF / 0xC6000000-0xC6FF0000 / 0x40100000-0x40200000
  (ASCII-noise filtered); the DVCDriver footprint list in §2.2 is the
  deduplicated result for 0x310000-0x350000.
- Early-map census: same scan restricted to kernel-side ROM 0x0-0x460000,
  bucketed by the icu-timer §4 table's windows.
- Task 114 registry decode: capstone ARM disassembly
  (`tools/task114_dis.py <hex-offset> <hex-len>`, va base
  0x80000000); registry-cell literal scans (4-aligned word scan for
  0xC8010xxx and for `RegisterImages` thunk targets); ROM-directory
  decode at 0x46544+; flash TOC parse at file 0x140000 of
  `firmware-re/nokia-e7-onenand.img` (device X = file X+0x140000);
  md5/entropy/overlap comparisons as cited in §7.
- Schematic: `pdftotext -layout` on `docs/schematics/schematics_decrypted.pdf`,
  pages 2/3/9/15; service manual IVE section = BB guide p3-23.

## 7. DVCExe image set — the VPU firmware inventory (task 114, 2026-08-31)

**Method:** static ARM disassembly (capstone; helper
`tools/task114_dis.py`) of the DVCExe registry code and
`DVCDriver::Boot()` in `firmware-re/dumps/romdumpplus.dmp` (all offsets =
file offsets, va = +0x80000000; RAM 0xC801xxxx = kernel static-data cells),
ROM-directory decode, and byte-level reconciliation against the OneNAND flash
copies (`firmware-re/nokia-e7-onenand.img`, device X = file X+0x140000).
**Legend for this section:** CONFIRMED = decoded from the cited bytes;
CONTRACT = shape fixed by cross-checked code paths, exact values pending;
OPEN = not resolvable from the artifacts.

### 7.1 Two modules, not one (ROM directory names the players)

The ROM file directory (entries at 0x46544+, format
`{0xFFFF, size:4, imagePtr:4, 0x81xx, UTF-16 name}`) identifies the code
around the registry anchor as two DLLs:

| DLL | ROM image | size | role |
|---|---|---|---|
| `vc_exe_registry.dll` | 0x22E040 | 0x25C | **DVCExe**: owns the slot array; `RegisterImages` 0x22E1AC, `GetImages` 0x22E1D8, slot-array init 0x22E138; strings 0x22E250-0x22E2B8 |
| `signed_2727b1_v2.dll` | 0x22E320 | 0xFAD34 (~1 MB) | **the signed VC firmware image-set container**: E32 header (UID1 0x10000079; header words delimit the blob through 0x803290C0) + its own ARM initializer (constant-loader stubs 0x22E3AC-0x22E3F0, constructor 0x22E454, set name `bcm2727b1` at 0x22E540) + the three images |

This corrects bcm2727-firmware-analysis.md §1/§3.1: `signed_2727b1_v2.dll`
is **not** an "ARM HAL driver" — its ~1 MB "code" section *is* the VC
firmware image set, and the DLL exists to register it with DVCExe at kernel
build/boot. CONFIRMED (directory entry + E32 header + initializer decode).

Registry RAM cells (kernel static data; each DLL's ROM header carries a
writable-cell descriptor — records containing `{cell, size}` at 0x22E090
(0xC80104A0, 0x14), inside vc_exe_registry.dll, and 0x22E374
(0xC80104C0, 0x34), inside signed_2727b1_v2.dll; the descriptors also
carry module-reference pointers into the ROM directory at
0x4654C-0x465B0 whose exact semantics are not decoded):

- **0xC80104A0** guard (bit0 = initialized) + **0xC80104A4…0xC80104B3** the
  **4 image-set slots** (zeroed by 0x22E11C via 0x22E138 before first use).
- **0xC80104C0** guard + **0xC80104C4** the one registered set record.
  CONFIRMED (stores/loads at 0x8022E47C-0x8022E510, 0x8022E138-0x8022E170,
  0x8022E1AC-0x8022E1CC).

### 7.2 The registry walk (constructor → record → slots)

Constructor 0x8022E454 (`signed_2727b1_v2.dll`): guards-check → three
constant-loader calls → fills the record at 0xC80104C4 → publishes guard →
`RegisterImages(slots, 0xC80104C4)`. Record shape (every store decoded):

```
+0x00  image count (=3; see note)      +0x1C  img3 {ptr,len,sig}
+0x04  img1 {ptr,len,sig}              +0x28  name[12] "bcm2727b1"
+0x10  img2 {ptr,len,sig}              total 0x34 with guard cell
```

- img1 `{0x8022E588, 0x2050, 0x802305DC}` — helper 0x8022E3AC
  (`*len=0x2068`, ptr=0x8022E588)
- img2 `{0x802305F8, 0x2050, 0x8023264C}` — helper 0x8022E3C4 (same len
  literal 0x2068)
- img3 `{0x80232668, 0xF6A2B, 0x80329097}` — helper 0x8022E3DC
  (`*len=0xF6A4F`). The stored lengths are the helper lengths minus
  0x18/0x18/0x24 and the sig pointers are ptr+len−0x14/−0x14/−0x20 —
  exactly the container geometry below. CONFIRMED.
- **Container geometry:** every image is
  `[payload]["sign" magic, 4 B][20-byte signature]`; the record's len field
  = payload length, the sig field = pointer to the 20 signature bytes.
  The signatures are *not* plain SHA-1 of the payload (computed and
  compared — img1 `2fcc3826…`, img2 `cda7da55…`, img3 `bcaa61d5…`);
  they ride along with every upload (§7.4) for the VC boot ROM to verify.
  img3 carries 12 extra bytes after its signature
  (0x3290AB-0x3290B6, nature unknown). CONFIRMED geometry; sig algorithm
  OPEN.
- The count word at record+0 is read by `GetImages` (0x8022E1E4: skips sets
  whose count exceeds the request, keeps the set with the most images) and
  by DVCDriver's accessor (0x8032D9D4 `return set->count`), but is written
  by **no** code path — it is static-initialized with the container DLL's
  data cell (descriptor 0x22E374). Value 3 INFERRED (Boot uses slots 0-2
  and the accessor exists to check it).
- **Exactly one set is ever registered**: ROM-wide literal scan finds no
  other writer of 0xC80104A4-region cells and no other caller of
  `RegisterImages` — slots 1-3 stay empty and the "more image sets than
  slots available" panic (0x22E250) is dead code in this build. CONFIRMED
  (scan of 0x0-0x460000).

### 7.3 The image table (mapped into the ROM blob)

| slot | payload [file off) | len (B) | "sign" @ | payload md5 | content |
|---|---|---|---|---|---|
| 0 | 0x22E588-0x2305D8 | 8,272 (0x2050) | 0x2305D8 | `3c2ba6c817599e372421340e27ab0af5` | VC boot-rom **loader, variant A**: 0x200-byte zero header, VPU code from 0x22E788; embeds a 128-byte key-like blob shared with variant B |
| 1 | 0x2305F8-0x232648 | 8,272 (0x2050) | 0x232648 | `ab6f4b13da1d1449b7da90d7a6ea127e` | **same loader, build variant B** — 3,251 bytes differ (scattered +0x10 offsets and one 0x40-byte block relocated); identical 128-byte key blob |
| 2 | 0x232668-0x329093 | 1,010,219 (0xF6A2B) | 0x329093 | `84d88307bbaf3c48004a09f4a433eb72` | **main Nucleus firmware**: VPU entry-point table at the start (0x12A8x vectors), every §2.1 string inside — `IVE_DSI`@0x2B3554, `ive_dsi.c`@0x2B355C, `rtos_nucleus.c`@0x2C504C, `/Z:/vlls/bcm2727b1/`@0x328A48 |

Whole set spans **0x22E588-0x3290B7 = 1,024,047 B (~1.0 MB)**, with 8-byte
gaps between containers. This pins §2.1's region precisely: the old bounds
"~0x22E780-0x329000, ~826 KB" started at img1's first nonzero byte
(0x22E788) and dropped ~180 KB of arithmetic; the registry-decoded bounds
are 0x22E588/0x3290B7. CONFIRMED.

### 7.4 Who consumes what — `DVCDriver::Boot()` (0x8032DCB0)

Boot's image flow, decoded end to end (vcdriver.dll, ROM 0x3290D0+):

1. copies two 0x20-byte blobs from its static cells **0xC801055C** and
   **0xC801057C** to the stack (kernel copy import 0x800AF434, thunks
   0x32942C) and uploads them as boot stages 1-2 (uploader 0x8032DB78 →
   channel send 0x8032C800) — these are the "boot ROM not responding
   (stage 1/2)" exchanges.
2. `GetImages(slots, requested=this+0x31C, &this+0x318)` — else
   "no images returned from DVCExe" (0x32DF04 format string).
3. uploads **img1 or img2** (`set+4` / `set+0x10`, chosen by Boot's arg
   r8) through 0x8032DAA0 — the "2nd stage boot loader" — which also
   transmits the image's 20-byte signature (0x14 bytes copied from the
   sig pointer, 0x8032DB1C-0x8032DB2C) and a length-tagged destination
   word `(x & 0x00FFFFFF) | 0x02000000`.
4. uploads **img3** (`set+0x1C/+0x20/+0x24`, 0x8032E090/0x8032E0BC) —
   the main firmware — then negotiates the final state (expects
   count==3/answer==1 via 0x8032D9EC = the BootParseMessage wrapper).

So the set's slots are: **0/1 = alternative builds of the 8 KB VC
boot-rom loader (A/B selected by a Boot flag), 2 = the main ~1 MB Nucleus
firmware.** CONFIRMED (code). The two 0x20-byte stage blobs of step 1 are
a CONTRACT (shape: uploaded triples from DVCDriver static cells) whose
**initial values are OPEN**: no ROM code writes those cells, and no
{ptr,len,sig}-shaped initializers exist anywhere in the kernel-side ROM
(pattern-scanned twice); they are either kernel-build-relocated constants
not carried in this dump or runtime-filled (e.g. from the NLoader/SWBL
SDRAM handoff of the flash IVE3 images — see §7.5).

### 7.5 Provenance vs the flash copies — verdict

Real flash TOC (parsed from the dump at file 0x140000; corrects the
"0x12380/0x0C000" offsets in bcm2727-firmware-analysis.md §2.1, which
dropped digits): **SOS+IVE3A dev 0x1238000 size 0x28000 (163,840 B)**,
**SOS+IVE3B dev 0x0C0000 size 0x1FFD51 (2,096,465 B)** → file
0x1378000 / 0x200000.

- **IVE3A is completely zero-filled in this phone's dump** — 0 nonzero
  bytes in all 163,840 (and zeros, not 0xFF, so programmed-blank rather
  than erased). The "BB5-encrypted IVE3A" reading in
  bcm2727-firmware-analysis.md is corrected: the partition is **blank**.
  CONFIRMED.
- **IVE3B is real but is not the ROM set**: entropy 7.98 (encrypted/
  packaged), a small header (`a3959780 02000000 16230298…`), exactly one
  meaningful string (`fota_version` +0x3E4), a repeating 5-byte tail
  pattern, **no** plaintext source paths (no `ive_dsi.c`, no
  `rtos_nucleus`, no `vlls/bcm2727b1` — all present in ROM img3), its
  head occurs nowhere in the ROM, and no 4 KB window of ROM img3 occurs
  anywhere in it. Sizes differ 1,010,219 vs 2,096,465.
  md5(IVE3A)=`6cc3d8ecd5a9967c9227be8d17b988a6`,
  md5(IVE3B)=`f983f4371dbd986ea0e33c0d82a163d1`. CONFIRMED.
- **Verdict: the in-ROM set is neither IVE3B nor its fallback — it is a
  distinct, smaller (~1.0 MB vs ~2.1 MB) plaintext build of the same
  firmware family, and it is the *primary* boot source**: it is the only
  set DVCExe ever registers, `DVCDriver::Boot()` uploads it directly from
  ROM, and the phone's IVE3A slot is blank. The flash pair is the
  factory/FOTA channel — IVE3B carries an encrypted ~2.1 MB variant
  (FOTA-marked) that nothing on the decoded APE boot path reads.
  Inequality CONFIRMED (above); "primary vs fallback" INFERRED from
  single-set registration + blank IVE3A. Whether the runtime-filled
  stage-1/2 blobs of §7.4 point at an SDRAM copy of flash IVE3 images is
  the remaining OPEN thread.
- Note: the QEMU chain's "IVE3 binary not found" (task 107) is a shim
  curation artifact (onenand doc §5.3 excludes SOS+IVE3*), not hardware
  evidence either way.

### 7.6 What this hands step 2/3 (MeSSI + boot-protocol decode)

- Transport entry points pinned: channel sends 0x8032C800 /
  0x8032C1A0 through `this+0xB4`/`this+0xB8` objects; expect/wait helper
  0x803294B4 (1000 ms × 5 retries); BootParseMessage wrapper 0x8032D9EC.
- The image-upload message grammar now has anchors: command byte 0x42,
  destination word `(x & 0x00FFFFFF) | 0x02000000`, length, then payload
  + 20-byte signature — decode the framing around 0x8032DAA0/0x8032DB78.
- For the VPU-side decode (§5 step 5): the loader pair is small enough
  (2×8,272 B) to reverse completely, and its 128-byte shared blob is
  almost certainly the stage-2 verification key — worth identifying
  before touching `ive_dsi.c`.

## 8. MeSSI-16 transport — engine base, register map, frame, CRC (task 116, 2026-09-01)

**Method:** static ARM disassembly of vcdriver.dll (ROM 0x329000-0x332200)
and the VCHI host library (0x333000-0x33C000) in `firmware-re/dumps/
romdumpplus.dmp`, seeded from the §7.6 anchors. Two decode notes for
reproducibility: (a) the MeSSI accessor cluster loads its literal pool at
0x8032EDFC **backward** (`ldr rX, [pc, #-imm]`) — a capstone linear sweep
that "finds nothing" there is the known constant-fold/data-in-code trap,
decode LDR words manually; (b) several tool one-liners below lost a hex
digit between VA/file-offset — va = file offset + 0x80000000, always.
**Legend:** CONFIRMED = decoded from the cited instructions; INFERRED =
interpretation; OPEN = not resolvable statically.

### 8.1 Engine base — CONFIRMED VA 0xC6034000 (PA 0x20204000)

The DVCDriver PDD object graph (all decoded at 0x803297D0-0x803298CC):

| field | object | ctor | role |
|---|---|---|---|
| this+0xB4 | 0xC5C B | 0x8032C204 | DVCMessi channel container, 64 slots × 0x208 (send 0x8032C1A0) |
| this+0xB8 | 0x374 B | 0x8032C944 | second channel object (send 0x8032C800 — the uploader's path) |
| this+0x310 | 0x90 B | 0x8032E9B8 | **TControllerMessi** (vtable 0x803311D0) |
| this+0xBC | 0x238 B | 0x8032D8C4 | boot-protocol object (vtable 0x80331168) |

`TControllerMessi` creator 0x8032E9B8: `[obj+0] = vtable 0x803311D0`,
**`[obj+4] = 0xC6034000`** (literal @0x8032EE08) — the engine base every
accessor reaches through `[controller+4]`. Its constructor 0x8032F238
binds IRQ **0xC9** (=201; DFC handler = the ISR 0x8032F104) and creates
four 0x5C-byte channel objects at controller+0x7C/0x80/0x84/0x88 via
0x8032FAB8`(size 0x5C, idx 0-3, parent=[controller+4], base 0xC6034200)`.
Per-channel windows: **0xC6034250 + 0x30×ch** (i.e. channel-group window
0xC6034200 with globals at +0x00..+0x4F and channels at +0x50, stride
0x30 — decoded from `add rX, r1, (ch+ch*2)<<4` at 0x8032F060/0x8032FBA8).

PA mapping: VA 0xC603xxxx is the sys-page window — **not** in the icu-timer
§4 bootstrap table (mapped later by the resource manager); its linear PA
is 0x2020xxxx (gpio-static-pinmux §1.1, xti-trace §3 dual view). So:
engine **PA 0x20204000**, group window PA 0x20204200, engine+0x240 →
PA 0x20204240. (Checked against the §4 table entry-by-entry: no
0xC6034xxx VA appears there, consistent with the sys-page rule.)

### 8.2 The four §2.2 candidates — verdict table

| candidate | verdict | evidence |
|---|---|---|
| **0xC6034000** | **CONFIRMED = MeSSI engine base** | `[TControllerMessi+4]` @0x8032E9C8; all accessors go through it (e.g. master ack at +0x10 = 0x8032F208) |
| **0xC6034200** | **CONFIRMED = engine+0x200, the channel-group window** | literal @0x8032EDFC, loaded 9× by the accessor cluster (0x8032F050, 0x8032F2A8, 0x8032F360, 0x8032F468, 0x8032F578, 0x8032F6E8, 0x8032F7D4) |
| **0xC6034240** | **CONFIRMED = engine+0x240, global engine control** | written 0xB5F at init end 0x8032F4B8; bit0 reset pulse 0x8032F688-0x8032F698 (`rmw(0xC6034240,0,1)` then `(…,1,0)`); bit6 set at start 0x8032F73C; bit11 cleared after a 500,000-unit delay 0x8032F750-0x8032F75C |
| **0xC6032664** | **REFUTED — GENIO pad-bank register, not MeSSI** | kernel consumer 0x800BEC84: pad-mux bank driver (4 banks, bases @0x800BEFCC-0x800BEFDC = 0xC6032644/0xC6032664…, stride 0x20; config `[15:5]`=function/`[4:0]`=pin at 0x800BECCC-0x800BED04). vcdriver's copy (pool word @0x803308DC, loaded at 0x803308B8) only *reads* it: `r0=[0xC6032664]; r2=r0&0x1F; … ×0x9600000` — a rate calculation off the pad bank, not an engine access. Negative result recorded per task clause |

Also refuted-by-silence: the **VCHI host library (0x333000-0x33C000)
contains zero MMIO literals** — every 0x20xxxxxx-looking word there is
ASCII. The transport hardware is touched only by vcdriver.dll; VCHI is
pure framing software on top. (CONFIRMED, full-region literal scan.)

### 8.3 Register map (VA 0xC6034xxx; RMW = `*r = (*r & ~clear)|set`, kernel helper 0x800B7BEC with a 16-bit variant 0x800B7C1C)

Engine globals, base 0xC6034000:

| off | VA | R/W | meaning (evidence) |
|---|---|---|---|
| +0x10 | 0xC6034010 | R/W1C | master IRQ status/ack: bits 0-3 = per-channel pending, bits 25/26/27 = global events (ISR 0x8032F104 reads, then W1C-writes the same bits; sub-clears `[base+0x1204]` by read-write-back 0x8032F124-0x8032F130) |
| +0x50 | 0xC6034050 | W | engine config word **0x842** at init (0x8032F45C-0x8032F464) |
| +0x40 | 0xC6034040 | RW | global ctrl: bit19 (0x80000) set at start (0x8032F770), cleared at power-off (0x8032E9B0); bit30 (0x40000000) set/cleared in the power path (0x8032F560-0x8032F574) |
| +0x240 | 0xC6034240 | RW | engine control: init 0xB5F; bit0 = soft reset (pulse); bit6 = start; bit11 = released 500 k-units after start |
| +0x1444 | 0xC6034444 | W | = 6 at start (shared-ring programming, 0x8032F61C) |
| +0x144C | 0xC603444C | W | = ring/descriptor cookie from cell 0xC801059C via 0x8032FE54 → 0x80330554 (0x8032F5F8) |
| +0x1460 | 0xC6034460 | W | = 1 at start (0x8032F60C) |

Channel-group window, base 0xC6034200:

| off | VA | R/W | meaning |
|---|---|---|---|
| +0x04 | 0xC6034204 | RW | clock/divider: bit13 (0x2000) set at init (0x8032F46C-0x8032F478) |
| +0x08 | 0xC6034208 | R | status: **bit0 = busy**; power-down polls it clear with a 10,000-iteration sleep loop (0x8032F380-0x8032F388) |
| +0x0C | 0xC603420C | RW | pair-A (ch0/ch1) event-interrupt **enable** mask (TX-enable ORs the channel's mask \|0x2000000 here, 0x8032FC78) |
| +0x10 | 0xC6034210 | R/W1C | pair-A event **status**: low half = ch0 events (mask 0xFBF), high half = ch1 (0x0FBF0000); ISR re-reads after every ack until quiet (0x8032F064-0x8032F084) |
| +0x14 / +0x18 | 0xC6034214/18 | RW / R/W1C | same pair for ch2/ch3 |

Per-channel window, base 0xC6034250 + 0x30×ch:

| off | meaning (evidence) |
|---|---|
| +0x00 | control/status: init **0xFF01002C** (0x8032FBB4); bits 24-31 = event enables (0xFF), bit16 = W1C ack, **bit17 = busy** (poll loop 0x8032FDB8-0x8032FDF8), bit0 = channel enable / bit1 disable (RMW helper 0x8032FD64) |
| +0x04 | channel tag: init **`(ch<<16) \| 0x3210`** (0x8032FB74-0x8032FB7C) — the 0x3210 magic + channel id in [19:16] |
| +0x18, +0x28 | cleared at TX-enable (0x8032FCD0, 0x8032FCC0) |
| +0x1C | = 0x07B00000 at TX-enable (0x8032FCE4) — watermark/flag word, semantics OPEN |
| (read) +0x00 | 0x8032F7D4 reads `[win]` and **dereferences it** → at runtime the engine writes a message/descriptor pointer into the control word slot — data moves by descriptors/DMA in shared RAM, not by CPU-poked FIFO ports (no data-port access exists anywhere in the cluster; the +0x1444/0x144C/0x1460 globals program the ring). Descriptor layout OPEN (needs runtime capture or VC-side decode) |

Per-channel event bits (channel-object masks, 0x8032F910): each channel
gets ~9 single-bit events — ch0/ch2 in the low half of their pair-status
register (0x800, 0x100, 0x400, 0x80, 0x10, 0x2, 0x20, 0x200, 0x4 → 0xFBF),
ch1/ch3 the same pattern <<16. Channel ids at `[chobj+0x50]`: **1, 2, 4, 8**
(id 1 @0x8032F9F8, id 2 @0x8032FA38, id 4 @0x8032FA80, id 8 @0x8032F9AC).

### 8.4 The VCHI message frame and the CRC

**Message header — 0x1C bytes, then payload** (TX builder 0x80334488-
0x80334508; RX parser/checker 0x80334FF0-0x80335178):

```
 +0x00  u32   service/message type id
 +0x04  u32   payload length            (RX checks it vs actual, "Ill gal XULXBUF" mismatch log)
 +0x08  u32   route/slot tag            (byte table in a kernel data cell @0x357C, indexed by slot — load at 0x80334490)
 +0x0C  u32   CRC-32C of the payload    ← see below
 +0x10  u32   flags / completion id
 +0x14  i16   seq a                     (RX copies payload-relative data when set, 0x803350DC-0x80335104)
 +0x16  u8    flag b                    (+0x17 = 0)
 +0x18  u16   seq c
 +0x1A  u16   seq d
 +0x1C  ...   payload
```

**CRC — CONFIRMED: CRC-32C (Castagnoli), *not* a 16-bit CRC.** The
"MeSSI-16" name does not name the CRC width (the -16 is the SSI link's
16-bit flavor — same naming as the 16-bit RMW kernel helper 0x800B7C1C;
link-vs-frame width otherwise OPEN). Decode chain:

- check site: 0x80335144-0x80335178 — only when
  `[msg->[8]]->[0x68] != 0` (a per-controller **CRC-enable flag**; when
  clear the CRC is skipped entirely), `crc32c(payload=[msg+0xC],
  len=[msg+0x10]) != [hdr+0xC]` → the `VCHI: MeSSI-16 CRC failure`
  log (string @0x80335214, ADR-loaded at 0x80335164).
- CRC function 0x803343EC: table-driven, **init 0xFFFFFFFF**, per byte
  `crc = tbl[(crc ^ b) & 0xFF] ^ (crc >> 8)` — standard reflected form;
  table entries are **32-bit** (`ldr ip, [r3, ip, lsl #2]`).
- table: 256 × u32 at kernel RAM **0xC80105EC** (vcdriver/VCHI data
  cell), generated at boot by 0x803366D4 with per-entry shift/xor loop
  against the literal **0x82F63B78** @0x80336AD4 = the reflected CRC-32C
  polynomial (normal form 0x1EDC6F41). No final XOR, no output
  reflection beyond the table form. (Same algorithm used on the TX side
  at 0x803344AC.)
- Coverage = payload only (header excluded), position = header+0x0C.

### 8.5 Power/clock choreography + EHwClkMeSSI gate

Resource requests at MeSSI init (0x8032F3C4-0x8032F3F8): ids **0xE, 0xD,
4** through the kernel resource manager. Lookup is **1-based**
(`array[id-1]`, 0x800C4470-0x800C4478) and the name table order gives
id 0xE = EHwClkMeSSI, 0xD = EHwClkLoSSI, 4 = EHwClkCamera (binding of
the resource array to name-table order INFERRED; 1-based indexing
CONFIRMED). Kernel.exe itself contains **no** MMIO literals for any
0xC6033xxx clock register (negative result: the resource handler does
not touch the gate) — the physical gating is done by vcdriver:

- power-up (0x8032EB4C / 0x8032EA08):
  `[0xC6033178] = 0x20` (bit 5) and `[0xC6033158] = 0x80` — **the
  EHwClkMeSSI gate pair, PA 0x20203178 bit5 + PA 0x20203158**;
  padmux two-phase `[0xC60319E8] = 0x00040100 → 0x00020080`; staged
  `[0xC6033008] |= {0x800|4}` → delay 31,250 units → `|= {0x400|2}` →
  delay → `|= {0x200|1}`; reset pulse `RMW(0xC6033038, ~0xC0 → 0xC0)`;
  idle-wait on [0xC6034208].bit0; then engine/channel init (§8.3).
- power-down (0x8032F8DC + 0x8032F7FC): disable channels, unbind IRQ
  0xC9, wait idle, staged `[0xC6033008] &= ~2 → ~1 → ~4` (delays),
  pad deconfig `[0xC60319E8]=0x100, [0xC60319E4]=0x10000,
  [0xC60319D4]=0x800, [0xC60319E4]=0x80`.
- MeSSI pin group = padmux rows 0xC60319D4/0xC60319E4/0xC60319E8
  (PA 0x202019D4/19E4/19E8) — extends §2.2's pad-mux footprint.

### 8.6 Reproducibility

LDR-literal scans (manual word decode, both U directions), manual BL
decoding (capstone sweeps stop in data), capstone view via
`tools/task114_dis.py <off> <len>`; all addresses va (subtract
0x80000000 for file offsets). Key functions: TControllerMessi creator
0x8032E9B8, ctor 0x8032F238, ISR 0x8032F104, ISR demux 0x8032F02C,
hw-init 0x8032F3AC, channel ctor 0x8032FAB8, channel hw-init 0x8032FB64,
TX-enable 0x8032FC54, start 0x8032F6E0, clock stop 0x8032F7FC, VCHI
TX-header builder 0x8033441C-0x80334510, RX check 0x80334FF0-0x80335178,
CRC 0x803343EC, CRC-table gen 0x803366D4.

### 8.7 What this hands step 3 (DVCDriver boot protocol)

- The transport the boot messages ride is now fully located: uploads go
  `uploader 0x8032DB78 → [this+0xB8] container → send 0x8032C800` (stage
  1/2 blobs) and `0x8032DAA0 → send 0x8032C1A0` (img + 20-byte signature
  + `(x & 0x00FFFFFF)|0x02000000` destination word); the messages
  themselves are §8.4 frames with the payload grammar anchored by the
  command byte 0x42.
- Boot-protocol object at this+0xBC (ctor 0x8032D8C4): vtable 0x80331168,
  per-stage timeout cells = 0x3E8 (1000) at +0x20, 4 × 0x24-byte reply
  slots at +0x1B0 — matches the 1000 ms × 5 retry wait helper
  0x803294B4 (thunk → kernel 0x8009EC28); BootParseMessage 0x8032D9EC
  parses into these slots.
- IRQ 0xC9 / ISR 0x8032F104 is where boot-ROM replies surface (HAT
  events ride the master-ack bits 0-3 + global bits 25-27 at engine+0x10).
- The engine-control register 0xC6034240 (init 0xB5F, bit0 reset, bit6
  start, bit11 release) is the bring-up lever any emulator/Linux driver
  must reproduce before channel 0 talks; the per-channel tag word
  `(ch<<16)|0x3210` at window+0x04 is the wire-side channel id to match
  against VC-side decode.
- OPEN for step 3/4: shared-RAM descriptor layout behind
  engine+0x1444/0x144C/0x1460 (runtime capture or VC-side decode);
  semantics of TX watermark 0x07B00000 and engine word 0x842.

## 9. DVCDriver boot protocol — the exchange grammar that brings the VC up (task 119, 2026-09-03)

**Method:** static ARM disassembly (capstone via `tools/task114_dis.py`,
va base 0x80000000) of `DVCDriver::Boot()` 0x8032DCB0 and its full callee
graph in vcdriver.dll (ROM 0x3290D0+): uploader 0x8032DB78, image upload
0x8032DAA0, BootParseMessage 0x8032D9EC, wait helper 0x803294B4 → kernel
0x8009EC28, the channel sends 0x8032C800/0x8032C1A0 → 0x8032BEBC, the boot
object 0x8032D8C4 (this+0xBC) with its slot pump 0x8032D274/dispatcher
0x8032D12C, the completion ISR chain 0x8032C5C4, and the HAT reader
0x8032CE3C. Literal pools decoded manually (several are backward
`ldr rX,[pc,#-imm]`, the §8.6 trap). vcdriver's E32/ROM header at file
0x3290D0 decoded for the writable-cell layout. **Legend:** CONFIRMED =
decoded from the cited instructions; INFERRED = interpretation; OPEN =
not resolvable statically.

**Headline results**

1. The boot exchanges are **0x20-byte magic-framed slots**: word 0 =
   **0xAFE45251**, word 7 (offset 0x1C) = **0xF001BC4E**, with a
   `{count<<24 | 24-bit answer}` response word at +0x04. The same framing
   wraps the APE→VC image-upload header — the magics delimit every boot
   message in both directions.
2. The stage "uploads" are **not uploads**: the uploader posts the cell as
   a receive/exchange buffer with its descriptor length explicitly zeroed
   (`SetLength(desc,0)` at 0x8032DB90). The only APE→VC payload traffic in
   the whole boot is the two **command-0x42 image uploads** (loader +
   firmware), each = image bytes + a 0x20-byte header carrying
   `(len&0x00FFFFFF)|0x02000000` and the 20-byte signature.
3. Transport: **one physical engine** (TControllerMessi, 0xC6034000, IRQ
   0xC9) serves both paths — the small-message channel object this+0xB8
   (stage exchanges) and the scatter-gather bulk container this+0xB4
   (image data, 8 KB chunking, MMU-translated descriptors). Neither B4/B8
   code touches engine MMIO (literal scan: only supplies/padmux/clock in
   0x329600-0x32C800).
4. The stage-blob OPEN of §7.4 is **resolved by refutation**: the cells
   0xC801055C/0xC801057C sit in vcdriver's **BSS** (data cell
   0xC8010500+0x38 initialized from ROM 0x331370 — zeros; BSS 0x98 B covers
   0xC8010538-0xC80105D0). They are pure runtime exchange buffers; the VC
   boot ROM's replies are the first nonzero content they ever hold.

### 9.1 The sending paths and their descriptors (CONFIRMED)

- **this+0xB8** (0x374 B, ctor 0x8032C944 — the DVCMessiChannel family;
  it also acquires the **HAT pin** at ctor time, strings 0x32CD2C-0x32CD6C):
  a message ring of 2 slots × 0x18C with 0x18-byte entries at slot+0x28.
  `send 0x8032C800(ch, slot, msgDesc, pCount, tag)` writes per entry:
  `[e+0] = slot|tag`, `[e+4..+0xF] = {type|len, maxlen, ptr}` (the message
  descriptor, kern helper 0x800AF434 shape), `[e+0x10] = 0`,
  `[e+0x14] = pCount`. Ring full → −18 (`0x8032C84C`).
- **this+0xB4** (0xC5C B, ctor 0x8032C204, sub-object vtable 0x803311AC):
  the bulk path — 2 slots × 0x618, each with a 64×0x18 descriptor ring;
  `send 0x8032C1A0 → 0x8032BEBC` walks the caller's buffer pages
  (translation helpers 0x80329374/0x80330B30), merges contiguous ranges
  (max 0x20000), and defaults to **0x2000 (8 KB)** chunks — the loader-size
  granularity. This is the CCP2/bulk class of §2.2 by behavior; no separate
  CCP2 engine MMIO exists in vcdriver.
- Both channel containers hold the shared 0x188-B low-level driver
  **[this+0x30C]** (ctor 0x8032A9FC, bound to IRQ **[this+0xEC]**, vtable
  0x8033118C) which owns the power/clock choreography (supply pair
  0xC60248xx/0xC60258xx, padmux 0xC60318E0-958, 0xC6033038) — the
  TControllerMessi engine cluster (§8) does the wire.

### 9.2 The 0x20-byte exchange frame (CONFIRMED)

```
 +0x00  u32  0xAFE45251                     magic A (both directions)
 +0x04  u32  response word:
          TX (image header): (payloadLen & 0x00FFFFFF) | 0x02000000
          RX (reply):        count << 24 | answer (24-bit)
 +0x08  u32  RX: image count (consumed when response == 0x01000001)
        TX: first word of the 20-byte signature ride-along
 +0x0C  20 B signature bytes ..+0x1B (image header TX only)
 +0x1C  u32  0xF001BC4E                     magic B (both directions)
```

- Reply validation (BootParseMessage 0x8032D9EC): requires
  `[cell+0]==0xAFE45251` **and** `[cell+0x1C]==0xF001BC4E`, else
  "Unrecognised VideoCore response" + two `%08x %08x %08x %08x` hex-dump
  lines (0x32DBC0/0x32DBFC) and return −44.
- `count = [cell+4]>>24`, `answer = [cell+4]&0x00FFFFFF` (0x8032DA4C-0x8032DA64).
- **Image-ID negotiation:** if the reply word is exactly **0x01000001**
  (count=1, answer=1) and the requested-count sentinel
  `[this+0x31C]==0x7FFFFFFF`, then `[this+0x31C] := [cell+8]` and the
  pending request at this+0x320 completes (0x8032DA5C-0x8032DA8C) — the VC
  tells the host how many images to feed it. The sentinel is posted at
  DVCDriver construct (0x803298BC, `0x7FFFFFFF`); 0x8032A5F8 also keys the
  fast/slow clock-mode choice off it (resource ids {0xE,2,8} vs {0x11,6,0xB}).
- Image-upload header template: ROM 0x330E40 (0x20 B) = magic A + 5 zeros +
  magic B; 0x8032DB00-0x8032DB2C stamps the length word and copies the
  signature into +0x08.

### 9.3 Boot() state machine (CONFIRMED; anchors = the print sites)

`Boot(this, imgSel, &answer)` — imgSel r8=0 → set+4 (img1/loader-A), ≠0 →
set+0x10 (img2/loader-B); `*answer=1` only on full success (0x8032E114).

```
  set transport clock mode 1 (0x8032A5F8: mode word [drv+0x28]=0x32,
  resource ids keyed off [this+0x31C])
  copy cellA(0x20)→stack, cellB(0x20)→stack        [cells are BSS-zero, §9.8]
  arm RX: 0x8032BE40(B4, cb 0x8032DC94, this, irq[this+0xEC])
          0x8032C788(B8, cb, this, irq)            [binds B8 data ISR 0x8032C5C4]
          0x8032C908(B8)                            [prime]
  ── STAGE 1 ─────────────────────────────────────────────────────────
  upload cellA (uploader 0x8032DB78 → send 0x8032C800(B8,slot0,desc,pCount,tag=2))
  publish display-power property =1 (0x8032958C, handle [this+0xC0])
  wait(pCount, 5 ms × 100 = 500 ms)                 [0x803294B4 → 0x8009EC28]
   ├ timeout → "VideoCore boot ROM not responding (stage 1); HAT=%d,
   │            iMessiReadDfc.Queued()=%d" (0x330E60) → FAIL −33
   └ ok → BootParseMessage(cellA, &count, &answer)
       ├ parse≠0 → "BootParseMessage failed %d" (0x32DE90) → FAIL
       ├ count≠1 → "Unexpected response from boot ROM %u %u" (0x32DEC0) → FAIL −44
       └ count==1 (reply may also set [this+0x31C] := [cell+8], §9.2)
  registry init (0x803294BC → 0x8022E138) + GetImages([this+0x31C], &set)
   └ set==0 → "no images returned from DVCExe" (0x32DEFC) → FAIL −39
  ── STAGE 2 ─────────────────────────────────────────────────────────
  upload cellA (pCount→sp+0xC); upload cellB (pCount→sp+0x14)
  ImageUpload(set+4|set+0x10, 0x8032DAA0, cmd 0x42, internal 5 s wait)
  wait(sp+0xC, 500 ms) → parse(cellA)
   ├ timeout → "...not responding (stage 2); HAT=%d..." (0x330EC4) → FAIL −33
   ├ parse≠0 → "BootParseMessage returned an error (stage 2)" (0x32E1BC) → FAIL −44
   ├ count≠3 → "Unexpected response after sending 2nd stage %u %u" (0x330F28) → FAIL −44
   └ answer≠1 → FAIL −46
  wait(sp+0x14, 500 ms) → parse(cellB); fail (parse≠0 or count≠1) → silent FAIL
  ── MAIN FIRMWARE ───────────────────────────────────────────────────
  scan img3's last 0x100 bytes for SCCS tag "@(#)" (0x40 28 23 29,
  0x8032DC18-0x8032DC74) → "VideoCore image ID found: %s" (0x32DE4C) /
  "not found" (0x32DE24)                                [logging only]
  upload cellA (pCount→sp+8); ImageUpload(set+0x1C img3, cmd 0x42, 5 s wait)
  wait(sp+8, 500 ms) → parse(cellA): count==3 && answer==1
   └ ok → *answer=1  SUCCESS      (any failure here is silent)
  exit: cancel B4 RX arm (0x8032E11C+), publish power property 0
```

All failures converge on `"Failed(%d) to start 2nd stage boot loader"`
(0x32E200, print 0x8032E030) then power-state(this,0). **No retries
anywhere** — one shot per phase, fresh postings each time.

### 9.4 Message tables (per exchange; §8.4 frame fields carry these payloads)

| # | dir | transport | payload | reply check | budget |
|---|-----|-----------|---------|-------------|--------|
| 1 | APE posts cellA | B8 slot0, tag 2, len 0 (receive-post) | — (cell = exchange buffer) | count==1; 0x01000001 → adopt [cell+8] | 500 ms |
| 2 | APE posts cellA, cellB | B8 slot0, tag 2 | — | (their pCounts gate phases 3/4) | — |
| 3 | APE→VC loader image | B4 slot1, cmd 0x42 | img bytes, len roundup16(imgLen)=0x2060 | — | 5 s internal |
| 4 | APE→VC loader header | B4 slot0, cmd 0x42, pCount | 0x20 B frame: {0xAFE45251, (0x2050)\|0x02000000, sig20, 0xF001BC4E} | reply into cellA: count==3 && answer==1 | 500 ms |
| 5 | APE posts cellB | B8 slot0, tag 2 | — | reply into cellB: count==1 | 500 ms |
| 6 | APE→VC firmware (img3) | B4 slot1, cmd 0x42 | img3 bytes (0xF6A2B) | — | 5 s internal |
| 7 | APE→VC firmware hdr | B4 slot0, cmd 0x42, pCount | {0xAFE45251, (0xF6A2B)\|0x02000000, sig20, 0xF001BC4E} | reply into cellA: count==3 && answer==1 → boot OK | 500 ms |

Interpretation notes: the `0x02` class tag in the length word mirrors the
reply word's `{count<<24|answer}` grammar (class/count in the top byte,
24-bit value below) — INFERRED: class 0x02 = "data length"; reply counts
1 = simple ack, 3 = tri-part state (loader running / ready for firmware /
final) — semantics beyond the equality checks OPEN. The tag-2 vs 0x42
selects B8-message vs B4-bulk framing; on the wire both are §8.4 VCHI
frames (0x1C-byte header + payload + CRC-32C).

### 9.5 How replies surface (CONFIRMED)

1. Engine IRQ **0xC9** → ISR 0x8032F104 → demux 0x8032F02C (per-channel
   masks, §8.3) → channel event handlers (0x8032F010/0x8032ECF4).
2. The B8 receive pump (0x8032CC40-0x8032CE24 region) fills the current RX
   ring entry's descriptor in place, sets its length word bit 3, and
   completes the entry.
3. The B8 completion ISR 0x8032C5C4 (bound at RX-arm to IRQ [this+0xEC])
   drains completed entries and calls the registered callback with
   status **0x2A00** (RX-filled; 0xA00 without the direction bit).
4. Boot's callback 0x8032DC94: `if (status & 0x201) *pCount = 1` —
   0x2A00&0x201=0x200 ✓, 0xA00&0x201=0 ✗ — only replies complete the wait.
5. The wait helper (kernel 0x8009EC28) polls `*pCount` every **5 ms**
   (0x80064028 ms→ticks of the r2 arg) up to r3 iterations → −33
   (KErrTimedOut). NB this corrects §7.6's "1000 ms × 5": the 1000 is
   iteration count in the image-upload wait (5 s budget); Boot's reply
   waits are 5 ms × 100 = 500 ms; the boot object also carries a separate
   1000 ms drain budget (ctor +0x20).
6. Post-boot path (step 4 inherits): the boot object's **4 logical slots**
   (bootobj+0x68 + i×0x6C, i<4) get per-slot event tallies from three
   TControllerMessi callbacks registered by Start 0x8032D3D0 via
   0x8032F7EC (cb 0x8032CEC8/0x8032CEA4/0x8032CE80 → [slot+0x40/44/48]);
   the slot DFC 0x8032D274 dequeues {len,ptr,extra} (0x8032D23C, counter
   cell 0xC801052C) and dispatches via 0x8032D12C to the consumer
   callback with status **0x4200** and a message descriptor (8 KB max,
   pinned via 0x8032947C). Sends on the same slots: export 0x8032D6B0.
   Start/Stop (0x8032D3D0/0x8032D494) and the B4/B8 RX-arms are exported
   through the thunk table at ROM 0x3316E0-0x33171C — i.e. driven from
   outside vcdriver.dll (the caller of Boot). The attention IRQ also
   powers MeSSI up on demand (handler 0x8032CFEC → 0x8032EA6C = IRQ 0xC9
   claim + clock ids 0xE/0xD).

### 9.6 HAT (CONFIRMED mechanism, VC-side polarity OPEN)

HAT is the **VC→AP attention GPIO** (§3's diagram). The B8 ctor acquires
the pin, wraps it and binds its ISR (prints 0x32CD2C/0x32CD48/0x32CD6C at
0x8032C990/0x8032C9B8/0x8032C9F0); the ISR latches the pin state at
**[B8+0x20]**. `0x8032CE3C(obj) = [obj+0x20]` (safe-read thunk) is what
Boot prints as `HAT=%d` on stage timeouts, alongside
`iMessiReadDfc.Queued()=%d` = byte [this+0xD6] forced to 0/1. So the two
timeout diagnostics are "attention line level" + "is a read DFC pending".

### 9.7 Timeout/retry semantics (CONFIRMED)

- Reply waits: 5 ms poll × 100 = **500 ms**; image-upload wait: 5 ms ×
  1000 = **5 s**; boot-object drain budget 1000 ms (ctor). One attempt per
  phase; no re-send loops. Errors: −6 null image ptr, −18 ring full, −33
  timeout, −39 no images, −44 parse/unexpected-response, −46 answer≠1.

### 9.8 The stage cells 0xC801055C/0xC801057C — resolved (CONFIRMED)

vcdriver.dll's writable cell (descriptor at ROM 0x329130): base RAM
**0xC8010500**, initialized data 0x38 B (ROM copy 0x331370: all zero
except 0xC8010504=0x400, 0xC8010508=0x64), **BSS 0x98 B →
0xC8010538-0xC80105D0**. The stage cells (0x55C/0x57C), the reply-report
cell 0xC8010550 (r2 arg of the boot-object IRQ-handler callback), the
message counter 0xC801052C and the VCHI shared-ring cookie 0xC801059C (§8.3)
all live in that BSS. Consequences:

- The "initial values" question is answered: **statically zero** — the
  §7.4 hypotheses "kernel-build relocated constants" / "pattern-scanned
  initializers missed" are refuted; nothing outside this dump needs to
  supply them.
- Structurally the first three words act as a kern descriptor
  `{type|len, maxlen, ptr}` when posted (uploader reads cell words 0-2 and
  zeroes the length field); after the exchange the same 0x20 bytes hold
  the magic-framed reply. I.e. **cell = exchange slot, request phase
  empty** — consistent with a master VC boot ROM that speaks first once
  powered/attentioned, and with "not responding (stage N)" being simply
  "no reply arrived".
- OPEN (runtime only): the exact DMA landing of the reply bytes (posted
  descriptor buffer vs the shared-RAM pool at 0xC8010534+) — the engine's
  descriptor/DMA machinery (§8.3 OPEN) moves the data; no APE code writes
  the cells (ROM-wide literal scan: only 0x32DE84/0x32DE88 reference them).

### 9.9 Corrections to earlier sections

- §7.4 step 1 ("copies two 0x20-byte blobs … and uploads them as boot
  stages 1-2") is reframed: the cells are posted as **receive/exchange
  buffers** (length zeroed), not transmitted blobs; the only TX payloads
  are the 0x42 image uploads. §7.4 step 3's "(x&0xFFFFFF)|0x02000000
  destination word" is the header's length word (x = image length), class
  tag 0x02 — not an address.
- §7.6's "wait helper (1000 ms × 5 retries)" → 5 ms poll interval × N
  (§9.7); "4×0x24 reply slots at +0x1B0" → 4×0x6C logical slots at
  bootobj+0x68 (+0x1B0 of §8.7 falls inside slot 3, which spans
  +0x1AC-+0x218).
- §7.4's "VideoCore image ID found/not found" prints belong to a
  pre-upload **SCCS "@(#)" version-tag scan** of img3 (0x8032DC18), not to
  an image-ID protocol message; the protocol-level negotiation is the
  0x01000001 reply case of §9.2.  *(Re-verification 2026-09-04: the
  scan's pattern is built from byte immediates — `cmp #0x40/#0x28/
  #0x23/#0x29` = '@','(','#',')' at 0x8032DC54-0x8032DC6C, backward
  `ldrb [r4,#-1]!` — which is why the byte string "@(#)" appears
  nowhere as data in the ROM; only two user-side copies exist at
  0x80F6498A/0x80F84D52.  Corroboration: the magic pair
  0xAFE45251/0xF001BC4E also occurs inside signed_2727b1_v2.dll's
  span at 0x8022ED98/0x8022EFFC/0x80230E08/0x8023106C — the framing
  family is shared.)*

### 9.10 Reproducibility

Capstone sweeps `tools/task114_dis.py <off> <len>` (va = off+0x80000000)
over: 0x32DCB0 (Boot), 0x32DB78/0x32DAA0 (uploaders), 0x32D9EC
(BootParseMessage), 0x32C800/0x32C1A0/0x32BEBC (sends), 0x32C5C4/0x32D274/
0x32D12C/0x32D3D0 (completion/dispatch/Start), 0x32CFEC/0x32EA6C
(attention/power); kernel side 0x09EC28 (wait), 0x064028 (ms→ticks),
0x0AF434/0x0AF1A0/0x06032C (descriptor helpers). Manual word decode of
literal pools: 0x32DE80-0x32DE8C, 0x32E1B8-0x32E240, 0x32DC10-0x32DC18
(special-response 0x01000001, template ptr 0x80330E40), header template at
0x330E40, thunk tables 0x329364-0x3295A0 and 0x3316E0-0x33171C, interface
table 0x331240-0x33128C. ROM header/data-cell: file 0x3290D0 (+0x24 data
size 0x38, +0x28 bss 0x98, data copy @0x331370), cell descriptor 0x329130.
String anchors: 0x32DBC0/0x32DBFC, 0x32DE24/0x32DE4C/0x32DE70,
0x32DE90/0x32DEC0/0x32DEFC, 0x32E1BC/0x32E200, 0x330E60/0x330EC4/0x330F28,
0x32CD2C-0x32CD6C.

### 9.11 What this hands step 4 (VCHI grammar)

- The post-boot message channel is the boot object's 4 logical slots:
  consumer callback + status 0x4200 + descriptor {len,ptr,extra} per
  message (0x8032D12C/0x8032D23C); sends via export 0x8032D6B0; slot
  tallies fed by the three TControllerMessi callbacks (0x8032CEC8/A4/80).
  Step 4 should decode the consumer registered through Start (the caller
  side of the 0x3316E0 thunk table) — that is where dispcontrol/dispmanx
  messages will surface first.
- Every boot frame rides §8.4's VCHI header (0x1C B + payload + CRC-32C at
  +0x0C); the magic pair 0xAFE45251/0xF001BC4E is boot-protocol-specific
  and should NOT be expected in post-boot service messages.
- Engine-side: channel enable/tag (§8.1) + the shared-ring cookie cell
  0xC801059C ("VCHI" magic adjacent at 0x32FF38 pool) are the wire-side
  correlators; the reply DMA-landing question (§9.8 OPEN) and the
  engine+0x1444/0x144C/0x1460 descriptor layout remain the two runtime
  captures that would confirm the transport end-to-end.

## 10. VCHI display-service grammar — the client-writable protocol (task 120, 2026-09-04)

**Method:** static ARM disassembly (capstone via `tools/task114_dis.py`,
ARM kernel-side / Thumb user-side; manual literal-pool decode per §8.6)
of every display-path module, located by a ROM-directory walk (§7's
format): kernel side vcdriver.dll 0x3290D0, vchi.dll 0x3313B0,
dvcrmiservice.dll 0x339380, vcos.dll 0x33AA70, vcbufmanx.dll 0x33BEC0,
dispcommanddrv.dll 0x340FD0, tv_out.dll 0x342EE0, vcdispmanvchi.dll
0x345D20, the DVCDispman/DVCDispmanX class image 0x346780 (not in the
directory walk's filtered output), vcdispmanupdatehandler.dll 0x347640,
vcdispmanldd.ldd 0x428740; user side rdispman.dll 0xD5DFC0,
vcbackendserver.dll 0xD50C10 (the Gce backend), vcendpointgceclient.dll
0xD5D930. Cross-module linkage was reconstructed from **import veneers**
— XIP ROM DLLs pre-bind imports as `ldr pc,[pc,#-4]; .word <target VA>`
pairs, so a region scan of those pairs yields the complete who-calls-whom
(no static word-refs to exports exist; kernel import cells are RAM,
§9.10's loader reality). **Legend:** CONFIRMED = decoded from the cited
instructions; CONTRACT = shape fixed by cross-checked paths, exact
semantics pending; OPEN = not resolvable statically.

**Headline results**

1. **The §9.11 Start-consumer is vchi.dll.** Its power-up 0x80337540-0x803375B8
   arms both RX paths (B8 small-message 0x8032C788 with cb 0x80337508, B4
   bulk 0x8032BDE4 with cb 0x80337510) and then calls **Start
   (0x8032D3D0) with consumer callback 0x803370B0** — a veneer on the
   dispatcher 0x80336FB0, which routes on the §9.11 status word (bit
   0x200 = message-received) and slot index to the connection's RX queues
   at conn+0x2C00+0xC4+8×slot. TX leaves through export **0x8032D6B0**
   with slot = `(([elem+0x1C] & 0xFF00) >> 8) - 2` (0x8033560C-0x80335628)
   — the message's route nibble selects the boot object's logical slot.
   (A second Start at 0x80337F84 registers consumer from pool
   0x80338B3C for the same connection object.)
2. **vchi.dll is the generic VCHI host library** (not display-specific):
   connection singleton 0x80335BD8 (0x3580-B object, +0x30 state, +0x68/+0x74
   counters, +0xAC connection id, +0xF8 control-service handle, +0x2C00 RX
   queues, +0x3000 service array with count at +0x3574, ≤0x20 services);
   service open 0x80337C74; send 0x803343B4 → 0x80332D50; blocking receive
   0x80338864 → 0x803381E4; event pump/callback entry 0x80336020
   (vchi_msg_hold-style peek), 0x80335FEC (release), 0x803389E8/0x80338A00
   (callback re-arm pair).
3. **The wire has two nesting levels.** Per-message: a 0x10-B record built
   at 0x80332F7C-0x80332FC4 into the TX-slot buffer
   `{u16 0, u16 total_len, u32 fourcc, u32 0, u16 0, u16 payload_len}`
   followed by the payload copied from the caller's iovec list. Around the
   TX batch: §8.4's 0x1C-B frame — its +0x00 "service/message type id" is
   now pinned as the **destination service fourcc** (service object +0xC,
   copied at open from request+0; TX builder 0x80334488-0x80334498, RX
   checker 0x80334FF0+). The boot magics of §9.2 appear nowhere here
   (§9.11's prediction confirmed).
4. **Dispmanx payloads are `{u32 message_id, args…}` with STATIC ids**
   (u32, 2-0x26), built in user space by rdispman.dll and passed through
   the kernel unmodified — the kernel client never invents message ids.
   The stop-clause (runtime-assigned ids) does not apply.
5. **All composition traffic is short-message.** Every vcdispmanvchi send
   passes flags 0: 0x80332D50 rejects >0x1000 B without flag bit3
   (−0x28) — no dispmanx op uses the bulk path; bulk data (surfaces) is
   the buffer manager's job (vcbufmanx, "BUFM"/"XMFB"). The 0x42-command
   scatter-gather bulk of §9 is boot-only.

### 10.1 The module chain (CONFIRMED; import-veneer graph)

```
 user:  vcbackendserver.dll (Gce: CVCCompositionBackendServer/Session,
          CVCLayer*/ComposeLayer, CVCCompositionUpdateMgr, CVCRegisteredSurface)
          |  loads "dispman logical device" ( ConstructL error string 0xD59D44)
          v  imports rdispman.dll ordinals 1 (status mapper 0x80D5E8BA),
          5 (update submit 0x80D5E23A) and 6 (element op 0x80D5E402) only
        rdispman.dll — the user-side dispmanx API: builds {msgid,args},
          talks to the LDD (DoControl/DoRequest)
          v  RBusLogicalChannel "vcdispman" (LDD name string 0x4298F4)
 kernel: vcdispmanldd.ldd 0x428740 (DDispmanLddChannel/Factory; 20-case
          request jump table 0x80428E28-0x80428E7C + async dispatch
          0x804290C0-0x804290F8)
          v  singletons: DVCDispman/DVCDispmanX image 0x346780 (67 exports,
          classes VCDispman::DVCDispman/DVCDispmanX, typeinfo 0x347473+)
        vcdispmanvchi.dll 0x345D20 (12 exports) — DVCDispmanVCHI: opens the
          two display services, marshals sends, owns the receive callbacks
          + vcdispmanupdatehandler.dll 0x347640 (DVCDispmanUpdateList,
          10 exports) — update-handle bookkeeping + completion muxes
          + dispcommanddrv.dll 0x340FD0 ("DSPC") + tv_out.dll 0x342EE0
        vchi.dll 0x3313B0 — VCHI host library (connection, services, framing)
          v  import-veneer block 0x80331474-0x80331734 (kernel services +
            vcdriver Start/Stop/sends/RX-arms)
        vcdriver.dll 0x3290D0 — §8/§9 transport + boot object (4 slots)
```

vcdispmanldd channel object: +0x2C DVCDispman client (vcdispmanvchi
0x803463F8 get/create, 0xC-byte object: vtable 0x80346708, +8 mutex
"DVCDispmanVCHI: VCHI Host Lock", created 0x80346360), +0x30 update
handler, +0x34 dispcommand client. LDD factory strings 0x429808/
0x429824 = the two completion muxes (§10.6).

### 10.2 Service enumeration (fourcc table)

| fourcc (LE u32) | ARM-side opener | VC-side anchor | role |
|---|---|---|---|
| **"DISP"** 0x50534944 | vcdispmanvchi (cell 0xC80121DC, written by the DLL-load initializer 0x80346504; open 0x8034614C→0x803461AC with cb 0x80345F8C) | dispc_q @0x2E7EFC; server strings **DISPCONT** @0x2E7F04 / **DISPCOND** @0x2E7F10; VLL-name-table hit 0x317488; `vchi_dispcontrol_server_init` @0x31A3E8, `vchi_dispservice_x_init` @0x31A45B (§2.1) | **the dispmanx command service** — every {msgid,args} of §10.5 rides it, replies too |
| **"UPDH"** 0x48445055 | vcdispmanvchi (cell 0xC80121E0, same initializer; cb 0x803460AC) | no VC-side string hit (handled inside the same server, no name-table entry) | **update-callback channel** — VC→AP update-completion notifications, drained into DVCDispmanUpdateList |
| **"DSPC"** 0x43505344 | dispcommanddrv (const 0x80342AF8; open 0x8034123C) | — (dispcontrol family) | display-command service: the LDD's non-message display ops (LDD case 0xE/0xF, §10.4) |
| **"BUFM"** 0x4D465542, **"XMFB"** 0x42464D58 | vcbufmanx (0x8033D4EC / 0x8033EE6C) | 0x310B88 / 0x253F30 (VLL tables) | buffer manager — surface/buffer transfer (bulk side; out of scope, feeds element resources) |
| **"CRMI"** 0x494D5243 | dvcrmiservice (0x80339B88) | — | RMI service (resource/power mgmt interface; also imports vcdriver directly: 0x8032E774/0x8032E6AC/0x8032E308) |
| **"TVOUT"** (VC name) | tv_out.dll (two connections obj+0x26C/+0x174; **ARM-side fourcc not statically pinned — OPEN**) | string **TVOUT** @0x2E8B14; `tvout.vll` @0x304ADB, `tvout.c` @0x2D70F4 (§2.1) | TV/HDMI out control |
| ("KHRN" 0x4E52484B) | khegl_ldd (cross-reference only — OUT of scope per task) | 0x310D28 | GLES/EGL — surface allocation rides it (backend strings "Videocore surface allocation failed (EGL surface)") |

vcdispmanvchi's DLL-load initializer (init list at 0x80346710 → 0x80346504)
writes the two fourccs into its data cells before any open; both opens use
vchi 0x80337C74 with a 0x24-B request `{fourcc, callback, ctx, &out_handle,
…}` (vcdispmanvchi builder 0x8034614C). The service object (0xB8 B, alloc
0x80337D00) carries the fourcc at +0xC — the value that lands in both wire
headers. "DISP" matches the Raspberry-Pi dispmanx service fourcc — same
software family, ids differ (§10.5 note).

### 10.3 Connection setup (the post-boot handshake, CONFIRMED shapes)

1. Connection singleton: first vchi API use (e.g. vcdispmanvchi open,
   0x80346174 → 0x80335BD8) allocates the 0x3580-B connection object.
2. `vchi_connect` gate 0x80336DE8 (state check only) then the connect
   message builder 0x80336E10: sends a **0x14-B control message
   `{0x00000000, 0x00000001, 0x00001010, conn_id, 0x00000800}`** through
   0x80333100 (control-type constant 0x00100004, pool 0x80337170) on the
   control handle conn+0xF8 — i.e. {zeros, version=1, ver 0x1010,
   runtime connection id, max message 0x800}. The peer-string "VCHI peer
   s…" sits in the adjacent pool (0x80337174).
3. Power-up 0x80337540: arm B8/B4 RX + **Start with consumer 0x803370B0**
   (headline 1) — from here the §9.11 4 logical slots deliver VCHI RX.
4. Service open per §10.2; the client's callback fires reason 8
   (connected) → vcdispmanvchi stores the service handle at 0xC80121D0
   (0x80345FD8) and sends become possible; reason 1 = message available.
5. Per send: 0x80346050 builds iovec `{{&msgid, 4}, {payload, len}}` →
   vchi 0x803343B4 → 0x80332D50 (alloc TX slot via 0x80332B34, emit the
   0x10-B record, iovec-copy, queue). Send+wait (0x803462E8 / flagged
   0x80346258) then blocks in vchi 0x80338864 receiving a **4-B reply**
   (status/handle) on the same service.

The element of §8.4's frame worth restating for a host implementation:
+0x00 fourcc, +0x04 payload length, +0x08 route byte (connection-indexed
kernel table, §8.4), +0x0C CRC-32C of payload, +0x10 flags/completion,
+0x14/+0x16/+0x18/+0x1A sequence fields from the message record — the
record fields [msg+0x18]/[msg+0x1C]/[msg+0x20] feed them (0x80332F4C-0x80332F78).

### 10.4 The user↔kernel API: vcdispmanldd request table (CONFIRMED)

Sync (RBusLogicalChannel::DoControl N, dispatch 0x80428DFC, jump table
0x80428E30-0x80428E7C; r6/r8 = arg/ret descriptors, −4 = unsupported):

| N | handler | target | shape |
|---|---|---|---|
| 0 | 0x80428F2C | vcdispmanvchi 0x80346258 | send: user desc {msgid, buf, len} (0xC B copied in, buf pinned); waits iff msgid&0x80000000 |
| 1 | 0x80428F2C | vcdispmanvchi 0x803462E8 | send + always wait, 4-B reply out |
| 3 | 0x80428FD8 | updatehandler 0x80347848 | returns [uh+8] (update counter), 4 B out |
| 4 | 0x80429000 | updatehandler 0x80347890 then 0x803479E4 | create/track update (kernel side of msg 0x10) |
| 5 | 0x8042901C | updatehandler 0x80347B88 | display-update arm (a2,a3) |
| 7 | 0x80429030 | vcdispmanvchi 0x80345EEC | close services |
| 8 | 0x80429044 | updatehandler 0x80347D44 | (a2) |
| 9 | 0x80429054 | tv_out 0x803440EC | (a2) |
| 0xA | 0x80428E80 | named-handle handshake | returns 4-B handle (looks up the updatehandler's lists) |
| 0xE | 0x8042907C | kernel 0x8005FFB8 via dispcommand obj | (a2, &out) 4 B out |
| 0xF | 0x80429064 | dispcommanddrv 0x80342ACC | &out, 4 B out |
| 0x12 | 0x804290A8 | updatehandler 0x80347890 | as 4 |
| 0x13 | 0x8042903C | updatehandler 0x80347C7C | (a2) |
| 2,6,0xB-0xD,0x10,0x11 | 0x804290B4 | — | −4 |

Async (DoRequest N → r1<0, `mvn r6,r1`, 0x804290C0-0x804290F8): **6** →
0x80429518 (update-completion notify — pairs with msg 0x11 submit);
**0xB, 0xC, 0x10, 0x11** → 0x804291EC (frame-count/buffer-available
notifiers, 0x20-B request structs from rdispman 0x80D5E4C8/0x80D5E508);
**0xD** → 0x80429458; **0x14** → 0x804290FC (queued request with 0xC-B
desc, mux-posted). rdispman's user-side wrappers: send 0x80D5E9C6 (N=0),
send+wait 0x80D5E9F6 (N=1), plus direct N=7/8/9/0xA/0xD/0xE/0xF helpers
(0x80D5E9B8-0x80D5E7EE).

### 10.5 The dispmanx message-id table (static, u32, payload = {id, args})

Built exclusively by rdispman.dll (0x80D5E0E4-0x80D5E796); lengths are
the args-only bytes after the id word. Names: CONFIRMED where the live
import path or the backend call-site string pins them; CONTRACT/INFERRED
otherwise (this firmware's own enum — do NOT assume RPi userland ids;
the shared "DISP" fourcc is where the family resemblance ends).

| id | len (B) | builder | semantics |
|---|---|---|---|
| 3 | 0xC | 0x80D5E0E4 (wait) | display open/info family, 3 words (INFERRED: first call in the file) |
| 4 | 4 | 0x80D5E0F4 (wait) | 1 word (display handle op, INFERRED) |
| 5 | 4 | 0x80D5E102 (no-wait) | 1 word (fire-and-forget, INFERRED close/hint) |
| 8 | 4 | 0x80D5E13E | 1 word |
| 0xA | 8 / 0xC | 0x80D5E14C / 0x80D5E546 | 2 / 3 words (two shapes share the id — arg-count dispatch) |
| 0xC | 8 | 0x80D5E15E (+LDD 9) | 2 words, paired with a tv_out-side op |
| 0xD | 0x14 | 0x80D5E194 (after LDD 5) | 5 words (display configuration, CONTRACT) |
| 0xF | 4 | 0x80D5E1CC | 1 word (paired with LDD 7 cleanup) |
| **0x10** | 4 | 0x80D5E1FA | **update start** — reply-waited, returns the update handle (CONTRACT; backend "starting update manager") |
| **0x11** | 8 | 0x80D5E1FA tail + exp5 0x80D5E270 | **update submit {handle, flag}** — CONFIRMED live (exp5, sent after arming the completion notifier, LDD async 6) |
| 0x12 | 4 | 0x80D5E296 | 1 word |
| **0x13** | 0x40 | 0x80D5E2AE (via 0x80D5E322 family, exp2/3/4) | **element add (full)** — 8 scalars + two 16-B rect structs (dest+src, src may be a resource-rect) — CONTRACT; exp2/3/4 are exported-unused in this ROM, so 0x13 is API-present but unproven on the wire |
| 0x14 | 8/24 | 0x80D5E3BC (exp4) | 2 words + optional rect — element remove/change-family (CONTRACT) |
| 0x15 | 8 | 0x80D5E498 | 2 words |
| 0x16 | 0xC | 0x80D5E3EE | 3 words |
| **0x17** | 0x1C-0x3C | 0x80D5E402 (exp6) | **element op with change-mask** — 7 base words {…, flags |=4 &~8 seen at backend 0x80D547F6, …} + optional dest-rect + optional src-rect (each +4 words); CONFIRMED live: backend ComposeLayer* "Adding Element"/"Adding Cloned Element" call sites 0x80D54804/0x80D54B68 |
| 0x18 | 8/24 | 0x80D5E110 | 2 words + optional rect |
| 0x19 | 0x10 | 0x80D5E4AA | 4 words |
| 0x1A | 8 | 0x80D5E6C2 | 2 words |
| 0x1B | 4 | 0x80D5E6D4 | 1 word |
| 0x1D | 4 | 0x80D5E58A | 1 word |
| 0x1E | 0x44 | 0x80D5E59A | 17 words — big element-add variant (0x13 v2, CONTRACT) |
| 0x1F | 0x10 | 0x80D5E646 | 4 words |
| (pool-derived) | 0x14 | 0x80D5E56C | 5 words, id = pool word +0x1B (variant selector, INFERRED) |
| 0x24 | 8 | 0x80D5E760 | 2 words |
| 0x25 | 4 | 0x80D5E772 | 1 word |
| 0x26 | 0x14 | 0x80D5E782 | 5 words = {handle?, rect} (snapshot/region family, CONTRACT) |

Decode trap worth recording: exp6's `movs r1,#7` / `movs r1,#0xB` are
**word-count markers** (7 words = base, 0xB = base+rect), not message ids
— the id is 0x17, latched at the wrapper call (0x80D5E45C-0x80D5E464).
Other small constants in rdispman that are NOT wire ids: LDD request
numbers (4, 5, 6, 7, 8, 9, 0xA, 0xD, 0xE, 0xF), euser-call arguments
(0x12/0x13 at 0x80D5E804+), and the async-DoRequest numbers (6, 0xB, 0xC,
0xD, 0x10, 0x11). Gaps in the id space (2, 0x20-0x23) have no builder in
this build.

Resource handles inside element payloads are VC-side runtime handles
(allocated via the EGL/KHR or BUFM services) — CONTRACT, not CONFIRMED:
a Linux host must also implement (or capture) one resource-allocation
path before it can point an element at real pixels.

### 10.6 Receive side: replies, updates, vsync (CONFIRMED machinery)

- **Replies** to send+wait: 4-B payloads on "DISP" (0x80345FE4 → vchi
  0x80338864, length checked == 4 else −8).
- **"UPDH" drain**: cb 0x803460AC loops vchi 0x80336020 (peek message) →
  updatehandler 0x80347B08 (allocate an update-list slot keyed by the
  message) + 0x80347B88 → release via 0x80335FEC; the "UpdateCallback
  handler already allocated, cannot update %08x" log (0x803465F0) fires
  on allocation failure. Reason 8 (disconnect) clears state (0x80345F30).
- **Completions to user space**: DVCDispmanUpdateList + the two LDD muxes
  **DISPMAN_LDD_UPDATE_MUX** (update completion — completes the pending
  async-6 from exp5) and **DISPMAN_LDD_FRAMECOUNT_MUX** (frame-count
  reached — completes async 0xB/0xC/0x10/0x11 notifiers; backend classes
  CVCNotifyFrameReached / NotifyFrameCountReached{,Cancel}L). The UPDH
  payload word grammar (which word is update-handle vs frame-count) is
  OPEN — needs one runtime capture; the mux/queue machinery around it is
  CONFIRMED.

### 10.7 The minimal-client contract (write-a-Linux-host checklist)

*Status marks added by task 127 (2026-09-06): **[PROVEN in-chain]** =
executed end-to-end by `tools/vc_session.c` against the responder
through the M2 driver, {poll, IRQ} × {TCG, icount}, §13 has the
as-built; unmarked items remain hardware-era. The §8.5 power/clock
choreography is not modeled (§13.5) and never gates these proofs.*

1. **Transport**: bring up MeSSI per §8.3/§8.5 (engine 0xC6034000, channel
   windows 0xC6034250+0x30×ch, tag (ch<<16)|0x3210) and boot the VC per §9
   (magic-framed exchanges, cmd-0x42 image uploads). No display traffic
   before Boot's count==3/answer==1.
   **[PROVEN in-chain — the register bring-up + ring + doorbells by the
   M2 driver (§12), the full boot ladder (stage-1 sentinel, loader +
   firmware 0x42 uploads, final count==3/answer==1) by the M3 client
   (§13.1 steps 1-3); §8.5's power/clock prelude is outside the model]**
2. **Open the post-boot slots**: arm RX on both channel objects
   (0x8032C788/0x8032BDE4 equivalents), register a consumer, call Start
   (0x8032D3D0) — 4 logical slots; RX arrives as {status 0x4200, slot,
   descriptor {len,ptr,extra}}.
   **[PROVEN in-chain in the M2-ABI shape — the driver's RECV arming +
   CONTRACT-v1 HAT delivery + event servicing replace the decoded
   slot/Start machinery; the Symbian 4-slot consumer itself is not
   emulated and does not need to be for the wire grammar]**
3. **Connect**: emit the 0x14-B control `{0, 1, 0x1010, conn_id, 0x800}`
   (type 0x00100004) and await the peer's answer ("VCHI peer" pool).
   **[PROVEN in-chain — §13.1 step 4, echo asserted verbatim]**
4. **Open the service**: open request carrying fourcc **"DISP"**
   (0x50534944) + a message callback; treat the connect-callback
   (reason 8) as send-ready. Open **"UPDH"** (0x48445055) with a drain
   callback for completions.
   **[NOT exercisable in emulation — the responder has no open exchange
   (it accepts DISP traffic right after the connect echo and pushes
   UPDH unsolicited); the open shape is decode-backed (§10.3 step 4)
   and a hardware session must speak it — first capture should pin its
   wire form]**
5. **Frame everything** per §10.3 headline 3: outer 0x1C-B §8.4 frame
   (fourcc, len, route, CRC-32C of payload at +0x0C) around per-message
   0x10-B records `{0, total_len, fourcc, 0, 0, plen}` + payload
   `{u32 id, args}`. Keep single messages ≤0x1000 B (short path only).
   **[PROVEN in-chain — every client frame is the double nesting; CRC-32C
   enforced in BOTH directions (the client verifies received frames'
   CRCs too, a first among the three guest artifacts); the ≤0x1000 bound
   held throughout (largest service frame 0x6C B)]**
6. **Framebuffer sequence** (ids per §10.5): display open/info (msg 3/4
   family) → obtain a VC resource for the pixel buffer (EGL "KHRN" or
   buffer-manager "BUFM"/"XMFB" — one of these must be spoken; OPEN
   which is cheapest for a host) → msg **0x10** update start (keep the
   returned handle) → element op **0x17** (or full **0x13**) with
   dest/src rects + resource handle → msg **0x11** {handle, flag} submit;
   read completions on UPDH; frame counting via the notifier requests.
   **[PROVEN in-chain from 0x10 on — update start (handle captured),
   0x17 with change-mask + both rects, 0x11 submit, UPDH completion
   drain (§13.1 steps 5-8); the display open/info (3/4) prefix and the
   resource path are NOT exercised — resource handles are VC-runtime
   state the responder does not model (§10.5 note), hardware-era]**
7. **Diagnostics path**: HAT (§9.6), the MeSSI CRC log (§8.4) and the
   "VCHI: CCP2 CRC failure" strings are the host's only failure
   observability — carry them into any Linux implementation log surface.
   **[PROVEN in-chain — the client's timeout path reports the
   master-ack/HAT snapshot (WAIT ioctl) and the final drain check; the
   §8.4 CRC-failure rejection is probed negatively every session
   (§13.1 step 7) with recovery]**

### 10.8 Corrections to earlier sections

- §2.2's "VCDispman kernel client (0x346540-0x347EF1)" is three modules:
  vcdispmanvchi.dll 0x345D20-0x346700 (the DVCDispmanVCHI strings are
  its data at 0x346540+), the un-named DVCDispman/DVCDispmanX class
  image 0x346780-0x347640, and vcdispmanupdatehandler.dll
  0x347640-0x347F70. The "update-callback handler, update lists" of
  §2.2 live in the third, not the first.
- §2.2's "composition requests become VCHI messages here" is refined:
  vcdispmanvchi only MARSHALS them — ids and payloads originate in
  user-side rdispman.dll; the kernel path is a pass-through (§10.5).
- §8.4 frame +0x00 "service/message type id" = the destination service
  fourcc (service-object +0xC); the same fourcc is repeated in the
  per-message record at +0x04. The route/slot byte at frame+0x08 is the
  connection-indexed table value; the TX slot for export 0x8032D6B0
  comes from the message's own route nibble (headline 1).
- §9.11's "decode the consumer registered through Start" is answered:
  vchi.dll 0x803375B0, consumer 0x803370B0 → dispatcher 0x80336FB0,
  routing on status bit 0x200 and the slot index into conn+0x2C00+0xC4.
### 10.9 Reproducibility

ROM-directory walk (variable-length entries, {size, imgPtr, 0x81xx,
UTF-16 name}, name end = next entry/0xFFFF marker); XIP image-header
decode (+0x10 entry, +0x18 data copy, +0x24/+0x28 data/bss, +0x38 export
dir, +0x3C export count, +0x60 RAM cell base, +0x64 dir entry);
import-veneer scans (`ldr pc,[pc,#-4]` + target word, per DLL span —
tools/task120_scan.py for word scans); capstone sweeps `tools/task114_dis.py
<off> <len>` at: 0x345D98-0x346760 (vcdispmanvchi), 0x3476B8-0x347F60
(updatehandler), 0x4287B8-0x429930 (LDD incl. jump tables 0x428DFC/
0x4290C0), 0x337C74/0x332D50/0x33441C/0x336DE8/0x337540/0x3370B0/0x335BD8
(vchi), 0x346780+0x28 (DVCDispman); Thumb disasm 0xD5E0E4-0xD5EA10
(rdispman), 0xD547C0-0xD54BC0 + 0xD56574 (backend exp5/exp6 call sites).
Key data: vcdispmanvchi pools 0x34652C-0x34674C (fourcc cells, mutex
names, export table 0x346718, initializer 0x346710); LDD strings 0x429808/
0x429824/0x4298F4; fourcc constants 0x342AF8 (DSPC), 0x33D4EC/0x33EE6C
(BUFM/XMFB), 0x339B88 (CRMI); VC-side name anchors 0x2E7EFC-0x2E7F18
(dispc_q/DISPCONT/DISPCOND), 0x2E8B14 (TVOUT); vchi connect pools
0x336B18 (0x1010) / 0x337170 (0x00100004) + string 0x337174.

### 10.10 What this hands step 5 (VPU ive_dsi.c)

- The panel ON path is now bounded on both sides: APE side §8-§10 end at
  "element submitted on DISP + completion on UPDH"; what remains is the
  VC-side dispc_q/DISPCONT handler chain down to `ive_dsi.c` (0x2B3554/
  0x2B355C tags) — step 5 should decode the DSI DCS init sequence and
  lane setup there, using the DISP message ids of §10.5 as the entry
  vocabulary (which id ultimately drives dispmanx.c → display driver).
- The two runtime captures that would close the transport OPENs remain:
  the reply DMA landing (§9.8) and the engine+0x1444/0x144C/0x1460
  descriptor layout — a single post-boot MeSSI log with §10.3's connect
  message visible would confirm both framing levels end-to-end.
- UPDH payload word grammar (§10.6 OPEN) is best captured in the same
  session (one update cycle suffices).

## 11. As-built — the QEMU MeSSI engine model + VC responder (task 125, 2026-09-06)

**What landed:** a MeSSI-16 engine model + a scripted VideoCore responder
in `emulation/qemu/hw/arm/bcm2727.c` (the SoC model file, task-123
convention: ignored tree, this doc is the contract record), and a
guest-side proof binary `tools/vchi_smoketest.c` (armv6 static, musl.cc
cross; the binary itself rebuildable via the command in its header).
Model = one 0x1480 MemoryRegion at **PA 0x20204000** (the §8.1 engine
base; VAs in §8.3 are the Symbian view, never mixed into the model), a
level IRQ line into the ICU at **id 201 (0xC9)**, env gates
`MESSI_DEBUG` (trace) and `MESSI_RESPONDER` (the VC stand-in — OFF in
the default boot path, so chain-regress's flow diff stays meaningful).
Validated in-chain: **chain-regress PASS (TCG + icount — the task-124
harness's first real use, 1202-line flow byte-identical with the model
present)** and the smoketest **SMOKE PASS at the `e7:~#` prompt under
both TCG and icount** (evidence log `emulation/qemu-e7sw/
boot-messi-task125.log`, icount run, MESSI_DEBUG on).

**Honesty clause, stated in those words:** the smoketest validates OUR
stack against OUR responder. Everything the responder speaks is
decode-backed (§8.4 frames, §9 boot grammar, §10 service grammar), but
no hardware was in the loop; the fidelity boundary is exactly the
CONTRACT table in §11.4.

### 11.1 Model register coverage (as-built)

| offset (from 0x20204000) | model behavior | backing |
|---|---|---|
| +0x010 master ack | R: bits 0-3 derived (pair-status halves nonzero), bit 25 = HAT level (responder frame queued, no armed RX descriptor); W: accepted, global latches are live levels | decode bits 0-3/25-27 CONFIRMED; HAT-as-bit25 CONTRACT |
| +0x040 global ctrl | RW storage (bit19 start, bit30 power path decoded; no behavior) | decode CONFIRMED, behavior benign |
| +0x050 engine config | RW storage (0x842 at init) | decode CONFIRMED, behavior benign |
| +0x240 engine ctrl | RW storage; bit0 soft-reset fires on a **rising edge** (0→1 after a stored 0 — the decoded RMW pulse; note the init word 0xB5F itself carries bit0, so any-write-resets would fire at init); reset clears statuses/rings/responder state, keeps config | bit0/6/11 semantics decode-backed; edge trigger CONTRACT |
| +0x1204 | scratch (the ISR's read-write-back cell) | address decode-backed, semantics benign |
| +0x1444/+0x144C/+0x1460 | ring mode (stores 6) / ring cookie (PA of the ring header) / activate-on-1 — parses + validates the header from guest RAM | addresses + values decode-backed; semantics = CONTRACT §11.2 |
| +0x204 clock/divider | RW storage (bit13 at init) | decode CONFIRMED |
| +0x208 group status | R: bit0 busy = always 0 (idle) | decode CONFIRMED; model is synchronous |
| +0x20C/+0x210 pair-A en/st | enable gates IRQ; status W1C, events per channel half | decode CONFIRMED (event *bit meanings* CONTRACT) |
| +0x214/+0x218 pair-B en/st | same for ch2/ch3 | decode CONFIRMED |
| +0x250+0x30×ch +0x00 control | RW; **bit0 write = channel doorbell** (processes that channel's rings synchronously: TX first, then queued VC frames to armed RX descriptors on ch0); bit1 disable benign; bit16 W1C acks the channel's event half; bit17 busy reads 0; bits 24-31 event-enable storage | decode CONFIRMED for the word shape (0xFF01002C init, bit16 ack, bit17 busy, bit0 enable); doorbell semantics CONTRACT |
| +0x250+0x30×ch +0x04 tag | RW storage, (ch<<16)\|0x3210 | decode CONFIRMED |
| +0x250+0x30×ch +0x08 | RW: PA of the channel's last completed descriptor | CONTRACT (§8.3's "engine writes a descriptor pointer" hint — deliberately NOT into the control word, see §11.4) |
| +0x18/+0x1C/+0x28 per-channel | RW storage (0x07B00000 watermark etc.) | addresses decode-backed, semantics OPEN/benign |
| event bits (pair halves) | 0x100 TX-done, 0x400 RX-available, 0x800 error — all inside the decoded 0xFBF bit set | bit positions CONTRACT, bit *set* decode-backed |
| IRQ 201 | level on (pair status & pair enable) ≠ 0; a host that never writes the pair enables never sees an IRQ (poll mode — how the userspace smoketest runs without an unhandled Linux IRQ) | id decode-backed; gating CONTRACT |

### 11.2 The ring CONTRACT v1 (ours — §8's declared OPEN, closed by fiat here)

`+0x144C` receives the **PA of a guest-built ring header**: `+0x00 u32
magic 'MSR1' (0x3152534D), +0x04 u32 ndesc (1..1024), +0x08 u32
ring_pa[8]` — index 2×ch = the channel's RX-post ring, 2×ch+1 = its TX
ring; each ring is ndesc 16-byte descriptors consumed in index order.
Descriptor: `+0x00 u32 ctrl` (bit0 ARMED by guest, bit1 DONE by engine,
bits[11:8] tag: **0 = ctrl/header frame** — the B4-slot0 class the boot
upload headers ride, **1 = bulk data chunk** — B4-slot1, **2 = small
message** — the B8 tag-2 class), `+0x04 u32 buf_pa` (frame buffer; TX =
engine reads, RX = engine writes), `+0x08 u32 len` (TX: frame length;
RX: capacity in, received length out), `+0x0C u32 cookie` (echoed).
Channel 0 = the message channel (all VC→AP traffic arrives here),
channel 1 = the bulk/upload channel; ch2/ch3 exist in the register map,
unused by the contract. Writing bit0 of a channel's control word is the
doorbell. Everything on the wire is a §8.4 frame (0x1C header + payload
+ CRC-32C at +0x0C); the frame +0x00 type vocabulary: 0x42 upload, 2
boot-class reply, 0x00100004 control, or a service fourcc.

### 11.3 The responder's scripted behavior (MESSI_RESPONDER=1)

State machine mirroring §9.3: **S1** — the first RX post on ch0 is
answered immediately with the stage-1 reply {0xAFE45251,
**0x01000001**, **3**, …, 0xF001BC4E} (sentinel + the image count the
host should feed, i.e. §9.2's [cell+8]); **LOADER** — ch1 tag-1 frames
are counted (content NOT verified — §7.2's signature algorithm is OPEN),
the tag-0 upload header (magics + class-0x02 length word checked against
the accumulated count) triggers two queued replies: count=3/answer=1
(0x03000001) for cellA then count=1/answer=2 (0x01000002) for cellB;
**FW** — same shape, the firmware header yields the final 0x03000001
("VideoCore boot complete"); **SERVICE** — the 0x14-B control connect
{0,1,0x1010,conn,0x800} is echoed; DISP msg **0x10** → 4-B reply with an
incrementing update handle; **0x13/0x17** → 4-B ack 0; **0x11**
{handle,flag} → no DISP reply, but a **UPDH push** {fourcc "UPDH", 4-B
handle}; other ids → generic ack. All replies/pushes are §8.4 frames
queued FIFO and delivered to armed RX descriptors on ch0 (a queued frame
with no armed descriptor asserts HAT = master-ack bit 25). **CRC-32C is
enforced on every received frame** (table generated at model init with
the ROM's own algorithm: init 0xFFFFFFFF, reflected 0x82F63B78); a bad
CRC logs the decoded message ("VCHI: MeSSI-16 CRC failure") and rejects
the frame silently with the error event — the §8.4 failure semantics,
probed negatively by the smoketest.

### 11.4 CONTRACT vs DECODE — what hardware could contradict

The register **addresses/values** and both protocol **grammars** are
decode-backed (§8.3, §8.4, §9, §10 — the CONFIRMED legend applies). The
model *invents* exactly the following; a hardware capture could
contradict any of it without touching the decode:

| invented (CONTRACT) | decode anchor it must not contradict |
|---|---|
| the entire ring format: cookie = ring-header PA, 'MSR1', ndesc, ring_pa[8], 16-B descriptors {ctrl, buf_pa, len, cookie}, tag nibble semantics, in-order consumption | the +0x1444/0x144C/0x1460 programming **sequence** (values 6 / cookie / 1) and §9.8's "the engine's descriptor/DMA machinery moves the data" |
| doorbell = channel-control bit0 write; TX-then-deliver processing order; synchronous, zero-latency completion | §8.3's TX-enable RMW path (bit0 enable); §9.7's host-side 5 ms poll cadence (any real engine latency ≤ that budget is invisible to the decode) |
| event-bit meanings 0x100/0x400/0x800 (within the decoded 0xFBF set); IRQ gating = pair-enable & status only; HAT as master-ack bit 25, NOT an IRQ source | the decoded bit set (0xFBF/0x0FBF0000), the W1C pair registers, §9.6's HAT-is-a-GPIO mechanism |
| per-channel +0x08 = last-completed-descriptor PA (the model's answer to §8.3's "engine writes a descriptor pointer into the control word slot") — deliberately NOT overwriting +0x00, which stays a control word | §8.3's read-and-dereference at 0x8032F7D4 (the model keeps the *capability* — a descriptor pointer readable per channel — but relocates it so control-word RMW semantics survive) |
| the wire type vocabulary {0x42, 2, 0x00100004, fourcc} on §8.4's +0x00; boot replies wrapped in type-2 frames | §8.4's header layout (CONFIRMED), §9.4's cmd-0x42/tag-2 classes, §10.3's control constant |
| reply word 0x01000002 for cellB (count=1; answer value unpinned by decode — only count==1 is checked); upload length must equal accumulated bytes; signatures not verified | §9.2/§9.4's reply checks (count==3/answer==1, count==1); §7.2's sig-algorithm OPEN |
| responder service semantics: handle allocation, 0x13/0x17 acked on DISP, UPDH payload = single u32 handle | §10.5/§10.6's message table + "4-B replies on DISP, completions on UPDH" (the UPDH word grammar is §10.6's declared OPEN) |

### 11.5 Validation record

- `tools/chain-regress.sh` (task 124's harness, first real use): **PASS**
  TCG and **PASS** `--icount` — boot 27 s each, the 1202-line
  nloader[xti flow byte-identical, all markers + the seven IRQ claims
  (no 201 entry — the model is inert without programming, by design).
- `tools/vchi_smoketest.c` at the prompt (delivered on the SD channel:
  `-drive …,id=sd`, `mount -t ext4 /dev/mmcblk1 /mnt`): **SMOKE PASS**
  under TCG and icount — full transcript in
  `emulation/qemu-e7sw/boot-messi-task125.log` (icount run with
  MESSI_DEBUG; the QEMU-side lines interleaved are the model trace).
  Build: the musl.cc armv6 cross command in the file header.
- Known benign asymmetry proven by the run: event latches coalesce (two
  deliveries set one RXAVAIL bit) — the host drains the RX ring per
  event, which is the shape a real driver needs anyway.

### 11.6 M2 hand-off — what the Linux VCHI transport driver binds to

1. **Resources:** `mmap` the 0x1480 window at PA 0x20204000 (DT node
   `messi@20204000` with interrupt `<201>` per the house DT-int=ICU-id
   convention); allocate the ring + buffers in DMA-coherent memory and
   program `+0x144C` (ring-header PA, 'MSR1'), `+0x1444 = 6`,
   `+0x1460 = 1`; init the four channels (0xFF01002C + tags
   `(ch<<16)|0x3210`); start via `+0x240` bit6 (bit0 is a rising-edge
   soft reset — do NOT write it casually) and `+0x40` bit19.
2. **IRQ path:** enable the channel's pair-enable bits (±0x20C/+0x214)
   and service ICU 201 by: read +0x10, ack it, drain pair statuses
   (W1C +0x210/+0x218), per-channel ack bit16 — the level drops when
   (status & enable) == 0. Poll mode (enable never set) is a valid
   bring-up step and is what the smoketest does.
3. **Protocol:** boot per §9 through the model's exchanges (posts on
   ch0, uploads on ch1), then connect (control 0x14-B echo), then DISP
   per §10.5's table — the responder acknowledges every decoded id, so
   the M2 driver can be brought up message-by-message against it before
   any hardware session. The §10.7 minimal-client checklist remains the
   authoritative host-side spec; the model + responder exist so steps
   1-6 of that checklist can be executed in-chain.
4. **Fidelity:** M2 runs against CONTRACT v1 (§11.2/§11.4). The first
   hardware session should capture the two §10.10 runtime OPENs (reply
   DMA landing, real descriptor layout) and reconcile the contract
   before treating transport behavior as decode-true.

   *As-built: §12 (task 126) — the driver landed per this recipe; the
   deviations section there lists the few places where the letter of
   this block was refined.*

## 12. M2 as-built — the Linux VCHI transport driver bcm2727-vchi (task 126, 2026-09-06)

**What landed:** a platform driver + misc char device for the MeSSI
engine — `kernel/linux/drivers/char/vchi-bcm2727.c`
(CONFIG_BCM2727_VCHI=y), ABI header
`kernel/linux/include/uapi/linux/bcm2727_vchi.h`, DT node
`messi@20204000` (`brcm,bcm2727b1-messi`, window 0x1480, interrupt
<201> per the DT-int = ICU-id convention) in the in-tree bcm2727.dtsi,
defconfig + kernel/configs mirror, and the guest proof
`tools/vchi_smoketest_drv.c` (armv6 static, musl.cc cross, delivered on
the SD channel).  Device node `/dev/bcm2727-vchi`; request_irq name
`20204000.messi`.

### 12.1 Driver shape

- **Bring-up at probe, §11.6 item 1 verbatim:** devm_ioremap_resource
  the 0x1480 window; one `dma_alloc_coherent` block (0x3000: page 0 =
  the 'MSR1' ring header + 8 rings of 4 descriptors, page 1 = the TX
  frame buffer 0x1000, page 2 = four 0x400 RX buffers); program
  +0x144C (ring-header PA) → +0x1444 = 6 → +0x1460 = 1; per-channel
  0xFF01002C + tag (ch<<16)|0x3210; engine control 0xB5F written
  exactly ONCE (its bit0 is the rising-edge soft reset), then the
  0xB5F|0x40 start release + global +0x40 bit19.  The coherent block's
  dma handle IS the PA the model/hardware reads (no IOMMU).
- **Probe is responder-free by construction:** nothing but
  configuration is written; the model raises events only on
  doorbells, so with no MESSI_RESPONDER the device sits idle and the
  regress flow diff stays byte-identical.
- **One frame in flight:** a mutex serializes the ABI; TX descriptors
  are armed per SEND (tag from the ioctl), RX descriptors per RECV,
  indices mirror the model's consumption order (NDESC = 4, proven
  shape from task 125's smoketest).

### 12.2 The userspace ABI (minimal synchronous frame-level)

One ioctl = one COMPLETE §8.4 wire frame (0x1C header incl. CRC-32C
computed by the caller — the driver does no protocol work):

| ioctl | arg | behavior |
|---|---|---|
| `BCM2727_VCHI_IOC_SEND` | `{buf, len, channel, tag, timeout_ms}` | copy in → TX descriptor → doorbell → block for TX-done |
| `BCM2727_VCHI_IOC_RECV` | `{buf, maxlen, channel, timeout_ms, →len}` | arm RX → doorbell (also delivers HAT-held frames) → block for one frame |
| `BCM2727_VCHI_IOC_WAIT` | `{timeout_ms, →master_ack}` | optional diagnostics: wait/snapshot the master-ack register (+0x10, HAT bit 25 — §10.7 item 7) |

Error semantics (ours, not decode): the engine's error event — a frame
the peer rejected, e.g. the bad-CRC probe — surfaces as `-EIO` from
SEND/RECV; a silent peer as `-ETIMEDOUT`; an oversized frame as
`-EMSGSIZE` (left in the ring).  No async queueing, no connection/
service state: that is the M3 client's (userspace) job, hardware-era
work deliberately deferred.

### 12.3 The poll/IRQ story (both modes validated)

- **IRQ (default):** `request_irq(201)` at probe, then pair-A enable
  +0x20C = 0x0D000D00 (TX-done/RX-avail/error for ch0+ch1 halves —
  only the three CONTRACT bits).  The ISR is §11.6 item 2 verbatim:
  read +0x10 and ack it, W1C +0x210/+0x218 with per-channel bit16
  acks, drain until quiet; observed bits go to a half-position latched
  shadow + waitqueue.  In the default no-responder boot ICU 201 shows
  count 0 in /proc/interrupts (claimed, never spurious).
- **Poll (`vchi-bcm2727.poll=1`):** the pair enables are never written
  — CONTRACT v1's gating means the engine never raises the line at
  all; SEND/RECV poll and W1C the pair-status halves directly.  This
  is the task-125 smoketest's mode, kept as the bring-up fallback (a
  failed request_irq falls back to it too, the memcard/i2c shape).
- **Validated ×{poll, IRQ} ×{TCG, icount}:** SMOKE PASS in all four
  combinations (evidence `emulation/qemu-e7sw/boot-vchi-task126.log`),
  plus chain-regress PASS with default envs (both TCG and icount) and
  with MESSI_RESPONDER=1 (flow byte-identical everywhere).

### 12.4 Deviations from §11.6 (and other as-built decisions)

1. **None in the register choreography** — item 1's sequence and item
   2's IRQ service are implemented as written.  The one refinement:
   the smoketest's double `0xB5F|0x40` write (start, then the
   decoded "bit11 release") is kept; the model implements no bit11
   behavior, so this is decode-faithful shape, not tested behavior.
2. **Default mode is IRQ, poll is the param** — §11.6 called poll "a
   valid bring-up step"; as-built, bring-up DID go poll-first (first
   green boot was poll mode), and the committed default is the
   hardware-era shape (IRQ), with poll=1 kept as the debug/fallback
   path.  Both are exit-criteria-validated.
3. **Event-coalescing interplay (found in validation):** a reply can
   land in an RX descriptor DURING a SEND's doorbell (a descriptor a
   previous timed-out RECV left armed).  The SEND's wait consumes only
   its own TX-done bits, so the consuming RECV clears the leftover
   RX-available event itself — otherwise the master-ack channel bit
   sticks and the final WAIT sees the device "not drained".
4. **Half-position bookkeeping (found in validation):** the ISR stores
   odd-channel event bits shifted into their pair half (`<< 16`), the
   same position the poll path masks out of the pair-status register
   — the two modes must agree on bit position or ch1 SENDs never see
   their completions (the first IRQ-mode wall).
5. **ABI ordering consequence:** the synchronous RECV forced the
   smoketest port's exchange order to send-then-receive (stage 1
   remains receive-first — the boot ROM speaks first, §9.3); peer
   frames queued with no armed descriptor ride CONTRACT v1's
   HAT-hold and are delivered by the next RECV's doorbell.

### 12.5 What this hands M3 (the VC-boot + dispman client)

Everything below the wire is now a userspace job: build §8.4 frames
(CRC-32C), run the §9 boot exchanges and the §10.3/§10.5 service
grammar through SEND/RECV, drain UPDH pushes with RECV + WAIT for
HAT.  `tools/vchi_smoketest_drv.c` is the seed of that client — it
already sequences the full boot + one update cycle.  Hardware-era
items the driver still punts on: the §8.5 power/clock choreography
(not modeled, not decoded into DT), any cache maintenance beyond
dma-coherent allocations, and async queueing.

   *As-built: §13 (task 127) — the client landed exactly on this
   recipe (image files, verbose exchange log, the bad-CRC probe); no
   ABI extension was needed, and the one granularity deviation
   (0xFE0 chunks vs §9.1's 8 KB) is recorded in §13.3.*

## 13. M3 as-built — the VC-boot + dispman client vc_session (task 127, 2026-09-06)

**What landed:** `tools/vc_session.c` (armv6 static, the task-103
musl.cc cross, delivered on the SD channel) — the userspace client
that completes the display software stack in emulation, speaking ONLY
through `/dev/bcm2727-vchi` (the §12 driver; no `/dev/mem`).  Two
layers in one binary, every exchange logged with its wire-frame bytes
and interpretation.  Validated {poll, IRQ} × {TCG, icount} with
MESSI_RESPONDER=1: **SESSION PASS in all four**, the four chain-regress
gates green after the tree landed; evidence
`emulation/qemu-e7sw/boot-vcsession-task127.log`.  The §10.7 checklist
is status-marked in place (item 4 is the one the emulation cannot
exercise).

### 13.1 The session map (what the client runs, in order)

§9 boot layer (ch0 = message/exchange channel, ch1 = bulk):

1. **stage-1**: post cellA (a RECV) — the boot ROM speaks first; the
   reply is parsed for both magics, the 0x01000001 sentinel, and the
   image count at [cell+8] (=3), which the client adopts (§9.2's
   negotiation).
2. **stage-2**: the loader image as cmd-0x42 frames on ch1 — payload
   in ≤0xFE0-B tag-1 chunks, closed by the tag-0 0x20-B magic-framed
   header `{0xAFE45251, (len&0xFFFFFF)|0x02000000, sig 20 B,
   0xF001BC4E}` — then the two cell replies (count=3/answer=1 into
   cellA, count=1 into cellB).
3. **main firmware**: the §9.3 SCCS `"@(#)"` scan of the last 0x100 B
   (logging only), the upload + header, the final count=3/answer=1 →
   VC BOOT COMPLETE.

§10 service layer:

4. **connect**: the 0x14-B control `{0, 1, 0x1010, conn, 0x800}` (type
   0x00100004); the reply asserted to be the verbatim echo.
5. **DISP 0x10** update start → the reply is §10.6's `{0x10-B record,
   4-B word}` shape (record fourcc/total/plen checked) and the word is
   the update handle, captured.
6. **DISP 0x17** element op: 7 base words + dest rect + src rect
   (change-mask encoding ours — §13.3 item 3).
7. **bad-CRC probe**: a deliberately corrupted 0x12 frame → the driver
   relays the engine's error event as SEND −EIO, then 500 ms of
   silence (§8.4 rejection), then the session continues — the
   recovery proof, run every session.
8. **DISP 0x11** submit `{handle, flag}` → no DISP reply; the UPDH
   completion drain (loop RECV until the UPDH frame whose word equals
   the handle — non-completion frames are logged and skipped).
9. **final diagnostics**: WAIT snapshot — master-ack 0 (HAT low) —
   §10.7 item 7's observability carried into the client; the same
   snapshot rides every timeout failure path (the §9.3 `HAT=%d`
   diagnostic).

Timeouts follow §9.7 (500 ms reply waits, 5 s bulk sends).  The
client transcript is byte-identical across the whole validation matrix
except the single `driver mode:` line (read from the driver's sysfs
param); TCG vs −icount are fully identical within a mode, poll vs IRQ
differ only in that line and in which side services the same event
set (32 model-trace lines).

### 13.2 The image-file convention

`vc_session [-2] <loader-image> <firmware-image>`; each file is the
image payload.  If it ends with the §7.2 container tail — `"sign"`
magic + 20 signature bytes, the ROM's signed_2727b1_v2.dll geometry —
the client splits it and rides the REAL signature; otherwise a
documented synthetic `{0x5A^i}` rides (the responder checks lengths,
not signatures, §11.4, and the signature algorithm is §7.2's OPEN —
synthetic is a labeled convention, not a faked real one; the log
prints the provenance either way).  `-2` selects the §9.3 loader
variant (imgSel ≠ 0 → set+0x10).  **Hardware:** carve the real set
from the ROM copy per §7.3 — img1 at 0x22E588 len 0x2050, img3 at
0x232668 len 0xF6A2B, each with its tail — and pass those files; the
session then uploads the genuine bytes, rides the genuine signatures,
and the `"@(#)"` scan surfaces the version tag.

The synthetic pair used in validation (deterministic content, md5s in
the evidence-log header): loader 0x2050 B — the real img1/img2 length
— and firmware 0x4000 B, a synthetic size (the responder only sums).

### 13.3 As-built decisions (ours; the decode and the model untouched)

1. **Chunk granularity 0xFE0, not §9.1's 0x2000.**  The M2 driver's
   single TX page caps one wire frame at 0x1000 (§12.2); §9.1's 8-KB
   default is the Symbian scatter-gather path's granularity.  The
   responder accumulates chunk bytes and checks only the header's
   length word (§11.4), so the sessions are protocol-equivalent; a
   hardware-era host wanting true 8-KB chunks grows the driver's TX
   buffer (or implements scatter-gather) — a named deviation, not a
   hidden one.
2. **The cellA/cellB receive-posts of §9.4 msgs 1/2/5 are RECVs.**
   The synchronous ABI forced send-then-receive everywhere except
   stage-1 (kept receive-first — the boot ROM speaks first; §12.4
   item 5's recorded ordering consequence); replies queued while no
   descriptor is armed ride CONTRACT v1's HAT-hold.  The wire sees
   exactly the decoded traffic: the 0x42 uploads and the replies.
3. **Client-chosen protocol values are labeled at the send site**: the
   conn id (0x2727), the 0x10 arg word, the 0x17 base-word values and
   both rects (360×640 panel-shaped, synthetic), the change-mask
   encoding (0x3 = dest|src rects follow).  §10.5 pins shapes and
   lengths; these fill the unpinned slots and the log says so.
4. **RX CRC verified client-side** — both ends now enforce §8.4 (the
   model always did; the two smoketests did not check received
   frames' CRCs).
5. **The service-open exchange does not exist in the emulation
   grammar** (§10.7 item 4's mark): the responder accepts DISP traffic
   right after the connect echo and pushes UPDH unsolicited, so the
   client has no open step to run.  The open shape is decode-backed
   (§10.3 step 4); the first hardware capture should pin its wire
   form.

### 13.4 What M3 deliberately is not (the hardware-era boundary)

The emulation deliverable is the **protocol-complete client**; the
hardware session still owns:

- **Real firmware sources** — the ROM carve per §7.3 (or the flash
  IVE3B question of §7.5) plus the real signature algorithm (§7.2
  OPEN; the loader pair's 128-B shared blob is the likely stage-2
  verification key, §7.6).
- **The §10.10 runtime captures** — the reply DMA landing (§9.8), the
  engine descriptor layout behind +0x1444/+0x144C/+0x1460, the UPDH
  word grammar (§10.6) — to reconcile CONTRACT v1; until then the
  ring/delivery mechanics are ours by fiat (§11.4).
- **A scanout front-end** (fbdev/drm) on top of the update cycle —
  pixels need a resource path (KHRN/BUFM, §10.5's note), which is
  VC-runtime state the emulation does not model.
- **The §8.5 power/clock choreography, HAT GPIO polarity (§9.6 OPEN),
  and driver async queueing** (§12.2's deliberate deferral).

After M3 the display arc's emulation scope is closed: every layer
validatable without hardware — transport driver, boot protocol,
service grammar — exists as running code, and the boundary above is
exact and documented.
