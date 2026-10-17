#!/bin/bash
# Repair + rebuild emulation/qemu/build after the 2026-08 distro upgrade.
#
# Why this exists: the upgrade removed python3.13 (the build dir's meson
# venv broke) and the glib/numa/pixman dev packages (headers, .so symlinks,
# gdbus-codegen gone). Without root, the stale build dir is repaired by:
#   1. replacing the broken venv meson with a shim running meson from
#      ~/.local/lib/python3.*/site-packages under python3.14
#   2. extracting the dev .debs under build/{glib,numa,pixman}-dev
#      (apt-get download works as a user; needs network only once)
#   3. idempotent build.ninja surgery: drop regen inputs that no longer
#      exist, repoint dev-symlink link inputs at runtime sonames, strip
#      -Werror (new GCC vs vendored dtc)
#   4. ninja with CPATH pointing at the extracted headers
#
# The proper fix, if you ever have root:
#   apt install libglib2.0-dev libnuma-dev libpixman-1-dev
# plus a working meson — then re-add `select PL011` to hw/arm/Kconfig
# (NOKIA_E7; reverted because PL011 is default-y in arm-softmmu and the
# select would force a meson regen) and let meson regenerate cleanly.
# Details: tasks/093-O-qemu-nloader-linux-chain.md "Build environment
# repair".

set -euo pipefail

BUILD="$(cd "$(dirname "${BASH_SOURCE[0]}")/qemu/build" && pwd)"
cd "$BUILD"

# --- 1. venv meson shim (only if the current one is broken) ---
if [ -x pyvenv/bin/meson ] && ! pyvenv/bin/meson --version >/dev/null 2>&1; then
    PY313_SITE="$(ls -d "$HOME"/.local/lib/python3.*/site-packages 2>/dev/null | tail -1)"
    if [ -z "$PY313_SITE" ] || ! PYTHONPATH="$PY313_SITE" python3 -c 'import mesonbuild' 2>/dev/null; then
        echo "ERROR: no importable mesonbuild found under ~/.local — install meson (pip --user) or apt" >&2
        exit 1
    fi
    printf '#!/bin/sh\nexport PYTHONPATH=%s${PYTHONPATH:+:$PYTHONPATH}\nexec /usr/bin/python3 -c "import sys; from mesonbuild.mesonmain import main; sys.exit(main())" "$@"\n' \
        "$PY313_SITE" > pyvenv/bin/meson
    chmod +x pyvenv/bin/meson
    echo "venv meson shim installed (meson $(pyvenv/bin/meson --version))"
fi

# --- 2. dev debs: download + extract anything missing ---
fetch() { # fetch <pkg>... into <dir>, verified by <test-path>
    local dir=$1 test=$2; shift 2
    if [ ! -e "$test" ]; then
        echo "extracting $* into $dir"
        apt-get download "$@" || true
        local d
        for d in "$@"; do
            for f in ${d}_*_amd64.deb; do
                [ -f "$f" ] && dpkg -x "$f" "$dir"
            done
        done
    fi
}
fetch glib-dev  glib-dev/usr/include/glib-2.0/glib.h \
      libglib2.0-dev libgio-2.0-dev libgio-2.0-dev-bin libglib2.0-dev-bin
fetch numa-dev  numa-dev/usr/include/numa.h            libnuma-dev
fetch pixman-dev pixman-dev/usr/include/pixman-1/pixman.h libpixman-1-dev

export CPATH="$BUILD/glib-dev/usr/include/glib-2.0:$BUILD/glib-dev/usr/include/gio-unix-2.0:$BUILD/glib-dev/usr/lib/x86_64-linux-gnu/glib-2.0/include:$BUILD/numa-dev/usr/include:$BUILD/pixman-dev/usr/include/pixman-1${CPATH:+:$CPATH}"

# --- 3. build.ninja surgery (idempotent; no-ops once applied or after a
#        clean meson regen puts proper paths back) ---
python3 - <<'EOF'
import glob, os, re

if not os.path.exists('build.ninja'):
    raise SystemExit('no build.ninja here — configure the build dir first')
s = open('build.ninja').read()
orig = s

# regen inputs that no longer exist on this host
s = s.replace(' /usr/bin/python3.13', ' ')
s = s.replace(' | /usr/bin/gdbus-codegen', '')
# gdbus-codegen now lives in the extracted tree (relative to build dir)
if 'glib-dev/usr/bin/gdbus-codegen' not in s:
    s = s.replace('/usr/bin/gdbus-codegen ui/',
                  'glib-dev/usr/bin/gdbus-codegen ui/')
s = s.replace('glib-devglib-dev/usr/bin/gdbus-codegen',
              'glib-dev/usr/bin/gdbus-codegen')  # undo older double-patch

# dev .so symlinks removed — link against the runtime sonames instead
# (delimiter-agnostic: the ref can be followed by a space, comma or EOL)
for lib in ('libgio-2.0', 'libglib-2.0', 'libgmodule-2.0', 'libgobject-2.0'):
    s = s.replace(f'/usr/lib/x86_64-linux-gnu/{lib}.so.so.0',
                  f'/usr/lib/x86_64-linux-gnu/{lib}.so.0')  # undo double-patch
    so = f'/usr/lib/x86_64-linux-gnu/{lib}.so'
    if so in s and not os.path.exists(so):
        s = re.sub(re.escape(so) + r'(?=[ ,\n]|$)', so + '.0', s)
for lib in ('libselinux', 'libnuma'):
    so = f'/usr/lib/x86_64-linux-gnu/{lib}.so '
    if so in s and not os.path.exists(so.rstrip()):
        s = s.replace(so, f'/usr/lib/x86_64-linux-gnu/{lib}.so.1 ')
s = s.replace(' -lnuma ', ' /usr/lib/x86_64-linux-gnu/libnuma.so.1 ')

# versioned libtinfo moved (6.5 -> 6.6 here): repoint at what exists
for m in set(re.findall(r'/usr/lib/x86_64-linux-gnu/libtinfo\.so\.\d+\.\d+', s)):
    if not os.path.exists(m):
        cand = sorted(glob.glob('/usr/lib/x86_64-linux-gnu/libtinfo.so.*.*'))
        if cand:
            s = s.replace(m, cand[-1])

# new GCC flags vendored dtc code (-Werror is fatal there)
s = s.replace(' -Werror ', ' ')

if s != orig:
    open('build.ninja', 'w').write(s)
    print('build.ninja: repair patches applied')
else:
    print('build.ninja: already clean')
EOF

# --- 4. build ---
# Default to the only target we use: the full `ninja` also builds qemu-nbd
# (needs libselinux headers, gone with the upgrade) and TCG test plugins
# (hardcode -I/usr/include/glib-2.0), which are not needed here.
if [ $# -eq 0 ]; then
    set -- qemu-system-arm
fi
exec ninja "$@"
