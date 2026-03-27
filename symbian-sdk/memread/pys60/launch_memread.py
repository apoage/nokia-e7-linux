# launch_memread.py - Launch memread.exe and check for PADCONF dump
# Run from PyS60 after installing memread.sisx
import os
import e32

log = []
def w(s):
    log.append(s)
    print s

def save_log():
    txt = '\n'.join(log)
    for p in ['E:\\pylog.txt', 'C:\\Data\\pylog.txt']:
        try:
            f = open(p, 'w')
            f.write(txt)
            f.close()
            return
        except:
            pass

w("=== memread launcher ===")

# Check exe and ldd exist
for drv in ['C', 'E']:
    for fn in ['memread.exe', 'memread.ldd']:
        fp = '%s:\\sys\\bin\\%s' % (drv, fn)
        try:
            if os.path.isfile(fp):
                w("FOUND: %s (%d bytes)" % (fp, os.path.getsize(fp)))
        except:
            pass

# Clean old output
for p in ['E:\\padconf.bin', 'C:\\Data\\padconf.bin',
          'E:\\memread_err.txt', 'C:\\Data\\memread_err.txt']:
    try:
        os.remove(p)
        w("Removed: %s" % p)
    except:
        pass

# Launch memread.exe with wait
# It will: load LDD, dump PADCONF to E:\padconf.bin, then start TCP server
# We use wait=1 so we get the return code, but memread blocks on TCP server
# So launch WITHOUT wait first, then check for padconf.bin
w("\nLaunching memread.exe (no wait)...")
try:
    ret = e32.start_exe('memread.exe', '')
    w("start_exe returned: %s" % str(ret))
except Exception, ex:
    w("start_exe error: %s %s" % (type(ex).__name__, str(ex)))

# Wait for PADCONF dump (should be near-instant)
e32.ao_sleep(3)

# Check for output
w("\n=== Check PADCONF dump ===")
found = False
for p in ['E:\\padconf.bin', 'C:\\Data\\padconf.bin']:
    try:
        if os.path.isfile(p):
            sz = os.path.getsize(p)
            w("FOUND: %s (%d bytes)" % (p, sz))
            found = True
            # Read first 32 bytes as hex
            f = open(p, 'rb')
            data = f.read(32)
            f.close()
            hexstr = ' '.join(['%02x' % ord(b) for b in data])
            w("First 32 bytes: %s" % hexstr)
    except Exception, ex:
        w("Error: %s" % str(ex))

if not found:
    w("NO padconf.bin FOUND")
    # Try launching with wait to see error
    w("\nRetrying with wait=1...")
    try:
        ret = e32.start_exe('memread.exe', '', 1)
        w("start_exe(wait) returned: %s" % str(ret))
    except Exception, ex:
        w("Error: %s %s" % (type(ex).__name__, str(ex)))

w("\n=== Done ===")
save_log()
