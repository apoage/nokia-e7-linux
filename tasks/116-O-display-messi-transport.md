# Task 116: display step 2 — MeSSI-16 transport decode (the mailbox the whole display path rides on)

## Status

**DONE 2026-09-01 — findings in docs/bcm2727-display-recon.md §8.**
Engine base CONFIRMED VA 0xC6034000 (PA 0x20204000); register map,
frame + CRC-32C spec, EHwClkMeSSI gate, candidate refutation table and
step-3 hand-off anchors all delivered there. No implementation touched.

**IDLE-TASK PROMPT (paste into the queue):**
> Execute tasks/116-O-display-messi-transport.md in /home/lukas/ps3/e7
> (read it and docs/bcm2727-display-recon.md first — §0/§2.2 the two
> VCHI transports, §4 anchor list, §5 plan, §7.6 the entry points task
> 114 handed over). One job: display step 2 — decode the MeSSI-16
> transport from `firmware-re/dumps/romdumpplus.dmp`: disassemble
> TControllerMessi + DVCMessiRx/TxChannel (ROM 0x329000-0x332000, the
> DVCDriver PDD region) and the VCHI host library (0x333000-0x33C000),
> working from the §7.6 anchors (channel sends 0x8032C800/0x8032C1A0
> via this+0xB4/this+0xB8 objects, wait helper 0x803294B4,
> BootParseMessage 0x8032D9EC, uploader 0x8032DB78). Deliver: (a) the
> MeSSI engine base — test the four unattributed DVCDriver MMIO
> candidates VA 0xC6032664/0xC6034000/0xC6034200/0xC6034240 against the
> icu-timer §4 early-map table and the xti-trace §3 dual-view rule
> (negative results recorded); (b) the register map (TX/RX data,
> status, enable/irq bits); (c) the frame format — header/length/
> channel-id fields and the CRC (the 'VCHI: MeSSI-16 CRC failure'
> string @0x335214 anchors the check — pin width, polynomial,
> position); (d) the EHwClkMeSSI clock gate register+bit (power-name
> table @0xCC328). Write display-recon new §8 'MeSSI-16 transport'
> (register map + frame diagram + CRC) and mark §5 step 2 DONE with a
> pointer; end §8 with the anchors step 3 (DVCDriver boot protocol)
> inherits. NO implementation, NO QEMU/kernel changes. Stop per the
> file's clause if the engine base cannot be pinned — deliver the
> framing + ranked candidates, never a guessed base. Commit when done.

## Why

Task 110's verdict: display is VideoCore-mediated, and the strategic
Linux path is "keep the VC firmware, write a VCHI host". Task 114
decoded WHICH firmware set boots and handed over the transport entry
points (§7.6). Everything downstream — the DVCDriver boot protocol
(step 3), the dispcontrol/dispmanx grammar (step 4), any Linux VCHI
host — rides on this transport. §5 calls it "the mailbox the whole
display path rides on": one engine, one framing, then every later task
is grammar work on top.

## Method

1. Disassemble around the §7.6 anchors (va 0x8032C800, 0x8032C1A0 —
   the channel-send methods; follow the this+0xB4/this+0xB8 objects to
   the TControllerMessi engine object and its register accesses).
   Generic capstone helper under `tools/` OK (task114_dis.py takes
   `<off> <len>` with va base 0x80000000 — exactly right here); scratch
   stays in /tmp.
2. From every MMIO access in the MeSSI paths, collect the VA constants;
   map each to PA via the icu-timer §4 early-map table / xti-trace §3
   dual view (0xC6038800←0x20208800 is the proven example — the rule
   is NOT uniform, use the table). Match against the four §2.2
   candidates; whichever window the accessors actually hit is the
   engine base. Record refuted candidates with the reason.
3. Reconstruct the register map from the accessor functions (data
   registers, status/poll bits, enable/irq, FIFO semantics — expect a
   16-bit-flavored engine given the '-16' name).
4. Reconstruct the frame format from the send path + the CRC-failure
   check: field layout, where the CRC sits, its width/polynomial
   (table-driven? in-code loop?), and what "MeSSI-16" names — the CRC
   width or the bus width. Cross-check against the VC-side transport
   strings (mphi_ccp2.c @0x2BAAA0 is the CCP2 cousin — reference
   only, § gotchas).
5. The clock: find EHwClkMeSSI's consumer — the resource-manager
   handle → register+bit the DVCDriver toggles.
6. Write §8, mark §5 step 2 DONE, hand anchors to step 3.

## Exit criteria

1. Engine base pinned CONFIRMED (VA + PA + the accessor evidence) — or
   all four candidates refuted on the record and the base OPEN with
   ranked alternatives.
2. Register map table (offset, width, R/W, decoded meaning).
3. Frame format diagram + CRC spec (width, polynomial, coverage,
   position in frame).
4. Clock gate register+bit (or OPEN with the searched range).
5. §8 written, §5 step 2 DONE pointer, step-3 hand-off anchors listed.

## If it does NOT work

- Base unpinable (e.g. accessors all go through one indirect pointer
  whose value isn't a literal): deliver framing + CRC + register map
  keyed on the accessor structure, mark the base OPEN with ranked
  candidates and what would settle it (phone probe / VC-side decode).
  Do NOT write a guessed base as CONFIRMED.
- If the CRC loop doesn't resolve to a standard polynomial, publish
  the exact instruction sequence — a verifiable oddity beats a
  mislabeled CCITT.

## Gotchas

- romdumpplus.dmp: va = file offset + 0x80000000. §4/§7.6 anchors are
  va — convert before slicing the dump.
- 0xC801xxxx = RAM cells (kernel static data), 0xC60xxxxx = kernel VA
  MMIO — different animals; map MMIO to PA before comparing with
  anything in the xti/onenand docs.
- Keep MeSSI (message transport) and CCP2 (bulk transport, 'VCHI: CCP2
  CRC failure' @0x335230, VC-side mphi_ccp2.c) SEPARATE — cross-
  reference, don't merge. This task is MeSSI only.
- The four candidate windows might be GPIO-bank or CCP2 blocks — a
  refutation is a deliverable, not a failure.
- Capstone constant-fold trap (`orr rX,rX,rX,lsr #N`).
- Rule 4: BCM2708/VC4 headers in firmware-re/videocore-ref/ are shape
  precedent ONLY — ive-era (VC3) addresses will differ.
- Commit locally (docs + task status); never push.
