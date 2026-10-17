#!/usr/bin/env python3
"""Task 114: disassemble the DVCExe constructor region (ARM) from the ROM dump.
ROM va 0x80000000; file offset = va - 0x80000000."""
import sys
from capstone import *

ROM = "/home/lukas/ps3/e7/firmware-re/dumps/romdumpplus.dmp"
BASE = 0x80000000

def dis(off, length, mode=CS_MODE_ARM | CS_MODE_LITTLE_ENDIAN):
    with open(ROM, "rb") as f:
        f.seek(off)
        code = f.read(length)
    md = Cs(CS_ARCH_ARM, mode)
    for i in md.disasm(code, BASE + off):
        print(f"{i.address:08x}  {i.bytes.hex():<10} {i.mnemonic:<8} {i.op_str}")

if __name__ == "__main__":
    off = int(sys.argv[1], 16)
    length = int(sys.argv[2], 16)
    dis(off, length)
