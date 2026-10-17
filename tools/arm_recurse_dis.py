#!/usr/bin/env python3
"""Recursive-disassembly helper for raw ARM/Thumb images (capstone).

Generic version of the task114_dis.py one-shot: takes a raw file, a file
offset, a VA base and a starting ISA mode, walks control flow (b / bl /
b<cond> / blx) from the entry, switching ARM<->Thumb on BLX.  Resolves
`ldr rN, [pc, #imm]` literal-pool loads and `add rN, pc, #imm` /
`sub rN, pc, #imm` / `adr` PC-relative materialisation (scanning back
through the block for the definition when the register is used by
bx/blx reg), annotates literal values that point back into the image
(with the string when the target is printable), and prints unreached
gap ranges to stderr so linear coverage can be checked by hand.

Usage:
  arm_recurse_dis.py FILE OFFSET LENGTH BASE [--thumb] [--entry OFF] [--max N]
  arm_recurse_dis.py FILE OFFSET LENGTH BASE --linear ARM|THUMB

VA of the byte at OFFSET is BASE (use 0 for image-relative study).
"""
import argparse
import re
import struct
import sys

from capstone import (
    CS_ARCH_ARM, CS_MODE_ARM, CS_MODE_LITTLE_ENDIAN, CS_MODE_THUMB, Cs,
)

PRINTABLE = set(range(0x20, 0x7F))
REG_ALIAS = {"sl": "r10", "fp": "r11", "ip": "r12", "sp": "r13",
             "lr": "r14", "pc": "r15"}


def norm_reg(r):
    return REG_ALIAS.get(r, r)


class Dis:
    def __init__(self, data, base):
        self.data = data
        self.base = base  # VA of data[0]
        self.mds = {
            "arm": Cs(CS_ARCH_ARM, CS_MODE_ARM | CS_MODE_LITTLE_ENDIAN),
            "thumb": Cs(CS_ARCH_ARM, CS_MODE_THUMB | CS_MODE_LITTLE_ENDIAN),
        }
        self.reached = {}  # va -> (mode, insn)
        self.literals = {}  # va -> dict reg -> const (materialised constants)

    def va2off(self, va):
        off = va - self.base
        return off if 0 <= off < len(self.data) - 1 else None

    def one(self, va, mode):
        off = self.va2off(va)
        if off is None:
            return None
        insns = list(self.mds[mode].disasm(self.data[off:off + 4], va, count=1))
        return insns[0] if insns else None

    def word(self, va):
        off = self.va2off(va & ~3)
        if off is None or off + 4 > len(self.data):
            return None
        return struct.unpack("<I", self.data[off:off + 4])[0]

    def string_at(self, va):
        off = self.va2off(va)
        if off is None:
            return None
        end = self.data.find(b"\0", off, off + 64)
        if end < 0:
            return None
        s = self.data[off:end]
        if len(s) >= 3 and all(b in PRINTABLE for b in s):
            return s.decode("ascii")
        return None

    def pc_base(self, va, mode, size):
        if mode == "arm":
            return (va + 8) & ~3
        return (va + 4) & ~3 if size == 2 else va + 4

    def materialise(self, va, mode, insn):
        """If insn materialises a constant into a register, return
        (reg, value); else None.  Handles literal-pool ldr, pc-rel
        add/sub, adr."""
        txt = f"{insn.mnemonic} {insn.op_str}"
        m = re.match(r"ldr(?:\.w)?\s+(\w+), \[pc, #(-?0x[0-9a-f]+|\d+)\]$", txt)
        if m:
            lit_va = self.pc_base(va, mode, insn.size) + int(m.group(2), 0)
            val = self.word(lit_va)
            if val is not None:
                return norm_reg(m.group(1)), val, lit_va
            return None
        m = re.match(r"(add|sub)(?:\.w)?\s+(\w+), pc, #(-?0x[0-9a-f]+|\d+)$", txt)
        if m:
            sign = -1 if m.group(1) == "sub" else 1
            val = self.pc_base(va, mode, insn.size) + sign * int(m.group(3), 0)
            return norm_reg(m.group(2)), val, None
        if insn.mnemonic == "adr" or (insn.mnemonic == "add" and
                                      re.match(r"\w+, pc$", insn.op_str.split(", ")[0] + ", pc")):
            m = re.match(r"(?:adr|add)\s+(\w+), (?:pc, )?#(-?0x[0-9a-f]+|\d+)$", txt)
            if m:
                val = self.pc_base(va, mode, insn.size) + int(m.group(2), 0)
                return norm_reg(m.group(1)), val, None
        return None

    # ---------------- reachability ----------------
    def walk(self, entry, mode, max_insn=50000):
        work = [(entry, mode, 0)]
        seen = set()
        n = 0
        while work and n < max_insn:
            va, md, depth = work.pop(0)
            while n < max_insn:
                if (va, md) in seen:
                    break
                off = self.va2off(va)
                if off is None:
                    break
                insn = self.one(va, md)
                if insn is None:
                    break
                seen.add((va, md))
                n += 1
                self.reached[va] = (md, insn)
                nxt, edges = self.step(va, md, insn)
                for tgt, tmd in edges:
                    if self.va2off(tgt) is not None:
                        work.append((tgt, tmd, depth + 1))
                if nxt is None:
                    break
                va = nxt

    def block_defs(self, va, mode):
        """Scan the linear block ending at va for materialised constants
        (regs dict at va).  Stops at branch/barrier."""
        regs = {}
        cur = va
        # walk back at most 64 insns
        for _ in range(64):
            prev = self.find_prev(cur, mode)
            if prev is None:
                break
            insn = self.reached.get(prev, (None, None))[1]
            if insn is None:
                break
            if insn.mnemonic.split(".")[0] in (
                    "b", "bl", "blx", "bx", "pop", "ldm", "push", "stm",
                    "cbz", "cbnz") and prev != va:
                break
            mat = self.materialise(prev, mode, insn)
            if mat:
                reg, val, _ = mat
                if reg not in regs:
                    regs[reg] = val
            cur = prev
        return regs

    def find_prev(self, va, mode):
        if mode == "arm":
            return va - 4 if (va - 4) in self.reached else None
        for back in (2, 4):
            if va - back in self.reached:
                return va - back
        return None

    def step(self, va, mode, insn):
        mn = insn.mnemonic.split(".")[0]
        ops = insn.op_str
        size = insn.size
        is_arm = mode == "arm"
        cont = va + size

        def imm_target():
            m = re.match(r"#(-?0x[0-9a-f]+|-?\d+)$", ops.strip())
            return int(m.group(1), 0) if m else None

        if mn in ("b", "bl"):
            t = imm_target()
            if t is None:
                return None, []
            return None, [(t, mode)] + ([(cont, mode)] if mn == "bl" else [])
        if mn.startswith("b") and mn not in ("bic", "bfi", "bfc", "bkpt", "bsw"):
            # conditional branch, cbz/cbnz (thumb)
            if mn in ("bx", "blx") or mn.startswith("bx"):
                pass
            else:
                t = imm_target()
                if t is not None:
                    return cont, [(t, mode)]
                return cont, []
        if mn in ("blx", "bx"):
            if ops.strip() in ("lr", "r14"):
                return None, []  # return
            m = re.match(r"#(-?0x[0-9a-f]+|-?\d+)$", ops.strip())
            if m:  # blx imm: always switches, is a call (continues)
                t = int(m.group(1), 0) & ~1
                return cont, [(t, "thumb" if is_arm else "arm")]
            m = re.match(r"(\w+)$", ops.strip())
            if m:
                reg = norm_reg(m.group(1))
                regs = self.block_defs(va, mode)
                if reg in regs:
                    val = regs[reg]
                    tmd = "thumb" if val & 1 else "arm"
                    return None, [(val & ~1, tmd)]
                if mn == "blx":  # indirect call, unknown target
                    return cont, []
                return None, []
            return cont, []
        if mn in ("pop", "ldm", "ldmdb", "ldmia") and re.search(r"\bpc\b", ops):
            return None, []
        return cont, []


def annotate(d, va, mode, insn):
    notes = []
    mat = d.materialise(va, mode, insn)
    if mat:
        reg, val, lit_va = mat
        note = f"; {reg} = {val:#010x}"
        if lit_va is not None:
            note += f" [pool {lit_va:#x}]"
        s = d.string_at(val)
        off = d.va2off(val)
        if off is not None:
            note += f" img+{off:#x}"
            if s:
                note += f' "{s}"'
        notes.append(note)
    # annotate bx/blx reg when resolvable
    mn = insn.mnemonic.split(".")[0]
    if mn in ("blx", "bx") and not insn.op_str.strip().startswith("#"):
        m = re.match(r"(\w+)$", insn.op_str.strip())
        if m and m.group(1) not in ("lr", "r14"):
            reg = norm_reg(m.group(1))
            regs = d.block_defs(va, mode)
            if reg in regs:
                val = regs[reg]
                notes.append(f"; -> {val & ~1:#x} "
                             f"{'thumb' if val & 1 else 'arm'}")
    return " ".join(notes)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("file")
    ap.add_argument("offset", help="file offset (hex)")
    ap.add_argument("length", help="length (hex)")
    ap.add_argument("base", help="VA of byte at OFFSET (hex); 0 = image-relative")
    ap.add_argument("--entry", help="entry file offset (hex), default OFFSET")
    ap.add_argument("--thumb", action="store_true")
    ap.add_argument("--linear", choices=["arm", "thumb"])
    ap.add_argument("--max", type=int, default=50000)
    args = ap.parse_args()

    off = int(args.offset, 16)
    length = int(args.length, 16)
    base = int(args.base, 16)
    with open(args.file, "rb") as f:
        f.seek(off)
        data = f.read(length)

    if args.linear:
        md = Cs(CS_ARCH_ARM, (CS_MODE_THUMB if args.linear == "thumb"
                              else CS_MODE_ARM) | CS_MODE_LITTLE_ENDIAN)
        for i in md.disasm(data, base):
            print(f"{i.address:08x}  {i.bytes.hex():<9} {i.mnemonic:<8} {i.op_str}")
        return

    entry = base + ((int(args.entry, 16) if args.entry else off) - off)
    d = Dis(data, base)
    d.walk(entry, "thumb" if args.thumb else "arm", args.max)
    for va in sorted(d.reached):
        mode, insn = d.reached[va]
        if insn is None:
            continue
        note = annotate(d, va, mode, insn)
        print(f"{va:08x}  {insn.bytes.hex():<9} {insn.mnemonic:<8} {insn.op_str}"
              f"{'  ' + note if note else ''}")
    cov = sorted(v - base for v in d.reached)
    print(f"\n# {len(cov)} insns reached; gaps (image offsets):",
          file=sys.stderr)
    prev = None
    for o in cov:
        if prev is not None and o > prev + (4):
            print(f"#   gap {prev + 4:#06x}..{o:#06x}", file=sys.stderr)
        prev = max(prev or 0, o)


if __name__ == "__main__":
    main()
