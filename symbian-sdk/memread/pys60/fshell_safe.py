# fshell_safe.py v6 - Try KNOWN VALID virtual addresses
# ROM is at 0x80000000 (confirmed by meminfo: 11MB ROM)
# SuperPage has hw mapping info at 0xF000xxxx range
import os, e32

log = []
def w(s):
    log.append(s)
    print s

def save():
    try:
        f = open('E:\\fshell_log.txt', 'w')
        f.write('\n'.join(log))
        f.close()
    except:
        pass

def run(cmd):
    w("CMD: %s" % cmd)
    save()
    try:
        r = e32.start_exe('fshell.exe', '-e "%s"' % cmd, 1)
        w("  ret: %s" % str(r))
        return r
    except Exception, ex:
        w("  err: %s" % str(ex))
        return -999

w("=== FShell v6 - safe address probe ===")

# Step 1: ROM base (MUST be mapped - meminfo says 11MB ROM)
run("readmem 0x80000000 16 e:\\read_rom.bin")
save()

if os.path.isfile('E:\\read_rom.bin'):
    sz = os.path.getsize('E:\\read_rom.bin')
    w("ROM read OK! %d bytes" % sz)
    if sz > 0:
        f = open('E:\\read_rom.bin', 'rb')
        d = f.read()
        f.close()
        w(' '.join(['%02x' % ord(b) for b in d]))
else:
    w("ROM read failed - readmem may not work at all")
    w("=== abort ===")
    save()
    raise SystemExit

# Step 2: If ROM works, try SuperPage area
# SuperPage dump showed values at 0xF000xxxx
run("readmem 0xF0000000 256 e:\\read_f000.bin")
save()

# Step 3: Try common Symbian HW mapping ranges
# OMAP3 L4 (0x48000000 phys) might be at these virtual addrs:
addrs = [
    ('0xC6000000', 'C6M common HW base'),
    ('0xC6002030', 'C6M+PADCONF offset'),
    ('0xF8000000', 'F8M'),
    ('0xFA000000', 'FAM'),
]

for addr, desc in addrs:
    w("\nProbing %s (%s)" % (addr, desc))
    save()
    fn = 'e:\\read_%s.bin' % addr[2:6]
    run("readmem %s 16 %s" % (addr, fn))
    save()
    try:
        if os.path.isfile(fn):
            sz = os.path.getsize(fn)
            if sz > 0:
                f = open(fn, 'rb')
                d = f.read()
                f.close()
                w("  %d bytes: %s" % (sz, ' '.join(['%02x' % ord(b) for b in d[:16]])))
    except:
        pass

w("\n=== done ===")
save()
