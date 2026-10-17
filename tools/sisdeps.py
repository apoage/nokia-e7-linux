#!/usr/bin/env python3
"""SIS v3 controller extractor (dependency/platform analysis).

Serialization rules per gnupoc makesis-4 sisfield.h:
- compound children: u32 type + u32 length + data, data padded to 4 bytes
- SISArray (2): u32 childType, then per child: u32 length + data (padded)
- SISInfo (14): compound children + 2-byte trailer (installType, installFlags)
- SISString (1): raw UTF-16LE

Usage: python3 sisdeps.py FILE [FILE...]
"""
import re
import struct
import sys
import zlib

TYPES = {1: "String", 2: "Array", 3: "Compressed", 4: "Version", 5: "VersionRange",
         6: "Date", 7: "Time", 8: "DateTime", 9: "Uid", 11: "Language",
         12: "Contents", 13: "Controller", 14: "Info", 15: "SupportedLanguages",
         16: "SupportedOptions", 17: "Prerequisites", 18: "Dependency",
         19: "Properties", 20: "Property", 21: "Signatures", 22: "CertChain",
         23: "Logo", 24: "FileDescription", 25: "Hash", 26: "If", 27: "ElseIf",
         28: "InstallBlock", 29: "Expression", 30: "Data", 31: "DataUnit",
         32: "FileData", 33: "SupportedOption", 34: "ControllerChecksum",
         35: "DataChecksum", 36: "Signature", 37: "Blob", 38: "SignatureAlgorithm",
         39: "SignatureCertChain", 40: "DataIndex", 41: "Capabilities"}


def pad(n):
    return (4 - (n & 3)) & 3


def find_controller(data):
    for m in re.finditer(rb'\x78[\x01\x5a\x9c\xda]', data):
        try:
            out = zlib.decompressobj().decompress(data[m.start():])
        except Exception:
            continue
        if len(out) < 8:
            continue
        t, l = struct.unpack_from("<II", out, 0)
        if t == 13 and 0 < l <= len(out) - 8:
            return out[8:8 + l]
    return None


def parse_compound(buf, depth, out):
    """Walk padded TLV children of a compound."""
    indent = "  " * depth
    pos = 0
    while pos + 8 <= len(buf):
        t, l = struct.unpack_from("<II", buf, pos)
        pos += 8
        if l > len(buf) - pos:
            out.append("%s<bad len %d type %d at %d>" % (indent, l, t, pos - 8))
            return
        payload = buf[pos:pos + l]
        pos += l + pad(l)
        emit(t, payload, depth, out)


def parse_array(payload, depth, out):
    indent = "  " * depth
    child_type, pos = struct.unpack_from("<I", payload, 0)[0], 4
    out.append("%sArray<%s> {" % (indent, TYPES.get(child_type, str(child_type))))
    while pos + 4 <= len(payload):
        l = struct.unpack_from("<I", payload, pos)[0]
        pos += 4
        if l > len(payload) - pos:
            out.append("%s  <bad child len %d>" % (indent, l))
            return
        emit(child_type, payload[pos:pos + l], depth + 1, out)
        pos += l + pad(l)
    out.append("%s}" % indent)


def emit(t, payload, depth, out):
    indent = "  " * depth
    name = TYPES.get(t, "?%d" % t)
    if t == 9:
        (v,) = struct.unpack_from("<I", payload, 0)
        out.append("%sUid 0x%08x" % (indent, v))
    elif t == 1:
        out.append("%sString %r" % (indent, payload.decode("utf-16-le", "replace")))
    elif t == 2:
        parse_array(payload, depth, out)
    elif t == 4:
        maj, minr, bld = struct.unpack_from("<III", payload, 0)
        out.append("%sVersion %d.%d.%d" % (indent, maj, minr, bld))
    elif t == 5:
        out.append("%sVersionRange {" % indent)
        parse_compound(payload, depth + 1, out)
        out.append("%s}" % indent)
    elif t == 8:
        y, mo, d, h, mi, s = struct.unpack_from("<6I", payload, 0)
        out.append("%sDateTime %04d-%02d-%02d %02d:%02d:%02d" % (indent, y, mo, d, h, mi, s))
    elif t == 14:
        out.append("%sInfo {" % indent)
        parse_compound(payload[:-2], depth + 1, out)
        out.append("%s  installType=%d installFlags=%d" % (indent, payload[-2], payload[-1]))
        out.append("%s}" % indent)
    elif t == 41:
        raw = payload.ljust(8, b"\0")
        (c,) = struct.unpack("<Q", raw[:8])
        out.append("%sCapabilities 0x%016x" % (indent, c))
    elif t in (5, 18, 24, 26, 27, 28, 12, 13, 14, 17, 19, 21, 30, 31):
        out.append("%s%s {" % (indent, name))
        parse_compound(payload, depth + 1, out)
        out.append("%s}" % indent)
    else:
        out.append("%s%s <%d bytes>" % (indent, name, len(payload)))


def main():
    for path in sys.argv[1:]:
        print("=== %s ===" % path)
        ctrl = find_controller(open(path, "rb").read())
        if ctrl is None:
            print("  no controller found")
            continue
        out = []
        parse_compound(ctrl, 0, out)
        print("\n".join(out))


if __name__ == "__main__":
    main()
