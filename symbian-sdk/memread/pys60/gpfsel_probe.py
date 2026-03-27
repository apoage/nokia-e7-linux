# gpfsel_probe.py v4 -- full probes, saves after each, Py2.5 safe
# Target: Nokia E7-00 with PyS60 1.4.5 (Python 2.5)
# BCM2835-compatible GPIO base: phys 0x20200000, kernel VA 0xC8004xxx
# GPFSEL0-6 at offsets 0x00-0x18 (7 regs x 4 bytes = 28 bytes)
# All probes are READ-ONLY -- no hardware writes.

import os, struct, e32

try:
    import appuifw
except:
    appuifw = None

try:
    import socket
except:
    socket = None

try:
    import sysinfo
except:
    sysinfo = None

# --- Logging ---

log = []

def l(msg):
    log.append(msg)
    print msg

def save():
    txt = '\n'.join(log)
    for p in ['E:\\gpfsel_probe.txt', 'C:\\Data\\gpfsel_probe.txt']:
        try:
            f = open(p, 'w')
            f.write(txt)
            f.close()
        except:
            pass

# --- Constants ---

GPIO_PHYS = 0x20200000
GPIO_KVA  = 0xC8004000
GPFSEL_OFFSETS = [0x00, 0x04, 0x08, 0x0C, 0x10, 0x14, 0x18]
GPFSEL_NAMES   = ['GPFSEL0', 'GPFSEL1', 'GPFSEL2', 'GPFSEL3', 'GPFSEL4', 'GPFSEL5', 'GPFSEL6']

CANDIDATE_BASES = [
    (0x20200000, "phys (unmapped, expect fail)"),
    (0xC8004000, "ROM-derived KVA guess"),
    (0xC8200000, "KVA offset variant"),
    (0xFC200000, "high-map variant (RPi-like)"),
    (0xF2200000, "alt high-map"),
]

l("=" * 60)
l("GPFSEL Probe v4 - BCM2727B1 GPIO Function Select Registers")
l("=" * 60)
l("")
save()

# ===================================================================
# PROBE 1: RomPatcherPlus.exe via e32.start_exe
# ===================================================================
l("--- PROBE 1: RomPatcherPlus.exe command-line launch ---")

rpp_paths = [
    'C:\\sys\\bin\\RomPatcherPlus.exe',
    'E:\\sys\\bin\\RomPatcherPlus.exe',
]
rpp_found = None
for p in rpp_paths:
    try:
        if os.path.isfile(p):
            l("  FOUND: %s (%d bytes)" % (p, os.path.getsize(p)))
            if rpp_found is None:
                rpp_found = p
    except:
        pass

if rpp_found is None:
    l("  RomPatcherPlus.exe not found")
else:
    l("  Attempting launch (GUI app, no CLI dump expected)...")
    try:
        r = e32.start_exe(os.path.basename(rpp_found), '', 0)
        l("  start_exe returned: %s" % str(r))
    except Exception, ex:
        l("  start_exe error: %s" % str(ex))

# Check patcher LDD files
l("\n  Patcher LDD files:")
patcher_ldds = []
for name in ['patcher.ldd', 'patcherS3.ldd', 'patcherShadow.ldd']:
    for drv in ['C', 'E']:
        fp = '%s:\\sys\\bin\\%s' % (drv, name)
        try:
            if os.path.isfile(fp):
                sz = os.path.getsize(fp)
                l("    FOUND: %s (%d bytes)" % (fp, sz))
                patcher_ldds.append(fp)
                try:
                    f = open(fp, 'rb')
                    hdr = f.read(16)
                    f.close()
                    l("      header: %s" % ' '.join(['%02x' % ord(b) for b in hdr]))
                except:
                    pass
        except:
            pass

if not patcher_ldds:
    l("    No patcher LDD files found")

l("")
save()

# ===================================================================
# PROBE 2: Filesystem-based access
# ===================================================================
l("--- PROBE 2: Filesystem-based access ---")

dev_paths = [
    '/dev/mem', '/dev/kmem', '/dev/gpio',
]

for p in dev_paths:
    try:
        if os.path.exists(p):
            l("  EXISTS: %s" % p)
        else:
            l("  not found: %s" % p)
    except Exception, ex:
        l("  error: %s: %s" % (p, str(ex)))

# Scan C:\sys\bin for interesting files (skip Z: -- hangs)
l("\n  Scanning C:\\sys\\bin for gpio/mem/hw related files...")
try:
    bins = os.listdir('C:\\sys\\bin')
    matches = []
    keywords = ['gpio', 'mem', 'hw_', 'hal', 'patcher', 'patch',
                'readmem', 'dump', 'physical', 'kernel', 'fshell']
    for fn in bins:
        fnl = fn.lower()
        for kw in keywords:
            if fnl.find(kw) >= 0:
                matches.append(fn)
                break
    if matches:
        matches.sort()
        for m in matches:
            fp = 'C:\\sys\\bin\\%s' % m
            try:
                sz = os.path.getsize(fp)
                l("    %s (%d bytes)" % (m, sz))
            except:
                l("    %s (size unknown)" % m)
    else:
        l("    No matching binaries found")
    l("    (total files in C:\\sys\\bin: %d)" % len(bins))
except Exception, ex:
    l("  Cannot list C:\\sys\\bin: %s" % str(ex))

# Check for memoryAccess or fshell LDDs
l("\n  Checking for memory-access LDDs...")
for name in ['memoryaccess-fshell.ldd', 'memoryaccess.ldd',
             'memaccess.ldd', 'crash.ldd']:
    for drv in ['C', 'E']:
        fp = '%s:\\sys\\bin\\%s' % (drv, name)
        try:
            if os.path.isfile(fp):
                l("    FOUND: %s (%d bytes)" % (fp, os.path.getsize(fp)))
        except:
            pass

l("")
save()

# ===================================================================
# PROBE 3: Socket-based local IPC
# ===================================================================
l("--- PROBE 3: Socket-based IPC ---")

if socket is None:
    l("  socket module not available")
else:
    debug_ports = [4444, 1234, 8080, 9999, 2323, 4242, 7777]
    for port in debug_ports:
        try:
            s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            s.settimeout(1)
            r = s.connect_ex(('127.0.0.1', port))
            if r == 0:
                l("  PORT %d: OPEN!" % port)
                try:
                    s.sendall('help\r\n')
                    e32.ao_sleep(0.5)
                    data = s.recv(1024)
                    l("    recv: %s" % repr(data[:200]))
                except:
                    l("    (connected but no response)")
                s.close()
            else:
                s.close()
        except Exception, ex:
            pass
    l("  No debug servers found on common ports")

    l("\n  Socket address families:")
    for a in dir(socket):
        if a[0] != '_':
            try:
                v = getattr(socket, a)
                if type(v) == int:
                    l("    %s = %d" % (a, v))
            except:
                pass

l("")
save()

# ===================================================================
# PROBE 4: e32 module internals
# ===================================================================
l("--- PROBE 4: e32 module internals ---")

l("  dir(e32):")
e32_attrs = dir(e32)
e32_attrs.sort()
for a in e32_attrs:
    try:
        v = getattr(e32, a)
        t = type(v).__name__
        if callable(v):
            l("    %s  [%s, callable]" % (a, t))
        else:
            l("    %s = %s  [%s]" % (a, repr(v)[:80], t))
    except Exception, ex:
        l("    %s  [error: %s]" % (a, str(ex)))

# Try hidden e32 functions
l("\n  Testing e32 hidden functions...")

try:
    r = e32._s60_load_ldd('patcher.ldd')
    l("    _s60_load_ldd('patcher.ldd'): %s" % str(r))
except AttributeError:
    l("    _s60_load_ldd: not available")
except Exception, ex:
    l("    _s60_load_ldd error: %s" % str(ex))

mem_funcs = ['_peek', '_poke', '_read_mem', '_mem_read', '_phys_read',
             'mem_read', 'peek', 'read_raw', 'raw_read', '_raw_read',
             '_s60_mem_read', '_s60_peek']
for fn in mem_funcs:
    try:
        func = getattr(e32, fn)
        l("    e32.%s EXISTS! type=%s" % (fn, type(func).__name__))
        try:
            r = func(GPIO_KVA, 4)
            l("      %s(0x%08x, 4) = %s" % (fn, GPIO_KVA, repr(r)))
        except:
            try:
                r = func(GPIO_KVA)
                l("      %s(0x%08x) = %s" % (fn, GPIO_KVA, repr(r)))
            except Exception, ex2:
                l("      call error: %s" % str(ex2))
    except AttributeError:
        pass
    except Exception, ex:
        l("    e32.%s error: %s" % (fn, str(ex)))

l("\n  Ao_timer methods:")
try:
    t = e32.Ao_timer()
    for a in dir(t):
        if a[:2] != '__':
            l("    %s" % a)
    t.cancel()
except Exception, ex:
    l("    error: %s" % str(ex))

l("")
save()

# ===================================================================
# PROBE 5: appuifw hardware access
# ===================================================================
l("--- PROBE 5: appuifw module ---")

if appuifw is None:
    l("  appuifw not available")
else:
    l("  dir(appuifw):")
    aa = dir(appuifw)
    aa.sort()
    for a in aa:
        if a[0] != '_':
            try:
                v = getattr(appuifw, a)
                t = type(v).__name__
                if t in ('int', 'str', 'float', 'unicode'):
                    l("    %s = %s" % (a, repr(v)[:60]))
                else:
                    l("    %s  [%s]" % (a, t))
            except:
                l("    %s  [error]" % a)

l("")
save()

# ===================================================================
# PROBE 6: HAL / sysinfo / other system APIs
# ===================================================================
l("--- PROBE 6: HAL / sysinfo / other system APIs ---")

if sysinfo is not None:
    l("  sysinfo available:")
    sa = dir(sysinfo)
    sa.sort()
    for a in sa:
        if a[0] != '_':
            try:
                v = getattr(sysinfo, a)
                if callable(v):
                    try:
                        r = v()
                        l("    %s() = %s" % (a, repr(r)[:80]))
                    except:
                        l("    %s()  [callable, error]" % a)
                else:
                    l("    %s = %s" % (a, repr(v)[:80]))
            except:
                l("    %s  [error]" % a)
else:
    l("  sysinfo not available")

# Other modules
probe_modules = [
    'telephone', 'messaging', 'inbox', 'contacts', 'calendar',
    'audio', 'graphics', 'topwindow', 'globalui', 'btsocket',
    'envy', 'sensor', 'scriptext', 'misty', 'e32db',
    'e32dbm', 'key_codes', 'keycapture', 'logs',
    'gles', 'glcanvas', 'camera', 'location', 'positioning',
]

l("\n  Module availability scan:")
available_mods = []
for mod in probe_modules:
    try:
        m = __import__(mod)
        l("    %s: YES" % mod)
        available_mods.append((mod, m))
    except ImportError:
        pass
    except Exception, ex:
        l("    %s: import error: %s" % (mod, str(ex)))

# Scan for hw-related attrs in available modules
for mod_name, mod in available_mods:
    attrs = dir(mod)
    hw_attrs = []
    for a in attrs:
        if a[0] == '_':
            continue
        al = a.lower()
        hit = False
        for kw in ['hw', 'mem', 'read', 'raw', 'dev', 'reg', 'port', 'io', 'phys']:
            if al.find(kw) >= 0:
                hit = True
                break
        if hit:
            hw_attrs.append(a)
    if hw_attrs:
        l("    %s interesting attrs: %s" % (mod_name, ', '.join(hw_attrs)))

l("")
save()

# ===================================================================
# PROBE 7: FShell readmem
# ===================================================================
l("--- PROBE 7: FShell readmem for GPIO registers ---")

fshell_found = False
for drv in ['C', 'E']:
    fp = '%s:\\sys\\bin\\fshell.exe' % drv
    try:
        if os.path.isfile(fp):
            l("  FOUND: %s" % fp)
            fshell_found = True
    except:
        pass

if fshell_found:
    script_lines = []
    for base, desc in CANDIDATE_BASES:
        outfile = 'e:\\gpfsel_%08x.bin' % base
        script_lines.append('readmem 0x%08x 28 %s' % (base, outfile))
    script_lines.append('readmem 0xC8004000 256 e:\\gpfsel_kva_scan.bin')
    script_lines.append('exit')
    script = '\n'.join(script_lines) + '\n'

    l("  Writing FShell script (%d lines)..." % len(script_lines))
    try:
        f = open('E:\\gpfsel_fshell.script', 'w')
        f.write(script)
        f.close()
        l("  Saved E:\\gpfsel_fshell.script")
    except Exception, ex:
        l("  Write error: %s" % str(ex))

    for args in [
        '--exec "source e:\\gpfsel_fshell.script"',
        '-e "source e:\\gpfsel_fshell.script"',
        'e:\\gpfsel_fshell.script',
    ]:
        l("  Trying: fshell.exe %s" % args)
        try:
            r = e32.start_exe('fshell.exe', args, 1)
            l("    returned: %s" % str(r))
        except Exception, ex:
            l("    error: %s" % str(ex))
        e32.ao_sleep(2)

        found_any = False
        for base, desc in CANDIDATE_BASES:
            outfile = 'E:\\gpfsel_%08x.bin' % base
            try:
                if os.path.isfile(outfile):
                    sz = os.path.getsize(outfile)
                    l("    OUTPUT: %s (%d bytes)" % (outfile, sz))
                    found_any = True
                    if sz >= 4:
                        f = open(outfile, 'rb')
                        data = f.read(28)
                        f.close()
                        for i in range(min(7, len(data) // 4)):
                            val = struct.unpack('<I', data[i*4:(i+1)*4])[0]
                            l("      %s (0x%08x+0x%02x) = 0x%08x" % (
                                GPFSEL_NAMES[i], base, GPFSEL_OFFSETS[i], val))
            except:
                pass

        kva_scan = 'E:\\gpfsel_kva_scan.bin'
        try:
            if os.path.isfile(kva_scan):
                sz = os.path.getsize(kva_scan)
                l("    KVA SCAN: %s (%d bytes)" % (kva_scan, sz))
                found_any = True
                f = open(kva_scan, 'rb')
                data = f.read(256)
                f.close()
                for i in range(min(7, len(data) // 4)):
                    val = struct.unpack('<I', data[i*4:(i+1)*4])[0]
                    l("      offset 0x%02x = 0x%08x" % (i * 4, val))
        except:
            pass

        if found_any:
            l("  SUCCESS -- got readmem output")
            break
    else:
        l("  FShell readmem did not produce output files")
else:
    l("  fshell.exe not found")

l("")
save()

# ===================================================================
# PROBE 8: Custom memread.exe
# ===================================================================
l("--- PROBE 8: Custom memread.exe for GPIO ---")

memread_found = False
for drv in ['C', 'E']:
    fp = '%s:\\sys\\bin\\memread.exe' % drv
    try:
        if os.path.isfile(fp):
            l("  FOUND: %s (%d bytes)" % (fp, os.path.getsize(fp)))
            memread_found = True
    except:
        pass

if memread_found:
    for base, desc in CANDIDATE_BASES[:3]:
        args = '0x%08x 28 e:\\gpfsel_%08x_mr.bin' % (base, base)
        l("  Trying: memread.exe %s" % args)
        try:
            r = e32.start_exe('memread.exe', args, 1)
            l("    returned: %s" % str(r))
        except Exception, ex:
            l("    error: %s" % str(ex))
        e32.ao_sleep(1)
else:
    l("  memread.exe not found")

l("")
save()

# ===================================================================
# PROBE 9: Raw device file access
# ===================================================================
l("--- PROBE 9: Raw device file access ---")

raw_paths = [
    '\\Physical0',
    '\\PhysicalMedia0',
    '\\\\.\\PhysicalDrive0',
]

for p in raw_paths:
    try:
        f = open(p, 'rb')
        data = f.read(16)
        f.close()
        l("  READABLE: %s (first 16 bytes: %s)" % (
            p, ' '.join(['%02x' % ord(b) for b in data])))
    except Exception, ex:
        l("  %s: %s" % (p, str(ex)))

# Read ekern.exe header
l("\n  Checking ekern.exe for CPU info...")
for drv in ['C']:
    ekern = '%s:\\sys\\bin\\ekern.exe' % drv
    try:
        if os.path.isfile(ekern):
            f = open(ekern, 'rb')
            hdr = f.read(256)
            f.close()
            l("  %s: %d bytes header" % (ekern, len(hdr)))
            if len(hdr) >= 52:
                uid1 = struct.unpack('<I', hdr[0:4])[0]
                uid2 = struct.unpack('<I', hdr[4:8])[0]
                uid3 = struct.unpack('<I', hdr[8:12])[0]
                cpu = struct.unpack('<H', hdr[48:50])[0]
                l("    UIDs: 0x%08x 0x%08x 0x%08x" % (uid1, uid2, uid3))
                l("    CPU field: 0x%04x" % cpu)
                cpu_names = {0x1000: 'ARM', 0x2001: 'ARMv5T', 0x2002: 'ARMv6'}
                l("    CPU type: %s" % cpu_names.get(cpu, 'unknown'))
            break
    except Exception, ex:
        l("  %s: %s" % (ekern, str(ex)))

l("")
save()

# ===================================================================
# PROBE 10: Built-in module scan
# ===================================================================
l("--- PROBE 10: Built-in module scan ---")

try:
    import sys
    l("  Python version: %s" % sys.version)
    l("  Platform: %s" % sys.platform)
    l("  Executable: %s" % getattr(sys, 'executable', 'unknown'))
    if hasattr(sys, 'builtin_module_names'):
        l("  Built-in modules: %s" % ', '.join(sys.builtin_module_names))
    if hasattr(sys, 'path'):
        l("  sys.path:")
        for p in sys.path:
            l("    %s" % p)
except Exception, ex:
    l("  sys error: %s" % str(ex))

for mod in ['_symbian_s60', 'e32posix', 'errno', 'imp',
            'thread', 'cStringIO', 'marshal']:
    try:
        m = __import__(mod)
        l("\n  %s available:" % mod)
        attrs = dir(m)
        interesting = []
        for a in attrs:
            if a[:2] != '__':
                interesting.append(a)
        l("    attrs: %s" % ', '.join(interesting[:20]))
    except:
        pass

l("")
save()

# ===================================================================
# SUMMARY
# ===================================================================
l("=" * 60)
l("SUMMARY")
l("=" * 60)
l("")
l("GPIO phys base: 0x%08x" % GPIO_PHYS)
l("GPIO KVA guess: 0x%08x" % GPIO_KVA)
l("GPFSEL range: 0x00 - 0x18 (7 regs, 28 bytes)")
l("")
l("Without ctypes or a kernel-mode LDD channel, PyS60 cannot")
l("directly read physical or kernel-virtual memory. Viable paths:")
l("  1. FShell readmem (if fshell.exe available + memoryAccess LDD)")
l("  2. Custom memread.exe with working CRT (e32.start_exe)")
l("  3. patcher.ldd kernel channel (needs C++ exe to open channel)")
l("  4. Symbian LDD with DPlatChunkHw (needs kernel-mode driver)")
l("")

save()
print "=== gpfsel_probe.py complete ==="
