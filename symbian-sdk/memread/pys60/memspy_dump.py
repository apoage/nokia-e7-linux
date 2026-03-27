# memspy_dump.py - Launch MemSpyConsole and try to dump kernel memory
# MemSpyConsole.exe is properly signed with full capabilities
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

w("=== MemSpy CLI dump ===")

# Clean old MemSpy logs so we can see new output
w("\n--- Cleaning old MemSpy logs ---")
memspy_dir = 'E:\\MemSpy'
try:
    if os.path.isdir(memspy_dir):
        for fn in os.listdir(memspy_dir):
            fp = memspy_dir + '\\' + fn
            try:
                os.remove(fp)
                w("Removed: %s" % fn)
            except:
                pass
    else:
        w("No MemSpy dir yet")
except Exception, ex:
    w("Error: %s" % str(ex))

# Try launching MemSpyConsole with various arguments
# Common patterns: --heap, --stack, --kernel, --memory <addr> <len>
w("\n--- Launching MemSpyConsole ---")

# First try with no args - see what it outputs
try:
    ret = e32.start_exe('MemSpyConsole.exe', '', 1)
    w("MemSpyConsole (no args) returned: %s" % str(ret))
except Exception, ex:
    w("Error: %s %s" % (type(ex).__name__, str(ex)))

e32.ao_sleep(3)

# Try with common arg patterns
for args in ['--help', '-h', 'help', 'kernel', 'system',
             'heap kernel', 'dump 0x48002030 584']:
    w("\nTrying args: '%s'" % args)
    try:
        ret = e32.start_exe('MemSpyConsole.exe', args, 1)
        w("Returned: %s" % str(ret))
    except Exception, ex:
        w("Error: %s %s" % (type(ex).__name__, str(ex)))
    e32.ao_sleep(1)

# Check what MemSpy wrote
w("\n--- MemSpy output files ---")
try:
    if os.path.isdir(memspy_dir):
        for fn in os.listdir(memspy_dir):
            fp = memspy_dir + '\\' + fn
            sz = os.path.getsize(fp)
            w("FILE: %s (%d bytes)" % (fn, sz))
            if sz < 5000:
                try:
                    f = open(fp, 'r')
                    content = f.read()
                    f.close()
                    w(content[:2000])
                except:
                    pass
    else:
        w("No MemSpy output dir")
except Exception, ex:
    w("Error: %s" % str(ex))

# Also try MemSpy.exe (might be different)
w("\n--- Trying MemSpy.exe ---")
try:
    ret = e32.start_exe('MemSpy.exe', '', 1)
    w("MemSpy.exe returned: %s" % str(ret))
except Exception, ex:
    w("Error: %s %s" % (type(ex).__name__, str(ex)))

e32.ao_sleep(2)

# Check for new files
try:
    if os.path.isdir(memspy_dir):
        for fn in os.listdir(memspy_dir):
            fp = memspy_dir + '\\' + fn
            sz = os.path.getsize(fp)
            w("FILE: %s (%d bytes)" % (fn, sz))
except:
    pass

w("\n=== Done ===")
save_log()
