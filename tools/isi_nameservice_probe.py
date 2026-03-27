#!/usr/bin/env python3
"""
isi_nameservice_probe.py — Probe ISI Name Service for modem routing

Uses the same AF_PHONET socket approach as isi_scan.py (proven working).

Theory: ISI Name Service (0xDB) dynamically routes resource IDs to devices.
Previous scans sent directly to device 0x60 (modem) — this tries routing
via device 0x00 and letting the Name Service translate.

Usage:
    sudo python3 isi_nameservice_probe.py
"""

import socket, struct, ctypes, ctypes.util, os, select, fcntl, time, sys, subprocess

AF_PHONET = 35

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

def setup_phonet():
    """Set up Phonet interface and address"""
    if not os.path.exists('/sys/class/net/usbpn0'):
        print("ERROR: usbpn0 not found. Is phone connected in Nokia Suite mode?")
        return False

    subprocess.run(["ip", "address", "flush", "dev", "usbpn0"], capture_output=True)
    subprocess.run(["ip", "link", "set", "usbpn0", "down"], capture_output=True)
    subprocess.run(["ip", "link", "set", "usbpn0", "up"], capture_output=True)

    # Get ifindex
    tmpsock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    ifr = struct.pack('256s', b'usbpn0')
    result = fcntl.ioctl(tmpsock.fileno(), 0x8933, ifr)
    ifindex = struct.unpack('16sI', result[:20])[1]
    tmpsock.close()

    # Add Phonet address via netlink (address 0x10 = PN_DEV_PC)
    nl = socket.socket(socket.AF_NETLINK, socket.SOCK_RAW, 0)
    nl.bind((0, 0))
    ifam = struct.pack('=BBBBI', AF_PHONET, 0, 0, 0, ifindex)
    nla = struct.pack('=HH', 5, 2) + bytes([0x10]) + b'\x00\x00\x00'
    total = 16 + len(ifam) + len(nla)
    nlh = struct.pack('=IHHII', total, 20, 0x405, 1, os.getpid())
    nl.send(nlh + ifam + nla)
    resp = nl.recv(4096)
    err = struct.unpack('=i', resp[16:20])[0]
    nl.close()

    if err == 0 or err == -17:  # 0=OK, -17=EEXIST (already added)
        return True
    print(f"  Address add failed: {err}")
    return False

def create_socket():
    s = socket.socket(AF_PHONET, socket.SOCK_DGRAM, 1)
    s.setsockopt(socket.SOL_SOCKET, socket.SO_BINDTODEVICE, b'usbpn0\x00')
    phonet_bind(s, dev=0x10, obj=0x01)
    return s

def query(sock, resource, msg_id, data=b'', dev=0x00, timeout=2.0):
    """Send ISI query and return response"""
    tid = int(time.time() * 100) & 0xFF
    msg = bytes([tid, msg_id]) + data
    ret = phonet_sendto(sock, msg, dev=dev, obj=0x00, resource=resource)
    if ret <= 0:
        return None

    result = phonet_recv(sock, timeout=timeout)
    if not result:
        return None

    resp_data, (obj, resp_dev, resp_res) = result
    return {
        'data': resp_data,
        'dev': resp_dev,
        'res': resp_res,
        'obj': obj,
        'tid': resp_data[0] if len(resp_data) > 0 else 0,
        'msg': resp_data[1] if len(resp_data) > 1 else 0,
        'payload': resp_data[2:] if len(resp_data) > 2 else b'',
    }

def decode_response(resp):
    """Decode ISI response into human-readable string"""
    if not resp:
        return "NO RESPONSE"
    data = resp['data']
    if len(data) < 2:
        return f"SHORT: {data.hex()}"
    msg = data[1]
    if msg == 0xF0:  # COMMON_MESSAGE error
        sub = data[2] if len(data) > 2 else 0
        errcodes = {0x01: "NOT_SUPPORTED", 0x14: "NOT_REACHABLE", 0x17: "NOT_AUTHENTICATED"}
        return f"ERROR: {errcodes.get(sub, f'0x{sub:02x}')}"
    elif msg == 0x13:  # COMM_ISI_VERSION_GET_RESP
        ver = data[2:4].hex() if len(data) >= 4 else "??"
        return f"VERSION {ver}"
    else:
        return f"MSG 0x{msg:02x} [{len(data)}B] {data.hex()}"


# Modem resources to test
MODEM_RESOURCES = {
    0x8E: "PN_AT_MODEM",
    0x91: "PN_MODEM_TEST",
    0x92: "PN_MODEM_NVD",
    0x93: "PN_MODEM_PERM",     # PM READ!
    0xB7: "PN_RF_HAL",
    0xC2: "PN_MODEM_MCE",
    0xC3: "PN_MODEM_MON",
    0xC8: "PN_MODEM_NETWORK",
    0xC9: "PN_MODEM_CALL",
    0xF1: "PN_GSM_RF_TEST",
}

HOST_RESOURCES = {
    0x1B: "PN_PHONE_INFO",
    0x10: "PN_COMMGR",
    0x32: "PN_GSS",
    0x43: "PN_NVD",
    0xDB: "PN_NAMESERVICE",
}

# PNS Name Service message IDs
PNS_NAME_QUERY_REQ = 0x04
PNS_NAME_QUERY_RESP = 0x05


def main():
    print("=== ISI Name Service Modem Routing Probe ===\n")

    if not setup_phonet():
        sys.exit(1)

    sock = create_socket()
    print("Socket ready\n")

    # Step 1: Verify with IMEI read (proven working)
    print("=" * 60)
    print("Step 1: Verify ISI connection (IMEI read)")
    print("=" * 60)
    # INFO_SERIAL_NUMBER_READ_REQ=0x00, INFO_SB_SN_IMEI_PLAIN=0x41
    resp = query(sock, 0x1B, 0x00, data=bytes([0x41]))
    if resp:
        result = decode_response(resp)
        print(f"  PN_PHONE_INFO: {result}")
        print(f"  Raw: {resp['data'].hex()}")
    else:
        print("  FAILED — no response. Check USB connection.")
        sock.close()
        sys.exit(1)
    print("  Connection OK!\n")

    # Step 2: Version query to KNOWN WORKING resources (from original scan)
    # This tells us if version query format is correct
    print("=" * 60)
    print("Step 2: Version query to known-working resources")
    print("=" * 60)
    for res_id, name in [(0x1B, "PN_PHONE_INFO"), (0x43, "PN_NVD")]:
        resp = query(sock, res_id, 0x12)
        result = decode_response(resp)
        print(f"  0x{res_id:02x} {name}: {result}")
        if resp and resp['data']:
            print(f"       raw: {resp['data'].hex()}")

        # Aliveness check
        time.sleep(0.5)
        alive = query(sock, 0x1B, 0x00, data=bytes([0x41]), timeout=2.0)
        if alive:
            print(f"       Phone still alive: YES")
        else:
            print(f"       Phone still alive: NO — CRASHED!")
            print("       Stopping to avoid further damage.")
            sock.close()
            sys.exit(1)
        time.sleep(0.5)
    print()

    # Step 3: Try ONE modem resource via dev=0x00 (safest first)
    print("=" * 60)
    print("Step 3: Single modem resource test via dev=0x00")
    print("=" * 60)
    res_id, name = 0xC8, "PN_MODEM_NETWORK"
    print(f"  Testing 0x{res_id:02x} {name} via dev=0x00...")
    resp = query(sock, res_id, 0x12, dev=0x00)
    result = decode_response(resp)
    print(f"  Result: {result}")
    if resp:
        print(f"  From dev=0x{resp['dev']:02x}, raw: {resp['data'].hex()}")

    time.sleep(0.5)
    alive = query(sock, 0x1B, 0x00, data=bytes([0x41]), timeout=2.0)
    if alive:
        print(f"  Phone still alive: YES")
    else:
        print(f"  Phone still alive: NO — CRASHED on modem resource query!")
        sock.close()
        sys.exit(1)

    print(f"\n  Now trying via dev=0x60 (direct to modem)...")
    resp = query(sock, res_id, 0x12, dev=0x60, timeout=1.5)
    result = decode_response(resp)
    print(f"  Result: {result}")

    time.sleep(0.5)
    alive = query(sock, 0x1B, 0x00, data=bytes([0x41]), timeout=2.0)
    if alive:
        print(f"  Phone still alive: YES")
    else:
        print(f"  Phone still alive: NO — CRASHED on direct modem query!")
        sock.close()
        sys.exit(1)
    print()

    # Step 4: If we're still alive, try remaining modem resources one by one
    print("=" * 60)
    print("Step 4: Remaining modem resources (one at a time, with crash check)")
    print("=" * 60)
    for res_id, name in sorted(MODEM_RESOURCES.items()):
        if res_id == 0xC8:
            continue  # Already tested
        resp = query(sock, res_id, 0x12, dev=0x00)
        result = decode_response(resp)
        sender = f"(dev=0x{resp['dev']:02x})" if resp else ""
        print(f"  0x{res_id:02x} {name:20s}: {result} {sender}")

        # Check if phone survived
        time.sleep(0.5)
        alive = query(sock, 0x1B, 0x00, data=bytes([0x41]), timeout=2.0)
        if not alive:
            print(f"  >>> PHONE CRASHED on 0x{res_id:02x}! Stopping.")
            sock.close()
            sys.exit(1)
        time.sleep(0.3)
    print()

    # Step 6: Skip PNS_NAME_QUERY_REQ — variable-length payloads may crash phone
    # Only try if Steps 2-5 all succeeded without crashes
    print("=" * 60)
    print("Step 6: Summary")
    print("=" * 60)
    print("  If Steps 2-5 all showed NO RESPONSE for modem resources,")
    print("  the Name Service routing is not available for modem access.")
    print("  UART capture (J2060 test point) remains the primary path.")
    print()

    # Step 7: Try COMMGR subscription query
    print("=" * 60)
    print("Step 7: COMMGR (0x10) version + subscription")
    print("=" * 60)
    resp = query(sock, 0x10, 0x12)
    result = decode_response(resp)
    print(f"  COMMGR version: {result}")

    # Try PNS_SUBSCRIBED_RESOURCES_EXTEND_IND (read)
    resp = query(sock, 0x10, 0x10)  # PNS_SUBSCRIBED_RESOURCES_IND
    if resp:
        print(f"  Subscription query: msg=0x{resp['msg']:02x} {resp['data'].hex()}")
    else:
        print(f"  Subscription query: NO RESPONSE")
    print()

    print("=" * 60)
    print("DONE")
    print("=" * 60)

    sock.close()


if __name__ == "__main__":
    main()
