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
echo "-- ABIv1 LVO convention gate (2026-09-21 probe_ahi guest page-fault) --"
echo "-- Guest binaries MUST emit rdx-base calls (build-pc SDK); any"
echo "-- 'mov %rax,%r12' means the stale r12 SDK leaked in and the binary"
echo "-- will fault on first LVO call. Checked on every AROS artifact. --"
for _ab in /tmp/ri/aros/probe_ahi /tmp/ri/aros/reincarnation_stub.library; do
  _r12=$(x86_64-aros-objdump -d "$_ab" 2>/dev/null | grep -c 'mov[[:space:]]*%rax,%r12' || true)
  test "$_r12" = "0" || { echo "FAIL: stale r12-convention calls in $_ab ($_r12 found)"; exit 1; }
done
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
grep -q '#define RI_DEVICE_FRAMES 64u' "$ROOT/audio_io/audio.h" || { echo "FAIL: device-frames not M1.1-measured 64"; exit 1; }
grep -q 'audio: AHI unavailable - null backend active (offline render only)' "$ROOT/audio_io/audio.h" || { echo "FAIL: fallback string drifted"; exit 1; }
if grep -rn "malloc\|calloc\|realloc\|Forbid\|Disable(" "$ROOT/audio_io/audio.c" "$ROOT/audio_io/backend_null.c" 2>/dev/null; then echo "FAIL: banned construct in W1 backend"; exit 1; fi
echo "-- live-scope tripwire (I1: au_render_frames is first-light only) --"
if ! grep -q "RI_LIVE_FULL_GRAPH_UNIMPLEMENTED" "$ROOT/audio_io/audio.c"; then echo "FAIL: live-scope marker missing from audio.c (full-graph live rendering still unwired — spec §5; removing the marker requires wiring the shared engine core)"; exit 1; fi
mkdir -p /tmp/ri/run/t6 # t6 test writes its scaffold here; clean wipes /tmp/ri
bash "$ROOT/scripts/ri_build_host.sh" test t6_w1backend >/dev/null || { echo "FAIL: t6_w1backend"; exit 1; }
gcc $CFLAGS -o "$OUT/compare" "$ROOT/tools/compare.c" || { echo "FAIL: compare build"; exit 1; }
T6=/tmp/ri/run/t6
"$OUT/compare" --events-a "$T6/file.events" --events-b "$T6/live.events" --wav-a "$T6/file.wav" --wav-b "$T6/live.wav" | grep -q "COMPARE: IDENTICAL" || { echo "FAIL: file-vs-live differ (not one renderer)"; exit 1; }
echo "-- AROS compile of backend TUs (compile-only, no link) --"
if [ ! -f ../Vulkan4Aros/scripts/aros_build_env.sh ]; then echo "FAIL: Vulkan4AROS tree (toolchain source) not found"; exit 1; fi
. ../Vulkan4Aros/scripts/aros_build_env.sh
export PATH="$AROS_TOOLCHAIN:$PATH"
# ABIv1 SDK (2026-09-21): v1 build-pc tree, never the env default (r12 convention).
V1SDK_ABS="$(cd "$ROOT/../Vulkan4Aros/src/abi/v1/core-pc-x86_64/bin/pc-x86_64/AROS/Developer/include" && pwd)"
if [ -d "$V1SDK_ABS" ]; then SDK="$V1SDK_ABS"; else echo "FAIL: v1 build-pc SDK absent"; exit 1; fi
CFLAGS_AU="-std=c99 -O2 -Wall -Wextra -Werror -mcmodel=large -mno-red-zone -ffixed-r12 -I$ROOT -I$SDK -I$SDK/aros/posixc -I$SDK/aros/stdc"
mkdir -p "$OUT/aros" # AROS objects stay out of $OUT: `test` links $OUT/*.o (host)
x86_64-aros-gcc $CFLAGS_AU -c "$ROOT/audio_io/audio.c" -o "$OUT/aros/audio_aros.o" || { echo "FAIL: audio.c AROS compile"; exit 1; }
x86_64-aros-gcc $CFLAGS_AU -c "$ROOT/audio_io/backend_null.c" -o "$OUT/aros/backend_null_aros.o" || { echo "FAIL: backend_null.c AROS compile"; exit 1; }
echo "== Phase 6b: seq master clock 10-min accumulation (WBS 2.1, TC-2.1.1) =="
test -f "$ROOT/engine/seq/riseq.h" || { echo "FAIL: missing engine/seq/riseq.h"; exit 1; }
test -f "$ROOT/engine/seq/riseq.c" || { echo "FAIL: missing engine/seq/riseq.c"; exit 1; }
bash "$ROOT/scripts/ri_build_host.sh" sched >/dev/null || { echo "FAIL: sched build (riseq)"; exit 1; }
bash "$ROOT/scripts/ri_build_host.sh" test t21_seq >/dev/null || { echo "FAIL: t21_seq"; exit 1; }
bash "$ROOT/scripts/ri_build_host.sh" test t21_seqloop >/dev/null || { echo "FAIL: t21_seqloop"; exit 1; }
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
echo "== Phase 9: 909 sampler + clean pack + provenance gate (Task 9, gate G9) =="
bash "$ROOT/scripts/ri_build_host.sh" test t1_909 >/dev/null || { echo "FAIL: t1_909"; exit 1; }
for v in bd sd ch oh cr rd; do
  test -f "$ROOT/docs/evidence/909/$v.md" || { echo "FAIL: missing ledger 909/$v.md"; exit 1; }
  grep -q "Provenance manifest" "$ROOT/docs/evidence/909/$v.md" || { echo "FAIL: ledger $v lacks manifest"; exit 1; }
  grep -q "CC0/RI" "$ROOT/docs/evidence/909/$v.md" || { echo "FAIL: ledger $v lacks license row"; exit 1; }
done
grep -q "P-13" "$ROOT/docs/evidence/909/bd.md" || { echo "FAIL: P-13 unrecorded"; exit 1; }
grep -q "NO-OP" "$ROOT/docs/evidence/909/cr.md" || { echo "FAIL: crash quirk unrecorded"; exit 1; }
grep -q "steal" "$ROOT/docs/evidence/909/ch.md" || { echo "FAIL: hat steal unrecorded"; exit 1; }
gcc $CFLAGS -o "$OUT/inspect" "$ROOT/tools/inspect.c" "$OUT"/*.o || { echo "FAIL: inspect build"; exit 1; }
PK="$ROOT/reference/packs/classic-01"
test -f "$PK/pack.rbnm" || { echo "FAIL: missing clean pack"; exit 1; }
test -f "$PK/MANIFEST.txt" || { echo "FAIL: missing pack manifest"; exit 1; }
"$OUT/inspect" --rbnm "$PK/pack.rbnm" | grep -q "RBNM OK" || { echo "FAIL: pack.rbnm rejected"; exit 1; }
"$OUT/inspect" --manifest "$PK/MANIFEST.txt" | grep -q "MANIFEST OK" || { echo "FAIL: MANIFEST rejected"; exit 1; }
echo "-- manifest-gap negative gates (audit fails on any gap) --"
T9=/tmp/ri/run/audit9
mkdir -p "$T9"
head -1 "$PK/MANIFEST.txt" | sed 's/;map=[^;]*//' > "$T9/gap.manf"
tail -n +2 "$PK/MANIFEST.txt" >> "$T9/gap.manf"
if "$OUT/inspect" --manifest "$T9/gap.manf" >/dev/null 2>&1; then echo "FAIL: gap manifest accepted"; exit 1; fi
head -c 120 "$PK/pack.rbnm" > "$T9/trunc.rbnm"
if "$OUT/inspect" --rbnm "$T9/trunc.rbnm" >/dev/null 2>&1; then echo "FAIL: truncated pack accepted"; exit 1; fi
echo "-- 909 goldens re-verified --"
SG9="$ROOT/tests/golden/909"
for v in bd sd ch oh cr rd; do
  test -f "$SG9/$v.wav" || { echo "FAIL: missing golden 909/$v.wav"; exit 1; }
  test -f "$SG9/$v.wav.sha256" || { echo "FAIL: missing sidecar 909/$v.wav.sha256"; exit 1; }
done
(cd "$ROOT" && sha256sum -c tests/golden/909/bd.wav.sha256 tests/golden/909/sd.wav.sha256 tests/golden/909/ch.wav.sha256 tests/golden/909/oh.wav.sha256 tests/golden/909/cr.wav.sha256 tests/golden/909/rd.wav.sha256) || { echo "FAIL: 909 golden sha256 mismatch"; exit 1; }
for v in bd sd ch oh cr rd; do
  "$OUT/render" --909 "$v" --out "$T9/$v.wav" >/dev/null || exit 1
  cmp -s "$SG9/$v.wav" "$T9/$v.wav" || { echo "FAIL: 909/$v re-render differs (not deterministic)"; exit 1; }
done
echo "-- audibility floor (silent goldens never pin: peak>=1000, rms>=100) --"
for v in bd sd ch oh cr rd; do
  od -An -t d2 -v -j44 "$T9/$v.wav" | awk 'BEGIN { m=0; s=0; n=0 }
    { for (i=1;i<=NF;i++) { a=$i; if (a<0) a=-a; if (a>m) m=a; s+=$i*$i; n++ } }
    END { r=sqrt(s/n); printf "909/%s peak=%d rms=%.0f\n", VN, m, r;
      if (m<1000 || r<100) exit 1 }' VN="$v" || { echo "FAIL: 909/$v silent (audibility floor)"; exit 1; }
done
echo "-- S909 render-diff (pack differs from default) --"
"$OUT/render" --909 bd --out "$T9/dflt.wav" >/dev/null || exit 1
"$OUT/render" --909pack bd --out "$T9/pack.wav" >/dev/null || exit 1
if "$OUT/compare" --events-a "$SG/sched-check.events" --events-b "$SG/sched-check.events" --wav-a "$T9/dflt.wav" --wav-b "$T9/pack.wav" | grep -q "COMPARE: IDENTICAL"; then echo "FAIL: pack renders identical to default (S909 dead)"; exit 1; fi
echo "== Phase 10: PCF black-box route + FX trio (Task 10, gate G10) =="
T10=/tmp/ri/run/audit10
mkdir -p "$T10" /tmp/ri/run/t10
bash "$ROOT/scripts/ri_build_host.sh" test t1_fx >/dev/null || { echo "FAIL: t1_fx"; exit 1; }
test -f "$ROOT/docs/evidence/pcf/engine.md" || { echo "FAIL: missing pcf engine ledger"; exit 1; }
grep -q "P-15" "$ROOT/docs/evidence/pcf/engine.md" || { echo "FAIL: ledger lacks P-15"; exit 1; }
grep -q "OPEN-04" "$ROOT/docs/evidence/pcf/engine.md" || { echo "FAIL: ledger lacks OPEN-04"; exit 1; }
test -f "$ROOT/docs/evidence/pcf/red-t1_fx.txt" || { echo "FAIL: missing RED evidence"; exit 1; }
test -f "$ROOT/reference/pcf-table.bin" || { echo "FAIL: missing pcf-table.bin (missing-is-broken)"; exit 1; }
echo "-- negative gate (no table -> pcf build fails with the #error) --"
mv "$ROOT/reference/pcf-table.bin" "$T10/hide.bin"
if bash "$ROOT/scripts/ri_build_host.sh" pcf >"$T10/neg.log" 2>&1; then
  mv "$T10/hide.bin" "$ROOT/reference/pcf-table.bin"
  echo "FAIL: pcf build accepted a missing table"
  exit 1
fi
mv "$T10/hide.bin" "$ROOT/reference/pcf-table.bin"
grep -q "PCF table unverified" "$T10/neg.log" || { echo "FAIL: pcf failure is not the #error gate"; exit 1; }
bash "$ROOT/scripts/ri_build_host.sh" pcf >/dev/null || { echo "FAIL: pcf rebuild after restore"; exit 1; }
echo "-- pcf goldens re-verified --"
SG10="$ROOT/tests/golden/pcf"
for v in pcf-sweep fx-delay fx-chain; do
  test -f "$SG10/$v.wav" || { echo "FAIL: missing golden pcf/$v.wav"; exit 1; }
  test -f "$SG10/$v.wav.sha256" || { echo "FAIL: missing sidecar pcf/$v.wav.sha256"; exit 1; }
done
(cd "$ROOT" && sha256sum -c tests/golden/pcf/pcf-sweep.wav.sha256 tests/golden/pcf/fx-delay.wav.sha256 tests/golden/pcf/fx-chain.wav.sha256) || { echo "FAIL: pcf golden sha256 mismatch"; exit 1; }
"$OUT/render" --pcf sweep --out "$T10/pcf-sweep.wav" >/dev/null || exit 1
"$OUT/render" --fx delay --out "$T10/fx-delay.wav" >/dev/null || exit 1
"$OUT/render" --fx chain --out "$T10/fx-chain.wav" >/dev/null || exit 1
cmp -s "$SG10/pcf-sweep.wav" "$T10/pcf-sweep.wav" || { echo "FAIL: pcf/pcf-sweep re-render differs (not deterministic)"; exit 1; }
cmp -s "$SG10/fx-delay.wav" "$T10/fx-delay.wav" || { echo "FAIL: pcf/fx-delay re-render differs (not deterministic)"; exit 1; }
cmp -s "$SG10/fx-chain.wav" "$T10/fx-chain.wav" || { echo "FAIL: pcf/fx-chain re-render differs (not deterministic)"; exit 1; }
echo "-- audibility floor (silent goldens never pin: peak>=1000, rms>=100) --"
for v in pcf-sweep fx-delay fx-chain; do
  od -An -t d2 -v -j44 "$T10/$v.wav" | awk 'BEGIN { m=0; s=0; n=0 }
    { for (i=1;i<=NF;i++) { a=$i; if (a<0) a=-a; if (a>m) m=a; s+=$i*$i; n++ } }
    END { r=sqrt(s/n); printf "pcf/%s peak=%d rms=%.0f\n", VN, m, r;
      if (m<1000 || r<100) exit 1 }' VN="$v" || { echo "FAIL: pcf/$v silent (audibility floor)"; exit 1; }
done
echo "-- FX render-diff (chain differs from dry: path live, not a rename) --"
"$OUT/render" --fx dry --out "$T10/dry.wav" >/dev/null || exit 1
if "$OUT/compare" --events-a "$SG/sched-check.events" --events-b "$SG/sched-check.events" --wav-a "$T10/dry.wav" --wav-b "$T10/fx-chain.wav" | grep -q "COMPARE: IDENTICAL"; then echo "FAIL: fx chain renders identical to dry (FX dead)"; exit 1; fi
echo "== Phase 11: mixer + RIDevice registry (Task 11, gate G11) =="
T11=/tmp/ri/run/audit11
mkdir -p "$T11"
bash "$ROOT/scripts/ri_build_host.sh" test t1_mixer >/dev/null || { echo "FAIL: t1_mixer"; exit 1; }
test -f "$ROOT/docs/evidence/sequencer/fader-law.md" || { echo "FAIL: missing fader-law ledger"; exit 1; }
grep -q "P-17" "$ROOT/docs/evidence/sequencer/fader-law.md" || { echo "FAIL: ledger lacks P-17"; exit 1; }
grep -q "(v/127)" "$ROOT/docs/evidence/sequencer/fader-law.md" || { echo "FAIL: ledger lacks fader law"; exit 1; }
grep -q "P-16" "$ROOT/docs/evidence/sequencer/fader-law.md" || { echo "FAIL: ledger lacks P-16"; exit 1; }
grep -q "E0 fader law" "$ROOT/engine/dsp/params.c" || { echo "FAIL: params.c header lacks E0 fader record"; exit 1; }
test -f "$ROOT/docs/evidence/mixer/red-t1_mixer.txt" || { echo "FAIL: missing RED evidence"; exit 1; }
grep -q "FAIL" "$ROOT/docs/evidence/mixer/red-t1_mixer.txt" || { echo "FAIL: RED evidence has no FAIL lines"; exit 1; }
test -f "$ROOT/docs/evidence/mixer/engine.md" || { echo "FAIL: missing mixer engine ledger"; exit 1; }
echo "-- mixer goldens re-verified --"
SG11="$ROOT/tests/golden/mixer"
for v in mix-four mix-solo; do
  test -f "$SG11/$v.wav" || { echo "FAIL: missing golden mixer/$v.wav"; exit 1; }
  test -f "$SG11/$v.wav.sha256" || { echo "FAIL: missing sidecar mixer/$v.wav.sha256"; exit 1; }
done
(cd "$ROOT" && sha256sum -c tests/golden/mixer/mix-four.wav.sha256 tests/golden/mixer/mix-solo.wav.sha256) || { echo "FAIL: mixer golden sha256 mismatch"; exit 1; }
"$OUT/render" --mix four --out "$T11/mix-four.wav" >/dev/null || exit 1
"$OUT/render" --mix solo --out "$T11/mix-solo.wav" >/dev/null || exit 1
cmp -s "$SG11/mix-four.wav" "$T11/mix-four.wav" || { echo "FAIL: mixer/mix-four re-render differs (not deterministic)"; exit 1; }
cmp -s "$SG11/mix-solo.wav" "$T11/mix-solo.wav" || { echo "FAIL: mixer/mix-solo re-render differs (not deterministic)"; exit 1; }
echo "-- audibility floor (silent goldens never pin: peak>=1000, rms>=100) --"
for v in mix-four mix-solo; do
  od -An -t d2 -v -j44 "$T11/$v.wav" | awk 'BEGIN { m=0; s=0; n=0 }
    { for (i=1;i<=NF;i++) { a=$i; if (a<0) a=-a; if (a>m) m=a; s+=$i*$i; n++ } }
    END { r=sqrt(s/n); printf "mixer/%s peak=%d rms=%.0f\n", VN, m, r;
      if (m<1000 || r<100) exit 1 }' VN="$v" || { echo "FAIL: mixer/$v silent (audibility floor)"; exit 1; }
done
echo "-- mixer render-diff (solo differs from four: solo path live, not a rename) --"
if "$OUT/compare" --events-a "$SG/sched-check.events" --events-b "$SG/sched-check.events" --wav-a "$T11/mix-four.wav" --wav-b "$T11/mix-solo.wav" | grep -q "COMPARE: IDENTICAL"; then echo "FAIL: mixer solo renders identical to four (solo dead)"; exit 1; fi
echo "== Phase 12: GUI logic + MCC shells + panels (Task 12, gate G12) =="
bash "$ROOT/scripts/ri_build_host.sh" test t1_knob >/dev/null || { echo "FAIL: t1_knob"; exit 1; }
test -f "$ROOT/docs/evidence/gui/acceptance.md" || { echo "FAIL: missing gui acceptance"; exit 1; }
grep -q -- "- \[ \]" "$ROOT/docs/evidence/gui/acceptance.md" || { echo "FAIL: acceptance has no checkable boxes"; exit 1; }
grep -q "P-18" "$ROOT/docs/evidence/gui/acceptance.md" || { echo "FAIL: acceptance lacks P-18"; exit 1; }
grep -q "TC-2.9" "$ROOT/docs/evidence/gui/acceptance.md" || { echo "FAIL: acceptance lacks TC-2.9"; exit 1; }
grep -q "TC-2.10\|TC-2.11" "$ROOT/docs/evidence/gui/acceptance.md" || { echo "FAIL: acceptance lacks TC-2.10/2.11"; exit 1; }
grep -q "ReBirth-101" "$ROOT/docs/evidence/gui/acceptance.md" || { echo "FAIL: acceptance lacks tutorial workflow"; exit 1; }
grep -q "Tester:" "$ROOT/docs/evidence/gui/acceptance.md" || { echo "FAIL: acceptance lacks sign-off lines"; exit 1; }
test -f "$ROOT/docs/evidence/gui/red-t1_knob.txt" || { echo "FAIL: missing RED evidence"; exit 1; }
grep -q "FAIL" "$ROOT/docs/evidence/gui/red-t1_knob.txt" || { echo "FAIL: RED evidence has no FAIL lines"; exit 1; }
echo "-- AROS-only shells guarded + out of host build --"
for f in gui/widgets/rknb.mcc.c gui/widgets/rfdr.mcc.c gui/widgets/rstp.mcc.c gui/widgets/rlvl.mcc.c app/main.c; do
  test -f "$ROOT/$f" || { echo "FAIL: missing $f"; exit 1; }
  grep -q "#ifndef __AROS__" "$ROOT/$f" || { echo "FAIL: $f lacks __AROS__ guard"; exit 1; }
  grep -q '#error ".*AROS-only' "$ROOT/$f" || { echo "FAIL: $f lacks AROS-only #error"; exit 1; }
done
if grep -rn "widgets\|app/main" "$ROOT/scripts/ri_build_host.sh" 2>/dev/null; then echo "FAIL: AROS shells leak into host build"; exit 1; fi
grep -q "Numeric → Knob" "$ROOT/gui/widgets/rknb.mcc.c" || { echo "FAIL: rknb lacks reuse note"; exit 1; }
echo "-- AROS compile of GUI TUs (compile-only, no link) --"
if [ ! -f ../Vulkan4Aros/scripts/aros_build_env.sh ]; then echo "FAIL: Vulkan4AROS tree (toolchain source) not found"; exit 1; fi
. ../Vulkan4Aros/scripts/aros_build_env.sh
export PATH="$AROS_TOOLCHAIN:$PATH"
# ABIv1 SDK (2026-09-21): v1 build-pc tree, never the env default (r12 convention).
V1SDK_ABS="$(cd "$ROOT/../Vulkan4Aros/src/abi/v1/core-pc-x86_64/bin/pc-x86_64/AROS/Developer/include" && pwd)"
if [ -d "$V1SDK_ABS" ]; then SDK="$V1SDK_ABS"; else echo "FAIL: v1 build-pc SDK absent"; exit 1; fi
CFLAGS_GUI="-std=gnu99 -O2 -Wall -Wextra -Werror -Wno-pointer-sign -mcmodel=large -mno-red-zone -mno-ms-bitfields -fno-strict-aliasing -ffixed-r12 -fno-builtin -I$ROOT -I$SDK -I$SDK/aros/posixc -I$SDK/aros/stdc"
mkdir -p "$OUT/aros"
x86_64-aros-gcc $CFLAGS_GUI -c "$ROOT/gui/knob_logic.c" -o "$OUT/aros/knob_logic_aros.o" || { echo "FAIL: knob_logic.c AROS compile"; exit 1; }
x86_64-aros-gcc $CFLAGS_GUI -c "$ROOT/gui/panels.c" -o "$OUT/aros/panels_aros.o" || { echo "FAIL: panels.c AROS compile"; exit 1; }
for w in rknb rfdr rstp rlvl; do
  x86_64-aros-gcc $CFLAGS_GUI -c "$ROOT/gui/widgets/$w.mcc.c" -o "$OUT/aros/$w.o" || { echo "FAIL: $w.mcc.c AROS compile"; exit 1; }
done
x86_64-aros-gcc $CFLAGS_GUI -c "$ROOT/app/main.c" -o "$OUT/aros/app_main_aros.o" || { echo "FAIL: app/main.c AROS compile"; exit 1; }
echo "== Phase 13: formats full + MIDI + automation + ARexx + datatypes + fuzz (Task 13, gate G13) =="
T13=/tmp/ri/run/audit13
mkdir -p "$T13/c1" "$T13/c2" "$T13/rs" "$T13/regen"
bash "$ROOT/scripts/ri_build_host.sh" test t1_formats >/dev/null || { echo "FAIL: t1_formats"; exit 1; }
for f in rbng rbnm-full midi arexx automation datatypes fuzz green-defects; do
  test -f "$ROOT/docs/evidence/formats/$f.md" || { echo "FAIL: missing formats ledger $f.md"; exit 1; }
done
test -f "$ROOT/docs/evidence/formats/red-t1_formats.txt" || { echo "FAIL: missing RED evidence"; exit 1; }
grep -q "FAIL\|fatal error" "$ROOT/docs/evidence/formats/red-t1_formats.txt" || { echo "FAIL: RED evidence shows no failure"; exit 1; }
grep -q "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad" "$ROOT/tests/unit/t1_formats.c" || { echo "FAIL: abc vector ungated"; exit 1; }
echo "-- corpus determinism (regenerate + cmp) --"
gcc $CFLAGS -o "$OUT/mksong" "$ROOT/tools/mksong.c" "$OUT"/*.o || { echo "FAIL: mksong build"; exit 1; }
"$OUT/mksong" "$T13/regen" >/dev/null || exit 1
for k in 01 02 03 04 05 06 07 08 09 10; do
  test -f "$ROOT/tests/golden/songs/corpus/s$k.rbng" || { echo "FAIL: missing corpus s$k.rbng"; exit 1; }
  cmp -s "$ROOT/tests/golden/songs/corpus/s$k.rbng" "$T13/regen/s$k.rbng" || { echo "FAIL: corpus s$k not deterministically generated"; exit 1; }
  "$OUT/inspect" --rbng "$ROOT/tests/golden/songs/corpus/s$k.rbng" | grep -q "RBNG OK" || { echo "FAIL: corpus s$k rejected"; exit 1; }
done
echo "-- corpus double-render md5-identical --"
for k in 01 02 03 04 05 06 07 08 09 10; do
  "$OUT/render" --rbngsong "$ROOT/tests/golden/songs/corpus/s$k.rbng" --out "$T13/c1/s$k.wav" --dump-events "$T13/c1/s$k.events" >/dev/null || exit 1
  "$OUT/render" --rbngsong "$ROOT/tests/golden/songs/corpus/s$k.rbng" --out "$T13/c2/s$k.wav" --dump-events "$T13/c2/s$k.events" >/dev/null || exit 1
  cmp -s "$T13/c1/s$k.wav" "$T13/c2/s$k.wav" || { echo "FAIL: corpus s$k render not deterministic"; exit 1; }
  cmp -s "$T13/c1/s$k.events" "$T13/c2/s$k.events" || { echo "FAIL: corpus s$k events not deterministic"; exit 1; }
  "$OUT/mksong" --resave "$ROOT/tests/golden/songs/corpus/s$k.rbng" "$T13/rs/s$k.rbng" >/dev/null || exit 1
  cmp -s "$ROOT/tests/golden/songs/corpus/s$k.rbng" "$T13/rs/s$k.rbng" || { echo "FAIL: corpus s$k serialize-parse-serialize differs"; exit 1; }
done
echo "-- audibility floor on corpus renders (peak>=1000, rms>=100) --"
for k in 01 02 03 04 05 06 07 08 09 10; do
  od -An -t d2 -v -j44 "$T13/c1/s$k.wav" | awk 'BEGIN { m=0; s=0; n=0 }
    { for (i=1;i<=NF;i++) { a=$i; if (a<0) a=-a; if (a>m) m=a; s+=$i*$i; n++ } }
    END { r=sqrt(s/n); printf "corpus/s%s peak=%d rms=%.0f\n", VN, m, r;
      if (m<1000 || r<100) exit 1 }' VN="$k" || { echo "FAIL: corpus s$k silent (audibility floor)"; exit 1; }
done
echo "-- MODR warn prompt + CPRG hook live --"
"$OUT/render" --rbngsong "$ROOT/tests/golden/songs/corpus/s10.rbng" --out "$T13/modr.wav" >"$T13/modr.log" 2>&1 || exit 1
grep -q "MODR: mod 'acid-01'" "$T13/modr.log" || { echo "FAIL: MODR warn prompt missing"; exit 1; }
grep -q "CPRG:" "$T13/modr.log" || { echo "FAIL: CPRG hook silent"; exit 1; }
echo "-- WAV headers (sox --i when present, else inspect --wav) --"
if command -v sox >/dev/null 2>&1; then
  sox --i "$T13/c1/s01.wav" | grep -q "48000" || { echo "FAIL: sox rate check"; exit 1; }
else
  for k in 01 02 03 04 05 06 07 08 09 10; do
    "$OUT/inspect" --wav "$T13/c1/s$k.wav" | grep -q "WAV OK" || { echo "FAIL: corpus s$k WAV header"; exit 1; }
  done
fi
echo "-- RBNM-full: reserialize byte-identical + CPRG fallback --"
"$OUT/inspect" --rbnm "$ROOT/reference/packs/classic-01/pack.rbnm" | grep -q "RBNM OK" || { echo "FAIL: pack rejected after full"; exit 1; }
"$OUT/inspect" --rbnm-reserialize "$ROOT/reference/packs/classic-01/pack.rbnm" "$T13/pack2.rbnm" | grep -q "RESERIALIZED" || exit 1
cmp -s "$ROOT/reference/packs/classic-01/pack.rbnm" "$T13/pack2.rbnm" || { echo "FAIL: pack reserialize differs (unknown/CPRG bytes lost)"; exit 1; }
echo "-- fuzz 500/500 no-crash --"
bash "$ROOT/scripts/ri_fuzz.sh" 500 | tail -2
echo "-- AROS-only backends guarded + out of host build --"
for f in midi_io/camd_backend.c project/datatypes/rbng.datatype.c project/datatypes/rbnm.datatype.c; do
  test -f "$ROOT/$f" || { echo "FAIL: missing $f"; exit 1; }
  grep -q "#ifndef __AROS__" "$ROOT/$f" || { echo "FAIL: $f lacks __AROS__ guard"; exit 1; }
  grep -q '#error ".*AROS-only' "$ROOT/$f" || { echo "FAIL: $f lacks AROS-only #error"; exit 1; }
done
if grep -rn "camd_backend\|datatypes" "$ROOT/scripts/ri_build_host.sh" 2>/dev/null; then echo "FAIL: AROS backends leak into host build"; exit 1; fi
echo "-- AROS compile of format/MIDI TUs (compile-only, no link) --"
if [ ! -f ../Vulkan4Aros/scripts/aros_build_env.sh ]; then echo "FAIL: Vulkan4AROS tree (toolchain source) not found"; exit 1; fi
. ../Vulkan4Aros/scripts/aros_build_env.sh
export PATH="$AROS_TOOLCHAIN:$PATH"
SDK="$AROS_SDK_INCLUDE"
CFLAGS_FMT="-std=gnu99 -O2 -Wall -Wextra -Werror -Wno-pointer-sign -mcmodel=large -mno-red-zone -mno-ms-bitfields -fno-strict-aliasing -ffixed-r12 -fno-builtin -I$ROOT -I$SDK -I$SDK/aros/posixc -I$SDK/aros/stdc"
for tu in project/sha256.c project/rbng.c project/arexx.c project/undo.c midi_io/midi.c midi_io/camd_backend.c project/datatypes/rbng.datatype.c project/datatypes/rbnm.datatype.c; do
  bn=$(basename "$tu" .c)
  x86_64-aros-gcc $CFLAGS_FMT -c "$ROOT/$tu" -o "$OUT/aros/${bn}_aros.o" || { echo "FAIL: $tu AROS compile"; exit 1; }
done
echo "== Phase 14: REL soak/docs/installer/beta-exit (Task 14, gate G14) =="
T14=/tmp/ri/run/audit14
mkdir -p "$T14"
bash "$ROOT/scripts/ri_build_host.sh" test t1_rel >"$T14/rel.log" 2>&1 || { echo "FAIL: t1_rel"; exit 1; }
grep -q "PASS rel" "$T14/rel.log" || { echo "FAIL: t1_rel no PASS"; exit 1; }
grep -q "RENDERED-DE: Abspielen" "$T14/rel.log" || { echo "FAIL: DE proof string not rendered"; exit 1; }
echo "-- bench worst-case (short: 4500 blocks = 6 s audio) --"
gcc $CFLAGS -o "$OUT/bench" "$ROOT/tools/bench.c" "$OUT"/*.o -lm || { echo "FAIL: bench build"; exit 1; }
"$OUT/bench" 4500 >"$T14/bench.log" 2>&1 || { echo "FAIL: bench run"; exit 1; }
grep -q "DETERMINISTIC" "$T14/bench.log" || { echo "FAIL: bench nondeterministic"; exit 1; }
grep -q "BENCH OK" "$T14/bench.log" || { echo "FAIL: bench no OK"; exit 1; }
echo "-- short soak (30 s, switching under load) --"
RI_SOAK_BENCH_BLOCKS=4500 bash "$ROOT/scripts/ri_soak.sh" 30 "$T14/soak" >"$T14/soak-tail.log" 2>&1 || { echo "FAIL: soak 30s"; exit 1; }
grep -q "SOAK PASS" "$T14/soak/SOAK.log" || { echo "FAIL: soak no PASS"; exit 1; }
grep -q "underruns=0" "$T14/soak/SOAK.log" || { echo "FAIL: soak underruns"; exit 1; }
echo "-- AmigaGuide: every control row present --"
test -f "$ROOT/docs/ReIncarnation.guide" || { echo "FAIL: missing manual"; exit 1; }
for row in "303A cutoff" "303A reso" "303A envmod" "303A decay" "303A accent" "303A volume" \
  "303B cutoff" "303B reso" "303B envmod" "303B decay" "303B accent" "303B volume" \
  "808 level" "808 tune" "808 decay" "808 snappy" "808 tone" "808 accent" \
  "909 tune" "909 level" "909 decay" "909 flamres" \
  "mixer bus1" "mixer bus2" "mixer bus3" "mixer bus4" "mixer master" "mixer send1" \
  "transport play" "transport stop" "transport tempo" "transport pattern" "transport shuffle"; do
  grep -q "$row" "$ROOT/docs/ReIncarnation.guide" || { echo "FAIL: guide lacks: $row"; exit 1; }
done
for cmd in OPENSONG PLAY STOP EXPORTWAV SETRPPARAM; do
  grep -q "$cmd" "$ROOT/docs/ReIncarnation.guide" || { echo "FAIL: guide lacks ARexx $cmd"; exit 1; }
done
echo "-- autodocs: exact signatures shipped --"
for d in audio mixer midi arexx formats dsp seq fx gui; do
  test -f "$ROOT/docs/autodoc/$d.doc" || { echo "FAIL: missing autodoc $d.doc"; exit 1; }
done
check_sig() { # $1=header-rel $2=doc-rel $3=signature-literal
  grep -qF "$3" "$ROOT/$1" || { echo "FAIL: $1 lacks: $3"; exit 1; }
  grep -qF "$3" "$ROOT/$2" || { echo "FAIL: $2 lacks: $3"; exit 1; }
}
check_sig audio_io/audio.h docs/autodoc/audio.doc "struct AudioObject *AuCreateObject(struct Library *AudioBase, struct TagItem *tags)"
check_sig audio_io/audio.h docs/autodoc/audio.doc "uint32_t AuAddSource(struct AudioObject *ao, struct TagItem *tags)"
check_sig audio_io/audio.h docs/autodoc/audio.doc "uint32_t AuAddBus(struct AudioObject *ao, const char *name, struct TagItem *tags)"
check_sig audio_io/audio.h docs/autodoc/audio.doc "int AuConnect(struct AudioObject *ao, uint32_t src, uint32_t bus)"
check_sig audio_io/audio.h docs/autodoc/audio.doc "int AuStart(struct AudioObject *ao)"
check_sig audio_io/audio.h docs/autodoc/audio.doc "void AuStop(struct AudioObject *ao)"
check_sig audio_io/audio.h docs/autodoc/audio.doc "uint32_t AuQueryAttr(struct AudioObject *ao, uint32_t attr)"
check_sig audio_io/audio.h docs/autodoc/audio.doc "int AuRenderToFile(struct AudioObject *ao, const char *path, uint32_t ms)"
check_sig engine/mixer/mixer.h docs/autodoc/mixer.doc "void ri_mix_init(struct RiMixer *m, float sr)"
check_sig engine/mixer/mixer.h docs/autodoc/mixer.doc "int ri_mix_set_fader(struct RiMixer *m, uint32_t bus, uint8_t v)"
check_sig engine/mixer/mixer.h docs/autodoc/mixer.doc "void ri_mix_render(struct RiMixer *m, const float *bus_in[RI_MIX_NBUS],"
check_sig engine/mixer/mixer.h docs/autodoc/mixer.doc "float *out, float *send_out, uint32_t n)"
check_sig engine/framework/ridevice.h docs/autodoc/mixer.doc "void ri_devices_init(void)"
check_sig engine/framework/ridevice.h docs/autodoc/mixer.doc "struct RIDevice *ri_device_get(uint32_t index)"
check_sig engine/framework/ridevice.h docs/autodoc/mixer.doc "uint32_t ri_device_count(void)"
check_sig midi_io/midi.h docs/autodoc/midi.doc "int32_t midi_cc_lookup(const struct RIMidiLearn *m, uint32_t cc)"
check_sig midi_io/midi.h docs/autodoc/midi.doc "int midi_mmc_cmd(const void *buf, uint32_t n)"
check_sig project/arexx.h docs/autodoc/arexx.doc "int arexx_parse(const char *line, struct RIArexxCmd *cmd)"
check_sig project/arexx_dispatch.h docs/autodoc/arexx.doc "void arexx_dispatch(const struct RIArexxCmd *cmd, struct RIArexxReply *rep)"
check_sig project/rbng.h docs/autodoc/formats.doc "int rbng_read_song(const char *path, struct RISong *s, char *err,"
check_sig project/rbnm.h docs/autodoc/formats.doc "int rbnm_reserialize(const char *src, const char *dst, char *err,"
check_sig project/undo.h docs/autodoc/formats.doc "int ri_undo_commit(struct RIUndo *u, uint32_t ctl, uint8_t val)"
check_sig gui/panels.h docs/autodoc/gui.doc "unsigned int ri_panel_count(void)"
check_sig gui/catalog.h docs/autodoc/gui.doc 'const char *ri_catalog_get(const char *locale, const char *msgid)'
check_sig engine/dsp/rb303.h docs/autodoc/dsp.doc "void rb303_render(struct RB303Voice *v, float *out, uint32_t n, float sr)"
check_sig engine/dsp/rb808.h docs/autodoc/dsp.doc "void rb808_render_mix(struct RB808Set *s, float *out, uint32_t n, float sr)"
check_sig engine/dsp/rb909.h docs/autodoc/dsp.doc "void rb909_trigger(struct RB909Set *s, uint32_t voice, uint32_t accent,"
check_sig engine/dsp/rb909.h docs/autodoc/dsp.doc "uint8_t tune, int32_t flam_delay_smp)"
check_sig engine/fx/pcf.h docs/autodoc/fx.doc "void pcf_render(struct PCF *p, const float *in, float *out, uint32_t n"
check_sig engine/fx/fx.h docs/autodoc/fx.doc "void RiFXRender(struct RIFX *x, float *in, float *out, uint32_t frames,"
check_sig engine/fx/fx.h docs/autodoc/fx.doc "float sr, float bpm)"
check_sig gui/knob_logic.h docs/autodoc/gui.doc "double ri_knob_drag_to_value(double start, double dy_px, int fine)"
echo "-- catalogs: EN + DE proof in source AND table --"
test -f "$ROOT/locale/ReIncarnation.cd" || { echo "FAIL: missing .cd"; exit 1; }
test -f "$ROOT/locale/en.ct" || { echo "FAIL: missing en.ct"; exit 1; }
test -f "$ROOT/locale/de.ct" || { echo "FAIL: missing de.ct"; exit 1; }
grep -q "Abspielen" "$ROOT/locale/de.ct" || { echo "FAIL: de.ct lacks proof string"; exit 1; }
grep -q "Abspielen" "$ROOT/gui/catalog.c" || { echo "FAIL: catalog.c lacks proof string"; exit 1; }
echo "-- installer + icons as files --"
test -f "$ROOT/Install/ReIncarnation-Install" || { echo "FAIL: missing installer"; exit 1; }
grep -q "PROGDIR:ReIncarnation" "$ROOT/Install/ReIncarnation-Install" || { echo "FAIL: installer lacks program stanza"; exit 1; }
test -f "$ROOT/Install/icons/tool.png" || { echo "FAIL: missing tool icon"; exit 1; }
test -f "$ROOT/Install/icons/drawer.png" || { echo "FAIL: missing drawer icon"; exit 1; }
echo "-- Task-13 deferred wiring now owned --"
test -f "$ROOT/project/arexx_aros.c" || { echo "FAIL: missing arexx_aros.c"; exit 1; }
grep -q "#ifndef __AROS__" "$ROOT/project/arexx_aros.c" || { echo "FAIL: arexx_aros lacks guard"; exit 1; }
grep -q "ri_camd_open" "$ROOT/midi_io/camd_backend.c" || { echo "FAIL: CAMD open unwired"; exit 1; }
grep -q "ri_camd_close" "$ROOT/midi_io/camd_backend.c" || { echo "FAIL: CAMD close unwired"; exit 1; }
grep -q "ri_rbng_datatype_reg" "$ROOT/project/datatypes/rbng.datatype.c" || { echo "FAIL: RBNG reg missing"; exit 1; }
grep -q "ri_rbnm_datatype_reg" "$ROOT/project/datatypes/rbnm.datatype.c" || { echo "FAIL: RBNM reg missing"; exit 1; }
if grep -rn "arexx_aros\|camd_backend\|datatypes" "$ROOT/scripts/ri_build_host.sh" 2>/dev/null; then echo "FAIL: AROS shells leak into host build"; exit 1; fi
echo "-- AROS compile of new AROS-side code (compile-only, no link) --"
if [ ! -f ../Vulkan4Aros/scripts/aros_build_env.sh ]; then echo "FAIL: Vulkan4AROS tree (toolchain source) not found"; exit 1; fi
. ../Vulkan4Aros/scripts/aros_build_env.sh
export PATH="$AROS_TOOLCHAIN:$PATH"
SDK="$AROS_SDK_INCLUDE"
CFLAGS_REL="-std=gnu99 -O2 -Wall -Wextra -Werror -Wno-pointer-sign -mcmodel=large -mno-red-zone -mno-ms-bitfields -fno-strict-aliasing -ffixed-r12 -fno-builtin -I$ROOT -I$SDK -I$SDK/aros/posixc -I$SDK/aros/stdc"
for tu in project/arexx_aros.c midi_io/camd_backend.c project/datatypes/rbng.datatype.c project/datatypes/rbnm.datatype.c gui/catalog.c project/arexx_dispatch.c; do
  bn=$(basename "$tu" .c)
  x86_64-aros-gcc $CFLAGS_REL -c "$ROOT/$tu" -o "$OUT/aros/${bn}_rel_aros.o" || { echo "FAIL: $tu AROS compile"; exit 1; }
done
echo "-- beta-exit record + repo hygiene --"
test -f "$ROOT/docs/evidence/formats/beta-exit.md" || { echo "FAIL: missing beta-exit.md"; exit 1; }
grep -q "DEFERRED" "$ROOT/docs/evidence/formats/beta-exit.md" || { echo "FAIL: beta-exit has no DEFERRED rows"; exit 1; }
grep -q "No beta ran" "$ROOT/docs/evidence/formats/beta-exit.md" || { echo "FAIL: beta-exit claims a beta"; exit 1; }
for f in ri_audit.sh ri_build_aros.sh ri_build_host.sh ri_fuzz.sh ri_soak.sh; do
  test -f "$ROOT/scripts/$f" || { echo "FAIL: missing scripts/$f"; exit 1; }
done
test "$(ls "$ROOT/scripts" | wc -l)" = "5" || { echo "FAIL: scripts/ holds non-shared files"; exit 1; }
if git -C "$ROOT" status --porcelain | grep -E "\.o$|\.library$"; then echo "FAIL: build artifacts in tree"; exit 1; fi
if grep -rnw "TODO\|TBD\|FIXME" "$ROOT/docs/ReIncarnation.guide" "$ROOT/docs/autodoc" "$ROOT/locale" "$ROOT/Install" 2>/dev/null; then echo "FAIL: placeholder in REL docs"; exit 1; fi
echo "AUDIT 0/0 PASS"
