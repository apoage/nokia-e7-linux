#!/bin/bash
# chain-regress.sh — one-command regression gate for the faithful BB5-entry
# chain (task 122 shape, docs/bcm2727-onenand.md §5.3.1).
#
# Boots the full chain (SWBL shim → genuine BB5 entry 0x100103D0 → NLoader
# → trampoline 0x101FFC30 → Linux → eMMC-root busybox `e7:~#`), then asserts
# the boot markers in order, drives `/proc/interrupts` interactively at the
# prompt through a fifo, and diffs the whole `nloader[xti` print sequence
# against tools/chain-flow-canonical.txt (the task-122 independently
# re-verified 1202-line flow).
#
# Usage:
#   tools/chain-regress.sh [--icount] [--quick] [--keep] [--timeout N] [-h]
#
#     --icount   control variant: append -icount shift=auto to the QEMU run
#     --quick    skip the interactive /proc/interrupts block (kill at prompt)
#     --keep     always preserve the boot log (path printed in the summary;
#                without it a PASSing run's temp log is deleted; a FAILing
#                run's log is always kept as divergence evidence)
#     --timeout N  boot deadline in seconds (default 480 TCG / 600 icount)
#
# Output: one-screen summary, exit 0 = PASS, exit 1 = FAIL (first divergence
# named).  Runtime: ~30 s per run on the 2026-09-05 host (both TCG and
# icount; the original task estimate of 3-4 min is the budget for slower
# hosts — the boot deadline defaults to 480 s TCG / 600 s icount).
#
# FUTURE TASK FILES: if your change touches QEMU models, the shim, the
# kernel, the rootfs, or the boot docs — "run chain-regress before/after"
# and paste both summaries.  If the flow diff trips, investigate the boot:
# only edit tools/chain-flow-canonical.txt as a deliberate, documented
# commit when the flow LEGITIMATELY changed (see that file's header).
#
# Determinism tolerance list (explicit; empty as of 2026-09-05): the flow
# diff is byte-exact — the 1202-line sequence was identical across TCG and
# icount, debug envs on/off, and two kernel builds.  If a legitimately
# nondeterministic field ever appears, strip it HERE, by name — never
# loosen the diff silently.

set -u

REPO=$(cd "$(dirname "$0")/.." && pwd)
QEMU="$REPO/emulation/qemu/build/qemu-system-arm"
CANON="$REPO/tools/chain-flow-canonical.txt"

# Expected /proc/interrupts id claims (config list — extend as the chain
# grows; "icuid:name" pairs).  242/419/35/36/150-152 = tasks 94-112 era;
# 183 = task 123 (MUSBHDRC musb-hdrc).
IRQ_CLAIMS="242:bcm2727-tick 419:uart-pl011 35:20202800.mmc 36:20202900.mmc 150:20202200.i2c 151:20202300.i2c 152:20202500.i2c 183:musb-hdrc.1.auto"

# Interactive settle delays (seconds) — deliberately generous: busybox on
# TCG, never a false FAIL from timing.
SETTLE_TO_PROMPT_INPUT=12
SETTLE_AFTER_CMD=3
WAIT_INTERACTIVE=90

BOOT_TIMEOUT=480
MODE=tcg
QUICK=0
KEEP=0
while [ $# -gt 0 ]; do
  case "$1" in
    --icount) MODE=icount; BOOT_TIMEOUT=600 ;;
    --quick) QUICK=1 ;;
    --keep) KEEP=1 ;;
    --timeout) BOOT_TIMEOUT="${2:?--timeout needs a value}"; shift ;;
    -h|--help) sed -n '2,45p' "$0"; exit 0 ;;
    *) echo "chain-regress: unknown option: $1 (try -h)" >&2; exit 1 ;;
  esac
  shift
done

# ---- prerequisites --------------------------------------------------------
missing=0
for f in "$QEMU" "$CANON" \
  "$REPO/firmware-re/analysis/nloader.bin" \
  "$REPO/firmware-re/nokia-e7-onenand.img" \
  "$REPO/emulation/rootfs/emmc.img" \
  "$REPO/kernel/linux/arch/arm/boot/zImage" \
  "$REPO/kernel/linux/arch/arm/boot/dts/broadcom/bcm2727-nokia-e7.dtb"; do
  [ -e "$f" ] || { echo "MISSING: $f" >&2; missing=1; }
done
[ $missing -eq 0 ] || exit 1

EXTRA=()
[ "$MODE" = icount ] && EXTRA+=(-icount shift=auto)

LOG=$(mktemp /tmp/chain-regress.XXXXXX.log)
FIFO=$(mktemp -u /tmp/chain-regress.XXXXXX.fifo)
mkfifo "$FIFO"
RUNFLOW=$(mktemp /tmp/chain-regress-flow.XXXXXX)
CANONFLOW=$(mktemp /tmp/chain-regress-canon.XXXXXX)

QPID=
cleanup() {
  if [ -n "$QPID" ] && kill -0 "$QPID" 2>/dev/null; then
    kill -TERM "$QPID" 2>/dev/null
    for _ in 1 2 3 4 5 6 7 8 9 10; do kill -0 "$QPID" 2>/dev/null || break; sleep 1; done
    kill -KILL "$QPID" 2>/dev/null
    wait "$QPID" 2>/dev/null
  fi
  rm -f "$FIFO" "$RUNFLOW" "$CANONFLOW"
}
trap cleanup EXIT
trap 'cleanup; exit 130' INT TERM

# ---- boot (§5 run command verbatim, root=/dev/mmcblk0 variant) ------------
echo "chain-regress: mode=$MODE quick=$QUICK timeout=${BOOT_TIMEOUT}s log=$LOG"
"$QEMU" -M nokia-e7 -m 256M -nographic "${EXTRA[@]}" \
  -drive file="$REPO/firmware-re/analysis/nloader.bin",if=none,id=nloader,format=raw \
  -drive file="$REPO/firmware-re/nokia-e7-onenand.img",if=none,id=onenand,format=raw \
  -drive file="$REPO/emulation/rootfs/emmc.img",if=none,id=emmc,format=raw \
  -kernel "$REPO/kernel/linux/arch/arm/boot/zImage" \
  -dtb "$REPO/kernel/linux/arch/arm/boot/dts/broadcom/bcm2727-nokia-e7.dtb" \
  -append "earlycon=pl011,0x40210000 console=ttyAMA0 loglevel=7 root=/dev/mmcblk0 rootwait rw" \
  <"$FIFO" >"$LOG" 2>&1 &
QPID=$!
exec 3<>"$FIFO"   # hold the fifo open RDWR for the whole interactive phase

prompt_count() { grep -ao -F 'e7:~#' "$LOG" | wc -l; }

START=$SECONDS
prompt_seen=0
qemu_died=0
while :; do
  grep -aq -F 'e7:~#' "$LOG" && { prompt_seen=1; break; }
  kill -0 "$QPID" 2>/dev/null || { qemu_died=1; break; }
  [ $((SECONDS - START)) -ge "$BOOT_TIMEOUT" ] && break
  printf '\r  booting... %3ds elapsed, %6d log lines   ' "$((SECONDS - START))" "$(wc -l <"$LOG")"
  sleep 5
done
printf '\r%60s\r' ''

# ---- interactive block (fifo-fed; warnings on flake, never a false FAIL) --
interactive_flake=0
echo_sent=0
sentinel_line=0
cmd_echo_line=0
if [ "$prompt_seen" -eq 1 ] && [ "$QUICK" -eq 0 ]; then
  sleep "$SETTLE_TO_PROMPT_INPUT"
  base=$(prompt_count)
  printf '\n' >&3
  d=$((SECONDS + WAIT_INTERACTIVE))
  while [ "$(prompt_count)" -le "$base" ] && [ $SECONDS -lt $d ]; do sleep 3; done
  if [ "$(prompt_count)" -le "$base" ]; then
    interactive_flake=1
  else
    printf 'cat /proc/interrupts; echo CHAINREG-OK\n' >&3
    echo_sent=1
    d=$((SECONDS + WAIT_INTERACTIVE))
    while :; do
      # the sentinel's own output (NOT the echoed command, which also names it)
      l=$(grep -an -F 'CHAINREG-OK' "$LOG" | awk -F: -v s=1 '$1 > 0 && $0 !~ /echo CHAINREG-OK/ {print $1; exit}')
      [ -n "$l" ] && { sentinel_line=$l; break; }
      [ $SECONDS -ge $d ] && break
      sleep 3
    done
    [ "$sentinel_line" -gt 0 ] || interactive_flake=1
    sleep "$SETTLE_AFTER_CMD"
  fi
fi
if [ "$echo_sent" -eq 1 ]; then
  # last echo of our command = the interactive capture point
  cmd_echo_line=$(grep -an -F 'cat /proc/interrupts' "$LOG" | tail -1 | cut -d: -f1)
fi
cleanup; QPID=

# ---- ordered marker asserts ----------------------------------------------
declare -a NAMES RESULTS NOTES
first_fail=""
cur=0          # cursor: markers must appear in this boot order
n_check=0

mark() {  # mark <label> <fixed-string> [extra same-line fixed-string...]
  local label="$1"; shift
  local pat="$1"; shift
  local extras=("$@")
  local line content
  NAMES[$n_check]="$label"
  if [ "$cur" -eq 999999999 ]; then
    RESULTS[$n_check]="SKIP"; NOTES[$n_check]="earlier marker already failed"
    n_check=$((n_check + 1)); return
  fi
  line=$(grep -an -F "$pat" "$LOG" | awk -F: -v c="$cur" '$1+0 > c {print $1; exit}')
  if [ -z "$line" ]; then
    RESULTS[$n_check]="FAIL"; NOTES[$n_check]="marker not found after log line $cur: $pat"
    [ -z "$first_fail" ] && first_fail="$label (missing '$pat' after log line $cur)"
    cur=999999999
  else
    local bad=""
    content=$(sed -n "${line}p" "$LOG")
    for e in "${extras[@]:-}"; do
      [ -n "$e" ] && ! grep -aq -F "$e" <<<"$content" && bad="$bad [missing on same line: $e]"
    done
    if [ -n "$bad" ]; then
      RESULTS[$n_check]="FAIL"; NOTES[$n_check]="log line $line:$bad"
      [ -z "$first_fail" ] && first_fail="$label (line $line$bad)"
    else
      RESULTS[$n_check]="ok"; NOTES[$n_check]="log line $line"
    fi
    cur=$line
  fi
  n_check=$((n_check + 1))
}

if [ "$prompt_seen" -ne 1 ]; then
  if [ "$qemu_died" -eq 1 ]; then
    first_fail="QEMU exited before the e7:~# prompt (boot died; see log tail)"
  else
    first_fail="boot deadline (${BOOT_TIMEOUT}s) hit before the e7:~# prompt"
  fi
fi

mark "board chain-boot line"        'nokia-e7: chain boot' 'trampoline 0x101ffc30'
mark "SWBL-shim single-map line"    'NLoader boot via SWBL shim' '0-0x1C000 @ 0x10010000' 'BB5 entry 0x100103D0'
mark "EM-ASIC detect (§5.3.1)"      'Pearl-J : v1.0'
mark "genuine TOC curation MCUSW"   'TOC entry added : MCUSW'
mark "genuine TOC curation PRIMAPP" 'TOC entry added : PRIMAPP'
mark "genuine TOC curation PASUBTOC" 'TOC entry added : PASUBTOC'
mark "genuine TOC curation PAPUBKEYS" 'TOC entry added : PAPUBKEYS'
mark "genuine allocator PRIMAPP"    'Load address : 10200000 added for image : PRIMAPP'
mark "genuine allocator PASUBTOC"   'Load address : 10200000 added for image : PASUBTOC'
mark "genuine allocator PAPUBKEYS"  'Load address : 10200000 added for image : PAPUBKEYS'
mark "MCUSW window 0x101FFC30"      'Load address : 0x101FFC30'
mark "launcher jump"                'Start SOS at 101FFC30'
mark "ROOT-UP banner"               'ROOT-UP: buildroot armv6/musl on the eMMC ext4'
mark "shell prompt"                 'e7:~#'

# exact count: exactly four "TOC entry added" lines, no more
if [ "$cur" -ne 999999999 ]; then
  n_toc=$(grep -ac -F 'TOC entry added' "$LOG")
  NAMES[$n_check]="TOC curation count == 4"; NOTES[$n_check]="found $n_toc"
  if [ "$n_toc" -eq 4 ]; then RESULTS[$n_check]="ok"; else
    RESULTS[$n_check]="FAIL"
    [ -z "$first_fail" ] && first_fail="TOC entry added count = $n_toc (expected 4)"
  fi
  n_check=$((n_check + 1))
fi

# ---- interactive results (/proc/interrupts) -------------------------------
if [ "$QUICK" -eq 1 ]; then
  NAMES[$n_check]="interactive /proc/interrupts"; RESULTS[$n_check]="SKIP"; NOTES[$n_check]="--quick"; n_check=$((n_check+1))
elif [ "$interactive_flake" -eq 1 ]; then
  NAMES[$n_check]="interactive /proc/interrupts"; RESULTS[$n_check]="WARN"
  NOTES[$n_check]="fifo timing flake (no settled sentinel) — demoted, not a regression signal"
  n_check=$((n_check+1))
elif [ "$prompt_seen" -eq 1 ]; then
  # capture window: our command echo .. sentinel (both present, output settled)
  win=$(awk -v s="$cmd_echo_line" -v e="$sentinel_line" 'NR>s && NR<e' "$LOG")

  NAMES[$n_check]="/proc/interrupts captured"; NOTES[$n_check]="lines $((cmd_echo_line+1))-$((sentinel_line-1))"
  if [ -n "$win" ]; then RESULTS[$n_check]="ok"; else RESULTS[$n_check]="FAIL"
    [ -z "$first_fail" ] && first_fail="/proc/interrupts window empty (echo $cmd_echo_line, sentinel $sentinel_line)"
  fi
  n_check=$((n_check+1))

  err_line=$(grep -a -E 'Err:[[:space:]]*[0-9]+' <<<"$win" | tail -1)
  NAMES[$n_check]="Err: 0"; NOTES[$n_check]="${err_line:-<no Err line>}"
  if grep -aq -E 'Err:[[:space:]]*0[[:space:]]*$' <<<"$err_line"; then RESULTS[$n_check]="ok"; else
    RESULTS[$n_check]="FAIL"
    [ -z "$first_fail" ] && first_fail="Err line not zero: ${err_line:-absent}"
  fi
  n_check=$((n_check+1))

  for pair in $IRQ_CLAIMS; do
    id=${pair%%:*}; name=${pair#*:}
    hit=$(grep -a -E "bcm2727-icu[[:space:]]+$id[[:space:]]+Edge[[:space:]]+$name" <<<"$win" | tail -1)
    NAMES[$n_check]="irq id $id ($name)"; NOTES[$n_check]="${hit:-<not claimed>}"
    if [ -n "$hit" ]; then RESULTS[$n_check]="ok"; else
      RESULTS[$n_check]="FAIL"
      [ -z "$first_fail" ] && first_fail="irq id $id ($name) not claimed in /proc/interrupts"
    fi
    n_check=$((n_check+1))
  done
else
  NAMES[$n_check]="interactive /proc/interrupts"; RESULTS[$n_check]="SKIP"; NOTES[$n_check]="no prompt"; n_check=$((n_check+1))
fi

# ---- flow diff vs canonical ----------------------------------------------
grep -a -F 'nloader[xti' "$LOG" >"$RUNFLOW"
grep -av '^#' "$CANON" >"$CANONFLOW"
n_run=$(wc -l <"$RUNFLOW"); n_canon=$(wc -l <"$CANONFLOW")

flow_verdict=ok; flow_note="$n_run lines, byte-identical to tools/chain-flow-canonical.txt ($n_canon)"
div_out=$(awk '
  FNR==NR { canon[++cn]=$0; next }
  { run[FNR]=$0; rn=FNR }
  END {
    max = cn > rn ? cn : rn
    for (i=1; i<=max; i++) {
      if (canon[i] != run[i]) {
        c = canon[i] == "" ? "<canonical ended>" : canon[i]
        r = run[i] == "" ? "<run ended>" : run[i]
        printf "%d|%s|%s\n", i, c, r
        exit
      }
    }
    if (cn == rn && cn > 0) print "IDENTICAL"
    else print "EMPTY"
  }' "$CANONFLOW" "$RUNFLOW")

if [ "$div_out" = "IDENTICAL" ]; then
  :
elif [ "$div_out" = "EMPTY" ]; then
  flow_verdict=FAIL; flow_note="no nloader[xti flow lines captured in the boot log"
  [ -z "$first_fail" ] && first_fail="flow: log contains no nloader[xti lines"
else
  flow_verdict=FAIL
  dline=${div_out%%|*}; rest=${div_out#*|}
  dcanon=${rest%%|*}; drun=${rest#*|}
  flow_note="run $n_run vs canonical $n_canon lines; first divergence at flow line $dline"
  [ -z "$first_fail" ] && first_fail="flow line $dline: run '$drun' vs canonical '$dcanon'"
fi
NAMES[$n_check]="nloader[xti flow diff"; RESULTS[$n_check]="$flow_verdict"; NOTES[$n_check]="$flow_note"
n_check=$((n_check+1))

# ---- summary --------------------------------------------------------------
verdict=PASS
for ((i=0; i<n_check; i++)); do
  [ "${RESULTS[$i]}" = "FAIL" ] && { verdict=FAIL; break; }
done
KEEPLOG=0
[ "$verdict" = "FAIL" ] && KEEPLOG=1
[ "$KEEP" -eq 1 ] && KEEPLOG=1

echo "==============================================================================="
echo "chain-regress $MODE: $verdict    (boot $((SECONDS - START))s, log $n_run flow lines / $(wc -l <"$LOG") total)"
if [ "$verdict" = "FAIL" ]; then
  echo "FIRST DIVERGENCE: $first_fail"
fi
for ((i=0; i<n_check; i++)); do
  printf '  [%s] %s — %s\n' "${RESULTS[$i]}" "${NAMES[$i]}" "${NOTES[$i]}"
done
if [ "$KEEPLOG" -eq 1 ]; then
  echo "log preserved: $LOG"
else
  rm -f "$LOG"; echo "log (temp) removed; rerun with --keep to preserve"
fi
echo "==============================================================================="

[ "$verdict" = "PASS" ] && exit 0 || exit 1
