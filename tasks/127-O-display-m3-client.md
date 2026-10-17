# Task 127: display arc M3 — the VC-boot + dispman client (the software-complete stack)

## Status

**DONE + VALIDATED — executed 2026-09-06.** All four exit criteria met;
as-built record: docs/bcm2727-display-recon.md §13 (session map, the
image-file convention, as-built decisions incl. the 0xFE0-vs-8 KB chunk
mapping, the hardware-era boundary) + the §10.7 checklist items marked
PROVEN-in-chain in place. Evidence:
`emulation/qemu-e7sw/boot-vcsession-task127.log` (four full boots:
{poll, IRQ driver mode} × {TCG, icount}, each with the full session
transcript; plus the four chain-regress gates). The whole session
transcript of the IRQ/TCG run is embedded below.

Results vs the exit criteria:

1. **SESSION PASS at the e7:~# prompt in all four combinations** —
   {poll, IRQ} × {TCG, icount}, always with MESSI_RESPONDER=1
   (MESSI_DEBUG=1 on for the model trace). Driver mode cross-checked
   two ways per boot: the kernel probe line and the client's sysfs
   read agree in every log.
2. **Negative probe in-session**: the deliberately corrupted 0x12
   frame → SEND −EIO (the engine error event, §12.2) + 500 ms of
   silence (the §8.4 rejection, model logs "VCHI: MeSSI-16 CRC
   failure") → the 0x11 submit + UPDH completion succeed after it —
   recovery proven, every session.
3. Transcript below + evidence log committed.
4. §13 written; §10.7 items 1/2/3/5/6(partial, from 0x10)/7 marked
   PROVEN in-chain, item 4 explicitly marked NOT exercisable in
   emulation (the responder has no open exchange — decode-backed
   shape, hardware must speak it); §12.5 got the as-built pointer.
   HANDOFF untouched.

Walls found and fixed (both client-side; driver, model and decode
untouched — the M2 ABI needed NO extension):

- The reply-record check initially tested a u32 at +0x0C for zero,
  but the §10.3 record's last word is {u16 reserved, u16 payload_len}
  — the plen half at +0x0E made it nonzero and failed every service
  reply (found at the first full run; fixed to check the u16 halves).
- The driver-mode log line read the module-param sysfs file expecting
  '1' — kernel bool params print 'Y'/'N', so the poll boots were
  mislabeled IRQ (the kernel probe line exposed it during matrix
  verification; fixed to accept both).

Landed: `tools/vc_session.c` (armv6 static, musl.cc cross, delivered
on the SD channel with the synthetic image pair — binary never
committed, the task-125 precedent). NO kernel changes, NO QEMU
changes. chain-regress after the tree touch: PASS ×{default envs,
MESSI_RESPONDER=1} × {TCG, icount} — 1202-line flow byte-identical
everywhere.

Determinism note (evidence-log header): the client transcript is
byte-identical across all four validation boots except the single
"driver mode:" line; TCG vs icount fully identical within a mode.

**IDLE-TASK PROMPT (paste into the queue):**
> Execute tasks/127-O-display-m3-client.md in /home/lukas/ps3/e7
> (read it and docs/bcm2727-display-recon.md first — §9 the boot
> exchange grammar, §10 the service grammar + §10.7 the
> minimal-client checklist, §11.4 what the responder does/does not
> verify, §11.6 the protocol path, and §12 task 126's driver ABI).
> One job: the userspace client that completes the display stack in
> emulation — `tools/vc_session.c` (armv6 static, the task-103 musl
> cross; same delivery as vchi_smoketest), speaking ONLY through
> /dev/bcm2727-vchi (no /dev/mem). Two layers in one binary:
> (1) the VC-boot layer per §9 — stage exchanges on ch0 (receive
> posts, the 0x01000001 sentinel negotiation, image count from the
> reply), then the cmd-0x42 image uploads on ch1 (image bytes as
> 8 KB chunks + the 0x20-B magic-framed header carrying
> (len&0xFFFFFF)|0x02000000 + the 20-B signature ride-along) using
> image files passed as arguments — synthetic sizes in emulation
> (the responder checks lengths, not signatures, §11.4; on hardware
> the real set comes from the ROM copy per §7 — document, do not
> fake), ending in the final count==3/answer==1; (2) the service
> layer per §10 — the 0x14-B connection control echo, DISP msg 0x10
> update-start (capture the handle), 0x17 element op (change-mask +
> rects), 0x11 submit {handle, flag}, and the UPDH completion drain
> — the §10.7 checklist executed in-chain, end to end, with every
> exchange logged verbosely (frame bytes + interpretation). Exit
> criteria: (1) SESSION PASS at the e7:~# prompt under TCG and
> -icount with MESSI_RESPONDER=1, in both driver modes (poll/IRQ)
> if the driver exposes both; (2) a negative probe: a deliberately
> corrupted frame is rejected by the CRC layer and the session
> recovers/reports per §8.4; (3) the whole session transcript in the
> task file + evidence log committed (boot-vcsession-task127.log);
> (4) docs: display-recon new §13 'M3 as-built' (the client's
> session map, the image-file convention, and the hardware-era
> mapping note: real firmware sources + the §10.10 runtime captures
> to reconcile CONTRACT v1 + an fbdev/drm front-end are the
> hardware-session work — the emulation deliverable is the
> protocol-complete client); mark the §10.7 checklist items that are
> now proven in-chain; HANDOFF untouched. Stop per the file's
> clauses. Commit when done.

## Why

M2 moved the transport into a real kernel driver; M3 closes the
software arc — after it, "keep the VC firmware, write a Linux VCHI
host" exists as running code for every layer we can validate without
hardware: driver transport, boot protocol, service grammar. What
remains display-wise is inherently hardware (real firmware bytes,
the runtime descriptor capture, scanout) — this task makes the
boundary exact and documented.

## Method

1. Read the responder's expectations from the model source (it is
   the oracle: exact reply shapes, the length checks, the id set it
   acknowledges).
2. Write the client in layers (boot session, connect, update cycle)
   with per-exchange verbose logging; run against MESSI_RESPONDER=1
   + MESSI_DEBUG=1 while iterating.
3. Negative probe (bad CRC mid-session), then the final validation
   matrix (TCG/icount × driver modes).
4. Docs per the prompt; commit evidence.

## Exit criteria

1-4 per the prompt (SESSION PASS across the matrix, the bad-CRC
probe, transcript + log, §13 as-built with the hardware-era
boundary).

## If it does NOT work

- If a session step stalls against the responder, bisect with the
  double trace (MESSI_DEBUG + client logging): a client bug → fix
  the client; a responder gap (an id it never acknowledges) → fix
  the RESPONDER in the model + note it in §11 (it is ours, and the
  decode tables of §9/§10 are the spec both ends must match).
- If the driver ABI proves too tight for the session flow (e.g.
  needing concurrent ch0-listen while uploading on ch1): extend the
  ABI minimally in the driver + §12 (e.g. a non-blocking RECV or a
  channel-select wait), documenting the addition — do not route
  around the driver with /dev/mem.

## Gotchas

- The binary rides the SD channel like vchi_smoketest (power-of-2
  sd.img; debugfs injection; commit SOURCE only, never the binary —
  the task-125 stray-binary cleanup precedent).
- The upload path is the bulk B4-class flow (8 KB chunks) — do not
  serialize it through the small-message path; §9.1/§11 define which
  channel class each phase uses.
- CRC-32C: reuse the exact implementation shape from the smoketest
  (init 0xFFFFFFFF, reflected 0x82F63B78) so all three artifacts —
  model, smoketest, client — share one code shape.
- Frame nesting is double (§8.4 header around per-message records)
  — build helpers, don't hand-pack bytes.
- MESSI_RESPONDER=1 on every validation boot; default-env boots stay
  clean; run tools/chain-regress.sh after any tree touch.
- Commit locally; never push.

## Session transcript (the criterion-3 copy)

The IRQ-mode TCG boot, client + model trace interleaved exactly as the
serial console printed it (boot markers etc. in the evidence log;
identical transcript in the other three boots except the driver-mode
line — see the determinism note above):

```console
vc_session: task 127 — M3, the VC-boot + dispman client (display-recon §9 boot layer + §10 service layer, through /dev/bcm2727-vchi)
device open — the driver owns bring-up, the ring and the events
driver mode: IRQ-driven (ICU 201 claimed, pair enables on)
image /mnt/loader.bin: 8272 B payload, signature synthetic {0x5A^i} — responder checks lengths, not signatures (§11.4)
image /mnt/fw.bin: 16384 B payload, signature synthetic {0x5A^i} — responder checks lengths, not signatures (§11.4)
--- §9 boot layer ---
stage-1: post cellA as a receive buffer — the boot ROM speaks first (§9.3)
messi: delivered VC frame 60 B to RX desc @0x808f8040
messi: stage-1 exchange: sentinel reply + image count 3
messi: ch0 ctrl <- 0xff01002d
messi: pair-A status W1C 0x00000400 -> 0x00000000
messi: ch0 ctrl <- 0xff01002c
<< #1 ch0 type 0x00000002 plen 32 crc 0xa4a6ffe3 | stage-1 cellA reply
     0000: 02000000 20000000 00000000 e3ffa6a4
     0010: 00000000 00000000 00000000 5152e4af
     0020: 01000001 03000000 00000000 00000000
     0030: 00000000 00000000 4ebc01f0
   parse: magics ok; word 0x01000001 -> count 1, answer 1; [cell+8] 0x00000003
   sentinel 0x01000001 -> image-id negotiation (§9.2)
   requested-count adopted: 0x7FFFFFFF -> 3 — the VC tells the host how many images to feed
stage-2: upload the loader image, then exchange both cell replies (§9.4 msgs 2-5)
   loader variant A — set+4 (img1) (§9.3 imgSel)
upload loader: 8272 B in 3 chunk(s) of <= 0xfe0, then the 0x20-B magic-framed header (cmd 0x42, §9.4 msgs 3-4/6-7)
>> #2 ch1 tag1 type 0x00000042 plen 4064 crc 0x9e594ee5 | cmd-0x42 loader chunk 1/3 (file 0x000000..0x000fdf)
     0000: 42000000 e00f0000 00000000 e54e599e
     0010: 00000000 00000000 00000000 53594e54
     0020: 482d5643
     ...
     0000: 8f129518 9b1ea124
messi: ch1 tag1 frame type 0x00000042 plen 4064 ok
messi: upload chunk 4064 B (total 4064)
messi: ch1 ctrl <- 0xff01002d
messi: pair-A status W1C 0x01000000 -> 0x00000000
messi: ch1 ctrl <- 0xff01002c
>> #3 ch1 tag1 type 0x00000042 plen 4064 crc 0x24dd01c1 | cmd-0x42 loader chunk 2/3 (file 0x000fe0..0x001fbf)
     0000: 42000000 e00f0000 00000000 c101dd24
     0010: 00000000 00000000 00000000 a72aad30
     0020: b336b93c
     ...
     0000: 2fb235b8 3bbe41c4
messi: ch1 tag1 frame type 0x00000042 plen 4064 ok
messi: upload chunk 4064 B (total 8128)
messi: ch1 ctrl <- 0xff01002d
messi: pair-A status W1C 0x01000000 -> 0x00000000
messi: ch1 ctrl <- 0xff01002c
>> #4 ch1 tag1 type 0x00000042 plen 144 crc 0x460fafe2 | cmd-0x42 loader chunk 3/3 (file 0x001fc0..0x00204f)
     0000: 42000000 90000000 00000000 e2af0f46
     0010: 00000000 00000000 00000000 47ca4dd0
     0020: 53d659dc
     ...
     0000: df62e568 eb6ef174
messi: ch1 tag1 frame type 0x00000042 plen 144 ok
messi: upload chunk 144 B (total 8272)
messi: ch1 ctrl <- 0xff01002d
messi: pair-A status W1C 0x01000000 -> 0x00000000
messi: ch1 ctrl <- 0xff01002c
>> #5 ch1 tag0 type 0x00000042 plen 32 crc 0x4d6581f5 | cmd-0x42 loader header: len 0x2050|0x02000000 + 20-B signature (synthetic)
     0000: 42000000 20000000 00000000 f581654d
     0010: 00000000 00000000 00000000 5152e4af
     0020: 50200002 5a5b5859 5e5f5c5d 52535051
     0030: 56575455 4a4b4849 4ebc01f0
messi: ch1 tag0 frame type 0x00000042 plen 32 ok
messi: VC frame queued with no RX descriptor (HAT asserted, 1 frames waiting)
messi: VC frame queued with no RX descriptor (HAT asserted, 2 frames waiting)
messi: loader accepted — count=3/answer=1 + cellB reply
messi: ch1 ctrl <- 0xff01002d
messi: pair-A status W1C 0x01000000 -> 0x00000000
messi: ch1 ctrl <- 0xff01002c
messi: delivered VC frame 60 B to RX desc @0x808f8050
messi: VC frame queued with no RX descriptor (HAT asserted, 1 frames waiting)
messi: ch0 ctrl <- 0xff01002d
messi: pair-A status W1C 0x00000400 -> 0x00000000
messi: ch0 ctrl <- 0xff01002c
<< #6 ch0 type 0x00000002 plen 32 crc 0x33f359ba | cellA reply (loader accepted)
     0000: 02000000 20000000 00000000 ba59f333
     0010: 00000000 00000000 00000000 5152e4af
     0020: 01000003 00000000 00000000 00000000
     0030: 00000000 00000000 4ebc01f0
   parse: magics ok; word 0x03000001 -> count 3, answer 1; [cell+8] 0x00000000
   replyA: count=3 answer=1 — loader running, ready for firmware
messi: delivered VC frame 60 B to RX desc @0x808f8060
messi: ch0 ctrl <- 0xff01002d
messi: pair-A status W1C 0x00000400 -> 0x00000000
messi: ch0 ctrl <- 0xff01002c
<< #7 ch0 type 0x00000002 plen 32 crc 0x8c48252d | cellB reply
     0000: 02000000 20000000 00000000 2d25488c
     0010: 00000000 00000000 00000000 5152e4af
     0020: 02000001 00000000 00000000 00000000
     0030: 00000000 00000000 4ebc01f0
   parse: magics ok; word 0x01000002 -> count 1, answer 2; [cell+8] 0x00000000
   replyB: count=1 (answer value unpinned by decode — §11.4)
main firmware: SCCS scan, upload, final exchange (§9.3)
   VideoCore image ID not found (synthetic image — the ROM img3 carries the "@(#)" tag, §9.3)
upload firmware: 16384 B in 5 chunk(s) of <= 0xfe0, then the 0x20-B magic-framed header (cmd 0x42, §9.4 msgs 3-4/6-7)
>> #8 ch1 tag1 type 0x00000042 plen 4064 crc 0x4ac13fc7 | cmd-0x42 firmware chunk 1/5 (file 0x000000..0x000fdf)
     0000: 42000000 e00f0000 00000000 c73fc14a
     0010: 00000000 00000000 00000000 53594e54
     0020: 482d5643
     ...
     0000: 8f129518 9b1ea124
messi: ch1 tag1 frame type 0x00000042 plen 4064 ok
messi: upload chunk 4064 B (total 4064)
messi: ch1 ctrl <- 0xff01002d
messi: pair-A status W1C 0x01000000 -> 0x00000000
messi: ch1 ctrl <- 0xff01002c
>> #9 ch1 tag1 type 0x00000042 plen 4064 crc 0x24dd01c1 | cmd-0x42 firmware chunk 2/5 (file 0x000fe0..0x001fbf)
     0000: 42000000 e00f0000 00000000 c101dd24
     0010: 00000000 00000000 00000000 a72aad30
     0020: b336b93c
     ...
     0000: 2fb235b8 3bbe41c4
messi: ch1 tag1 frame type 0x00000042 plen 4064 ok
messi: upload chunk 4064 B (total 8128)
messi: ch1 ctrl <- 0xff01002d
messi: pair-A status W1C 0x01000000 -> 0x00000000
messi: ch1 ctrl <- 0xff01002c
>> #10 ch1 tag1 type 0x00000042 plen 4064 crc 0x14192898 | cmd-0x42 firmware chunk 3/5 (file 0x001fc0..0x002f9f)
     0000: 42000000 e00f0000 00000000 98281914
     0010: 00000000 00000000 00000000 47ca4dd0
     0020: 53d659dc
     ...
     0000: cf52d558 db5ee164
messi: ch1 tag1 frame type 0x00000042 plen 4064 ok
messi: upload chunk 4064 B (total 12192)
messi: ch1 ctrl <- 0xff01002d
messi: pair-A status W1C 0x01000000 -> 0x00000000
messi: ch1 ctrl <- 0xff01002c
>> #11 ch1 tag1 type 0x00000042 plen 4064 crc 0x17b1863c | cmd-0x42 firmware chunk 4/5 (file 0x002fa0..0x003f7f)
     0000: 42000000 e00f0000 00000000 3c86b117
     0010: 00000000 00000000 00000000 e76aed70
     0020: f376f97c
     ...
     0000: 6ff275f8 7bfe8104
messi: ch1 tag1 frame type 0x00000042 plen 4064 ok
messi: upload chunk 4064 B (total 16256)
messi: ch1 ctrl <- 0xff01002d
messi: pair-A status W1C 0x01000000 -> 0x00000000
messi: ch1 ctrl <- 0xff01002c
>> #12 ch1 tag1 type 0x00000042 plen 128 crc 0x73fe4029 | cmd-0x42 firmware chunk 5/5 (file 0x003f80..0x003fff)
     0000: 42000000 80000000 00000000 2940fe73
     0010: 00000000 00000000 00000000 870a8d10
     0020: 9316991c
     ...
     0000: ef72f578 fb7e0184
messi: ch1 tag1 frame type 0x00000042 plen 128 ok
messi: upload chunk 128 B (total 16384)
messi: ch1 ctrl <- 0xff01002d
messi: pair-A status W1C 0x01000000 -> 0x00000000
messi: ch1 ctrl <- 0xff01002c
>> #13 ch1 tag0 type 0x00000042 plen 32 crc 0x716a332b | cmd-0x42 firmware header: len 0x4000|0x02000000 + 20-B signature (synthetic)
     0000: 42000000 20000000 00000000 2b336a71
     0010: 00000000 00000000 00000000 5152e4af
     0020: 00400002 5a5b5859 5e5f5c5d 52535051
     0030: 56575455 4a4b4849 4ebc01f0
messi: ch1 tag0 frame type 0x00000042 plen 32 ok
messi: VC frame queued with no RX descriptor (HAT asserted, 1 frames waiting)
messi: responder: VideoCore boot complete (final count=3 answer=1)
messi: ch1 ctrl <- 0xff01002d
messi: pair-A status W1C 0x01000000 -> 0x00000000
messi: ch1 ctrl <- 0xff01002c
messi: delivered VC frame 60 B to RX desc @0x808f8070
messi: ch0 ctrl <- 0xff01002d
messi: pair-A status W1C 0x00000400 -> 0x00000000
messi: ch0 ctrl <- 0xff01002c
<< #14 ch0 type 0x00000002 plen 32 crc 0x33f359ba | final reply
     0000: 02000000 20000000 00000000 ba59f333
     0010: 00000000 00000000 00000000 5152e4af
     0020: 01000003 00000000 00000000 00000000
     0030: 00000000 00000000 4ebc01f0
   parse: magics ok; word 0x03000001 -> count 3, answer 1; [cell+8] 0x00000000
   final: count=3 answer=1 -> VC BOOT COMPLETE
--- §10 service layer ---
connect: the 0x14-B control message on the control handle (§10.3)
   {zeros, version 1, ver 0x1010, conn id 0x2727 (ours), max message 0x800} — type 0x00100004
>> #15 ch0 tag2 type 0x00100004 plen 20 crc 0x1a39ac29 | control connect
     0000: 04001000 14000000 00000000 29ac391a
     0010: 00000000 00000000 00000000 00000000
     0020: 01000000 10100000 27270000 00080000
messi: ch0 tag2 frame type 0x00100004 plen 20 ok
messi: VC frame queued with no RX descriptor (HAT asserted, 1 frames waiting)
messi: control/connect echoed
messi: VC frame queued with no RX descriptor (HAT asserted, 1 frames waiting)
messi: ch0 ctrl <- 0xff01002d
messi: pair-A status W1C 0x00000100 -> 0x00000000
messi: ch0 ctrl <- 0xff01002c
messi: delivered VC frame 48 B to RX desc @0x808f8040
messi: ch0 ctrl <- 0xff01002d
messi: pair-A status W1C 0x00000400 -> 0x00000000
messi: ch0 ctrl <- 0xff01002c
<< #16 ch0 type 0x00100004 plen 20 crc 0x1a39ac29 | connect echo
     0000: 04001000 14000000 00000000 29ac391a
     0010: 00000000 00000000 00000000 00000000
     0020: 01000000 10100000 27270000 00080000
   ok: peer echoed the connect message verbatim
DISP msg 0x10: update start (§10.5)
   arg0 = 1 (the display/context word — §10.5 shape, semantics CONTRACT)
>> #17 ch0 tag2 type 0x50534944 plen 24 crc 0xaf842b55 | DISP 0x10 update start
     0000: 44495350 18000000 00000000 552b84af
     0010: 00000000 00000000 00000000 00001800
     0020: 44495350 00000000 00000800 10000000
     0030: 01000000
messi: ch0 tag2 frame type 0x50534944 plen 24 ok
messi: VC frame queued with no RX descriptor (HAT asserted, 1 frames waiting)
messi: DISP 0x10 update start -> handle 1
messi: VC frame queued with no RX descriptor (HAT asserted, 1 frames waiting)
messi: ch0 ctrl <- 0xff01002d
messi: pair-A status W1C 0x00000100 -> 0x00000000
messi: ch0 ctrl <- 0xff01002c
messi: delivered VC frame 48 B to RX desc @0x808f8050
messi: ch0 ctrl <- 0xff01002d
messi: pair-A status W1C 0x00000400 -> 0x00000000
messi: ch0 ctrl <- 0xff01002c
<< #18 ch0 type 0x50534944 plen 20 crc 0x573d8d43 | 0x10 reply
     0000: 44495350 14000000 00000000 438d3d57
     0010: 00000000 00000000 00000000 00001400
     0020: 44495350 00000000 00000400 01000000
   parse: record {total 0x14, fourcc 0x50534944, plen 4} + word 0x00000001 (1)
   update handle 1 captured (kept for 0x17/0x11 — §10.7 item 6)
DISP msg 0x17: element op — change-mask + dest/src rects (§10.5)
   base {update 1, element 0, layer 0, change-mask 0x3, flags 0x4, opacity 0xff, transform 0}
   dest rect {0,0,360,640}, src rect {0,0,360,640} — 360x640 panel shape, values synthetic
>> #19 ch0 tag2 type 0x50534944 plen 80 crc 0xb992136d | DISP 0x17 element op (7 base + dest + src)
     0000: 44495350 50000000 00000000 6d1392b9
     0010: 00000000 00000000 00000000 00005000
     0020: 44495350 00000000 00004000 17000000
     0030: 01000000 00000000 00000000 03000000
     0040: 04000000 ff000000 00000000 00000000
     0050: 00000000 68010000 80020000 00000000
     0060: 00000000 68010000 80020000
messi: ch0 tag2 frame type 0x50534944 plen 80 ok
messi: VC frame queued with no RX descriptor (HAT asserted, 1 frames waiting)
messi: DISP 0x17 element op -> ack 0
messi: VC frame queued with no RX descriptor (HAT asserted, 1 frames waiting)
messi: ch0 ctrl <- 0xff01002d
messi: pair-A status W1C 0x00000100 -> 0x00000000
messi: ch0 ctrl <- 0xff01002c
messi: delivered VC frame 48 B to RX desc @0x808f8060
messi: ch0 ctrl <- 0xff01002d
messi: pair-A status W1C 0x00000400 -> 0x00000000
messi: ch0 ctrl <- 0xff01002c
<< #20 ch0 type 0x50534944 plen 20 crc 0x8a7827fb | 0x17 reply
     0000: 44495350 14000000 00000000 fb27788a
     0010: 00000000 00000000 00000000 00001400
     0020: 44495350 00000000 00000400 00000000
   parse: record {total 0x14, fourcc 0x50534944, plen 4} + word 0x00000000 (0)
   ok: element op acked 0
bad-CRC probe: a deliberately corrupted frame mid-session (§8.4)
   msg 0x12 {1} with the CRC bit-flipped — a frame the peer must reject
>> #21 ch0 tag2 type 0x50534944 plen 24 crc 0x3dfcd11a | DISP 0x12 with corrupted CRC (probe)
     0000: 44495350 18000000 00000000 1ad1fc3d
     0010: 00000000 00000000 00000000 00001800
     0020: 44495350 00000000 00000800 12000000
     0030: 01000000
messi: VCHI: MeSSI-16 CRC failure (ch0 frame type 0x50534944, 24 B payload) — frame rejected
messi: ch0 ctrl <- 0xff01002d
messi: pair-A status W1C 0x00000900 -> 0x00000000
messi: ch0 ctrl <- 0xff01002c
   ok: SEND -> -EIO — the driver relayed the engine's error event (§12.2)
messi: ch0 ctrl <- 0xff01002d
   ok: silent for 500 ms — rejected with no reply (§8.4); the session recovers on the next exchange
DISP msg 0x11: update submit {handle, flag} (§10.5)
   {handle 1, flag 0}
>> #22 ch0 tag2 type 0x50534944 plen 28 crc 0x4cb58571 | DISP 0x11 submit {handle, flag}
     0000: 44495350 1c000000 00000000 7185b54c
     0010: 00000000 00000000 00000000 00001c00
     0020: 44495350 00000000 00000c00 11000000
     0030: 01000000 00000000
messi: ch0 tag2 frame type 0x50534944 plen 28 ok
messi: delivered VC frame 48 B to RX desc @0x808f8070
messi: DISP 0x11 submit {handle 1} -> UPDH completion
messi: ch0 ctrl <- 0xff01002d
messi: pair-A status W1C 0x00000500 -> 0x00000000
messi: ch0 ctrl <- 0xff01002c
   0x11 draws no DISP reply — the completion arrives as an UPDH push (§10.6)
<< #23 ch0 type 0x48445055 plen 20 crc 0xf9e64dc2 | UPDH completion drain
     0000: 55504448 14000000 00000000 c24de6f9
     0010: 00000000 00000000 00000000 00001400
     0020: 55504448 00000000 00000400 01000000
   parse: record {total 0x14, fourcc 0x48445055, plen 4} + word 0x00000001 (1)
   ok: completion drained on UPDH with the right handle — update cycle complete
HAT low, master ack quiet — every exchange drained (§10.7 item 7 diagnostics)
SESSION PASS
VCS-DONE-0
```
