# Symbian^3 Toolchain (for on-device development)

## The CRT Startup Discovery

The single most important finding for anyone trying to build Symbian executables
with GCC: **you must link with `eexe.lib`**.

Without it, your exe enters `E32Main()` with no heap — `CTrapCleanup::New()`
fails silently, and the process crashes immediately. This explains every
"installs but doesn't run" report in Symbian homebrew history.

### The Fix

```bash
# Link with CRT startup
$CC -nostdlib -shared -Wl,--entry,_E32Startup \
  -o app.elf app.o crt_stubs.o \
  -Wl,--whole-archive eexe.lib usrt2_2.lib -Wl,--no-whole-archive \
  euser.dso efsrv.dso scppnwdl.dso
```

CRT startup chain: `_E32Startup` → `RunThread` → `UserHeap::SetupThreadHeap`
→ `CallThrdProcEntry` → `E32Main()`

### Required Stubs (crt_stubs.cpp)

```cpp
void operator delete(void* p, unsigned int) throw()
    { ::operator delete(p); }

extern "C" {
void* __cxa_begin_catch(void* p) { return p; }
void __cxa_end_catch() {}
void __cxa_end_cleanup() {}
static const char _ZTS15XLeaveException[] = "15XLeaveException";
extern const struct { void* vt; const char* n; }
    _ZTI15XLeaveException __attribute__((weak));
const struct { void* vt; const char* n; }
    _ZTI15XLeaveException = { 0, _ZTS15XLeaveException };
void _ZN23TCppRTExceptionsGlobalsC1Ev(void*) {}
}
```

## Docker Build Environment

```dockerfile
# Uses arm-none-symbianelf-gcc 12.1.0
docker build -t symbian-sdk .
docker run --rm -v $(pwd):/project symbian-sdk bash -c '...'
```

Full SDK installed via gnupoc: headers, DSOs, tools (elf2e32, makesis, signsis, rcomp).

## Deployment to Phone

SIS packages with `capability=none` install normally. Higher capabilities
require either jailbreak (Open4All + InstallServer via RomPatcher) or
direct binary copy via Python (PyS60):

```python
# Copy exe directly to sys\bin (requires Open4All patch)
sf = open('E:\\app.exe', 'rb')
data = sf.read(); sf.close()
df = open('C:\\sys\\bin\\app.exe', 'wb')
df.write(data); df.close()
```

## Key Limitation: TCB Capability

TCB (Trusted Computing Base) is **ROM-only** on Symbian. No SIS-installed
binary can have it, regardless of certificate. This means:

- Cannot load custom kernel drivers (LDDs)
- Cannot open channels to TCB-requiring LDDs (like memoryAccess-fshell)
- RomPatcher's LDDs work only because they're manufacturer-signed

This is the fundamental wall for kernel-level access on Symbian.
