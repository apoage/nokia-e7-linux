#!/usr/bin/env python3
"""bridge_daemon.py — persistent PC-side hub for the E7 reverse bridge.

- listens 0.0.0.0:4445 for the phone agent (auto re-accept after drops)
- PINGs the phone every 10 s while idle so Symbian keeps the WLAN up
- control port 127.0.0.1:5555: a control client is spliced 1:1 onto the
  phone link (full bridge_pc protocol passthrough)

Usage:
    python3 bridge_daemon.py &
    python3 bridge_pc.py daemon ping          # etc, same ops as direct
"""
import select
import socket
import sys
import threading
import time

PHONE_PORT = 4445
CTRL_PORT = 5555


class Hub:
    def __init__(self):
        self.phone = None
        self.phone_addr = None
        self.ctrl_active = False
        self.lock = threading.Lock()
        self.logf = open("/tmp/bridge_daemon.log", "a", buffering=1)

    def log(self, msg):
        self.logf.write("%s %s\n" % (time.strftime("%H:%M:%S"), msg))

    def phone_connected(self):
        with self.lock:
            return self.phone is not None

    def wait_phone(self, srv):
        while True:
            sock, addr = srv.accept()
            # never let a host-side test agent steal the phone slot
            selfown = self.own_ip()
            if selfown and addr[0] == selfown:
                self.log("rejecting self-connection from %s" % (addr[0],))
                try:
                    sock.close()
                except Exception:
                    pass
                continue
            with self.lock:
                if self.phone is not None:
                    try:
                        self.phone.close()
                    except Exception:
                        pass
                self.phone = sock
                self.phone_addr = addr
            self.log("phone connected from %s" % (addr,))
            return sock

    @staticmethod
    def own_ip():
        try:
            s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
            s.connect(("8.8.8.8", 53))
            ip = s.getsockname()[0]
            s.close()
            return ip
        except Exception:
            return None

    def keepalive_loop(self):
        fails = 0
        while True:
            time.sleep(10)
            with self.lock:
                s = self.phone
                busy = self.ctrl_active
            if s is None or busy:
                continue
            try:
                s.settimeout(10)
                s.sendall(b"PING\n")
                try:
                    s.recv(4096)  # drain the OK line so splices stay clean
                except socket.timeout:
                    pass
                s.settimeout(None)
                fails = 0
            except Exception as e:
                fails += 1
                self.log("keepalive fail #%d: %r" % (fails, e))
                if fails >= 3:
                    with self.lock:
                        self.phone = None
                    self.log("phone dropped after 3 keepalive fails")

    def serve_ctrl(self, ctrl_srv, phone_srv):
        while True:
            ctrl, caddr = ctrl_srv.accept()
            self.log("ctrl client %s" % (caddr,))
            # make sure we have a phone link
            with self.lock:
                sock = self.phone
            if sock is None:
                try:
                    ctrl.sendall(b"ERR phone not connected\n")
                    ctrl.close()
                except Exception:
                    pass
                self.log("ctrl rejected: no phone")
                continue
            with self.lock:
                self.ctrl_active = True
            # splice ctrl <-> phone until either side closes
            try:
                while True:
                    with self.lock:
                        sock = self.phone
                    if sock is None:
                        break
                    r, _, _ = select.select([ctrl, sock], [], [], 30)
                    if not r:
                        continue
                    if ctrl in r:
                        data = ctrl.recv(4096)
                        if not data:
                            break
                        sock.sendall(data)
                    if sock in r:
                        data = sock.recv(4096)
                        if not data:
                            break
                        ctrl.sendall(data)
            except Exception as e:
                self.log("splice ended: %r" % e)
            try:
                ctrl.close()
            except Exception:
                pass
            with self.lock:
                self.ctrl_active = False
            self.log("ctrl client gone")


def main():
    hub = Hub()
    phone_srv = socket.socket()
    phone_srv.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    phone_srv.bind(("0.0.0.0", PHONE_PORT))
    phone_srv.listen(1)
    ctrl_srv = socket.socket()
    ctrl_srv.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    ctrl_srv.bind(("127.0.0.1", CTRL_PORT))
    ctrl_srv.listen(1)
    hub.log("daemon up: phone :%d, ctrl 127.0.0.1:%d" % (PHONE_PORT, CTRL_PORT))
    print("bridge_daemon: phone :%d ctrl :%d" % (PHONE_PORT, CTRL_PORT))

    threading.Thread(target=hub.keepalive_loop, daemon=True).start()
    threading.Thread(target=hub.serve_ctrl, args=(ctrl_srv, phone_srv),
                     daemon=True).start()

    # main thread: maintain phone connection forever
    while True:
        hub.wait_phone(phone_srv)


if __name__ == "__main__":
    main()
