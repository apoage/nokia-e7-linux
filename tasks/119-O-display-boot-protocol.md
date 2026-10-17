# Task 119: display step 3 — DVCDriver::Boot() message grammar (the boot-ROM handshake)

## Status

**DONE 2026-09-03 — findings in docs/bcm2727-display-recon.md §9.**
Full Boot() state machine + message tables delivered: 0x20-byte
magic-framed exchange slots (0xAFE45251/0xF001BC4E, response word
{count<<24|answer}), stage phases are receive-posts (length zeroed) not
TX blobs, the only APE→VC payload = the two cmd-0x42 image uploads
(data + header with (len&0xFFFFFF)|0x02000000 + 20-B signature), replies
via MeSSI IRQ 0xC9 → completion counter (status 0x2A00 & 0x201) → 5 ms
poll (500 ms reply budgets, 5 s image budgets, no retries), HAT = GPIO
attention latch [B8+0x20], final success = count==3/answer==1. Stage-blob
OPEN resolved by refutation: cells are BSS-zero (vcdriver data cell
0xC8010500, ROM copy 0x331370). §5 step 3 marked DONE; step-4 hand-off
anchors in §9.11. No implementation touched.

**IDLE-TASK PROMPT (paste into the queue):**
> Execute tasks/119-O-display-boot-protocol.md in /home/lukas/ps3/e7
> (read it and docs/bcm2727-display-recon.md first — §2.2 the DVCDriver
> strings, §5 the plan, §7.4 Boot's image flow, §7.6 the message
> anchors, and §8 the MeSSI transport task 116 just decoded — §8.4's
> frame format and §8.7's hand-off are the entry points). One job:
> display step 3 — decode the DVCDriver::Boot() boot-ROM protocol from
> `firmware-re/dumps/romdumpplus.dmp`: the exact message words for
> stage-1/stage-2 upload, the HAT handshake, image-ID negotiation,
> 2nd-stage-loader start, the main-firmware upload, and the final
> state negotiation. Work from the anchors: Boot 0x8032DCB0, uploader
> 0x8032DB78, image upload 0x8032DAA0, channel sends 0x8032C800 /
> 0x8032C1A0, BootParseMessage 0x8032D9EC, wait helper 0x803294B4
> (1000 ms × 5), boot-protocol object this+0xBC (ctor 0x8032D8C4,
> vtable 0x80331168, 4×0x24 reply slots at +0x1B0), boot strings
> 0x32DBC0-0x32E226, stage-blob cells 0xC801055C/0xC801057C, command
> byte 0x42, destination word (x&0xFFFFFF)|0x02000000, final
> count==3/answer==1. Determine which transport each phase rides
> (MeSSI sends vs the CCP2 bulk path — img3 is ~1 MB, do not assume
> it rides MeSSI) and how replies surface (MeSSI IRQ 0xC9, ISR
> 0x8032F104, demux 0x8032F02C — vs the wait helper's polling).
> Express every message as a §8.4 frame: the 0x1C-byte header fields
> + the payload grammar byte by byte. Deliver display-recon new §9
> 'DVCDriver boot protocol' (state machine + message tables + reply
> formats + timeout/retry semantics), mark §5 step 3 DONE with a
> pointer, and end §9 with the anchors step 4 (VCHI grammar) inherits;
> re-check the stage-blob initial-values OPEN (0xC801055C/0xC801057C)
> with the transport now decoded — if still unresolvable statically,
> say what the grammar implies about their structure. NO
> implementation. Stop per the file's clause if the payload bytes are
> obfuscated beyond the framing — deliver framing + state machine +
> what decodes, mark the payload grammar OPEN. Commit when done.

## Why

Step 1 (task 114) inventoried WHICH firmware boots; step 2 (task 116)
decoded the transport it rides. Step 3 is the handshake itself: the
message words that bring the VideoCore from cold boot ROM to running
Nucleus firmware. It is the last decode before the grammar work (step
4, dispcontrol/dispmanx) and the reference for any future Linux-side
"boot the VC" implementation — the strategic path is "keep the VC
firmware, write a VCHI host", and that host must first SPEAK this
boot protocol.

## Method

1. Disassemble Boot 0x8032DCB0 and everything it calls (uploader
   0x8032DB78, 0x8032DAA0, 0x8032E090/0x8032E0BC, BootParseMessage
   0x8032D9EC, wait helper 0x803294B4), plus the boot-protocol object
   (ctor 0x8032D8C4, its vtable 0x80331168, the 4×0x24 reply slots at
   +0x1B0 — reply slot shape = the parse target). Generic capstone
   helper under `tools/` OK (task114_dis.py, va base 0x80000000);
   scratch in /tmp.
2. String-anchor the state machine: every print in 0x32DBC0-0x32E226
   ("VideoCore boot ROM not responding (stage 1/2); HAT=%08x",
   "VideoCore image ID found/not found", "Failed(%d) to start 2nd
   stage boot loader", "no images returned from DVCExe" …) → the code
   path that prints it → the transitions around it.
3. For each exchange: sender side (which channel object, which send
   fn 0x8032C800 vs 0x8032C1A0 — and whether the big image phases
   switch to the CCP2 path: check DVCCcpRxChannel usage and what
   §2.2's "CCP2 = bulk: firmware, bufs" implies; the 8 KB loader and
   ~1 MB firmware may ride different transports), the frame's header
   fields per §8.4, the payload bytes (command 0x42 family, dest word,
   lengths, the 20-byte signature ride-along), and the expected reply
   (shape of the 0x24-byte slots, how BootParseMessage fills them,
   count==3/answer==1 semantics).
4. The attention/receive path: does Boot poll via the wait helper
   (1000 ms × 5) or is it woken by the MeSSI ISR (0xC9 → 0x8032F104 →
   demux 0x8032F02C → which callback)? Decode "HAT" while at it — the
   strings print HAT=%08x; determine what the HAT value is (a status
   word from the VC? a GPIO read? §2.2's VC→AP attention line).
5. Re-visit the stage-blob cells 0xC801055C/0xC801057C with the
   grammar in hand: even if the initial values stay OPEN (kernel-build
   relocated constants), the upload path's structure constrains what
   they must contain — record the implied shape.
6. Write §9 (state machine diagram + one table per exchange: dir,
   transport, frame fields, payload bytes, reply, timeout/retry);
   mark §5 step 3 DONE; write §9's hand-off block for step 4.

## Exit criteria

1. State machine: every boot-ROM exchange in order, with the print
   that anchors it and the failure branch (which print, what Boot
   does — retry via the wait helper, or fail the whole Boot).
2. Message tables: each frame's §8.4 header values + payload byte
   grammar (as far as decodable), reply slot shapes, the final
   negotiation (count==3/answer==1) pinned.
3. Transport attribution per phase (MeSSI vs CCP2), with evidence.
4. The HAT mechanism identified (or explicitly OPEN with the searched
   paths).
5. Stage-blob structural implications recorded (values may stay OPEN).
6. §9 written, §5 step 3 DONE pointer, step-4 hand-off anchors;
   reproducibility block.

## If it does NOT work

- Payload bytes opaque (checksummed/scrambled beyond the §8.4 CRC):
  deliver the framing + state machine + reply shapes, mark payload
  grammar OPEN with the exact opaque bytes on record — do not guess
  opcodes into the doc as CONFIRMED.
- If Boot's code turns out to route through a table of function
  pointers whose targets aren't statically resolvable, decode what is
  reachable, enumerate the unresolved slots with their likely roles,
  stop.
- VPU-side cross-checks are OPPORTUNISTIC ONLY: the 8 KB VC loader
  images are VPU code (no disassembler in the repo) — byte-pattern
   checks (e.g., locating the 0x42 handler) are welcome, deep VPU
  decode is out of scope.

## Gotchas

- va = file offset + 0x80000000 — ADD, never concatenate (the named
  hex hazard, onenand §7.4 + display-recon §8.6 both carry it).
- Backward literal pools: the accessor cluster loads `ldr rX,
  [pc, #-imm]` — capstone's linear sweep hides them; decode LDR words
  manually (§8.6).
- 0xC801xxxx = kernel RAM cells; their initial values may genuinely
  not be in the dump (task 114's finding) — don't chase phantoms.
- Keep MeSSI and CCP2 attributions separate; a wrong "it's all
  MeSSI" would poison step 4.
- Capstone constant-fold trap (`orr rX,rX,rX,lsr #N`).
- Rule 4: BCM2708/VC4 precedents in firmware-re/videocore-ref/ are
  shape references only.
- Commit locally (docs + task status + any generic tool); never push.
