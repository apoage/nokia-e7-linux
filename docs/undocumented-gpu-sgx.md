# PowerVR SGX530/540 — Reverse Engineering Notes & Undocumented Features

> **Note (2026-03-23):** Real HW is BCM2727B1 (ARM1176), not OMAP3630. SGX is OMAP3630-specific and does not exist on BCM2727. This document applies only to the synthetic QEMU emulation. See docs/critical-cpu-discovery.md

This document combines findings from the in-house SGX540 reversing work
(`sgx540-reversing/`) with broader research on SGX530 internals for GPGPU
exploitation on the Nokia E7 (OMAP 3630).

> **Note**: Nokia E7 has SGX530. The reversing repo targets SGX540 (Galaxy Tab 2).
> Same USSE ISA family — instruction encoding is compatible, register counts
> and pipe counts differ. Findings transfer directly.

---

## 1. USSE ISA — Reverse Engineered (from sgx540-reversing)

### Register Banks (11 types identified)

| Bank | Name | Purpose |
|------|------|---------|
| r# | Temporary | General purpose |
| o# | Output | Shader output |
| pa# | Primary Attribute | Vertex input |
| sa# | Secondary Attribute | Uniforms |
| c# | Constants | Hardwired float constants |
| i# | Internal | Intermediate registers |
| g# | Global | Global state |
| pclink | PC Link | Return address |
| drc# | DRC | Memory barrier register |

### Key Instructions

| Opcode | Mnemonic | Notes |
|--------|----------|-------|
| 0 | fmad | Float multiply-add (core ALU op) |
| 5 | mov | Move (also with .repeat, .skipinv) |
| 9 | fop | Float operations (fsub, fadd via subop) |
| 29 | ldad | Load doubleword (bpcache, drc barrier) |
| 30 | stad | Store doubleword |
| 31 | ext | Extended ops (br, ba, wdf, emitvtx) |

### Instruction Encoding (64-bit)

```
Bits [31:27] — Opcode
Bits [26:21] — Destination register
Bits [20:14] — Source 0
Bits [13:7]  — Source 1
Bits [6:0]   — Source 2

Second word bank select bits:
  Bit 17 — SWITCH_SOURCE1_BANK
  Bit 16 — SWITCH_SOURCE2_BANK
  Bit 19 — SWITCH_DEST_BANK
  Bit 23 — SKIPINV flag
```

### Repeat and Swizzle

Most instructions support `repeat#` — operates across consecutive registers:
```
mov.repeat4 o0, r0  →  o0=r0, o1=r1, o2=r2, o3=r3
```

Swizzle reorders: `mov.xzy o0, r0` → o0=r0, o2=r1, o1=r2

### Memory Model

- Load: `ldad.bpcache r4, [r3,+#1], drc0` — offset in element units
- Store: `stad.bpcache [r3,+#0], r5`
- `wdf drc0` — wait for load/store completion (barrier)
- **10-cycle WAW hazard** confirmed: store immediately after load to same
  register reads stale value. Need 10+ intervening instructions.

### Control Flow

- `br -#imm` — relative branch (±4096 instructions)
- `ba #imm` — absolute branch
- `.savelink` — writes to pclink for call semantics
- `lapc` — return via pclink
- Predicates: `p0`, `!p0`, conditions via test instructions

### Microkernel

- Located in KernelCode region (GPU VA 0xe400000)
- Contains `0xDEADBEEF` patch points filled by libsrv_init
- PDS (Programmable Data Sequencer) bootstraps microkernel
- `pds_init` binary patched with microkernel address at load time

### PDS (Programmable Data Sequencer)

Separate small processor that feeds data to USSE shaders. Not yet
fully reversed. Controls shader dispatch, data loading.

### GPU Memory Map (from sgx540-reversing)

| Region | GPU VA | Purpose |
|--------|--------|---------|
| General | 0x1000+ | General allocations |
| TAData | 0xC800000 | Tile Accelerator data |
| KernelCode | 0xE400000 | Microkernel USSE code |
| KernelData | 0xF000000 | Microkernel data |
| PixelShaderCode | 0xF400000 | Fragment shaders |
| VertexShaderCode | 0xFC00000 | Vertex shaders |
| PDSPPixelCodeData | 0xDC00000 | PDS pixel code |
| PDSPVertexCodeData | 0xE800000 | PDS vertex code |
| CacheCoherent | 0xD800000 | Cache coherent region |
| PerContext3DParameters | 0xB800000 | 3D state |

---

## 2. Driver Structure (from sgx540-reversing)

| Library | Purpose |
|---------|---------|
| libGLESv* | OpenGL ES implementation |
| libIMGegl | EGL implementation |
| libsrv_um | Low-level: memory allocation, syscalls |
| libglslcompiler | GLSL → USSE compiler (loaded via libsrv_um) |
| libpvr2d | 2D operations (unknown detail) |
| libsrv_init | Microkernel loader, GPU init |

### Shader Compilation Pipeline

1. GLSL source → libglslcompiler → intermediate (PSU format)
2. Intermediate → driver finalization → USSE machine code
3. Code written to PixelShaderCode/VertexShaderCode GPU regions
4. PDS programs set up to dispatch shaders

---

## 3. GPGPU via Fragment Shaders

No OpenCL/compute shaders on SGX530. GPGPU pattern:

1. Create EGL pbuffer context (headless)
2. Render fullscreen quad
3. Fragment shader processes each pixel
4. Read result via glReadPixels or render-to-texture FBO

### TBDR (Tile-Based Deferred Rendering) Exploitation

SGX processes tiles that fit in on-chip scratch memory. For image processing:
- Tile-sized convolution kernels get **free on-chip data reuse**
- Structure processing to match tile boundaries for maximum locality
- SGX530 tile size: typically 16x16 or 32x32 pixels

### Texture Sampling as Free Computation

Bilinear filtering is essentially a free 2x2 weighted average:
```glsl
// Free bilinear interpolation for sub-pixel sampling
vec4 sample = texture2D(image, uv + offset * 0.5 / imageSize);
```

For image scaling, blur, and interpolation — hardware texture unit
computes weighted average faster than ALU math.

### EGL_IMG Extensions

TI SGX implementation may expose non-standard extensions:
- `EGL_IMG_context_priority` — prioritize compute context
- `GL_IMG_texture_stream2` — zero-copy texture from camera/video
- `GL_IMG_read_format` — optimized readback formats

### Readback Limitation

`glReadPixels` on SGX530 is slow (synchronous, stalls pipeline).
Best for display-bound processing where result goes directly to screen.
For offline processing, prefer DSP/NEON over GPU.

---

## 4. Direct Hardware Access (Advanced)

### SGX Register Base

SGX530 MMIO registers on OMAP3630: **0x50000000** (SGX subsystem base).

```c
// pvrsrvkm kernel module provides ioctl interface
// Direct register access possible via /dev/mem if driver not loaded
```

### Microkernel Replacement

The SGX runs a proprietary microkernel loaded by libsrv_init.
The reversing work has identified:
- Microkernel is USSE code in KernelCode region
- Patch points (0xDEADBEEF) for runtime configuration
- PDS bootstrap sequence

**Potential**: custom microkernel could implement compute dispatch
bypassing the graphics pipeline entirely.

---

## 5. Shader Binary Format (from sgx540-reversing)

### glGetProgramBinaryOES Output

```
4 bytes:  magic 0x38B4FA10
4 bytes:  hash
20 bytes: constant header
4 bytes:  remaining size
36 bytes: vertex shader info
4 bytes:  vertex shader length
N bytes:  vertex shader code
...
(fragment shader follows same pattern)
```

### Shader Code Format

```
1 byte:  unknown
"PSU" magic
4 bytes: revision
4 bytes: binary length
...
```

Note: these are NOT final GPU code — driver performs secondary
finalization before writing to GPU memory.

---

## 6. Findings from Shader Analysis

### Vertex Shader Prelude

82 instructions written before any compiled shader code.
Purpose partially unknown — likely GPU state initialization.

### fmad as Universal ALU Op

Addition compiled as `fmad.skipinv.repeat4 o0, pa0, c52, sa10`
— multiply by 1.0 (c52) then add. The "multiply suppression" mechanism
is not fully understood.

### Repeat Delay Slots

Evidence of pipeline delays between repeated operations.
Compiler interleaves independent work between repeat groups.

### `skipinv` Flag

Almost all output-modifying instructions have `skipinv` set.
Purpose unknown — possibly related to shader instance invalidation.

### Index Register

Writing to an immediate destination sets the index register
(low or high half). Used with BANK_INDEXED for dynamic register access.

---

## 7. Open RE Work (from sgx540-reversing checklist)

- [x] Disassembler implemented
- [x] Instruction semantics for common shaders
- [x] Partial assembler
- [x] Shader replacement via LD_PRELOAD hook
- [ ] Full instruction encoding reversal
- [ ] MOE (Multi-Operation Execution?) mechanism
- [ ] PDS (Programmable Data Sequencer) full RE
- [ ] Microkernel full RE
- [ ] Independent rendering without libGL

---

## 8. SGX530 vs SGX540 Differences

| Feature | SGX530 (Nokia E7) | SGX540 (Galaxy Tab 2) |
|---------|-------------------|-----------------------|
| USSE pipes | 2 | 4 |
| Clock | ~200 MHz | ~300 MHz |
| Fill rate | ~200 Mpix/s | ~400 Mpix/s |
| ISA | Same family | Same family |
| Tile size | Same | Same |

Instruction encoding is compatible. Code from SGX540 RE applies to SGX530.
Performance scales by pipe count.

---

*Sources: sgx540-reversing/ (in-house RE), PowerVR SDK docs,
community RE (FreeGPU, pvr_omap), TI SGX DDK*
