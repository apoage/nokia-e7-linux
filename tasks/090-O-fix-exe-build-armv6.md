# Task 090: Fix Exe Build for ARM1176 (ARMv6)

## Status
**SOLVED — 2026-08-20 device session.** test_A and test_B both executed on
the phone (marker files E:\test_ok_A.txt / E:\test_ok_B.txt written).
First custom native exes to run in this project.

## The Recipe (proven, replicate for every future exe)

1. **uid3 AND sid in the UNPROTECTED range 0xA0000000-0xAFFFFFFF**
   (never 0xE... test range — retail rejects at exec; never 0x2... protected
   — needs real signing)
2. **zero capabilities** (self-signable; children need caps ⊆ parent anyway)
3. Link: `-nostdlib -shared -Wl,--entry=_E32Startup` + `crt_stubs_v6.o` +
   `eexe.lib` + `usrt3_1.lib` (both from sdk release/armv5/urel/) + needed
   .dso imports (euser/efsrv/scppnwdl) — see build_variants.sh
4. elf2e32: `--targettype=exe --capability=none --vid=0`
5. **SIS must declare all four platform deps**:
   0x101F7961, 0x102032BE, 0x102752AE, **0x1028315F** ("Series60ProductID").
   0x1028315F is the one Symbian^3 satisfies — without it: three
   "component missing" prompts then "unable to install" (both signed and
   unsigned). Found by full controller parse (tools/sisdeps.py) of
   fshell.sis, the package proven to install on this phone.
6. Sign with `makesis -c` (built-in self-signature; the openssl/sha256
   signsis path failed to install). Unaccepted "untrusted" warning is fine.

## Root Cause (2026-08-20)

The exe format was never broken. `tools/e32hdr.py` (new parser, layout per
kernelhwsrv `f32image.h` with the 124-byte base header) diffed our test_v6.exe
against the known-working RomPatcherPlus.exe from this phone:

- flags 0x1200002a, deflate compression, tools version, cpu id 0x2001
  (ARMv5), process priority 0x015e, exportDescType 0x01 — **all identical**
- both have **zero capabilities** (the old "ReadDeviceData caps cause -46"
  theory is dead — test_v6 was already built capless)
- ELF structure is correct: `_E32Startup` from `eexe.lib` entry, proper
  euser/efsrv imports

The **only** semantic difference: `uid3/secureId`.

| binary | uid3/secureId | range | result |
|--------|---------------|-------|--------|
| RomPatcherPlus.exe | 0x20047b9f | protected (signed) | runs |
| our exes (all past attempts) | 0xe0000001/0xe0000099 | **test** | error 3 / -46 |

Symbian UID ranges: 0x10000000-0x2FFFFFFF protected (needs real signing),
0xA0000000-0xAFFFFFFF **unprotected (self-sign OK)**, 0xE0000000-0xEFFFFFFF
**test range — dev-cert only**. gnupoc's examples all use 0xE... UIDs, which
work on the emulator but are rejected by retail devices for untrusted code.
Every exe this project ever built used a 0xE... UID. That's the bug.

## Variant Matrix (built, in symbian-sdk/memread/)

`build_variants.sh` (host script; GCC phase in docker, elf2e32/makesis/signsis
from gnupoc-package on host):

| file | uid | compression | purpose |
|------|-----|-------------|---------|
| test_A.exe | 0xA0000099 | deflate | the fix — matches normal S^3 binaries |
| test_B.exe | 0xA000009B | none | fix minus compression variable |
| test_C.exe | 0xE0000099 | none | control — expected to fail like all predecessors |
| test_variants.sisx | pkg 0xA0000099 | — | self-signed SIS with all three |

Each writes `E:\test_ok_<letter>.txt` on success. Phone-side runner:
`pys60/run_variants.py`.

Build gotchas fixed along the way (now encoded in build_variants.sh):
- entry point needs `-Wl,--entry=_E32Startup` (elf2e32 copies e_entry)
- `eexe.lib` (RVCT, in `release/armv5/urel/`) needs `usrt3_1.lib` for the
  hidden `CallThrdProcEntry` symbol
- elf2e32 `--libpath` wants both `foo.dso` and `foo{000a0000}.dso` variants
  (restored from docker image into symbian-sdk/lib/, untracked)

## Verification (next device session)

1. Mass-storage copy run_variants.py + (optionally) exes to E:\
2. Install test_variants.sisx (untrusted prompt expected)
3. Run run_variants.py from Python — reports start_exe return code per
   variant + which marker files appeared
4. If A or B runs: exe execution unblocked → Task 089 bridge + Task 088
   patcher_probe become buildable same-day

If A and B both still fail with -46 while C fails identically, the SID-range
theory is wrong and next suspect is SWI trust state of mass-storage-copied
files vs SIS-installed (B via SIS vs bare copy distinguishes this).
