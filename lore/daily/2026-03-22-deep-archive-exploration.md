# 2026-03-22: Deep Archive Exploration (Autonomous Session)

## Session Goal
User authorized 5hr autonomous exploration budget. 6 parallel research
agents analyzed all available source archives and documentation.

## Sources Analyzed
1. **modemadaptation** Symbian source — ISI routing, Name Service, USB Phonet bridge
2. **kernelhwsrv** Symbian source — AllocShadowPage, CopyToShadowMemory, ModifyCode
3. **Forum archives** — 134,865 files, HaRET/kexec/DPlatChunkHw discussions
4. **Mailing list** — 27,330 files, PADCONF/BeagleBoard/OMAP3 discussions
5. **Phoenix DLLs** — tcsclient.dll, ta_security_impl.dll, tucsonclient.dll
6. **Linux kernel** — OMAP3630 pad config, N950/N9 pinctrl, Phonet stack

## Key Findings

### 1. ISI Routing Discovery (BREAKTHROUGH)
The ISI Name Service (0xDB) does dynamic resource→device routing:
- Messages to device 0x00 with modem resource IDs get translated
- `isinameservice.cpp:194-200` rewrites ISI header receiver device
- Our previous approach (rdev=0x60 directly) bypassed this translation
- New tool `isi_nameservice_probe.py` tests the correct routing path
- **May enable modem access without any security bypass**

### 2. N950 Pinctrl Reference
`omap3-n950-n9.dtsi` has 35 pad declarations for the same OMAP3630:
- SSI modem (8), eMMC (6), UART2/BT (4), accel (2), WLAN (2), modem GPIO (3)
- Not directly usable for E7 (different peripherals) but good reference
- UART capture still required for E7-specific pads

### 3. HaRET Dead End on Symbian
Forum expert confirmed: HaRET relies on WinCE kernel mode request.
No equivalent on Symbian without manufacturer-signed code.
Shadow pages are ephemeral. No one has booted Linux from Symbian.

### 4. BB5 Auth = Smartcard Hardware
SuperDongle uses FPS10 smartcard protocol. Keys on physical card.
ISI firewall is hardware-detected at boot. No software bypass.

### 5. RomPatcher Limits
CopyToShadowMemory: max 32 bytes per call. Shadow pages read-only on
Cortex-A8. Data patches only (caps, SID, VID), not code injection.
DebugSupport::ModifyCode exists but needs debug capability.

### 6. "Firewall" Task Reframed
Task 084 (defeat ISI firewall) closed — resource 0x43 is PN_NVD.
The real blocker is ISI routing, not a firewall.
New Task 087 created for Name Service routing test.

## Files Created
- `docs/isi-protocol/routing-analysis.md` — Full routing architecture
- `docs/research-findings-2026-03-22.md` — Consolidated findings
- `tools/isi_nameservice_probe.py` — Name Service routing test tool
- `tasks/087-O-isi-nameservice-modem-routing.md` — New task
- Updated `tasks/084-P-defeat-isi-firewall.md` — Status: Closed/Reframed

## Revised Priority Stack
1. **UART capture** (J2060) — unblocks everything, ~2hr hardware setup
2. **ISI Name Service probe** (Task 087) — zero risk software test
3. **RomPatcher RE** — if we need supervisor mode for HaRET or SecEnv patch
4. **HaRET Linux boot** — blocked by supervisor mode access

## Next Steps When User Returns
1. Run `isi_nameservice_probe.py` on the phone — immediate test
2. Order USB-UART adapter + 1.8V level shifter for UART capture
3. If Name Service routing works → try PM read via PN_MODEM_PERM
