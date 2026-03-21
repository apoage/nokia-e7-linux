#!/usr/bin/env python3
"""
isi_scan.py — Hurt Me Plenty ISI resource scanner

Scans ALL 256 ISI resource IDs on the Nokia E7.
Crash-resilient: logs each attempt to file, auto-resumes after reboot.
Waits for phone to reconnect if disconnected.

Usage:
    sudo python3 isi_scan.py          # Start/resume scan
    sudo python3 isi_scan.py reset    # Reset scan to start from 0
"""

import socket, struct, ctypes, ctypes.util, os, select, fcntl, time, sys, subprocess

AF_PHONET = 35
SCAN_LOG = "/home/lukas/ps3/e7/docs/isi-scan-results.txt"
SCAN_IDX = "/tmp/isi_scan_idx.txt"

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
    with open(SCAN_LOG, 'a') as f:
        f.write(msg + '\n')

def get_scan_idx():
    try:
        return int(open(SCAN_IDX).read().strip())
    except:
        return 0

def set_scan_idx(idx):
    open(SCAN_IDX, 'w').write(str(idx))

def wait_for_phone():
    """Wait for Nokia phone to appear on USB and set up Phonet"""
    while True:
        result = subprocess.run(['lsusb', '-d', '0421:'], capture_output=True, text=True)
        if '0421:' in result.stdout:
            print("Phone detected on USB")
            time.sleep(3)  # Give it time to enumerate

            # Find and bind cdc_phonet
            for bus_dev in ['3-2', '3-1', '1-2', '1-6']:
                intf = f"{bus_dev}:1.14"
                driver_path = f"/sys/bus/usb/devices/{intf}/driver"
                if os.path.exists(f"/sys/bus/usb/devices/{intf}"):
                    if not os.path.exists(driver_path):
                        try:
                            with open('/sys/bus/usb/drivers/cdc_phonet/bind', 'w') as f:
                                f.write(intf)
                            print(f"  Bound cdc_phonet to {intf}")
                        except:
                            pass
                    else:
                        print(f"  cdc_phonet already bound at {intf}")

                    # Wait for usbpn0
                    for _ in range(10):
                        if os.path.exists('/sys/class/net/usbpn0'):
                            break
                        time.sleep(0.5)

                    if os.path.exists('/sys/class/net/usbpn0'):
                        return True

        print("Waiting for phone...", end='\r')
        time.sleep(2)

def setup_phonet():
    """Set up Phonet interface and address"""
    subprocess.run(["ip", "address", "flush", "dev", "usbpn0"], capture_output=True)
    subprocess.run(["ip", "link", "set", "usbpn0", "down"], capture_output=True)
    subprocess.run(["ip", "link", "set", "usbpn0", "up"], capture_output=True)

    # Get ifindex
    tmpsock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    ifr = struct.pack('256s', b'usbpn0')
    result = fcntl.ioctl(tmpsock.fileno(), 0x8933, ifr)
    ifindex = struct.unpack('16sI', result[:20])[1]
    tmpsock.close()

    # Add Phonet address via netlink
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

    if err == 0 or err == -17:
        return True
    print(f"  Address add failed: {err}")
    return False

def create_socket():
    """Create and bind Phonet socket"""
    s = socket.socket(AF_PHONET, socket.SOCK_DGRAM, 1)
    s.setsockopt(socket.SOL_SOCKET, socket.SO_BINDTODEVICE, b'usbpn0\x00')
    phonet_bind(s, dev=0x10, obj=0x01)
    return s

def probe_resource(sock, res_id):
    """Probe a single ISI resource. Returns response or None."""
    # Send COMM_ISI_VERSION_GET_REQ (0x12)
    msg = bytes([res_id & 0xFF, 0x12])
    ret = phonet_sendto(sock, msg, dev=0x00, obj=0x00, resource=res_id)
    if ret <= 0:
        return "SEND_FAIL"

    result = phonet_recv(sock, timeout=1.0)
    if not result:
        return None

    data, addr = result
    if len(data) < 2:
        return f"SHORT:{data.hex()}"

    if data[1] == 0xF0:
        # Error response
        sub = data[2] if len(data) > 2 else 0
        if sub == 0x14:
            return "NOT_REACHABLE"
        elif sub == 0x17:
            return "NOT_AUTHENTICATED"
        else:
            return f"ERROR:0xF0/0x{sub:02x}"
    elif data[1] == 0x13:
        # ISI version response!
        ver = data[2:4].hex() if len(data) >= 4 else "??"
        return f"VERSION:{ver} [{len(data)}B] {data.hex()}"
    else:
        return f"RESPONSE:msg=0x{data[1]:02x} [{len(data)}B] {data.hex()}"

KNOWN = {
    0x01:"CALL", 0x02:"SMS", 0x06:"SS", 0x08:"SIM_AUTH", 0x09:"SIM",
    0x0A:"NETWORK", 0x0E:"???", 0x10:"COMMGR", 0x15:"MTC",
    0x1B:"PHONE_INFO", 0x31:"GPDS", 0x32:"GSS", 0x43:"FIREWALL",
    0x54:"GPS", 0x62:"EPOC_INFO", 0x8C:"UICC", 0xB4:"WRAN",
    0xC2:"MODEM_MCE", 0xC5:"MODEM_INFO", 0xC8:"MODEM_NET",
    0xC9:"MODEM_CALL", 0xDB:"NAMESERVICE",
}

def main():
    if len(sys.argv) > 1 and sys.argv[1] == 'reset':
        set_scan_idx(0)
        try: os.remove(SCAN_LOG)
        except: pass
        print("Scan reset to 0")
        return

    start_idx = get_scan_idx()
    if start_idx >= 256:
        print("Scan already complete! Use 'reset' to start over.")
        print(f"Results in: {SCAN_LOG}")
        return

    log(f"\n=== ISI Resource Scan starting at 0x{start_idx:02x} ({time.strftime('%Y-%m-%d %H:%M:%S')}) ===")

    while start_idx < 256:
        # Wait for phone
        if not os.path.exists('/sys/class/net/usbpn0'):
            print("\nusbpn0 gone — waiting for phone reconnect...")
            wait_for_phone()

        # Setup
        if not setup_phonet():
            time.sleep(2)
            continue

        try:
            sock = create_socket()
        except Exception as e:
            print(f"Socket error: {e}, retrying...")
            time.sleep(2)
            continue

        # Verify connection with IMEI read first
        phonet_sendto(sock, bytes([0xFF, 0x00, 0x41]), dev=0x00, resource=0x1B)
        result = phonet_recv(sock, timeout=3.0)
        if not result or result[0][1] == 0xF0:
            print("Phone not responding to IMEI — retrying setup...")
            sock.close()
            time.sleep(2)
            continue

        print(f"Phone alive! Starting scan from 0x{start_idx:02x}\n")

        # Scan resources
        consecutive_fails = 0
        crashed = False
        for res in range(start_idx, 256):
            name = KNOWN.get(res, "")

            # Try up to 3 attempts per resource
            best_result = "SEND_FAIL"
            for attempt in range(3):
                result = probe_resource(sock, res)

                if result == "SEND_FAIL":
                    consecutive_fails += 1
                    if consecutive_fails >= 3:
                        # Phone crashed — need to reconnect
                        log(f"  0x{res:02x} ({name:15s}): CRASH detected (attempt {attempt+1})")
                        try: sock.close()
                        except: pass

                        print(f"  Phone crashed around 0x{res:02x}, waiting 45s for reboot...")
                        time.sleep(45)

                        # Reconnect
                        print("  Reconnecting...")
                        if not os.path.exists('/sys/class/net/usbpn0'):
                            wait_for_phone()
                        if not setup_phonet():
                            time.sleep(5)
                            setup_phonet()
                        try:
                            sock = create_socket()
                        except:
                            time.sleep(5)
                            sock = create_socket()

                        # Verify phone is alive
                        phonet_sendto(sock, bytes([0xFF, 0x00, 0x41]), dev=0x00, resource=0x1B)
                        alive = phonet_recv(sock, timeout=3.0)
                        if alive:
                            print("  Phone alive again!")
                            consecutive_fails = 0
                            continue  # Retry this resource
                        else:
                            print("  Phone not responding after reconnect, waiting more...")
                            time.sleep(30)
                            continue
                    else:
                        time.sleep(1)
                        continue  # Quick retry
                else:
                    # Got a response (even if error) — phone is alive
                    consecutive_fails = 0
                    best_result = result
                    break

            # Log the best result we got
            if best_result == "SEND_FAIL":
                log(f"  0x{res:02x} ({name:15s}): CRASH (all 3 attempts failed)")
            elif best_result is None or best_result == "NOT_REACHABLE":
                pass  # Not interesting
            else:
                log(f"  0x{res:02x} ({name:15s}): {best_result}")

            # Write index AFTER successful probe
            set_scan_idx(res + 1)

            # Delay between probes
            time.sleep(0.3)

        if res >= 255:
            start_idx = 256  # Completed full scan

        try:
            sock.close()
        except:
            pass

    log(f"\n=== Scan COMPLETE ({time.strftime('%Y-%m-%d %H:%M:%S')}) ===")
    print(f"\nResults saved to: {SCAN_LOG}")

if __name__ == "__main__":
    main()
