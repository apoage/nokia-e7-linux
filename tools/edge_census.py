#!/usr/bin/env python3
"""edge_census.py -- loads-vs-stores census for BCM2727 GPIO register slots
(open question #2 of docs/gpio-static-pinmux.md).

For every ROM literal inside a GPIO register slot (default: the EDGE/OUT
set/clr pair at 0xC6031A54 + bank*0x10), follow the loaded register and
classify each memory access through it:
  - MMIO store  (str* [reg,#imm])   -> write
  - MMIO load   (ldr* [reg,#imm])   -> read   <-- decisive for Q2
  - pointer plumbing (register stored into RAM) -> channel-init struct fill

If the EDGE pair is never read anywhere in the ROM, it is a write-only
enable latch (edge-detect enable), not a readable status/output latch.

Usage: python3 tools/edge_census.py [rom] [--slot edge|data|wake|irq]
"""
import re
import sys

sys.path.insert(0, "tools")
import gpio_static as g  # noqa: E402

SLOTS = {
    "edge": (g.EDGE_SLOT, 0x08),
    "data": (g.DATA_SLOT, 0x0C),
    "wake": (g.WAKE_SLOT, 0x0C),
    "irq":  (g.IRQ_SLOT, 0x40),
}

BASE_RE = re.compile(r"\[(r\d+)(?:, ?#[^\]]+)?\]")


def track(rom, va, mode, value, loads, stores, plumbing):
    """Disassemble forward from the literal load site; classify memory ops
    whose base register currently holds `value`."""
    holders = set()   # registers currently holding the literal value
    for addr, mn, ops, note in g.disasm(rom, va, 0x100, mode):
        parts = [p.strip() for p in ops.split(",")] if ops else []
        dst = parts[0] if parts else None
        if mn.startswith("ldr") and "[pc" in (ops or ""):
            m = re.search(r"\[0?x?([0-9a-f]+)\]=0x0?x?([0-9a-f]+)", note or "")
            if m and int(m.group(2), 16) == value:
                holders.add(dst)
            elif dst in holders:
                holders.discard(dst)
            continue
        if not ops:
            continue
        m = BASE_RE.search(ops)
        base_reg = m.group(1) if m else None
        if base_reg in holders:
            if mn.startswith("ldr"):
                loads.append((addr, mode, mn, ops, value))
                if dst in holders:
                    holders.discard(dst)   # clobbered by loaded data
            elif mn.startswith("str"):
                stores.append((addr, mode, mn, ops, value))
        elif mn.startswith("str") and dst in holders:
            plumbing.append((addr, mode, mn, ops, value))
        # a load/write into a holder register destroys the pointer value
        if dst in holders and (mn.startswith("ldr") or
                               mn in ("mov", "movs", "movw",
                                      "add", "adds", "sub", "subs")):
            srcs = [p for p in parts[1:]
                    if re.fullmatch(r"r\d+|sp|lr|pc", p)]
            if not (srcs and all(s in holders for s in srcs)):
                holders.discard(dst)


def census(rom, slot_name):
    base, span = SLOTS[slot_name]
    lo, hi = base, base + 7 * 0x10
    consts = g.scan_constants(rom, [(lo, hi)])
    loads, stores, plumbing = [], [], []
    n_literals = 0
    for value in sorted(consts):
        for off in consts[value]:
            xr = g.find_ldr_literal_xrefs(rom, off)
            for va, mode, regno in xr:
                n_literals += 1
                track(rom, va, mode, value, loads, stores, plumbing)
    return n_literals, loads, stores, plumbing


def main():
    rom_path = sys.argv[1] if len(sys.argv) > 1 and \
        not sys.argv[1].startswith("--") else \
        "firmware-re/dumps/romdumpplus.dmp"
    slot = "edge"
    if "--slot" in sys.argv:
        slot = sys.argv[sys.argv.index("--slot") + 1]
    rom = g.Rom(rom_path)
    n, loads, stores, plumbing = census(rom, slot)
    print("=== %s slot census (%s): %d literal xrefs ===" %
          (slot, rom_path, n))
    print("\nMMIO READS through slot literals: %d" % len(loads))
    for addr, mode, mn, ops, value in loads:
        print("  %08x(%s)  %-5s %-28s [0x%08x %s]"
              % (addr, mode, mn, ops, value, g.gpio_window_tag(value)))
    print("\nMMIO WRITES through slot literals: %d" % len(stores))
    for addr, mode, mn, ops, value in stores:
        print("  %08x(%s)  %-5s %-28s [0x%08x %s]"
              % (addr, mode, mn, ops, value, g.gpio_window_tag(value)))
    print("\npointer-plumbing stores (reg -> RAM): %d" % len(plumbing))
    for addr, mode, mn, ops, value in plumbing[:60]:
        print("  %08x(%s)  %-5s %-28s [0x%08x]"
              % (addr, mode, mn, ops, value))


if __name__ == "__main__":
    main()
