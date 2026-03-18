# Security Policy

## Nature of This Project

This is a hardware reverse-engineering and Linux porting project for a
discontinued phone (Nokia E7-00, released 2011). The security considerations
are different from typical software projects.

## What This Project Contains

- **QEMU machine emulation code** — custom peripheral models, no real security surface
- **Device tree source** — hardware description, no executable code
- **Kernel configuration** — standard Linux defconfig
- **Documentation** — hardware findings, RE notes, session logs
- **Build scripts** — toolchain configuration

## Security-Sensitive Content We DON'T Publish

- Nokia firmware binaries or ROM dumps
- Cryptographic keys (DES, AES, signing keys)
- Bootloader bypass patches
- Certificate stores or signing credentials
- Phone-specific identifiers (IMEIs)

## Reporting

If you notice any inadvertently published sensitive material (keys, firmware
fragments, personal data), please open an issue or email the maintainer.

## Supported Versions

This is a research project with no production deployments. There are no
"supported versions" in the traditional sense.
