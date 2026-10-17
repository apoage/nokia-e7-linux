#!/bin/sh
# Build patcher_probe with the recipe proven by test_A/test_B (2026-08-20):
# unprotected-range UID 0xA000xxxx + eexe.lib/usrt3_1.lib link + 4 platform
# deps (0x1028315F is the one S^3 satisfies) + makesis -c self-signature.
# Output: patcher_probe.exe + patcher_probe.sisx
set -e
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
MEMREAD="$ROOT/symbian-sdk/memread"
GNUTOOLS="$ROOT/symbian-sdk/gnupoc-package/tools"
ELF2E32="$GNUTOOLS/elf2e32/elf2e32"
MAKESIS="$GNUTOOLS/makesis-4/src/makesis"
UID=0xA0000098

docker run --rm -v "$ROOT:/work" symbian-sdk:latest sh -e -c '
MEMREAD=/work/symbian-sdk/memread
GCC=/root/gcc-builds/gcc-12.1.0/bin/arm-none-symbianelf-g++
SDK=/opt/symbian/sdk
LIB=$SDK/epoc32/release/armv5/lib
CXXFLAGS="-nostdinc -nostdlib -ffreestanding -fno-exceptions -fno-rtti \
-fno-builtin -fshort-wchar -fshort-enums -fno-sized-deallocation \
-march=armv6 -mthumb -mfloat-abi=soft -Wno-narrowing \
-D__SYMBIAN32__ -D__EPOC32__ -D__MARM__ -D__MARM_ARMV5__ -D__EABI__ \
-D__GCC32__ -D__GCCE__ -D__SUPPORT_CPP_EXCEPTIONS__=0 -D_UNICODE \
-D__declspec(x)="
INC="-I$SDK/epoc32/include -I$SDK/epoc32/include/platform -I$MEMREAD"
cd "$MEMREAD"
$GCC $CXXFLAGS $INC -c patcher_probe.cpp -o patcher_probe.o
$GCC -nostdlib -shared -Wl,--entry=_E32Startup -o patcher_probe.elf \
    patcher_probe.o crt_stubs_v6.o \
    $SDK/epoc32/release/armv5/urel/eexe.lib \
    $SDK/epoc32/release/armv5/urel/usrt3_1.lib \
    $LIB/euser.dso $LIB/efsrv.dso $LIB/scppnwdl.dso
echo "docker phase done"
'

cd "$MEMREAD"
"$ELF2E32" \
    --elfinput=patcher_probe.elf \
    --output=patcher_probe.exe \
    --targettype=exe \
    --uid1=0x1000007a --uid2=0x100039ce --uid3=$UID \
    --sid=$UID --vid=0x00000000 \
    --capability=none \
    --heap=0x1000,0x100000 \
    --stack=0x8000 \
    --linkas=patcher_probe{000a0000}[$UID].exe \
    --libpath="$ROOT/symbian-sdk/lib"

cat > patcher_probe.pkg <<EOF
&EN

#{"PatcherProbe"},($UID),1,0,0

%{"Nokia-E7-Linux"}
:"Nokia-E7-Linux"

(0x101F7961), *, *, *, {"Series60ProductID"}
(0x102032BE), *, *, *, {"Series60ProductID"}
(0x102752AE), *, *, *, {"Series60ProductID"}
(0x1028315F), *, *, *, {"Series60ProductID"}

"patcher_probe.exe" - "!:\\sys\\bin\\patcher_probe.exe"
EOF
"$MAKESIS" -c patcher_probe.pkg patcher_probe_signed.sis
mv patcher_probe_signed.sis patcher_probe.sisx
ls -l patcher_probe.exe patcher_probe.sisx
