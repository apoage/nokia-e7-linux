#!/usr/bin/env python3
"""E32 image header parser/dumper for Symbian binaries.

Layout follows gnupoc elf2e32 e32image.h (E32ImageHeader + E32ImageHeaderComp +
E32ImageHeaderV), verified against kernelhwsrv loader expectations.

Usage:
    python3 e32hdr.py FILE [FILE...]      # dump each header
    python3 e32hdr.py --diff A B          # field-by-field diff of two images
"""
import struct
import sys

CAPS = [
    (0, "TCB"), (1, "CommDD"), (2, "PowerMgmt"), (3, "MultimediaDD"),
    (4, "ReadDeviceData"), (5, "WriteDeviceData"), (6, "DRM"), (7, "TrustedUI"),
    (8, "ProtServ"), (9, "DiskAdmin"), (10, "NetworkControl"), (11, "AllFiles"),
    (12, "SwEvent"), (13, "NetworkServices"), (14, "LocalServices"),
    (15, "ReadUserData"), (16, "WriteUserData"), (17, "Location"),
    (18, "SurroundingsDD"), (19, "UserEnvironment"),
]

STD_FIELDS = [
    "uid1", "uid2", "uid3", "uidChecksum", "signature", "headerCrc",
    "moduleVersion", "compressionType", "toolsVersion", "timeLo", "timeHi",
    "flags", "codeSize", "dataSize", "heapSizeMin", "heapSizeMax", "stackSize",
    "bssSize", "entryPoint", "codeBase", "dataBase", "dllRefTableCount",
    "exportDirOffset", "exportDirCount", "textSize", "codeOffset",
    "dataOffset", "importOffset", "codeRelocOffset", "dataRelocOffset",
]

K_HDR_FMT_V = 0x02000000
K_COMP_DEFLATE = 0x101F7AFC

FLAG_NAMES = [
    (0x00000001, "Dll"), (0x00000002, "NoCallEntryPoint"),
    (0x00000008, "ABI_EABI"), (0x00000020, "Ept_Eka2"),
    (0x00000100, "Unpaged"), (0x00000200, "Paged"),
    (0x00000800, "Debuggable"), (0x00001000, "DataUnpaged"),
    (0x00002000, "DataPaged"), (0x00004000, "SMPSafe"),
    (0x10000000, "ImpFmt_ELF"), (0x02000000, "HdrFmt_V"),
    (0x00100000, "HWFloat_VFPv2"),
]


def cap_names(caps0, caps1):
    out = [n for b, n in CAPS if caps0 >> b & 1]
    out += ["bit%d@caps1" % b for b in range(32) if caps1 >> b & 1]
    return out


def parse(path):
    with open(path, "rb") as f:
        data = f.read()
    vals = struct.unpack_from("<30I2H", data, 0)
    img = dict(zip(STD_FIELDS, vals[:30]))
    img["processPriority"], img["cpuIdentifier"] = vals[30], vals[31]
    img["fileSize"] = len(data)
    # Layout per kernelhwsrv f32image.h class hierarchy: E32ImageHeader is
    # 30 u32 + 2 u16 = 124 bytes; E32ImageHeaderComp adds iUncompressedSize
    # @124 (always present, 0 when uncompressed); E32ImageHeaderV adds the
    # security fields from @128. Verified against loader validation math
    # (edEnd == iCodeOffset) and known-good binaries.
    off = 124
    img["uncompressedSize"] = struct.unpack_from("<I", data, off)[0]
    off += 4
    if img["flags"] & K_HDR_FMT_V:
        (img["secureId"], img["vendorId"], c0, c1, img["exceptionDescriptor"],
         img["spare2"], img["exportDescSize"], img["exportDescType"]) = \
            struct.unpack_from("<6IHB", data, off)
        img["caps0"], img["caps1"] = c0, c1
        img["caps"] = cap_names(c0, c1)
        off += 26
    img["headerTotal"] = img["codeOffset"]
    return img


def fmt(img):
    lines = []
    for k in STD_FIELDS + ["processPriority", "cpuIdentifier", "fileSize",
                           "uncompressedSize", "secureId", "vendorId",
                           "caps0", "caps1", "exceptionDescriptor",
                           "exportDescSize", "exportDescType", "headerTotal"]:
        if k not in img:
            continue
        v = img[k]
        if isinstance(v, int):
            lines.append("%-22s 0x%08x  (%d)" % (k, v & 0xFFFFFFFF, v))
        else:
            lines.append("%-22s %s" % (k, v))
    if "flags" in img:
        fl = [n for b, n in FLAG_NAMES if img["flags"] & b]
        lines.append("flags decoded        %s" % " | ".join(fl))
    if "compressionType" in img:
        c = img["compressionType"]
        lines.append("compression          %s" % {
            0: "none", K_COMP_DEFLATE: "deflate"}.get(c, hex(c)))
    if "cpuIdentifier" in img:
        lines.append("cpu                  %s" % {
            0x2000: "ARMv4", 0x2001: "ARMv5"}.get(
                img["cpuIdentifier"], hex(img["cpuIdentifier"])))
    if "caps" in img:
        lines.append("capabilities         %s" % ("+".join(img["caps"]) or "(none)"))
    return "\n".join(lines)


def diff(a, b):
    keys = [k for k in STD_FIELDS + [
        "processPriority", "cpuIdentifier", "fileSize", "uncompressedSize",
        "secureId", "vendorId", "caps0", "caps1", "exceptionDescriptor",
        "exportDescSize", "exportDescType"] if k in a or k in b]
    for k in keys:
        va, vb = a.get(k), b.get(k)
        if va != vb:
            print("  %-20s A=0x%08x  B=0x%08x" % (k, va if va is not None else -1,
                                                  vb if vb is not None else -1))
    ca = set(a.get("caps", []))
    cb = set(b.get("caps", []))
    if ca != cb:
        print("  caps only in A: %s" % (ca - cb or "-"))
        print("  caps only in B: %s" % (cb - ca or "-"))


if __name__ == "__main__":
    args = sys.argv[1:]
    if args and args[0] == "--diff":
        a, b = parse(args[1]), parse(args[2])
        print("=== %s vs %s ===" % (args[1], args[2]))
        diff(a, b)
    else:
        for p in args:
            print("=== %s ===" % p)
            print(fmt(parse(p)))
            print()
