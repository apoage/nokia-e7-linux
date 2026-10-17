# Task 111: I2C masters decode — full register/protocol contract (sys+0x22/23/25xx)

## Status

**DONE 2026-08-28 — decode complete, see `docs/bcm2727-i2c.md`.**
Both sides decoded (NLoader polled driver + the Symbian ROM I2C0
driver): full register map, transfer sequences, IRQ event bits
(Nack/Arbit/Ardy/RxData/TxData), **I2C0 = ICU 0x96 CONFIRMED**, bus
map (bus 2 = chargers 0x6B/0x6A — the "0xD6 companion channel";
bus 0 = PMIC/sensors cluster). QEMU-model + Linux-driver contract in
doc §6 (i2cdetect = the validation). Follow-up task = implement
model + driver + retire the globals+0x24 XTI shim.

**IDLE-TASK PROMPT (paste into the queue):**
> Execute tasks/111-O-i2c-masters-decode.md in /home/lukas/ps3/e7
> (read docs/bcm2727-xti-trace.md §2 first — the masters are already
> located with CONFIRMED register writes). One job: the full
> register/protocol decode of the three I2C masters (sys+0x2200 /
> +0x2300 / +0x2500) from nloader.bin and the Symbian ROM driver —
> register map, transfer sequence, completion/IRQ semantics, and which
> devices sit on which bus. Deliver docs/bcm2727-i2c.md with a
> QEMU-model + Linux-driver contract section modeled on the memcard
> doc §6.2. Do NOT implement the model or the driver (explicit
> follow-up task). Commit when done.

## Why

Desk item 2. The I2C block is half-decoded already (XTI doc §2: the
ex-"NLoader UART" is an I2C command channel; the Symbian I2C0 driver
lives at VA 0xC6032200 with the same register shape; three masters
exist per the clock names EHwClkI2C0/1/2). i2c-tools are already in
the buildroot root image (task 103) — once the model+driver follow-up
lands, in-chain i2cdetect becomes the validation. This task produces
the contract that de-risks both.

## Anchors (from bcm2727-xti-trace.md §2, CONFIRMED)

- Bases: sys-block **+0x2200 / +0x2300 / +0x2500**; NLoader uses
  0x200322xx ≡ kernel 0xC60322xx (dual view 0x2003/0x2020).
- NLoader code: string writer 0x1004C710, copier 0x10021DFC, the I2C
  send path keyed by (+0x0C stream offset, +0x24 I2C base).
- Symbian driver strings: "I2CThread_0", "Arbit/Cancel/RxData/TxData/
  TimeOut", "DI2CPowerHandler" (ROM, VA 0xC6032200 neighborhood).
- Known bus citizens (nloader strings + 2026-02-26 chronicle hardware
  table): SMB338x USB-charger SMPS ("SMB338%c found, rev:%x,
  (I2C_%d)"), BT "BC02" power path — build the per-bus device table
  from the xrefs.

## Steps

1. Disassemble NLoader's I2C send path around the anchors: register
   offsets written/read, start/stop/address/data phases, how
   completion is awaited (poll vs IRQ) on the boot path.
2. The Symbian ROM driver (0xC6032200 region + the I2CThread strings)
   for the OS-side semantics — IRQ enable/status bits, arbitration,
   timeout handling. This is where IRQ ids (if any) surface: cross-ref
   the ICU decoded id space (icu-timer doc).
3. Bus map: which master talks to which device (string xrefs on the
   charger/BT prints; any other I2C citizens in the ROM — PMIC,
   audio codec, touch controller candidates).
4. Write the contract: register table, transfer sequence, IRQ model,
   bus-device map, and the QEMU+driver design notes (mirror memcard
   doc §6.2's shape: what the model must fake per device for
   i2cdetect to be meaningful).

## Deliverables

- `docs/bcm2727-i2c.md` — decode + contract, legend-marked
  (CONFIRMED/CONTRACT/OPEN).
- This task's Status; HANDOFF desk item 2 pointer.
- Commit (docs only).

## If it does NOT work

If the ROM driver decode stalls (packed/compressed region, missing
xrefs), deliver what NLoader-side decode established + the explicit
OPEN list, and stop — do not invent register semantics.

## Gotchas

- Capstone constant-fold trap; Thumb base 0x10020000 for nloader,
  ROM va 0x80000000.
- The three masters likely share one register shape — decode ONE
  fully, diff the other two (cheap) instead of three full walks.
- No QEMU/kernel changes in this task; commit locally.
