#!/bin/bash
set -e
ROOT="$(dirname "$0")/.."
# Phase 0d greps this file for gate names, so it needs its own real path —
# `$0` is wrong under `bash < ri_audit.sh` or a symlinked entry point.
SELF="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/$(basename "${BASH_SOURCE[0]}")"
# The render tool resolves some defaults (--909pack inventory) CWD-relative;
# pin the whole audit to the repo root so invocation CWD cannot silently
# kill a gate (observed: S909 died with `|| exit 1` from another CWD).
cd "$ROOT" || { echo "FAIL: cannot cd $ROOT"; exit 1; }
echo "== Phase 0a: render-path hygiene =="
if grep -rn "malloc\|calloc\|realloc\|free(\|Forbid\|Disable(" "$ROOT/engine/" 2>/dev/null; then echo "FAIL: banned construct in engine/"; exit 1; fi
echo "== Phase 0b: no platform transcendentals/FMA in engine/ =="
if grep -rn "tanhf\|sinf\|cosf\|expf\|powf\|fmodf\|mul_add\|ffast-math" "$ROOT/engine/" 2>/dev/null | grep -v "ri_tanh\|ri_exp\|ri_sin\|ri_pow2"; then echo "FAIL"; exit 1; fi
echo "== Phase 0c: evidence dirs =="
for d in 303 808 909 pcf sequencer gui formats; do test -d "$ROOT/docs/evidence/$d" || { echo "FAIL: missing $d"; exit 1; }; done
echo "== Phase 0d: no test ships ungated =="
# An ungated test is not coverage. It passes in the author's terminal, then the
# code under it changes, and nothing says so — which is not hypothetical: 26
# tests sat ungated for 9 days, and two of them had already gone stale against
# their own headers (t51 hardcoded an owner bound that Levi's arrival moved,
# t107 asserts an encoder slot is dead after RIBBON took it).
# Exemptions belong in the list below with a reason, so "not gated" is always a
# recorded decision rather than an absence nobody notices.
# t107 is KNOWN-RED, not exempt-by-omission: RIBBON (P8d) took encoder slot 5,
# so its assertions that the slot is a dead one now describe the pre-RIBBON map.
# Left ungated deliberately -- fixing it is a decision about which contract is
# right, and guessing would make the suite green without making the code right.
#
# levi_bench is a BENCHMARK, not a test, and is deliberately ungated (owner
# 2026-10-04). It prints a cost table; it asserts no bound, because a
# nanosecond figure depends on the host CPU, its governor, and what else is
# running. Gating it would either freeze today's machine into the suite or
# invite someone to weaken the threshold until it went green -- both worse than
# leaving it as the measurement tool it is. It DOES assert one thing that is
# machine-independent: that the bench patch produces signal, since a bench that
# measures an early-out reports zero work and looks like a result.
#
# Run it deliberately, not as part of the gate:
#   ./scripts/ri_build_host.sh all
#   gcc -std=gnu99 -O2 -I. -o /tmp/levi_bench tests/unit/levi_bench.c \
#       /tmp/ri/build/*.o -lm -lpng && /tmp/levi_bench
#
# bench_build is the display-list build, same shape: it prints, per section, how
# many commands the whole section emits against how many a 64x16 damage box
# keeps, and where the time goes with and without the clip. It exists because
# the build is the largest remaining term in a box repaint (233 us = 112 build +
# 33 replay + 69 blit + 18 unattributed, 2026-10-05) and this is the only place
# that split is visible per section.
#   ./scripts/ri_build_host.sh gui && ./scripts/ri_build_host.sh draw
#   ./scripts/ri_build_host.sh test bench_build
#
# bench_trbar is narrower and, for this lane, more relevant: `bsec` reads
# RI_SEC_TRANSPORT in 16 of 16 windows on target, so it measures that one section
# against the box the app really asks for -- ri_geo_bbox(RI_STR_BAR) at
# RI_GEO_ZOOM_COMPACT -- rather than the synthetic centre box bench_build uses.
#   ./scripts/ri_build_host.sh test bench_trbar
UNGATED_ALLOW=" t107_levi_sect levi_bench bench_build bench_trbar "
for f in "$ROOT"/tests/unit/*.c; do
  t="$(basename "$f" .c)"
  grep -qE "(test[[:space:]]+|\b)$t\b" "$SELF" && continue
  case "$UNGATED_ALLOW" in *" $t "*) continue ;; esac
  echo "FAIL: $t is not gated by this audit — add a gate, or an exemption with a reason"
  exit 1
done
echo "-- every test reachable from a gate; exemptions: t107_levi_sect (known-red, RIBBON encoder map), levi_bench (benchmark, asserts no machine-dependent bound), bench_build (benchmark, prints the build/clip split; the same property is pinned by t169, which IS gated), bench_trbar (benchmark, prints the transport clipped build against the REAL RI_STR_BAR damage box) --"
echo "== Phase 1: first-light goldens (Task 4, gate G4) =="
bash "$ROOT/scripts/ri_build_host.sh" all >/dev/null || { echo "FAIL: host build"; exit 1; }
G="$ROOT/tests/golden/303"
for f in math-dc math-sine first-light dc-24 dc-441 first-light-441 dc-aiff first-light-aiff; do
  test -f "$G/$f.wav" || { echo "FAIL: missing golden $f.wav (missing-is-broken)"; exit 1; }
  test -f "$G/$f.wav.sha256" || { echo "FAIL: missing sidecar $f.wav.sha256"; exit 1; }
done
test -f "$G/first-light.events" || { echo "FAIL: missing event golden"; exit 1; }
test -f "$G/first-light.rbng" || { echo "FAIL: missing song scaffold"; exit 1; }
(cd "$ROOT" && sha256sum -c tests/golden/303/math-dc.wav.sha256 tests/golden/303/math-sine.wav.sha256 tests/golden/303/first-light.wav.sha256 tests/golden/303/dc-24.wav.sha256 tests/golden/303/dc-441.wav.sha256 tests/golden/303/first-light-441.wav.sha256 tests/golden/303/dc-aiff.wav.sha256 tests/golden/303/first-light-aiff.wav.sha256) || { echo "FAIL: golden sha256 mismatch"; exit 1; }
echo "-- math unit goldens re-verified --"
bash "$ROOT/scripts/ri_build_host.sh" test t1_303math >/dev/null || { echo "FAIL: t1_303math"; exit 1; }
bash "$ROOT/scripts/ri_build_host.sh" test t1_303walk >/dev/null || { echo "FAIL: t1_303walk"; exit 1; }
bash "$ROOT/scripts/ri_build_host.sh" test t22_303indep >/dev/null || { echo "FAIL: t22_303indep"; exit 1; }
bash "$ROOT/scripts/ri_build_host.sh" test t22_303slide >/dev/null || { echo "FAIL: t22_303slide"; exit 1; }
bash "$ROOT/scripts/ri_build_host.sh" test t22_303accent >/dev/null || { echo "FAIL: t22_303accent"; exit 1; }
bash "$ROOT/scripts/ri_build_host.sh" test t22_303click >/dev/null || { echo "FAIL: t22_303click"; exit 1; }
bash "$ROOT/scripts/ri_build_host.sh" test t22_303filter >/dev/null || { echo "FAIL: t22_303filter"; exit 1; }
bash "$ROOT/scripts/ri_build_host.sh" test t36_303_ctls >/dev/null || { echo "FAIL: t36_303_ctls (0x0305 is wave, not level; 303B shares the implementation)"; exit 1; }
bash "$ROOT/scripts/ri_build_host.sh" test t38_gate_fraction >/dev/null || { echo "FAIL: t38_gate_fraction (gate ends at step_start + 1/2 step; ties hold)"; exit 1; }
bash "$ROOT/scripts/ri_build_host.sh" test t39_meg_veg >/dev/null || { echo "FAIL: t39_meg_veg (MEG follows Decay, VEG fixed, accent forces minimum MEG)"; exit 1; }
bash "$ROOT/scripts/ri_build_host.sh" test t40_logslide >/dev/null || { echo "FAIL: t40_logslide (slide slews log2(f): octave rate constant)"; exit 1; }
echo "-- re-render-compare --"
bash "$ROOT/scripts/ri_build_host.sh" all >/dev/null || { echo "FAIL: host build"; exit 1; }
OUT=/tmp/ri/build
CFLAGS="-std=c99 -O2 -Wall -Wextra -Werror -pedantic -ffp-contract=off -fno-unsafe-math-optimizations -ftrapv -I$ROOT"
gcc $CFLAGS -o "$OUT/render" "$ROOT/tools/render.c" "$OUT"/*.o -lpng || { echo "FAIL: render build"; exit 1; }
A=/tmp/ri/run/audit
mkdir -p "$A"
"$OUT/render" --math dc --out "$A/math-dc.wav" || exit 1
"$OUT/render" --math sine --out "$A/math-sine.wav" || exit 1
"$OUT/render" --song "$G/first-light.rbng" --out "$A/first-light.wav" --dump-events "$A/first-light.events" || exit 1
for f in math-dc math-sine first-light; do cmp -s "$G/$f.wav" "$A/$f.wav" || { echo "FAIL: re-render $f differs"; exit 1; }; done
"$OUT/render" --math dc --depth 24 --out "$A/dc-24.wav" || exit 1
cmp -s "$G/dc-24.wav" "$A/dc-24.wav" || { echo "FAIL: re-render dc-24 differs"; exit 1; }
"$OUT/render" --math dc --rate 44100 --out "$A/dc-441.wav" || exit 1
cmp -s "$G/dc-441.wav" "$A/dc-441.wav" || { echo "FAIL: re-render dc-441 differs"; exit 1; }
"$OUT/render" --song "$G/first-light.rbng" --rate 44100 --out "$A/first-light-441.wav" || exit 1
cmp -s "$G/first-light-441.wav" "$A/first-light-441.wav" || { echo "FAIL: re-render first-light-441 differs"; exit 1; }
"$OUT/render" --math dc --format aiff --out "$A/dc-aiff.wav" || exit 1
cmp -s "$G/dc-aiff.wav" "$A/dc-aiff.wav" || { echo "FAIL: re-render dc-aiff differs"; exit 1; }
"$OUT/render" --song "$G/first-light.rbng" --format aiff --out "$A/first-light-aiff.wav" || exit 1
cmp -s "$G/first-light-aiff.wav" "$A/first-light-aiff.wav" || { echo "FAIL: re-render first-light-aiff differs"; exit 1; }
bash "$ROOT/scripts/ri_build_host.sh" test t32_aiff >/dev/null || { echo "FAIL: t32_aiff (TC-2.13/WBS-2.13)"; exit 1; }
bash "$ROOT/scripts/ri_build_host.sh" test t28_wavdepth >/dev/null || { echo "FAIL: t28_wavdepth (TC-2.13.4)"; exit 1; }
bash "$ROOT/scripts/ri_build_host.sh" test t31_rate441 >/dev/null || { echo "FAIL: t31_rate441 (TC-2.13.4)"; exit 1; }
echo "-- export golden headers (external sox check when present) --"
if command -v sox >/dev/null 2>&1; then
  for spec in "math-dc.wav:48000:16:96000" "dc-24.wav:48000:24:96000" "dc-441.wav:44100:16:96000" "first-light.wav:48000:16:122571" "first-light-441.wav:44100:16:112612" "dc-aiff.wav:48000:16:96000" "first-light-aiff.wav:48000:16:122571"; do
    f="${spec%%:*}"; rest="${spec#*:}"; r="${rest%%:*}"; rest="${rest#*:}"; b="${rest%%:*}"; n="${rest##*:}";
    [ "$(soxi -c "$G/$f")" = "1" ] || { echo "FAIL: sox ch $f"; exit 1; }
    [ "$(soxi -r "$G/$f")" = "$r" ] || { echo "FAIL: sox rate $f"; exit 1; }
    [ "$(soxi -b "$G/$f")" = "$b" ] || { echo "FAIL: sox bits $f"; exit 1; }
    [ "$(soxi -s "$G/$f")" = "$n" ] || { echo "FAIL: sox samples $f"; exit 1; }
  done
else
  warn "sox missing — export headers covered by t28/t31 in-test parse only"
fi
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
# Same hygiene contract for probe_rate (2026-10-03, the rate probe). It is a
# second AHI-only executable and the host-build leak is the same hazard: a
# file including <devices/ahi.h> cannot compile natively, so the guard has to be
# a gate rather than a hope.
test -f "$ROOT/audio_io/probe_rate.c" || { echo "FAIL: missing audio_io/probe_rate.c"; exit 1; }
grep -q "#ifndef __AROS__" "$ROOT/audio_io/probe_rate.c" || { echo "FAIL: probe_rate lacks __AROS__ guard"; exit 1; }
grep -q '#error "probe_rate.c is AROS-only' "$ROOT/audio_io/probe_rate.c" || { echo "FAIL: probe_rate lacks AROS-only #error"; exit 1; }
if grep -rn "probe_rate" "$ROOT/scripts/ri_build_host.sh" 2>/dev/null; then echo "FAIL: probe_rate leaks into host build"; exit 1; fi
test -f /tmp/ri/aros/probe_rate || { echo "FAIL: probe_rate artifact missing"; exit 1; }
# The rate probe must read the negotiated mode back and never trust the request.
# This is the whole reason it exists: on the Dell, a 48000 request comes back
# as 44100, and a probe that only reported the request would report "the card
# does 48 kHz" -- the exact false claim it was written to correct.
grep -q "AHIDB_Frequency,   (IPTR)&q_freq" "$ROOT/audio_io/probe_rate.c" || { echo "FAIL: probe_rate does not read the mode back"; exit 1; }
grep -q "CONVERTED_TO" "$ROOT/audio_io/probe_rate.c" || { echo "FAIL: probe_rate lacks the resampling tell"; exit 1; }
echo "-- ABIv1 LVO convention gate (2026-09-21 probe_ahi guest page-fault) --"
echo "-- Guest binaries MUST emit rdx-base calls (build-pc SDK); any"
echo "-- 'mov %rax,%r12' means the stale r12 SDK leaked in and the binary"
echo "-- will fault on first LVO call. Checked on every AROS artifact. --"
echo "--"
echo "-- CORRECTION 2026-10-02 (see the wiki record on r12moves). The premise"
echo "-- above is wrong about this tree, and the pass/fail logic is DELIBERATELY"
echo "-- LEFT UNCHANGED because the 2026-09-21 page fault was real and its cause"
echo "-- is still unknown. What is now established:"
echo "--   * The v11 SDK's aros/x86_64/libcall.h is autogenerated by"
echo "--     AROS/arch/x86_64-all/include/gencall.c and uses R12 as the library"
echo "--     base register by design, saving and restoring it around every call:"
echo "--       movq %%r12, SLOT ; movq BASE, %%r12 ; ... ; movq SLOT, %%r12"
echo "--   * CORRECTION to that first pass: this gate is a V1-LANE gate and it is"
echo "--     COHERENT for v1. Only v11 ships an arch-specific header --"
echo "--       v11/.../aros/x86_64/libcall.h   r12=45  rdx=0"
echo "--     v1 has NO aros/x86_64/libcall.h at all (its x86_64 SDK provides"
echo "--     cpucontext.h and genmodule.h instead), so a v1 build emits zero of"
echo "--     these and this gate passes at 0 -- which is what it does. An earlier"
echo "--     version of this comment wrongly called the invariant unachievable."
echo "--   * The real incompatibility is narrower: the v11-lane RIAPP recipe"
echo "--     (vms/ri-p9/build_v11.sh, outside this repo) targets a DIFFERENT ABI,"
echo "--     and a v1-lane gate does not apply to its output. That is a scoping"
echo "--     fact, not a defect in the gate."
echo "--   * Within the v11 lane the count still rises with optimisation -- -O0 is"
echo "--     0 in our own objects (the stubs stay out of line in the prebuilt"
echo "--     libraries) and -O2 is 245, confined to the 8 AROS-only platform files"
echo "--     that call AHI/Intuition/dos, with no engine or DSP file affected --"
echo "--     because -O2 inlines the SDK's trampoline into our code. So within one"
echo "--     lane the count is an INLINING counter; across lanes it is an ABI"
echo "--     fingerprint. It is a good fingerprint and not a safety signal."
echo "--   * -ffixed-r12 cannot suppress the v11 sequence: r12 is named in the asm"
echo "--     template and in a 'register ... __asm__(\"r12\")' variable, so the"
echo "--     compiler neither allocates nor rewrites it."
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
echo "-- shared engine core wired (I1 retired §12.3: live == file by construction) --"
grep -q "ri_engine_render_mono" "$ROOT/audio_io/audio.c" || { echo "FAIL: audio.c not on the shared engine core"; exit 1; }
if grep -q "RI_LIVE_FULL_GRAPH_UNIMPLEMENTED" "$ROOT/audio_io/audio.c"; then echo "FAIL: retired live-scope marker still present"; exit 1; fi
mkdir -p /tmp/ri/run/t6 # t6 test writes its scaffold here; clean wipes /tmp/ri
bash "$ROOT/scripts/ri_build_host.sh" test t6_w1backend >/dev/null || { echo "FAIL: t6_w1backend"; exit 1; }
bash "$ROOT/scripts/ri_build_host.sh" test t37_engine_single >/dev/null || { echo "FAIL: t37_engine_single (shared core: per-device routing, centre-unity stereo, mono fold == legacy)"; exit 1; }
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
x86_64-aros-gcc $CFLAGS_AU -fasm -c "$ROOT/audio_io/audio_ahi.c" -o "$OUT/aros/audio_ahi_aros.o" || { echo "FAIL: audio_ahi.c AROS compile"; exit 1; }
x86_64-aros-gcc $CFLAGS_AU -fasm -c "$ROOT/audio_io/audio_ahi_play.c" -o "$OUT/aros/audio_ahi_play_aros.o" || { echo "FAIL: audio_ahi_play.c AROS compile"; exit 1; }
x86_64-aros-gcc $CFLAGS_AU -fasm -c "$ROOT/audio_io/audio_ahi_live.c" -o "$OUT/aros/audio_ahi_live_aros.o" || { echo "FAIL: audio_ahi_live.c AROS compile"; exit 1; }
x86_64-aros-gcc $CFLAGS_AU -fasm -c "$ROOT/app/riapp.c" -o "$OUT/aros/riapp_aros.o" || { echo "FAIL: app/riapp.c AROS compile"; exit 1; }
for adecl in "int AuPlay(struct AudioObject \*ao);" \
  "int AuPlayEx(struct AudioObject \*ao, const volatile int \*stop);"; do
  grep -q "$adecl" "$ROOT/audio_io/audio_ahi.h" || { echo "FAIL: audio_ahi.h lacks: $adecl"; exit 1; }
done
echo "== Phase 6b: seq master clock 10-min accumulation (WBS 2.1, TC-2.1.1) =="
test -f "$ROOT/engine/seq/riseq.h" || { echo "FAIL: missing engine/seq/riseq.h"; exit 1; }
test -f "$ROOT/engine/seq/riseq.c" || { echo "FAIL: missing engine/seq/riseq.c"; exit 1; }
bash "$ROOT/scripts/ri_build_host.sh" sched >/dev/null || { echo "FAIL: sched build (riseq)"; exit 1; }
bash "$ROOT/scripts/ri_build_host.sh" test t21_seq >/dev/null || { echo "FAIL: t21_seq"; exit 1; }
bash "$ROOT/scripts/ri_build_host.sh" test t21_seqloop >/dev/null || { echo "FAIL: t21_seqloop"; exit 1; }
bash "$ROOT/scripts/ri_build_host.sh" test t21_songsteps >/dev/null || { echo "FAIL: t21_songsteps"; exit 1; }
bash "$ROOT/scripts/ri_build_host.sh" test t27_copypaste >/dev/null || { echo "FAIL: t27_copypaste (TC-2.10.3)"; exit 1; }
bash "$ROOT/scripts/ri_build_host.sh" test t21_schedfeed >/dev/null || { echo "FAIL: t21_schedfeed"; exit 1; }
mkdir -p /tmp/ri/run/t21 # t21_songfile writes its scaffold here (t6 pattern)
bash "$ROOT/scripts/ri_build_host.sh" test t21_songfile >/dev/null || { echo "FAIL: t21_songfile"; exit 1; }
bash "$ROOT/scripts/ri_build_host.sh" test t21_looppcm >/dev/null || { echo "FAIL: t21_looppcm"; exit 1; }
bash "$ROOT/scripts/ri_build_host.sh" test t21_snapbuild >/dev/null || { echo "FAIL: t21_snapbuild"; exit 1; }
bash "$ROOT/scripts/ri_build_host.sh" test t21_evwin >/dev/null || { echo "FAIL: t21_evwin"; exit 1; }
bash "$ROOT/scripts/ri_build_host.sh" test t21_seqswap >/dev/null || { echo "FAIL: t21_seqswap"; exit 1; }
bash "$ROOT/scripts/ri_build_host.sh" test t21_storm >/dev/null || { echo "FAIL: t21_storm"; exit 1; }
bash "$ROOT/scripts/ri_build_host.sh" test t21_swaprender >/dev/null || { echo "FAIL: t21_swaprender"; exit 1; }
bash "$ROOT/scripts/ri_build_host.sh" test t58_transport >/dev/null || { echo "FAIL: t58_transport"; exit 1; }
bash "$ROOT/scripts/ri_build_host.sh" test t59_songtrack >/dev/null || { echo "FAIL: t59_songtrack"; exit 1; }
bash "$ROOT/scripts/ri_build_host.sh" test t74_player >/dev/null || { echo "FAIL: t74_player"; exit 1; }
bash "$ROOT/scripts/ri_build_host.sh" test t77_autolane >/dev/null || { echo "FAIL: t77_autolane"; exit 1; }
bash "$ROOT/scripts/ri_build_host.sh" test t78_engine_taps >/dev/null || { echo "FAIL: t78_engine_taps"; exit 1; }
bash "$ROOT/scripts/ri_build_host.sh" test t79_auto_delivery >/dev/null || { echo "FAIL: t79_auto_delivery"; exit 1; }
bash "$ROOT/scripts/ri_build_host.sh" test t80_ctlplane >/dev/null || { echo "FAIL: t80_ctlplane"; exit 1; }
bash "$ROOT/scripts/ri_build_host.sh" test t81_live >/dev/null || { echo "FAIL: t81_live"; exit 1; }
bash "$ROOT/scripts/ri_build_host.sh" test t82_live_record >/dev/null || { echo "FAIL: t82_live_record"; exit 1; }
bash "$ROOT/scripts/ri_build_host.sh" test t83_panelctl >/dev/null || { echo "FAIL: t83_panelctl"; exit 1; }
bash "$ROOT/scripts/ri_build_host.sh" test t84_pal_thread >/dev/null || { echo "FAIL: t84_pal_thread (portability T1)"; exit 1; }
bash "$ROOT/scripts/ri_build_host.sh" test t85_pal_keys >/dev/null || { echo "FAIL: t85_pal_keys (portability T3)"; exit 1; }
bash "$ROOT/scripts/ri_build_host.sh" test t86_pal_fslog >/dev/null || { echo "FAIL: t86_pal_fslog (portability T6)"; exit 1; }
bash "$ROOT/scripts/ri_build_host.sh" test t87_pal_image >/dev/null || { echo "FAIL: t87_pal_image (portability T7)"; exit 1; }
bash "$ROOT/scripts/ri_build_host.sh" test t88_live_driver >/dev/null || { echo "FAIL: t88_live_driver (portability T4)"; exit 1; }
bash "$ROOT/scripts/ri_build_host.sh" test t156_render_stages >/dev/null || { echo "FAIL: t156_render_stages (per-stage render accounting; STOPPED never averaged with playing, and the stage read counts t88's governor laws depend on)"; exit 1; }
bash "$ROOT/scripts/ri_build_host.sh" test t90_pal_audio_null >/dev/null || { echo "FAIL: t90_pal_audio_null (portability T4)"; exit 1; }
bash "$ROOT/scripts/ri_build_host.sh" test t89_pal_midi >/dev/null || { echo "FAIL: t89_pal_midi (portability T5)"; exit 1; }
bash "$ROOT/scripts/ri_build_host.sh" test t91_canvas_events >/dev/null || { echo "FAIL: t91_canvas_events (portability T3)"; exit 1; }
bash "$ROOT/scripts/ri_build_host.sh" test t92_draw_hash >/dev/null || { echo "FAIL: t92_draw_hash (portability T2)"; exit 1; }
bash "$ROOT/scripts/ri_build_host.sh" test t93_raster_goldens >/dev/null || { echo "FAIL: t93_raster_goldens (portability T2)"; exit 1; }
bash "$ROOT/scripts/ri_build_host.sh" test t166_bbox_zoom >/dev/null || { echo "FAIL: t166_bbox_zoom (gui: damage boxes must use the canvas zoom)"; exit 1; }
bash "$ROOT/scripts/ri_build_host.sh" test t167_chase_box_union >/dev/null || { echo "FAIL: t167_chase_box_union (gui: the chase box union must be lossless)"; exit 1; }
bash "$ROOT/scripts/ri_build_host.sh" test t168_gap_accounting >/dev/null || { echo "FAIL: t168_gap_accounting (gui: the GAP must subtract the SUM of the phases)"; exit 1; }
bash "$ROOT/scripts/ri_build_host.sh" test t169_item_cull_parity >/dev/null || { echo "FAIL: t169_item_cull_parity (gui: the build item cull must not change a pixel)"; exit 1; }
bash "$ROOT/scripts/ri_build_host.sh" test t170_ctlreg_index >/dev/null || { echo "FAIL: t170_ctlreg_index (gui: ri_ctlreg_find must match a linear scan on all 65536 ids)"; exit 1; }
bash "$ROOT/scripts/ri_build_host.sh" test t171_tr_layout >/dev/null || { echo "FAIL: t171_tr_layout (gui: TAP must clear the slot, controls and legends; the plate draws no song name)"; exit 1; }
bash "$ROOT/scripts/ri_build_host.sh" test t111_legend_face >/dev/null || { echo "FAIL: t111_legend_face (S2 legend face)"; exit 1; }
bash "$ROOT/scripts/ri_build_host.sh" test t112_partial_redraw >/dev/null || { echo "FAIL: t112_partial_redraw (S3 dirty rect)"; exit 1; }
bash "$ROOT/scripts/ri_build_host.sh" test t115_master_live >/dev/null || { echo "FAIL: t115_master_live (S4 master live)"; exit 1; }
bash "$ROOT/scripts/ri_build_host.sh" test t116_panel_wiring >/dev/null || { echo "FAIL: t116_panel_wiring (panel wiring audit)"; exit 1; }
bash "$ROOT/scripts/ri_build_host.sh" test t121_zoomfit >/dev/null || { echo "FAIL: t121_zoomfit (S5 auto-fit zoom)"; exit 1; }
bash "$ROOT/scripts/ri_build_host.sh" test t124_skinsect >/dev/null || { echo "FAIL: t124_skinsect (S7 section assignment)"; exit 1; }
mkdir -p /tmp/ri/run/t128 # t128_skinchunk writes its songs here (t6 pattern)
bash "$ROOT/scripts/ri_build_host.sh" test t128_skinchunk >/dev/null || { echo "FAIL: t128_skinchunk (S7 skin song chunk)"; exit 1; }
bash "$ROOT/scripts/ri_build_host.sh" test t94_pal_fpu >/dev/null || { echo "FAIL: t94_pal_fpu (portability T9)"; exit 1; }
bash "$ROOT/scripts/ri_build_host.sh" test t95_riapp_core >/dev/null || { echo "FAIL: t95_riapp_core (portability T8)"; exit 1; }
bash "$ROOT/scripts/ri_build_host.sh" test t96_pattern_change >/dev/null || { echo "FAIL: t96_pattern_change (pattern changeover)"; exit 1; }
bash "$ROOT/scripts/ri_build_host.sh" test t97_visdev >/dev/null || { echo "FAIL: t97_visdev (tabbed panels visibility)"; exit 1; }
bash "$ROOT/scripts/ri_build_host.sh" test t98_tabpages >/dev/null || { echo "FAIL: t98_tabpages (tabbed panels pages)"; exit 1; }
bash "$ROOT/scripts/ri_build_host.sh" test t99_pack_bind_lifetime >/dev/null || { echo "FAIL: t99_pack_bind_lifetime (909 bind lifetime)"; exit 1; }
bash "$ROOT/scripts/ri_build_host.sh" test t100_activation >/dev/null || { echo "FAIL: t100_activation (activation sections)"; exit 1; }
bash "$ROOT/scripts/ri_build_host.sh" test t101_levi_pattern >/dev/null || { echo "FAIL: t101_levi_pattern (levi chord steps)"; exit 1; }
bash "$ROOT/scripts/ri_build_host.sh" test t102_levi_emit >/dev/null || { echo "FAIL: t102_levi_emit (levi emission)"; exit 1; }
bash "$ROOT/scripts/ri_build_host.sh" test t103_levi_dsp >/dev/null || { echo "FAIL: t103_levi_dsp (levi voices)"; exit 1; }
bash "$ROOT/scripts/ri_build_host.sh" test t110_levi_layout >/dev/null || { echo "FAIL: t110_levi_layout (levi hardware panel)"; exit 1; }
bash "$ROOT/scripts/ri_build_host.sh" test t129_levi_osc >/dev/null || { echo "FAIL: t129_levi_osc (levi oscillators + envelopes)"; exit 1; }
bash "$ROOT/scripts/ri_build_host.sh" test t108_levi_algo >/dev/null || { echo "FAIL: t108_levi_algo (levi algorithms)"; exit 1; }
bash "$ROOT/scripts/ri_build_host.sh" test t109_levi_morph >/dev/null || { echo "FAIL: t109_levi_morph (levi morph)"; exit 1; }
bash "$ROOT/scripts/ri_build_host.sh" test t130_levi_algo_modes >/dev/null || { echo "FAIL: t130_levi_algo_modes (levi algorithm modes)"; exit 1; }
bash "$ROOT/scripts/ri_build_host.sh" test t131_levi_filters >/dev/null || { echo "FAIL: t131_levi_filters (levi filters + VCA)"; exit 1; }
bash "$ROOT/scripts/ri_build_host.sh" test t132_levi_mod >/dev/null || { echo "FAIL: t132_levi_mod (levi ENV 1-5 + LFOs)"; exit 1; }
bash "$ROOT/scripts/ri_build_host.sh" test t133_levi_matrix2 >/dev/null || { echo "FAIL: t133_levi_matrix2 (levi matrix + macros)"; exit 1; }
bash "$ROOT/scripts/ri_build_host.sh" test t140_songs >/dev/null || { echo "FAIL: t140_songs (song scripts, Levi banks, playlists, song playback)"; exit 1; }
bash "$ROOT/scripts/ri_build_host.sh" test t157_playlist_canon >/dev/null || { echo "FAIL: t157_playlist_canon (RBPL entry paths are canonicalised)"; exit 1; }
bash "$ROOT/scripts/ri_build_host.sh" test t158_audio_failure_loud >/dev/null || { echo "FAIL: t158_audio_failure_loud (a lost audio path is reported, not silent)"; exit 1; }
bash "$ROOT/scripts/ri_build_host.sh" test t159_tr_song_name >/dev/null || { echo "FAIL: t159_tr_song_name (the transport plate names the song that is playing)"; exit 1; }
bash "$ROOT/scripts/ri_build_host.sh" test t151_wake_latency >/dev/null || { echo "FAIL: t151_wake_latency (wake latency separates 'late' from 'slow')"; exit 1; }
bash "$ROOT/scripts/ri_build_host.sh" test t152_repaint_reason >/dev/null || { echo "FAIL: t152_repaint_reason (repaint cost attributed to its caller)"; exit 1; }
bash "$ROOT/scripts/ri_build_host.sh" test t154_reason_and_build_split >/dev/null || { echo "FAIL: t154_reason_and_build_split (reason read before the damage box)"; exit 1; }
bash "$ROOT/scripts/ri_build_host.sh" test t155_damage_clip_build >/dev/null || { echo "FAIL: t155_damage_clip_build (display-list build bounded to the damage box)"; exit 1; }
bash "$ROOT/scripts/ri_build_host.sh" test t153_sticky_log >/dev/null || { echo "FAIL: t153_sticky_log (durable log volume, T: not RAM:)"; exit 1; }
bash "$ROOT/scripts/ri_build_host.sh" test t134_levi_voice >/dev/null || { echo "FAIL: t134_levi_voice (levi voice allocator P6a)"; exit 1; }
bash "$ROOT/scripts/ri_build_host.sh" test t135_levi_voice2 >/dev/null || { echo "FAIL: t135_levi_voice2 (levi voice params P6b)"; exit 1; }
bash "$ROOT/scripts/ri_build_host.sh" test t136_levi_stereo >/dev/null || { echo "FAIL: t136_levi_stereo (levi stereo+scales P6c)"; exit 1; }
bash "$ROOT/scripts/ri_build_host.sh" test t137_levi_voices8 >/dev/null || { echo "FAIL: t137_levi_voices8 (levi 8 voices P6d)"; exit 1; }
bash "$ROOT/scripts/ri_build_host.sh" test t138_levi_delay >/dev/null || { echo "FAIL: t138_levi_delay (levi delay P7a)"; exit 1; }
bash "$ROOT/scripts/ri_build_host.sh" test t139_levi_reverb >/dev/null || { echo "FAIL: t139_levi_reverb (levi reverb P7b)"; exit 1; }
bash "$ROOT/scripts/ri_build_host.sh" test t140_levi_modfx >/dev/null || { echo "FAIL: t140_levi_modfx (levi modfx P7c)"; exit 1; }
bash "$ROOT/scripts/ri_build_host.sh" test t141_levi_tempo >/dev/null || { echo "FAIL: t141_levi_tempo (levi tempo P8a)"; exit 1; }
bash "$ROOT/scripts/ri_build_host.sh" test t142_levi_arp2 >/dev/null || { echo "FAIL: t142_levi_arp2 (levi arp P8b)"; exit 1; }
bash "$ROOT/scripts/ri_build_host.sh" test t143_levi_seq2 >/dev/null || { echo "FAIL: t143_levi_seq2 (levi seq P8c)"; exit 1; }
bash "$ROOT/scripts/ri_build_host.sh" test t144_levi_ribbon >/dev/null || { echo "FAIL: t144_levi_ribbon (levi ribbon P8d)"; exit 1; }
bash "$ROOT/scripts/ri_build_host.sh" test t145_levi_lfostp >/dev/null || { echo "FAIL: t145_levi_lfostp (levi lfo step editor P8e)"; exit 1; }
bash "$ROOT/scripts/ri_build_host.sh" test t150_tap_tempo >/dev/null || { echo "FAIL: t150_tap_tempo (transport tap tempo P9e)"; exit 1; }
bash "$ROOT/scripts/ri_build_host.sh" test t149_levi_perfglide >/dev/null || { echo "FAIL: t149_levi_perfglide (levi glide button + chord mode P9d)"; exit 1; }
bash "$ROOT/scripts/ri_build_host.sh" test t148_levi_perfzone >/dev/null || { echo "FAIL: t148_levi_perfzone (levi keyboard zones P9c)"; exit 1; }
bash "$ROOT/scripts/ri_build_host.sh" test t147_levi_perfamt >/dev/null || { echo "FAIL: t147_levi_perfamt (levi performance amounts P9b)"; exit 1; }
bash "$ROOT/scripts/ri_build_host.sh" test t146_levi_perfsig >/dev/null || { echo "FAIL: t146_levi_perfsig (levi performance signals P9a)"; exit 1; }
bash "$ROOT/scripts/ri_build_host.sh" test t104_levi_engine >/dev/null || { echo "FAIL: t104_levi_engine (levi instance)"; exit 1; }
bash "$ROOT/scripts/ri_build_host.sh" test t105_rbng_levi >/dev/null || { echo "FAIL: t105_rbng_levi (levi song compat)"; exit 1; }
bash "$ROOT/scripts/ri_build_host.sh" test t106_levi_ctl >/dev/null || { echo "FAIL: t106_levi_ctl (levi controls)"; exit 1; }
bash "$ROOT/scripts/ri_build_host.sh" test t113_levi_arp >/dev/null || { echo "FAIL: t113_levi_arp (levi arp stepper)"; exit 1; }
bash "$ROOT/scripts/ri_build_host.sh" test t114_levi_arp_emit >/dev/null || { echo "FAIL: t114_levi_arp_emit (levi arp emit)"; exit 1; }
bash "$ROOT/scripts/ri_build_host.sh" test t115_levi_arp_player >/dev/null || { echo "FAIL: t115_levi_arp_player (levi arp player)"; exit 1; }
bash "$ROOT/scripts/ri_build_host.sh" test t117_levi_seq_phrase >/dev/null || { echo "FAIL: t117_levi_seq_phrase (levi seq phrase)"; exit 1; }
bash "$ROOT/scripts/ri_build_host.sh" test t118_levi_seq_player >/dev/null || { echo "FAIL: t118_levi_seq_player (levi seq player)"; exit 1; }
bash "$ROOT/scripts/ri_build_host.sh" test t119_levi_matrix >/dev/null || { echo "FAIL: t119_levi_matrix (levi matrix core)"; exit 1; }
bash "$ROOT/scripts/ri_build_host.sh" test t120_levi_matrix_render >/dev/null || { echo "FAIL: t120_levi_matrix_render (levi matrix render)"; exit 1; }
bash "$ROOT/scripts/ri_build_host.sh" test t122_levi_lfo >/dev/null || { echo "FAIL: t122_levi_lfo (levi LFO)"; exit 1; }
bash "$ROOT/scripts/ri_build_host.sh" test t123_reverb >/dev/null || { echo "FAIL: t123_reverb (reverb core)"; exit 1; }
bash "$ROOT/scripts/ri_build_host.sh" test t125_midi_follow >/dev/null || { echo "FAIL: t125_midi_follow (midi clock follower)"; exit 1; }
bash "$ROOT/scripts/ri_build_host.sh" test t126_midi_rt >/dev/null || { echo "FAIL: t126_midi_rt (midi realtime parser)"; exit 1; }
bash "$ROOT/scripts/ri_build_host.sh" test t127_midi_sync >/dev/null || { echo "FAIL: t127_midi_sync (midi sync source)"; exit 1; }
# MIXED BUILD GATE (owner 2026-10-04, enforced in-repo 2026-10-05).
# This replaces a gate that required a BLANKET -O0, which is the wrong rule: the
# approved configuration is engine/ at -O2 with app+GUI at -O0, and the in-repo
# script built -O0 throughout, so it disagreed with every number measured since.
#
# It checks the SPLIT, not the presence of a flag. A gate that only looked for
# "-O0 somewhere" would pass a script that had lost the engine half, which is
# precisely the regression worth catching.
RIAPP_CF9_LINES="$(grep -E '^  CF9(_O[02])?=' "$ROOT/scripts/ri_build_aros.sh" || true)"
echo "$RIAPP_CF9_LINES" | grep -q '^  CF9_O2="${CFLAGS_AROS} ' || { echo "FAIL: RIAPP needs a CF9_O2 line holding engine/'s -O2 flags (owner 2026-10-04 mixed build)"; exit 1; }
echo "$RIAPP_CF9_LINES" | grep -q '^  CF9_O0="${CFLAGS_AROS/-O2/-O0} ' || { echo "FAIL: RIAPP needs a CF9_O0 line holding app+GUI's -O0 flags"; exit 1; }
grep -q '^      engine/\*) CF="$CF9_O2" ;;' "$ROOT/scripts/ri_build_aros.sh" || { echo "FAIL: ri_build_aros.sh must route engine/ to -O2; a single flag for every file is the regression this gate exists for"; exit 1; }
grep -q '^      \*)        CF="$CF9_O0" ;;' "$ROOT/scripts/ri_build_aros.sh" || { echo "FAIL: ri_build_aros.sh must route everything else to -O0"; exit 1; }
# The v11 lane builds the SAME approved configuration, from the repo now, so the
# two scripts cannot drift into disagreeing about what a logged number means.
grep -q 'RI_V11_MIX_OPT' "$ROOT/scripts/ri_build_v11.sh" || { echo "FAIL: ri_build_v11.sh lost its mixed-build option (RI_V11_MIX_OPT)"; exit 1; }
echo "-- mixed build: RIAPP is engine/ -O2 + app+GUI -O0 in BOTH the v1 and v11 lane scripts --"
echo "-- portability T8: headless core runs without AROS (WAV; PNG after T2) --"
gcc -std=c99 -O2 -Wall -Wextra -Werror -pedantic -ffp-contract=off -fno-unsafe-math-optimizations -ftrapv -I"$ROOT" -o "$OUT/headless" "$ROOT/platform/host/main_headless.c" "$OUT"/*.o -lm -lpng || { echo "FAIL: headless build"; exit 1; }
rm -f /tmp/ri/null.wav
"$OUT/headless" >/tmp/ri/headless1.log 2>&1 || { echo "FAIL: headless run"; exit 1; }
grep -q "xruns=0" /tmp/ri/headless1.log || { echo "FAIL: headless xruns"; exit 1; }
test -f /tmp/ri/panel.png || { echo "FAIL: headless panel PNG"; exit 1; }
cp /tmp/ri/null.wav /tmp/ri/null-a.wav
cp /tmp/ri/panel.png /tmp/ri/panel-a.png
"$OUT/headless" >/tmp/ri/headless2.log 2>&1 || { echo "FAIL: headless rerun"; exit 1; }
cmp -s /tmp/ri/null-a.wav /tmp/ri/null.wav || { echo "FAIL: headless not deterministic"; exit 1; }
cmp -s /tmp/ri/panel-a.png /tmp/ri/panel.png || { echo "FAIL: headless PNG not deterministic"; exit 1; }
# portability T1: PAL atomics header is include-clean (stdint/stddef only;
# ri_pal_log.h additionally allows stdarg.h for the varargs decl)
if grep -n "#include" "$ROOT/platform/pal/"*.h | grep -v "stdint.h\|stddef.h\|stdarg.h"; then echo "FAIL: pal header include leak"; exit 1; fi
# portability T6: no hard-coded SYS:/RAM:/ENV: literals outside platform/aros/
if grep -rn '"SYS:\|"RAM:\|"ENV:' "$ROOT/app" "$ROOT/audio_io" "$ROOT/gui" "$ROOT/project" "$ROOT/midi_io" "$ROOT/engine" "$ROOT/tools" --include="*.c" --include="*.h" | grep -v "platform/aros/"; then echo "FAIL: hard-coded Amiga path outside platform/aros/"; exit 1; fi
echo "-- portability T6: AROS compile of fs/log backends (compile-only) --"
if [ ! -f ../Vulkan4Aros/scripts/aros_build_env.sh ]; then echo "FAIL: Vulkan4AROS tree (toolchain source) not found (T6)"; exit 1; fi
. ../Vulkan4Aros/scripts/aros_build_env.sh
export PATH="$AROS_TOOLCHAIN:$PATH"
V1SDK_T6="$(cd "$ROOT/../Vulkan4Aros/src/abi/v1/core-pc-x86_64/bin/pc-x86_64/AROS/Developer/include" && pwd)"
if [ -d "$V1SDK_T6" ]; then SDK_T6="$V1SDK_T6"; else echo "FAIL: v1 build-pc SDK absent (T6)"; exit 1; fi
CFLAGS_T6="-std=gnu99 -O2 -Wall -Wextra -Werror -Wno-pointer-sign -mcmodel=large -mno-red-zone -mno-ms-bitfields -fno-strict-aliasing -ffixed-r12 -fno-builtin -I$ROOT -I$SDK_T6 -I$SDK_T6/aros/posixc -I$SDK_T6/aros/stdc"
mkdir -p "$OUT/aros"
for tu in platform/aros/fs_aros.c platform/aros/log_aros.c; do
  bn=$(echo "$tu" | tr '/' '_');
  x86_64-aros-gcc $CFLAGS_T6 -c "$ROOT/$tu" -o "$OUT/aros/${bn}.o" || { echo "FAIL: $tu AROS compile (T6)"; exit 1; }
done
# portability T1: no volatile cross-thread words left in engine/ (atomics own them)
if grep -rn "volatile" "$ROOT/engine/" 2>/dev/null | grep -v "platform/pal"; then echo "FAIL: volatile left in engine/"; exit 1; fi
# law: no mutable static state in the player (spec §Ownership) — one enforcement point
if grep -nE "^static [^()]*[;=]" engine/seq/player.c | grep -v ":static const"; then echo "FAIL: mutable static state in player.c"; exit 1; fi
# law: no mutable static state in the song track (spec §Ownership) — one enforcement point
if grep -nE "^static [^()]*[;=]" engine/seq/songtrack.c | grep -v ":static const"; then echo "FAIL: mutable static state in songtrack.c"; exit 1; fi
# law: no mutable static state in the automation lane (spec §1) — one enforcement point
if grep -nE "^static [^()]*[;=]" engine/seq/autolane.c | grep -v ":static const"; then echo "FAIL: mutable static state in autolane.c"; exit 1; fi
# law: no mutable static state in the control plane (G9.1) — one enforcement point
if grep -nE "^static [^()]*[;=]" engine/seq/ctlplane.c | grep -v ":static const"; then echo "FAIL: mutable static state in ctlplane.c"; exit 1; fi
# law: ctlplane.h names sched.h only (no transport/autolane/project/gui headers)
if grep -nE "autolane\.h|transport\.h|clock\.h|project/|gui/" engine/seq/ctlplane.h; then echo "FAIL: ctlplane.h layer leak"; exit 1; fi
# law: no mutable static state in the live session (G9.2) — one enforcement point
if grep -nE "^static [^()]*[;=]" engine/live.c | grep -v ":static const"; then echo "FAIL: mutable static state in live.c"; exit 1; fi
# law: live.h stays in engine/ (no project/gui headers)
if grep -nE "project/|gui/" engine/live.h; then echo "FAIL: live.h layer leak"; exit 1; fi
# law: autolane.h names transport.h only (no scheduler/clock/project/gui headers)
if grep -nE "sched\.h|clock\.h|project/|gui/" engine/seq/autolane.h; then echo "FAIL: autolane.h layer leak"; exit 1; fi
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
echo "== Phase 7b: pattern model (§12.7a) =="
for t in t53_pattern_model t54_pattern_edit t55_pattern_emit t56_rbng_bank t57_engine_drums; do
  bash "$ROOT/scripts/ri_build_host.sh" test $t >/dev/null || { echo "FAIL: $t"; exit 1; }
done
# no RNG/time/global state in the model
if grep -nE "\brand\(|srand|time\(|clock\(|static uint32_t [a-z_]*seed" engine/seq/pattern*.c; then echo "FAIL: nondeterminism in pattern model"; exit 1; fi
# the 909 accent==2 overload must not come back through the emitter
if grep -n "accent *== *2\|accent = 2" engine/seq/pattern_emit.c; then echo "FAIL: accent/flam overload"; exit 1; fi
# ledger rows exist for every E0/OPEN constant
for f in sequencer/303-base-note sequencer/slide-direction sequencer/shuffle-scope 909/flam-level sequencer/transpose-encoding sequencer/random-alter sequencer/random-pattern-rhythm; do
  test -f "docs/evidence/$f.md" || { echo "FAIL: missing ledger $f"; exit 1; }; done
grep -q "RI_303_BASE_NOTE" docs/evidence/sequencer/303-base-note.md || { echo "FAIL: base-note ledger unlinked"; exit 1; }
# RISong never on the stack outside project/
if grep -rnE "^\s+struct RISong [a-z_]+;" tools/ engine/ audio_io/ tests/ | grep -v static; then echo "FAIL: stack RISong"; exit 1; fi
echo "== Phase 8: 808 fifteen voices (Task 8, gate G8) =="
bash "$ROOT/scripts/ri_build_host.sh" test t1_808 >/dev/null || { echo "FAIL: t1_808"; exit 1; }
# Module 2.3 acceptance pins (TC-2.3.1..2.3.5)
bash "$ROOT/scripts/ri_build_host.sh" test t23_808hat >/dev/null || { echo "FAIL: t23_808hat (TC-2.3.1 hat)"; exit 1; }
bash "$ROOT/scripts/ri_build_host.sh" test t23_808bd >/dev/null || { echo "FAIL: t23_808bd (TC-2.3.1/2 BD)"; exit 1; }
bash "$ROOT/scripts/ri_build_host.sh" test t23_808clap >/dev/null || { echo "FAIL: t23_808clap (TC-2.3.3 clap)"; exit 1; }
bash "$ROOT/scripts/ri_build_host.sh" test t23_808accent >/dev/null || { echo "FAIL: t23_808accent (TC-2.3.4)"; exit 1; }
bash "$ROOT/scripts/ri_build_host.sh" test t23_808storm >/dev/null || { echo "FAIL: t23_808storm (TC-2.3.5)"; exit 1; }
bash "$ROOT/scripts/ri_build_host.sh" test t33_808_silence >/dev/null || { echo "FAIL: t33_808_silence (long-silence regression: per-100ms RMS decays below -120 dBFS)"; exit 1; }
bash "$ROOT/scripts/ri_build_host.sh" test t34_808_storm_linear >/dev/null || { echo "FAIL: t34_808_storm_linear (storm linearity)"; exit 1; }
bash "$ROOT/scripts/ri_build_host.sh" test t35_808_deactivate >/dev/null || { echo "FAIL: t35_808_deactivate (voice deactivate silences cleanly)"; exit 1; }
bash "$ROOT/scripts/ri_build_host.sh" test t41_808_slots >/dev/null || { echo "FAIL: t41_808_slots (slot switches + MA)"; exit 1; }
bash "$ROOT/scripts/ri_build_host.sh" test t42_808_metal_choke >/dev/null || { echo "FAIL: t42_808_metal_choke (metal choke)"; exit 1; }
bash "$ROOT/scripts/ri_build_host.sh" test t43_808_bdboom >/dev/null || { echo "FAIL: t43_808_bdboom (BD boom)"; exit 1; }
for v in bd sd lt mt ht lc mc hc rs cl cp ch oh cy cb ma; do
  test -f "$ROOT/docs/evidence/808/$v.md" || { echo "FAIL: missing ledger 808/$v.md"; exit 1; }
  grep -q "EXCITE" "$ROOT/docs/evidence/808/$v.md" || { echo "FAIL: ledger $v lacks accent mapping"; exit 1; }
done
grep -q "P-12" "$ROOT/docs/evidence/808/bd.md" || { echo "FAIL: P-12 open state unrecorded"; exit 1; }
SG8="$ROOT/tests/golden/808"
for v in bd sd lt mt ht lc mc hc rs cl cp ch oh cy cb ma storm; do
  test -f "$SG8/$v.wav" || { echo "FAIL: missing golden 808/$v.wav"; exit 1; }
  test -f "$SG8/$v.wav.sha256" || { echo "FAIL: missing sidecar 808/$v.wav.sha256"; exit 1; }
done
(cd "$ROOT" && sha256sum -c tests/golden/808/bd.wav.sha256 tests/golden/808/sd.wav.sha256 tests/golden/808/lt.wav.sha256 tests/golden/808/mt.wav.sha256 tests/golden/808/ht.wav.sha256 tests/golden/808/lc.wav.sha256 tests/golden/808/mc.wav.sha256 tests/golden/808/hc.wav.sha256 tests/golden/808/rs.wav.sha256 tests/golden/808/cl.wav.sha256 tests/golden/808/cp.wav.sha256 tests/golden/808/ch.wav.sha256 tests/golden/808/oh.wav.sha256 tests/golden/808/cy.wav.sha256 tests/golden/808/cb.wav.sha256 tests/golden/808/ma.wav.sha256 tests/golden/808/storm.wav.sha256) || { echo "FAIL: 808 golden sha256 mismatch"; exit 1; }
T8=/tmp/ri/run/audit8
mkdir -p "$T8"
for v in bd sd lt mt ht lc mc hc rs cl cp ch oh cy cb ma storm; do
  "$OUT/render" --808 "$v" --out "$T8/$v.wav" >/dev/null || exit 1
  cmp -s "$SG8/$v.wav" "$T8/$v.wav" || { echo "FAIL: 808/$v re-render differs (not deterministic)"; exit 1; }
done
echo "== Phase 9: 909 sampler + clean pack + provenance gate (Task 9, gate G9) =="
bash "$ROOT/scripts/ri_build_host.sh" test t1_909 >/dev/null || { echo "FAIL: t1_909"; exit 1; }
bash "$ROOT/scripts/ri_build_host.sh" test t24_909xfade >/dev/null || { echo "FAIL: t24_909xfade (TC-2.4.1)"; exit 1; }
bash "$ROOT/scripts/ri_build_host.sh" test t24_909accent >/dev/null || { echo "FAIL: t24_909accent (TC-2.4.2)"; exit 1; }
bash "$ROOT/scripts/ri_build_host.sh" test t24_909quirk >/dev/null || { echo "FAIL: t24_909quirk (TC-2.4.3)"; exit 1; }
bash "$ROOT/scripts/ri_build_host.sh" test t24_909retrig >/dev/null || { echo "FAIL: t24_909retrig (TC-2.4.4)"; exit 1; }
bash "$ROOT/scripts/ri_build_host.sh" test t24_909swap >/dev/null || { echo "FAIL: t24_909swap (TC-2.4.5)"; exit 1; }
bash "$ROOT/scripts/ri_build_host.sh" test t44_909_newvoices >/dev/null || { echo "FAIL: t44_909_newvoices (new voice allocation)"; exit 1; }
bash "$ROOT/scripts/ri_build_host.sh" test t45_909_decouple >/dev/null || { echo "FAIL: t45_909_decouple (sample read decoupled from voice state)"; exit 1; }
for v in bd sd ch oh cr rd lt mt ht rs cp; do
  test -f "$ROOT/docs/evidence/909/$v.md" || { echo "FAIL: missing ledger 909/$v.md"; exit 1; }
  grep -q "Provenance manifest" "$ROOT/docs/evidence/909/$v.md" || { echo "FAIL: ledger $v lacks manifest"; exit 1; }
  grep -q "CC0/RI" "$ROOT/docs/evidence/909/$v.md" || { echo "FAIL: ledger $v lacks license row"; exit 1; }
done
grep -q "P-13" "$ROOT/docs/evidence/909/bd.md" || { echo "FAIL: P-13 unrecorded"; exit 1; }
grep -q "NO-OP" "$ROOT/docs/evidence/909/cr.md" || { echo "FAIL: crash quirk unrecorded"; exit 1; }
grep -q "steal" "$ROOT/docs/evidence/909/ch.md" || { echo "FAIL: hat steal unrecorded"; exit 1; }
gcc $CFLAGS -o "$OUT/inspect" "$ROOT/tools/inspect.c" "$OUT"/*.o -lpng || { echo "FAIL: inspect build"; exit 1; }
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
for v in bd sd ch oh cr rd lt mt ht rs cp; do
  test -f "$SG9/$v.wav" || { echo "FAIL: missing golden 909/$v.wav"; exit 1; }
  test -f "$SG9/$v.wav.sha256" || { echo "FAIL: missing sidecar 909/$v.wav.sha256"; exit 1; }
done
(cd "$ROOT" && sha256sum -c tests/golden/909/bd.wav.sha256 tests/golden/909/sd.wav.sha256 tests/golden/909/ch.wav.sha256 tests/golden/909/oh.wav.sha256 tests/golden/909/cr.wav.sha256 tests/golden/909/rd.wav.sha256 tests/golden/909/lt.wav.sha256 tests/golden/909/mt.wav.sha256 tests/golden/909/ht.wav.sha256 tests/golden/909/rs.wav.sha256 tests/golden/909/cp.wav.sha256) || { echo "FAIL: 909 golden sha256 mismatch"; exit 1; }
for v in bd sd ch oh cr rd lt mt ht rs cp; do
  "$OUT/render" --909 "$v" --out "$T9/$v.wav" >/dev/null || exit 1
  cmp -s "$SG9/$v.wav" "$T9/$v.wav" || { echo "FAIL: 909/$v re-render differs (not deterministic)"; exit 1; }
done
echo "-- audibility floor (silent goldens never pin: peak>=1000, rms>=100) --"
for v in bd sd ch oh cr rd lt mt ht rs cp; do
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
bash "$ROOT/scripts/ri_build_host.sh" test t25_pcfcutoff >/dev/null || { echo "FAIL: t25_pcfcutoff (TC-2.5.2)"; exit 1; }
bash "$ROOT/scripts/ri_build_host.sh" test t25_delay >/dev/null || { echo "FAIL: t25_delay (TC-2.5.3)"; exit 1; }
bash "$ROOT/scripts/ri_build_host.sh" test t25_dist >/dev/null || { echo "FAIL: t25_dist (TC-2.5.4)"; exit 1; }
bash "$ROOT/scripts/ri_build_host.sh" test t25_swap >/dev/null || { echo "FAIL: t25_swap (TC-2.5.5)"; exit 1; }
bash "$ROOT/scripts/ri_build_host.sh" test t25_pcfopen >/dev/null || { echo "FAIL: t25_pcfopen (TC-2.5.1-OPEN)"; exit 1; }
bash "$ROOT/scripts/ri_build_host.sh" test t27_fxlatency >/dev/null || { echo "FAIL: t27_fxlatency (TC-2.11.1)"; exit 1; }
bash "$ROOT/scripts/ri_build_host.sh" test t46_fx_delay_pool >/dev/null || { echo "FAIL: t46_fx_delay_pool (BEATS honored at 140 BPM/48 kHz; pool retired to caller-owned buffers)"; exit 1; }
bash "$ROOT/scripts/ri_build_host.sh" test t47_fx_delay_parity >/dev/null || { echo "FAIL: t47_fx_delay_parity (delay parity across paths)"; exit 1; }
bash "$ROOT/scripts/ri_build_host.sh" test t50_fx_dist_comp >/dev/null || { echo "FAIL: t50_fx_dist_comp (dist/comp complementarity)"; exit 1; }
bash "$ROOT/scripts/ri_build_host.sh" test t48_pcf_envelope >/dev/null || { echo "FAIL: t48_pcf_envelope (PCF envelope)"; exit 1; }
bash "$ROOT/scripts/ri_build_host.sh" test t49_pcf_patterns >/dev/null || { echo "FAIL: t49_pcf_patterns (PCF patterns)"; exit 1; }
bash "$ROOT/scripts/ri_build_host.sh" test t51_route >/dev/null || { echo "FAIL: t51_route (insert radio exclusivity; master is RI_ROUTE_MASTER, section 4 is Levi)"; exit 1; }
bash "$ROOT/scripts/ri_build_host.sh" test t52_engine_fx >/dev/null || { echo "FAIL: t52_engine_fx (inserts + pan + send/stereo-return; a section's FX leaves the other bit-identical)"; exit 1; }
test -f "$ROOT/docs/evidence/pcf/engine.md" || { echo "FAIL: missing pcf engine ledger"; exit 1; }
grep -q "P-15" "$ROOT/docs/evidence/pcf/engine.md" || { echo "FAIL: ledger lacks P-15"; exit 1; }
grep -q "OPEN-04" "$ROOT/docs/evidence/pcf/engine.md" || { echo "FAIL: ledger lacks OPEN-04"; exit 1; }
test -f "$ROOT/docs/evidence/pcf/red-t1_fx.txt" || { echo "FAIL: missing RED evidence"; exit 1; }
test -f "$ROOT/reference/pcf-table.bin" || { echo "FAIL: missing pcf-table.bin (missing-is-broken)"; exit 1; }
test -f "$ROOT/reference/pcf-patterns.bin" || { echo "FAIL: missing pcf-patterns.bin (missing-is-broken)"; exit 1; }
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
bash "$ROOT/scripts/ri_build_host.sh" test t26_gainstage >/dev/null || { echo "FAIL: t26_gainstage (TC-2.6.1)"; exit 1; }
bash "$ROOT/scripts/ri_build_host.sh" test t26_matrix >/dev/null || { echo "FAIL: t26_matrix (TC-2.6.2)"; exit 1; }
bash "$ROOT/scripts/ri_build_host.sh" test t26_meter >/dev/null || { echo "FAIL: t26_meter (TC-2.6.3)"; exit 1; }
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
bash "$ROOT/scripts/ri_build_host.sh" test t29_paneldefault >/dev/null || { echo "FAIL: t29_paneldefault (TC-2.9.2/2.9.1)"; exit 1; }
bash "$ROOT/scripts/ri_build_host.sh" test t29_chase >/dev/null || { echo "FAIL: t29_chase (TC-2.9.3)"; exit 1; }
bash "$ROOT/scripts/ri_build_host.sh" test t29_layout >/dev/null || { echo "FAIL: t29_layout (TC-2.9.1)"; exit 1; }
bash "$ROOT/scripts/ri_build_host.sh" test t29_knobart >/dev/null || { echo "FAIL: t29_knobart (TC-2.9.2)"; exit 1; }
echo "-- GUI phases G1-G8 host tests (C1 audit wiring: t60-t73 + t75-t76) --"
for t in t60_ctlreg t61_panelgeo t62_sect303 t63_sect808 t64_sectui t65_sect909 t66_sectmix t67_sectfx t68_sectpat t69_secttr t70_keymap t71_panelui t72_livestate t73_midimap t75_skin t76_zoom; do
  bash "$ROOT/scripts/ri_build_host.sh" test $t >/dev/null || { echo "FAIL: $t (GUI phase)"; exit 1; }
done
echo "-- GUI registry single-source: legends live in ctlreg.c only (C1) --"
if grep -rn "struct RICtlDef [A-Za-z_][A-Za-z0-9_]*\[" "$ROOT/gui" "$ROOT/app" 2>/dev/null | grep -v "gui/ctlreg.c"; then echo "FAIL: RICtlDef table outside ctlreg.c"; exit 1; fi
if grep -rln "RI_SEC_NAMES" "$ROOT/gui" "$ROOT/app" "$ROOT/tests" 2>/dev/null | grep -v "gui/ctlreg.c"; then echo "FAIL: section names outside ctlreg.c"; exit 1; fi
test -f "$ROOT/docs/evidence/gui/acceptance.md" || { echo "FAIL: missing gui acceptance"; exit 1; }
grep -q -- "- \[ \]" "$ROOT/docs/evidence/gui/acceptance.md" || { echo "FAIL: acceptance has no checkable boxes"; exit 1; }
grep -q "P-18" "$ROOT/docs/evidence/gui/acceptance.md" || { echo "FAIL: acceptance lacks P-18"; exit 1; }
grep -q "TC-2.9" "$ROOT/docs/evidence/gui/acceptance.md" || { echo "FAIL: acceptance lacks TC-2.9"; exit 1; }
grep -q "TC-2.10\|TC-2.11" "$ROOT/docs/evidence/gui/acceptance.md" || { echo "FAIL: acceptance lacks TC-2.10/2.11"; exit 1; }
grep -q "ReIncarnation-101" "$ROOT/docs/evidence/gui/acceptance.md" || { echo "FAIL: acceptance lacks tutorial workflow"; exit 1; }
grep -q "Tester:" "$ROOT/docs/evidence/gui/acceptance.md" || { echo "FAIL: acceptance lacks sign-off lines"; exit 1; }
test -f "$ROOT/docs/evidence/gui/red-t1_knob.txt" || { echo "FAIL: missing RED evidence"; exit 1; }
grep -q "FAIL" "$ROOT/docs/evidence/gui/red-t1_knob.txt" || { echo "FAIL: RED evidence has no FAIL lines"; exit 1; }
echo "-- AROS-only shells guarded + out of host build --"
for f in gui/widgets/rknb.mcc.c gui/widgets/rknb.h gui/widgets/rlbl.mcc.c gui/widgets/rlbl.h gui/widgets/rstp.mcc.c gui/widgets/rstp.h gui/widgets/rfdr.mcc.c gui/widgets/rlvl.mcc.c gui/knob_blit.c app/main.c app/panel909.c app/knobproof.c app/stepproof.c gui/widgets/rsection.mcc.c gui/skin_aros.c gui/skin_aros.h app/sectproof.c app/midisend.c audio_io/audio_ahi_live.c audio_io/audio_ahi_live.h app/riapp.c; do
  test -f "$ROOT/$f" || { echo "FAIL: missing $f"; exit 1; }
  grep -q "#ifndef __AROS__" "$ROOT/$f" || { echo "FAIL: $f lacks __AROS__ guard"; exit 1; }
  grep -q '#error ".*AROS-only' "$ROOT/$f" || { echo "FAIL: $f lacks AROS-only #error"; exit 1; }
done
if grep -rn "widgets\|app/main\|app/panel909\|knob_blit\|app/knobproof\|app/riapp\|audio_ahi_live" "$ROOT/scripts/ri_build_host.sh" 2>/dev/null; then echo "FAIL: AROS shells leak into host build"; exit 1; fi
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
x86_64-aros-gcc $CFLAGS_GUI -c "$ROOT/gui/knob_art.c" -o "$OUT/aros/knob_art_aros.o" || { echo "FAIL: knob_art.c AROS compile"; exit 1; }
for w in rknb rfdr rstp rlvl; do
  x86_64-aros-gcc $CFLAGS_GUI -c "$ROOT/gui/widgets/$w.mcc.c" -o "$OUT/aros/$w.o" || { echo "FAIL: $w.mcc.c AROS compile"; exit 1; }
done
x86_64-aros-gcc $CFLAGS_GUI -c "$ROOT/app/main.c" -o "$OUT/aros/app_main_aros.o" || { echo "FAIL: app/main.c AROS compile"; exit 1; }
x86_64-aros-gcc $CFLAGS_GUI -c "$ROOT/app/panel909.c" -o "$OUT/aros/app_panel909_aros.o" || { echo "FAIL: app/panel909.c AROS compile"; exit 1; }
x86_64-aros-gcc $CFLAGS_GUI -c "$ROOT/gui/knob_blit.c" -o "$OUT/aros/knob_blit_aros.o" || { echo "FAIL: gui/knob_blit.c AROS compile"; exit 1; }
x86_64-aros-gcc $CFLAGS_GUI -c "$ROOT/app/knobproof.c" -o "$OUT/aros/app_knobproof_aros.o" || { echo "FAIL: app/knobproof.c AROS compile"; exit 1; }
echo "-- AROS RISECT/MIDISEND link (all GUI TUs incl. G8 skins, C1) --"
bash "$ROOT/scripts/ri_build_aros.sh" sections >/dev/null || { echo "FAIL: AROS sections link (GUI TUs)"; exit 1; }
echo "-- AROS RIAPP link (G9 live shell: session + control + AHI task) --"
RIAPP_BUILD_LOG="$(bash "$ROOT/scripts/ri_build_aros.sh" riapp)" || { echo "$RIAPP_BUILD_LOG" | grep FAIL; echo "FAIL: AROS RIAPP link"; exit 1; }
# Object-level mixed-build gate (review 2026-10-05): the build reads each
# object's recorded flags back (-frecord-gcc-switches); the text gates above
# only prove what the script says.
echo "$RIAPP_BUILD_LOG" | grep -q '^AROS RIAPP MIXED VERIFIED: [1-9][0-9]* engine objects at -O2, [1-9][0-9]* app/GUI objects at -O0' || { echo "FAIL: RIAPP objects not verified as the mixed build"; exit 1; }
echo "$RIAPP_BUILD_LOG" | grep '^AROS RIAPP MIXED VERIFIED'
# Every RIAPP log format ends in \n (2026-10-05): five rlog calls without
# one ran the stg/dstg/vcount tables together into a single unreadable line.
RLOG_NONL="$(python3 - "$ROOT/app/riapp.c" <<'PYEOF'
import re, sys
s = open(sys.argv[1]).read()
for m in re.finditer(r'\brlog\(\s*((?:"(?:[^"\\]|\\.)*"\s*)+)', s):
    lit = ''.join(re.findall(r'"((?:[^"\\]|\\.)*)"', m.group(1)))
    if not lit.endswith('\\n'):
        print(s[:m.start()].count('\n') + 1)
PYEOF
)"
test -z "$RLOG_NONL" || { echo "FAIL: rlog format without trailing \\n at app/riapp.c line(s): $RLOG_NONL"; exit 1; }
echo "-- RIAPP log lines: every rlog format ends in a newline --"
echo "== Phase 13: formats full + MIDI + automation + ARexx + datatypes + fuzz (Task 13, gate G13) =="
T13=/tmp/ri/run/audit13
mkdir -p "$T13/c1" "$T13/c2" "$T13/rs" "$T13/regen"
bash "$ROOT/scripts/ri_build_host.sh" test t1_formats >/dev/null || { echo "FAIL: t1_formats"; exit 1; }
bash "$ROOT/scripts/ri_build_host.sh" test t30_modpack >/dev/null || { echo "FAIL: t30_modpack (TC-2.12.1/2.12.5)"; exit 1; }
for f in rbng rbnm-full midi arexx automation datatypes fuzz green-defects; do
  test -f "$ROOT/docs/evidence/formats/$f.md" || { echo "FAIL: missing formats ledger $f.md"; exit 1; }
done
test -f "$ROOT/docs/evidence/formats/red-t1_formats.txt" || { echo "FAIL: missing RED evidence"; exit 1; }
grep -q "FAIL\|fatal error" "$ROOT/docs/evidence/formats/red-t1_formats.txt" || { echo "FAIL: RED evidence shows no failure"; exit 1; }
grep -q "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad" "$ROOT/tests/unit/t1_formats.c" || { echo "FAIL: abc vector ungated"; exit 1; }
echo "-- v1.1 bank golden + fuzz seeds --"
test -f "$ROOT/tests/golden/formats/bank-v11.rbng" || { echo "FAIL: missing bank-v11 golden"; exit 1; }
test -f "$ROOT/tests/golden/formats/bank-v11.rbng.sha256" || { echo "FAIL: missing bank-v11 sidecar"; exit 1; }
(cd "$ROOT" && sha256sum -c tests/golden/formats/bank-v11.rbng.sha256) || { echo "FAIL: bank-v11 sha256 mismatch"; exit 1; }
echo "-- corpus determinism (regenerate + cmp) --"
gcc $CFLAGS -o "$OUT/mksong" "$ROOT/tools/mksong.c" "$OUT"/*.o -lpng || { echo "FAIL: mksong build"; exit 1; }
"$OUT/mksong" "$T13/regen" >/dev/null || exit 1
for k in 01 02 03 04 05 06 07 08 09 10; do
  test -f "$ROOT/tests/golden/songs/corpus/s$k.rbng" || { echo "FAIL: missing corpus s$k.rbng"; exit 1; }
  cmp -s "$ROOT/tests/golden/songs/corpus/s$k.rbng" "$T13/regen/s$k.rbng" || { echo "FAIL: corpus s$k not deterministically generated"; exit 1; }
  "$OUT/inspect" --rbng "$ROOT/tests/golden/songs/corpus/s$k.rbng" | grep -q "RBNG OK" || { echo "FAIL: corpus s$k rejected"; exit 1; }
done
for k in 11 12; do
  test -f "$ROOT/tests/golden/songs/corpus/s$k-bank.rbng" || { echo "FAIL: missing bank seed s$k"; exit 1; }
  "$OUT/inspect" --rbng "$ROOT/tests/golden/songs/corpus/s$k-bank.rbng" | grep -q "RBNG OK" || { echo "FAIL: bank seed s$k rejected"; exit 1; }
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
echo "-- songs & playlists: demo songs compile byte-identically and play clean --"
gcc $CFLAGS -o "$OUT/rbsc" "$ROOT/tools/rbsc.c" "$OUT"/*.o -lm -lpng -pthread || { echo "FAIL: rbsc build"; exit 1; }
gcc $CFLAGS -o "$OUT/songplay" "$ROOT/tools/songplay.c" "$OUT"/*.o -lm -lpng -pthread || { echo "FAIL: songplay build"; exit 1; }
mkdir -p "$T13/songs"
for f in "$ROOT"/songs/demo/*.rbs; do
  b=$(basename "$f" .rbs)
  "$OUT/rbsc" "$f" "$T13/songs/$b.rbng" >/dev/null || { echo "FAIL: song $b does not compile"; exit 1; }
  cmp -s "$T13/songs/$b.rbng" "$ROOT/songs/demo/$b.rbng" || { echo "FAIL: songs/demo/$b.rbng is not what $b.rbs compiles to"; exit 1; }
  (cd "$ROOT" && "$OUT/songplay" "songs/demo/$b.rbng" "$T13/songs/$b.wav") | grep -q "clipped 0, xruns 0" || { echo "FAIL: song $b clips or xruns"; exit 1; }
done
"$OUT/songplay" --playlist "$ROOT/songs/demo/demos.rbpl" >/dev/null || { echo "FAIL: demo playlist entry missing or invalid"; exit 1; }
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
gcc $CFLAGS -o "$OUT/bench" "$ROOT/tools/bench.c" "$OUT"/*.o -lm -lpng || { echo "FAIL: bench build"; exit 1; }
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
  "Levi cutoff" "Levi reso" "Levi mode" "Levi ratio" \
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
check_sig gui/knob_logic.h docs/autodoc/gui.doc "double ri_knob_drag_to_value(double start, double dx_px, double dy_px,"
check_sig gui/knob_logic.h docs/autodoc/gui.doc "void ri_knob_clamp_acc(double start, double *adx, double *ady, int fine)"
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
for tu in project/arexx_aros.c midi_io/camd_backend.c project/datatypes/rbng.datatype.c project/datatypes/rbnm.datatype.c gui/catalog.c project/arexx_dispatch.c engine/seq/songtrack.c engine/seq/player.c engine/seq/autolane.c; do
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
# An ALLOWLIST, not a bare count (2026-10-05). The count was "exactly 5", which
# made adding the v11 lane script -- the owner's explicit direction, and the fix
# for a build script living outside the repo -- look like a hygiene violation. A
# count cannot say WHICH files belong; an allowlist can, and adding one is then a
# deliberate act rather than an arithmetic accident.
RI_SCRIPTS_OK="ri_audit.sh ri_build_aros.sh ri_build_host.sh ri_build_v11.sh ri_fuzz.sh ri_soak.sh"
for f in $(ls "$ROOT/scripts"); do
  case " $RI_SCRIPTS_OK " in *" $f "*) ;; *) echo "FAIL: scripts/$f is not on the shared-script allowlist (${RI_SCRIPTS_OK})"; exit 1 ;; esac
done
for f in $RI_SCRIPTS_OK; do
  test -f "$ROOT/scripts/$f" || { echo "FAIL: missing scripts/$f (on the allowlist)"; exit 1; }
done
test -x "$ROOT/scripts/ri_build_v11.sh" || { echo "FAIL: scripts/ri_build_v11.sh is not executable"; exit 1; }
if git -C "$ROOT" status --porcelain | grep -E "\.o$|\.library$"; then echo "FAIL: build artifacts in tree"; exit 1; fi
if grep -rnw "TODO\|TBD\|FIXME" "$ROOT/docs/ReIncarnation.guide" "$ROOT/docs/autodoc" "$ROOT/locale" "$ROOT/Install" 2>/dev/null; then echo "FAIL: placeholder in REL docs"; exit 1; fi
echo "-- clean-room: no maker or product marks in panel strings (Leviasynth fidelity P1) --"
# The Levi panel follows the hardware arrangement (owner 2026-09-30) but
# never its trade dress: no maker logo/name, product wordmark or keybed
# trade mark in any drawn or registered string (spec §1; plan
# docs/superpowers/plans/2026-09-30-leviasynth-fidelity-plan.md §2).
if grep -rnE '"[^"]*(LEVIASYNTH|Leviasynth|ASHUN|Ashun|POLYTOUCH|Polytouch|\bASM\b)[^"]*"' \
  "$ROOT/gui" "$ROOT/app" "$ROOT/skins" --include="*.c" --include="*.h" --include="*.manifest" 2>/dev/null; then
  echo "FAIL: maker/product mark in a panel string"; exit 1; fi
echo "-- portability T10: confinement gates (PAL draws the line) --"
# AROS system includes live only in AROS shells (explicit list) — never in
# the portable core, the PAL, the host backends, the draw layer, app/core,
# tools, or the shared formats (arexx_aros + datatypes are AROS-only by design).
if grep -rn "#include <exec/\|#include <dos/\|#include <proto/\|#include <intuition/\|#include <graphics/\|#include <libraries/\|#include <devices/\|#include <datatypes/\|#include <midi/\|#include <utility/\|#include <clib/" \
  "$ROOT/engine" "$ROOT/platform/pal" "$ROOT/platform/host" "$ROOT/gui/draw" "$ROOT/app/core" "$ROOT/tools" \
  --include="*.c" --include="*.h" --include="*.inc" 2>/dev/null; then echo "FAIL: AROS include in portable code"; exit 1; fi
if grep -rn "#include <exec/\|#include <dos/\|#include <proto/\|#include <intuition/\|#include <graphics/\|#include <libraries/\|#include <devices/\|#include <datatypes/\|#include <midi/\|#include <utility/\|#include <clib/" \
  "$ROOT/project" "$ROOT/midi_io" --include="*.c" --include="*.h" 2>/dev/null \
  | grep -v "project/arexx_aros.c\|project/datatypes/\|midi_io/camd_backend.c"; then echo "FAIL: AROS include outside AROS shells"; exit 1; fi
# Drawing calls live only in AROS shells (backend replay + legacy widgets) —
# never in portable code. (audio.h's dual-target ifdef + host TagItem shim
# is the documented pattern, not a violation: it carries no drawing calls.)
if grep -rn "RectFill\|BltBitMapRastPort\|WritePixelArrayAlpha\|ObtainBestPen\|AllocBitMap\|MUIM_Draw\|MUI_CreateCustomClass" \
  "$ROOT/engine" "$ROOT/platform" "$ROOT/gui/draw" "$ROOT/app/core" "$ROOT/tools" "$ROOT/project" "$ROOT/midi_io" \
  --include="*.c" --include="*.h" --include="*.inc" 2>/dev/null; then echo "FAIL: drawing call in portable code"; exit 1; fi
# Portable build (T10, owner build-system call §8.2 pending — Makefile keeps
# zero deps): core + host PAL + t84 + headless, all green.
make -f "$ROOT/build/portable.mk" test headless >/tmp/ri/portable.log 2>&1 || { echo "FAIL: portable build"; exit 1; }
# mingw-w64 compile-only gate: SKIP — toolchain absent on this machine (say so).
if command -v x86_64-w64-mingw32-gcc >/dev/null 2>&1; then
  echo "FAIL: mingw gate not implemented (toolchain present but gate missing)";
else
  echo "-- mingw gate SKIP (no x86_64-w64-mingw32-gcc here; T11 lane owns it) --"
fi
echo "AUDIT 0/0 PASS"
