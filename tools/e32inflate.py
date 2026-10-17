#!/usr/bin/env python3
"""Inflate deflate-compressed E32 images (exe/dll/ldd) and dump strings.

Usage: python3 e32inflate.py FILE [...]   -> writes FILE.inflated + prints
candidate device/driver names (short lowercase strings near exports).
"""
import struct
import sys
import zlib


def inflate_e32(path):
    data = open(path, "rb").read()
    vals = struct.unpack_from("<30I2H", data, 0)
    (uid1, uid2, uid3, uid_crc, sig, hdr_crc, modver, compression, toolsver,
     tlo, thi, flags, code_size, data_size, heapmin, heapmax, stack, bss,
     entry, code_base, data_base, dllrefs, expoff, expcnt, textsz,
     codeoff, dataoff, importoff, codereloc, datareloc) = vals[:30]
    if compression == 0:
        return data, "none"
    uncomp_size = struct.unpack_from("<I", data, 124)[0]
    payload = data[codeoff:]
    for wbits in (-15, 15):
        try:
            out = zlib.decompressobj(wbits).decompress(payload)
            if len(out) >= uncomp_size - 16:
                return out[:uncomp_size], "deflate"
        except Exception:
            continue
    return None, "fail"


def main():
    for path in sys.argv[1:]:
        out, mode = inflate_e32(path)
        if out is None:
            print("%s: inflate FAILED" % path)
            continue
        open(path + ".inflated", "wb").write(out)
        print("=== %s (%s, %d bytes inflated)" % (path, mode, len(out)))
        # ASCII strings, short lowercase alphanumeric candidates = names
        cur = b""
        found = []
        for b in out:
            if 32 <= b < 127:
                cur += bytes([b])
            else:
                if 3 <= len(cur) <= 24 and cur.isascii():
                    s = cur.decode()
                    if s[0].islower() and any(c.isupper() for c in s) \
                            and s.isalnum():
                        found.append(s)
                cur = b""
        for s in sorted(set(found)):
            print("  name-candidate:", s)


if __name__ == "__main__":
    main()
