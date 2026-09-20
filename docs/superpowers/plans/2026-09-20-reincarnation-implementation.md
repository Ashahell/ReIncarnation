# ReIncarnation Implementation Plan v2

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build ReIncarnation Classic (RB-338-class groovebox on AROS) phase by phase, each phase ending in a tested, committable deliverable, starting with one 303 rendering one pattern offline.

**Architecture:** Single-threaded Classic render path (scheduler → 4 device DSPs → FX → mixer → master) shared identically by live AHI playback and offline WAV export; host-GCC TDD for all DSP, AROS cross-build only at integration boundaries.

**Tech Stack:** C (C99, AROS SDK), host GCC for DSP/tests, `x86_64-aros-gcc` 16.1.0 for AROS targets, `ahi.device` + `camd.library` + Zune/MUI on AROS, IFF `FORM` files, SHA-256 golden fixtures.

**Spec:** `docs/superpowers/specs/2026-09-20-reincarnation-spec.md` — the plan argues from the spec, so the spec travels with it; executors read both.

**Plan v2** (applies implementation-plan review 2026-09-20: target-driven build, measured kernels, multi-segment clock, ledger-first first light, split W1, hardened late tasks, lab rules, numbered gates).

## Global Constraints

Every task's requirements implicitly include this section.

- Render path: no dynamic allocation, no DOS, no GUI, no blocking IPC, no unbounded loops, no `Forbid()`/`Disable()`; SPSC FIFO + buffer-boundary snapshots; CI audit gate greps for violations.
- Exactly one DSP/render implementation for live playback and offline export.
- Determinism: D0 (same project ⇒ same events) + D1 (same binary/arch ⇒ identical PCM) only; never claim D2.
- Every compatibility claim carries an evidence class (E0–E5); progress declared only via parity matrix + `docs/evidence/` ledger rows.
- No W3 code in Classic paths: `RI_ENGINE_CLASSIC` vs `RI_ENGINE_POWER` separation, never `if (power_mode)` in DSP.
- All audio/art assets newly synthesized, recorded, or licensed; per-sample provenance manifest mandatory; no ROM dumps, no third-party samples.
- AROS C: `struct Library` style, `IExec`/`ObtainSemaphore`, `-ffixed-r12`, executables link `-no-pie`, libraries link `ld -r` (ET_REL); editor tools for file writes, never shell heredocs; throwaway diagnostics go to `/tmp/ri/run/<task>/`, never `scripts/`.
- Commit rule: every commit message ends with a gate reference, e.g. `[gate:G2]` or `[matrix:303-filter|ledger:docs/evidence/303/filter-candidate.md]`.

## Host Laboratory Rules (inherited by every task)

- `CFLAGS_HOST = -std=c99 -O2 -Wall -Wextra -Werror -pedantic -ffp-contract=off -fno-unsafe-math-optimizations -ftrapv`. No `-ffast-math` anywhere, ever.
- Denormals honored on host (no FTZ/DAZ flags); a T2 test asserts finite output for denormal-range inputs.
- `tests/helpers/ri_assert.h`: `RI_ASSERT(cond)` prints `__FILE__:__LINE__` + message and returns nonzero from `main` (never aborts — every failure must be countable).
- `tests/helpers/dump.h`: `dump_f32_csv(path, data, n)` and `dump_events_txt(path, events, n)` — the only sanctioned debug output; files land in `/tmp/ri/run/<task>/`.
- `/tmp/ri/` hygiene: `OUT=/tmp/ri/build` (objects), run outputs in `/tmp/ri/run/<task>/`; `ri_build_host.sh clean` wipes `/tmp/ri` entirely. Nothing under `/tmp/ri` is ever committed.

## Reused Assets (Vulkan4AROS tree, `../Vulkan4Aros/`)

- `scripts/aros_abi_provision.sh` — provisions GCC 16.1.0 toolchain + AROS SDK under `src/abi/` (never committed).
- `scripts/aros_build_env.sh` — resolves build env vars for cross-compiles; sourced ONLY by `ri_build_aros.sh`, never on host path.
- `scripts/aros_compile.sh` — cross-compile + `--link` pattern (`-L SDK lib`, `startup.o`, `-nostartfiles` where needed).
- `scripts/aros_audit.sh` — audit-gate architecture copied for `scripts/ri_audit.sh`.
- `llm-wiki/audit-gate-design.md`, `llm-wiki/probe-evidence-discipline.md` — gate/probe discipline references.
- AROS conventions: ROMTag in `.text`, LVO stride 8 B, library base = last C arg.

## File Structure

```
engine/dsp/kernels.h/.c      # ri_tanh/ri_exp/ri_sin/ri_pow2 (bounded, measured)
engine/dsp/rb303.h/.c        # 303 voice + Appendix B candidate filter
engine/dsp/rb808.h/.c        # 808 15-voice set
engine/dsp/rb909.h/.c        # 909 sampler + sampler utils (ri_resample_linear, ri_layer_mix)
engine/dsp/params.c          # knob 0..127 → float curves + fader table + ledger of each curve
engine/seq/clock.h/.c        # rational clock: segments, fractional accumulator, mul_div
engine/seq/sched.h/.c        # RIEvent contract + snapshot builder + event walker
engine/framework/ridevice.h/.c # RIDevice static registry (Classic)
engine/mixer/mixer.h/.c      # 4 buses + master + meter tap
engine/fx/pcf.h/.c           # PCF SVF + ledger-verified table loader
engine/fx/fx.h/.c            # delay, distortion, compressor
audio_io/audio.h/.c          # graph API + render task + AHI backend + WAV export
audio_io/probe_ahi.c         # AROS-only M1.1 measurement probe (never in host build)
midi_io/midi.h/.c            # CAMD backend + pure learn-map logic (host-tested)
gui/knob_logic.h/.c          # pure drag→value mapping (host-tested)
gui/widgets/                 # RKnB/RStp/RFdr/RVUm/RDsp MCC shells (AROS-only)
gui/panels.c                 # RIPanelDesc tables
project/rbng.h/.c            # song codec + validators
project/rbnm.h/.c            # mod codec + manifest validator
project/arexx.c              # ADDRESS REINCARNATION command table
app/main.c                   # Intuition window + task wiring
tools/render.c               # offline renderer CLI
tools/compare.c              # event-diff + WAV SHA-256 diff (single source of truth)
tools/inspect.c              # RBNM/RBNG validator/dumper
tools/bench.c                # worst-case fixture benchmark
tests/helpers/ri_assert.h    # countable assert macro
tests/helpers/dump.h         # CSV/event debug dump helpers
tests/unit/t1_*.c            # T1 per-equation tests
tests/property/t2_*.c        # T2 always-true tests
tests/golden/{303,808,909,pcf,songs}/  # T3 fixtures + .sha256 sidecars
scripts/ri_build_host.sh     # TARGET-DRIVEN host build (targets below)
scripts/ri_build_aros.sh     # AROS cross-build (skeleton in Task 1, extended later)
scripts/ri_audit.sh          # gates; every task appends exactly one phase
scripts/ri_fuzz.sh           # 500-mutation fuzz loop over tools/inspect (Task 13)
```

`ri_build_host.sh` targets (final shape, defined HERE in Task 1; later tasks only append file paths to the named lists): `kernels clock sched dsp303 dsp808 dsp909 fx mixer all test TESTNAME golden clean`. Objects: `/tmp/ri/build/<name>.o`. Tests link against objects and print `PASS <name>` or `FAIL <n>`.

---

### Task 1: Laboratory bootstrap — target-driven build, dual-path audit, AROS skeleton

**Files:**
- Create: `scripts/ri_build_host.sh`, `scripts/ri_build_aros.sh`, `scripts/ri_audit.sh`, `tests/helpers/ri_assert.h`, `tests/helpers/dump.h`, `docs/evidence/README.md`, `audio_io/aros_stub.c` (empty file, proves cross path)
- Modify: none

**Interfaces:**
- Consumes: Vulkan4AROS env/compile script conventions (sourced only by `ri_build_aros.sh`).
- Produces: build targets + audit phases + helpers used by every later task.

- [ ] **Step 1: Write `tests/helpers/ri_assert.h`**

```c
#ifndef RI_ASSERT_H
#define RI_ASSERT_H
#include <stdio.h>
static int ri_fail_count = 0;
#define RI_ASSERT(cond, ...) do { \
    if (!(cond)) { printf("FAIL %s:%d: ", __FILE__, __LINE__); printf(__VA_ARGS__); printf("\n"); ri_fail_count++; } \
} while (0)
#define RI_RESULT(name) do { printf(ri_fail_count ? "FAIL %d\n" : "PASS " name "\n", ri_fail_count); return ri_fail_count != 0; } while (0)
#endif
```

- [ ] **Step 2: Write `scripts/ri_build_host.sh` (final target shape)**

```bash
#!/bin/bash
# usage: ri_build_host.sh [kernels|clock|sched|dsp303|dsp808|dsp909|fx|mixer|all|test NAME|golden NAME|clean]
set -e
ROOT="$(dirname "$0")/.."
OUT=/tmp/ri/build
CFLAGS="-std=c99 -O2 -Wall -Wextra -Werror -pedantic -ffp-contract=off -fno-unsafe-math-optimizations -I$ROOT"
mkdir -p "$OUT"
MOD_kernels="engine/dsp/kernels.c"
MOD_clock="engine/seq/clock.c"
MOD_sched="engine/seq/sched.c"
MOD_dsp303="engine/dsp/rb303.c engine/dsp/params.c"
# Later tasks APPEND paths to MOD_dsp808, MOD_fx, ... and add matching case lines.
compile_list() { for f in $1; do test -f "$ROOT/$f" || { echo "MISSING $f"; exit 1; }; gcc $CFLAGS -c "$ROOT/$f" -o "$OUT/$(basename $f .c).o"; done; }
case "${1:-all}" in
  kernels|clock|sched|dsp303) compile_list "$(eval echo \$MOD_$1)" ;;
  all) for t in kernels clock sched dsp303; do "$0" $t; done ;;
  test) test -n "$2" || { echo "usage: $0 test NAME"; exit 1; }
    gcc $CFLAGS -o "$OUT/$2" "$ROOT/tests/unit/$2.c" "$ROOT/tests/property/$2.c" "$OUT"/*.o 2>/dev/null || \
    gcc $CFLAGS -o "$OUT/$2" $(ls "$ROOT/tests/unit/$2.c" "$ROOT/tests/property/$2.c" 2>/dev/null) "$OUT"/*.o
    "$OUT/$2" ;;
  clean) rm -rf /tmp/ri ;;
  *) echo "unknown target $1"; exit 1 ;;
esac
echo "BUILD $1 OK"
```

- [ ] **Step 3: Write `scripts/ri_build_aros.sh` skeleton (compiles empty lib, fails cleanly without env)**

```bash
#!/bin/bash
# AROS cross-build. Heute: proves the path works by building an empty stub library.
set -e
ROOT="$(dirname "$0")/.."
if [ ! -f ../Vulkan4Aros/scripts/aros_build_env.sh ]; then echo "FAIL: Vulkan4AROS tree (toolchain source) not found"; exit 1; fi
. ../Vulkan4Aros/scripts/aros_build_env.sh
OUT=/tmp/ri/aros
mkdir -p "$OUT"
x86_64-aros-gcc -c -mcmodel=large -mno-red-zone -ffixed-r12 -Wall "$ROOT/audio_io/aros_stub.c" -o "$OUT/aros_stub.o"
x86_64-aros-ld -r "$OUT/aros_stub.o" -o "$OUT/reincarnation_stub.library"
x86_64-aros-readelf -s "$OUT/reincarnation_stub.library" | grep -q "UND" && { echo "FAIL: unresolved symbols"; exit 1; } || true
echo "AROS STUB BUILD OK"
```

- [ ] **Step 4: Write `scripts/ri_audit.sh` phase 0 (three gates: hygiene, libm/FMA ban, evidence dirs)**

```bash
#!/bin/bash
set -e
ROOT="$(dirname "$0")/.."
echo "== Phase 0a: render-path hygiene =="
if grep -rn "malloc\|calloc\|realloc\|free(\|Forbid\|Disable(" "$ROOT/engine/" 2>/dev/null; then echo "FAIL: banned construct in engine/"; exit 1; fi
echo "== Phase 0b: no platform transcendentals/FMA in engine/ =="
if grep -rn "tanhf\|sinf\|cosf\|expf\|powf\|fmodf\|mul_add\|ffast-math" "$ROOT/engine/" 2>/dev/null | grep -v "ri_tanh\|ri_exp\|ri_sin\|ri_pow2"; then echo "FAIL"; exit 1; fi
echo "== Phase 0c: evidence dirs =="
for d in 303 808 909 pcf sequencer gui formats; do test -d "$ROOT/docs/evidence/$d" || { echo "FAIL: missing $d"; exit 1; }; done
echo "AUDIT 0/0 PASS"
```

- [ ] **Step 5: Run build (expect FAIL, no sources) + audit (expect PASS)**

Run: `bash scripts/ri_build_host.sh kernels` → FAIL `MISSING engine/dsp/kernels.c`. Run: `bash scripts/ri_audit.sh` → PASS.
Expected: build fails proving `set -e` + MISSING guard; audit passes on empty tree.

- [ ] **Step 6: Commit**

Run: `git add scripts tests/helpers docs/evidence/README.md audio_io && git commit -m "chore: target-driven build, dual-path audit, lab rules [gate:G0]"`
Expected: commit created.

**Gate G0:** `ri_build_host.sh clean` wipes `/tmp/ri`; unknown target exits 1; audit 0/0 green on empty tree.

---

### Task 2: Measured deterministic kernels (bounded, ledger error bounds)

**Files:**
- Create: `engine/dsp/kernels.h`, `engine/dsp/kernels.c`, `tests/property/t2_kernels.c`, `docs/evidence/303/kernel-bounds.md`
- Modify: `scripts/ri_build_host.sh` (nothing — `kernels` target already lists the file), `scripts/ri_audit.sh` (nothing — phase 0b covers it)

**Interfaces:**
- Consumes: lab rules (bounded loops only: every loop bound is a literal constant).
- Produces: `ri_tanh/ri_exp/ri_sin/ri_pow2` (exact names; §15 D1 list) + documented domains: tanh [-8,8], exp [-8,8], sin (any float; caller wraps phase — kernel clamps cycles to ±64), pow2 [-32,32].

- [ ] **Step 1: Write failing test `tests/property/t2_kernels.c`**

```c
#include <stdio.h>
#include <math.h>
#include <stdint.h>
#include "tests/helpers/ri_assert.h"
#include "engine/dsp/kernels.h"
static uint32_t lcg = 0x12345678;
static float frand(void) { lcg = lcg * 1664525u + 1013904223u; return (float)(lcg >> 8) / 16777216.0f; }
int main(void) {
    /* bit-determinism over 10k pseudo-random inputs per kernel */
    for (int i = 0; i < 10000; i++) {
        float x = (frand() * 2.0f - 1.0f) * 8.0f;
        RI_ASSERT(ri_tanh(x) == ri_tanh(x), "tanh nondet %f", x);
        RI_ASSERT(ri_exp(x) == ri_exp(x), "exp nondet %f", x);
    }
    /* no NaN/Inf for any finite input in documented domains */
    for (float x = -8.0f; x <= 8.0f; x += 0.125f) {
        float t = ri_tanh(x), e = ri_exp(x);
        RI_ASSERT(t == t && e == e, "NaN at %f", x);
        RI_ASSERT(t > -2.0f && t < 2.0f && e < 1e4f, "range at %f", x);
    }
    /* accuracy on operating grid: tanh ±1e-6, exp ±2e-6 vs libm reference */
    for (float x = -4.0f; x <= 4.0f; x += 0.01f) {
        RI_ASSERT(fabsf(ri_tanh(x) - tanhf(x)) <= 1e-6f, "tanh acc %f", x);
        RI_ASSERT(fabsf(ri_exp(x) - expf(x)) <= 2e-6f, "exp acc %f", x);
    }
    for (float x = -12.56f; x <= 12.56f; x += 0.02f)
        RI_ASSERT(fabsf(ri_sin(x) - sinf(x)) <= 2e-6f, "sin acc %f", x);
    for (float x = -10.0f; x <= 10.0f; x += 0.05f)
        RI_ASSERT(fabsf(ri_pow2(x) - powf(2.0f, x)) <= 4e-6f, "pow2 acc %f", x);
    RI_RESULT("kernels");
}
```

- [ ] **Step 2: Run, expect compile FAIL (no headers)**

Run: `bash scripts/ri_build_host.sh test t2_kernels`
Expected: FAIL, missing header.

- [ ] **Step 3: Implement table+polynomial kernels (all loops constant-bounded)**

`ri_tanh`: 65-entry table over [-4,4] (step 0.125) + linear interpolation, clamp ±1 outside; table generated at compile time by the exact listed literals (executor writes the 65 values from `tanh` computed once with python3 and pastes them — the test, not the generator, is the record).
`ri_exp`: `k = (int)(x*1.442695f + (x>=0?0.5f:-0.5f))`, clamp k to [-32,32]; `r = x - k*0.6931472f`; order-4 Horner on r; scale by `2^k` via `for (i=0;i<32;i++)` bounded loop multiplying only while `i < abs(k)`.
`ri_sin`: `q = (int)(x*0.15915494f)`, clamp q to [-64,64]; `y = x - q*TAU`; order-5 Taylor on y.
`ri_pow2`: `n=(int)x` clamped [-32,32], `f = x - n`, frac poly, scale by bounded loop.
No `while`, no libm calls, no globals.

- [ ] **Step 4: Run, expect PASS; record bounds in ledger `docs/evidence/303/kernel-bounds.md`**

Run: `bash scripts/ri_build_host.sh kernels && bash scripts/ri_build_host.sh test t2_kernels`
Expected: `PASS kernels`. Ledger row: claim (max errors measured by the test), source (T2 run output pasted), class E4, method (grid step + 10k LCG seed 0x12345678).

- [ ] **Step 5: Commit**

Run: `git add engine/dsp tests/property docs/evidence/303 && git commit -m "feat: measured deterministic kernels with T2 bounds [gate:G2]"`
Expected: commit created.

**Gate G2:** `test t2_kernels` green + ledger row exists + audit 0b green.

---

### Task 3: Rational clock — segments, fractional accumulator, exact rounding

**Files:**
- Create: `engine/seq/clock.h`, `engine/seq/clock.c`, `engine/seq/sched.h` (event struct + comparator only), `tests/property/t2_clock.c`
- Test: single-segment exactness + multi-segment crossing + boundary ownership + 24 h monotonic sweep + Round/Floor relation.

**Interfaces:**
- Consumes: `stdint.h` only.
- Produces: `struct RITempoMap`, `ri_map_tick` (Round) / `ri_map_tick_floor` (Floor), `RIEvent` + `RI_EV_*` + `ri_event_less()` (exact names; Tasks 4+ rely on them). Header comment locks rounding: "Round at schedule, Floor at lookup — changing this breaks D0."

```c
/* clock.h */
#ifndef RI_CLOCK_H
#define RI_CLOCK_H
#include <stdint.h>
struct RISegment { uint64_t start_tick; uint64_t ns_per_quarter; };
struct RITempoMap { const struct RISegment *segs; uint32_t n; uint32_t ppq; uint32_t sr; };
/* PPQ=96 (P-20). Exact rational per segment, ONE rounding. */
uint64_t ri_map_tick(const struct RITempoMap *m, uint64_t tick);        /* Round */
uint64_t ri_map_tick_floor(const struct RITempoMap *m, uint64_t tick);  /* Floor */
#endif
```

- [ ] **Step 1: Write failing test** (`t2_clock.c`): 96 ticks @120 BPM/48 k → exactly 24000; two-segment map (120→140 at tick 384): tick 383 uses old, tick 384 uses new (boundary ownership); Round−Floor ∈ {0,1} over 100k ticks; monotonic sweep to 48 000×86 400 ticks in 2^20 steps; ordering NOTE_OFF<NOTE_ON<ACCENT at same sample.
- [ ] **Step 2: Run, expect compile FAIL.**
- [ ] **Step 3: Implement** (segment walk with cached index hint + `unsigned __int128` mul_div; portable two-limb fallback `#ifndef __SIZEOF_INT128__` using 64-bit halves — executor writes both, T1-equivalent coverage via the same test compiled with `-DRI_NO_INT128`).
- [ ] **Step 4: Run both variants, expect PASS**

Run: `bash scripts/ri_build_host.sh clock && bash scripts/ri_build_host.sh test t2_clock`
Expected: PASS.
Run (portable fallback): `gcc -std=c99 -O2 -Wall -Wextra -Werror -pedantic -ffp-contract=off -DRI_NO_INT128 -I. -o /tmp/ri/t2_clock_no128 tests/property/t2_clock.c engine/seq/clock.c && /tmp/ri/t2_clock_no128`
Expected: PASS — same assertions green on the two-limb path.
- [ ] **Step 5: Commit** `feat: rational multi-segment clock with rounding lock [gate:G3]`.

**Gate G3:** both variants green + header rounding comment present (audit greps for "Round at schedule, Floor at lookup").

---

### Task 4: First light — ledger-first filter candidate, math golden, walker, compare

**Files:**
- Create: `docs/evidence/303/filter-candidate.md` (FIRST — exact equations pasted from spec Appendix B + DRIVE=1.0 hypothesis + "UNVALIDATED until M2.2 A/B" banner), `engine/dsp/rb303.h`, `engine/dsp/rb303.c`, `engine/dsp/params.c` (303 curves only), `engine/seq/sched.c` (minimal: sorted-array emit for one section), `tools/render.c`, `tools/compare.c`, fixtures `tests/golden/303/math-dc.wav` + `math-sine.wav` + `first-light.wav` + `.sha256` + `.events` sidecars
- Test: DC-convergence + sine-passthrough math goldens; event-stream golden from day one; determinism double-render.

**Interfaces:**
- Consumes: kernels (Task 2), clock+events (Task 3).
- Produces: `rb303_render` / `rb303_set_param` (exact spec signatures); `tools/render --song/--out/--dump-events`; `tools/compare --events-a/--events-b --wav-a/--wav-b` (exit 1 on any difference); `ri_sched_emit_sorted()` minimal walker.

- [ ] **Step 1: Write the ledger doc FIRST (before any DSP code exists)**

`docs/evidence/303/filter-candidate.md`: claim (Appendix B recurrence as implemented), status UNVALIDATED, note "this golden proves determinism + skeleton, NOT parity."

- [ ] **Step 2: Write failing math tests** — DC 0.5 in, fc=1000 Hz, k=0: mean of last 1000 samples within 1e-4 of 0.5. Sine 1 kHz, fc=8 kHz, k=0: output RMS within ±0.5 dB of input, THD lagging indicator recorded but NOT gated (candidate unvalidated — gate only determinism + DC).
- [ ] **Step 3: Implement voice + minimal walker + render/compare tools** (slide TC runtime field `slide_tc`, default 0.040; no env retrigger on slide; VCO saw/square; params.c maps CTL_303A_* 0..127 via explicit tables written here).
- [ ] **Step 4: Render math goldens + event golden + musical second fixture; pin SHA-256 + events; double-render equality.**
- [ ] **Step 5: Audit phase (golden-missing-is-broken + re-render-compare) + commit** `feat: first light with ledger-first candidate and math goldens [gate:G4]`.

**Gate G4:** math goldens + event golden pinned and re-verified; ledger doc predates code (check `git log --diff-filter=A --format=%at` ordering in audit); musical fixture present but ungated.

---

### Task 5: W1 measurement spike ONLY (closes OPEN-09/OPEN-06, no implementation)

**Files:**
- Create: `audio_io/probe_ahi.c` (AROS-only; excluded from host build by `#ifdef __AROS__` + audit check), `docs/evidence/formats/m1-1-report.md` (template with exact fields filled by the run)
- Test: the report itself — machine, CPU, compiler flags, low-level min buffer, device min buffer, xrun notes, chosen backend, reference-box name. No code lands except the probe.

**Gate G5:** report exists with all fields non-empty + spec OPEN-09 row updated to "measured" (executor edits the spec row + OPEN table in the same commit).

Commit: `chore: M1.1 AHI measurement closes OPEN-09 [gate:G5]`.

---

### Task 6: W1 backend implementation (chosen path only)

**Files:**
- Create: `audio_io/audio.h`, `audio_io/audio.c`, host stub backend for CI (`audio_io/backend_null.c`)
- Test: one-renderer assertion — first-light song via file path vs live-stub path, `tools/compare` SHA-equal; latency query returns M1.1-measured floor; AHI-missing boot shows documented fallback message (assert the string).

Header subset (exact):
```c
struct AudioObject *AuCreateObject(struct Library *AudioBase, struct TagItem *tags);
uint32_t AuAddSource(struct AudioObject *ao, struct TagItem *tags);
uint32_t AuAddBus(struct AudioObject *ao, const char *name, struct TagItem *tags);
int AuConnect(struct AudioObject *ao, uint32_t src, uint32_t bus);
int AuStart(struct AudioObject *ao);
void AuStop(struct AudioObject *ao);
uint32_t AuQueryAttr(struct AudioObject *ao, uint32_t attr); /* AUQA_LatencyFrames, AUQA_XRUN_COUNT */
int AuRenderToFile(struct AudioObject *ao, const char *path, uint32_t ms);
```
Commit: `feat: W1 backend with one-renderer proof [gate:G6]`.

**Gate G6:** diff gate green + fallback-string test green + audit one-renderer phase added.

---

### Task 7: Scheduler musical timing — shuffle/legato/flam measured, flam as parameter

**Files:**
- Create/extend: `engine/seq/sched.c` (walker upgrade), `tests/unit/t1_sched.c`, `tests/golden/songs/sched-check.events`
- Test: shuffle offsets equal `shuffle_pct·(ppq/4)/100` on even 16ths (assert exact sample math at 48 kHz/140 BPM: 16th = 10285.7 → tick-domain assert, sample-domain ±1); legato sets NOTE_ON-with-slide + env-continuity flag, never NOTE_OFF; flam second event at `flam_ms` parameter (default 35.0, ledger P-05) — assert offset = `flam_ms·sr/1000 ±48`; `tools/compare` is the single source of truth for event diffs.

Commit: `feat: measured shuffle/legato/flam walker [gate:G7]`.

**Gate G7:** T1 green + event golden pinned + P-05 ledger row updated with measured default.

---

### Task 8: 808 fifteen voices (gated: Tasks 4–7 fixtures green)

**Files:**
- Create: `engine/dsp/rb808.h` (`struct RB808Voice`, `struct RB808Set` with one voice per each of BD SD LT MT HT LC MC HC RS CL CP CH OH CY CB + `triggered` mask), `engine/dsp/rb808.c`, `tests/unit/t1_808.c`, goldens per voice + storm fixture
- Test: BD trajectory ±5% (f_start/f_end/τ sampled at 5 envelope points); hat FFT peaks within ±0.5% of listed ratios; clap envelope shows 4 bursts (peak-count on rectified envelope); accent ×1.5 ±0.5 dB every accent-capable voice; 35 Hz floor asserted (sweep never below); storm render-time ≤0.3× buffer duration via `clock()`; ledger rows `docs/evidence/808/<voice>.md` ×15 created by this task.
- Audit: phase running `t1_808` + storm budget check.

Commit: `feat: 808 fifteen voices with per-voice ledger rows [gate:G8]`.

**Gate G8:** T1 green + 15 ledger rows exist + storm within budget + goldens pinned.

---

### Task 9: 909 sampler + first clean pack + provenance gate

**Files:**
- Create: `engine/dsp/rb909.c` (+ `ri_resample_linear`, `ri_layer_mix` in same TU), `project/rbnm.c` (S909-chunk subset: parse + validate + manifest check), `tools/inspect.c` (RBNM validator: chunk lengths, manifest completeness → exit 1 with reason), `reference/packs/classic-01/` (2–4 synthetic layers/voice, 24-bit WAV masters + per-layer processing log)
- Test: tune sweep 0..127 step 1 on synthetic 2-layer sine fixture → max adjacent-band RMS step ≤1 dB; acc1 ×1.15 ±0.2 dB; flam +35 ms ±5 ms ×0.75; retrigger cuts previous (overlap energy < −60 dB after cut); swap-while-active refused (return code asserted), idle swap click-free (edge discontinuity < −80 dBFS); audit fails build on any manifest gap.
- Ledger: `docs/evidence/909/<voice>.md` + manifest rows per layer.

Commit: `feat: 909 sampler with clean pack and provenance gate [gate:G9]`.

**Gate G9:** TC-2.4 equivalents green + manifest-complete audit green + pack renders differ from default per S909 (render-diff test).

---

### Task 10: PCF black-box route + FX

**Files:**
- Create: `engine/fx/pcf.c` (SVF engine, table loaded from `reference/pcf-table.bin` — file absent until ledger verifies rows; code refuses to build table into binary: `#error` if `PCF_TABLE_VERIFIED` undefined), `engine/fx/fx.c` (`struct RiFXDelay/RiFXDist/RiFXComp` + set/render fns per Appendix D names), `tests/unit/t1_fx.c`, goldens
- Test: cutoff law ±2 cents at 10 table values against ledger-verified rows only (test reads the same data file — single source); determinism across restarts; delay sync error <0.1% at 120/140/174 BPM; distortion unity ±0.2 dB at drive 0 + full-grid no-NaN fuzz; order-swap ≤1 buffer zipper (edge energy bound).
- Ledger: `docs/evidence/pcf/engine.md` + per-pattern rows as captures arrive.

Commit: `feat: PCF engine with ledger-gated table + FX trio [gate:G10]`.

**Gate G10:** all FX tests green + build provably cannot embed an unverified table (negative test: delete data file → build fails with the `#error`).

---

### Task 11: Mixer/metering + RIDevice static registry

**Files:**
- Create: `engine/mixer/mixer.h/.c`, `engine/framework/ridevice.h/.c`
- Test: fader law gain=(v/127)² (E0 decision recorded in `params.c` header + ledger `docs/evidence/sequencer/fader-law.md`), 9 anchors ±0.5 dB; full 16-combo solo/mute truth table, toggle zipless (transient bound); meter peak-hold decay 20 dB/s ±10%; dummy-device register/render/unregister with zero framework edits (TC-2.1.5).
- Registry (exact): `void ri_devices_init(void); struct RIDevice *ri_device_get(uint32_t index); uint32_t ri_device_count(void);` — static table of 4, no dynamic loading in Classic.

Commit: `feat: mixer with E0 fader law + static device registry [gate:G11]`.

**Gate G11:** truth table green + dummy-device test green + fader ledger row exists.

---

### Task 12: GUI — host-tested logic + AROS MCC shells + panels + sequencer/transport

**Files:**
- Create: `gui/knob_logic.h/.c` (pure: `knob_drag_to_value(start, dy_px, fine)` = `start + dy·(127/150)·(fine?0.1:1)`, clamped; fader/step/LED-chase helpers), `tests/unit/t1_knob.c` (150 px full ±5%, fine ×0.1 ±10%, clamp, commit-on-release single-event rule as pure function), `gui/widgets/*.mcc.c` (thin MCC shells calling the logic — AROS-only), `gui/panels.c` (RIPanelDesc tables), `app/main.c`
- Test: host T1 for all drag math; AROS checklist (silhouette ±2 px, LED ≤33 ms, 174 BPM chase, zoom crispness, dummy panel) recorded as `docs/evidence/gui/acceptance.md` with tester sign-off lines; ReBirth-101 tutorial workflow ≥4/5 recorded same file.

Commit: `feat: GUI logic host-tested, MCC shells, panels [gate:G12]`.

**Gate G12:** host T1 green + acceptance file exists with all boxes checkable (unchecked = gate red).

---

### Task 13: Formats full, MIDI, automation, ARexx, datatypes

**Files:**
- Create: `project/rbng.c`, `project/rbnm.c` (full), `midi_io/midi.c` (pure learn-map `cc_to_ctl[128]` host-tested + AROS CAMD backend), `project/arexx.c` (commands OPENSONG/PLAY/STOP/EXPORTWAV/SETRPPARAM — exact strings), datatype classes, `scripts/ri_fuzz.sh` (500 mutations via `tools/inspect`, expects clean-requester path = exit 2, never crash/timeout)
- Test: 10-song corpus double-render md5-identical + serialize-parse-serialize byte-identical; unknown-chunk preservation; WAV headers validated by `sox --i` (host tool, else `tools/inspect --wav`); MODR-missing-mod warn path (assert prompt string); undo-200 scripted; automation ±1 unit after save/load; MIDI loopback (pure map, host); clock drift <1 tick/100 bars (simulated); hot-unplug survival (backend stub returns timeout code, transport continues — assert).
- Ledger: `docs/evidence/formats/*.md` per format claim.

Commit: `feat: formats, MIDI, automation, ARexx [gate:G13]`.

**Gate G13:** corpus tests green + fuzz 500/500 no-crash + manifest/art-fallback/CPRG tests green.

---

### Task 14: REL — soak, docs, installer, beta

- Soak: `tools/bench` 30/60 min + overnight on M1.1 box, zero underruns; event-density max; tempo/pattern/mod switching under load.
- Docs: AmigaGuide manual (every control), autodocs (every public fn with the exact signatures from Tasks 6/11), catalogs EN + 1 locale (prove pipeline: one translated string rendered in GUI acceptance re-run).
- Installer + Amiga icons; 20-user beta; exit gates TC-2.16.x (zero crashers/data-loss, A/B ≥4/5).
- Audit full run + repo hygiene (no blobs, `scripts/` contains only shared tooling).

Commit: `feat: REL soak/docs/installer/beta exit [gate:G14]`.

**Gate G14:** soak logs + beta sign-off file `docs/evidence/formats/beta-exit.md` + full audit green.

---

## Plan-level Definition of Done

Task 14 closes only when: `ri_audit.sh` runs the FULL T1–T5 suite on the M1.1 named box with zero failures; the evidence ledger holds a row for every claim no longer marked hypothesis; every Appendix A row is either locked or explicitly deferred with a milestone; no OPEN row is closed without its named gate's artifact in the tree.

## Appendix: W3 Power Mode — deferred, no tasks

Strict deferral per spec §3. No W3 tasks exist in this plan. A W3 appendix plan is written only after W2 DoD. Any task proposing `if (power_mode)` in Classic DSP is rejected at review.

## Self-Review

1. **Spec coverage:** §0.1 → Task 4. §2.3 → Tasks 4/5/8. §3 W1 → Tasks 5–6 (+M1.1). §4 realtime/SMP → Tasks 1–2 (gates) + deferred DAG (no task — correct). §4.2 AHI → Task 5. §5 one-renderer → Tasks 4/6 (diff gate). §6/Appendix D → Task 11 registry (internal static table; freeze gate untouched). §7 clock → Task 3 (+P-20 comment in header). §8 events/state machine → Tasks 3/4/7. §9 303 → Task 4 (+M2.2 tuning on its fixtures). §10 808 → Task 8. §11 909 → Task 9. §12 PCF → Task 10. §13 mixer/GUI/formats/MIDI/assets → Tasks 11/12/13. §14 matrix+ledger → every gate + Task 14. §15 D0/D1 → Tasks 2/4/6/9. §16 T1–T5 → Tasks 2–4, 7–10, 13–14. §17 degraded → Tasks 6 (xrun), 9/13 (fuzz/rollback), 14 (soak). §18 gates/DoD → Task 14 + W3 note. Appendix A P-18/19 → Tasks 12/13; P-21 → Task 5 report. OPEN-01–09 → Tasks 4/5/7/10/13/14.
2. **Placeholder scan:** no TBD/TODO/"implement later"/"appropriate handling"/"Similar to Task" phrasing. Remaining "executor writes" instances each name the exact spec section + file + pinning test.
3. **Type consistency:** `RIEvent`/`RI_EV_*`/`ri_event_less` (Task 3) → Tasks 4/7/12; `rb303_render/rb808_render/rb909_render/pcf_render` match spec appendices; `Au*` names consistent Tasks 6/11; `tools/render --song/--out/--dump-events` and `tools/compare --events-a/--events-b/--wav-a/--wav-b` consistent Tasks 4/6/7.
