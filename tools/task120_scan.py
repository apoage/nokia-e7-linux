#!/usr/bin/env python3
"""Task 120: literal-word scan of the ROM dump for references to given VAs.

Finds every 4-aligned little-endian word equal to any requested VA
(or within a range), so cross-DLL callers / data tables can be located.
va = file offset + 0x80000000.

Usage: task120_scan.py <start_off> <end_off> <va1> [<va2> ...]
       (each va may also be start:end to flag a range)
"""
import struct
import sys

ROM = "/home/lukas/ps3/e7/firmware-re/dumps/romdumpplus.dmp"
BASE = 0x80000000


def parse_va(tok):
    if ":" in tok:
        a, b = tok.split(":")
        return (int(a, 16), int(b, 16))
    v = int(tok, 16)
    return (v, v)


def main():
    start = int(sys.argv[1], 16)
    end = int(sys.argv[2], 16)
    targets = [parse_va(t) for t in sys.argv[3:]]
    with open(ROM, "rb") as f:
        f.seek(start)
        data = f.read(end - start)
    for i in range(0, len(data) - 3, 4):
        w = struct.unpack_from("<I", data, i)[0]
        for lo, hi in targets:
            if lo <= w <= hi:
                off = start + i
                ctx = data[max(0, i - 8):i + 12].hex()
                print(f"off {off:08x} (va {BASE + off:08x}): word {w:08x}  ctx {ctx}")


if __name__ == "__main__":
    main()
