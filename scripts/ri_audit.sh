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
bash "$ROOT/scripts/ri_build_host.sh" all >/dev/null || { echo "FAIL: host build"; exit 1; }
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
echo "== Phase 6: W1 one-renderer proof (Task 6, gate G6) =="
test -f "$ROOT/audio_io/audio.h" || { echo "FAIL: missing audio_io/audio.h"; exit 1; }
test -f "$ROOT/audio_io/audio.c" || { echo "FAIL: missing audio_io/audio.c"; exit 1; }
test -f "$ROOT/audio_io/backend_null.c" || { echo "FAIL: missing audio_io/backend_null.c"; exit 1; }
for decl in "AuCreateObject(struct Library \*AudioBase, struct TagItem \*tags)" \
  "AuAddSource(struct AudioObject \*ao, struct TagItem \*tags)" \
  "AuAddBus(struct AudioObject \*ao, const char \*name, struct TagItem \*tags)" \
  "AuConnect(struct AudioObject \*ao, uint32_t src, uint32_t bus)" \
  "AuStart(struct AudioObject \*ao)" \
  "AuStop(struct AudioObject \*ao)" \
  "AuQueryAttr(struct AudioObject \*ao, uint32_t attr)" \
  "AuRenderToFile(struct AudioObject \*ao, const char \*path, uint32_t ms)"; do
  grep -q "$decl" "$ROOT/audio_io/audio.h" || { echo "FAIL: audio.h lacks: $decl"; exit 1; }
done
grep -q '#define RI_DEVICE_FRAMES 256u' "$ROOT/audio_io/audio.h" || { echo "FAIL: device-frames default not 256"; exit 1; }
grep -q 'audio: AHI unavailable - null backend active (offline render only)' "$ROOT/audio_io/audio.h" || { echo "FAIL: fallback string drifted"; exit 1; }
if grep -rn "malloc\|calloc\|realloc\|Forbid\|Disable(" "$ROOT/audio_io/audio.c" "$ROOT/audio_io/backend_null.c" 2>/dev/null; then echo "FAIL: banned construct in W1 backend"; exit 1; fi
mkdir -p /tmp/ri/run/t6 # t6 test writes its scaffold here; clean wipes /tmp/ri
bash "$ROOT/scripts/ri_build_host.sh" test t6_w1backend >/dev/null || { echo "FAIL: t6_w1backend"; exit 1; }
gcc $CFLAGS -o "$OUT/compare" "$ROOT/tools/compare.c" || { echo "FAIL: compare build"; exit 1; }
T6=/tmp/ri/run/t6
"$OUT/compare" --events-a "$T6/file.events" --events-b "$T6/live.events" --wav-a "$T6/file.wav" --wav-b "$T6/live.wav" | grep -q "COMPARE: IDENTICAL" || { echo "FAIL: file-vs-live differ (not one renderer)"; exit 1; }
echo "-- AROS compile of backend TUs (compile-only, no link) --"
if [ ! -f ../Vulkan4Aros/scripts/aros_build_env.sh ]; then echo "FAIL: Vulkan4AROS tree (toolchain source) not found"; exit 1; fi
. ../Vulkan4Aros/scripts/aros_build_env.sh
export PATH="$AROS_TOOLCHAIN:$PATH"
SDK="$AROS_SDK_INCLUDE"
CFLAGS_AU="-std=c99 -O2 -Wall -Wextra -Werror -mcmodel=large -mno-red-zone -ffixed-r12 -I$ROOT -I$SDK -I$SDK/aros/posixc -I$SDK/aros/stdc"
mkdir -p "$OUT/aros" # AROS objects stay out of $OUT: `test` links $OUT/*.o (host)
x86_64-aros-gcc $CFLAGS_AU -c "$ROOT/audio_io/audio.c" -o "$OUT/aros/audio_aros.o" || { echo "FAIL: audio.c AROS compile"; exit 1; }
x86_64-aros-gcc $CFLAGS_AU -c "$ROOT/audio_io/backend_null.c" -o "$OUT/aros/backend_null_aros.o" || { echo "FAIL: backend_null.c AROS compile"; exit 1; }
echo "== Phase 7: sched shuffle/legato/flam (Task 7, gate G7) =="
bash "$ROOT/scripts/ri_build_host.sh" test t1_sched >/dev/null || { echo "FAIL: t1_sched"; exit 1; }
test -f "$ROOT/docs/evidence/sequencer/flam-default.md" || { echo "FAIL: missing P-05 ledger row"; exit 1; }
grep -q "P-05" "$ROOT/docs/evidence/sequencer/flam-default.md" || { echo "FAIL: ledger row lacks P-05"; exit 1; }
grep -q "35.0" "$ROOT/docs/evidence/sequencer/flam-default.md" || { echo "FAIL: ledger row lacks measured default"; exit 1; }
SG="$ROOT/tests/golden/songs"
test -f "$SG/sched-check.rbng" || { echo "FAIL: missing sched-check song"; exit 1; }
test -f "$SG/sched-check.events" || { echo "FAIL: missing sched-check event golden"; exit 1; }
test -f "$SG/sched-check.wav" || { echo "FAIL: missing sched-check wav golden"; exit 1; }
test -f "$SG/sched-check.wav.sha256" || { echo "FAIL: missing sched-check sidecar"; exit 1; }
(cd "$ROOT" && sha256sum -c tests/golden/songs/sched-check.wav.sha256) || { echo "FAIL: sched-check sha256 mismatch"; exit 1; }
T7=/tmp/ri/run/audit7
mkdir -p "$T7"
"$OUT/render" --song "$SG/sched-check.rbng" --out "$T7/sched-check.wav" --dump-events "$T7/sched-check.events" || exit 1
"$OUT/compare" --events-a "$SG/sched-check.events" --events-b "$T7/sched-check.events" --wav-a "$SG/sched-check.wav" --wav-b "$T7/sched-check.wav" | grep -q "COMPARE: IDENTICAL" || { echo "FAIL: sched-check re-render differs (not deterministic)"; exit 1; }
echo "== Phase 8: 808 fifteen voices (Task 8, gate G8) =="
bash "$ROOT/scripts/ri_build_host.sh" test t1_808 >/dev/null || { echo "FAIL: t1_808"; exit 1; }
for v in bd sd lt mt ht lc mc hc rs cl cp ch oh cy cb; do
  test -f "$ROOT/docs/evidence/808/$v.md" || { echo "FAIL: missing ledger 808/$v.md"; exit 1; }
  grep -q "EXCITE" "$ROOT/docs/evidence/808/$v.md" || { echo "FAIL: ledger $v lacks accent mapping"; exit 1; }
done
grep -q "P-12" "$ROOT/docs/evidence/808/bd.md" || { echo "FAIL: P-12 open state unrecorded"; exit 1; }
SG8="$ROOT/tests/golden/808"
for v in bd sd lt mt ht lc mc hc rs cl cp ch oh cy cb storm; do
  test -f "$SG8/$v.wav" || { echo "FAIL: missing golden 808/$v.wav"; exit 1; }
  test -f "$SG8/$v.wav.sha256" || { echo "FAIL: missing sidecar 808/$v.wav.sha256"; exit 1; }
done
(cd "$ROOT" && sha256sum -c tests/golden/808/bd.wav.sha256 tests/golden/808/sd.wav.sha256 tests/golden/808/lt.wav.sha256 tests/golden/808/mt.wav.sha256 tests/golden/808/ht.wav.sha256 tests/golden/808/lc.wav.sha256 tests/golden/808/mc.wav.sha256 tests/golden/808/hc.wav.sha256 tests/golden/808/rs.wav.sha256 tests/golden/808/cl.wav.sha256 tests/golden/808/cp.wav.sha256 tests/golden/808/ch.wav.sha256 tests/golden/808/oh.wav.sha256 tests/golden/808/cy.wav.sha256 tests/golden/808/cb.wav.sha256 tests/golden/808/storm.wav.sha256) || { echo "FAIL: 808 golden sha256 mismatch"; exit 1; }
T8=/tmp/ri/run/audit8
mkdir -p "$T8"
for v in bd sd lt mt ht lc mc hc rs cl cp ch oh cy cb storm; do
  "$OUT/render" --808 "$v" --out "$T8/$v.wav" >/dev/null || exit 1
  cmp -s "$SG8/$v.wav" "$T8/$v.wav" || { echo "FAIL: 808/$v re-render differs (not deterministic)"; exit 1; }
done
echo "AUDIT 0/0 PASS"
