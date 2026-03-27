# run_patcher.py - Deploy and launch patcher_probe.exe
import e32, os

print "deploying patcher_probe..."
try:
    sf = open('E:\\sys\\bin\\patcher_probe.exe', 'rb')
    data = sf.read()
    sf.close()
    print "src: %d bytes" % len(data)
    # Copy to C:\sys\bin so it's found by name
    try:
        os.remove('C:\\sys\\bin\\patcher_probe.exe')
        print "deleted old C copy"
    except:
        pass
    try:
        df = open('C:\\sys\\bin\\patcher_probe.exe', 'wb')
        df.write(data)
        df.close()
        print "wrote C:\\sys\\bin\\patcher_probe.exe (%d)" % len(data)
    except Exception, ex:
        print "C copy failed: %s" % str(ex)
except Exception, ex:
    print "read error: %s" % str(ex)

# Clean old logs
for fn in ['patcher_log.txt']:
    for drv in ['E:\\', 'C:\\Data\\']:
        try:
            os.remove(drv + fn)
        except:
            pass

print "launching by name..."
try:
    r = e32.start_exe('patcher_probe.exe', '', 1)
    print "returned: %s" % str(r)
except Exception, ex:
    print "error: %s" % str(ex)

e32.ao_sleep(3)

# Copy C:\Data log to E: for USB access
try:
    src = 'C:\\Data\\patcher_log.txt'
    if os.path.isfile(src):
        sf = open(src, 'rb')
        d = sf.read()
        sf.close()
        df = open('E:\\c_patcher_log.txt', 'wb')
        df.write(d)
        df.close()
        print "copied C log (%d bytes)" % len(d)
except:
    pass

# Show results
for p in ['E:\\patcher_log.txt', 'E:\\c_patcher_log.txt']:
    try:
        if os.path.isfile(p):
            print "OUTPUT: %s (%d)" % (p, os.path.getsize(p))
    except:
        pass

print "done"
