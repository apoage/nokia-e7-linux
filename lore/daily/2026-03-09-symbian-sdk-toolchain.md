# 2026-03-09 — Symbian SDK Toolchain Bootstrap

## Goal
Set up Symbian^3 cross-compilation on Linux to build memread LDD + app
for reading OMAP3630 PADCONF registers from the running Nokia E7.

## Key Achievement
**Built working Symbian .exe and .ldd from Linux using GCC 15 + elf2e32.**
No Windows VM, no RVCT, no Docker — native Linux toolchain.

## Toolchain Stack
```
arm-linux-gnueabihf-g++ (GCC 15.2.0)
    ↓ .cpp → .o (ARM EABI, freestanding)
    ↓ .o + .dso → .elf (shared object)
elf2e32 (GNUPoc, patched)
    ↓ .elf → .exe / .ldd (Symbian E32 format)
makesis (GNUPoc)
    ↓ .exe + .ldd + .rsc → .sis (install package)
```

## Problems Solved

### 1. elf2e32 segfault with GCC 15 ELFs
GCC 15 linker embeds GNU version info (`.gnu.version_r`) in the ELF.
elf2e32's `getDsoName()` returns NULL for symbols where version lookup fails,
then `fixImportRelocation()` dereferences NULL at `reloc.cpp:75`.

**Fix**: Patched elf2e32.cpp to check for NULL and fall back to trying all
NEEDED DSOs from the dynamic section.

### 2. Symbol name mangling mismatch
- Without `_UNICODE`: `TDesC` = `TDesC8` → mangled as `6TDesC8`
- DSOs built by RVCT have `TDesC` = `TDesC16` → mangled as `7TDesC16`
- **Fix**: `-D_UNICODE`

### 3. Sized deallocation
- GCC 15 generates `_ZdlPvj` (sized `operator delete(void*, unsigned int)`)
- Symbian DSOs only have `_ZdlPv` (unsized `operator delete(void*)`)
- **Fix**: `-fno-sized-deallocation`

### 4. `__declspec(dllexport)` unsupported
- SDK headers define `EXPORT_C` as `__declspec(dllexport)` for `__GCC32__+__MARM__`
- GCC on Linux doesn't support `__declspec` (Windows/Symbian-specific)
- **Fix**: `-D'__declspec(x)='`

### 5. `operator new/delete` not in euser.dso
- Found in `scppnwdl.dso` (Symbian C++ new/delete library)
- **Fix**: Link against scppnwdl.dso

### 6. DSO versioned filenames
- elf2e32 looks for `euser{000a0000}.dso`, not `euser.dso`
- **Fix**: Symlinks in lib/

### 7. makesis OpenSSL 3.x breakage
- signutils.cpp uses removed OpenSSL APIs (`EVP_PKEY` direct access, `ERR_remove_state`)
- **Fix**: Created `makesis-signutils-stub.cpp` with no-op signing functions

### 8. Kernel header include depth (LDD)
- kernelhwsrv source tree has deep interdependencies (nklib.h → nk_plat.h → ...)
- **Fix**: Use SDK's pre-packaged `platform/` include tree instead of source tree

## Phone Testing
- `hello.sis` (2.6KB): **installs OK** on jailbroken Unit B
- Compatibility warning (no platform dep line) — accepted
- **Not visible in launcher** — rcomp generating 8-bit strings, needs UCS-2
- Haven't confirmed EXE actually runs yet

## Next Steps
1. Fix rcomp Unicode output (try `-u` flag or generate UCS-2 resources)
2. Alternatively: find file manager on phone to run exe directly
3. Verify hello.exe creates E:\hello.txt
4. If confirmed: build and deploy memread.sis
5. Connect via WiFi TCP for interactive register reads

## Files Created/Modified
- `e7/symbian-sdk/memread/` — all source, Makefile, .pkg files
- `e7/symbian-sdk/lib/` — DSO imports with versioned symlinks
- `e7/symbian-sdk/gnupoc-package/tools/elf2e32/elf2e32.cpp` — patched
- `e7/symbian-sdk/gnupoc-package/tools/elf2e32/reloc.h` — added declaration
- `e7/symbian-sdk/gnupoc-package/tools/rcomp-7.0.1/` — built from source
- `~/.claude/projects/-home-lukas-ps3/memory/symbian-sdk.md` — detailed notes
