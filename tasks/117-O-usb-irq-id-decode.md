# Task 117: USB kernel IRQ id decode — the last decode gap before the Linux MUSB glue

## Status

**EXECUTED 2026-09-01 — CLOSED. Result: two ids CONFIRMED —
MUSB core = ICU 0xB7/183, MUSB HSDMA = ICU 0xD7/215 (split ids,
separate handlers; DUsbRapuInterruptHandler class in the Symbian USB
PDD module ROM 0x80173060-0x80191900). OTG glue block 0x20025Axx is
NOT covered by either — the separate DUsbOtgDriverFactory module
binds 0x63/0x64/0x65 (candidates recorded, per-id attribution OPEN).
Full decode + reproducibility: docs/bcm2727-usb-flashmode.md §10;
§6 item 3 closed, §5 updated.**

**IDLE-TASK PROMPT (paste into the queue):**
> Execute tasks/117-O-usb-irq-id-decode.md in /home/lukas/ps3/e7 (read
> it and docs/bcm2727-usb-flashmode.md first — §2.1 the base + the
> kernel-side literal evidence, §2.5 the no-NLoader-IRQ finding, §5/§6
> the glue prerequisites and open items). One job: close §6 item 3 —
> decode the kernel-side USB interrupt id from the Symbian ROM
> (`firmware-re/dumps/romdumpplus.dmp`): locate the Symbian USB OTG
> PDD from its 14 0xC6038800 literal-pool hits (0x80174530 sits next
> to the '  KRoPower              = 0x%02x' register-dump string; also
> pools at 0x8017b050/0x8017dffc — do NOT trust §2.5's 0x800B7C00-
> 0x800B9E00 range blindly, locate from the pools), find its interrupt
> bind/dispatch registration (Kern::InterruptBind-class call or the
> ASSP/variant interrupt table — the same method that confirmed
> I2C0 = ICU 0x96, docs/bcm2727-i2c.md), and extract the numeric id.
> Sanity-map it into the ICU id space (DT int = ICU id numerically;
> live ids in the chain: tick 0xF2/242, uart 0x1A3/419, mmc 0x23/0x24,
> i2c 0x96-0x98; the ICU carries 421 ids) and look for a second id
> covering the OTG system-glue block 0x20025Axx (VBUS/ID pins) and/or
> the HSDMA block. Decode the ISR shape while there: single MUSB id
> reading INTRUSB/INTRTX/INTRRX (+ HSDMA status at +0x200), or split
> ids? Deliver: §6 item 3 closed in bcm2727-usb-flashmode.md + new
> §10 'USB interrupt decode' (id(s), trigger type if visible, handler
> shape, ack sequence) + §5's glue prerequisite list updated (irq id
> no longer OPEN). NO kernel/QEMU changes — decode only. Stop per the
> file's clause if the binding is dynamic/table-fed and the table is
> not in the dump. Commit when done.

## Why

The parked "USB Linux MUSB glue" queue item has every decode
prerequisite done EXCEPT the irq id: base (0x20208800 PA), full
register layout vs mainline musb_regs.h, ULPI viewport, HSDMA +
0x200 with the BCM2727 channel order, clock/pad bring-up, the
0x20038C00 PHY block — all decoded in task 108. §2.5 proved NLoader
is polled, so the id can only come from the Symbian ROM driver — the
I2C0 precedent (ICU 0x96 confirmed from the ROM I2C driver's bind)
shows the method works. With this id, a future glue task needs no
further decode. (The glue itself stays a separate, later task — it
needs a QEMU MUSB model or hardware to validate; do not start it
here.)

## Method

1. Slice the ROM around the 0xC6038800 pools (0x80174530,
   0x8017B050, 0x8017DFFC); find the driver's bounds via the pool
   neighborhoods and its string cluster (register-dump format
   strings). Generic capstone helper under `tools/` OK (va base
   0x80000000 — task114_dis.py fits as-is); scratch in /tmp.
2. Hunt the interrupt registration: Symbian kernel-side drivers bind
   via the variant/ASSP layer — look for (a) an InterruptBind/
   InstallHandler-style call with an immediate id, (b) a table of
   {id, handler} records, (c) HAL/property lookups feeding a bind.
   The I2C0 decode (docs/bcm2727-i2c.md — its §5 method note) is the
   worked example.
3. Cross-validate the id three ways before calling it CONFIRMED:
   numeric range sanity (ICU has 421 ids), no collision with the ids
   the chain already claims, and (if present) a second independent
   use of the same constant inside the driver (e.g. disable paths).
4. Decode the ISR: entry → which status registers are read/acked
   (INTRUSB 0x0A / INTRTX 0x02 / INTRRX 0x04 / HSDMA +0x200), EOI
   shape — this is the driver-side evidence for how Linux glue should
   wire one IRQ (or several).
5. The OTG-glue block (0x20025Axx / VA 0xC6025Axx, xti-trace §4's
   "USB OTG" entry — VBUS/ID/pins/clock): check whether the same
   driver (or its neighbor) binds an id for it; if yes record it as
   the extcon-class interrupt the Linux side will also want.
6. Write §10, close §6 item 3, update §5's prerequisite list.

## Exit criteria

1. Driver region located (bounds + how determined).
2. The bind decoded: numeric id CONFIRMED (with the three-way
   validation) — or the dynamic-table finding documented with the
   table's location and what IS decodable.
3. ISR shape decoded (status reads + ack sequence; single vs split
   ids), trigger type if visible.
4. §6 item 3 closed, §5 updated, §10 written with a reproducibility
   block.

## If it does NOT work

- Binding feeds off a runtime table not present in the dump (or the
  driver installs through a handle resolved at boot): document the
  call site, the table lookup, and what a phone probe would read;
  leave the id OPEN, stop.
- If only a Symbian LOGICAL irq number appears with no visible
  mapping to the ICU space: record the number + the mapping method's
  absence as OPEN — do not numerically equate it with an ICU id
  without the I2C-style corroboration.

## Gotchas

- va = file offset + 0x80000000 in romdumpplus.dmp.
- The MUSB core (0xC6038800 ← PA 0x20208800) and the OTG system-glue
  block (0xC6025Axx ← 0x20025Axx) are DIFFERENT blocks (usb-flashmode
  §2.1) — an id found near glue strings may belong to the latter.
- Symbian logical interrupt numbers are not guaranteed == hardware
  ICU ids; the I2C0 precedent matched directly, but state the
  corroboration, not just the number.
- `grep -a` on the docs (CRLF/extended-ASCII content makes plain grep
  treat files as binary — known hazard).
- Capstone constant-fold trap (`orr rX,rX,rX,lsr #N`).
- Do NOT write any DTS node or driver here — the glue task needs a
  validation target (QEMU MUSB model or hardware) that does not exist
  yet; this task closes the DECODE gap only.
- Commit locally (docs + task status); never push.
