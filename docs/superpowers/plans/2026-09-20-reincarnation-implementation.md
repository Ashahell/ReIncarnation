# ReIncarnation Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build ReIncarnation Classic (RB-338-class groovebox on AROS) phase by phase, each phase ending in a tested, committable deliverable, starting with one 303 rendering one pattern offline.

**Architecture:** Single-threaded Classic render path (scheduler → 4 device DSPs → FX → mixer → master) shared identically by live AHI playback and offline WAV export; host-GCC TDD for all DSP, AROS cross-build only at integration boundaries.

**Tech Stack:** C (C99, AROS SDK), host GCC for DSP/tests, `x86_64-aros-gcc` 16.1.0 for AROS targets, `ahi.device` + `camd.library` + Zune/MUI on AROS, IFF `FORM` files, SHA-256 golden fixtures.

**Spec:** `docs/superpowers/specs/2026-09-20-reincarnation-spec.md` — the plan argues from the spec, so the spec travels with it; executors read both.

## Global Constraints

Every task's requirements implicitly include this section.

- Render path: no dynamic allocation, no DOS, no GUI, no blocking IPC, no unbounded loops, no `Forbid()`/`Disable()`; SPSC FIFO + buffer-boundary snapshots; CI audit gate greps for violations.
- Exactly one DSP/render implementation for live playback and offline export.
- Determinism: D0 (same project ⇒ same events) + D1 (same binary/arch ⇒ identical PCM) only; never claim D2.
- Every compatibility claim carries an evidence class (E0–E5); progress declared only via parity matrix + `docs/evidence/` ledger rows.
- No W3 code in Classic paths: `RI_ENGINE_CLASSIC` vs `RI_ENGINE_POWER` separation, never `if (power_mode)` in DSP.
- All audio/art assets newly synthesized, recorded, or licensed; per-sample provenance manifest mandatory; no ROM dumps, no third-party samples.
- AROS C: `struct Library` style, `IExec`/`ObtainSemaphore`, `-ffixed-r12`, executables link `-no-pie`, libraries link `ld -r` (ET_REL); editor tools for file writes, never shell heredocs; throwaway diagnostics go to `/tmp/ri/`, never `scripts/`.

## Reused Assets (Vulkan4AROS tree, `../Vulkan4Aros/`)

- `scripts/aros_abi_provision.sh` — provisions GCC 16.1.0 toolchain + AROS SDK under `src/abi/` (never committed).
- `scripts/aros_build_env.sh` — resolves build env vars for cross-compiles.
- `scripts/aros_compile.sh` — cross-compile + `--link` pattern (`-L SDK lib`, `startup.o`, `-nostartfiles` where needed).
- `scripts/aros_audit.sh` — audit-gate architecture to copy for `scripts/ri_audit.sh` (phase-per-subsystem asserts + fingerprint drift gate).
- `llm-wiki/audit-gate-design.md`, `llm-wiki/probe-evidence-discipline.md` — gate/probe discipline references.
- AROS conventions: ROMTag in `.text`, LVO stride 8 B, library base = last C arg (register method: base in r12 at LVO entry — hand-rolled trampolines only where needed).

## File Structure

```
engine/dsp/rb303.h/.c        # 303 voice (struct + render + param curves)
engine/dsp/rb808.h/.c        # 808 15-voice set
engine/dsp/rb909.h/.c        # 909 sampler + shared sampler utils
engine/dsp/params.c          # knob 0..127 → float curves for all devices
engine/dsp/kernels.c         # allowlisted transcendental kernels (ri_tanh/ri_exp/ri_sin/ri_pow2)
engine/fx/pcf.h/.c           # PCF SVF + pattern table loader
engine/fx/fx.h/.c            # delay, distortion, compressor
engine/mixer/mixer.h/.c      # section buses + master + meter tap
engine/seq/clock.h/.c        # rational master clock (mul_div, segments)
engine/seq/sched.h/.c        # snapshot builder + event walker + shuffle/legato/flam
engine/framework/ridevice.h/.c # RIDevice registry (Classic: static table)
audio_io/audio.h/.c          # audio.library client: graph, render task, AHI backend, WAV export
midi_io/midi.h/.c            # CAMD backend + CC→ctl learn + clock bridge
gui/panels.h/.c              # RIPanelDesc tables per device
gui/widgets/                 # RKnB/RStp/RFdr/RVUm/RDsp Zune MCC classes
project/rbng.h/.c            # song load/save + validators
project/rbnm.h/.c            # mod loader + manifest + validators
project/arexx.c              # ADDRESS REINCARNATION commands
app/main.c                   # Intuition window + task wiring
tools/render.c               # offline renderer CLI (first-light command)
tools/compare.c              # SHA-256 + error-metric + event-stream diff
tools/inspect.c              # RBNM/RBNG validator/dumper
tools/bench.c                # worst-case fixture benchmark (W1.1 procedure)
tests/unit/t1_*.c            # T1 per-equation tests (host GCC)
tests/property/t2_*.c        # T2 always-true tests
tests/golden/{303,808,909,pcf,songs}/  # T3 fixtures + .sha256 sidecars
scripts/ri_audit.sh          # gate script (copied pattern from Vulkan4AROS aros_audit.sh)
scripts/ri_build_host.sh     # host build of engine + tools + tests
scripts/ri_build_aros.sh     # AROS cross-build (wraps aros_build_env.sh + aros_compile.sh)
```

---

### Task 1: Laboratory bootstrap (dirs, build, audit skeleton, ledger seed)

**Files:**
- Create: `scripts/ri_build_host.sh`, `scripts/ri_audit.sh`, `docs/evidence/README.md`
- Modify: none (tree dirs already created)

**Interfaces:**
- Consumes: Vulkan4AROS `aros_build_env.sh` / `aros_compile.sh` calling conventions (sourced, not copied).
- Produces: `ri_build_host.sh` (used by every later task), `ri_audit.sh` phase 0 (extended by every later task).

- [ ] **Step 1: Write `scripts/ri_build_host.sh`**

```bash
#!/bin/bash
# Host build: engine (no AROS headers) + tools + tests. Fails on any warning.
set -e
ROOT="$(dirname "$0")/.."
OUT=/tmp/ri/build
mkdir -p "$OUT"
gcc -std=c99 -O2 -Wall -Werror -I"$ROOT" -c "$ROOT/engine/dsp/kernels.c" -o "$OUT/kernels.o"
echo "HOST BUILD OK"
```

- [ ] **Step 2: Run it, expect failure (no sources yet)**

Run: `bash scripts/ri_build_host.sh`
Expected: FAIL with "kernels.c: No such file" — proves the script runs and `set -e` bites.

- [ ] **Step 3: Write `scripts/ri_audit.sh` phase 0 (render-path grep gate + evidence-dir gate)**

```bash
#!/bin/bash
# ri_audit.sh phase 0. Exit nonzero on any violation.
set -e
ROOT="$(dirname "$0")/.."
echo "== Phase 0: render-path hygiene =="
if grep -rn "malloc\|calloc\|realloc\|free(\|Forbid\|Disable(" "$ROOT/engine/" 2>/dev/null; then
  echo "FAIL: banned construct in engine/"; exit 1
fi
echo "== Phase 0: evidence dirs =="
for d in 303 808 909 pcf sequencer gui formats; do
  test -d "$ROOT/docs/evidence/$d" || { echo "FAIL: missing docs/evidence/$d"; exit 1; }
done
echo "AUDIT 0/0 PASS"
```

- [ ] **Step 4: Run audit, expect PASS (engine/ empty, evidence dirs exist)**

Run: `bash scripts/ri_audit.sh`
Expected: PASS — `AUDIT 0/0 PASS`.

- [ ] **Step 5: Write `docs/evidence/README.md` (ledger row format)**

```markdown
# Evidence ledger
One file per claim: `docs/evidence/<area>/<claim>.md` with fields:
claim | source | evidence class (E0-E5) | confidence | method | fixture | status
No claim moves hypothesis→normative without a row here.
```

- [ ] **Step 6: Commit**

Run: `git add scripts/ri_build_host.sh scripts/ri_audit.sh docs/evidence/README.md && git commit -m "chore: laboratory bootstrap (build, audit phase 0, ledger)"`
Expected: commit created.

---

### Task 2: Deterministic kernels + T2 math-property tests

**Files:**
- Create: `engine/dsp/kernels.h`, `engine/dsp/kernels.c`, `tests/property/t2_kernels.c`
- Test: `tests/property/t2_kernels.c` via `scripts/ri_build_host.sh` (extend it)

**Interfaces:**
- Consumes: nothing.
- Produces: `ri_tanh/ri_exp/ri_sin/ri_pow2` (exact names; §15 D1 enforcement list) used by every DSP task.

- [ ] **Step 1: Write the failing test `tests/property/t2_kernels.c`**

```c
#include <stdio.h>
#include <math.h>
#include "engine/dsp/kernels.h"
int main(void) {
    int fails = 0;
    /* determinism: same input ⇒ identical bits, twice */
    float a = ri_tanh(0.7f), b = ri_tanh(0.7f);
    if (a != b) { printf("FAIL tanh nondeterministic\n"); fails++; }
    /* accuracy band vs libm (reference only, not shipped): |ri_tanh - tanhf| < 2e-6 */
    for (float x = -3.0f; x <= 3.0f; x += 0.25f)
        if (fabsf(ri_tanh(x) - tanhf(x)) > 2e-6f) { printf("FAIL tanh accuracy at %f\n", x); fails++; }
    if (ri_exp(0.0f) != 1.0f) { printf("FAIL exp(0)\n"); fails++; }
    if (ri_sin(0.0f) != 0.0f) { printf("FAIL sin(0)\n"); fails++; }
    if (ri_pow2(1.0f) != 2.0f) { printf("FAIL pow2(1)\n"); fails++; }
    printf(fails ? "FAIL %d\n" : "PASS kernels\n", fails);
    return fails != 0;
}
```

- [ ] **Step 2: Run test, expect FAIL (no kernels yet)**

Run: `gcc -std=c99 -O2 -Wall -I. -o /tmp/ri/t2_kernels tests/property/t2_kernels.c -lm && /tmp/ri/t2_kernels`
Expected: FAIL at compile (`kernels.h: No such file`).

- [ ] **Step 3: Implement `engine/dsp/kernels.h` + `kernels.c` (pure C, no libm calls in shipped code)**

```c
/* kernels.h */
#ifndef RI_KERNELS_H
#define RI_KERNELS_H
float ri_tanh(float x);
float ri_exp(float x);
float ri_sin(float x);
float ri_pow2(float x);   /* 2^x */
#endif
```

```c
/* kernels.c — rational/polynomial approximations, NO libm dependency.
   Accuracy target: stated in the test above. No malloc, no globals. */
#include "engine/dsp/kernels.h"
float ri_tanh(float x) {
    if (x > 3.0f) return 1.0f;
    if (x < -3.0f) return -1.0f;
    float x2 = x * x;
    /* Pade [3/2]-style approximant, tuned coefficients */
    return x * (27.0f + x2) / (27.0f + 9.0f * x2);
}
float ri_exp(float x) {
    if (x > 10.0f) x = 10.0f;
    if (x < -10.0f) x = -10.0f;
    /* Taylor order 6 via Horner */
    float t = 1.0f + x * (1.0f + x * (0.5f + x * (0.1666667f + x * (0.0416667f + x * 0.0083333f))));
    return t;
}
float ri_sin(float x) {
    /* range-reduce to [-pi, pi] with float constants, then order-5 Taylor */
    const float PI = 3.14159265f, TAU = 6.2831853f;
    while (x > PI) x -= TAU;
    while (x < -PI) x += TAU;
    float x2 = x * x;
    return x * (1.0f + x2 * (-0.1666667f + x2 * 0.0083333f));
}
float ri_pow2(float x) { return ri_exp(0.6931472f * x); }
```

- [ ] **Step 4: Run test, expect PASS**

Run: `gcc -std=c99 -O2 -Wall -I. -o /tmp/ri/t2_kernels tests/property/t2_kernels.c engine/dsp/kernels.c -lm && /tmp/ri/t2_kernels`
Expected: PASS — `PASS kernels`. (If the Pade accuracy misses 2e-6 at some grid point, adjust coefficients — the test tells you where.)

- [ ] **Step 5: Extend audit with libm-transcendental ban in engine/**

Append to `scripts/ri_audit.sh`:

```bash
echo "== Phase 0b: no platform transcendentals in engine/ =="
if grep -rn "tanhf\|tanh(\|sinf\|sin(\|cosf\|expf\|expl\|powf\|mul_add\|ffast-math" "$ROOT/engine/" 2>/dev/null | grep -v "ri_tanh\|ri_exp\|ri_sin\|ri_pow2"; then
  echo "FAIL: platform transcendental in engine/"; exit 1
fi
```

Run: `bash scripts/ri_audit.sh`
Expected: PASS (kernels.c contains only `ri_`-prefixed names; adjust the grep if it false-positives on comments).

- [ ] **Step 6: Commit**

Run: `git add engine/dsp/kernels.h engine/dsp/kernels.c tests/property/t2_kernels.c scripts/ri_audit.sh && git commit -m "feat: deterministic DSP kernels with property tests"`
Expected: commit created.

---

### Task 3: Rational clock + event ordering (T2 properties)

**Files:**
- Create: `engine/seq/clock.h`, `engine/seq/clock.c`, `engine/seq/sched.h` (event struct + ordering only), `tests/property/t2_clock.c`
- Test: `tests/property/t2_clock.c`

**Interfaces:**
- Consumes: nothing (integer arithmetic only).
- Produces: `ri_tick_to_sample(uint64_t ticks, ...)` exact-rational conversion; `RIEvent` struct + `RI_EV_*` + `ri_event_less()` comparator (exact names; later tasks rely on them).

- [ ] **Step 1: Write failing test `tests/property/t2_clock.c`**

```c
#include <stdio.h>
#include "engine/seq/clock.h"
#include "engine/seq/sched.h"
int main(void) {
    int fails = 0;
    struct RISegment seg = { 96 /*ppq*/, 500000000 /*ns per quarter = 120 BPM*/, 48000 /*sr*/ };
    /* 96 ticks @120BPM/48k = exactly 24000 samples */
    uint64_t s = ri_tick_to_sample(96, &seg);
    if (s != 24000) { printf("FAIL tick->sample: got %llu\n", (unsigned long long)s); fails++; }
    /* ordering: NOTE_OFF < NOTE_ON < ACCENT at same sample */
    struct RIEvent off = { 100, RI_EV_NOTE_OFF, 0, 0, 0, 0 };
    struct RIEvent on  = { 100, RI_EV_NOTE_ON, 0, 0, 0, 0 };
    struct RIEvent acc = { 100, RI_EV_ACCENT, 0, 0, 0, 0 };
    if (!ri_event_less(&off, &on) || !ri_event_less(&on, &acc)) { printf("FAIL ordering\n"); fails++; }
    /* monotonicity: samples never go backwards across 1M ticks */
    uint64_t prev = 0;
    for (uint64_t t = 0; t < 1000000; t += 997) {
        uint64_t cur = ri_tick_to_sample(t, &seg);
        if (cur < prev) { printf("FAIL monotonic at %llu\n", (unsigned long long)t); fails++; break; }
        prev = cur;
    }
    printf(fails ? "FAIL %d\n" : "PASS clock\n", fails);
    return fails != 0;
}
```

- [ ] **Step 2: Run, expect FAIL (headers missing)**

Run: `gcc -std=c99 -O2 -Wall -I. -o /tmp/ri/t2_clock tests/property/t2_clock.c && /tmp/ri/t2_clock`
Expected: FAIL at compile.

- [ ] **Step 3: Implement `clock.h`/`clock.c` (widened integer mul_div, single rounding) + `sched.h` (struct + comparator)**

```c
/* clock.h */
#ifndef RI_CLOCK_H
#define RI_CLOCK_H
#include <stdint.h>
struct RISegment { uint32_t ppq; uint64_t ns_per_quarter; uint32_t sample_rate; };
uint64_t ri_tick_to_sample(uint64_t ticks, const struct RISegment *seg);
#endif
```

```c
/* clock.c */
#include "engine/seq/clock.h"
uint64_t ri_tick_to_sample(uint64_t ticks, const struct RISegment *seg) {
    /* samples = ticks * ns_per_quarter * sr / (ppq * 1e9), ONE rounding (round-half-up) */
    unsigned __int128 num = (unsigned __int128)ticks * seg->ns_per_quarter * seg->sample_rate;
    unsigned __int128 den = (unsigned __int128)seg->ppq * 1000000000ULL;
    return (uint64_t)((num + den / 2) / den);
}
```

```c
/* sched.h (event contract only — walker comes in Task 6) */
#ifndef RI_SCHED_H
#define RI_SCHED_H
#include <stdint.h>
#define RI_EV_TRANSPORT 0
#define RI_EV_PATTERN_CHANGE 1
#define RI_EV_NOTE_OFF 2
#define RI_EV_NOTE_ON 3
#define RI_EV_NOTE_CONTINUE 4
#define RI_EV_ACCENT 5
#define RI_EV_FLAM 6
#define RI_EV_AUTOMATION 7
#define RI_EV_PARAM 8
#define RI_EV_METER 9
struct RIEvent { uint64_t sample; uint32_t type; uint16_t device, voice, value, flags; };
static inline int ri_ev_priority(uint32_t t) { return (int)t; } /* type ids ARE priority order */
static inline int ri_event_less(const struct RIEvent *a, const struct RIEvent *b) {
    if (a->sample != b->sample) return a->sample < b->sample;
    if (a->type != b->type) return a->type < b->type;
    if (a->device != b->device) return a->device < b->device;
    return a->voice < b->voice;
}
#endif
```

- [ ] **Step 4: Run test, expect PASS**

Run: `gcc -std=c99 -O2 -Wall -I. -o /tmp/ri/t2_clock tests/property/t2_clock.c engine/seq/clock.c && /tmp/ri/t2_clock`
Expected: PASS — `PASS clock` (24000 exact, ordering holds, monotonic).

- [ ] **Step 5: Commit**

Run: `git add engine/seq/clock.h engine/seq/clock.c engine/seq/sched.h tests/property/t2_clock.c && git commit -m "feat: rational clock and frozen event ordering with property tests"`
Expected: commit created.

---

### Task 4: First light — 303 voice skeleton + offline renderer + golden

**Files:**
- Create: `engine/dsp/rb303.h`, `engine/dsp/rb303.c` (VCO + envelope skeleton; filter = Appendix B candidate), `engine/dsp/params.c` (303 curves), `tools/render.c`, `tests/golden/303/first-light.rbng` (minimal text song: 16-step C-minor pattern), `tests/expected/first-light.sha256` (filled after first verified run)
- Test: golden render twice ⇒ identical SHA-256 (D1); event-stream dump eyeballed once.

**Interfaces:**
- Consumes: `kernels.h`, `sched.h` (`RIEvent`), `clock.h`.
- Produces: `rb303_render(struct RB303Voice *v, const struct RBStepEvent *ev, float *out, uint32_t n, float sample_rate)`; `tools/render` CLI (`--song PATH --out PATH [--dump-events]`); first golden fixture.

- [ ] **Step 1: Write `engine/dsp/rb303.h` (spec §9 contract shape)**

```c
#ifndef RB303_H
#define RB303_H
#include <stdint.h>
struct RBStepEvent { uint32_t step; uint8_t note, flags; };
#define RBSTEP_ACCENT 0x01
#define RBSTEP_SLIDE  0x02
struct RB303Voice {
    float stage[3], phase, f_cur, f_target;
    float amp_env, acc_env, filter_env_cv;
    uint32_t gate : 1, slide : 1, accent : 1, waveform : 1;
    float cutoff, resonance, env_mod, decay, accent_amt;
};
void rb303_render(struct RB303Voice *v, const struct RBStepEvent *ev,
                  float *out, uint32_t n, float sample_rate);
void rb303_set_param(struct RB303Voice *v, uint32_t ctl_id, uint8_t value);
#endif
```

- [ ] **Step 2: Write minimal `rb303.c` (VCO + envelopes + Appendix B candidate filter; slide TC as runtime field `slide_tc`, default 0.040)**

Render: per-sample VCO (saw/square select, phase accumulate), slide slew `f_cur += (f_t - f_cur)*(1-ri_exp(-1/(sr*slide_tc)))`, amp/accent envs per §9 formulas, Appendix B candidate recurrence with `ri_tanh`. No env retrigger when `slide` set. (~120 lines; full code written by executor from spec §9 + Appendix B.)

- [ ] **Step 3: Write `tools/render.c` (first-light command from spec §0.1)**

Parses a minimal text song (16 lines `NOTE FLAGS`, e.g. `C3 ACCENT`), builds one 303 voice at default params, renders 2 bars at 48 kHz/140 BPM through scheduler event list → mono float → 16-bit WAV writer (inline, ~60 lines, no libs). Flags: `--song`, `--out`, `--dump-events` (prints `sample type device voice value flags` per event).

- [ ] **Step 4: First render + determinism check**

Run: `gcc -std=c99 -O2 -Wall -I. -o /tmp/ri/render tools/render.c engine/dsp/rb303.c engine/dsp/params.c engine/dsp/kernels.c engine/seq/clock.c && /tmp/ri/render --song tests/golden/303/first-light.rbng --out /tmp/ri/fl1.wav && /tmp/ri/render --song tests/golden/303/first-light.rbng --out /tmp/ri/fl2.wav && sha256sum /tmp/ri/fl1.wav /tmp/ri/fl2.wav`
Expected: two identical hashes (D1 on same machine). Listen/plot once (eyeball: decaying saw-ish notes, accents louder) — record observation in `docs/evidence/303/first-light.md` ledger row (E4 once golden pinned).

- [ ] **Step 5: Pin golden + add audit phase (golden-missing-is-broken)**

Save: `cp /tmp/ri/fl1.wav tests/golden/303/first-light.wav && sha256sum tests/golden/303/first-light.wav > tests/expected/first-light.sha256`. Append audit phase to `scripts/ri_audit.sh` that re-renders and compares (fails if binary missing, golden missing, or hash differs).

- [ ] **Step 6: Commit**

Run: `git add engine/dsp/rb303.h engine/dsp/rb303.c engine/dsp/params.c tools/render.c tests/golden/303/ tests/expected/ docs/evidence/303/ scripts/ri_audit.sh && git commit -m "feat: first light (303 voice, offline renderer, pinned golden)"`
Expected: commit created. This is the M2.2 foundation; 303 A/B tuning iterates on this fixture.

---

### Task 5: W1 minimal audio (M1.1 measurement incl. OPEN-09, AHI backend, WAV export)

**Files:**
- Create: `audio_io/audio.h` (graph API subset), `audio_io/audio_ahi.c` (AROS-only backend), `audio_io/wav.c` (WAV writer used by tools/render), `tools/bench.c`, `docs/evidence/formats/m1-1-report.md` (filled with measured numbers)
- Test: `tools/bench` worst-case fixture (spec §10 procedure) on host (provisional) + on AROS named box (normative).

**Interfaces:**
- Consumes: engine render path from Task 4; AROS `ahi.device`/`AHI_*` low-level API (M1.1 decides path).
- Produces: `AuCreateObject/AuAddSource/AuAddBus/AuConnect/AuStart/AuStop/AuQueryAttr(AUQA_LatencyFrames, AUQA_XRUN_COUNT)` minimal subset; `AuRenderToFile()`.

- [ ] **Step 1: M1.1 measurement spike (answer OPEN-09 + latency floor)**

On the AROS guest (toolchain via `../Vulkan4Aros/scripts/aros_build_env.sh`), build and run a probe that opens both the low-level `AHI_AllocAudio` path and the `ahi.device CMD_WRITE` path, reporting stable device-buffer minimums and xrun behavior. Record EXACT numbers into `docs/evidence/formats/m1-1-report.md` (machine, compiler flags, both buffer floors, chosen backend). This report closes OPEN-09 and OPEN-06 (reference box identity).

- [ ] **Step 2: Implement `audio_io/` minimal graph (single-threaded, §4.1 order)**

Sources → buses → master; 64-frame engine block; device buffer = N×engine blocks per M1.1 result; render Task woken by AHI hook signal (§4.2); `AuRenderToFile` reuses the same render function with file sink (one-renderer rule, asserted by a test rendering the first-light song both ways and diffing SHA-256).

- [ ] **Step 3: `tools/bench` + provisional-vs-measured budget update**

Run: `/tmp/ri/bench --fixture worst-case --seconds 1800` on host and on the named box; write measured % + xruns into the M1.1 report; update spec §10 numbers ONLY from this report (until then Appendix A hypotheses stand).

- [ ] **Step 4: Extend audit (one-renderer diff gate) + commit**

Audit phase: render first-light via live-path-stub and file path, `cmp` equal. Commit: `git add audio_io/ tools/bench.c docs/evidence/formats/ && git commit -m "feat: W1 minimal audio backend with M1.1 measurement"`.

---

### Task 6: Scheduler walker + shuffle/legato/flam (D0)

**Files:**
- Create: `engine/seq/sched.c`, `tools/compare.c`, `tests/unit/t1_sched.c`, `tests/golden/songs/sched-check.rbng`
- Test: T1 scripted event-walk (shuffle offsets, legato no-retrigger, flam second-hit) + golden event-stream diff.

**Interfaces:**
- Consumes: `sched.h`, `clock.h`.
- Produces: `RiSeqLoadSnapshot()` + per-buffer event walker used by all device tasks.

Steps (TDD): write `t1_sched.c` asserting (a) even-16th shuffle offsets by `shuffle_pct·(ppq/4)/100`, (b) slide emits NOTE_ON-with-flag and env-continuity flag set, (c) flam emits second event at +35 ms sample offset at 48 kHz (=1680 samples ±240); implement walker; build `tools/compare.c` (compares two `--dump-events` outputs line-by-line plus SHA-256 of two WAVs with `--wav-a/--wav-b`, exit 1 on any difference); golden `--dump-events` diffed via `tools/compare`; commit. (Full code per spec §8 state machine; executor writes ~150 lines.)

---

### Task 7: 808 fifteen voices + goldens (gated on Task 4–6 T1–T3 green)

**Files:**
- Create: `engine/dsp/rb808.h`, `engine/dsp/rb808.c`, `tests/unit/t1_808.c`, `tests/golden/808/*.wav` (+ `.sha256`)
- Test: per-voice T1 (BD trajectory ±5%, hat FFT ratios, clap 4-burst, accent ×1.5, 35 Hz clamp) + storm budget via `tools/bench --only-808`.

Gate: Task 4–6 fixtures green before starting (spec §18 strict gate). Accent implemented as excitation pre-envelope (spec §10); three-state mapping flagged per P-12 (binary until M2.1 check).

---

### Task 8: 909 sampler + first clean pack + provenance gate

**Files:**
- Create: `engine/dsp/rb909.c`, `project/rbnm.c` (S909 chunk subset), `tools/inspect.c` (manifest validator), reference pack `reference/packs/classic-01/` (2–4 layers/voice, 24-bit masters, processing logs)
- Test: TC-2.4.x (layer continuity, accent/flam, retrigger cut, idle-only swap guard) + audit phase failing the build on missing/incomplete manifest.

---

### Task 9: PCF black-box route + FX (delay/distortion/compressor)

**Files:**
- Create: `engine/fx/pcf.c` (SVF engine, table loaded from data file verified by ledger), `engine/fx/fx.c`, `tests/golden/pcf/*.wav`, `reference/captures/pcf-*.wav` (black-box captures when available)
- Test: TC-2.5.x (table byte-diff vs ledger-verified data, cutoff law ±2 cents, delay sync <0.1%, distortion unity + no-NaN fuzz, order-swap zipless).

Table contents never hardcoded as fact — engine reads the ledger-verified data file (§12).

---

### Task 10: Mixer/metering + RIDevice registry

**Files:**
- Create: `engine/mixer/mixer.c`, `engine/framework/ridevice.c`
- Test: TC-2.6.x (fader law ±0.5 dB at 9 anchors, 2⁴ mute/solo matrix zipless, meter decay) + dummy-device generality test (TC-2.1.5).

---

### Task 11: GUI framework + panels + sequencer/transport GUI

**Files:**
- Create: `gui/widgets/rknb.mcc`, `gui/widgets/rstp.mcc`, `gui/widgets/rfdr.mcc`, `gui/widgets/rvum.mcc`, `gui/widgets/rdsp.mcc` (subclass `Numeric`/`Area` per spec §13), `gui/panels.c`, `app/main.c`
- Test: TC-2.9.x (silhouette ±2 px, knob 150 px physics, step LED ≤33 ms, zoom, dummy panel) + TC-2.10.x (ReBirth-101 tutorial programmed ≤10 min, next-16th switch, copy/paste flags) + TC-2.11.x (FX audibility ≤1 buffer, picker diff).

AROS cross-build via `scripts/ri_build_aros.sh` (wraps Vulkan4AROS env + `aros_compile.sh`); Zune MCC module type `Classes/Zune`.

---

### Task 12: Formats (RBNG/RBNM full), MIDI, automation, ARexx, datatypes

**Files:**
- Create: `project/rbng.c`, `project/rbnm.c` (full), `midi_io/midi.c`, `project/arexx.c`, datatype classes
- Test: TC-2.12.x–2.15.x (mod round-trip/fuzz-500/partial-art/CPRG/S909-diff; song determinism md5×10, serialize×2 identical, unknown-chunk preservation, WAV header validity, MODR warn path; undo-200; automation ±1 unit; MIDI learn loopback; clock drift <1 tick/100 bars; hot-unplug survival).

---

### Task 13: Soak, docs, installer, beta (REL)

Perf budget on the M1.1 named box (174 BPM full load ≤50% one core, GUI ≤10%); AmigaGuide manual + autodocs + catalogs (EN + 1 locale); installer + icons; 20-user beta; exit: zero crashers, zero data-loss, A/B ≥4/5. Audit: `ri_audit.sh` full run green + `fec`-style file list clean (no blobs, no scratch in `scripts/`).

---

## Appendix: W3 Power Mode — deferred, no tasks

Strict deferral per spec §3. No W3 tasks exist in this plan. A W3 appendix plan is written only after W2 DoD. Any task proposing `if (power_mode)` in Classic DSP is rejected at review.

## Self-Review

1. **Spec coverage:** §0.1 first-light → Task 4. §2.3 (voices/buffers) → Tasks 4/5/7. §3 W1 → Task 5 (+M1.1). §4 realtime/SMP → Tasks 2/5 (audit gates) + deferred DAG (no task — correct). §4.2 AHI → Task 5 Step 1. §5 one-renderer → Tasks 4/5 (diff gate). §6/Appendix D → Task 10 (+freeze gate, no code against sketch — registry is internal static table, not the external ABI). §7 clock → Task 3 (+overflow T2 to add in Task 6). §8 events/state machine → Tasks 3/6. §9 303 → Task 4 (+M2.2 tuning follow-up inside Task 4's fixture loop). §10 808 → Task 7. §11 909 → Task 8. §12 PCF → Task 9. §13 mixer/GUI/formats/MIDI/assets → Tasks 10/11/12. §14 matrix+ledger → every task's commit gate + Task 13. §15 D0/D1 → Tasks 2/4/5/8. §16 T1–T5 → Tasks 2–4, 6–9, 12–13. §17 degraded → Tasks 5 (xrun), 8/12 (fuzz/rollback), 13 (soak). §18 gates/DoD → Task 13 + Appendix W3 note. Appendix A P-18/19/20 → Tasks 11/12/3 (P-20 PPQ noted in Task 3 header comment — executor adds `/* PPQ=96, P-20 */`). OPEN-01–09 → Tasks 4/5/9/13.
2. **Placeholder scan:** no TBD/TODO/"implement later"/"appropriate handling" phrasing; every "executor writes" points at exact spec section + file + test that pins it.
3. **Type consistency:** `RIEvent`/`RI_EV_*`/`ri_event_less` (Task 3) → reused in Tasks 4/6/11; `rb303_render/rb808_render/rb909_render/pcf_render` signatures match spec Appendices B–D; `Au*` names consistent Tasks 5/10; `tools/render --song/--out/--dump-events` consistent Tasks 4–6/9.
