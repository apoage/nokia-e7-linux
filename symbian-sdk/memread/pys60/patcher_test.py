# patcher_test.py — Open patcher LDD directly from Python
# PyS60 has e32 module with Symbian API access
import e32, os

log = ""

def l(msg):
    global log
    print msg
    log += msg + "\n"

l("=== patcher LDD test ===")

# Try loading logical devices
for name in ['patcherShadow', 'patcherS3', 'patcher']:
    try:
        r = e32.ao_callgate(lambda: None)  # just checking e32 works
    except:
        pass
    fn = name + '.ldd'
    l("load %s..." % fn)
    try:
        # e32.load_ldd might exist in some PyS60 versions
        r = e32._s60_load_ldd(fn)
        l("  result: %s" % str(r))
    except AttributeError:
        l("  no _s60_load_ldd")
    except Exception, ex:
        l("  error: %s" % str(ex))

# Check if we can use the C-level API via ctypes-like approach
# PyS60 2.0 has the 'appuifw' and 'e32' modules
# Check what's available
l("\ne32 dir:")
for attr in dir(e32):
    if not attr.startswith('_'):
        l("  %s" % attr)

# Try to find any RBusLogicalChannel equivalent
# PyS60 might expose some kernel APIs
try:
    import socket
    l("\nsocket available")
except:
    l("\nno socket")

try:
    import sysinfo
    l("sysinfo available")
    try:
        l("  imei: %s" % sysinfo.imei())
    except:
        pass
    try:
        l("  sw: %s" % sysinfo.sw_version())
    except:
        pass
except:
    l("no sysinfo")

# Check if there's a way to call native Symbian DLLs
try:
    import ctypes
    l("\nctypes available!")
except:
    l("\nno ctypes")

# Try using the e32.start_exe to run RomPatcherPlus itself
# with arguments that trigger the dump
l("\nChecking RomPatcherPlus...")
for path in ['C:\\sys\\bin\\RomPatcherPlus.exe',
             'E:\\sys\\bin\\RomPatcherPlus.exe',
             'C:\\sys\\bin\\rompatcherplus.exe']:
    if os.path.isfile(path):
        l("  found: %s (%d bytes)" % (path, os.path.getsize(path)))

# Check patcher LDD files
l("\nChecking LDD files...")
for path in ['C:\\sys\\bin\\patcher.ldd',
             'C:\\sys\\bin\\patcherS3.ldd',
             'C:\\sys\\bin\\patcherShadow.ldd',
             'E:\\sys\\bin\\patcher.ldd',
             'E:\\sys\\bin\\patcherS3.ldd',
             'E:\\sys\\bin\\patcherShadow.ldd']:
    if os.path.isfile(path):
        l("  found: %s (%d bytes)" % (path, os.path.getsize(path)))

# Save log
try:
    f = open('E:\\patcher_test.txt', 'w')
    f.write(log)
    f.close()
    print "saved E:\\patcher_test.txt"
except:
    try:
        f = open('C:\\Data\\patcher_test.txt', 'w')
        f.write(log)
        f.close()
        print "saved C:\\Data\\patcher_test.txt"
    except:
        print "cant save log"

l("done")
