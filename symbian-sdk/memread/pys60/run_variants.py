# run_variants.py — Task 090 device-side verification
#
# Tries to start each exe test variant and reports the result + marker files.
# Deploy via mass storage, run from PyS60 shell.
import e32
import os

VARIANTS = ['A', 'B', 'C']

print('--- task 090 exe variant matrix ---')
for v in VARIANTS:
    exe = u'E:\\sys\\bin\\test_%s.exe' % v
    try:
        r = e32.start_exe(u'test_%s.exe' % v, u'')
        print('start test_%s: rc=%s' % (v, r))
    except Exception, e:
        print('start test_%s: EXC %s' % (v, e))

# start_exe is asynchronous-ish; give them a moment
e32.ao_sleep(3)

print('--- markers ---')
for v in VARIANTS:
    p = u'E:\\test_ok_%s.txt' % v
    try:
        if os.path.exists(p):
            f = open(p, 'r')
            print('%s: %s' % (p, f.read().strip()))
            f.close()
        else:
            print('%s: ABSENT' % p)
    except Exception, e:
        print('%s: EXC %s' % (p, e))
print('--- done ---')
