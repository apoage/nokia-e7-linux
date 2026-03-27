# 2026-03-27: GitHub Cleanup and Safe Push

## Incident

During a merge of `origin/main` into local `master`, used `git add -A` which
staged 171,617 files including:
- Nokia proprietary ROM dumps, service manuals
- Archive folder (hack kits, firmware, SIS files)
- Symbian SDK source code and binaries
- Private chat exports
- Third-party decompiled code

Push to GitHub failed (HTTP 500 — payload too large). No proprietary data
was published. GitHub repo was manually rolled back ~1 week as precaution.

## Fix

1. Created clean `.gitignore` blocking all proprietary/third-party content
2. Created `clean-push` branch from `origin/main`
3. Carefully cherry-picked 78 files — only our original work:
   - Our analysis docs (docs/*.md)
   - Our tools (tools/*.py)
   - Our phone scripts (symbian-sdk/memread/pys60/*.py)
   - Our source code (symbian-sdk/memread/*.cpp)
   - Our session notes (lore/*.md)
   - Our kernel config and DTS
   - Phone probe outputs (docs/phone-probes/)
   - Wiki pages, README, community files
4. Pushed clean commit to `main` — 78 files, 14,619 lines

## What Must NOT Be Published
- docs/romdumpplus.dmp, superpage.dmp (Nokia proprietary dumps)
- docs/service-manual/ (Nokia IP)
- docs/bcm2708-gpio.h, bcm2708-hardware.h (third-party, license unclear)
- archive/ (Nokia tools, hack kits, firmware)
- ChatExport/ (private conversations)
- symbian-sdk/kernelhwsrv/ (Symbian source, EPL license)
- symbian-sdk/lib/ (Nokia DSO libraries)
- symbian-sdk/gnupoc-package/ (third-party tools)
- firmware-re/ (decompiled firmware)
- CLAUDE.md, tasks/, orch.md (internal project management)

## .gitignore Updated
All of the above are now in `.gitignore` to prevent future accidents.

## Lesson
Never use `git add -A` or `git add .` in a workspace with proprietary data.
Always add specific files by name.
