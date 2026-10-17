# Task 125: display arc M1 — QEMU MeSSI engine model + VC responder stub

## Status

**DONE + VALIDATED — executed 2026-09-06.** All four exit criteria met;
as-built record: docs/bcm2727-display-recon.md §11 (register coverage,
ring CONTRACT v1, responder script, the CONTRACT-vs-decode table, M2
hand-off). Evidence: `emulation/qemu-e7sw/boot-messi-task125.log`
(icount run, MESSI_DEBUG + MESSI_RESPONDER on, full smoketest
transcript); chain-regress PASS in both modes — the task-124 harness's
first real use.

Results vs the exit criteria:

1. Model + responder in the tree (`emulation/qemu/hw/arm/bcm2727.c`,
   MeSSI section + `bcm2727_messi_init`); chain boots `e7:~#`;
   **chain-regress PASS (TCG and --icount: boot 27 s each, 1202-line
   nloader[xti flow byte-identical, all markers + IRQ claims; no
   /proc/interrupts 201 entry — the model is inert until programmed and
   the responder is env-gated OFF in the default boot path).**
2. Smoketest **SMOKE PASS at the prompt** under TCG AND icount
   (transcript below and in the evidence log; delivered via the SD
   channel: `-drive …,id=sd` + `mount -t ext4 /dev/mmcblk1 /mnt`).
3. §11 written as-built with the loud CONTRACT table (§11.4) — the
   descriptor/ring layout, doorbell/event-bit meanings, IRQ gating and
   timing are OURS; the frame grammar (§8.4), boot grammar (§9) and
   service grammar (§10) are decode-backed.
4. M2 hand-off block = §11.6 (DT/registers/IRQ bring-up, poll→IRQ
   progression, message-by-message bring-up path against the
   responder, fidelity reminder).

Walls found and fixed (all model/test-side, decode untouched):

- Engine-ctrl bit0 "soft reset": the decoded init word 0xB5F itself
  carries bit0, so reset-on-any-bit0-write fired on every engine-ctrl
  write and killed the ring right at start. The decoded pulse is an
  explicit RMW toggle — the model now resets only on a bit0 rising
  edge (§11.1).
- The responder's 512-B staging buffer rejected 4-KB bulk chunks
  ("illegal length") — CRC is now computed streaming from guest RAM,
  and only small semantic payloads are staged.
- Event latches coalesce: two queued replies set ONE RXAVAIL bit, so a
  wait-then-ack-per-frame test wedged on the second reply. Fixed
  guest-side by ring-drain-per-event (the shape a real driver uses);
  recorded in §11.5 as a proven benign asymmetry.
- The service sends initially had no armed RX buffer (the responder
  queues + HATs instead of answering) — the test now posts a buffer
  before every service send, matching §10.3's arm-RX-then-send order.

**IDLE-TASK PROMPT (paste into the queue):**
> Execute tasks/125-O-messi-model-responder.md in /home/lukas/ps3/e7
> (read it and docs/bcm2727-display-recon.md first — §8 the MeSSI
> engine (base VA 0xC6034000/PA 0x20204000, register map §8.3, IRQ
> 0xC9, clock gate §8.5), §8.4 the VCHI frame + CRC-32C, §9 the boot
> exchange grammar (magic frames 0xAFE45251/0xF001BC4E, the
> state machine §9.3, 0x42 image uploads), §10 the service grammar
> (DISP/UPDH, msg 0x10/0x17/0x11 cycle), and the CONTRACT-vs-
> CONFIRMED legend). One job: a QEMU-side MeSSI engine model + a
> built-in VC responder, plus a guest-side proof binary. Model:
> sysbus at PA 0x20204000, window ≥0x1480, implementing the §8.3
> CONFIRMED map (master ack +0x10, engine config +0x50, global ctrl
> +0x40/+0x240, channel-group +0x200 with W1C pair status/enable
> +0x0C-0x18, per-channel windows 0x20204250+0x30×ch with the
> 0xFF01002C control word + channel tags, ring programming
> +0x1444/+0x144C/+0x1460); IRQ to ICU 0xC9/201. The descriptor/
> shared-ring layout is §8's declared OPEN — define a MINIMAL ring
> format as an explicit MODEL CONTRACT (documented in the doc, never
> passed off as decode). Responder (inside the model, env-gated
> MESSI_RESPONDER=1): a state machine speaking §9's boot exchanges
> (stage receive-posts answered with magic-framed replies ending in
> count==3/answer==1, 0x42 upload acks) and then a minimal DISP/UPDH
> service (msg 0x10 start → handle reply; 0x13/0x17 element ops →
> acks; 0x11 submit → completion on UPDH), including the CRC-32C
> check on received frames (reject with the §8.4 failure semantics).
> Guest proof: `tools/vchi_smoketest.c` (armv6 static, the musl.cc
> cross) run at the chain prompt — boots "the VC" through the §9
> exchange sequence via raw register pokes, then performs one §10
> update cycle (0x10 → 0x17 → 0x11 → UPDH completion), asserts every
> reply, prints SMOKE PASS. Exit criteria: (1) model+responder in
> the tree, chain still boots `e7:~#` with chain-regress PASS
> (task 124's harness — this is its first real use); (2) the
> smoketest passes at the prompt (transcript in the task file);
> (3) display-recon gains an as-built §11: the model's register
> coverage, the responder's scripted behavior, and a LOUD
> contract-vs-decode table (what hardware could contradict: the
> descriptor/ring layout, timing/IRQ details — the frame grammar and
> register map are decode-backed); (4) M2's hand-off block (what the
> Linux VCHI transport driver will bind to). Stop per the file's
> clauses. NO kernel changes. Commit when done.

## Why

The display decode is complete (steps 1-4) and the strategic path is
"keep the VC firmware, write a Linux VCHI host" — but that host
needs something to talk to before hardware time. A register-level
engine model + scripted responder gives the whole future stack (M2
transport driver, M3 boot+dispman client) an in-chain target, the
same way the OneNAND model unblocked task 104's chain. Everything
the responder speaks is decode-backed (§8.4 frames, §9 boot grammar,
§10 service grammar); the ONLY invented piece is the descriptor ring
(§8's honest OPEN), kept explicit as a CONTRACT.

## Method

1. Model per §8.3 (the Symbian accessor cluster is the behavioral
   spec — reread §8.1/§8.3 against each register before
   implementing); env-gated MESSI_DEBUG like ONENAND_DEBUG.
2. Ring CONTRACT: simplest format that satisfies the +0x1444/
   +0x144C/+0x1460 programming sequence — e.g. a cookie cell pointing
   at a descriptor array {buf_ptr, len, flags} in a fixed shared-RAM
   window; write it into §11 as ours.
3. Responder state machine: §9.3's states verbatim (stage posts →
   replies; the 0x01000001 sentinel exchange; upload acks; final
   count==3/answer==1), then the §10 DISP loop; CRC-32C on RX.
4. Smoketest: bare register access from userspace at the prompt
   (/dev/mem mmap of 0x20204000 — devmem-style; the chain's root has
   access), the §9+§10 sequences, assert-verbose; compile with the
   task-103 cross, copy into emmc.img's root (or load via the SD
   channel) so it's at the prompt.
5. Validate: chain-regress PASS, smoketest PASS, icount control
   spot-check; write §11 + M2 hand-off.

## Exit criteria

1-4 per the prompt (model+responder landed with regress green,
smoketest PASS transcript, §11 as-built + CONTRACT table, M2
hand-off block).

## If it does NOT work

- The §8.3 map proves insufficient to design a coherent ring around
  (+0x1444 semantics resist): land the register model alone + the
  blocker analysis in the task file, mark the responder OPEN, stop —
  that is a real result (it would mean M2 needs the phone-side
  descriptor capture first).
- Smoketest wedges on IRQ timing: poll-mode the test (mask the IRQ,
   drive completion by status polling) and note it — the IRQ path
   gets exercised properly in M2 with the real driver.

## Gotchas

- This validates OUR stack against OUR responder — say it in §11 in
  those words; the fidelity boundary is exactly the CONTRACT table.
- Keep the responder OUT of the default boot path (env-gated) so
  chain-regress's flow diff stays meaningful.
- PA/VA discipline: the model is PA 0x20204000; §8's VAs are
  Symbian kernel view (0xC6034xxx) — never mix them in one table.
- New QEMU source file → build.ninja surgery; rebuild order; the
  smoketest binary is a repo artifact (commit source; the binary
  itself can be rebuilt via the documented cross command).
- CRC-32C: implement the table-gen exactly as §8.4 decodes it
  (init 0xFFFFFFFF, reflected 0x82F63B78) so a future M3 uses the
  same code.
- Capstone trap / hex ADD-not-concatenate; commit locally; never
  push.

## Smoketest transcript (icount run, guest lines only; full log with the
model trace in emulation/qemu-e7sw/boot-messi-task125.log)

```
e7:~# md5sum /mnt/vchi_smoketest
f150e69a465ff77abdac8156b1cd27d7  /mnt/vchi_smoketest
e7:~# /mnt/vchi_smoketest
vchi_smoketest: task 125 — MeSSI model + VC responder proof
engine mapped @0x20204000
shared block 12 pages, PA[0]=0x0880a9000
bring-up: cfg 0x842, clk bit13, ctrl 0xB5F, tags, ring v1 (4 desc/ring), started
--- §9 boot exchanges ---
stage-1: post cellA, await the boot ROM's first word
  rx-post cellA  desc 0 armed
  ok: sentinel 0x01000001, image count 3 (the VC speaks first)
stage-2: post cellA+cellB, upload the loader, exchange headers
  rx-post cellA  desc 1 armed
  rx-post cellB  desc 2 armed
  upload loader: 8272 B in 3 chunks, header {len 0x2050|0x02000000, sig}
  replyA: count=3 answer=1 (loader running)
  replyB: count=1
main firmware: post cellA, upload img3, final exchange
  rx-post cellA  desc 3 armed
  upload firmware: 16384 B in 5 chunks, header {len 0x4000|0x02000000, sig}
  final: count=3 answer=1 -> VC BOOT COMPLETE
--- §10 service cycle ---
connect: 0x14-B control {0,1,0x1010,conn 0x1234,0x800}
  rx-post svc    desc 0 armed
  ok: peer echoed the connect message
DISP msg 0x10 (update start)
  rx-post svc    desc 1 armed
  ok: update handle 1
DISP msg 0x17 (element op, 7 base words)
  rx-post svc    desc 2 armed
  ok: element op acked
DISP bad-CRC probe (the §8.4 failure semantics)
  ok: frame rejected (error event, no reply)
DISP msg 0x11 (update submit {handle, flag})
  rx-post svc    desc 3 armed
  ok: completion arrived on UPDH with the right handle
HAT low, all events drained, pair enables never written (poll mode)
SMOKE PASS
```

QEMU-side lines of note from the same run (MESSI_DEBUG):

```
qemu-system-arm: info: bcm2727: MeSSI engine @0x20204000 (0x1480 window, §8.3
  map), IRQ 201 (ICU 0xC9), ring CONTRACT v1, VC responder ON, MESSI_DEBUG trace ON
messi: ring active: cookie 0x880a9000, 4 desc/ring, ch0 RX 0x880a9040 TX 0x880a9080, ...
messi: stage-1 exchange: sentinel reply + image count 3
messi: loader accepted — count=3/answer=1 + cellB reply
messi: responder: VideoCore boot complete (final count=3 answer=1)
messi: VCHI: MeSSI-16 CRC failure (ch0 frame type 0x50534944, 24 B payload) — frame rejected
```

Rebuild command for the binary (task-103 cross):

```
toolchain/musl.cc/armv6-linux-musleabi-cross/bin/armv6-linux-musleabi-gcc \
  -static -Os -march=armv6 -mtune=arm1176jzf-s \
  -o tools/vchi_smoketest tools/vchi_smoketest.c
```
