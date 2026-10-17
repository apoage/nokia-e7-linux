# Task 108: NLoader USB flash-mode decode — boot-mode entry, MUSB base, protocol framing

## Status

**DONE 2026-08-26 — gate PASSED, decode complete, doc delivered.**
Driver code fully present (step-1 gate passes emphatically: the
whole MUSB stack 0x10024600-0x10026000 is linked in). Deliverable:
`docs/bcm2727-usb-flashmode.md`. Headlines: MUSBHDRC base
**0x20038800** (sys+0x8800; kernel VA 0xC6038800 confirmed in the
Symbian ROM next to its "KRoPower" dump string); mainline register
layout incl. ULPI viewport 0x74-0x76 and HSDMA at +0x200 (BCM2727
channel-reg order CONTROL@+4/ADDR@+8/COUNT@+0xC); **polled, no ICU
irq in NLoader**; boot-source latch nibble [0x20040400] bits 20-23
→ 16-case switch (8 = "USB -> Update Mode" → 5 s × ~10 warm-reset
host window); flash framing level 1 = **control OUT vendor requests
with 1..1012-byte data stage** (12-byte header at 0x10007750 +
payload at 0x1000775C, handed up via the staged 0x100070ac callback
table); descriptors in the staged 0x100070xx block — **VID/PID bytes
OPEN** (not in nloader.bin; phone-side catch will settle). Runtime
gdb confirmation of the Normal path recorded (§1.4: latch nibble 0,
reason 0x64 → [0x30003e7c]).

**IDLE-TASK PROMPT (paste into the queue):**
> Execute tasks/108-O-usb-flashmode-decode.md in /home/lukas/ps3/e7
> (read it, docs/rom-boot-research.md incl. both dated notes, and
> lore/daily/2026-08-26-gdbstub-musb-lead.md first). One job: decode
> NLoader's USB flash-mode from firmware-re/analysis/nloader.bin —
> (1) the boot-mode entry conditions ("ADL -> Local Mode" / "USB ->
> Update Mode" ...), (2) the MUSB controller register base + IRQ id,
> (3) the USB descriptor table, (4) the first level of the flash
> protocol's command framing. Deliver docs/bcm2727-usb-flashmode.md
> with the house CONFIRMED/CONTRACT/OPEN legend. Optionally
> runtime-confirm the mode-selection path with gdb-multiarch against
> the chain. Do NOT write a QEMU USB model or Linux driver, do NOT
> touch Phoenix, and do NOT change the booting chain or its blobs.
> Stop per the file's clause if the USB driver code turns out absent
> from the binary. Commit when done.

## Why

The flasher boxes' channel (Phoenix/JAF/ATF "dead phone USB flashing")
has its **device side inside NLoader** — confirmed 2026-08-26 by
strings: a full Mentor **MUSB** OTG driver, the flash-mode descriptor
"Nokia Mobile Device (NLoader mode)", and the boot-mode selection that
trips it. Decoding it here gives the wire protocol without Phoenix
DLLs or smartcards (the FPS10/DK2 walls guard the *service* tools, not
the dead-phone flash channel), pins the USB controller contract for
the future Linux MUSB glue (controller IP is public MUSBHDRC —
kernel/linux/drivers/usb/musb/), and writes the brief for the
phone-side observe-only ROM-mode catch (rom-boot-research §2 note:
the OMAP protocol story is unverified for BCM2727).

## Anchors (file offset in nloader.bin; vaddr = 0x10020000 + offset)

| off | vaddr | string / role |
|-----|-------|---------------|
| 0x2f5c | 0x10022f5c | "ADL -> Local Mode" |
| 0x2f70 | 0x10022f70 | "USB -> Update Mode" |
| 0x2f98 | 0x10022f98 | "Forced Local Mode" |
| 0x2fd8 | 0x10022fd8 | "FOTA -> Update Mode" |
| 0x3010/0x3034 | — | "Initialise USB delay counter" / "USB delay counter : %d" |
| 0x4c40 | 0x10024c40 | "USB Serial Number: %s" |
| 0x4c78 | 0x10024c78 | "Nokia Mobile Device (NLoader mode)" — descriptor table nearby |
| 0x529c / 0x57c0 / 0x5d80 | — | "MUSB: Looping for too long" / "No MUSB DMA channels available" / "USB reset received!" |
| 0xb80 / 0xc14 / 0xd10 | — | CHRG_DET pin / charger-detect prints (USB-host vs wall) |

## Steps

1. **Gate — is the driver code present?** xref the error strings to
   code (literal pools / adr). The OneNAND precedent: SWBL's LLD was
   an ERASED gap in our dump — if the MUSB driver code is likewise
   absent, write that finding and STOP (the phone-side catch becomes
   the only path).
2. **Mode entry.** Find who prints the mode lines (XTI printf format
   xrefs) and decode the condition chain: startup-reason nibble vs
   charger detection vs a host command. Known adjacent machinery: the
   SUSD→nibble-2 determiner at 0x100318f2 and the reason re-read at
   0x10022d42 (onenand doc §5.2 item 2). What selects Local/Update?
3. **MUSB base + IRQ.** Collect MMIO constants from the usb_hw_* code
   region; match the register layout against
   `kernel/linux/drivers/usb/musb/musb_regs.h` (MUSBHDRC: FADDR/POWER/
   INTRTXE.../FIFO windows). Cross-check the XTI peripheral map
   (sys+0x25Axx claim, bcm2727-xti-trace.md §"peripheral map") and the
   kernel early-map table (icu-timer doc §4). Identify the ICU irq id
   if routed.
4. **Descriptor table.** Dump around 0x10024c78: full string/device
   descriptors — expect VID 0421 and the flash-mode PID (0x0105-class
   per rom-boot-research §1).
5. **Framing (first level only).** The EP0/bulk command loop: what
   does a host send to be answered, what's the first response. Enough
   to brief the phone-side catch session and a future QEMU MUSB model
   — NOT the full flash command set.
6. **Optional runtime confirmation.** gdbstub works now (gdb-multiarch
   + `-s`): hbreak the mode-selection site, boot the chain, watch the
   Normal path with reason 0x64, detach (chain then boots to `e7:~#`).

## Deliverables

- `docs/bcm2727-usb-flashmode.md` — decode + contracts, legend-marked.
- This task's Status → outcome; HANDOFF desk item 3 pointer update.
- Commit (docs + task file only; nothing in the QEMU tree).

## If it does NOT work

Driver code absent (step-1 gate fails), or the base cannot be pinned
from binaries: write the negative result + evidence into the new doc's
OPEN section and stop — do not guess a base, do not start sniffing
experiments on hardware from an idle task.

## Gotchas

- Capstone `orr rX,rX,rX,lsr #N` constant-folds (tools/gpio_static.py
  warning) — re-derive shifted constants by hand.
- nloader.bin is Thumb, base 0x10020000; XTI prints are post-MMU.
- gdbstub: use `gdb-multiarch` (`target remote :1234`, `-s -S` on
  QEMU) — the old "gdb lacks ARM" note is resolved (2026-08-26).
- QMP not HMP for any memory dumps; /tmp leftovers unreliable.
- Commit locally; QEMU tree is gitignored by convention.
