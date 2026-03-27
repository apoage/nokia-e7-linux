# Makefile for memread LDD + app
# Cross-compile for Symbian^3 on Nokia E7-00 (OMAP3630/ARM Cortex-A8)
#
# Requires: arm-linux-gnueabihf-g++ (sudo apt install g++-arm-linux-gnueabihf)
# Produces: memread.ldd (kernel driver) + memread.exe (user app)

# Tools
CXX       = arm-linux-gnueabihf-g++
ELF2E32   = ../gnupoc-package/tools/elf2e32/elf2e32
MAKESIS   = ../gnupoc-package/tools/makesis-4/src/makesis

# Paths
SDK_INC   = /tmp/sdk-probe/CPP_API/epoc32/include
SDK_PLAT  = /tmp/sdk-probe/CPP_API/epoc32/include/platform
KERN_INC  = ../kernelhwsrv/kernel/eka/include
SDK_LIB   = ../lib

# Common flags
CXXFLAGS  = -nostdinc -nostdlib -ffreestanding -fno-exceptions -fno-rtti \
            -fno-builtin -fshort-wchar -fshort-enums -fno-sized-deallocation \
            -march=armv7-a -mthumb -mfloat-abi=soft -Wno-narrowing \
            -D__SYMBIAN32__ -D__EPOC32__ -D__MARM__ -D__MARM_ARMV5__ \
            -D__EABI__ -D__GCC32__ -D__GCCE__ -D__SUPPORT_CPP_EXCEPTIONS__=0 \
            -D_UNICODE -D'__declspec(x)='

# MW includes (Avkon, CONE, etc.)
SDK_MW    = /tmp/sdk-probe/CPP_API/epoc32/include/mw
# User-space includes
APP_INC   = -I$(SDK_INC) -I$(SDK_PLAT) -I$(SDK_MW) -I. -Istubs
# Kernel-space includes (LDD)
LDD_INC   = -I$(SDK_PLAT) -I$(SDK_PLAT)/kernel -I$(SDK_PLAT)/nkern \
            -I$(SDK_PLAT)/nkern/arm -I$(SDK_INC) \
            -I$(SDK_PLAT)/memmodel/epoc -I.

.PHONY: all clean app ldd

all: memread.exe memread.ldd

# --- User-space app ---
memread_app.o: memread_app.cpp memread_ldd.h
	$(CXX) $(CXXFLAGS) $(APP_INC) -c $< -o $@

# Link as shared object (elf2e32 converts to E32 exe)
memread_app.elf: memread_app.o
	$(CXX) -nostdlib -shared -o $@ $< \
		$(SDK_LIB)/euser.dso \
		$(SDK_LIB)/efsrv.dso \
		$(SDK_LIB)/esock.dso \
		$(SDK_LIB)/insock.dso \
		$(SDK_LIB)/scppnwdl.dso

memread.exe: memread_app.elf
	$(ELF2E32) \
		--elfinput=$< \
		--output=$@ \
		--targettype=exe \
		--uid1=0x1000007a --uid2=0x100039ce --uid3=0xe0000001 \
		--sid=0xe0000001 \
		--capability=NetworkServices+ReadDeviceData \
		--heap=0x1000,0x100000 \
		--stack=0x5000 \
		--linkas=memread_app{000a0000}[e0000001].exe \
		--libpath=$(SDK_LIB)

# --- Kernel LDD ---
memread_ldd.o: memread_ldd.cpp memread_ldd.h
	$(CXX) $(CXXFLAGS) $(LDD_INC) -D__KERNEL_MODE__ -c $< -o $@

memread_ldd.elf: memread_ldd.o
	$(CXX) -nostdlib -shared -o $@ $< \
		$(SDK_LIB)/ekern.dso

memread.ldd: memread_ldd.elf
	$(ELF2E32) \
		--elfinput=$< \
		--output=$@ \
		--targettype=ldd \
		--uid1=0x100000af --uid2=0x100039d0 --uid3=0xe0000002 \
		--sid=0xe0000002 \
		--capability=All \
		--heap=0x1000,0x100000 \
		--stack=0x5000 \
		--linkas=memread{000a0000}[e0000002].ldd \
		--libpath=$(SDK_LIB)

sis: memread.sis

memread.sis: memread.exe memread.ldd memread.pkg
	$(MAKESIS) memread.pkg memread.sis

# --- Hello test app ---
hello.o: hello.cpp
	$(CXX) $(CXXFLAGS) $(APP_INC) -c $< -o $@

hello.elf: hello.o
	$(CXX) -nostdlib -shared -o $@ $< \
		$(SDK_LIB)/euser.dso \
		$(SDK_LIB)/efsrv.dso \
		$(SDK_LIB)/scppnwdl.dso

hello.exe: hello.elf
	$(ELF2E32) \
		--elfinput=$< \
		--output=$@ \
		--targettype=exe \
		--uncompressed \
		--uid1=0x1000007a --uid2=0x100039ce --uid3=0xe0000099 \
		--sid=0xe0000099 \
		--capability=NetworkServices+ReadDeviceData \
		--heap=0x1000,0x100000 \
		--stack=0x5000 \
		--linkas=hello{000a0000}[e0000099].exe \
		--libpath=$(SDK_LIB)

hello.sis: hello.exe hello_reg.rsc hello.rsc hello.pkg
	$(MAKESIS) hello.pkg hello.sis

# --- Hello GUI (proper Avkon app) ---
hello_gui.o: hello_gui.cpp
	$(CXX) $(CXXFLAGS) $(APP_INC) -c $< -o $@

hello_gui.elf: hello_gui.o
	$(CXX) -nostdlib -shared -o $@ $< \
		$(SDK_LIB)/euser.dso \
		$(SDK_LIB)/efsrv.dso \
		$(SDK_LIB)/apparc.dso \
		$(SDK_LIB)/cone.dso \
		$(SDK_LIB)/eikcore.dso \
		$(SDK_LIB)/avkon.dso \
		$(SDK_LIB)/ws32.dso \
		$(SDK_LIB)/scppnwdl.dso

hello_gui.exe: hello_gui.elf
	$(ELF2E32) \
		--elfinput=$< \
		--output=$@ \
		--targettype=exe \
		--uid1=0x1000007a --uid2=0x100039ce --uid3=0xe0000099 \
		--sid=0xe0000099 \
		--capability=NetworkServices+ReadDeviceData \
		--heap=0x1000,0x100000 \
		--stack=0x5000 \
		--linkas=hello_gui{000a0000}[e0000099].exe \
		--libpath=$(SDK_LIB)

hello_gui.sis: hello_gui.exe hello_reg.rsc hello.rsc hello_gui.pkg
	$(MAKESIS) hello_gui.pkg hello_gui.sis

app: memread.exe
ldd: memread.ldd

clean:
	rm -f *.o *.elf memread.exe memread.ldd memread.sis hello.exe hello.sis \
		hello_gui.exe hello_gui.sis
