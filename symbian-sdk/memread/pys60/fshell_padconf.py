# fshell_padconf.py - Write FShell script then execute it
import os, e32

# Write FShell script to run readmem
script = """readmem 0x48002030 584 e:\\padconf.bin
readmem 0x48002030 64
meminfo
exit
"""

# Write script file
f = open('E:\\padconf.script', 'w')
f.write(script)
f.close()
print "script written"

# Try launching fshell with script
# fshell.exe accepts: --console vt100cons --exec "source e:\padconf.script"
# Or just: fshell.exe -e "source e:\padconf.script"
for args in [
    'e:\\padconf.script',
    '--exec "source e:\\padconf.script"',
    '-e "readmem 0x48002030 584 e:\\padconf.bin"',
    '"readmem 0x48002030 584 e:\\padconf.bin"',
    ]:
    print "trying: fshell %s" % args
    try:
        r = e32.start_exe('fshell.exe', args, 1)
        print "returned: %s" % str(r)
    except Exception, ex:
        print "error: %s" % str(ex)
    e32.ao_sleep(2)

    # Check if padconf.bin was created
    if os.path.isfile('E:\\padconf.bin'):
        sz = os.path.getsize('E:\\padconf.bin')
        print "FOUND padconf.bin! %d bytes" % sz
        break

# Also try launching readmem directly (it might be a standalone exe)
print "\ntrying readmem.exe directly..."
try:
    r = e32.start_exe('readmem.exe', '0x48002030 584 e:\\padconf.bin', 1)
    print "readmem.exe returned: %s" % str(r)
except Exception, ex:
    print "readmem.exe error: %s" % str(ex)

# Check results
for p in ['E:\\padconf.bin', 'E:\\padconf.script']:
    try:
        if os.path.isfile(p):
            print "FILE: %s (%d bytes)" % (p, os.path.getsize(p))
    except:
        pass

print "done"
