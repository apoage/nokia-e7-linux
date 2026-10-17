# Task 091: Device Session — Exe Fix Verification + Bridge Bring-up

## Status
**EXECUTED 2026-08-20/21 — major milestones landed**

- Phase 1 DONE: test_A + test_B installed and RAN (exe problem solved;
  recipe in task 090). test_C dropped (0xE SID blocked install).
- Phase 2 DONE: bridge over WiFi operational (agent + bridge_pc + hub);
  iteration is now seconds. PyS60 core is Python 2.2.2 — agent v6+
  handles it.
- Phase 3 SUPERSEDED: all runtime kernel-memory routes closed by the
  LDD caps wall (task 088 update). XPEEK/xscan built instead for safe
  user-space probing; address space mapped.
- Phase 4 partial: HAL dump captured (docs/phone-probes/
  hal-dump-2026-08-21.txt).

Success criteria met for phases 1-2. Phase 3's goal (GPFSEL) moved to
static ROM analysis + VideoCore/NLoader shapes.

## Pre-session checklist (PC side, before connecting)

- [ ] `ls symbian-sdk/memread/test_A.exe test_B.exe test_C.exe
      test_variants.sisx` — all present (built by build_variants.sh)
- [ ] Phone WiFi connected to the same LAN as PC (already configured)
- [ ] USB cable ready

## Phase 1: Exe matrix (Task 090 verification) — 10 min

1. Connect USB, mass-storage mode
2. Copy to E:\ root:
   - `symbian-sdk/memread/test_variants.sisx`
   - `symbian-sdk/memread/pys60/run_variants.py`
   - `symbian-sdk/memread/pys60/bridge_phone.py` (deploy now, used in phase 2)
3. Disconnect mass storage
4. Install `test_variants.sisx` from phone file manager (expect "untrusted
   application" warning — accept). Note any install error code.
5. Open Python shell → run `run_variants.py`
6. RECORD the exact output (start_exe return codes per variant + marker
   file status). Expected outcomes:
   - A runs, C fails → **test-range SID confirmed as root cause**, exe
     execution unblocked, proceed to phase 2
   - A fails identically to C → SID theory wrong; record errors, next
     suspect is SWI trust state (retest A copied bare to E:\sys\bin vs
     SIS-installed)
   - B runs, A fails → deflate handling issue in our elf2e32 path (unlikely,
     RP+ is deflate too)

## Phase 2: WiFi bridge bring-up (Task 089) — 10 min

1. On phone Python shell: run `bridge_phone.py` (it blocks — that's the
   agent running; keep Python in foreground, keep phone unlocked)
2. Get the phone IP (from the phone: `import socket;
   socket.gethostbyname(socket.gethostname())`, or router DHCP list)
3. On PC:
   - `python3 tools/bridge_pc.py <phone_ip> ping`
   - `python3 tools/bridge_pc.py <phone_ip> info` (check `hack=True` —
     if False, `_hack.pyd` isn't on this Python's path; note it)
   - `python3 tools/bridge_pc.py <phone_ip> exec "import e32;
     print(e32.drive_list())"`
4. From here NO MORE mass-storage cycles: iterate via
   `bridge_pc.py script <file.py>` (push+exec+cleanup in one shot)

## Phase 3: GPFSEL attack queue (Task 088) — 15+ min

Run in priority order (each is a script pushed via the bridge; crash of the
Python process only kills that EXEC, the agent survives — worst case restart
the agent):

1. **2D. Negative peek** (5 min): `_hack.peek` with values ≥ 0x80000000 /
   negative ints — checks for signed-int truncation reaching kernel VA.
   Target read: anything in 0xC8004000..0xC8004160 returns GPFSEL bytes.
2. **2B. Framebuffer pivot** (10 min): enumerate mapped regions by probing
   readable addresses around known user-mapped buffers (dipro/audio shared
   chunks); look for adjacent kernel-window mappings.
3. **1C. HAL variant group**: `UserSvr::HalFunction`-equivalent via misty
   redirect (documented in task 088).
4. **1A. RomPatcher LDD channel**: needs `patcher_probe.exe` rebuilt with
   uid 0xA000xxxx + same eexe/usrt link — build takes 2 min once phase 1
   confirms the exe fix (run `build_variants.sh` pattern with
   patcher_probe.cpp).
5. Record EVERYTHING: `bridge_pc.py exec "..." > session-log.txt` per probe.

## Phase 4 (if time): Linux-port data harvest

Cheap wins via bridge EXEC while connected:
- `sysinfo` battery/display/clock data for DTS constraints
- Full HAL attribute dump (all groups) → hardware properties table
- e32 drive/volume info → storage map
- Any peek-able peripheral registers discovered in phase 3

## Success criteria

- Phase 1: at least one custom exe runs on the phone (first time ever this
  project) → unlocks native tools, network bridge daemon, LDD probing
- Phase 2: remote exec works → iteration cost drops from ~2 min to ~1 s
- Phase 3: 28 GPFSEL bytes → real-hardware device tree (the last missing
  input for the Linux bring-up)
