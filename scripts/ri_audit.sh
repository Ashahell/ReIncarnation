#!/bin/bash
set -e
ROOT="$(dirname "$0")/.."
echo "== Phase 0a: render-path hygiene =="
if grep -rn "malloc\|calloc\|realloc\|free(\|Forbid\|Disable(" "$ROOT/engine/" 2>/dev/null; then echo "FAIL: banned construct in engine/"; exit 1; fi
echo "== Phase 0b: no platform transcendentals/FMA in engine/ =="
if grep -rn "tanhf\|sinf\|cosf\|expf\|powf\|fmodf\|mul_add\|ffast-math" "$ROOT/engine/" 2>/dev/null | grep -v "ri_tanh\|ri_exp\|ri_sin\|ri_pow2"; then echo "FAIL"; exit 1; fi
echo "== Phase 0c: evidence dirs =="
for d in 303 808 909 pcf sequencer gui formats; do test -d "$ROOT/docs/evidence/$d" || { echo "FAIL: missing $d"; exit 1; }; done
echo "== Phase 1: first-light goldens (Task 4, gate G4) =="
G="$ROOT/tests/golden/303"
for f in math-dc math-sine first-light; do
  test -f "$G/$f.wav" || { echo "FAIL: missing golden $f.wav (missing-is-broken)"; exit 1; }
  test -f "$G/$f.wav.sha256" || { echo "FAIL: missing sidecar $f.wav.sha256"; exit 1; }
done
test -f "$G/first-light.events" || { echo "FAIL: missing event golden"; exit 1; }
test -f "$G/first-light.rbng" || { echo "FAIL: missing song scaffold"; exit 1; }
(cd "$ROOT" && sha256sum -c tests/golden/303/math-dc.wav.sha256 tests/golden/303/math-sine.wav.sha256 tests/golden/303/first-light.wav.sha256) || { echo "FAIL: golden sha256 mismatch"; exit 1; }
echo "-- math unit goldens re-verified --"
bash "$ROOT/scripts/ri_build_host.sh" test t1_303math >/dev/null || { echo "FAIL: t1_303math"; exit 1; }
bash "$ROOT/scripts/ri_build_host.sh" test t1_303walk >/dev/null || { echo "FAIL: t1_303walk"; exit 1; }
echo "-- re-render-compare --"
bash "$ROOT/scripts/ri_build_host.sh" all >/dev/null || { echo "FAIL: host build"; exit 1; }
OUT=/tmp/ri/build
CFLAGS="-std=c99 -O2 -Wall -Wextra -Werror -pedantic -ffp-contract=off -fno-unsafe-math-optimizations -ftrapv -I$ROOT"
gcc $CFLAGS -o "$OUT/render" "$ROOT/tools/render.c" "$OUT"/*.o || { echo "FAIL: render build"; exit 1; }
A=/tmp/ri/run/audit
mkdir -p "$A"
"$OUT/render" --math dc --out "$A/math-dc.wav" || exit 1
"$OUT/render" --math sine --out "$A/math-sine.wav" || exit 1
"$OUT/render" --song "$G/first-light.rbng" --out "$A/first-light.wav" --dump-events "$A/first-light.events" || exit 1
for f in math-dc math-sine first-light; do cmp -s "$G/$f.wav" "$A/$f.wav" || { echo "FAIL: re-render $f differs"; exit 1; }; done
cmp -s "$G/first-light.events" "$A/first-light.events" || { echo "FAIL: re-render events differ"; exit 1; }
echo "-- ledger-before-code --"
t_led=$(git -C "$ROOT" log --diff-filter=A --format=%at -- docs/evidence/303/filter-candidate.md | tail -1)
t_code=$(git -C "$ROOT" log --diff-filter=A --format=%at -- engine/dsp/rb303.c | tail -1)
test -n "$t_led" && test -n "$t_code" || { echo "FAIL: add-timestamps missing"; exit 1; }
test "$t_led" -lt "$t_code" || { echo "FAIL: ledger $t_led not before code $t_code"; exit 1; }
c_led=$(git -C "$ROOT" log --diff-filter=A --format=%H -- docs/evidence/303/filter-candidate.md | tail -1)
c_code=$(git -C "$ROOT" log --diff-filter=A --format=%H -- engine/dsp/rb303.c | tail -1)
test "$c_led" != "$c_code" || { echo "FAIL: ledger and code added in one commit"; exit 1; }
git -C "$ROOT" merge-base --is-ancestor "$c_led" "$c_code" || { echo "FAIL: ledger commit not ancestor of code commit"; exit 1; }
echo "== Phase 5: M1.1 probe hygiene (Task 5, gate G5) =="
test -f "$ROOT/audio_io/probe_ahi.c" || { echo "FAIL: missing audio_io/probe_ahi.c"; exit 1; }
grep -q "#ifndef __AROS__" "$ROOT/audio_io/probe_ahi.c" || { echo "FAIL: probe lacks __AROS__ guard"; exit 1; }
grep -q '#error "probe_ahi.c is AROS-only' "$ROOT/audio_io/probe_ahi.c" || { echo "FAIL: probe lacks AROS-only #error"; exit 1; }
if grep -rn "probe_ahi" "$ROOT/scripts/ri_build_host.sh" 2>/dev/null; then echo "FAIL: probe leaks into host build"; exit 1; fi
bash "$ROOT/scripts/ri_build_aros.sh" >/dev/null || { echo "FAIL: AROS build (stub+probe)"; exit 1; }
test -f /tmp/ri/aros/probe_ahi || { echo "FAIL: probe_ahi artifact missing"; exit 1; }
echo "AUDIT 0/0 PASS"
