# install_ldd.py - Try to manually copy memread.ldd to sys\bin
# Open4All patch should allow writing to sys folders
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

w("=== LDD installer ===")

# Check if LDD already exists
for drv in ['C', 'E', 'Z']:
    fp = '%s:\\sys\\bin\\memread.ldd' % drv
    try:
        if os.path.isfile(fp):
            w("Already exists: %s (%d bytes)" % (fp, os.path.getsize(fp)))
    except:
        pass

# Try to copy from E:\ root to E:\sys\bin\
src = 'E:\\memread.ldd'
w("\nChecking source: %s" % src)
try:
    if os.path.isfile(src):
        w("Source exists: %d bytes" % os.path.getsize(src))
    else:
        w("Source NOT found")
        w("Listing E:\\ root...")
        try:
            for fn in os.listdir('E:\\'):
                if 'memread' in fn.lower() or 'ldd' in fn.lower():
                    w("  %s" % fn)
        except Exception, ex:
            w("listdir error: %s" % str(ex))
except Exception, ex:
    w("Error: %s" % str(ex))

# Try copying to sys\bin
for dst_drv in ['E', 'C']:
    dst = '%s:\\sys\\bin\\memread.ldd' % dst_drv
    w("\nTrying to copy to %s..." % dst)
    try:
        sf = open(src, 'rb')
        data = sf.read()
        sf.close()
        df = open(dst, 'wb')
        df.write(data)
        df.close()
        w("SUCCESS: wrote %d bytes to %s" % (len(data), dst))
        # Verify
        if os.path.isfile(dst):
            w("Verified: %s exists (%d bytes)" % (dst, os.path.getsize(dst)))
    except Exception, ex:
        w("FAILED: %s %s" % (type(ex).__name__, str(ex)))

# Now try loading the LDD
w("\n=== Try loading LDD ===")
try:
    r = e32.Ao_lock()  # dummy - just need to import e32
except:
    pass

# Can't call User::LoadLogicalDevice from Python directly
# But we can launch memread.exe which will try to load it
w("Launching memread.exe...")
try:
    ret = e32.start_exe('memread.exe', '', 1)
    w("Result: %s" % str(ret))
except Exception, ex:
    w("Error: %s %s" % (type(ex).__name__, str(ex)))

e32.ao_sleep(2)

# Check for output
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
    except:
        pass

w("\n=== Done ===")
save_log()
