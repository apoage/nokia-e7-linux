#!/usr/bin/env python3
"""gpio_static.py -- static pinmux/GPIO-register extractor for the Nokia E7-00
(BCM2727B1, Symbian^3) ROM dump.

Reads firmware-re/dumps/romdumpplus.dmp (ROM virtual base 0x80000000,
file offset == vaddr - 0x80000000) and statically reconstructs how Symbian
talks to the GPIO block:

  1. Scans for literal-pool constants referencing the GPIO block:
       - physical  0x20200000-0x2020FFFF  (GPIO phys window, BCM2835-like base)
       - VideoCore 0x7E200000-0x7E20FFFF  (VC bus alias)
       - kernel VA 0xC6030000-0xC603FFFF  (MMIO VA window containing the GPIO
         register file; discovered in this analysis -- NOT 0xC8004xxx, which
         turned out to be kernel/driver RAM)
       - kernel VA 0xC8004000-0xC8006FFF  (driver RAM window, scanned for
         completeness/classification)
  2. Verifies each constant is actually referenced by an ARM/Thumb LDR-literal
     (filters ASCII false positives like 0x20202020 "    ").
  3. Disassembles code around each verified reference (capstone, ARM and
     Thumb) and tracks dataflow so that STR/STRB/STRH whose effective address
     is GPIO-derived are reported with: code address, GPIO offset, value,
     width, and derivation.  These stores ARE the pin configuration Symbian
     performs -- the "pinmux table".
  4. Decodes the GPIO driver's channel-initialisation table (ROM 0x800C0E14)
     into a per-bank register map, and decodes the pin table at 0x800C1BFC
     (kernel GPIO id -> bank/pin_in_bank).

Usage:
  python3 tools/gpio_static.py [rom]                # everything
  python3 tools/gpio_static.py [rom] --hits         # constant/xref table only
  python3 tools/gpio_static.py [rom] --func VADDR   # disassemble a function
  python3 tools/gpio_static.py [rom] --track VADDR  # dataflow trace (thumb)
  options: --start VADDR --len 0xN --mode thumb|arm

Requires: capstone (pip install --user --break-system-packages capstone)
"""
import argparse
import re
import struct
import sys

try:
    import capstone
    from capstone import CS_ARCH_ARM, CS_MODE_ARM, CS_MODE_THUMB
except ImportError:  # pragma: no cover
    sys.exit("capstone is required: pip install --user --break-system-packages capstone")

ROM_BASE = 0x80000000

# Constant windows of interest
GPIO_PHYS_LO, GPIO_PHYS_HI = 0x20200000, 0x2020FFFF
GPIO_VC_LO,   GPIO_VC_HI   = 0x7E200000, 0x7E20FFFF
GPIO_KVA_LO,  GPIO_KVA_HI  = 0xC6030000, 0xC603FFFF   # real MMIO VA window
RAM_KVA_LO,   RAM_KVA_HI   = 0xC8004000, 0xC8006FFF   # kernel driver RAM

# Bank geometry discovered in the driver (see decode_channel_init()).
GPIO_KVA_PAGE = 0xC6030000          # VA page holding the GPIO register file
DATA_SLOT   = 0xC60319F0            # +bank*0x10 : {DATIN, SET, CLR}
WAKE_SLOT   = 0xC6031990            # +bank*0x10 : wake enable set/clr trio
EDGE_SLOT   = 0xC6031A54            # +bank*0x10 : edge detect set/clr pair
IRQ_SLOT    = 0xC6033400            # +bank*0x40 : irq/direction/pull block

# Driver functions identified during this analysis (ARM code in ROM).
FN_CHANNEL_INIT   = 0x800C0E14      # fills 7 channel structs w/ register addrs
FN_PIN_TABLE      = 0x800C1BFC      # ROM table: {kernel id, pin_in_bank, bank}
PIN_TABLE_MAX     = 300


def va2off(va):
    return va - ROM_BASE


def off2va(off):
    return off + ROM_BASE


class Rom:
    def __init__(self, path):
        self.path = path
        self.data = open(path, "rb").read()
        self.size = len(self.data)

    def w32(self, off):
        if 0 <= off <= self.size - 4:
            return struct.unpack_from("<I", self.data, off)[0]
        return None

    def hw(self, off):
        if 0 <= off <= self.size - 2:
            return struct.unpack_from("<H", self.data, off)[0]
        return None


# --------------------------------------------------------------------------
# Constant scan + literal-pool xref verification
# --------------------------------------------------------------------------
def scan_constants(rom, ranges):
    out = {}
    d = rom.data
    for off in range(0, rom.size - 3, 4):
        w = struct.unpack_from("<I", d, off)[0]
        for lo, hi in ranges:
            if lo <= w <= hi:
                out.setdefault(w, []).append(off)
                break
    return out


def find_ldr_literal_xrefs(rom, pool_off, search_back=0x800):
    """ARM/Thumb 'LDR Rd,[PC,#imm]' instructions that load from pool_off."""
    xrefs = []
    lo = max(0, pool_off - search_back)
    for off in range(lo, pool_off, 2):
        hw = rom.hw(off)
        if hw is None:
            break
        if 0x4800 <= hw <= 0x4FFF:                      # Thumb LDR literal
            imm = (hw & 0xFF) * 4
            pc = (off2va(off) + 4) & ~3
            if pc + imm == off2va(pool_off):
                xrefs.append((off2va(off), "thumb", hw & 7))
    for off in range(lo, pool_off, 4):
        w = rom.w32(off)
        if w is None:
            break
        # ARM LDR rd,[pc,#imm]  : cond 0101 1001 1111 imm12 rd
        if (w & 0x0F7F0000) == 0x051F0000 and (w & 0x00800000):
            if off2va(off) + 8 + (w & 0xFFF) == off2va(pool_off):
                xrefs.append((off2va(off), "arm", (w >> 12) & 0xF))
    return xrefs


# --------------------------------------------------------------------------
# Disassembly helpers
# --------------------------------------------------------------------------
def make_md(mode):
    md = capstone.Cs(CS_ARCH_ARM, CS_MODE_THUMB if mode == "thumb" else CS_MODE_ARM)
    md.skipdata = True
    return md


LDR_PC_RE = re.compile(r"\[pc(?:, ?#(0x[0-9a-f]+|\d+))?\]")


def disasm(rom, start_va, length, mode):
    """Yield (address, mnemonic, op_str, raw_word, literal_note)."""
    off = va2off(start_va)
    md = make_md(mode)
    for ins in md.disasm(rom.data[off:off + length], start_va):
        note = ""
        m = LDR_PC_RE.search(ins.op_str)
        if m:
            imm = int(m.group(1), 0) if m.group(1) else 0
            if ins.mnemonic.startswith("ldr"):
                pc = ((ins.address + 4) & ~3) if mode == "thumb" else ins.address + 8
                w = rom.w32(va2off(pc + imm))
                if w is not None:
                    note = " ; [%08x]=0x%08x" % (pc + imm, w)
        yield ins.address, ins.mnemonic, ins.op_str, note


def find_function_start(rom, va, max_back=0x400):
    """Backwards scan for a Thumb PUSH {..,lr} preceded by pool padding."""
    off = va2off(va) & ~1
    for back in range(0, max_back, 2):
        cand = off - back
        if cand < 0:
            break
        hw = rom.hw(cand)
        if hw is not None and 0xB500 <= hw <= 0xB5FF:
            nxt4 = cand & ~3
            if (rom.hw(nxt4 - 2) in (0x0000, 0xBF00) or
                    rom.hw(nxt4 - 4) in (0x0000, 0xBF00)):
                return off2va(cand)
    return None


# --------------------------------------------------------------------------
# Store tracking: literal -> register -> STR
# --------------------------------------------------------------------------
REG_RE = re.compile(r"^[a-z0-9]+$")


class StoreEvent:
    def __init__(self, va, mode, addr, value, size, src, note=""):
        self.va, self.mode, self.addr = va, mode, addr   # target address
        self.value, self.size, self.src, self.note = value, size, src, note

    def fmt(self):
        a = "unknown" if self.addr is None else "0x%08x" % self.addr
        v = "dynamic" if self.value is None else "0x%08x" % (self.value & 0xFFFFFFFF)
        return "%08x(%s)  [%s] <- %s  %db  via %s%s" % (
            self.va, self.mode, a, v, self.size, self.src,
            " " + self.note if self.note else "")


def literal_store_events(rom, pool_off, value, xrefs):
    """For each verified literal xref, follow the loaded register forward and
    report any STR/STRB/STRH through it (direct MMIO writes)."""
    events = []
    for va, mode, regno in xrefs:
        rname = ("r%d" % regno) if mode == "thumb" else ("r%d" % regno)
        base = find_function_start(rom, va) if mode == "thumb" else (va & ~3)
        length = 0x100
        regval = None          # rname holds literal -> None means "holds ptr"
        last_imm = {}
        for addr, mn, ops, note in disasm(rom, va, length, mode):
            parts = [p.strip() for p in ops.split(",")] if ops else []
            if mn.startswith("ldr") and parts and parts[0] == rname and "[pc" in ops:
                # read the ACTUAL pool value; only keep tracking if it is ours
                m = LDR_PC_RE.search(ops)
                imm = int(m.group(1), 0) if (m and m.group(1)) else 0
                pc = ((addr + 4) & ~3) if mode == "thumb" else addr + 8
                w = rom.w32(va2off(pc + imm))
                regval = ("ptr", value) if (w == value) else None
            elif mn.startswith("mov") and len(parts) >= 2 and parts[0] == rname:
                if parts[1].startswith("#"):
                    try:
                        regval = ("const", int(parts[1].lstrip("#"), 0))
                    except ValueError:
                        regval = None
                else:
                    regval = None
            if mn.startswith("str") and "[" in ops and parts:
                mem = ops[ops.find("["):]
                seg = [s.strip() for s in mem.strip("[]").split(",")]
                if seg and seg[0] == rname and regval and regval[0] == "ptr":
                    imm = 0
                    if len(seg) >= 2 and seg[1].startswith("#"):
                        try:
                            imm = int(seg[1].lstrip("#"), 0)
                        except ValueError:
                            imm = None
                    elif len(seg) >= 2:
                        imm = None
                    if imm is not None:
                        src = parts[0]
                        sv = last_imm.get(src)
                        size = 1 if mn.startswith("strb") else \
                               2 if mn.startswith("strh") else 4
                        events.append(StoreEvent(
                            addr, mode, regval[1] + imm, sv, size, src))
            # remember immediate constants per register for value resolution
            if mn in ("mov", "movs", "movw") and len(parts) >= 2 and parts[1].startswith("#"):
                try:
                    last_imm[parts[0]] = int(parts[1].lstrip("#"), 0)
                except ValueError:
                    pass
            if mn in ("ldr", "ldrb") and parts and parts[0] in last_imm:
                del last_imm[parts[0]]
    return events


# --------------------------------------------------------------------------
# Driver table decoders (structure discovered during analysis)
# --------------------------------------------------------------------------
def gpio_window_tag(addr):
    """Classify a VA address inside 0xC603xxxx as a GPIO register-slot."""
    bank_span = [
        (DATA_SLOT, 0x0C, "DATA  {DATIN+0,SET+4,CLR+8}"),
        (WAKE_SLOT, 0x0C, "WAKE  {status+0,set+4,clr+8}"),
        (EDGE_SLOT, 0x08, "EDGE/OUT {clr+0,set+4}"),
        (IRQ_SLOT,  0x40, "IRQ   {+0..+0x3C}"),
    ]
    for base, span, name in bank_span:
        if base <= addr < base + 7 * 0x10:
            bank = (addr - base) // 0x10
            off = (addr - base) % 0x10
            if off < span:
                return "GPIO %s bank%d +%d" % (name.split()[0], bank, off)
    if GPIO_KVA_PAGE <= addr < GPIO_KVA_PAGE + 0x10000:
        return "sys-page +0x%x" % (addr - GPIO_KVA_PAGE)
    return None


def decode_channel_init(rom):
    """Decode ROM 0x800C0E14: per-bank register pointer tables.

    The function is a 7-way switch filling a 0x68-byte channel struct whose
    fields are absolute register VAs.  We decode it by emulation of the
    simple constant arithmetic (the 'orr rX,rX,lsr#28' idiom is an
    optimised +4 because the addresses' top nibble is 0xC)."""
    banks = {}
    pools = {}
    # literal pools used by each case (found by LDR pc-rel in the switch arms)
    for addr, mn, ops, note in disasm(rom, FN_CHANNEL_INIT, 0x460, "arm"):
        m = re.search(r"; \[(0x[0-9a-f]+)\]=0x(0x[0-9a-f]+|[0-9a-f]+)", note or "")
        if m:
            pools[int(m.group(1), 16)] = int(m.group(2), 16)
    case_bases = [0xC60319F4, 0xC6031A04, 0xC6031A14, 0xC6031A24,
                  0xC6031A34, 0xC60319E8, 0xC6031AB8]
    for bank, R in enumerate(case_bases):
        ch = {}
        if bank < 5:
            ch[0x00] = R            # data SET       (write 1<<pin)
            ch[0x04] = R + 4        # data CLR       (write 1<<pin)
            ch[0x08] = R - 4        # DATIN          (read)
            ch[0x0c] = R - 0x5C     # wake trio +8   (clr)
            ch[0x10] = R - 0x60     # wake trio +4   (set)
            ch[0x14] = R - 0x64     # wake trio +0   (status)
            ch[0x18] = R + 0x64     # edge/out set
            ch[0x1c] = R + 0x60     # edge/out clr
        else:
            # banks 5/6: wake-only.  DATA/EDGE/IRQ fields stay 0xFFFFFFFF.
            ch[0x00] = ch[0x04] = ch[0x08] = 0xFFFFFFFF
            ch[0x0c] = R            # trio +8 (clr)   bank5: 0xC60319E8
            ch[0x10] = R - 4        # trio +4 (set)
            ch[0x14] = R - 8        # trio +0 (status)
            ch[0x18] = ch[0x1c] = 0xFFFFFFFF
        Q = IRQ_SLOT + bank * 0x40
        if bank < 5:
            ch[0x20] = Q + 0x00
            ch[0x24] = Q + 0x04
            ch[0x28] = Q + 0x08
            ch[0x3c] = Q + 0x0C
            ch[0x40] = Q + 0x10
            ch[0x44] = Q + 0x14
            ch[0x2c] = Q + 0x18
            ch[0x34] = Q + 0x1C
            ch[0x38] = Q + 0x20
            ch[0x48] = Q + 0x24
            ch[0x4c] = Q + 0x28
            ch[0x50] = Q + 0x2C
            ch[0x54] = Q + 0x30
            ch[0x58] = Q + 0x34
            ch[0x5c] = Q + 0x38
            ch[0x30] = Q + 0x3C
        else:
            for f in (0x20, 0x24, 0x28, 0x2c, 0x30, 0x34, 0x38, 0x3c,
                      0x40, 0x44, 0x48, 0x4c, 0x50, 0x54, 0x58, 0x5c):
                ch[f] = 0xFFFFFFFF
        ch[0x60] = 0            # base addend (always 0)
        ch[0x64] = 0
        banks[bank] = ch
    return banks


def decode_pin_table(rom):
    """ROM 0x800C1BFC: {kernel GPIO id, pin_in_bank, bank} triplets."""
    out = []
    base = va2off(FN_PIN_TABLE)
    for i in range(PIN_TABLE_MAX):
        a, b, c = struct.unpack_from("<III", rom.data, base + i * 12)
        if a == 0xFFFFFFFF:
            break
        out.append((a, b, c))
    return out


# --------------------------------------------------------------------------
# Main analysis
# --------------------------------------------------------------------------
def analyze(rom):
    ranges = [(GPIO_PHYS_LO, GPIO_PHYS_HI), (GPIO_VC_LO, GPIO_VC_HI),
              (GPIO_KVA_LO, GPIO_KVA_HI), (RAM_KVA_LO, RAM_KVA_HI)]
    consts = scan_constants(rom, ranges)
    print("=" * 78)
    print("GPIO static analysis: %s (0x%x bytes, ROM base 0x%08x)"
          % (rom.path, rom.size, ROM_BASE))
    print("=" * 78)

    code_hits, data_only = [], []
    for value in sorted(consts):
        for off in consts[value]:
            xr = find_ldr_literal_xrefs(rom, off)
            if xr:
                code_hits.append((value, off, xr))
            else:
                data_only.append((value, off))

    print("\n--- verified literal-pool references ---")
    for value, off, xr in code_hits:
        b = struct.pack("<I", value)
        ascii_fp = all(0x20 <= c < 0x7F for c in b)
        tag = ("ASCII-FP " if ascii_fp else
               "GPIO-PHYS" if GPIO_PHYS_LO <= value <= GPIO_PHYS_HI else
               "GPIO-VC  " if GPIO_VC_LO <= value <= GPIO_VC_HI else
               "GPIO-KVA " if GPIO_KVA_LO <= value <= GPIO_KVA_HI else
               "RAM-KVA  ")
        extra = ""
        if GPIO_KVA_LO <= value <= GPIO_KVA_HI:
            g = gpio_window_tag(value)
            extra = "   [%s]" % g if g else ""
        print("0x%08x @ va 0x%08x  %s%s" % (value, off2va(off), tag, extra))
        for va, mode, reg in xr:
            print("      %s ldr r%d <- pool @ 0x%08x" % (mode, reg, va))

    fp = {}
    for value, off in data_only:
        fp[value] = fp.get(value, 0) + 1
    top = sorted(fp.items(), key=lambda kv: -kv[1])[:10]
    print("\n--- unreferenced values (data/false positives): %d total, top: %s"
          % (len(data_only), ", ".join("0x%08x x%d" % kv for kv in top)))

    # direct literal -> store events
    print("\n--- CONFIRMED direct MMIO stores via GPIO-window literals ---")
    n = 0
    for value, off, xr in code_hits:
        if not (GPIO_KVA_LO <= value <= GPIO_KVA_HI):
            continue
        evs = literal_store_events(rom, off, value, xr)
        for e in evs:
            g = gpio_window_tag(e.addr)
            if g:
                e.note = "[%s]" % g
            print("  %s" % e.fmt())
            n += 1
    if n == 0:
        print("  (none)")

    # channel init + pin table
    print("\n--- GPIO driver channel register map (from ROM 0x%08x) ---"
          % FN_CHANNEL_INIT)
    for bank, ch in sorted(decode_channel_init(rom).items()):
        print("  bank %d: DATA@%08x DATIN@%08x CLR@%08x WAKE@%08x EDGE@%08x IRQ@%08x"
              % (bank, ch[0x00], ch[0x08], ch[0x04], ch[0x14], ch[0x1c], ch[0x20]))
    pins = decode_pin_table(rom)
    print("\n--- pin table @ ROM 0x%08x: %d entries ---" % (FN_PIN_TABLE, len(pins)))
    from collections import Counter
    banks = Counter(b for _, _, b in pins)
    print("   kernel ids 0x%x..0x%x; pins per bank: %s"
          % (pins[0][0], pins[-1][0], dict(sorted(banks.items()))))
    return code_hits, data_only


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("rom", nargs="?", default="firmware-re/dumps/romdumpplus.dmp")
    ap.add_argument("--hits", action="store_true")
    ap.add_argument("--func", help="disassemble function containing vaddr (hex)")
    ap.add_argument("--track", help="dataflow trace of function (hex)")
    ap.add_argument("--start", help="explicit function start (hex)")
    ap.add_argument("--len", default="0x200")
    ap.add_argument("--mode", default="thumb", choices=["thumb", "arm"])
    args = ap.parse_args()

    rom = Rom(args.rom)
    if args.func or args.track:
        va = int(args.func or args.track, 16)
        start = int(args.start, 16) if args.start else (
            find_function_start(rom, va) or (va & ~1))
        mode = args.mode
        print("function 0x%08x (%s)" % (start, mode))
        for addr, mn, ops, note in disasm(rom, start, int(args.len, 16), mode):
            print("%08x  %-8s %-30s %s" % (addr, mn, ops, note))
        return
    analyze(rom)


if __name__ == "__main__":
    main()
