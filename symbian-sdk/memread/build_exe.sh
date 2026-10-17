#!/bin/sh
# build_exe.sh NAME UID — build one probe exe + SISX with the proven recipe.
#   NAME: source file NAME.cpp in this dir (output NAME.exe, NAME.sisx)
#   UID : e.g. 0xA0000096 (unprotected range!)
set -e
NAME=$1
UID=$2
[ -f "$(dirname "$0")/$NAME.cpp" ] || { echo "usage: build_exe.sh NAME UID (needs NAME.cpp)"; exit 1; }
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
MEMREAD="$ROOT/symbian-sdk/memread"
GNUTOOLS="$ROOT/symbian-sdk/gnupoc-package/tools"
ELF2E32="$GNUTOOLS/elf2e32/elf2e32"
MAKESIS="$GNUTOOLS/makesis-4/src/makesis"

docker run --rm -v "$ROOT:/work" symbian-sdk:latest sh -e -c '
NAME='"$NAME"'
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
$GCC $CXXFLAGS $INC -c $NAME.cpp -o $NAME.o
$GCC -nostdlib -shared -Wl,--entry=_E32Startup -o $NAME.elf $NAME.o crt_stubs_v6.o \
    $SDK/epoc32/release/armv5/urel/eexe.lib \
    $SDK/epoc32/release/armv5/urel/usrt3_1.lib \
    $LIB/euser.dso $LIB/efsrv.dso $LIB/scppnwdl.dso
echo "docker phase done"
'

cd "$MEMREAD"
"$ELF2E32" \
    --elfinput=$NAME.elf --output=$NAME.exe \
    --targettype=exe \
    --uid1=0x1000007a --uid2=0x100039ce --uid3=$UID \
    --sid=$UID --vid=0x00000000 --capability=none \
    --heap=0x1000,0x100000 --stack=0x8000 \
    --linkas=$NAME{000a0000}[$UID].exe \
    --libpath="$ROOT/symbian-sdk/lib"

cat > $NAME.pkg <<EOF
&EN

#{"$NAME"},($UID),1,0,0

%{"Nokia-E7-Linux"}
:"Nokia-E7-Linux"

(0x101F7961), *, *, *, {"Series60ProductID"}
(0x102032BE), *, *, *, {"Series60ProductID"}
(0x102752AE), *, *, *, {"Series60ProductID"}
(0x1028315F), *, *, *, {"Series60ProductID"}

"$NAME.exe" - "!:\\sys\\bin\\$NAME.exe"
EOF
"$MAKESIS" -c $NAME.pkg ${NAME}_signed.sis
mv ${NAME}_signed.sis $NAME.sisx
ls -l $NAME.exe $NAME.sisx
