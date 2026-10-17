# Task 110: display/DSI reconnaissance — who owns the panel path

## Status

**DONE 2026-08-28 — recon executed.** Verdict: **VideoCore-mediated; no
APE-side DSI/LCD controller exists.** DSI + TE0 wires originate at the
BCM2727B "IVE" companion (schematic p2/p9, D1400), the DSI driver
(`ive_dsi.c`, `IVE_DSI`, `DSI_TE`) lives inside the VideoCore firmware
embedded in the ROM at ~0x22E780-0x329000, and every APE display
component is a VCHI client (DVCDriver boot + Vaux/Vout/Vio rails only;
vcdispman LDD; TvOutKernelIve; EGL/KHR; DDC/HDCP). Follow-up decode is a
mailbox/protocol job (MeSSI + VCHI dispcontrol/dispmanx), not a register
map. Full evidence + anchors + sized plan:
**docs/bcm2727-display-recon.md**.

**QUEUED 2026-08-26 — written, not executed.** Desk item 1 (HANDOFF).
Deliberately scoped as RECON ONLY (the full decode is unmapped and
unbounded); safe idle pick — read/decode, no builds.

**IDLE-TASK PROMPT (paste into the queue):**
> Execute tasks/110-O-display-dsi-recon.md in /home/lukas/ps3/e7
> (read it, docs/videocore-reference-analysis.md and
> docs/undocumented-gpu-sgx.md first). One job: reconnaissance —
> determine who owns display output on the E7 (an APE-side DSI/LCD
> controller the CPU programs directly, vs a VideoCore-mediated path
> via mailbox/RPC to the VPU) and collect the anchors for a follow-up
> decode. Deliver docs/bcm2727-display-recon.md: the ownership verdict
> with evidence, the anchor list (register ranges or mailbox protocol
> IDs), and a proposed follow-up plan. NO implementation, NO QEMU
> model, NO driver. Stop when the ownership question is answered with
> evidence. Commit when done.

## Why

Desk item 1, never started. The BCM2727 is a VideoCore-family part —
on the Raspberry Pi precedent (see firmware-re/videocore-ref/), display
lives behind a firmware mailbox, not a CPU-programmed DSI block; if
the E7 is shaped the same way, the task is a mailbox/VPU decode, not a
register map. One hard fact already (2026-08-26): `nloader.bin`
contains ZERO display/LCD/DSI/backlight strings — display init is
entirely post-NLoader (Symbian ROM side or VideoCore). That kills the
"bootloader programs the panel" shortcut and points at the two
sources below.

## Sources to mine (in order)

1. **The Symbian ROM** (`firmware-re/dumps/romdumpplus.dmp`, va
   0x80000000, 31 MB): display HAL / surface-update / PDD components.
   Look for DSI-typical MMIO write clusters (register ranges in the
   0x20xxxxxx/0x2020xxxx sys window, per the XTI peripheral map in
   bcm2727-xti-trace.md) vs mailbox-doorbell patterns into the
   VideoCore (cf. the XTI doorbell the trace doc already models).
   Driver strings to hunt: "DSI", "LCD", "display", "surface",
   "videocore", "TVOut".
2. **The kernel early-map table** (icu-timer doc §4, ROM 0x800020A0
   VA←PA pairs) and the XTI peripheral map — any display-ish block
   already listed but unnamed.
3. **docs/videocore-reference-analysis.md** + `firmware-re/
   videocore-ref/` (rpi-open-firmware) + `sgx540-reversing/` — the
   VideoCore-side precedent and any BCM2727-specific findings.
4. **docs/genio-padtable.md** — pads named for display functions
   (DSI lanes often ride GENIO); corroborates a CPU-side vs
   Chip-internal path.
5. Schematics/service manual if present (docs/schematics/,
   docs/service-manual/ — referenced by rom-boot-research §8).

## Deliverables

- `docs/bcm2727-display-recon.md` — verdict + evidence + anchors +
  follow-up plan (sized like a normal decode task).
- This task's Status; HANDOFF desk item 1 pointer.
- Commit (docs only).

## If it does NOT work

If the ROM evidence is inconclusive (no display component found /
everything routes through opaque VPU RPC), write exactly that with
what was excluded — a negative recon still retires the "unknown
shape" problem and sizes the hardware session needed. Do NOT guess a
register map.

## Gotchas

- Old notes are smoke until verified from binaries (rule 4).
- Capstone constant-fold trap (`orr rX,rX,rX,lsr #N`).
- Do not drift into the sgx540 GPU reversing side thread — display
  ownership only. The GPU thread has its own directory and rhythm.
- ROM va 0x80000000; MMIO dual view 0x2003xxxx≡0x2020xxxx (XTI doc).
