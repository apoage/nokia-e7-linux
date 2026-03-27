# launch_hello.py - Find where exe is (or isn't) and test launch
import os
import e32
import sys

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

w("=== Exe hunt ===")

# Check ALL drives for test.exe and hello.exe
w("\n--- sys/bin scan (all drives) ---")
found_exes = []
for drv in ['C', 'D', 'E', 'F', 'Z']:
    bindir = '%s:\\sys\\bin' % drv
    try:
        files = os.listdir(bindir)
        count = len(files)
        # Look for our exes
        for fn in files:
            fl = fn.lower()
            if fl in ['test.exe', 'hello.exe', 'memread.exe']:
                fp = bindir + '\\' + fn
                sz = os.path.getsize(fp)
                w("FOUND: %s (%d bytes)" % (fp, sz))
                found_exes.append(fp)
        # Also report total count
        w("%s: %d files total" % (bindir, count))
    except Exception, ex:
        w("%s: %s" % (bindir, str(ex)))

# Check private app registration
w("\n--- App registration check ---")
for drv in ['C', 'E']:
    regdir = '%s:\\private\\10003a3f\\import\\apps' % drv
    try:
        files = os.listdir(regdir)
        for fn in files:
            fl = fn.lower()
            if 'hello' in fl or 'test' in fl:
                fp = regdir + '\\' + fn
                w("REG: %s (%d bytes)" % (fp, os.path.getsize(fp)))
    except Exception, ex:
        w("%s: %s" % (regdir, str(ex)))

# Check resource/apps
w("\n--- Resource check ---")
for drv in ['C', 'E']:
    resdir = '%s:\\resource\\apps' % drv
    try:
        files = os.listdir(resdir)
        for fn in files:
            fl = fn.lower()
            if 'hello' in fl or 'test' in fl:
                fp = resdir + '\\' + fn
                w("RSC: %s (%d bytes)" % (fp, os.path.getsize(fp)))
    except Exception, ex:
        w("%s: %s" % (resdir, str(ex)))

# Check SIS registry (installed apps)
w("\n--- SIS registry ---")
for drv in ['C', 'E']:
    sisdir = '%s:\\sys\\install\\sisregistry' % drv
    try:
        for uid_dir in os.listdir(sisdir):
            uid_lower = uid_dir.lower()
            # Our UIDs: e0000099 (hello), e00000bb (test)
            if uid_lower in ['e0000099', 'e00000bb', '100039ce']:
                full = sisdir + '\\' + uid_dir
                w("SIS REG: %s" % full)
                try:
                    for fn in os.listdir(full):
                        w("  %s (%d)" % (fn, os.path.getsize(full + '\\' + fn)))
                except:
                    pass
    except Exception, ex:
        w("%s: %s" % (sisdir, str(ex)))

# Try launching if found
if found_exes:
    for fp in found_exes:
        w("\n--- Launch %s ---" % fp)
        try:
            ret = e32.start_exe(fp, '', 1)
            w("Result: %s" % str(ret))
        except Exception, ex:
            w("Error: %s %s" % (type(ex).__name__, str(ex)))
        e32.ao_sleep(2)
        # Check output
        for op in ['E:\\test_ok.txt', 'C:\\Data\\test_ok.txt',
                    'E:\\hello.txt', 'C:\\Data\\hello.txt',
                    'E:\\hello_err.txt', 'C:\\Data\\hello_err.txt']:
            try:
                if os.path.isfile(op):
                    f = open(op, 'r')
                    w("OUTPUT %s: [%s]" % (op, f.read().strip()))
                    f.close()
            except:
                pass
else:
    w("\nNO EXES FOUND IN sys/bin ON ANY DRIVE")
    # Try launching by name anyway
    for name in ['test.exe', 'hello.exe']:
        w("\n--- Try launch by name: %s ---" % name)
        try:
            ret = e32.start_exe(name, '', 1)
            w("Result: %s" % str(ret))
        except Exception, ex:
            w("Error: %s %s" % (type(ex).__name__, str(ex)))

w("\n=== Done ===")
save_log()
