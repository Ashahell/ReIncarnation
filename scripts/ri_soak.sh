#!/bin/bash
# ri_soak.sh — Task-14 time-boxed host soak (shared tooling, gate G14).
# usage: ri_soak.sh SECONDS [OUTDIR]   (OUTDIR default /tmp/ri/soak)
#
# What it does: builds the host tree, runs tools/bench once (worst-case
# DSP blocks, determinism-checked inside bench), then loops the render
# CLI over the worst-case fixture set until the deadline:
#   808 storm, mix four, fx chain, pcf sweep, 909pack bd,
#   corpus s01..s10 round-robin (pattern/mod switching under load),
#   sched-check tempo variants 140/174/90 (tempo switching under load).
# Every render's md5 must equal its first-pass pin; any nonzero exit or
# md5 drift counts one UNDERRUN event. Prints machine + compiler flags +
# durations + iteration/underrun counts; writes SOAK.log in OUTDIR.
# Exit 0 iff zero underruns.
#
# Honesty scope: this is an OFFLINE soak — it proves determinism +
# throughput (headroom) under switching load. DEVICE underruns (AHI
# callback deadlines on the named box) need the M1.1 run; method in
# docs/evidence/formats/beta-exit.md. The underrun counter here is
# failed-or-nondeterministic renders, and it reads 0 by assertion.
set -u
ROOT="$(dirname "$0")/.."
SECS="${1:-60}"
OUT="${2:-/tmp/ri/soak}"
case "$SECS" in
  ''|*[!0-9]*) echo "usage: $0 SECONDS [OUTDIR]"; exit 2 ;;
esac
test "$SECS" -ge 5 || { echo "soak: SECONDS >= 5"; exit 2; }
mkdir -p "$OUT/pass1" "$OUT/cur"
LOG="$OUT/SOAK.log"
: > "$LOG"
exec > >(tee -a "$LOG") 2>&1

CFLAGS="-std=c99 -O2 -Wall -Wextra -Werror -pedantic -ffp-contract=off -fno-unsafe-math-optimizations -ftrapv -I$ROOT"
echo "SOAK machine: $(uname -srmo) nproc=$(nproc) gcc=$(gcc -dumpfullversion) flags=$CFLAGS"
echo "SOAK deadline: ${SECS}s out=$OUT date=$(date -u +%FT%TZ)"

bash "$ROOT/scripts/ri_build_host.sh" all >/dev/null || { echo "SOAK FAIL: host build"; exit 1; }
BOUT=/tmp/ri/build
gcc $CFLAGS -o "$BOUT/bench" "$ROOT/tools/bench.c" "$BOUT"/*.o -lm || { echo "SOAK FAIL: bench build"; exit 1; }
gcc $CFLAGS -o "$BOUT/render" "$ROOT/tools/render.c" "$BOUT"/*.o -lm || { echo "SOAK FAIL: render build"; exit 1; }
R="$BOUT/render"

echo "-- bench worst-case (${RI_SOAK_BENCH_BLOCKS:-45000} blocks) --"
T0=$(date +%s)
"$BOUT/bench" "${RI_SOAK_BENCH_BLOCKS:-45000}" || { echo "SOAK FAIL: bench (nondeterministic)"; exit 1; }
T1=$(date +%s)
echo "SOAK bench wall: $((T1 - T0))s"

# Tempo variants of the text scaffold (switching under load).
sed 's/^tempo=.*/tempo=174/' "$ROOT/tests/golden/songs/sched-check.rbng" > "$OUT/sched-174.rbng"
sed 's/^tempo=.*/tempo=90/' "$ROOT/tests/golden/songs/sched-check.rbng" > "$OUT/sched-90.rbng"

# Fixture list: "tag|args..." — args reference $R inputs, OUTF filled per pass.
FIX="storm|--808 storm
mix4|--mix four
fxchain|--fx chain
pcfsweep|--pcf sweep
packbd|--909pack bd
sched140|--song $ROOT/tests/golden/songs/sched-check.rbng
sched174|--song $OUT/sched-174.rbng
sched90|--song $OUT/sched-90.rbng"
CORPUS="01 02 03 04 05 06 07 08 09 10"

render_one() { # $1=tag $2+=render args $3=outwav ; prints md5
  local tag="$1" out="$2"
  shift 2
  "$R" "$@" --out "$out" >/dev/null 2>&1 || return 1
  md5sum "$out" | cut -d' ' -f1
}

echo "-- first pass (pins) --"
UND=0
IT=0
declare -A PIN
while IFS= read -r line; do
  tag="${line%%|*}"
  args="${line#*|}"
  # shellcheck disable=SC2086
  md=$(render_one "$tag" "$OUT/pass1/$tag.wav" $args) || { echo "SOAK UNDERRUN: first-pass $tag failed"; UND=$((UND + 1)); continue; }
  PIN[$tag]="$md"
done <<< "$FIX"
for k in $CORPUS; do
  md=$(render_one "corp$k" "$OUT/pass1/corp$k.wav" --rbngsong "$ROOT/tests/golden/songs/corpus/s$k.rbng") \
    || { echo "SOAK UNDERRUN: first-pass corp$k failed"; UND=$((UND + 1)); continue; }
  PIN[corp$k]="$md"
done

echo "-- loop until deadline --"
END=$(( $(date +%s) + SECS ))
SLOWEST="none 0"
while [ "$(date +%s)" -lt "$END" ]; do
  IT=$((IT + 1))
  while IFS= read -r line; do
    tag="${line%%|*}"
    args="${line#*|}"
    S0=$(date +%s%N)
    # shellcheck disable=SC2086
    md=$(render_one "$tag" "$OUT/cur/$tag.wav" $args) || { echo "SOAK UNDERRUN: iter $IT $tag render failed"; UND=$((UND + 1)); continue; }
    S1=$(date +%s%N)
    ms=$(( (S1 - S0) / 1000000 ))
    test "$ms" -gt "$(echo "$SLOWEST" | cut -d' ' -f2)" 2>/dev/null && SLOWEST="$tag $ms"
    test "${PIN[$tag]:-x}" = "$md" || { echo "SOAK UNDERRUN: iter $IT $tag md5 drift"; UND=$((UND + 1)); }
  done <<< "$FIX"
  for k in $CORPUS; do
    md=$(render_one "corp$k" "$OUT/cur/corp$k.wav" --rbngsong "$ROOT/tests/golden/songs/corpus/s$k.rbng") \
      || { echo "SOAK UNDERRUN: iter $IT corp$k render failed"; UND=$((UND + 1)); continue; }
    test "${PIN[corp$k]:-x}" = "$md" || { echo "SOAK UNDERRUN: iter $IT corp$k md5 drift"; UND=$((UND + 1)); }
  done
  # Event-density max: corpus s10 carries the MODR warn path; assert the
  # prompt still fires under load (switching must not mute it).
  "$R" --rbngsong "$ROOT/tests/golden/songs/corpus/s10.rbng" --out "$OUT/cur/modr.wav" >"$OUT/cur/modr.log" 2>&1 \
    || { echo "SOAK UNDERRUN: iter $IT modr render failed"; UND=$((UND + 1)); }
  grep -q "MODR: mod 'acid-01'" "$OUT/cur/modr.log" || { echo "SOAK UNDERRUN: iter $IT MODR prompt silent"; UND=$((UND + 1)); }
done

echo "SOAK iterations=$IT underruns=$UND slowest_render_ms=($SLOWEST)"
if [ "$UND" -ne 0 ]; then
  echo "SOAK FAIL: $UND underrun events"
  exit 1
fi
echo "SOAK PASS: $IT iterations, 0 underruns"
