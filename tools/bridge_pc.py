#!/usr/bin/env python3
"""bridge_pc.py - PC-side client for the E7 WiFi bridge agent (Task 089).

Usage:
    python3 bridge_pc.py <phone_ip>                      # interactive REPL
    python3 bridge_pc.py <phone_ip> ping
    python3 bridge_pc.py <phone_ip> exec "import e32; print(e32.drive_list())"
    python3 bridge_pc.py <phone_ip> eval "1+1"
    python3 bridge_pc.py <phone_ip> push local.py E:\\deployme.py
    python3 bridge_pc.py <phone_ip> pull E:\\gpfsel_probe.txt out.txt
    python3 bridge_pc.py <phone_ip> peek 0x50800010 16
    python3 bridge_pc.py <phone_ip> poke 0x50800010 0x00
    python3 bridge_pc.py <phone_ip> ls "E:\\"
    python3 bridge_pc.py <phone_ip> rm E:\\old.txt
    python3 bridge_pc.py <phone_ip> script run_probe.py   # push+exec+cleanup

Phone side: symbian-sdk/memread/pys60/bridge_phone.py (PyS60, port 4444).
"""
import base64
import socket
import sys
import os
import re

PORT = 4444
PROMPT = "phone> "


class Bridge:
    def __init__(self, host, port=PORT, timeout=30):
        self.sock = socket.create_connection((host, port), timeout=timeout)
        self.buf = b""

    def _recv_line(self):
        while b"\n" not in self.buf:
            chunk = self.sock.recv(4096)
            if not chunk:
                raise EOFError("connection closed")
            self.buf += chunk
        line, self.buf = self.buf.split(b"\n", 1)
        return line.decode("utf-8", "replace").rstrip("\r")

    def _recv_exact(self, n):
        while len(self.buf) < n:
            chunk = self.sock.recv(min(65536, n - len(self.buf)))
            if not chunk:
                raise EOFError("connection closed")
            self.buf += chunk
        data, self.buf = self.buf[:n], self.buf[n:]
        return data

    def cmd(self, line):
        """Send one command line, return (status_line, payload_bytes|None)."""
        self.sock.sendall(line.encode("utf-8") + b"\n")
        status = self._recv_line()
        m = re.match(r"^DATA (\d+)$", status)
        if m:
            return status, self._recv_exact(int(m.group(1)))
        m = re.match(r"^OK B64 (\S+)$", status)
        if m:
            return status, base64.b64decode(m.group(1))
        return status, None

    # convenience wrappers -----------------------------------------------
    def ping(self):
        s, _ = self.cmd("PING")
        return s

    def exec_(self, code):
        b64 = base64.b64encode(code.encode("utf-8")).decode()
        s, out = self.cmd("EXEC %s" % b64)
        if out is not None:
            sys.stdout.write(out.decode("utf-8", "replace"))
        return s

    def eval_(self, expr):
        return self.cmd("EVAL %s" % expr)[0]

    def push(self, local, remote):
        with open(local, "rb") as f:
            data = f.read()
        self.sock.sendall(("PUSH %s %d\n" % (remote, len(data))).encode() +
                          data)
        return self._recv_line()

    def pull(self, remote, local=None):
        s, data = self.cmd("PULL %s" % remote)
        if data is not None:
            if local:
                with open(local, "wb") as f:
                    f.write(data)
            else:
                sys.stdout.buffer.write(data)
        return s

    def peek(self, addr, n=1):
        return self.cmd("PEEK %s %d" % (addr, n))[0]

    def xpeek(self, addr, n=1):
        """Crash-isolated read via native peek.exe child on the phone."""
        return self.cmd("XPEEK %s %d" % (addr, n))[0]

    def poke(self, addr, val):
        return self.cmd("POKE %s %s" % (addr, val))[0]

    def ls(self, d):
        s, data = self.cmd("LS %s" % d)
        if data:
            print(data.decode("utf-8", "replace"))
        return s

    def script(self, path):
        """Push a python file, exec it, remove it; print output."""
        remote = "E:\\_bridge_tmp.py"
        print(self.push(path, remote))
        s = self.exec_(open(path).read())
        print(s)
        print(self.cmd("RM %s" % remote)[0])

    def close(self):
        try:
            self.cmd("EXITCONN")
        except Exception:
            pass
        self.sock.close()


def listen_mode(interactive):
    """Wait for the phone's reverse connection (agent v7+), then run the
    normal command dispatch against it. A background pinger keeps traffic
    flowing so Symbian doesn't reap the WLAN."""
    import threading
    srv = socket.socket()
    srv.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    srv.bind(("0.0.0.0", 4445))
    srv.listen(1)
    print("waiting for phone to connect to 10.0.0.117:4445 ...")
    sock, addr = srv.accept()
    print("phone connected from %s" % (addr,))
    br = Bridge.__new__(Bridge)
    br.sock = sock
    br.buf = b""
    stop = threading.Event()

    def pinger():
        while not stop.is_set():
            stop.wait(10)
            if stop.is_set():
                return
            try:
                br.cmd("PING")
            except Exception:
                return

    t = threading.Thread(target=pinger)
    t.daemon = True
    t.start()
    if interactive:
        r = dispatch_repl(br, str(addr[0]))
    else:
        dispatch_one(br)
    stop.set()


def dispatch_repl(br, host="?"):
    print("commands: ping info exec eval push pull peek xpeek poke ls rm "
          "script die -- Ctrl-D to quit")
    while True:
        try:
            line = input(PROMPT).strip()
        except EOFError:
            break
        if not line:
            continue
        if run_op(br, line) == "die":
            break


def run_op(br, line):
    parts = line.split(" ", 1)
    op = parts[0]
    arg = parts[1] if len(parts) > 1 else ""
    try:
        if op == "ping":
            print(br.ping())
        elif op == "info":
            s, data = br.cmd("INFO")
            if data:
                print(data.decode("utf-8", "replace"))
            print(s)
        elif op == "exec":
            print(br.exec_(arg))
        elif op == "eval":
            print(br.eval_(arg))
        elif op == "push":
            a = arg.split(" ")
            print(br.push(a[0], a[1]))
        elif op == "pull":
            a = arg.split(" ")
            print(br.pull(a[0], a[1] if len(a) > 1 else None))
        elif op == "peek":
            a = arg.split(" ")
            print(br.peek(a[0], int(a[1]) if len(a) > 1 else 1))
        elif op == "xpeek":
            a = arg.split(" ")
            print(br.xpeek(a[0], int(a[1]) if len(a) > 1 else 1))
        elif op == "poke":
            a = arg.split(" ")
            print(br.poke(a[0], a[1]))
        elif op == "ls":
            print(br.ls(arg or "E:\\"))
        elif op == "rm":
            print(br.cmd("RM %s" % arg)[0])
        elif op == "script":
            print(br.script(arg))
        elif op == "die":
            print(br.cmd("DIE")[0])
            return "die"
        else:
            print("unknown op, try: ping info exec eval push pull peek "
                  "xpeek poke ls rm script die")
    except Exception as e:
        print("error: %r" % e)
    return None


def dispatch_one(br):
    op = sys.argv[2]
    if op == "ping":
        print(br.ping())
    elif op == "info":
        s, data = br.cmd("INFO")
        if data:
            print(data.decode("utf-8", "replace"))
        print(s)
    elif op == "exec":
        code = sys.argv[3] if len(sys.argv) > 3 else sys.stdin.read()
        print(br.exec_(code))
    elif op == "eval":
        print(br.eval_(sys.argv[3]))
    elif op == "push":
        print(br.push(sys.argv[3], sys.argv[4]))
    elif op == "pull":
        print(br.pull(sys.argv[3], sys.argv[4] if len(sys.argv) > 4 else None))
    elif op == "peek":
        print(br.peek(sys.argv[3], int(sys.argv[4]) if len(sys.argv) > 4 else 1))
    elif op == "xpeek":
        print(br.xpeek(sys.argv[3], int(sys.argv[4]) if len(sys.argv) > 4 else 1))
    elif op == "poke":
        print(br.poke(sys.argv[3], sys.argv[4]))
    elif op == "ls":
        print(br.ls(sys.argv[3] if len(sys.argv) > 3 else "E:\\"))
    elif op == "rm":
        print(br.cmd("RM %s" % sys.argv[3])[0])
    elif op == "script":
        br.script(sys.argv[3])
    elif op == "die":
        print(br.cmd("DIE")[0])
    else:
        print("unknown op:", op)
        sys.exit(1)


def main():
    if len(sys.argv) < 2:
        print(__doc__)
        sys.exit(1)
    if sys.argv[1] == "daemon":
        # talk through the persistent hub (bridge_daemon.py)
        br = Bridge("127.0.0.1", port=5555)
        if len(sys.argv) == 2:
            dispatch_repl(br, "daemon")
        else:
            dispatch_one(br)
            br.close()
        return
    if sys.argv[1] == "listen":
        listen_mode(interactive=len(sys.argv) == 2)
        return
    host = sys.argv[1]
    br = Bridge(host)
    if len(sys.argv) == 2:
        dispatch_repl(br, host)
        return
    dispatch_one(br)
    br.close()


if __name__ == "__main__":
    main()
