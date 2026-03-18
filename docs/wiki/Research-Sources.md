# Research Sources

## Community Contacts

| Person | Platform | Expertise |
|--------|----------|-----------|
| Max Bondarchenko (iCrazyDoctor) | t.me/symbian_world | RomPatcher internals, kernel arch, CFW |
| symbuzzer | t.me/symbian_world | CapsSwitch/PlatSec tool, kernel patching |
| Sicelo | postmarketOS | N900 maintainer, OMAP3 kernel work |
| Trzyzet | nokiahacking.pl | BB5 RE, attempted Linux on N95-2 |

## Telegram Groups
- **Symbian World**: t.me/symbian_world (active, ~127K messages analyzed)
- **Linux Mobile World**: t.me/linuxmobile_world

## Websites
- **symwld.com**: Delight CFW, patches, curated app store
- **nokiahacking.pl**: Polish Nokia forum, RE section
- **allaboutsymbian.com**: News, reviews, guides
- **fshell.sourceforge.net**: FShell documentation

## Archives
- **Nokia Service Tools** (15.3GB): archive.org/details/phoenix_service_sw_a12_2003_50_7_36-Feb10-085637
- **Symbian World Mega Repo**: mega.nz/folder/wnQkiSKD#vnJZyBYYbpJtfLMr2U69KQ
- **WunderWungiel Symbian**: wunderwungiel.pl/Symbian
- **Flasher tools**: wunderwungiel.pl/Maemo/flasher, wunderwungiel.pl/MeeGo/flasher
- **SymbianSource**: github.com/SymbianSource

## Technical References
- **elinux.org/N950**: N950 hardware wiki
- **postmarketOS OMAP3**: wiki.postmarketos.org/wiki/Texas_Instruments_OMAP_3_(OMAP3xxx)
- **FShell docs**: fshell.sourceforge.net/documentation/fshell/index.html
- **Symbian Platform Security**: web.archive.org/web/20240316184730/http://wiki.franklinheath.co.uk/index.php/Symbian_OS_Platform_Security
- **Nokia E7 Service Hints**: Internal Nokia service document (in this repo's docs/)

## Tools Used
- QEMU v10.0.0 (custom nokia-e7 machine)
- Linux 6.12 (arm cross-compile)
- Docker (symbian-sdk build environment)
- arm-none-symbianelf-gcc 12.1.0
- GNUPoc (elf2e32, makesis, signsis, rcomp)
- PyS60 (on-device Python scripting)
- FShell v5.00 (on-device shell)
- MemSpy (Nokia debug tool)
- RomPatcherPlus 3.1 (kernel patching)
