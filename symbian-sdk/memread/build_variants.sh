#!/bin/sh
# Build the exe test-variant matrix.
# Run from repo root:  symbian-sdk/memread/build_variants.sh
#
# Task 090 follow-up: E32 header diff (tools/e32hdr.py) proved our binaries
# are structurally identical to known-working ones; the only semantic delta
# is secureId/uid3 in the TEST range (0xE...) which retail devices reject
# for untrusted code. Untrusted binaries must use the UNPROTECTED range
# (0xA0000000-0xAFFFFFFF).
#
#   test_A  uid 0xA0000099  deflate     — expected fix
#   test_B  uid 0xA000009B  uncompressed — fix minus compression variable
#   (test_C with 0xE-range uid dropped: may block install at SWI stage)
#
# GCC runs inside the symbian-sdk:latest docker image (elf2e32 in the image
# is glibc-broken, so elf2e32/makesis/signsis run from gnupoc-package on host).
set -e
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
MEMREAD="$ROOT/symbian-sdk/memread"
GNUTOOLS="$ROOT/symbian-sdk/gnupoc-package/tools"
ELF2E32="$GNUTOOLS/elf2e32/elf2e32"
MAKESIS="$GNUTOOLS/makesis-4/src/makesis"
SIGNSIS="$GNUTOOLS/makesis-4/src/signsis"
LIB=symbian-sdk/lib

# --- phase 1: compile + link inside docker -------------------------------
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
for V in A B C; do
    $GCC $CXXFLAGS $INC -DMARKER="\"$V\"" -c test_variant.cpp -o test_$V.o
    $GCC -nostdlib -shared -Wl,--entry=_E32Startup -o test_$V.elf test_$V.o crt_stubs_v6.o \
        $SDK/epoc32/release/armv5/urel/eexe.lib $SDK/epoc32/release/armv5/urel/usrt3_1.lib $LIB/euser.dso $LIB/efsrv.dso $LIB/scppnwdl.dso
done
echo "docker phase done"
'

# --- phase 2: elf2e32 + SIS on host ---------------------------------------
cd "$MEMREAD"

e32()
{
    V=$1; UID=$2; EXTRA=$3
    "$ELF2E32" \
        --elfinput=test_$V.elf \
        --output=test_$V.exe \
        --targettype=exe \
        --uid1=0x1000007a --uid2=0x100039ce --uid3=$UID \
        --sid=$UID --vid=0x00000000 \
        --capability=none \
        --heap=0x1000,0x100000 \
        --stack=0x8000 \
        --linkas=test_$V{000a0000}[$UID].exe \
        --libpath="$ROOT/$LIB" \
        $EXTRA
}

e32 A 0xA0000099 ""
e32 B 0xA000009B "--uncompressed"
e32 C 0xE0000099 "--uncompressed"

cat > test_variants.pkg <<'EOF'
&EN

#{"ExeTestVariants"},(0xA0000099),1,0,0

%{"Nokia-E7-Linux"}
:"Nokia-E7-Linux"

; Platform dependencies — must match the set in fshell.sis (the package
; proven to install on THIS phone). The 4th (0x1028315F) is the one
; Symbian^3 actually satisfies; without it SWI fails with "unable to
; install" after the missing-component prompts.
(0x101F7961), *, *, *, {"Series60ProductID"}
(0x102032BE), *, *, *, {"Series60ProductID"}
(0x102752AE), *, *, *, {"Series60ProductID"}
(0x1028315F), *, *, *, {"Series60ProductID"}

"test_A.exe" - "!:\sys\bin\test_A.exe"
"test_B.exe" - "!:\sys\bin\test_B.exe"
EOF

# Self-signed via makesis -c (spec-conformant self-signature; the earlier
# openssl+signsis SHA256 attempt failed to install on the phone).
# NOTE: makesis only accepts .sis output names — rename after.
"$MAKESIS" -c test_variants.pkg test_variants_signed.sis
mv test_variants_signed.sis test_variants.sisx

# Unsigned variant: installs on InstallServer-patched devices
"$MAKESIS" test_variants.pkg test_variants_unsigned.sis

ls -l test_A.exe test_B.exe test_variants.sisx test_variants_unsigned.sis
