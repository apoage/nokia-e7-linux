#!/usr/bin/env python3
"""
isi_info_probe.py — Deep probe PHONE_INFO (0x1B) service.
This is the only resource that gives us real data.
Try ALL possible message IDs to discover hidden capabilities.
"""
import socket, struct, ctypes, ctypes.util, os, select, fcntl, time, subprocess

AF_PHONET = 35
LOG = "/home/lukas/ps3/e7/docs/isi-info-probe.txt"
libc = ctypes.CDLL(ctypes.util.find_library('c'), use_errno=True)

def make_sa(obj=0, dev=0, resource=0):
    return struct.pack('=HBBB11s', AF_PHONET, obj, dev, resource, b'\x00'*11)
def phonet_bind(sock, **kw):
    addr = make_sa(**kw)
    r = libc.bind(sock.fileno(), addr, len(addr))
    if r < 0: raise OSError(ctypes.get_errno(), os.strerror(ctypes.get_errno()))
def phonet_sendto(sock, data, **kw):
    addr = make_sa(**kw)
    buf = ctypes.create_string_buffer(data)
    return libc.sendto(sock.fileno(), buf, len(data), 0, addr, len(addr))
def phonet_recv(sock, timeout=2.0):
    ready = select.select([sock], [], [], timeout)
    if not ready[0]: return None
    buf = ctypes.create_string_buffer(4096)
    addr = ctypes.create_string_buffer(16)
    alen = ctypes.c_int(16)
    r = libc.recvfrom(sock.fileno(), buf, 4096, 0, addr, ctypes.byref(alen))
    if r <= 0: return None
    _, obj, dev, res = struct.unpack('=HBBB', addr.raw[:5])
    return buf.raw[:r], (obj, dev, res)
def log(msg):
    print(msg)
    with open(LOG, 'a') as f:
        f.write(msg + '\n')
def setup():
    subprocess.run(["ip", "address", "flush", "dev", "usbpn0"], capture_output=True)
    subprocess.run(["ip", "link", "set", "usbpn0", "down"], capture_output=True)
    subprocess.run(["ip", "link", "set", "usbpn0", "up"], capture_output=True)
    tmpsock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    ifr = struct.pack('256s', b'usbpn0')
    result = fcntl.ioctl(tmpsock.fileno(), 0x8933, ifr)
    ifindex = struct.unpack('16sI', result[:20])[1]
    tmpsock.close()
    nl = socket.socket(socket.AF_NETLINK, socket.SOCK_RAW, 0)
    nl.bind((0, 0))
    ifam = struct.pack('=BBBBI', AF_PHONET, 0, 0, 0, ifindex)
    nla = struct.pack('=HH', 5, 2) + bytes([0x10]) + b'\x00\x00\x00'
    total = 16 + len(ifam) + len(nla)
    nlh = struct.pack('=IHHII', total, 20, 0x405, 1, os.getpid())
    nl.send(nlh + ifam + nla)
    nl.recv(4096)
    nl.close()
    s = socket.socket(AF_PHONET, socket.SOCK_DGRAM, 1)
    s.setsockopt(socket.SOL_SOCKET, socket.SO_BINDTODEVICE, b'usbpn0\x00')
    phonet_bind(s, dev=0x10, obj=0x01)
    return s

sock = setup()
log(f"\n=== PHONE_INFO Deep Probe ({time.strftime('%Y-%m-%d %H:%M:%S')}) ===")

# Verify connection
phonet_sendto(sock, bytes([0xFF, 0x00, 0x41]), dev=0x00, resource=0x1B)
result = phonet_recv(sock)
if not result:
    print("Phone not responding!")
    exit(1)
log("Phone alive\n")

# === Scan all message IDs on PHONE_INFO ===
log("--- Scanning msg IDs 0x00-0xFF on PHONE_INFO (0x1B) ---")
for msg_id in range(0x00, 0x100):
    # Send with msg_id and minimal payload
    msg = bytes([msg_id, msg_id, 0x00, 0x00, 0x00, 0x00])
    phonet_sendto(sock, msg, dev=0x00, resource=0x1B)
    result = phonet_recv(sock, timeout=0.5)
    if result:
        data, addr = result
        resp_msg = data[1] if len(data) > 1 else 0
        if resp_msg == 0xF0:
            sub = data[2] if len(data) > 2 else 0
            if sub not in (0x01, 0x14):  # Skip common errors
                log(f"  msg 0x{msg_id:02x}: error 0x{sub:02x} [{len(data)}B] {data.hex()}")
        else:
            log(f"  msg 0x{msg_id:02x}: *** RESPONDS msg=0x{resp_msg:02x} [{len(data)}B] {data.hex()} ***")
    time.sleep(0.3)

# === Also probe SIM (0x09) which responded ===
log("\n--- Scanning msg IDs 0x00-0x30 on SIM (0x09) ---")
for msg_id in range(0x00, 0x30):
    msg = bytes([msg_id, msg_id, 0x00, 0x00])
    phonet_sendto(sock, msg, dev=0x00, resource=0x09)
    result = phonet_recv(sock, timeout=0.5)
    if result:
        data, addr = result
        resp_msg = data[1] if len(data) > 1 else 0
        if resp_msg != 0xF0:
            log(f"  msg 0x{msg_id:02x}: *** msg=0x{resp_msg:02x} [{len(data)}B] {data.hex()} ***")
    time.sleep(0.3)

sock.close()
log(f"\n=== Probe Complete ===")
