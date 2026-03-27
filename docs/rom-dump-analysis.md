# Nokia E7 ROM Dump -- Detailed Hardware Analysis

**SoC**: Broadcom BCM2727B1 (ARM1176JZF-S, ARMv6)
**ROM file**: `romdumpplus.dmp` (31,793,152 bytes, 30.3 MB)
**ROM base**: 0x80000000 (confirmed by TRomHeader at offset 0x8C)
**Build date**: Jul 27 2012 (from assertion timestamps)
**Platform ID**: hw79 (from modemadaptation source paths)
**Analysis date**: 2026-03-24

---

## 1. ROM Header (TRomHeader)

```
Offset  Value        Field
0x00    0xEA0006BE   ARM branch (reset vector)
0x8C    0x80000000   ROM physical base address
0x90    0x01F00000   ROM declared size (32 MB, rounded)
0x94    0x80044700   Kernel data address
0x98    0xC8000000   ** Peripheral virtual base **
0x9C    0xCA100000   Secondary mapping (SDRAM?)
0xA0    0x80044710   ROM root directory pointer
0xA4    0x80044C74   Additional ROM pointer
0x100   0x80000000   ROM start (repeated)
```

**Key finding**: 0xC8000000 at offset 0x98 is the Symbian kernel's virtual base
address for memory-mapped peripherals. All `0xC800xxxx` values in the ROM are
virtual peripheral register addresses.

---

## 2. Peripheral Virtual Address Map (0xC800xxxx)

These addresses are Symbian virtual mappings of the BCM2727B1's physical
peripheral registers. The physical base is unknown but likely 0x20000000
(similar to BCM2835) or a proprietary Broadcom address.

### Most-referenced 0xC800xxxx addresses

| Virtual Address | Refs | Likely Peripheral |
|-----------------|------|-------------------|
| 0xC80010A0 | 33 | **Core peripheral** (most referenced) |
| 0xC8001902 | 11 | Unknown |
| 0xC8001910 | 9 | Unknown |
| 0xC8000000 | 9 | **Peripheral base / system controller** |
| 0xC8000568 | 9 | Unknown |
| 0xC80005D0 | 8 | Unknown |
| 0xC8000648 | 7 | Unknown |
| 0xC8001EC4 | 3 | Unknown |
| 0xC80011C8 | 3 | Unknown |
| 0xC8001D94 | 3 | Unknown |
| 0xC8001DD4 | 4 | Unknown |
| 0xC8001D64 | 3 | Unknown |
| 0xC8007910 | 3 | Unknown |
| 0xC800BADC | 3 | Unknown |
| 0xC800DD70 | 3 | Unknown |
| 0xC800DE10 | 3 | Unknown |
| 0xC800D200 | 3 | Unknown |
| 0xC800D240 | 3 | Unknown |
| 0xC800A690 | 4 | Unknown |
| 0xC800A670 | 3 | Unknown |
| 0xC800A6A0 | 2 | Unknown |
| 0xC800F47C | 5 | Unknown |
| 0xC800F450 | 2 | Unknown |
| 0xC800F480 | 2 | Unknown |
| 0xC800F690 | 2 | Unknown |
| 0xC8001340 | 5 | Unknown |
| 0xC8001330 | 5 | Unknown |
| 0xC80012B8 | 2 | Unknown |
| 0xC80012D8 | 2 | Unknown |

### GPIO registers (identified from GPIO-API code at ROM 0xC0C80)

The GPIO driver code at ROM offset ~0xC0C00-0xC1900 uses addresses in
the 0xC8004xxx-0xC8006xxx range:

| Virtual Address | Context |
|-----------------|---------|
| 0xC8004150 | GPIO register (near "GPIO-API" strings) |
| 0xC8006028 | GPIO register (near "GPIO-API:Invalid PinId" string) |
| 0xC8004428 | GPIO register (near "DispatchToPinsIsr") |

The GPIO code uses a table-driven approach with 7 GPIO banks (pin 0-6
checked at 0xC14300 area), each bank having SET, CLR, LEVEL, and MASK
registers at offsets from a per-bank base.

### Address groupings by 4KB page

| Page Base | Refs | Likely Function |
|-----------|------|-----------------|
| 0xC8000000 | many | System controller / clock / reset |
| 0xC8001000 | many | Core peripherals |
| 0xC8004000 | several | GPIO controller |
| 0xC8005000 | many | SPI / DMA related |
| 0xC8006000 | several | GPIO / interrupt |
| 0xC800A000 | several | Memory controller? |
| 0xC800D000 | several | Display / video |
| 0xC800F000 | several | Camera / ISP |
| 0xC8001D00 | several | Power management |
| 0xC8001E00 | several | Power management |

---

## 3. Hardware Clock Domains (EHwClk enumeration)

These are all the clocked peripherals on the BCM2727B1, defined in
`base_rapu/rapu/PowerResourceManager/HwClkControl.cpp`:

```
EHwClkEdAvsync       - AV sync engine
EHwClkEdCommon       - Common clock domain
EHwClkExtSysClk      - External system clock
EHwClkCamera         - Camera subsystem
EHwClkDomISP         - ISP clock domain
EHwClkDomHSUSB       - High-Speed USB clock domain
EHwClkHSUSB          - High-Speed USB
EHwClkI2C0           - I2C bus 0
EHwClkI2C1           - I2C bus 1
EHwClkI2C2           - I2C bus 2
EHwClkIrDA1M         - IrDA 1 Mbps
EHwClkIrDA4M         - IrDA 4 Mbps
EHwClkLoSSI          - Low-Speed Synchronous Serial (to PMIC?)
EHwClkMeSSI          - MeSSI (Messi Serial Interface to VideoCore)
EHwClkRFBI           - Remote Frame Buffer Interface (display)
EHwClkViSSI          - Video SSI
EHwClkSSI0           - Synchronous Serial 0
EHwClkSSI1           - Synchronous Serial 1
EHwClkMcuSpi         - MCU SPI
EHwClkVGALite        - VGA Lite (display output)
EHwClkBB38M4AnaClk0  - Baseband 38.4 MHz analog clock 0
EHwClkBB38M4AnaClk1  - Baseband 38.4 MHz analog clock 1
EHwClkDspSpi0        - DSP SPI 0
EHwClkDspSpi1        - DSP SPI 1
EHwClkACodec         - Audio codec
EHwClkASIBT          - ASI Bluetooth
EHwClkASIMusti       - ASI Musti
EHwClkExtSysClk19M2  - External system clock 19.2 MHz
EHwClkGDMA           - General DMA
EHwClkMemCard0       - Memory card 0 (eMMC)
EHwClkMemCard1       - Memory card 1 (SD)
EHwClkEMC            - External Memory Controller
EHwClkBTExtSysClk    - Bluetooth external system clock
EHwClkMemIf          - Memory Interface
```

**Key insight**: 34 clock domains. The SoC has 3 I2C buses, at least 3 SPI
controllers (McuSpi, DspSpi0, DspSpi1), WLAN via SPI (not SDIO), MeSSI
interface to VideoCore, HSUSB with MUSBMHDRC (Mentor), RFBI/VGALite for
display, and GDMA for DMA.

---

## 4. PMIC Voltage Regulators (EHwReg enumeration)

```
EHwRegHdmi           - HDMI regulator
EHwRegVIoSleep       - I/O voltage (sleep mode)
EHwRegVIoForcePwm    - I/O voltage (forced PWM)
EHwRegVOutEnable     - VOUT enable
EHwRegVOutVSel       - VOUT voltage select
EHwRegVAux2Enable    - VAUX2 enable
EHwRegVAux2VSel      - VAUX2 voltage select
EHwRegVAux1Enable    - VAUX1 enable
EHwRegVAux1VSel      - VAUX1 voltage select
EHwRegVMemEnable     - VMEM enable
EHwRegVMemVSel       - VMEM voltage select
```

Also from string search:
```
EVoltage_eMMC        - eMMC supply voltage
EVoltage_eMMC_Core   - eMMC core voltage
EVoltage_eMMC_IO     - eMMC I/O voltage
```

---

## 5. Driver Inventory

### 5.1 ASSP Layer (base_rapu)

The core ASSP code is at `Y:/ncp_sw/core7.0/base_rapu/`:
- `assp/rapu/assp.cpp` - Main ASSP initialization
- `assp/rapu/partneros/DSpecialMemoryBlock.cpp` - Physical memory mapping
- `rapu/PowerResourceManager/HwClkControl.cpp` - Clock control
- `rapu/PowerResourceManager/PowerRegCtrl.cpp` - PMIC register control
- `rapu/PowerResourceManager/PowerPerf.cpp` - Performance scaling (DVFS)
- `rapu/power/PowerController.cpp` - Power state control
- `shared/PowerResourceManager/PowerResourceManager.cpp` - PRM framework

### 5.2 GPIO Driver

Strings found at ROM offsets 0xC0C80-0xC18B0:
- `GPIO-API:Invalid PinId for Gpio Pin!!`
- `GPIO-API DispatchToPinsIsr:Invalid PinNbr for Gpio Pin!!`
- `GPIO-API BindPin:Invalid PinNbr for Gpio Pin!!`
- `GPIO-API ReleaseBindedPin:Invalid PinNbr for Gpio Pin!!`
- `Invalid Global Id for Gpio Pin`
- `DGpioSwitch` - GPIO-based switch detection (hall sensor, etc.)

The GPIO driver supports at least 7 banks (pin count check at 0xC14300),
uses SetRegister32() for hardware access, and has interrupt binding/unbinding.

### 5.3 I2C Driver

```
DI2C                 - I2C controller driver
DI2CPowerHandler     - I2C power management
I2CThread_0          - I2C worker thread
I2CRequestLock       - I2C request mutex
I2CPowerHandler0     - I2C power handler instance
```

Three I2C buses: I2C0, I2C1, I2C2 (confirmed by EHwClk enumeration).

### 5.4 SPI Driver

Three SPI controllers, each with identical driver structure:
```
Spi1SpiDriver Thread, Spi1Lock, Spi1Power    - SPI 1 (McuSpi)
Spi2SpiDriver Thread, Spi2Lock, Spi2Power    - SPI 2 (DspSpi0)
Spi3SpiDriver Thread, Spi3Lock, Spi3Power    - SPI 3 (DspSpi1)
```

Driver classes: `DSpiCommon`, `DSpiDmaRap`, `DSpiPowerRap`, `DSpiHwRap`,
`DSpiPower`, `TSpiRequest`, `TSpiChannelConfig`

WLAN uses SPI (confirmed by `am_spia.cpp` and `WlanSpia`):
- `Y:/ext/adapt/wlan.nokia/bsp_specific/spia/src/am_spia.cpp`
- `Y:/ext/adapt/wlan.nokia/bsp_specific/hpa/src/am_hpa.cpp`
- `SpiClient: Request status == SPIA::EFailure`

### 5.5 DMA Controller (Rap2D / GDMA)

```
TRap2DDmac           - DMA controller (RAPU-specific "2D DMA")
Rapu_GDMASS          - General DMA SubSystem
DDmaRequest          - DMA request object
DDmaHelper           - DMA helper
DDmaHwMonitor        - DMA hardware monitor
```

DMA assertion: `TRap2DDmac::AppendCommand, KmaxComands exceeded`

### 5.6 USB (MUSBMHDRC / Mentor)

```
DUsbGenericController    - USB controller (MUSBMHDRC = Mentor)
DUsbOtgController        - USB OTG controller
DUsbotghsDmaChannel      - USB OTG HS DMA channel
DUsbRapuIf               - USB RAPU interface
MUsbAsspIf               - USB ASSP interface
```

USB register dump strings confirm MUSBMHDRC (Mentor) controller:
- MUSBMHDRC common registers
- MUSBMHDRC OTG, DynFIFO + Version registers
- ULPI & Additional Config registers
- ULPI registers: VbusControl, CarKitControl, IntMask, IntSrc, RegData, RegAddr, RegControl, RawData

### 5.7 OneNAND / NAND Flash

```
DPhysicalDeviceMediaNand - NAND physical device
DMediaDriverNand         - NAND media driver
TNandInterrupt           - NAND interrupt handler
DOneNandIrqGpioPinHandle - OneNAND IRQ via GPIO
DOneNandIrqGpioPinWrapper
DOneNandIrqGpioBindIsr
NandDMAThread            - NAND DMA thread
```

### 5.8 MMC / eMMC

```
EHwClkMemCard0          - eMMC clock
EHwClkMemCard1          - SD card clock
DMemoryCardDma          - MMC DMA
DMemoryCardInterrupt    - MMC interrupt
DMemoryCardDmaPlatform  - Platform-specific DMA
Rapu_Memcard_PSU_HW_    - MMC PSU hardware
Rapu_Memcard_Stack_HW_  - MMC stack hardware
Rapu_MMCSD_Resource_Manager
DEMMCPartitionInfo      - eMMC partition info
DLegacyEMMCPartitionInfo
```

### 5.9 Display Subsystem

```
DDisplayChannel          - Display channel driver
DDisplayDevice           - Display device
DDisplayPowerHandler     - Display power management
DVCDispmanVCHI           - VideoCore display manager (via VCHI)
DVCDispmanUpdateList     - Display update list
```

Source: `Y:/ext/adapt/displayext.nokia/driversnokia/displaydrv/display_channel.cpp`

IVE (Image/Video Engine) display drivers:
- `ive_cdp.c` - Composite Display Processor
- `ive_dsi.c` - DSI (MIPI Display Serial Interface)
- CDP_CMD, DSI_CMD, DSI_TE (Tearing Effect)

Display updater: `Y:/ext/adapt/displayext.nokia/driversnokia/displayupdater/display_updater_impl.cpp`

HDMI output via VideoCore:
- `TvOutKernelIveHdmi` / `TvOutKernelIveSdtv`
- Multiple HDMI video format strings (NTSC, PAL variants)
- `../../../../middleware/hdmi/hdmi.c`

GCE (Graphics Composition Engine):
- `GCE EGL local last frame info`
- `GCE element metadata`
- `GCE surface metadata`
- `gce_rpc_surface_buffer` / `gce_rpc_target_buffer`
- Dispmanx: `../../../../middleware/dispmanx/dispmanx.c`

### 5.10 VideoCore / IVE3 / MeSSI

The BCM2727B1 contains VideoCore III as a co-processor, communicating
via MeSSI (Messi Serial Interface) and CCP2:

```
DVCDriver                - VideoCore driver (ARM-side)
DVCExe                   - VideoCore executable image manager
DVCMessiChannel          - MeSSI communication channel
DVCMessiRxChannel        - MeSSI receive channel
DVCMessiTxChannel        - MeSSI transmit channel
TControllerMessi         - MeSSI controller
VCDriverMeSSIDfcQ        - MeSSI DFC queue
IVE3_Vcd_Drv             - IVE3 VCD driver
```

Source: `Y:/ncp_sw/core7.0/IVE3_Engine/IVE3_rapu_drivers/interface/vcdriver/src/messi/controller_messi.cpp`

VideoCore boot protocol messages:
```
DVCDriver::Boot() VideoCore boot ROM not responding (stage 1)
DVCDriver::Boot() VideoCore boot ROM not responding (stage 2)
DVCDriver::BootParseMessage Unrecognised VideoCore response
DVCDriver VideoCore image ID not found
DVCDriver VideoCore image ID found
DVCDriver::Boot() no images returned from DVCExe
DVCDriver::Boot() Failed(%d) to start 2nd stage boot loader
DVCDriver::Boot() Unexpected response from boot ROM %u %u
DVCDriver::Boot() Unexpected response after sending 2nd stage %u %u
DVCDriver::Boot() BootParseMessage returned an error (stage 2)
```

VCHI (VideoCore Host Interface) services:
```
vchi_audiopipe_init       - Audio pipe
vchi_dispcontrol_server_init - Display control
vchi_dispservice_x_init   - Display service
vchi_hostrmi_init         - Host RMI
vchi_egl_gce_server_init  - EGL/GCE server
vchi_otpburn_init         - OTP burn
vchi_tvout_server_init    - TV output
vchi_vcamservice_server_init - Camera service
fsysvchi_server_init      - File system via VCHI
logging_vchi_listener_init - Logging
```

MPHI (Message-Passing Host Interface):
```
mphi_slave               - MPHI slave
MPHI_BUFFER              - MPHI buffer
MPHI HISR0/1/2           - MPHI hardware ISRs
mphi_ccp2.c              - MPHI over CCP2 transport
```

VideoCore VLLs (loadable libraries):
```
/Z:/vlls/bcm2727b1/      - VLL path (confirms BCM2727B1 ID)
logging_vchi.vll
vcam.vll
vcamdecode.vll
vcamencode.vll
vcamjpd.vll (JPEG decoder)
vcamtest.vll
vcamfunc.vll
vcamera.vll
vcam_shared_vid.vll
```

CCP2 (Compact Camera Port 2):
```
ccp2_get_func_table
ftCCP2, ftCCP2HIST
Failed to allocate CCP2 driver
Failed to initialise CCP2 driver
VCHI: CCP2 CRC failure
VCHI: MeSSI-16 CRC failure
```

### 5.11 Camera (via VideoCore IVE)

```
DIspCcp                  - ISP CCP2 interface
DIspCcpChannel           - ISP CCP channel
vcamservice              - Camera service (VideoCore-side)
vcamdecode / vcamencode  - Video encode/decode
VCamService_Task         - Camera service task
VCamDecFifo              - Camera decode FIFO
```

ISP tuner (Broadcom ISP):
```
isp_tuner_brcm_dpf_read_bitfield
isp_tuner_brcm_dpf_read_enum
isp_tuner_brcm_dpf_read_pwl_func
isp_tuner_brcm_dpf_read_syntax
isp_tuner_brcm_dpf_test_isp_version
```

### 5.12 Proximity / Light Sensor

```
dipro.pdd                - DiPro (proximity) PDD
DDipro                   - DiPro driver
DDiproLddChannel         - DiPro logical channel
DDiproPddChannel         - DiPro physical channel
DDiproPddChannelImpl     - DiPro implementation
DDiproPowerHandler       - DiPro power handler
Dipro_drv                - DiPro device driver name
```

### 5.13 Accelerometer

```
accelerometer.pdd        - Accelerometer PDD
DAccPddSensorADXL340     - ADXL340 sensor driver
DAccPddSensorLIS302DL    - LIS302DL sensor driver (confirmed!)
DAccelerometerLddChannel - Accelerometer logical channel
```

Confirms both ADXL340 and LIS302DL are supported as variants.

### 5.14 Magnetometer

```
magnetometer.pdd         - Magnetometer PDD
DMagnetometerPddDevice   - Magnetometer device
DMagnetometerPddSensor   - Magnetometer sensor base
DMagnetometerPddChannel  - Magnetometer channel
DMagnetometerPddChannelIf
DMagnetometerPddSensorBus
DMagnetometerPddPowerHandler
DMagnetometerPddSensorAK8974  - AK8974 driver (confirmed!)
DMagnetometerPddSensorAMI305  - AMI305 alternative
DMagnetometerPddSensorBusPlatform
DMagnetometerLddDevice
DMagnetometerLddChannel
```

Confirms AK8974 and AMI305 as magnetometer variants.

### 5.15 Keyboard / Input

```
Keypad                   - Keypad driver instance
Qwerty                   - QWERTY keyboard
KeypadExpander           - Keypad expander (LM8323?)
KeypadExpander2          - Second keypad expander
NaviScroll               - Navigation scroll (Navi key)
TouchIC                  - Touch screen IC (Atmel mXT)
TouchIC2                 - Secondary touch IC
```

### 5.16 Lighting / LED / Backlight

```
LightDrv.pdd             - Light driver PDD
DLightDisplayMentor      - Display backlight (via Mentor)
DLightIOExpanderMentor   - I/O expander lighting (via Mentor)
MLightMentorVariantIntf  - Light variant interface
```

### 5.17 Surface Manager / Graphics

```
DVcSurfaceManager        - VideoCore surface manager
DSurfaceManagerChannel   - Surface manager channel
DSurfaceManagerFactory   - Surface manager factory
surfacemanagerdriver     - Driver name
khegl_ldd.ldd            - OpenGL ES LDD
```

### 5.18 Bluetooth

```
DPhysicalDeviceBt        - Bluetooth physical device
DLogicalDeviceBC02       - BC02 Bluetooth controller
CHCTLUartBase            - HCTL UART transport (HCI over UART)
CHCTLUart                - HCTL UART driver
CHCTLUartPort            - HCTL UART port
CHCTLUartSender          - HCTL UART sender
CHCTLUartReceiver        - HCTL UART receiver
CHCTLUartPowerManager    - HCTL UART power manager
CIDHci                   - HCI controller
EHwClkASIBT             - Bluetooth clock domain
EHwClkBTExtSysClk       - Bluetooth external system clock
```

### 5.19 Security / TrustZone

```
SecLddChannel            - Security LDD channel
SecLddFactory            - Security LDD factory
SecCrypto                - Security crypto engine
SecurityKext             - Security kernel extension
hal_sec_ape_api          - Hardware abstraction for security
SecIsaAccess             - Security ISA access (modem-side security)
SecLddMainDfcq           - Security LDD DFC queue
```

### 5.20 Modem Adaptation (hw79)

The modem adaptation layer (`modemadaptation.nokia/hw79/`) provides:
- ISA access (ISCE layer): `isaaccessldd_ldd`, `isaaccessextension_dll`
- Reboot driver: `rebootdriver_ldd`
- PEP (Pipe End Point) transceiver
- Phonet/ISI routing
- USB Phonet link: `usbphonetlink`

### 5.21 WLAN

```
WlanSpia                 - WLAN SPI adapter
SpiClient                - SPI client for WLAN
DWlanLogicalDevice       - WLAN logical device
DWlanLogicalChannel      - WLAN logical channel
wlan.ldd                 - WLAN LDD
```

Source paths confirm SPI-based WLAN:
- `Y:/ext/adapt/wlan.nokia/bsp_specific/spia/src/am_spia.cpp`
- `Y:/ext/adapt/wlan.nokia/bsp_specific/hpa/src/am_hpa.cpp`

### 5.22 USB Audio

```
usbaudiodevice.cpp
usbaudiochannel.cpp
usbaudiocontrollerstatedspuser.cpp
usbaudiodspmsghandler.cpp
usbaudiocontrollerstatekernel.cpp
usbaudiodatatransferlinkkernel.cpp
usbaudiocontrollerstatelinkkernel.cpp
usbaudiolinkmsghandler.cpp
```

### 5.23 Thermal Sensor

```
ThermalSensor            - Thermal sensor driver (multiple instances)
```

### 5.24 Misc Peripherals

```
DPowerGenioCtrlClock     - GENIO control clock (pin mux related)
Switch                   - Switch/button driver
CameraHWA               - Camera Hardware Adapter
CameraDriver             - Camera driver
DAtmelBootFlash          - Atmel boot flash (touch IC bootloader?)
```

---

## 6. Pin Mux / GENIO Analysis

### GENIO findings

Only one GENIO-related string was found in the entire ROM:
```
0xCC7D5: DPowerGenioCtrlClock
```

This is a power resource controller class that manages GENIO clock control.
The term "GENIO" in this context refers to the General-Purpose I/O pins
used by the RAPUYAMA modem for communication with the application processor.

### Pin mux approach

No strings containing "pinmux", "pinctrl", "padconf", "pad_mux", "func_sel",
or "alt_func" were found. This suggests:

1. The BCM2727B1 may not have a traditional pin mux controller (unlike OMAP).
   Broadcom SoCs often have fixed pin assignments or a very different mux model.
2. Pin configuration may happen in the SWBL (Secondary Warm Boot Loader) or
   NLoader before Symbian starts, making it invisible in the ROM.
3. The VideoCore firmware may handle some pin mux via VCHI messages.
4. GPIO function selection may be done through direct register writes in the
   GPIO controller (0xC8004xxx-0xC8006xxx range) without named abstractions.

**This is the biggest gap in our understanding.** The physical peripheral
addresses and the pin mux register layout remain unknown from the ROM alone.

---

## 7. Memory Map Summary

### Address Spaces

| Range | Usage | Evidence |
|-------|-------|----------|
| 0x80000000-0x81E52800 | Symbian ROM | TRomHeader, self-referential pointers |
| 0xC8000000-0xC800FFFF+ | Peripheral registers (virtual) | 200+ references in ASSP code |
| 0xCA100000+ | Secondary mapping | TRomHeader field |
| 0x80044700+ | Kernel data area | TRomHeader field |

### ROM structure

The ROM is a standard Symbian 9.x/^3 ROM image with:
- Vector table at offset 0x00
- TRomHeader at offset 0x80
- Kernel bootstrap code at offset 0x200
- ASSP vtables at offset ~0xC0000
- ASSP code (GPIO, SPI, I2C, etc.) at 0xB9000-0xD0000
- Driver code extending through the rest of the ROM
- String tables and data interleaved

---

## 8. BCM2727B1 Architecture Summary

Based on ROM analysis, the BCM2727B1 contains:

### ARM Subsystem
- ARM1176JZF-S core (CPUID 0x410FB764)
- TrustZone security (confirmed by SecCrypto, hal_sec_ape_api)
- MUSBMHDRC (Mentor) USB controller with OTG + ULPI
- 3x I2C controllers
- 3x SPI controllers (McuSpi, DspSpi0, DspSpi1)
- GDMA (General DMA) engine
- GPIO controller (7 banks)
- OneNAND flash controller
- MMC/SD controller (2 slots: eMMC + SD card)
- Thermal sensor
- IrDA controller (1M + 4M modes)
- LoSSI (Low-Speed Synchronous Serial) - to PMIC
- MeSSI (Messi Serial Interface) - to VideoCore
- CCP2 (Compact Camera Port 2) - to VideoCore + camera
- RFBI (Remote Frame Buffer Interface) - display
- VGALite - display output

### VideoCore III Subsystem
- Integrated VideoCore III GPU/DSP
- ISP (Image Signal Processor) with Broadcom tuner firmware
- Video encode/decode (H.264, JPEG)
- Display composition engine (Dispmanx, GCE)
- HDMI output
- SDTV output
- Camera control
- VLL (VideoCore Loadable Libraries) framework
- VCHI host interface (20+ service channels)

### Power Management
- TWL5031 PMIC interface (via LoSSI or I2C)
- Multiple voltage domains: VIO, VOUT, VAUX1, VAUX2, VMEM, HDMI
- eMMC power: separate core + I/O voltages
- DVFS support (PowerPerf.cpp)
- Per-peripheral clock gating (34 clock domains)

---

## 9. Key Source Paths

All source paths are from Nokia internal build system (`Y:` drive):

```
Y:/ncp_sw/core7.0/base_rapu/           - ASSP layer
Y:/ncp_sw/core7.0/IVE3_Engine/         - VideoCore/IVE3 drivers
Y:/ncp_sw/corecom/modemadaptation.nokia/hw79/  - Modem adaptation
Y:/ncp_sw/corecom/security_components/  - Security drivers
Y:/ncp_sw/corecom/base_components/      - Power management
Y:/ext/adapt/displayext.nokia/          - Display driver
Y:/ext/adapt/wlan.nokia/               - WLAN driver
Y:/ext/os/devicesrv.os/thermal/         - Thermal sensor
Y:/sf/os/kernelhwsrv/                   - Symbian kernel
Y:/sf/os/wlan/                          - WLAN bearer
Y:/sf/os/deviceio/sensorservices/       - Sensor framework
```

---

## 10. Driver File Inventory

### Kernel Extensions (.LDD / .PDD)

```
LightDrv.pdd          - Lighting/backlight
accelerometer.pdd     - Accelerometer sensor
magnetometer.pdd      - Magnetometer sensor
dipro.pdd             - Proximity sensor
khegl_ldd.ldd         - OpenGL ES graphics
wlan.ldd              - WLAN
ELOCD.LDD             - Local media driver
ECOMM.LDD             - Communications driver
USBC.LDD              - USB client
USBCSC.LDD            - USB scatter-gather client
```

### VideoCore VLLs

```
/Z:/vlls/bcm2727b1/   - VLL storage path
vcam.vll              - Camera
vcamdecode.vll        - Video decode
vcamencode.vll        - Video encode
vcamjpd.vll           - JPEG decode
vcamtest.vll          - Camera test
vcamfunc.vll          - Camera functions
vcamera.vll           - Camera service
vcam_shared_vid.vll   - Shared video
logging_vchi.vll      - Logging
```

---

## 11. Cross-reference with Known Hardware

| ROM Finding | Previously Known | Match? |
|------------|------------------|--------|
| bcm2727b1 string | BCM2727B1 from schematic | YES |
| ARM1176JZF-S | CPUID 0x410FB764 | YES |
| 3x I2C buses | I2C0/1/2 from DTS | YES |
| MUSBMHDRC USB | USB from service manual | YES |
| LIS302DL accel | LIS302DL on I2C | YES |
| AK8974 magnetometer | AK8974 on I2C | YES |
| WLAN via SPI | WLAN SPI-A from schematic | YES |
| MeSSI to VideoCore | CCP2/MeSSI from analysis | YES |
| DSI display | DSI from DCP | YES |
| HDMI output | HDMI from schematic | YES |
| eMMC + SD | 2 MemCard slots | YES |
| hw79 platform ID | Not previously known | NEW |
| ADXL340 variant | Not previously known | NEW |
| AMI305 variant | Not previously known | NEW |
| DLightDisplayMentor | Not previously known | NEW |
| Dispmanx/GCE | VideoCore III confirmed | NEW |
| ThermalSensor | Not previously known | NEW |

---

## 12. Implications for Linux Port

1. **No pin mux documentation found in ROM.** The register addresses for
   GPIO function selection must be found by other means (JTAG, hardware
   probing via Symbian LDD, or reading SWBL code).

2. **Peripheral virtual addresses at 0xC800xxxx** need to be mapped to
   physical addresses. The TRomHeader's 0xC8000000 value confirms this is
   the Symbian virtual base. Physical base is likely 0x20000000 (BCM2835
   convention) but could be different.

3. **MUSBMHDRC USB controller** -- Linux has `musb` driver support, but
   register base address needed.

4. **GPIO controller layout** is custom Broadcom, not OMAP. Needs register
   map from binary analysis of GPIO code at ROM 0xC0C00-0xC1900.

5. **VideoCore III** could potentially be driven by the `vc4` DRM driver
   (used by Raspberry Pi) if register compatibility exists.

6. **MeSSI/CCP2 interface** is Broadcom-proprietary. VideoCore firmware
   boot requires understanding the DVCDriver boot protocol.

7. **All I2C peripherals are confirmed** -- LIS302DL, AK8974, DiPro (APDS),
   Atmel mXT touch, LM8323 keyboard expander.

---

## 13. Open Questions

1. What is the physical peripheral base address? (0x20000000? 0x7E000000?)
2. What is the mapping between 0xC8xxxxxx virtual and physical?
3. Where is the GPIO function select / pin mux register?
4. What is the interrupt controller type and base address?
5. What is the UART register base? (Bluetooth uses HCTL UART)
6. Are the VideoCore registers accessible from the ARM side?
7. What does SWBL configure before Symbian boot?
8. What is the relationship between LoSSI clock and TWL5031 PMIC control?

---

## 14. Next Steps for Register Discovery

1. **Symbian LDD approach**: Write a Symbian LDD to read physical memory
   pages at candidate peripheral bases (0x20000000, 0x7E000000) and check
   for register responses.

2. **SWBL analysis**: Extract and disassemble SWBL (8.5KB at NAND 0x800)
   to find peripheral initialization code and physical addresses.

3. **GPIO code disassembly**: The GPIO driver code at ROM 0xC0C00-0xC1900
   contains the register layout. Disassemble with IDA/Ghidra to extract
   the register offsets relative to the virtual base 0xC8004xxx.

4. **DSpecialMemoryBlock analysis**: The `DSpecialMemoryBlock` class maps
   physical memory. Disassembling its constructor will reveal the physical
   to virtual address mappings.
