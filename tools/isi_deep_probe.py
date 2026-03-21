#!/usr/bin/env python3
"""
isi_deep_probe.py — Send known ISI messages to specific resources.

Unlike the scan (which sends VERSION_GET to all), this sends the CORRECT
message type for each resource. The firewall might allow service-specific
messages while rejecting version queries.
"""
import socket, struct, ctypes, ctypes.util, os, select, fcntl, time, subprocess, sys

AF_PHONET = 35
LOG = "/home/lukas/ps3/e7/docs/isi-deep-probe.txt"
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

def query(sock, res, msg, desc=""):
    ret = phonet_sendto(sock, msg, dev=0x00, obj=0x00, resource=res)
    if ret <= 0:
        return None
    result = phonet_recv(sock, timeout=2.0)
    if result:
        data, addr = result
        return data
    return None

sock = setup()
print("Ready!\n")

log(f"\n=== ISI Deep Probe ({time.strftime('%Y-%m-%d %H:%M:%S')}) ===")

# === PHONE_INFO (0x1B) — already works ===
log("\n--- 0x1B PHONE_INFO ---")
data = query(sock, 0x1B, bytes([0x01, 0x00, 0x41]))
if data:
    log(f"  IMEI: [{len(data)}B] {data.hex()}")
data = query(sock, 0x1B, bytes([0x02, 0x07, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00]))
if data:
    log(f"  VERSION: [{len(data)}B] {data.hex()}")
data = query(sock, 0x1B, bytes([0x03, 0x15, 0x00, 0x01]))
if data:
    log(f"  PRODUCT_NAME: [{len(data)}B] {data.hex()}")
# PP READ (Product Profile)
data = query(sock, 0x1B, bytes([0x04, 0x02, 0x00, 0xCA]))
if data:
    log(f"  PP_READ: [{len(data)}B] {data.hex()}")

# === MTC (0x15) — Mobile Terminal Control ===
log("\n--- 0x15 MTC ---")
# MTC_STATE_QUERY_REQ = 0x02
data = query(sock, 0x15, bytes([0x10, 0x02]))
if data:
    log(f"  STATE_QUERY: [{len(data)}B] {data.hex()}")
else:
    log(f"  STATE_QUERY: no response")

# === SIM (0x09) ===
log("\n--- 0x09 SIM ---")
data = query(sock, 0x09, bytes([0x11, 0x19]))  # SIM_STATUS_REQ
if data:
    log(f"  STATUS: [{len(data)}B] {data.hex()}")
else:
    log(f"  STATUS: no response")

# === NETWORK (0x0A) ===
log("\n--- 0x0A NETWORK ---")
data = query(sock, 0x0A, bytes([0x12, 0x00]))  # NET_MODEM_REG_STATUS_GET_REQ
if data:
    log(f"  REG_STATUS: [{len(data)}B] {data.hex()}")
else:
    log(f"  REG_STATUS: no response")

# === FIREWALL (0x43) — SKIP for now, msg 0x06+ may crash phone ===
log("\n--- 0x43 FIREWALL --- SKIPPED (crash risk)")
# Known: msg 0x02 and 0x04 respond safely
# msg 0x06 returned data but may have triggered crash
# TODO: investigate carefully later

# === COMMGR (0x10) ===
log("\n--- 0x10 COMMGR ---")
# COMM_ISI_VERSION_GET_REQ
data = query(sock, 0x10, bytes([0x30, 0x12]))
if data:
    log(f"  VERSION: [{len(data)}B] {data.hex()}")
# PNS_SUBSCRIBED_RESOURCES_IND = 0x10
data = query(sock, 0x10, bytes([0x31, 0x10]))
if data:
    log(f"  SUBSCRIBED: [{len(data)}B] {data.hex()}")

# === Unknown 0x2C (VERSION 04.00) ===
log("\n--- 0x2C UNKNOWN (v4.0) ---")
for msg_id in [0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x10, 0x12, 0x15, 0x20]:
    data = query(sock, 0x2C, bytes([0x40 + msg_id, msg_id, 0x00, 0x00]))
    if data and (len(data) < 2 or data[1] != 0xF0):
        log(f"  msg=0x{msg_id:02x}: [{len(data)}B] {data.hex()}")
    time.sleep(0.1)

# === Unknown 0x0E (VERSION FAFA) ===
log("\n--- 0x0E UNKNOWN (vFAFA) ---")
for msg_id in [0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x10, 0x12, 0x15, 0x20]:
    data = query(sock, 0x0E, bytes([0x50 + msg_id, msg_id, 0x00, 0x00]))
    if data and (len(data) < 2 or data[1] != 0xF0):
        log(f"  msg=0x{msg_id:02x}: [{len(data)}B] {data.hex()}")
    time.sleep(0.1)

# === Unknown 0xF1 (msg=0xCB) ===
log("\n--- 0xF1 UNKNOWN (msg 0xCB) ---")
for msg_id in [0x00, 0x01, 0x02, 0x03, 0x10, 0x12, 0xCA, 0xCB, 0xCC, 0xCD]:
    data = query(sock, 0xF1, bytes([msg_id, msg_id, 0x00, 0x00]))
    if data and (len(data) < 2 or data[1] != 0xF0):
        log(f"  msg=0x{msg_id:02x}: [{len(data)}B] {data.hex()}")
    time.sleep(0.3)

# === ERROR 0x01 resources — try their native messages ===
log("\n--- ERROR 0x01 resources with native messages ---")
# These rejected VERSION_GET but might accept their own message types
error_resources = [
    (0x04, "CALL?", bytes([0x70, 0x01, 0x00, 0x00])),  # CALL_CREATE_REQ?
    (0x0A, "NETWORK", bytes([0x71, 0x00, 0x00, 0x00])),  # NET_SET_REQ?
    (0x0D, "???", bytes([0x72, 0x00, 0x00, 0x00])),
    (0x11, "???", bytes([0x73, 0x00, 0x00, 0x00])),
    (0x42, "???", bytes([0x74, 0x00, 0x00, 0x00])),
    (0x62, "EPOC_INFO", bytes([0x75, 0x00, 0x41])),  # SERIAL_NUMBER_READ?
    (0x8E, "???", bytes([0x76, 0x00, 0x00, 0x00])),
    (0x9D, "???", bytes([0x77, 0x00, 0x00, 0x00])),
    (0xBB, "???", bytes([0x78, 0x00, 0x00, 0x00])),
]

for res, name, msg in error_resources:
    data = query(sock, res, msg)
    if data:
        is_error = len(data) >= 2 and data[1] == 0xF0
        if not is_error:
            log(f"  0x{res:02x} ({name}): *** RESPONDS *** [{len(data)}B] {data.hex()}")
        else:
            sub = data[2] if len(data) > 2 else 0
            log(f"  0x{res:02x} ({name}): error 0x{sub:02x}")
    else:
        log(f"  0x{res:02x} ({name}): no response")
    time.sleep(0.2)

sock.close()
log(f"\n=== Deep Probe Complete ===")
