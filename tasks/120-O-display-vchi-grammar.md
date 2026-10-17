# Task 120: display step 4 — the VCHI dispcontrol/dispmanx grammar (the client-writable protocol)

## Status

**DONE 2026-09-04 — findings in docs/bcm2727-display-recon.md §10.**
Stop clause NOT triggered: message ids are static (u32, 2-0x26, built by
user-side rdispman.dll — the kernel is a pass-through). Delivered: the
post-boot module chain (Gce/rdispman → vcdispmanldd.ldd → vcdispmanvchi
→ vchi.dll → vcdriver.dll, reconstructed from XIP import veneers), the
§9.11 Start-consumer identified (vchi.dll dispatcher 0x803370B0/
0x80336FB0, TX via export 0x8032D6B0 with slot from the message route
nibble), the service table ("DISP" 0x50534944 + "UPDH" 0x48445055 by
vcdispmanvchi; "DSPC" dispcommand, "BUFM"/"XMFB" bufman, "CRMI" rmi;
VC-side names DISPCONT/DISPCOND @dispc_q, TVOUT), connection setup
(0x14-B connect {0,1,0x1010,conn_id,0x800}), the two-level wire framing
(§8.4 outer frame + 0x10-B per-message record {0,len,fourcc,0,0,plen} +
{msgid,args} payload), the full LDD request table (sync 20-case + async),
the message-id table with payload sizes and confidence labels, the
receive/callback machinery (UPDH → DVCDispmanUpdateList →
DISPMAN_LDD_UPDATE_MUX / DISPMAN_LDD_FRAMECOUNT_MUX), the bulk-vs-message
split (all composition = short-message ≤0x1000 flags 0; bulk = bufman/
EGL resource path, ids CONFIRMED static), and the minimal-client
contract. §5 step 4 marked DONE; step-5 (VPU ive_dsi.c) hand-off in
§10.8. OPENs recorded: tv_out ARM-side fourcc, UPDH payload word
grammar, resource-handle allocation path. No implementation touched.

**IDLE-TASK PROMPT (paste into the queue):**
> Execute tasks/120-O-display-vchi-grammar.md in /home/lukas/ps3/e7
> (read it and docs/bcm2727-display-recon.md first — §2.1/§2.2 the
> service inventory and ARM-side anchors, §8 the MeSSI transport and
> §8.4's VCHI frame, §9 the boot protocol, and §9.11's step-4
> hand-off). One job: display step 4 — decode the post-boot
> display-service VCHI grammar from the ARM side of
> firmware-re/dumps/romdumpplus.dmp: the DVCDispmanVCHI kernel client
> (0x346540-0x347EF1) constructs every message the VC display service
> consumes; start where §9.11 points (the boot object's consumer
> registered through Start — the caller side of the 0x3316E0 thunk
> table — where dispcontrol/dispmanx messages surface first), then the
> vcdispman LDD (0x429808-0x4298F4) for the user↔kernel handle shapes,
> and the Gce composition backend (0xD4DCA8-0xD5AF78) for who-calls-
> whom. Deliver display-recon new §10 'VCHI display-service grammar':
> service enumeration (dispcontrol / dispservice / endpoint_screen —
> VC-side names per §2.1 — with their service ids/fourccs), connection
> setup, message-id tables and struct layouts for display-open,
> element add/remove/update, update-commit, and vsync-callback
> registration, the bulk-vs-message split per operation, and the
> minimal-client contract — exactly what a Linux host must implement
> to get a framebuffer on the panel. Rides §8.4's frame (0x1C-B
> header + payload + CRC-32C at +0x0C); the boot magic pair
> 0xAFE45251/0xF001BC4E is boot-protocol-only per §9.11 — do not
> expect it post-boot. Mark §5 step 4 DONE with a pointer and end §10
> with the step-5 (VPU ive_dsi.c) hand-off. Keep the khronos/EGL
> service (0x42E660+) OUT of scope — cross-reference only. NO
> implementation. Stop per the file's clause if message ids turn out
> runtime-assigned with no static tables — deliver the frame + the
> discovery-protocol shape, mark the grammar OPEN. Commit when done.

## Why

Steps 1-3 decoded WHAT firmware boots, the TRANSPORT it rides, and
the HANDSHAKE that brings the VC up. Step 4 is the actual display
API — the messages that open a display, add elements, and commit
updates. This is the decode the whole display plan exists for: the
strategic path is "keep the VC firmware, write a Linux VCHI host",
and §5 sized this step as the endpoint — "a protocol doc sufficient
to write a Linux VCHI host client". After this, only the VPU-side
panel driver (ive_dsi.c, step 5) remains, and it is optional for a
first framebuffer.

## Method

1. Start at §9.11's pointer: the boot object's four logical slots
   (consumer callback + status 0x4200 + descriptor {len,ptr,extra},
   0x8032D12C/0x8032D23C; sends via export 0x8032D6B0) — decode the
   consumer registered through Start (caller side of the 0x3316E0
   thunk table). That callback chain is where post-boot service
   messages first surface on the ARM side.
2. The DVCDispmanVCHI client (0x346540-0x347EF1): every send site
   constructs a message — enumerate them (message builder functions,
   their id/struct constants), and the update-callback handler +
   update lists for the receive side. Anchor strings: 'DVCDispmanVCHI:
   VCHI Host Lock', 'VCHI Message Available Semaphore' (§2.2).
3. Service enumeration: match ARM-side service registration against
   the VC-side names of §2.1 (dispcontrol.vll @0x31A3D8,
   vchi_dispcontrol_server_init @0x31A3E8, vchi_dispservice_x_init
   @0x31A45B, endpoint_screen.vll @0x31A405, dispc_q @0x2E7EFC) —
   service ids/fourccs, connection-open sequences, endpoint routing.
4. vcdispman LDD (0x429808+): the ioctl/request shapes the window
   server issues (DISPMAN_LDD_UPDATE_MUX, DISPMAN_LDD_FRAMECOUNT_MUX)
   — these define the kernel-client API the grammar serves; the Gce
   backend (0xD4DCA8-0xD5AF78) shows the call order on real use
   (open → create → update → commit, vsync wait).
5. For every message: id, direction, payload struct (field offsets,
   widths), bulk vs short-message path, and the reply/callback shape.
   Express payloads on top of §8.4's frame; note which fields are
   runtime-negotiated handles (CONTRACT, not CONFIRMED).
6. Write §10 with the tables + the minimal-client contract (the
   shortest sequence of exchanges that yields a visible framebuffer);
   mark §5 step 4 DONE; write the step-5 hand-off block.

## Exit criteria

1. Service table: name, id/fourcc, open sequence, endpoints.
2. Message tables: id/direction/struct/bulk-split for display-open,
   element add/remove/update, update-commit, vsync callback — or the
   explicit runtime-assigned finding per the stop clause.
3. The minimal-client contract (the write-a-Linux-host checklist).
4. §10 written + §5 step 4 DONE pointer + step-5 hand-off +
   reproducibility block.

## If it does NOT work

- Ids runtime-assigned (opaque handles from a discovery exchange):
  decode the discovery protocol itself (that IS the deliverable then),
  publish the exchange shape, mark the per-message grammar OPEN, stop.
- If the client routes through compression/opaqueness beyond framing,
  deliver the connection layer + what decodes, mark the payload
  grammar OPEN with the exact opaque bytes recorded.

## Gotchas

- va = file offset + 0x80000000 — ADD, never concatenate (the named
  hex hazard; onenand §7.4 + display-recon §8.6 both carry it).
- Backward literal pools (`ldr rX,[pc,#-imm]`) hide from capstone's
  linear sweep — decode LDR words manually (§8.6).
- Keep the services SEPARATE: dispcontrol vs dispmanx vs
  endpoint_screen vs khronos — a merged/wrong attribution poisons the
  client contract. Khronos/EGL (0x42E660+) is out of scope.
- VC-side strings are naming anchors only — VPU payloads are not
  disassemblable with repo tools; don't try.
- 0xC801xxxx cells are kernel RAM (service handles etc.); their
  initial values may legitimately be absent from the dump.
- Capstone constant-fold trap (`orr rX,rX,rX,lsr #N`).
- Commit locally (docs + task status + any generic tool); never push.
