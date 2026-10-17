# bridge_phone.py - WiFi remote-execution agent for the E7 (Task 089)
#
# Deploys once via USB mass storage, started from the PyS60 shell.
# Listens on TCP 4444 and speaks a line protocol with tools/bridge_pc.py.
#
# Compatible with PyS60 1.4.5 (Python 2.4-era: NO conditional expressions,
# NO bytes literals, NO `except as`, NO with/format) and host Python 3 for
# loopback testing. Empty-bytes and joins are built without b'' literals.
#
# Protocol (requests: one line; responses: one status line, then a raw
# payload of the given size ONLY when the status line is 'DATA <n>'):
#   PING                          -> OK <banner>
#   INFO                          -> DATA <len>\n<lines: ip, python, hack>
#   EXEC <base64 python code>     -> OK B64 <base64 stdout>  / ERR <why>
#   EVAL <python expression>      -> OK <repr>               / ERR <why>
#   PUSH <path> <size>\n<bytes>   -> OK wrote <n>
#   PULL <path>                   -> DATA <size>\n<bytes>
#   PEEK <addr> [n]               -> OK <hex bytes>          (via _hack)
#   POKE <addr> <byte>            -> OK
#   LS <dir>                      -> DATA <len>\n<names>
#   RM <path>                     -> OK
#   DIE                           -> OK (agent exits)
import socket
import base64
import sys
import os
import time

PORT = 4444
PC_IP = '10.0.0.117'   # PC running `bridge_pc.py listen` (reverse target)
PC_PORT = 4445
PY3 = sys.version_info[0] >= 3

LOG_PATH = 'E:\\bridge_log.txt'
if PY3:
    LOG_PATH = '/tmp/bridge_phone.log'  # host loopback mode

_logf = None


def log(msg):
    global _logf
    line = '%s %s' % (time.strftime('%H:%M:%S'), msg)
    try:
        sys.stdout.write(line + '\n')
    except Exception:
        pass
    try:
        if _logf is None:
            _logf = open(LOG_PATH, 'a')
        _logf.write(line + '\n')
        _logf.flush()
    except Exception:
        pass


def keep_awake():
    # Fight screen-lock / power-save so WLAN stays up; best effort.
    try:
        import miso
        miso.reset_inactivity_time()
        return
    except Exception:
        pass
    try:
        import e32
        e32.reset_inactivity()
    except Exception:
        pass


def xpeek(addr, n):
    # Crash-isolated read via native peek.exe child: a bad address kills
    # only the child; we report its exit code instead of dying.
    import e32
    rc = e32.start_exe('peek.exe', '%x %d' % (addr, n), 1)
    if rc != 0:
        return 'ERR peek child rc=%s (unmapped address?)' % rc
    f = open(u'E:\\peek_out.txt', 'r')
    line = f.read().strip()
    f.close()
    return 'OK %s' % to_native_str(line)


# --- optional _hack memory access (may not be installed) -----------------

hack = None


def load_hack():
    global hack
    try:
        import _hack
        hack = _hack
        log('_hack loaded: %s' % dir(_hack))
    except Exception:
        log('_hack not available: %s' % sys.exc_info()[1])


def hack_peek(addr):
    for name in ('peek', 'read8', 'read_byte', 'peekb'):
        fn = getattr(hack, name, None)
        if fn:
            return fn(addr)
    raise Exception('no peek function in _hack')


def hack_poke(addr, val):
    for name in ('poke', 'write8', 'write_byte', 'pokeb'):
        fn = getattr(hack, name, None)
        if fn:
            fn(addr, val)
            return
    raise Exception('no poke function in _hack')


# --- py2.4/py3-polymorphic helpers -----------------------------------------

def b64e(data):
    # ascii str out; py2.2 core has only encodestring, py3 only b64encode
    if PY3:
        if isinstance(data, str):
            data = data.encode('latin-1')
        return base64.b64encode(data).decode('ascii')
    return base64.encodestring(data).replace('\n', '')


def b64d(s):
    if PY3:
        return base64.b64decode(s)
    return base64.decodestring(s)


def to_native_str(data):
    # bytes -> str on py3; everything else passes through
    if PY3:
        if isinstance(data, bytes):
            return data.decode('latin-1')
        return data
    return data


def py3_bytes(s):
    # native str -> bytes for sockets/files (differs only on py3)
    if PY3 and isinstance(s, str):
        return s.encode('latin-1')
    return s


def join_chunks(chunks):
    if PY3:
        out = bytes(0)
        for c in chunks:
            out = out + c
        return out
    return ''.join(chunks)


def unicode_path(s):
    # ASCII path in -> path suitable for Symbian file APIs (py2 needs unicode)
    if not PY3 and isinstance(s, str):
        return s.decode('ascii')
    return s


# --- socket framing ---------------------------------------------------------

def recv_line(sock):
    buf = ''
    while True:
        c = to_native_str(sock.recv(1))
        if not c:
            return None
        if c == '\n':
            return buf.rstrip('\r')
        buf = buf + c
        if len(buf) > 65536:
            return buf


def recv_exact(sock, n):
    chunks = []
    got = 0
    while got < n:
        chunk = sock.recv(n - got)
        if not chunk:
            raise EOFError('short read')
        chunks.append(chunk)
        got = got + len(chunk)
    return join_chunks(chunks)


def send_line(sock, line):
    sock.sendall(py3_bytes(line + '\n'))


def send_payload(sock, line, data):
    send_line(sock, line)
    if data:
        sock.sendall(py3_bytes(data))


def local_ip():
    try:
        s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        s.connect(('8.8.8.8', 53))
        ip = to_native_str(s.getsockname()[0])
        s.close()
        return ip
    except Exception:
        return 'unknown'


# --- command dispatch ------------------------------------------------------

class _Cap(object):
    def __init__(self):
        self.parts = []

    def write(self, s):
        self.parts.append(s)

    def flush(self):
        pass

    def getvalue(self):
        return ''.join(self.parts)


def run_exec(code):
    # capture stdout while exec'ing (py2.4: no combined except/finally)
    out = _Cap()
    old = sys.stdout
    sys.stdout = out
    try:
        try:
            exec(code, {'__name__': '__bridge__'})
            result = 'OK B64 %s' % b64e(out.getvalue())
        except Exception:
            result = 'ERR exec: %r' % (sys.exc_info()[1],)
    finally:
        sys.stdout = old
    return result


def run_eval(expr):
    try:
        val = eval(expr, {'__name__': '__bridge__'})
        return 'OK %r' % (val,)
    except Exception:
        return 'ERR eval: %r' % (sys.exc_info()[1],)


def handle(sock, addr):
    log('client %s connected' % (addr,))
    while True:
        try:
            line = recv_line(sock)
        except Exception:
            break
        if line is None:
            break
        if not line:
            continue
        keep_awake()
        parts = line.split(' ')
        cmd = parts[0].upper()
        try:
            if cmd == 'PING':
                send_line(sock, 'OK e7-bridge alive %s' % time.strftime('%H:%M:%S'))
            elif cmd == 'INFO':
                info = ['ip=%s' % local_ip(),
                        'python=%s' % (sys.version.split()[0],),
                        'hack=%s' % (hack is not None,)]
                body = '\n'.join(info)
                send_payload(sock, 'DATA %d' % len(body), body)
            elif cmd == 'EXEC':
                code = to_native_str(b64d(parts[1]))
                send_line(sock, run_exec(code))
            elif cmd == 'EVAL':
                send_line(sock, run_eval(line[len('EVAL '):]))
            elif cmd == 'PUSH':
                path = unicode_path(parts[1])
                size = int(parts[2])
                data = recv_exact(sock, size)
                f = open(path, 'wb')
                f.write(data)
                f.close()
                send_line(sock, 'OK wrote %d' % len(data))
            elif cmd == 'PULL':
                f = open(unicode_path(parts[1]), 'rb')
                data = f.read()
                f.close()
                send_payload(sock, 'DATA %d' % len(data), data)
            elif cmd == 'PEEK':
                addr = int(parts[1], 0)
                n = 1
                if len(parts) > 2:
                    n = int(parts[2])
                if hack is None:
                    send_line(sock, 'ERR no _hack module')
                else:
                    vals = []
                    for i in range(n):
                        vals.append(hack_peek(addr + i) & 0xFF)
                    hexs = []
                    for v in vals:
                        hexs.append('%02x' % v)
                    send_line(sock, 'OK %s' % ' '.join(hexs))
            elif cmd == 'POKE':
                addr = int(parts[1], 0)
                val = int(parts[2], 0) & 0xFF
                if hack is None:
                    send_line(sock, 'ERR no _hack module')
                else:
                    hack_poke(addr, val)
                    send_line(sock, 'OK')
            elif cmd == 'LS':
                d = 'E:\\'
                if len(parts) > 1:
                    d = unicode_path(parts[1])
                names = os.listdir(d)
                strs = []
                for n in names:
                    strs.append(to_native_str(n))
                body = '\n'.join(strs)
                send_payload(sock, 'DATA %d' % len(body), body)
            elif cmd == 'RM':
                os.remove(unicode_path(parts[1]))
                send_line(sock, 'OK')
            elif cmd == 'XPEEK':
                # safe read via native child (agent survives bad addresses)
                addr = int(parts[1], 0)
                n = 1
                if len(parts) > 2:
                    n = int(parts[2])
                send_line(sock, xpeek(addr, n))
            elif cmd == 'DIE':
                send_line(sock, 'OK dying')
                log('DIE requested')
                return 'die'
            else:
                send_line(sock, 'ERR unknown cmd %s' % cmd)
        except EOFError:
            break
        except socket.error:
            log('sock error: %s' % sys.exc_info()[1])
            break
        except Exception:
            try:
                send_line(sock, 'ERR %r' % (sys.exc_info()[1],))
            except Exception:
                break
    log('client %s disconnected' % (addr,))
    return None


def open_server():
    srv = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    try:
        srv.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    except Exception:
        pass  # PyS60 socket module lacks SOL_SOCKET etc; fine without it
    srv.bind(('0.0.0.0', PORT))
    srv.listen(1)
    return srv


def set_timeout(sock, secs):
    # PyS60 socket may not support settimeout; timeouts are optional
    try:
        sock.settimeout(secs)
    except Exception:
        pass


def keepalive_ping():
    # Tiny UDP traffic so Symbian doesn't tear the WLAN down when idle
    try:
        u = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        u.sendto('ka', ('10.0.0.117', 9))  # PC, discard port
        u.close()
    except Exception:
        pass


def keepalive_thread():
    while True:
        time.sleep(20)
        keepalive_ping()
        keep_awake()


def start_keepalive():
    mod = None
    try:
        import thread as mod
    except Exception:
        try:
            import _thread as mod
        except Exception:
            mod = None
    if mod is None:
        log('keepalive thread unavailable')
        return
    try:
        mod.start_new_thread(keepalive_thread, ())
        log('keepalive thread started')
    except Exception:
        log('keepalive thread failed: %s' % sys.exc_info()[1])


def reverse_connector():
    # Reverse mode: hold an outbound TCP session to the PC. Symbian won't
    # reap the WLAN while a connection carries traffic, and the retry loop
    # self-heals after drops. Speaks the same protocol as server mode.
    while True:
        try:
            s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            s.connect((PC_IP, PC_PORT))
            log('reverse: connected to %s:%d' % (PC_IP, PC_PORT))
            handle(s, ('pc-reverse', PC_PORT))
            try:
                s.close()
            except Exception:
                pass
            log('reverse: dropped, retrying')
        except Exception:
            pass
        time.sleep(5)


def start_reverse():
    try:
        import thread as mod
    except Exception:
        try:
            import _thread as mod
        except Exception:
            return
    try:
        mod.start_new_thread(reverse_connector, ())
        log('reverse connector started -> %s:%d' % (PC_IP, PC_PORT))
    except Exception:
        log('reverse connector failed: %s' % sys.exc_info()[1])


def main():
    load_hack()
    start_keepalive()
    start_reverse()
    log('bridge starting on 0.0.0.0:%d (local ip: %s)' % (PORT, local_ip()))
    try:
        srv = open_server()
    except Exception:
        log('direct bind failed: %s' % sys.exc_info()[1])
        # PyS60 often needs an active access point before sockets work
        try:
            ap = socket.select_access_point()
            log('access point selected: %s' % ap)
            srv = open_server()
        except Exception:
            log('bind after AP selection failed: %s' % sys.exc_info()[1])
            return
    log('listening')
    while True:
        try:
            sock, addr = srv.accept()
        except Exception:
            log('accept failed: %s' % sys.exc_info()[1])
            time.sleep(1)
            continue
        keep_awake()
        set_timeout(sock, 600)
        try:
            try:
                r = handle(sock, addr)
            except Exception:
                log('handle crashed: %s' % sys.exc_info()[1])
                r = None
        finally:
            try:
                sock.close()
            except Exception:
                pass
        if r == 'die':
            break
    log('bridge stopped')


main()
