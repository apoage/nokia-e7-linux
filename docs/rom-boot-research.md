# Nokia E7 / BB5 ROM Boot Mode Research

Research date: 2026-03-09

## 1. USB Device Identity

When the Nokia E7 briefly appears as "Nokia USB ROM" in dmesg, it is using:
- **VID**: `0x0421` (Nokia Corporation)
- **PID**: `0x0106` (Nokia USB ROM / USB Flashing)
- **USB Class**: `0xFF/0xFF/0xFF` (vendor-specific protocol)

This is distinct from the BB5 ADL Loader mode (PID `0x0105`), which is the
normal flashing mode after the secondary bootloader has loaded.

The transition you saw: ROM boot (0x0106) -> secondary bootloader loads ->
ADL mode (0x0105) -> normal Symbian boot.

## 2. ROM Boot USB Protocol

> **Note (2026-03-23):** This section was written assuming RAPUYAMA = OMAP3630.
> Real HW CPUID = ARM1176JZF-S (ARMv6), likely Broadcom BCM2763. The ROM boot
> protocol may differ from TI's. See docs/critical-cpu-discovery.md.

The Nokia E7 uses the RAPUYAMA (D1800 package) application processor. The ROM
code implements a peripheral boot protocol over USB (originally assumed to be
TI's standard OMAP3630 protocol — needs re-evaluation).

### Protocol Sequence

1. **Device enumerates on USB** as VID `0x0451` (TI) PID `0xd00e` (OMAP3630)
   on raw OMAP boards. Nokia overrides this to VID `0x0421` PID `0x0106`.

2. **ASIC ID sent automatically** (OMAP3 behavior -- unlike OMAP4 which waits
   for a "Send ASIC ID" command first):
   - Total: **69 bytes over USB** (58 bytes over UART, no checksum on UART)
   - Structure (byte offsets):
     - `[0]`: Items/num subblocks
     - `[1-7]`: ID subblock (device ID info, OMAP chip revision at byte 7)
     - `[8-11]`: Secure mode subblock header
     - `[12-34]`: 2nd ID subblock header
     - `[35-57]`: Root key hash subblock header
     - `[58-68]`: Checksum subblock (USB only, 4-byte CRC)

3. **Host must read ASIC ID within 300ms** or the OMAP disconnects from USB.

4. **Host sends 4-byte command**: `0xF0030002` = "continue booting through USB"
   (other values exist for UART boot, etc.)

5. **Host sends 4-byte length** of the secondary bootloader binary to transfer.

6. **Host sends the secondary bootloader binary** via USB bulk transfer.
   The ROM loads it into OMAP internal SRAM and jumps to it.

### USB Endpoints
- Bulk OUT (host -> device): typically endpoint 0x01
- Bulk IN (device -> host): typically endpoint 0x81

### Key Constraint
USB peripheral boot is **only effective at power-on reset**, not warm/cold
software reset. The OMAP ROM checks a tracing vector bit to determine reset
type.

## 3. How Nokia E7 Enters ROM Boot Mode

### Normal boot order (from SYS_BOOT pins)
The OMAP3630 SYS_BOOT[5:0] pins determine boot device search order. On the
Nokia E7, the typical order is:
1. OneNAND (primary)
2. USB (fallback, if SYS_BOOT5 enables peripheral boot)

### When ROM boot activates
The ROM code tries NAND first. It reads the first sector; if bad/corrupt/blank,
tries up to 4 sectors. If ALL fail, it falls through to the next boot device
in the SYS_BOOT list. If USB peripheral boot is in the list, the OMAP
enumerates on USB as the ROM boot device.

### What you saw
The brief "Nokia USB ROM" appearance suggests:
- The OMAP ROM tried NAND, found valid NLoader/SWBL
- Started loading the secondary bootloader from NAND
- The secondary bootloader (NLoader) then re-enumerated USB with a different
  device descriptor, or the ROM USB device was only briefly visible during the
  transition

**Alternative**: The E7 may have USB in its SYS_BOOT fallback list, and the
ROM briefly enumerates on USB while simultaneously trying NAND. Once NAND
succeeds, USB is dropped.

## 4. BB5 Dead Phone USB Flashing (Phoenix Method)

This is the standard Nokia service procedure for "dead" phones:

### Procedure
1. Install Phoenix Service Software (v2012.04.xxx recommended for E7)
2. Extract firmware files into `C:\Program Files\Nokia\Phoenix\Products\RM-626\`
3. Open Phoenix, select product RM-626
4. Set connection to **"NO CONNECTION"**
5. Go to Flashing > Firmware Update
6. Check **"Dead Phone USB Flashing"** checkbox
7. Click **Refurbish**
8. Phoenix will say "Waiting for device..."
9. Connect the **powered-off** phone via USB cable
10. Press power button until backlight comes up (for non-dead phones)
    or just connect USB (for truly dead phones)
11. Phoenix detects the ROM boot device and proceeds

### What Phoenix does internally
- Waits for USB device VID `0x0421` PID `0x0106`
- Reads ASIC ID from the ROM bootloader
- Downloads CMT 2nd bootloader (`rap3gv2_2nd.fg`) and APE 2nd bootloader
  (`helen3_2nd.fg`) to the phone's SRAM
- These secondary bootloaders then handle the actual flash programming
- The protocol involves patching the APE boot in multiple steps (step1-3)

### Key Nokia BB5 files involved
- `rap3gv2_2nd.fg` - CMT (modem) secondary bootloader
- `helen3_2nd.fg` - APE (application processor / OMAP) secondary bootloader
- `rap3gv3_2nd.fg` - newer variant for later BB5 phones

## 5. Holding ROM Mode Active

### The 300ms problem
The OMAP3630 ROM code gives the USB host only **300ms to read the ASIC ID**
after USB enumeration. If no host reads it in time, the ROM disconnects and
moves to the next boot device (or hangs).

### Strategies to stay in ROM mode

1. **Have a USB host ready and listening** before powering on the phone.
   Use `lsusb` polling, udev rules, or a tool like `omap_loader` that
   blocks waiting for the device.

2. **Erase/corrupt NAND** - If the first 4 NAND blocks are all blank or
   corrupt, the ROM will try USB boot (if in SYS_BOOT list). The ROM stays
   in USB mode waiting for the host to complete the protocol. However, this
   requires already having write access to NAND, which is a chicken-and-egg
   problem.

3. **Force SYS_BOOT pins** - On dev boards, you can pull SYS_BOOT pins to
   force USB-first boot order. On the Nokia E7, these pins are hardwired on
   the PCB. Potentially modifiable with soldering, but extremely risky on
   irreplaceable hardware.

4. **Use Phoenix/flasher tools** - These tools are designed to catch the
   brief ROM boot window. Phoenix monitors USB for the device and immediately
   engages when it sees VID:PID 0421:0106.

### Does ROM mode wait indefinitely?
Once the ASIC ID is successfully read by the host and the `0xF0030002`
command is sent, the ROM code enters a transfer loop waiting for the
secondary bootloader binary. **This stage does not have a tight timeout** --
the ROM will wait for the USB bulk transfer to complete.

So the key is: catch the ASIC ID within 300ms, send the continue command,
and then you have control.

## 6. Available Tools

### Open source
- **0xFFFF** (https://github.com/pali/0xFFFF) - Open Free Fiasco Firmware
  Flasher. Supports cold flashing for N900, N950, N9. Source code in
  `src/cold-flash.c` documents the full USB ROM boot protocol.
  - Key file: `cold-flash.c` - ASIC ID parsing, USB bulk transfers
  - Supports: Nokia 770, N800, N810, N900, N950, N9
  - **Does NOT explicitly list E7** but the OMAP3630 ROM protocol is identical

- **omap_loader** (https://github.com/grant-h/omap_loader) - USB BootROM
  uploading utility for TI OMAP3. Pure ASIC ID + binary upload, no Nokia
  firmware layer. Could potentially upload a custom binary to E7's SRAM.

- **omap3_usbload** - Original OMAP3 USB boot utility by Martin Mueller.
  Predecessor to omap_loader.

- **omapboot** (https://github.com/kousu/omapboot) - Python implementation
  for OMAP4 but protocol is similar. Good reference for understanding the
  protocol in a readable language.

- **flasher-3.5** - Nokia's official Maemo flasher tool. Not open source but
  well-documented. Supports cold flashing N900.

### Proprietary
- **Phoenix Service Software** - Nokia's official service tool. Supports
  dead phone USB flashing for E7.
- **JAF (Just Another Flasher)** - Third-party BB5 flasher. ASIC ID reading,
  flash operations. Limited memory dump capabilities.
- **Infinity BEST** (BB5 Easy Service Tool) - Flash reading, RPL operations,
  content extraction. Can read flash up to 255MB.
- **ATF Box** - Advanced Turbo Flasher. Forensic capabilities.
- **Cyclone Box** - Another BB5 service tool.

## 7. BB5 Security and Memory Access

### Security levels
BB5 phones have multiple security levels:
- **SL1/SL2**: Older security, more tools can interact
- **SL3**: Modern security (Nokia E7 uses this). Certificate-based
  authentication required for many operations.

### ROM bootloader capabilities
The OMAP3630 ROM code itself is minimal:
- Reads ASIC ID (hardware identification)
- Accepts a secondary bootloader binary over USB
- Loads it to SRAM and executes it
- **Does NOT have arbitrary memory read commands** in the standard protocol
- The ROM is designed to WRITE (download) only, not READ back

### Memory access in practice
- **ROM mode**: Can only download a binary to SRAM. No read-back.
- **Secondary bootloader mode**: Depends on the bootloader. Nokia's 2nd
  bootloaders (rap3gv2_2nd.fg, helen3_2nd.fg) implement Nokia's proprietary
  flash protocol with authentication.
- **Custom secondary bootloader**: If you can get a custom binary to execute
  via ROM boot USB, it has full access to the OMAP3630 memory map (SRAM,
  SDRAM after init, all peripherals). This is the key opportunity.
- **Forensic tools** (Infinity BEST, Cellebrite): Can dump NAND flash contents
  but may not get spare area. Limited by BB5 security on SL3 devices.

### The opportunity: custom SRAM payload
If the E7's SYS_BOOT pins include USB peripheral boot as a fallback:
1. Corrupt/erase NAND first 4 blocks (requires existing access)
2. Or find a test point to force USB boot
3. ROM boot activates, enumerates USB
4. Use omap_loader to upload a custom ARM binary to OMAP3630 SRAM
5. This binary runs with full hardware access -- can:
   - Initialize SDRAM
   - Read NAND contents
   - Dump pad mux register configuration
   - Read any memory-mapped peripheral
   - Send data back over USB

## 8. Test Points and Hardware Methods

### BSI line
Nokia phones use the BSI (Battery Size Indicator) line on the battery
connector. The BSI resistance tells the phone the battery type. Grounding
BSI or presenting specific resistances can affect boot behavior on some
Nokia models.

### General BB5 test point approach
Some BB5 phones have undocumented test points on the PCB that can force
boot mode when shorted to ground during power-on. These are model-specific
and not publicly documented for the E7.

### E7-specific considerations
- The E7 has a non-removable battery (internal), making battery-pull
  procedures harder
- The micro-USB port is the primary service interface
- The service manual (in `docs/service-manual/`) may show relevant test points
- The schematic (in `docs/schematics/`) shows SYS_BOOT pin connections

## 9. Recommended Action Plan

### Immediate (no risk to hardware)

1. **Set up USB monitoring**: Create a udev rule or script that logs ALL USB
   device appearances with full descriptors when the E7 is connected:
   ```bash
   # Monitor USB events
   udevadm monitor --udev --subsystem-match=usb
   # In parallel, watch dmesg
   dmesg -w | grep -i nokia
   ```

2. **Try connecting while powered off**: Connect USB cable to PC first,
   start monitoring, then connect to powered-off E7. See if ROM boot
   device appears.

3. **Install 0xFFFF**: `apt install 0xffff` or build from source. Try:
   ```bash
   0xFFFF -d  # device mode, waits for Nokia USB device
   0xFFFF -I  # identify/inquiry mode
   ```

4. **Try omap_loader**: Build from source, run it before connecting phone.
   It will wait for OMAP3630 USB ROM device and attempt ASIC ID read.

### Medium risk

5. **Phoenix in VM**: Set up Windows VM with USB passthrough, install
   Phoenix 2012.04.xxx, try dead phone USB flashing procedure to confirm
   the E7 can be reached via ROM boot.

6. **UART capture**: Connect to E7's debug UART (if accessible) during boot.
   NLoader prints pad mux configuration over UART -- this is the #1 priority
   from MEMORY.md.

### Higher risk (Unit A only)

7. **Examine PCB test points**: With the service manual, identify any test
   points near SYS_BOOT pins or boot mode selection.

8. **Force USB boot**: If SYS_BOOT pins can be identified, temporarily
   pulling the right pin could force USB-first boot order.

## 10. Key References

- 0xFFFF source: https://github.com/pali/0xFFFF (especially src/cold-flash.c)
- omap_loader: https://github.com/grant-h/omap_loader
- OMAP35x/AM37x Initialization: http://processors.wiki.ti.com/index.php/OMAP35x_and_AM/DM37x_Initialization
- Doug Brown OMAP USB boot analysis: https://www.downtowndougbrown.com/2025/11/debugging-beagleboard-usb-boot-with-a-sniffer-fixing-omap_loader-on-modern-pcs/
- Maemo cold flashing wiki: https://wiki.maemo.org/Updating_the_firmware/Cold_Flashing
- BB5 platform wiki: https://lpcwiki.miraheze.org/wiki/BB5_platform
- Nokia flashing wiki: https://lpcwiki.miraheze.org/wiki/Flashing_or_updating_phones/Nokia
- CPKB dead phone flashing: http://www.cpkb.org/wiki/Nokia_dead_phone_USB_flashing_with_Phoenix_Service_Software
- Phoenix flashing tutorial: http://n8delight.blogspot.com/2013/07/how-to-flash-phoenix-flashing-tutorial.html
- OMAPpedia Bootloader Project: https://omappedia.com/wiki/Bootloader_Project
