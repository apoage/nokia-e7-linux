# run.py - Deploy (delete then copy), launch, copy C: logs to E:
import e32, os

# Copy latest exe to both drives - DELETE FIRST to avoid locked file issues
print "deploying..."
try:
    sf = open('E:\\memread_tcb.exe', 'rb')
    data = sf.read()
    sf.close()
    print "src: %d bytes" % len(data)
    for dst in ['C:\\sys\\bin\\memread.exe', 'E:\\sys\\bin\\memread.exe']:
        try:
            os.remove(dst)
            print "deleted: %s" % dst
        except:
            pass
        try:
            df = open(dst, 'wb')
            df.write(data)
            df.close()
            sz = os.path.getsize(dst)
            print "wrote: %s (%d)" % (dst, sz)
            if sz != len(data):
                print "WARNING: size mismatch!"
        except Exception, ex:
            print "fail %s: %s" % (dst, str(ex))
except Exception, ex:
    print "deploy error: %s" % str(ex)

# Clean old E: logs
for fn in ['memread_log.txt', 'memread_start.txt']:
    try:
        os.remove('E:\\' + fn)
    except:
        pass

print "launching..."
try:
    r = e32.start_exe('memread.exe', '', 1)
    print "returned: %s" % str(r)
except Exception, ex:
    print "error: %s" % str(ex)

e32.ao_sleep(2)

# Copy C:\Data logs to E: for USB access
for fn in ['memread_log.txt', 'memread_start.txt', 'padconf.bin']:
    src = 'C:\\Data\\' + fn
    dst = 'E:\\c_' + fn
    try:
        if os.path.isfile(src):
            sf = open(src, 'rb')
            d = sf.read()
            sf.close()
            df = open(dst, 'wb')
            df.write(d)
            df.close()
            print "copied %s (%d bytes)" % (fn, len(d))
    except:
        pass

# Show results
for p in ['E:\\memread_log.txt', 'E:\\c_memread_log.txt',
          'E:\\padconf.bin', 'E:\\c_padconf.bin']:
    try:
        if os.path.isfile(p):
            print "OUTPUT: %s (%d)" % (p, os.path.getsize(p))
    except:
        pass

print "done"
