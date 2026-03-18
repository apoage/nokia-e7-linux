#!/usr/bin/env python3
"""Build a Nokia E7 OneNAND flash image for QEMU.

Composes a 1GB OneNAND image from:
  - core/rofs.img  (FPSX decrypted payload: TOC + OS partitions)
  - core/SWBL.bin  (secondary bootloader, separate extraction)
  - analysis/nloader-patched.bin  (patched NLoader with BB5 auth bypass)

The resulting image is loaded by QEMU via:
  -drive file=nokia-e7-onenand.img,if=pflash,format=raw

Usage:
    python3 build-nand-image.py [--firmware-dir ../firmware-re] [--output nokia-e7-onenand.img]
"""
import argparse
import os
import struct
import sys
from pathlib import Path

ONENAND_SIZE = 1 * 1024 * 1024 * 1024  # 1GB (8Gbit device)

# TOC location in NAND
TOC_OFFSET = 0x140000
TOC_HEADER_SIZE = 20
TOC_ENTRY_SIZE = 32
TOC_MAX_ENTRIES = 32

# Partition offsets from TOC (for overlay files)
PART_SWBL     = 0x00000800
PART_RD       = 0x00003000
PART_PRIMAPP  = 0x00004000


def parse_toc(data: bytes, offset: int) -> list[dict]:
    """Parse NAND TOC entries from raw image data."""
    entries = []
    pos = offset + TOC_HEADER_SIZE
    for i in range(TOC_MAX_ENTRIES):
        entry = data[pos:pos + TOC_ENTRY_SIZE]
        if len(entry) < TOC_ENTRY_SIZE:
            break
        if entry == b'\x00' * TOC_ENTRY_SIZE or entry == b'\xff' * TOC_ENTRY_SIZE:
            break

        name = entry[0:12].split(b'\x00')[0].decode('ascii', errors='replace')
        nand_off = struct.unpack('<I', entry[12:16])[0]
        size = struct.unpack('<I', entry[16:20])[0]
        entries.append({'name': name, 'offset': nand_off, 'size': size, 'index': i})
        pos += TOC_ENTRY_SIZE

    return entries


def build_rd_flag() -> bytes:
    """Build R&D flag partition data.

    NLoader checks for R&D mode flag in this partition.
    The R&D flag byte enables "ROFS check disabled" mode.
    A simple non-zero/non-FF pattern signals R&D mode active.
    """
    data = bytearray(0x0C00)  # R&D partition size from TOC
    # R&D flag structure: set flag byte to indicate R&D mode
    # Based on NLoader analysis: the flag is a simple presence check
    data[0] = 0x01  # R&D mode enabled
    return bytes(data)


def main():
    parser = argparse.ArgumentParser(description='Build Nokia E7 OneNAND image for QEMU')
    parser.add_argument('--firmware-dir', type=Path,
                        default=Path(__file__).resolve().parent.parent / 'firmware-re',
                        help='Path to firmware-re directory')
    parser.add_argument('--output', type=Path,
                        default=Path(__file__).resolve().parent.parent / 'firmware-re' / 'nokia-e7-onenand.img',
                        help='Output image path')
    parser.add_argument('--use-original-nloader', action='store_true',
                        help='Use original nloader.bin instead of patched')
    args = parser.parse_args()

    fw = args.firmware_dir
    rofs_path = fw / 'core' / 'rofs.img'
    swbl_path = fw / 'core' / 'SWBL.bin'
    nloader_path = fw / 'analysis' / ('nloader.bin' if args.use_original_nloader
                                       else 'nloader-patched.bin')

    # Verify inputs
    missing = []
    for p in [rofs_path, swbl_path, nloader_path]:
        if not p.exists():
            missing.append(str(p))
    if missing:
        print(f"ERROR: missing files: {', '.join(missing)}", file=sys.stderr)
        sys.exit(1)

    rofs_data = rofs_path.read_bytes()
    swbl_data = swbl_path.read_bytes()
    nloader_data = nloader_path.read_bytes()

    print(f"Inputs:")
    print(f"  rofs.img:  {len(rofs_data):>12,d} bytes ({len(rofs_data)/1024/1024:.1f} MB)")
    print(f"  SWBL.bin:  {len(swbl_data):>12,d} bytes")
    print(f"  NLoader:   {len(nloader_data):>12,d} bytes ({'patched' if not args.use_original_nloader else 'original'})")

    # Parse TOC from rofs.img
    toc_entries = parse_toc(rofs_data, TOC_OFFSET)
    print(f"\nTOC: {len(toc_entries)} entries at 0x{TOC_OFFSET:X}")
    for e in toc_entries:
        neg = ' (neg)' if e['offset'] >= 0x80000000 else ''
        print(f"  {e['name']:16s} 0x{e['offset']:08X}{neg}  {e['size']:>12,d} bytes")

    # Create 1GB image filled with 0xFF (erased NAND)
    print(f"\nCreating {ONENAND_SIZE / 1024 / 1024 / 1024:.0f} GB image...")
    img = bytearray(b'\xff' * ONENAND_SIZE)

    # Layer 1: Copy rofs.img content (TOC + OS partitions at their NAND offsets)
    # rofs.img maps directly into the NAND address space
    rofs_len = min(len(rofs_data), ONENAND_SIZE)
    img[0:rofs_len] = rofs_data[:rofs_len]
    print(f"  Copied rofs.img ({rofs_len/1024/1024:.1f} MB) at offset 0")

    # Layer 2: Overlay SWBL at its TOC offset
    img[PART_SWBL:PART_SWBL + len(swbl_data)] = swbl_data
    print(f"  Placed SWBL.bin ({len(swbl_data)} bytes) at 0x{PART_SWBL:X}")

    # Layer 3: Set R&D flag
    rd_data = build_rd_flag()
    img[PART_RD:PART_RD + len(rd_data)] = rd_data
    print(f"  Set R&D flag ({len(rd_data)} bytes) at 0x{PART_RD:X}")

    # Layer 4: Overlay patched NLoader as PRIMAPP
    img[PART_PRIMAPP:PART_PRIMAPP + len(nloader_data)] = nloader_data
    print(f"  Placed NLoader ({len(nloader_data)} bytes) at 0x{PART_PRIMAPP:X}")

    # Verify key data placements
    print(f"\nVerification:")
    # TOC should have "SWBL" name
    toc_check = img[TOC_OFFSET + TOC_HEADER_SIZE + 0:TOC_OFFSET + TOC_HEADER_SIZE + 4]
    print(f"  TOC[0] name:     {'OK' if toc_check == b'SWBL' else 'FAIL'} ({toc_check})")

    # SWBL should have non-zero data (first 2KB is BB5 auth header = zeros)
    swbl_end = PART_SWBL + len(swbl_data)
    swbl_check = any(b != 0 and b != 0xFF for b in img[PART_SWBL:swbl_end])
    print(f"  SWBL data:       {'OK' if swbl_check else 'FAIL (all zero/FF)'}")

    # NLoader should have non-zero data
    nl_check = any(b != 0 and b != 0xFF for b in img[PART_PRIMAPP:PART_PRIMAPP + 256])
    print(f"  NLoader data:    {'OK' if nl_check else 'FAIL (all zero/FF)'}")

    # GENIO_INIT should have data (from rofs.img)
    gi_off = 0x01200000
    gi_check = any(b != 0 and b != 0xFF for b in img[gi_off:gi_off + 256])
    print(f"  GENIO_INIT data: {'OK' if gi_check else 'FAIL (all zero/FF)'}")

    # SOS+CORE should have data (from rofs.img)
    core_off = 0x02A40000
    core_check = any(b != 0 and b != 0xFF for b in img[core_off:core_off + 256])
    print(f"  SOS+CORE data:   {'OK' if core_check else 'FAIL (all zero/FF)'}")

    # Count non-erased blocks
    block_size = 128 * 1024  # 128KB OneNAND blocks
    used_blocks = 0
    for i in range(0, ONENAND_SIZE, block_size):
        block = img[i:i + block_size]
        if any(b != 0xFF for b in block):
            used_blocks += 1
    total_blocks = ONENAND_SIZE // block_size
    print(f"\n  Used blocks: {used_blocks}/{total_blocks} ({used_blocks * 100 // total_blocks}%)")

    # Write output
    print(f"\nWriting {args.output}...")
    args.output.write_bytes(bytes(img))
    print(f"Done. {os.path.getsize(args.output):,d} bytes written.")
    print(f"\nQEMU usage:")
    print(f"  -drive file={args.output},if=none,id=onenand,format=raw")


if __name__ == '__main__':
    main()
