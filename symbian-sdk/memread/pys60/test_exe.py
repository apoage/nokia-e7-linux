# test_exe.py — Run on phone via PyS60
# Checks file system and tries to run our exe
# Results written to E:\pylog.txt

import os
import e32

log = []

def w(s):
    log.append(s)
    print s

# Check drives
w("=== Drive check ===")
for d in ['C:', 'D:', 'E:', 'F:']:
    try:
        exists = os.path.isdir(d + '\\')
        w("%s exists=%s" % (d, exists))
    except:
        w("%s error" % d)

# Check sys/bin for our exes
w("\n=== sys/bin check ===")
for exe in ['hello.exe', 'hello2.exe', 'minimal.exe', 'memspy.exe']:
    for d in ['C:', 'E:']:
        p = d + '\\sys\\bin\\' + exe
        try:
            exists = os.path.isfile(p)
            if exists:
                w("FOUND: %s (%d bytes)" % (p, os.path.getsize(p)))
        except:
            pass

# Check what python can access
w("\n=== Python script folder ===")
try:
    pydir = 'E:\\Python'
    if os.path.isdir(pydir):
        w("E:\\Python contents:")
        for f in os.listdir(pydir):
            w("  " + f)
except Exception, ex:
    w("Error: %s" % str(ex))

# Try to write a file
w("\n=== File write test ===")
for path in ['E:\\pylog_test.txt', 'C:\\Data\\pylog_test.txt']:
    try:
        f = open(path, 'w')
        f.write('hello from pys60\n')
        f.close()
        w("WROTE: %s" % path)
        break
    except Exception, ex:
        w("FAIL %s: %s" % (path, str(ex)))

# Try to launch hello.exe
w("\n=== Exe launch test ===")
for d in ['C:', 'E:']:
    p = d + '\\sys\\bin\\hello.exe'
    try:
        if os.path.isfile(p):
            w("Launching %s..." % p)
            e32.start_exe(p, '')
            w("Launched OK")
    except Exception, ex:
        w("Launch error: %s" % str(ex))

# Try start_exe with just the name (Symbian resolves from sys\bin)
try:
    w("Trying e32.start_exe('hello.exe', '')...")
    e32.start_exe('hello.exe', '')
    w("Launched OK")
except Exception, ex:
    w("Launch error: %s" % str(ex))

# Save log
w("\n=== Done ===")
try:
    f = open('E:\\pylog.txt', 'w')
    f.write('\n'.join(log))
    f.close()
except:
    try:
        f = open('C:\\Data\\pylog.txt', 'w')
        f.write('\n'.join(log))
        f.close()
    except:
        pass
