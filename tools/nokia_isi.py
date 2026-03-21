#!/usr/bin/env python3
"""
nokia_isi.py — Nokia E7 ISI/Phonet protocol tool

Communicates with Nokia E7 over USB using the Phonet/ISI protocol.
Discovers ISI resources, reads phone info, and attempts PM/memory access.

Usage:
    python3 nokia_isi.py discover    # Discover all ISI resources
    python3 nokia_isi.py info        # Read phone info (IMEI, version)
    python3 nokia_isi.py pm <field>  # Read PM field (if resource found)
    python3 nokia_isi.py scan        # Scan all resource IDs 0x00-0xFF

Requires: pyusb, phone in Nokia Suite USB mode
"""

import usb.core
import usb.util
import struct
import sys
import time

# Nokia USB Vendor ID
NOKIA_VID = 0x0421

# Known Nokia E7 PIDs
KNOWN_PIDS = {
    0x0106: "ROM Boot",
    0x01c7: "Nokia Suite (Phonet)",
    0x01c8: "Mass Storage",
    0x0333: "Mass Storage (alt)",
    0x0396: "Mass Storage (MTP)",
    0x03a0: "RNDIS",
    0x03a1: "CDC Phonet",
}

# Phonet protocol constants
PN_DEV_PC = 0x10
PN_DEV_HOST = 0x00
PN_DEV_MODEM = 0x60
PN_DEV_SOS = 0x6C

# ISI Resource IDs (from oFono/libisi)
PN_CALL = 0x01
PN_SMS = 0x02
PN_SS = 0x06
PN_SIM_AUTH = 0x08
PN_SIM = 0x09
PN_NETWORK = 0x0A
PN_COMMGR = 0x10
PN_MTC = 0x15
PN_PHONE_INFO = 0x1B
PN_GPDS = 0x31
PN_GSS = 0x32
PN_FIREWALL = 0x43
PN_GPS = 0x54
PN_EPOC_INFO = 0x62
PN_UICC = 0x8C
PN_WRAN = 0xB4
PN_MODEM_MCE = 0xC2
PN_MODEM_INFO = 0xC5
PN_MODEM_NETWORK = 0xC8
PN_MODEM_CALL = 0xC9
PN_NAMESERVICE = 0xDB

RESOURCE_NAMES = {
    0x01: "PN_CALL", 0x02: "PN_SMS", 0x06: "PN_SS",
    0x08: "PN_SIM_AUTH", 0x09: "PN_SIM", 0x0A: "PN_NETWORK",
    0x10: "PN_COMMGR", 0x15: "PN_MTC", 0x1B: "PN_PHONE_INFO",
    0x31: "PN_GPDS", 0x32: "PN_GSS", 0x43: "PN_FIREWALL",
    0x54: "PN_GPS", 0x62: "PN_EPOC_INFO", 0x8C: "PN_UICC",
    0xB4: "PN_WRAN", 0xC2: "PN_MODEM_MCE", 0xC5: "PN_MODEM_INFO",
    0xC8: "PN_MODEM_NETWORK", 0xC9: "PN_MODEM_CALL",
    0xDB: "PN_NAMESERVICE",
}

# INFO service message IDs
INFO_SERIAL_NUMBER_READ_REQ = 0x00
INFO_SERIAL_NUMBER_READ_RESP = 0x01
INFO_PP_READ_REQ = 0x02
INFO_PP_READ_RESP = 0x03
INFO_VERSION_READ_REQ = 0x07
INFO_VERSION_READ_RESP = 0x08
INFO_PRODUCT_INFO_READ_REQ = 0x15
INFO_PRODUCT_INFO_READ_RESP = 0x16

# INFO subblock types
INFO_SB_SN_IMEI_PLAIN = 0x41
INFO_SB_MCUSW_VERSION = 0x48
INFO_PRODUCT_NAME = 0x01
INFO_PRODUCT_MANUFACTURER = 0x07

# COMMGR message IDs
COMM_ISI_VERSION_GET_REQ = 0x12
COMM_ISI_VERSION_GET_RESP = 0x13

# NAMESERVICE message IDs
PNS_NAME_QUERY_REQ = 0x04
PNS_NAME_QUERY_RESP = 0x05
PNS_SUBSCRIBED_RESOURCES_IND = 0x10


class PhonetUSB:
    """Low-level USB Phonet communication"""

    def __init__(self):
        self.dev = None
        self.ep_in = None
        self.ep_out = None
        self.intf = None

    def find_device(self):
        """Find Nokia phone on USB"""
        devs = list(usb.core.find(find_all=True, idVendor=NOKIA_VID))
        if not devs:
            print("No Nokia device found on USB")
            print("Make sure phone is in Nokia Suite / PC Suite mode:")
            print("  Settings > Connectivity > USB > Nokia Ovi Suite")
            return False

        for dev in devs:
            pid = dev.idProduct
            name = KNOWN_PIDS.get(pid, "Unknown")
            print(f"Found Nokia device: VID={dev.idVendor:04x} PID={pid:04x} ({name})")

            if pid in (0x0333, 0x0396, 0x01c8):
                print("  Phone is in Mass Storage mode - switch to Nokia Suite mode")
                continue

            self.dev = dev
            return True

        if not self.dev and devs:
            # Try the first device anyway
            self.dev = devs[0]
            print(f"  Using first device (PID={self.dev.idProduct:04x})")
            return True

        return False

    def find_phonet_interface(self):
        """Find the Phonet/CDC interface on the device"""
        if not self.dev:
            return False

        print(f"\nDevice configurations: {self.dev.bNumConfigurations}")

        for cfg in self.dev:
            print(f"  Config {cfg.bConfigurationValue}: {cfg.bNumInterfaces} interfaces")
            for intf in cfg:
                cls = intf.bInterfaceClass
                subcls = intf.bInterfaceSubClass
                proto = intf.bInterfaceProtocol
                alt = intf.bAlternateSetting
                num_ep = intf.bNumEndpoints

                # Only log interfaces with endpoints or special subclass
                if num_ep > 0 or subcls in (0xFE, 0xFD, 0x08):
                    print(f"    Interface {intf.bInterfaceNumber} alt={alt}: class={cls:02x} subcls={subcls:02x} proto={proto:02x} eps={num_ep}")

                # Phonet is CDC subclass 0xFE (UsbPnComm)
                # Data interface is the next one (subclass 0x00) with alt setting 1
                # Interface 14 = UsbPnComm (control), Interface 15 alt1 = UsbPnData (data)
                if subcls == 0xFE:
                    print(f"    --> Found Phonet control interface {intf.bInterfaceNumber}")
                    # The data interface is intf.bInterfaceNumber + 1, alt setting 1
                    data_intf_num = intf.bInterfaceNumber + 1
                    # Find the data interface with alt setting 1
                    for intf2 in cfg:
                        if intf2.bInterfaceNumber == data_intf_num and intf2.bAlternateSetting == 1:
                            print(f"    --> Found Phonet data interface {data_intf_num} alt=1")
                            for ep in intf2:
                                direction = "IN" if usb.util.endpoint_direction(ep.bEndpointAddress) == usb.util.ENDPOINT_IN else "OUT"
                                print(f"        EP 0x{ep.bEndpointAddress:02x} ({direction}) maxpkt={ep.wMaxPacketSize}")
                                if direction == "IN":
                                    self.ep_in = ep
                                else:
                                    self.ep_out = ep
                            if self.ep_in and self.ep_out:
                                self.intf = intf2
                                self.ctrl_intf = intf
                                print(f"    --> Phonet ready: IN=0x{self.ep_in.bEndpointAddress:02x} OUT=0x{self.ep_out.bEndpointAddress:02x}")
                                return True

        # Fallback: try LCIF interface (subclass 0xFD)
        for cfg in self.dev:
            for intf in cfg:
                if intf.bInterfaceSubClass == 0xFD:
                    data_intf_num = intf.bInterfaceNumber + 1
                    for intf2 in cfg:
                        if intf2.bInterfaceNumber == data_intf_num and intf2.bAlternateSetting >= 1 and intf2.bNumEndpoints >= 2:
                            for ep in intf2:
                                direction = "IN" if usb.util.endpoint_direction(ep.bEndpointAddress) == usb.util.ENDPOINT_IN else "OUT"
                                if direction == "IN":
                                    self.ep_in = ep
                                else:
                                    self.ep_out = ep
                            if self.ep_in and self.ep_out:
                                self.intf = intf2
                                print(f"    --> Using LCIF interface {data_intf_num}: IN=0x{self.ep_in.bEndpointAddress:02x} OUT=0x{self.ep_out.bEndpointAddress:02x}")
                                return True

        print("No Phonet interface found")
        return False

    def claim(self):
        """Claim the Phonet USB interface and set alt setting"""
        if not self.dev or not self.intf:
            return False

        # Detach kernel drivers from both control and data interfaces
        for intf_num in set([self.intf.bInterfaceNumber,
                           getattr(self, 'ctrl_intf', self.intf).bInterfaceNumber]):
            try:
                if self.dev.is_kernel_driver_active(intf_num):
                    print(f"  Detaching kernel driver from interface {intf_num}")
                    self.dev.detach_kernel_driver(intf_num)
            except (usb.core.USBError, NotImplementedError):
                pass

        try:
            usb.util.claim_interface(self.dev, self.intf)
            # Set alternate setting 1 on data interface (activates endpoints)
            self.dev.set_interface_altsetting(self.intf.bInterfaceNumber,
                                              self.intf.bAlternateSetting)
            print(f"  Interface {self.intf.bInterfaceNumber} claimed, alt={self.intf.bAlternateSetting}")
            return True
        except usb.core.USBError as e:
            print(f"  Claim failed: {e}")
            return False

    def send(self, data):
        """Send raw data to Phonet endpoint"""
        try:
            written = self.ep_out.write(data)
            return written
        except usb.core.USBError as e:
            print(f"  USB write error: {e}")
            return -1

    def recv(self, timeout=5000):
        """Receive data from Phonet endpoint"""
        try:
            data = self.ep_in.read(4096, timeout=timeout)
            return bytes(data)
        except usb.core.USBTimeoutError:
            return None
        except usb.core.USBError as e:
            print(f"  USB read error: {e}")
            return None

    def close(self):
        """Release interface"""
        if self.dev and self.intf:
            try:
                usb.util.release_interface(self.dev, self.intf)
            except:
                pass


class NokiaISI:
    """Nokia ISI protocol client"""

    def __init__(self):
        self.usb = PhonetUSB()
        self.trans_id = 0

    def connect(self):
        """Connect to Nokia phone"""
        if not self.usb.find_device():
            return False
        if not self.usb.find_phonet_interface():
            return False
        if not self.usb.claim():
            return False
        return True

    def _next_trans_id(self):
        self.trans_id = (self.trans_id + 1) & 0xFF
        return self.trans_id

    def send_isi(self, resource, msg_id, data=b'', rdev=PN_DEV_HOST):
        """Send ISI message"""
        tid = self._next_trans_id()
        payload = bytes([tid, msg_id]) + data

        # Phonet header (8 bytes)
        phonet_hdr = struct.pack('>BBBHBB',
            rdev,       # receiver device
            PN_DEV_PC,  # sender device (us)
            resource,   # ISI resource
            len(payload),  # payload length
            0x00,       # receiver object
            0x00,       # sender object
        )

        # CDC Phonet over USB needs a media byte prefix (0x00)
        pkt = b'\x00' + phonet_hdr + payload

        print(f"  TX [{len(pkt)}B]: {pkt.hex()}")
        sent = self.usb.send(pkt)
        return tid if sent > 0 else -1

    def recv_isi(self, timeout=5000):
        """Receive ISI response"""
        data = self.usb.recv(timeout)
        if not data:
            return None

        print(f"  RX [{len(data)}B]: {data[:32].hex()}")

        # CDC Phonet has 1-byte media prefix
        if len(data) < 9:  # 1 media + 7 header + 1 payload minimum
            return None

        # Skip media byte
        offset = 0
        if data[0] == 0x00 or len(data) > 8:
            offset = 1

        # Parse Phonet header
        if len(data) < offset + 7:
            return None
        rdev, sdev, res, length, robj, sobj = struct.unpack('>BBBHBB', data[offset:offset+7])

        # ISI payload
        payload = data[offset+7:]
        if len(payload) < 2:
            return None

        trans_id = payload[0]
        msg_id = payload[1]
        msg_data = payload[2:]

        return {
            'rdev': rdev, 'sdev': sdev, 'resource': res,
            'robj': robj, 'sobj': sobj,
            'trans_id': trans_id, 'msg_id': msg_id,
            'data': msg_data, 'raw': data,
        }

    def recv_isi(self, timeout=5000):
        """Receive ISI response"""
        data = self.usb.recv(timeout)
        if not data or len(data) < 8:
            return None

        # Parse Phonet header
        rdev, sdev, res, length, robj, sobj = struct.unpack('>BBBHBB', data[:7])

        # ISI payload starts at offset 7
        payload = data[7:]
        if len(payload) < 2:
            return None

        trans_id = payload[0]
        msg_id = payload[1]
        msg_data = payload[2:]

        return {
            'rdev': rdev, 'sdev': sdev, 'resource': res,
            'robj': robj, 'sobj': sobj,
            'trans_id': trans_id, 'msg_id': msg_id,
            'data': msg_data, 'raw': data,
        }

    def query(self, resource, msg_id, data=b'', timeout=5000, rdev=PN_DEV_HOST):
        """Send ISI query and wait for response"""
        tid = self.send_isi(resource, msg_id, data, rdev)
        if tid < 0:
            return None

        # Wait for response with matching transaction ID
        deadline = time.time() + timeout / 1000
        while time.time() < deadline:
            resp = self.recv_isi(timeout=1000)
            if resp and resp['trans_id'] == tid:
                return resp
            if resp:
                # Log unexpected messages
                res_name = RESOURCE_NAMES.get(resp['resource'], f"0x{resp['resource']:02x}")
                print(f"  [unexpected] resource={res_name} msg=0x{resp['msg_id']:02x}")

        return None

    def read_imei(self):
        """Read IMEI via PN_PHONE_INFO — try all device targets"""
        targets = [
            (PN_DEV_HOST, PN_PHONE_INFO, "HOST/PHONE_INFO"),
            (PN_DEV_SOS, PN_PHONE_INFO, "SOS/PHONE_INFO"),
            (PN_DEV_MODEM, PN_PHONE_INFO, "MODEM/PHONE_INFO"),
            (PN_DEV_MODEM, PN_MODEM_INFO, "MODEM/MODEM_INFO"),
            (PN_DEV_SOS, PN_MODEM_INFO, "SOS/MODEM_INFO"),
            (0x6C, 0x1B, "0x6C/0x1B"),
        ]
        for rdev, resource, desc in targets:
            print(f"Reading IMEI via {desc}...")
            resp = self.query(resource, INFO_SERIAL_NUMBER_READ_REQ,
                             bytes([INFO_SB_SN_IMEI_PLAIN]), rdev=rdev)
            if resp:
                print(f"  *** GOT RESPONSE via {desc}! ***")
                print(f"  msg=0x{resp['msg_id']:02x} data={resp['data'].hex()}")
                return resp
        print("  No response from any target")
        return None

    def read_version(self):
        """Read software version"""
        print("Reading version...")
        resp = self.query(PN_PHONE_INFO, INFO_VERSION_READ_REQ,
                         bytes([INFO_SB_MCUSW_VERSION]))
        if resp:
            print(f"  Response: msg=0x{resp['msg_id']:02x} data={resp['data'].hex()}")
            return resp
        else:
            print("  No response")
            return None

    def read_product_info(self):
        """Read product name"""
        print("Reading product info...")
        resp = self.query(PN_PHONE_INFO, INFO_PRODUCT_INFO_READ_REQ,
                         bytes([INFO_PRODUCT_NAME]))
        if resp:
            print(f"  Response: msg=0x{resp['msg_id']:02x} data={resp['data'].hex()}")
            return resp
        else:
            print("  No response")
            return None

    def comm_version(self):
        """Query COMMGR ISI version"""
        print("Querying COMMGR version...")
        resp = self.query(PN_COMMGR, COMM_ISI_VERSION_GET_REQ)
        if resp:
            print(f"  Response: msg=0x{resp['msg_id']:02x} data={resp['data'].hex()}")
            return resp
        else:
            print("  No response")
            return None

    def scan_resources(self, start=0x00, end=0xFF):
        """Scan all resource IDs by sending version query to each"""
        print(f"Scanning ISI resources 0x{start:02x}-0x{end:02x}...")
        found = []
        for res in range(start, end + 1):
            # Send COMM_ISI_VERSION_GET_REQ (0x12) to each resource
            tid = self.send_isi(res, COMM_ISI_VERSION_GET_REQ)
            if tid < 0:
                continue

            resp = self.recv_isi(timeout=500)
            if resp:
                name = RESOURCE_NAMES.get(res, "???")
                status = "OK" if resp['msg_id'] == COMM_ISI_VERSION_GET_RESP else f"msg=0x{resp['msg_id']:02x}"
                print(f"  0x{res:02x} ({name:20s}): {status} data={resp['data'][:8].hex()}")
                found.append(res)

            # Don't flood the phone
            time.sleep(0.05)

        print(f"\nFound {len(found)} responding resources:")
        for res in found:
            name = RESOURCE_NAMES.get(res, "UNKNOWN")
            print(f"  0x{res:02x} = {name}")
        return found

    def close(self):
        self.usb.close()


def main():
    if len(sys.argv) < 2:
        print(__doc__)
        sys.exit(1)

    cmd = sys.argv[1]

    # Add PID to known list
    KNOWN_PIDS[0x0335] = "Nokia Suite (CDC)"

    isi = NokiaISI()
    if not isi.connect():
        sys.exit(1)

    try:
        if cmd == "info":
            isi.comm_version()
            isi.read_imei()
            isi.read_version()
            isi.read_product_info()

        elif cmd == "discover" or cmd == "scan":
            found = isi.scan_resources()
            # Save results
            with open("isi_resources.txt", "w") as f:
                for res in found:
                    name = RESOURCE_NAMES.get(res, "UNKNOWN")
                    f.write(f"0x{res:02x} = {name}\n")
            print(f"\nSaved to isi_resources.txt")

        elif cmd == "listen":
            print("Listening for any data from phone (10 seconds)...")
            for _ in range(20):
                data = isi.usb.recv(timeout=500)
                if data:
                    print(f"  RECV [{len(data)}B]: {data.hex()}")

        elif cmd == "raw":
            # Send a raw packet and see what happens
            print("Sending raw probe packets...")
            # Try sending just the media byte
            isi.usb.send(b'\x00')
            time.sleep(0.5)
            data = isi.usb.recv(timeout=2000)
            if data:
                print(f"  Got response to empty: {data.hex()}")

            # Try nameservice ping with different framings
            for prefix in [b'', b'\x00', b'\x1b']:
                pkt = prefix + bytes([0x6C, 0x10, 0xDB, 0x00, 0x04, 0x00, 0x00, 0x01, 0x12])
                print(f"  Sending: {pkt.hex()}")
                isi.usb.send(pkt)
                time.sleep(0.3)
                data = isi.usb.recv(timeout=2000)
                if data:
                    print(f"  RESPONSE: {data.hex()}")

        elif cmd == "pm" and len(sys.argv) > 2:
            field = int(sys.argv[2])
            print(f"PM field {field} read not yet implemented")
            print("Need to discover PM resource ID first (run 'discover')")

        else:
            print(f"Unknown command: {cmd}")
            print(__doc__)

    finally:
        isi.close()


if __name__ == "__main__":
    main()
