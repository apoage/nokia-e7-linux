#!/usr/bin/env python3
"""xscan.py — phone-side crash-isolated address-space mapper.

Runs chunks of peek.exe probes on the phone via the bridge EXEC command
(one TCP round-trip per chunk, robust against WiFi drops). Dead addresses
kill only the peek child; the agent always survives.

Usage: python3 xscan.py <phone_ip> <start_hex> <end_hex> <step_hex>
       python3 xscan.py <phone_ip> list addr1,addr2,...
"""
import base64
import sys
import socket

sys.path.insert(0, "tools")
from bridge_pc import Bridge  # noqa: E402


def scan_chunk(br, addrs, nbytes=8):
    # py2.2-compatible code; one line per address: "addr rc data"
    lines = ["import e32", "out = []"]
    for a in addrs:
        lines.append("try:")
        lines.append("    rc = e32.start_exe(u'peek.exe', u'%x %d', 1)" % (a, nbytes))
        lines.append("    if rc == 0:")
        lines.append("        f = open(u'E:\\\\peek_out.txt', 'r')")
        lines.append("        out.append(f.read().strip())")
        lines.append("        f.close()")
        lines.append("    else:")
        lines.append("        out.append('%x rc=%%d' %% rc)" % a)
        lines.append("except Exception, e:")
        lines.append("    out.append('%x EXC %%r' %% (e,))" % a)
    lines.append("print('\\n'.join(out))")
    code = "\n".join(lines)
    b64 = base64.b64encode(code.encode()).decode()
    s, out = br.cmd("EXEC %s" % b64)
    if out:
        return out.decode("utf-8", "replace")
    return s


def main():
    host = sys.argv[1]
    mode = sys.argv[2]
    br = Bridge(host, timeout=120)
    if mode == "list":
        addrs = [int(x, 16) for x in sys.argv[3].split(",")]
    else:
        start, end, step = (int(x, 16) for x in sys.argv[3:6])
        addrs = list(range(start, end + 1, step))
    CH = 30
    for i in range(0, len(addrs), CH):
        chunk = addrs[i:i + CH]
        try:
            txt = scan_chunk(br, chunk)
        except (socket.timeout, EOFError) as e:
            print("chunk %d dropped (%r), reconnecting..." % (i, e))
            br = Bridge(host, timeout=120)
            continue
        print(txt)
        for line in txt.splitlines():
            if " rc=" not in line and "EXC" not in line and ":" in line:
                print("HIT: %s" % line)
    br.close()


if __name__ == "__main__":
    main()
