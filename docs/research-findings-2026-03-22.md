# Research Findings — 2026-03-22 Deep Archive Exploration

> **Note (2026-03-23):** Real HW is BCM2727B1 (ARM1176), not OMAP3630. See docs/critical-cpu-discovery.md

## Overview
6 parallel research agents explored modemadaptation source, forum archives,
mailing lists, Phoenix DLLs, Symbian kernel source, and Linux kernel source.

## Finding 1: ISI Routing Architecture (CRITICAL)

**Source**: modemadaptation Symbian source code

The ISI router uses a **Name Service** (resource 0xDB) for dynamic device routing:
- Messages to device 0x00 with modem resource IDs get intercepted by the Name Service
- Name Service rewrites the ISI header to route to device 0x60 via shared memory
- Our previous scans set rdev=0x60 directly, bypassing this translation

**Implication**: We should try sending messages to rdev=0x00 with modem resources.
The Name Service may route them to the modem if the modem has registered.

**Detail**: `isinameservice.cpp:194-200` — `LookupPhonetAddress(resource, &phoNetAddress)`
rewrites receiver device and object in the ISI header.

**See**: `docs/isi-protocol/routing-analysis.md`

## Finding 2: N950 Pad Configuration (Useful Reference)

**Source**: Linux kernel `arch/arm/boot/dts/ti/omap/omap3-n950-n9.dtsi`

The Nokia N950 (same OMAP3630) has 35 pad declarations in upstream Linux:
- SSI modem interface: 8 pads
- eMMC (SDMMC2): 6 pads
- UART2 (Bluetooth): 4 pads
- Accelerometer INT: 2 pads
- WLAN enable/IRQ: 2 pads
- Modem control GPIOs: 3 pads (ape_rst_rq, cmt_rst_rq, cmt_apeslpx, cmt_en)

**Cannot substitute for E7 PADCONF** because:
- Different flash memory (E7=OneNAND/GPMC, N950=eMMC/SDMMC2)
- Different modem interface routing
- Different sensor/peripheral GPIO assignments
- Different display/camera interfaces

**Useful for**: Common peripherals (I2C, PMIC, basic GPIOs).

## Finding 3: HaRET Cannot Work on Symbian

**Source**: Symbian Developer Forum archives

Forum expert confirmed: "HaRET only works on old, insecure version of Windows CE
which allow any process to ask to run in kernel mode. To do something similar on
Symbian you would need to have your program signed with all capabilities."

- No one has booted Linux from Symbian user space
- Shadow ROM pages are ephemeral (kernel can discard after process exits)
- No standard privilege escalation path on Symbian^3
- TCB is ROM-only — cannot be granted at runtime

**Conclusion**: HaRET approach requires a manufacturer-signed kernel driver
(like RomPatcher's patcher.ldd) for supervisor mode access.

## Finding 4: BB5 Auth is Smartcard-Based

**Source**: Phoenix DLL analysis (tcsclient.dll, ta_security_impl.dll)

- `CSuperDongleAuthentication` uses FPS10 smartcard protocol
- Keys stored on physical smartcard (OPKEYS/OPKEYS2), not in DLLs
- ISI firewall auth level is hardware-detected at boot (DK2 dongle on LPT)
- No software bypass: firewall is a read-only status reporter
- Phoenix uses XML `<challengeData>` for application-level BB5 auth
- OpenSSL 0.9.8d (3DES, RSA) for encryption, keys on smartcard

**Conclusion**: Cannot emulate SuperDongle without the physical smartcard.

## Finding 5: RomPatcher Shadow Memory Limits

**Source**: Symbian kernel source (kernelhwsrv)

- `Epoc::CopyToShadowMemory` — max **32 bytes per call**
- Shadow pages on Cortex-A8 are **initially read-only** (newer ARM platforms)
- FShell `sudo` uses shadow memory to patch ROM exe capabilities (data only)
- Code injection via shadow pages is limited by 32B chunks and read-only constraint
- `DebugSupport::ModifyCode()` exists for breakpoint insertion (code patching)
  but requires initialization and debug capability

**Conclusion**: RomPatcher can patch data (caps, SID, VID) but not inject
arbitrary executable code through the standard shadow memory API.

## Finding 6: OMAP3 PADCONF Register Layout

**Source**: Linux kernel source

- CORE1 PADCONF: 0x48002030, 0x238 bytes (284 pads)
- WKUP PADCONF: 0x48002A00, 0x5C bytes (46 pads)
- CORE2 PADCONF (OMAP3630 only): 0x480025A0
- Each pad: 16-bit register with MUX_MODE[2:0], PULL_ENA[3], PULL_UP[4], INPUT_EN[8]
- pinctrl-single driver: `compatible = "ti,omap3-padconf"`
- E7 DTS pinctrl sections are empty — waiting for UART capture

## Revised Priority Assessment

Based on these findings:

### Path A: UART Capture (HIGHEST PRIORITY)
- NLoader prints PADCONF values at boot
- J2060 test pad on bottom board
- ~2 hours setup + 15 min capture
- Gives us PADCONF AND modem resource registration log
- **Unblocks everything**

### Path B: ISI Name Service Routing (TRY NEXT)
- New tool: `tools/isi_nameservice_probe.py`
- Send modem resource queries via device 0x00
- If modem has registered, Name Service routes to it
- Zero risk, just software

### Path C: RomPatcher Privilege Escalation (RESEARCH)
- Need manufacturer-signed patcher.ldd (already on phone)
- Could patch SecEnv to disable ISI auth check
- Or patch ROM exe capabilities to grant TCB
- Requires deep RomPatcher RE

### Path D: HaRET Linux Boot (BLOCKED)
- Needs supervisor mode → needs manufacturer-signed LDD
- Same blocker as Path C
- If we solve Path C, this becomes viable
