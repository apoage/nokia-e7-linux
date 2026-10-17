# Task 089: Network Bridge — Remote Script Execution

## Status
**Implemented, pending device session** (2026-08-20)

- Phone side: `symbian-sdk/memread/pys60/bridge_phone.py` — PyS60 1.4.5 /
  Python 2.5 compatible AND runs unmodified on host Python 3 (loopback
  tested: ping/info/exec/eval/push/pull/ls/rm/peek-graceful-error all pass)
- PC side: `tools/bridge_pc.py` (one-shot + interactive REPL)
- Protocol: line commands; `DATA <n>` status lines carry raw payloads,
  `OK B64` carries base64 stdout (kept distinct so `EVAL 6*7` → `OK 42`
  can't be mistaken for a payload length)
- Deploy/run steps: task 091 phase 2

## Original Problem
Current workflow requires USB mass storage mount/unmount cycle (~40s each)
for every script deploy + result read. This is:
- Time-consuming (40s mount + copy + unmount + run + remount + read = 2+ min per iteration)
- Error-prone (filesystem corruption on crashes)
- Physically annoying (cable reconnect, mode switching)
- Blocking (phone can't run scripts while mounted as storage)

## Goal
Deploy a persistent agent on the phone that accepts commands over network,
executes Python code, and returns results — no USB remount needed.

## Available Network Capabilities

### WiFi TCP/IP (BEST OPTION)
- `AF_INET = 2048`, `SOCK_STREAM = 1`, `IPPROTO_TCP = 6`
- Phone has WiFi enabled (user confirmed)
- PyS60 `socket` module available
- `_wlantools.pyd` and `_wlanmgmt.pyd` for WLAN management
- `_iapconnect.pyd` for Internet Access Point connection
- `e32socket.pyd` for enhanced socket operations
- Process has `NetworkServices` capability (confirmed)
- **Latency:** <10ms on local network
- **Bandwidth:** 54Mbps (WiFi 802.11g)
- **Setup:** phone connects to same WiFi as PC

### Bluetooth RFCOMM (BACKUP)
- `BTPROTO_RFCOMM = 3`, `RFCOMM = 101`
- BT hardware present but currently disabled (HAL returned KErrNotSupported)
- `_misty.local_bt_address()` and `local_bt_name()` available
- Would need BT pairing + RFCOMM serial channel
- **Latency:** ~50ms
- **Bandwidth:** 2Mbps
- **Setup:** BT pairing required

### USB CDC (ALREADY WORKING)
- ISI/Phonet protocol already working over USB CDC
- `cdc_phonet` kernel module on PC
- Could piggyback data transfer on Phonet protocol
- But requires Nokia Suite USB mode (not mass storage)
- **Latency:** <5ms
- **Bandwidth:** 12Mbps (USB 2.0 Full Speed)
- **Complication:** shares USB with ISI, needs multiplexing

## Recommended Architecture

### WiFi TCP Bridge (Recommended)

```
PC (Linux)                         Phone (Symbian)
┌──────────────┐                  ┌──────────────────┐
│ bridge_pc.py │◄── TCP/WiFi ──►│ bridge_phone.py  │
│              │    port 4444     │                  │
│ - send script│                  │ - listen on 4444 │
│ - recv result│                  │ - exec received  │
│ - file push  │                  │ - return output  │
│ - file pull  │                  │ - auto-reconnect │
└──────────────┘                  └──────────────────┘
```

#### Phone Side: bridge_phone.py
- Runs in Python on phone, stays resident
- Listens on TCP port 4444 (or configurable)
- Accepts connections from PC
- Protocol:
  - `EXEC <python code>` → execute code, return stdout
  - `PUSH <path> <size>` → receive file, write to phone
  - `PULL <path>` → read file, send to PC
  - `PEEK <addr>` → read byte from address (via _hack.peek)
  - `PEEK32 <addr>` → read 32-bit word
  - `POKE <addr> <val>` → write byte
  - `PING` → `PONG`
- All responses prefixed with length
- Auto-reconnect on connection drop
- Keep-alive heartbeat

#### PC Side: bridge_pc.py
- Connects to phone's IP:4444
- Interactive REPL or script mode
- Commands:
  - `exec <code>` — run Python on phone
  - `push <local> <remote>` — upload file
  - `pull <remote> <local>` — download file
  - `peek <addr>` — read memory
  - `scan <start> <end> <target>` — ROM scan
  - `script <file.py>` — upload and execute script
  - `gpfsel` — shortcut to read GPIO registers

### Connection Setup
1. Phone connects to WiFi (already done)
2. Find phone's IP: `import socket; socket.gethostbyname(socket.gethostname())`
   or check router DHCP leases
3. Run `bridge_phone.py` on phone (one-time via USB)
4. Run `bridge_pc.py` on PC with phone's IP
5. All subsequent interaction over WiFi — no more USB mount cycles

## Implementation Notes

### PyS60 Socket Quirks
- `AF_INET = 2048` (not standard 2, Symbian-specific!)
- May need `_iapconnect` to establish WiFi data connection first
- Socket binding might need explicit IAP (Internet Access Point) selection
- `e32socket.pyd` has `access_point()` for selecting WiFi AP

### Security
- Bridge only listens on WiFi (local network)
- No authentication (trusted LAN assumption)
- Could add simple shared secret if needed

### Reliability
- Phone WiFi may disconnect on screen lock
- Use `misty.reset_inactivity_time()` to prevent auto-lock
- TCP keepalive for connection monitoring
- Auto-reconnect on both sides

### Crash Recovery
- If phone-side script crashes, bridge dies
- Need to manually restart via Python app on phone
- Could write a watchdog .py that auto-restarts bridge
- Or use `e32.start_exe` to launch bridge as separate process

## What This Unblocks

- **Rapid iteration:** test peek addresses in <1 second (vs 2+ minutes)
- **ROM scan:** run from PC, resume automatically, no manual reruns
- **GPFSEL read:** if we find the right address, read it instantly
- **Exploit development:** test poke modifications with immediate feedback
- **Bulk data transfer:** dump ROM sections without USB mount

## Files to Create
- `symbian-sdk/memread/pys60/bridge_phone.py` — phone-side TCP server
- `tools/bridge_pc.py` — PC-side client
- `tools/bridge_scan.py` — networked ROM scanner
- `tools/bridge_gpfsel.py` — networked GPFSEL reader

## Dependencies
- WiFi connected on phone (DONE)
- Phone IP address (need to discover)
- Python app running on phone (manual start)

## Alternative: Simple HTTP Server
Even simpler than custom protocol:
- Phone runs `BaseHTTPServer` equivalent
- PC uses `curl` or `wget`
- `/exec?code=...` endpoint
- `/peek?addr=...` endpoint
- `/pull?path=...` endpoint
- BUT: PyS60 might not have BaseHTTPServer
- Raw TCP socket is more reliable

## Alternative: File-based Bridge via WiFi
If TCP server is too complex:
- Phone mounts a shared folder via WiFi (SMB/FTP)
- PC writes script to shared folder
- Phone watches folder, executes new scripts
- Results written to output files
- PC reads results from shared folder
- Simpler but higher latency
