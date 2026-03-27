# install_direct.py - Copy memread.exe (with TCB cap) directly to sys\bin
# Bypasses SIS installer capability checks entirely
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

w("=== Direct install (bypass SIS) ===")

# Copy exe from E:\ root to sys\bin (overwrite the cap=none version)
for fn in ['memread_tcb.exe']:
    src = 'E:\\' + fn
    try:
        if not os.path.isfile(src):
            w("NOT FOUND: %s" % src)
            continue
        w("Source: %s (%d bytes)" % (src, os.path.getsize(src)))
        sf = open(src, 'rb')
        data = sf.read()
        sf.close()

        # Install as memread.exe in sys\bin
        for drv in ['E', 'C']:
            dst = '%s:\\sys\\bin\\memread.exe' % drv
            try:
                df = open(dst, 'wb')
                df.write(data)
                df.close()
                w("Wrote %s (%d bytes)" % (dst, len(data)))
            except Exception, ex:
                w("FAIL %s: %s" % (dst, str(ex)))
    except Exception, ex:
        w("Error: %s" % str(ex))

# Verify LDD is still there
for drv in ['E', 'C']:
    fp = '%s:\\sys\\bin\\memread.ldd' % drv
    try:
        if os.path.isfile(fp):
            w("LDD OK: %s (%d bytes)" % (fp, os.path.getsize(fp)))
    except:
        pass

# Now launch the TCB version
w("\n=== Launching memread.exe ===")
e32.ao_sleep(1)
try:
    ret = e32.start_exe('memread.exe', '', 1)
    w("Result: %s" % str(ret))
except Exception, ex:
    w("Error: %s %s" % (type(ex).__name__, str(ex)))

e32.ao_sleep(3)

# Check output
w("\n=== Check output ===")
for p in ['E:\\memread_log.txt', 'C:\\Data\\memread_log.txt',
          'E:\\padconf.bin', 'C:\\Data\\padconf.bin']:
    try:
        if os.path.isfile(p):
            sz = os.path.getsize(p)
            w("OUTPUT: %s (%d bytes)" % (p, sz))
            if p.endswith('.txt'):
                f = open(p, 'r')
                w(f.read())
                f.close()
            elif p.endswith('.bin') and sz > 0:
                f = open(p, 'rb')
                data = f.read(32)
                f.close()
                hexstr = ' '.join(['%02x' % ord(b) for b in data])
                w("First 32B: %s" % hexstr)
    except Exception, ex:
        w("Error reading %s: %s" % (p, str(ex)))

w("\n=== Done ===")
save_log()
