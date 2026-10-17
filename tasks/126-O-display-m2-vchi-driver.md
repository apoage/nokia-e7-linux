# Task 126: display arc M2 — the Linux VCHI transport driver (bcm2727-vchi)

## Status

**DONE + VALIDATED — executed 2026-09-06.** All four exit criteria met;
as-built record: docs/bcm2727-display-recon.md §12 (driver shape, the
ABI, the poll/IRQ story, deviations from §11.6 — two walls found in
validation, both driver-side). Evidence:
`emulation/qemu-e7sw/boot-vchi-task126.log` (four full boots: {poll,
IRQ driver mode} × {TCG, icount}, each with the smoketest transcript;
plus the four chain-regress gates and the ICU-201-idle proof).

Results vs the exit criteria:

1. **chain-regress PASS with default envs** (TCG + icount: boot 27-28 s
   each, 1202-line nloader[xti flow byte-identical — zero new flow
   lines; driver probes idle, `/proc/interrupts` shows ICU 201 claimed
   with count 0 = no stray interrupts) **and chain-regress PASS with
   MESSI_RESPONDER=1** (TCG + icount, flow byte-identical).
2. `tools/vchi_smoketest_drv.c` (the §9-boot + §10-update + bad-CRC
   sequences of tools/vchi_smoketest.c, through /dev/bcm2727-vchi)
   prints **SMOKE PASS in BOTH driver modes** — {poll, IRQ} ×
   {TCG, icount}, always with MESSI_RESPONDER=1, at the e7:~# prompt.
3. Evidence log committed: `emulation/qemu-e7sw/boot-vchi-task126.log`.
4. docs: display-recon §12 'M2 as-built' written + a pointer added at
   the end of §11.6. HANDOFF untouched.

Walls found and fixed (both kernel-side, decode + model untouched):

- IRQ-mode ch1 completions never woke the SEND wait: the ISR stored the
  odd-channel event half shifted DOWN (`st >> 16`) while the wait
  masked `bits << 16` — the latch and the mask never met. Fixed by
  storing latched bits in half position (§12.4 item 4).
- The final "everything drained" check failed with master-ack bit 0
  set: a reply delivered during a SEND's doorbell (into an RX
  descriptor a timed-out RECV left armed) left the RX-available event
  unconsumed by anyone's wait. The consuming RECV now clears it
  (§12.4 item 3).

Landed: `kernel/linux/drivers/char/vchi-bcm2727.c` (CONFIG_BCM2727_VCHI
=y, misc chardev /dev/bcm2727-vchi, IRQ-driven default +
`vchi-bcm2727.poll=1` fallback), uapi header
`kernel/linux/include/uapi/linux/bcm2727_vchi.h` (SEND/RECV/WAIT,
documented), DT node `messi@20204000` (interrupt <201>), defconfig +
kernel/configs mirror, `tools/vchi_smoketest_drv.c` on the SD channel.
NO QEMU changes.

**IDLE-TASK PROMPT (paste into the queue):**
> Execute tasks/126-O-display-m2-vchi-driver.md in /home/lukas/ps3/e7
> (read it and docs/bcm2727-display-recon.md first — §8 the engine
> decode, §10.7 the minimal-client checklist, §11 the model as-built
> with the CONTRACT-v1 ring and §11.4 honesty table, and §11.6 the
> M2 hand-off, which is the bring-up spec). One job: the Linux VCHI
> transport driver for the MeSSI engine, validated against task
> 125's responder. Kernel side: a platform driver + misc char device
> (`/dev/bcm2727-vchi`, house naming per i2c/mmc/musb precedents) —
> DTS node messi@20204000 with interrupt <201> (DT int = ICU id
> convention), defconfig entry + kernel/configs mirror. Bring-up per
> §11.6 verbatim: ioremap the 0x1480 window, allocate the ring +
> buffers DMA-coherent, program +0x144C (ring-header PA, 'MSR1'),
> +0x1444 = 6, +0x1460 = 1, init the four channels (0xFF01002C +
> (ch<<16)|0x3210 tags), start via +0x240 bit6 (bit0 is a
> rising-edge soft reset — NEVER write it casually) and +0x40 bit19;
> IRQ mode per §11.6 item 2 (enable pair bits, service ICU 201 by
> the read-ack-W1C-drain sequence) with a poll-fallback module
> parameter (the memcard/i2c precedent shape) — bring up poll-first,
> then IRQ. Userspace ABI: MINIMAL synchronous frame-level —
> ioctl(SEND {buf,len,channel,tag}) blocking until completion,
> ioctl(RECV {buf,maxlen,channel}) returning one frame, ioctl(WAIT
> optional) — no async queueing, that is hardware-era work; document
> the ABI in the driver header. Probe MUST be harmless with no
> responder (no events → probe OK, device idle). Exit criteria:
> (1) chain-regress PASS with default envs (driver probes idle, zero
> new flow lines) AND a regress-boot with MESSI_RESPONDER=1 stays
> green; (2) a ported proof `tools/vchi_smoketest_drv.c` (same
> §9-boot + §10-update + bad-CRC sequences as the committed
> tools/vchi_smoketest.c, but through /dev/bcm2727-vchi instead of
> /dev/mem) prints SMOKE PASS at the e7:~# prompt under TCG and
> -icount with MESSI_RESPONDER=1, in BOTH poll and IRQ driver modes;
> (3) evidence log committed (boot-vchi-task126.log); (4) docs:
> display-recon new §12 'M2 as-built' (driver shape, ABI, the
> poll/IRQ story, deviations from §11.6 if any) + a pointer from
> §11.6; HANDOFF untouched (main session folds). Stop per the
> file's clauses. Commit when done.

## Why

M1 gave the display arc a register-level engine and a scripted VC to
talk to; M2 is the kernel piece every later client rides. After M2,
the smoketest's raw /dev/mem pokes retire in favor of a real driver
— the same software shape hardware will run — and M3 (the VC-boot +
dispman client) becomes a pure userspace job. The responder
acknowledges every decoded message id, so the driver can be brought
up message-by-message with full visibility.

## Method

1. DTS + defconfig + driver skeleton (misc chardev + platform
   driver; probe maps the window, does the §11.6 init sequence,
   leaves IRQs off until mode selection).
2. Poll-mode path first: SEND = descriptor + doorbell (control-word
   bit0) + status drain; RECV = ring drain. Port the smoketest onto
   the ABI; iterate against the responder with MESSI_DEBUG=1.
3. IRQ mode: request_irq(201), pair enables, the §11.6 item-2 ack
   sequence; keep poll as the module-param fallback. Verify no stray
   IRQ 201 in the default (no-responder) boot via /proc/interrupts
   in the regress run.
4. Validate per exit criteria; commit evidence + docs.

## Exit criteria

1-4 per the prompt (regress green both envs, driver-mode SMOKE PASS
×{poll,IRQ} ×{TCG,icount}, evidence log, §12 as-built).

## If it does NOT work

- IRQ mode walls (level semantics, spurious 201): land poll mode +
  the failure analysis, mark IRQ OPEN with the trace — poll mode is
  a legitimate M2 landing (the smoketest precedent), hardware brings
  the IRQ truth.
- The CONTRACT-v1 ring resists kernel-side use (coherency, cache
  maintenance on the v6): the model is ours — if a ring detail must
  change, change it IN THE MODEL + §11 doc, never driver-side-only
  (both ends stay in sync; note the change as a CONTRACT v1.1 with
  the reason).
- ABI design doubts: stay minimal-synchronous per the prompt; a
  richer interface is deliberately out of scope.

## Gotchas

- +0x240 bit0 is a rising-edge soft RESET (the task-125 wall) —
  init writes 0xB5F exactly once per §8.3, never RMW-blindly.
- DMA-coherent allocations + explicit cache maintenance where the
  v6 needs it (the memcard FIFO precedents); the ring lives in
  guest-visible RAM the model reads directly.
- The responder is env-gated — every validation boot needs
  MESSI_RESPONDER=1; every regress boot must NOT have it.
- New kernel file → defconfig mirror sync (house convention);
  interactive checks through the fifo pattern (chain-regress.sh has
  the working timing).
- Hex arithmetic ADD-not-concatenate; commit locally; never push.
