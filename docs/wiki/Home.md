# Nokia E7-00 Linux Project

## Roadmap

### Phase 1: Emulation (Current)
- [x] QEMU machine boots Linux to shell
- [x] OMAP3630 core model (INTC, GPIO, sDMA, CM/PRM)
- [x] TWL5031 PMIC (I2C, RTC, USB PHY, regulators)
- [x] All I2C sensors probe (accel, mag, prox, touch, charger, LED)
- [x] LM8323 keyboard pipeline working
- [x] OneNAND model with backing store
- [x] NLoader boot path (34 unique messages, reached GENIO)
- [ ] PADCONF register dump from real hardware
- [ ] SDRC timing configuration
- [ ] Display output (DSS → framebuffer)
- [ ] USB gadget model
- [ ] WiFi (SPI) model

### Phase 2: Real Hardware Boot
- [ ] UART debug capture from E7
- [ ] Correct pad mux configuration in DTS
- [ ] SDRC/LPDDR timing from real hardware
- [ ] First kernel boot on real E7
- [ ] Display bring-up
- [ ] Touchscreen input

### Phase 3: Usable System
- [ ] WiFi networking
- [ ] Audio (TWL5031 codec)
- [ ] Keyboard fully mapped
- [ ] Camera (BCM2727 ISP)
- [ ] GPU (PowerVR SGX530)
- [ ] Cellular modem (RAPUYAMA)

## Wiki Pages

- [[Device Specification]] — hardware details
- [[Hardware Components]] — chip inventory and bus map
- [[QEMU Machine]] — emulation architecture
- [[Kernel Configuration]] — defconfig and DTS
- [[Boot Process]] — from ROM to Linux shell
- [[PADCONF Quest]] — the ongoing battle to read 584 bytes
- [[Symbian Toolchain]] — building and running code on the phone
- [[Research Sources]] — community contacts, archives, tools
