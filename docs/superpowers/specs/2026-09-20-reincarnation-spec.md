# ReIncarnation — Engineering Specification v4

**Target:** AROS
**Project:** ReIncarnation
**Primary compatibility target:** independently measured/documented behavior of the RB-338-class groovebox workflow
**Status:** Engineering specification v5 (v4 + review 2026-09-20 #3: §0.1 first-light rule; engine block vs device buffer split; AHI hook-context contract §4.2; event payload mapping + total order §8; f32 bus / f64 master reconciled §15; accent placeholder reconciled §2.3; 128-bit clock arithmetic §7; OPEN-09 AHI latency floor)
**Date:** 2026-09-20

**Sources (verbatim inputs in `llm-wiki/raw/articles/`):**
- `2026-09-20-aros-audio-modernization-rebirth-2.0-plan.md`
- `2026-09-20-reincarnation-deep-technical-dives.md` (Part 1)
- `2026-09-20-reincarnation-deep-technical-dives-part2.md` (Part 2)
- `2026-09-20-reincarnation-deep-technical-dives-part3.md` (Part 3)
- `2026-09-20-reincarnation-wbs-m2-2-to-m2-6.md` (WBS, modules 2.1–2.16, TC gates)

## Normative contracts (one page — everything else is supporting material)

`[LOCKED]` items: boring legal note (§1); E0–E5 + confidence scale (§2); single-threaded Classic ownership, no-alloc/no-lock/no-Forbid render contract, SPSC + snapshot enforcement (§4); one renderer for live and offline (§5); `RIEvent` struct + same-sample ordering + sort key (§8); rational-clock pattern + boundary clamp + locked timing rules (§7); D0+D1 determinism + D1 enforcement list (§15); T1–T5 + golden rules (§16); IFF forward-compat rules + SHA-256 identity (§13); W3 engine separation + strict deferral (§3); second skin "808-RI" (§13); 15-voice 808 set + buffer default/range (§2.3); bench measurement procedure (§10); degraded-mode behaviors (§17); strict build gates + W1.1 report gate (§18).

Open gates (machine-readable — tools extract rows starting with `| OPEN-`):

| ID | Gate | Locked by |
|----|------|-----------|
| OPEN-01 | Slide TC value (Appendix A P-03) | M2.2 blind 40-vs-60 A/B verdict |
| OPEN-02 | All Appendix A parameters P-01–P-20 | Named TC measurement each |
| OPEN-03 | 303/808/909 equation appendices B–C candidates | A/B + measurement TCs |
| OPEN-04 | PCF pattern contents (54×16) | Per-pattern ledger rows + black-box captures |
| OPEN-05 | CPU budget numbers | W1.1 benchmark report on named machine |
| OPEN-06 | Reference box identity | NAMED 2026-09-22 — Dell Latitude E6320 (iteration reference); ABIv1 ultimate target / acceptance lane (decision: llm-wiki/raw/articles/2026-09-22-dell-reference-box-iteration-abiv1-target.md) |
| OPEN-07 | API/ABI freeze (Appendix D checklist) | Freeze review, post-Classic |
| OPEN-08 | D-001 follow-ups | None — closed ("808-RI"); row kept so tools see zero open naming gates |
| OPEN-09 | Achievable device buffer / latency on AROS AHI (low-level vs `ahi.device` path) | MEASURED 2026-09-22 — low-level 64 frames; device dev_min=0 (abort-bounded metric); PlayerFreq accepted-but-not-honored at fixed ~11 Hz; evidence docs/evidence/formats/m1-1-report.md App. B + ABIv1 session-9 full green |
| OPEN-10 | ReBirth `.rbs` import (clean-room, user-owned files) | Legal review first, then importer (adopted 2026-09-24, review D-i) |

Progress is declared ONLY via the parity matrix (§14) and the evidence ledger (`docs/evidence/`). Binary "M2.x done" language is banned — milestones name gate sets, never declare parity.

## 0. Engineering doctrine

ReIncarnation is built according to five rules:

1. Measure before claiming parity.
2. One implementation path for live playback and offline rendering.
3. The realtime path allocates nothing, blocks on nothing, and performs no OS/UI work.
4. Every externally observable behavior has a testable contract.
5. Unknowns remain explicitly marked as unknowns until verified.

6. One thing works completely before the next thing exists (Terry rule).

The specification distinguishes between requirements, implementation decisions, measured behavior, hypotheses, and future work.

### 0.1 First light `[LOCKED]`

The first artifact of this project is not a library, a GUI, or an audio driver. It is one command:

```
tools/render --song tests/golden/303/first-light.rbng --out /tmp/first-light.wav
```

that renders ONE 303 playing ONE 16-step pattern through the scheduler, the 303 voice and the master bus, offline, deterministically (D1), and matches its own golden SHA-256 on a second run. No AHI, no MIDI, no GUI, no 808/909/FX, no mods. Everything in this document that is not needed for that command is `[DEFERRED]` until it passes T1–T3. Every later module is added the same way: one device, one fixture, one golden, then the next. A module that cannot be exercised by `tools/render` alone does not exist yet.

No statement in this document is intended to constitute legal advice or a determination of intellectual-property rights.

Status markers used throughout: `[LOCKED]` `[HYPOTHESIS]` `[OPEN]` `[DEFERRED]` `[UNVERIFIED]` `[LEGAL REVIEW]`. A value without an evidence class is not normative.

## 1. Legal note (deliberately boring)

Compatibility target: reproduce the documented/tested musical behavior of the reference application to the extent independently verified. This specification does not make a determination regarding copyright, trademark, trade dress, patent, design-right, or other intellectual-property status. `[LEGAL REVIEW]`

Project rules carried forward from v1 inputs: original artwork and newly recorded/licenced audio material only; no redistribution of third-party images or samples; external name ReIncarnation; legal review before any public release. Code prefixes `RI`/`RB`. ARexx primary port `ADDRESS REINCARNATION`; parser accepts `REBIRTHAROS` as a deprecated alias. `[LOCKED]`

## 2. Evidence classes and compatibility model

### 2.1 Evidence classes

| Class | Meaning |
|-------|---------|
| E0 | Design decision |
| E1 | Public documentation |
| E2 | Direct observation of reference behavior |
| E3 | Instrumented measurement of reference behavior |
| E4 | Independently reproduced and regression-tested behavior |
| E5 | Cross-validated against multiple independent observations |

Confidence: LOW (insufficient evidence), MEDIUM (observed/reasonably reproduced), HIGH (repeatedly measured and regression-tested), LOCKED (implementation contract; change requires explicit design decision).

### 2.2 Compatibility dimensions (tracked independently)

- BC — Behavioral compatibility: sequencing, timing, parameter response, automation, pattern/song behavior.
- AC — Audio compatibility: observable characteristics of synthesized and sampled audio.
- FC — File compatibility: RBNG/RBNM serialization and compatibility behavior.
- UXC — Workflow compatibility: keyboard, mouse, MIDI, transport, editing, operating workflow.

A feature is not parity-complete merely because it is implemented. Parity matrix (§14) tracks Implemented/Measured/Verified/Confidence per feature instead of a binary "W2 done".

### 2.3 Reconciliation log (v1 conflicts resolved here)

1. 808 voice list: inputs split between 14 (omitting clave) and 15 (with clave). `[LOCKED]` classic set = BD, SD, LT, MT, HT, LC, MC, HC, RS, CL, CP, CH, OH, CY, CB (15 drum voices) + section accent bus. WBS "14-voice" readings mean "all voices".
2. Buffers — two different things, never conflated: the **engine block** (scheduler/render granularity, `[LOCKED]` 64 frames at 48 kHz, fixed) and the **device buffer** (what the backend negotiates with AHI, `[HYPOTHESIS]` 64..4096 frames per `AUO_BufferFrames`, always an integer multiple of the engine block). Offline rendering uses engine blocks only. The achievable device buffer on real AROS/AHI is OPEN-09 — v1's "64-frame buffers" was a latency wish, not a platform fact (register entry 18 records a ≥20 ms floor on the shared-device shim path).
3. PCF index: pattern select 0..53 is stored state; step index derives from `beat_pos` on the 16th grid (v1 conflated the two).
4. 909 flam: `[HYPOTHESIS]` nominal 35 ms, per-voice table 30–40 ms, second hit ×0.75.
5. 808 accent: the v1 gain formula `accent ? 1.0 : 0.66` (= ×1.5 boost) is the **placeholder** for the pre-envelope excitation model of §10 (register entry 9) and the three-state off→weak→strong question (P-12). It lives in one place (`dsp/params.c`), is a ledger row, and is not a contract.
6. 808 voice set (adopted 2026-09-24, review D-b): the playable model is **11 slots / 16 sounds with 5 switches** (LT/LC, MT/MC, HT/HC, RS/CL, CP/MA — MA included), superseding item 1's 15-voice lock above (kept as the historical record; code migrates in §12 item 5). Per-sound Level plus per-sound parameters replace kit-wide knobs.

## 3. Workstreams (reduced W1 first)

- W1 — AROS audio foundation. Initial target `[LOCKED]`: float32 buses, stereo, 48 kHz, 64-frame engine block, offline WAV renderer first (§0.1), AHI output backend second, deterministic engine clock, realtime-safe render contract. Platform facts (register entries 17–20): `audio.library` is an AHI-client (unit-select + `ahir_Type`/`Frequency` negotiation + double-buffer contract, E2); channel/format ceilings and anti-click come from the AHI v6 mixing model (E1); latency floor ≥ 20 ms applies when routing through the shared-device shim (E1); MIDI = `camd.library` + `realtime.library` with pluggable `DEVS:MIDI/` drivers (E2); USB-MIDI scoped to Poseidon `camdusbmidi.class`/Bulk, USB-Audio/isochronous out of scope (E1). Multichannel, SMP rendering, SIMD optimization, additional formats are staged **after** the single-threaded reference path is proven (review §14). W1 milestones M1.1–M1.4 retained as staged followers, not day-one gates.
- W2 — ReIncarnation Classic: two monophonic 303s, 15-voice 808, 909 layered sampler, sequencer, mixer, Delay/Distortion/Compressor/PCF, MIDI, GUI, RBNG/RBNM, WAV/AIFF export.
- W3 — Power Mode: separate compatibility domain. `[LOCKED]` No W3 feature may alter the Classic execution path; implementation uses a separate engine/capability table (`RI_ENGINE_CLASSIC` vs `RI_ENGINE_POWER`), never `if (power_mode)` scattered through DSP. Decision 2026-09-20: strict deferral confirmed — no W3 design text beyond the locked feature list until W2 gates pass. W3 list (polyphonic 303, extra patterns, multichannel outs, 96 kHz, oversampling, per-knob LFO, second PCF, sidechain, network/MIDI expansion) is `[DEFERRED]` with its own future TC appendix.

## 4. Realtime contract `[LOCKED]`

The audio render path MUST: perform no dynamic allocation, no DOS operations, no GUI operations, no blocking IPC, no unbounded loops; avoid global mutable state; use preallocated buffers; consume immutable parameter snapshots; use lock-free communication where required. MUST NOT call `Forbid()`/`Disable()`. GUI/control plane may allocate, block, and communicate normally. Enforcement (register entries 29–30): SPSC command FIFO + buffer-boundary snapshot application + completion queue back; a CI audit gate greps the render path for alloc/lock/IO/syscall constructs.

### 4.1 Render-thread ownership `[LOCKED]`

Classic ships single-threaded first (Terry rule: make one thing work completely). Per audio callback ONE render thread owns the whole graph in fixed order: scheduler → 303A → 303B → 808 → 909 → FX inserts → mixer → master → backend sink. No device runs concurrently with another in Classic; no barriers, no work-stealing, no cross-thread buffer sharing. Scratch pools and bus buffers are preallocated per callback size class (64/128/256/…/4096) and reused; the callback never grows them.

SMP job DAG is `[DEFERRED]` design-only (never built before W3): audio callback owns the snapshot + scratch and fans out per-device jobs (303A, 303B, 808, 909), each job owning ONLY its section bus buffer; an explicit barrier joins jobs before the mixer job sums to master. Buffer ownership: job writes its bus, mixer reads after the barrier, backend reads master after mixer — single-writer per buffer per phase, no sharing within a phase. Miss policy: a worker that misses its deadline never blocks the callback — its bus renders silence for that buffer and an underrun is logged (audible glitch preferred over deadlock, and the glitch is counted in T5). No work-stealing in Classic scope. Cache rule: bus buffers are contiguous float arrays sized to the callback frame count, allocated once at `AuStart`.

### 4.2 AHI hook context and the render task `[LOCKED]`

AHI's `SoundFunc`/`PlayerFunc` hooks (low-level API) and the `ahi.device` I/O completion run in the driver's interrupt/mixer context, not in a Task: inside them the ONLY permitted call is `Signal()` (or `Cause()`) — no library calls, no memory access outside preallocated buffers, no floating point. Therefore the render path never lives in a hook. Structure: a dedicated render **Task** (priority above the GUI, below input/handlers; value is P-21) blocks in `Wait()` on a signal the hook raises; on wake it renders exactly the number of engine blocks the device buffer needs into a preallocated double buffer and hands it back. The GUI never touches audio buffers; the render task never touches DOS/Intuition. Which AHI path (low-level `AHI_AllocAudio`/`AHIA_MixFreq`/`AHIA_PlayerFunc` vs `ahi.device` `CMD_WRITE` double-buffering) gives the lower reliable device buffer on AROS is **measured, not assumed** (OPEN-09, M1.1): the backend is written against the low-level API first because it is the only one that exposes the mixing frequency and the player hook; `ahi.device` stays the fallback backend. Under-runs surface as §17 #5, never as a hang.

## 5. Engine architecture (one renderer)

```
GUI / MIDI / ARexx
        │
        ▼
immutable snapshot
        │
        ▼
transport / scheduler
        │
        ├── 303A
        ├── 303B
        ├── 808
        └── 909
                │
                ▼
          section buses
                │
                ▼
          FX / mixer
                │
                ▼
             master
                │
        ┌───────┴────────┐
        ▼                ▼
   audio backend      file backend
```

There is exactly one DSP/render implementation. Live playback and offline export MUST use the same engine with different output sinks. No separate live/offline renderers.

Device framework concept (WBS §0, not yet ABI): `RIDevice` = DSP + panel + control map + mod hooks; four sections are the first four registered devices; dummy-device registration/removal without framework changes remains the generality proof (TC-2.1.5/TC-2.9.5).

Repo layout (review §33): v1 layout plus `tests/`, `tools/`, `reference/` so the project is a laboratory, not just an app:

```
ReIncarnation/
├── app/ ├── engine/{dsp,mixer,fx,seq,framework}/
├── audio_io/ ├── midi_io/ ├── gui/ └── project/
├── tests/{unit,property,golden,integration,fuzz,soak}/
├── tools/{render,analyze,compare,inspect}/
├── reference/{captures,measurements,evidence}/
└── docs/
```

## 6. Device interface (concept — sketch lives in Appendix D)

The device framework (WBS §0) is a `[LOCKED]` architectural decision: `RIDevice` = DSP + panel + control map + mod hooks, four sections first, dummy-device generality proof (TC-2.1.5/TC-2.9.5). The C signatures are an **implementation sketch in Appendix D — explicitly not ABI**. No production code may be written against the sketch: the Appendix D freeze checklist (size/versioning, ownership, lifetime, threading, reentrancy, alignment, error handling, packing, capability negotiation, NULL semantics, sample format, channel ownership, vtable shape) is a hard gate before any freeze. Same demotion applies to `RIFX`, mixer, and MIDI surfaces.

## 7. Clock (no float authority)

Transport clock is the sole source of musical time. Engine maintains monotonic sample counter + tempo segment + PPQ position + fractional tick accumulator. PPQ initially 96 `[HYPOTHESIS]` (parameter P-20, Appendix A). Floating-point accumulation MUST NOT be the authoritative long-term position — conversion is exact rational per tempo segment: single widened `mul_div(dticks, ns_per_quarter·sr, ppq·1e9)` (128-bit intermediate — `unsigned __int128` on x86-64 AROS/clang, or a two-limb portable fallback; a T1 test proves the intermediate cannot overflow for `dticks ≤ 2^40`, `ns_per_quarter ≤ 2^40`, `sr ≤ 2^18`) with ONE rounding (Round at schedule time, Floor at lookup), error bounded ±0.5 sample per tempo change and never accumulating; reverse lookups clamp boundary ticks to the owning segment (register entry 27, E0+E4 pattern). v1 `ticks = samples·PPQ·BPM/(60·SR)` is the concept, not the implementation.

Locked sequencer-timing rules `[LOCKED]`:
- Tempo change mid-buffer: the buffer splits at the exact change sample; ticks before it use the old segment, ticks after use the new segment. Nearest-sample rounding is banned — the split is sample-exact.
- Pattern/song changes not on 16th boundaries: the current pattern instance plays to its end; the switch applies at the next 16th boundary. Mid-pattern cutoffs are banned.
- Shuffle order: shuffle offsets trigger times first; slide durations then stretch/shrink from the offset positions; flams are emitted last as separate timestamped events. Order is shuffle → slide-legato resolution → flam emission.
- 64-bit overflow: the master sample counter is uint64. At 192 kHz it overflows after ~3 million years — no wrap handling required; the fractional tick accumulator is re-anchored at every tempo event so long-session drift cannot accumulate. A T2 property test asserts monotonicity across a simulated 24-hour run.

## 8. Event contract `[LOCKED]`

```c
struct RIEvent {
    uint64_t sample;    /* master-clock sample position (absolute) */
    uint32_t type;      /* RI_EV_* below */
    uint16_t device;    /* 0..3 classic (303A, 303B, 808, 909), else RIDevice index */
    uint16_t voice;     /* per-device voice index */
    uint16_t value;     /* note/param/layer id (type-dependent) */
    uint16_t flags;     /* accent/slide/flam2/octave bits (type-dependent) */
};

/* Event types [LOCKED] */
#define RI_EV_TRANSPORT      0
#define RI_EV_PATTERN_CHANGE 1
#define RI_EV_NOTE_OFF       2
#define RI_EV_NOTE_ON        3
#define RI_EV_NOTE_CONTINUE  4   /* rest+slide: gate stays high, pitch slews */
#define RI_EV_ACCENT         5
#define RI_EV_FLAM            6   /* scheduler-emitted second hit, nominal +35 ms */
#define RI_EV_AUTOMATION     7
#define RI_EV_PARAM          8
#define RI_EV_METER          9
```

Payload mapping per type `[LOCKED]` (the struct has no free field, so this table is the contract): NOTE_ON/OFF/CONTINUE: `value` = note number (303) or layer id (909), `flags` = accent | slide | octave bits; ACCENT: `value` = accent level (0/1, or 0/1/2 once P-12 lands); FLAM: `value` = delay in samples (scheduler-computed, ≤ 65535), `flags` = second-hit bit; AUTOMATION and PARAM: `value` = control ID (§13, 16-bit intent range fits), `flags` = the 0..127 value; TRANSPORT: `value` = start/stop/continue; PATTERN_CHANGE: `value` = pattern index, `flags` = section; METER: `value` = bus id, `flags` = level (0..127 log). A type that needs more than 16+16 bits is a new type, not a packed one.

Same-sample ordering `[LOCKED]`: TRANSPORT → PATTERN_CHANGE → NOTE_OFF → NOTE_ON → NOTE_CONTINUE → ACCENT → FLAM → AUTOMATION → PARAM → METER. Full sort key: `(sample, type-priority, device, voice, insertion index)` — the last term makes the order total, so two producers emitting the same `(sample, type, device, voice)` (e.g. MIDI-in and pattern) resolve deterministically (D0) and the sort is stable by construction. In-memory scheduling struct only — never serialized (RBNG stores its own PATT/SONG/AUTO encoding), so C tail padding is irrelevant; field order above is normative. Evidence: E0 design decision, frozen by this spec; T2 asserts valid ordering on every buffer.

Pipeline: SONG → PATTERN → TIMING/SHUFFLE → device/automation fan-out → free-running PCF clock. Events carry in-block sample offsets, lists are sorted by the §8 key, and blocks split at event boundaries (register entry 28, E0 pattern). Parameters have two routes: inline at `process()` time (sample-accurate, with gesture begin/end) vs out-of-band snapshot load (offset info lost) — the scheduler uses inline for musical events and snapshot loads only across stopped-transport boundaries; the exact mapping is asserted by T2 ordering tests. Snapshot model (`RISeqSnapshot` built under `AuLock`, consumed lock-free), ~100-event preallocated scratch pool, zero alloc/lock per buffer.

303 gate/slide state machine `[LOCKED]` (narrative above is explanatory; this table is normative):

| Step input | Gate | Envelopes | Pitch | Emitted event |
|------------|------|-----------|-------|---------------|
| New note, no slide | retrigger (low→high) | reset | jumps | NOTE_ON (+ACCENT if flagged) |
| New note + slide | stays high | do NOT reset | slews at fixed τ_slide rate | NOTE_ON with slide flag (legato) |
| Rest + slide | stays high | do NOT reset | slews to held pitch | NOTE_CONTINUE |
| Rest, no slide | low | release | holds | NOTE_OFF |
| Accent flag on any of the above | — | accent env amplitude set for the note/continuation | — | ACCENT |

909 flam-as-separate-event and shuffle-stretch-slides stand as `[HYPOTHESIS]` with TC gates in §16.

Recording/MIDI/edge cases per Part 3 §2.5–2.6 retained: `AuSet*Attr` live path (~1–2 ms), GUI-thread 30 Hz tweak recorder at ppq/24 (song stays GUI-owned), CAMD bridge stamped against `AUQA_MasterClock`, next-16th pattern/song switches, pre-wrapped loops, sample-exact tempo changes.
> **Status: Outdated (2026-09-26, automation r2):** the recorder grid is the 32nd note = ppq/8 with forward quantize (E1 p. 84), not ppq/24. Retained above for history; see the automation design spec.

## 9. 303 implementation (targets, not truths)

Chain: oscillator → envelope → VCA → nonlinear filter → output. All numeric values in this section are Appendix A parameters (P-01–P-06), not contracts — the contracts are the chain order, the no-retrigger slide rule, the tanh-nonlinearity requirement, and the M2.2 A/B + measurement TCs. Initial targets `[HYPOTHESIS]` (evidence ledger entries required before M2.2 completes): amp τ 350 ms; accent τ 60 ms (`accent_env = 1 + A·exp(-t/60ms)`, VCA clamp 1.2); slide τ 30–50 ms nominal 40 ms; resonance coefficient range 0..~3.8; `g = 1 − exp(−2π·fc/SR)`; `fc = fc_base·2^((CV_env+CV_accent)/1200·env_mod)`; decay-knob τ 80 ms..4 s; accent resonance boost ~15%; VCO saw + 50% square ±1.0f, 0.5 ms crossfade + deterministic click on toggle.

Open303-lineage starting values (Evidence E1 — public code/docs, NOT parity claims; M2.2 A/B decides, both recorded): slide ~60 ms constant-time exponential (Open303 `slideTime` default; pitch-slew TC 60 ms; x3daw/openDAW "RC 12 ms → ~60 ms effective"; pitch-CV slide LPF 7.23 Hz); VEG normal decay 1230 ms / accent decay 200 ms; releases 0.5 ms normal / 50 ms accent; attacks ~3 ms both; MEG accent decay fixed 200 ms regardless of Decay knob; Decay knob 200–2000 ms exponential; post-filter chain allpass 14.008 Hz / HP 24.167 + 44.486 Hz / notch 7.5164 Hz BW 4.7 / feedback-HP 150 Hz. Explicit tension recorded: inputs said slide nominal 40 ms (30–50); lineage says ~60 ms. Decision 2026-09-20: implement slide TC as a runtime parameter; the M2.2 A/B rig compares 40 vs 60 blind (listening + step-response measurement) and the value locks only on the verdict, with both numbers kept in the ledger.

Filter honesty rule (review §§6–7): "tanh 3-stage 18 dB ladder" is NOT a reproducible filter spec. Parity requires: state equations, update ordering, saturation location, input/output drive scaling, feedback topology, resonance/cutoff mappings, state init, parameter interpolation, stability limits, denormal handling, Nyquist behavior, resonance-extreme behavior, sample-rate assumptions. The v1 recurrence shape is an **implementation candidate, Evidence E0, UNVALIDATED** — it enters normative text only after A/B + measurement TCs (TC-2.2.x) pass. Prior-art constraints on the candidate (register entries 3, 4, 7, 8): mismatched first-stage coefficient + feedback-loop HPF (150 Hz E2 default) + post-filter HP chain (44.486/24.167 Hz E2 defaults) instead of an ideal Moog cascade; ZDF discretisation (`g=tan(π·fc/fs)`, 2× oversampling) is the candidate family. Accent correction (entry 6, E3): accent is NOT a gain boost — MEG decay forced to minimum + shortened MEG summed onto the VCA (multi-segment, higher peak) + smoothed accent-sweep env onto cutoff with re-trigger buildup state. The `accent_env` formula above is the placeholder for that dual-envelope model until M2.2 measures it. Sequencer gate timing (entry 5, E1): 3-clocks-on/3-off per 6-pulse step, ~44 µs slide-latch pulse even on non-slid notes, gate held high across slid steps — the scheduler reproduces these, not a fixed 50% duty. Contract `RB303Voice` + `rb303_render` + `rb303_set_param` (0..127 curves in `dsp/params.c`) retained as interface sketch.

## 10. 808 implementation (15 voices, equation per voice)

Voices: BD SD LT MT HT LC MC HC RS CL CP CH OH CY CB. Each voice gets an explicit synthesis spec with: SOURCE, OSCILLATOR, PITCH, PITCH ENVELOPE, AMPLITUDE ENVELOPE, NOISE, FILTER, DISTORTION, MIX, ACCENT, OUTPUT — full equation skeletons live in Appendix C as E0 candidates. v1 recipes (parameters P-07–P-12, Appendix A) are `[HYPOTHESIS]` targets with TC-2.3.x gates; undocumented constants stay targets, never parity claims. Prior-art refinements (register entries 9–13): accent scales excitation pre-envelope (E0 service-manual trigger-amplitude model, 4–14 V, 7–14 V metal voices) — the `accent_gain` multiplier is its placeholder; kick = decaying sine + ~6 ms attack frequency bump + slow leakage sweep with accent-scaled excitation (E1); metal generator = six-oscillator cluster at the stated nominals with BPF→VCA→HPF chain + oversampling/warp-correction test (E1); snare = two partials at ~1.93× ratio with revision tolerance — 173.3/336.0 and 249.6/499.0 both pass (E3); clap = 3+1 bursts plus a SEPARATE ~100 ms tail level param (E3). Open discrepancy (entry 24, E3): reference hardware cycles 808 steps off→weak→strong (three-state); the binary accent model holds only until M2.1 verifies the mapping.

Performance: deleted "ops/sample" as a requirement (unmeasurable — tanh cost varies 5–100× by implementation). Replaced by measured CPU budget under defined worst-case conditions. Measurement procedure `[LOCKED]`: `tools/render` bench renders the fixed worst-case fixture (48 kHz, 64-frame buffers, 174 BPM, all 15 808 voices max decay + full accent, both 303s sliding/accented, 909 flam storm, all 4 FX active) for 30 min on the named reference box; report = % of one core + underrun count, with CPU model, clock, compiler, opt level, SIMD mode recorded alongside. Provisional budgets `[HYPOTHESIS]` to be confirmed/adjusted by the W1.1 benchmark (never a gate that blocks M2.2): full Classic load ≤ 50% of one reference core; single 303 ≤ 10%; 808 storm ≤ 10%; 909 ≤ 15%; FX chain ≤ 15%; GUI ≤ 10% CPU. Decision 2026-09-20: reference box stays unnamed until M1.1 — the benchmark report names the exact machine and this spec is updated then. The W1.1 report sets the final numbers; this spec records the method, not the verdict.

## 11. 909 implementation (provenance + split tune model)

Layered sampler: `SampleLayer{data, frames, rate, lo, hi}` + `RB909Voice` (Appendix D sketch, not frozen). Numeric voice values below are Appendix A parameters (P-13–P-15). Prior-art model (register entry 14, E0+E2): the reference is a hybrid — analog dual-oscillator layers plus 6-bit companded ROM layers; Tune acts as sample clock, accent as VCA scale, crash/ride decay shortens with Tune, and open/closed hats share one ROM so they cannot sound together (voice-steal rule, reproduced). Mandatory per-sample manifest in RBNM: sample ID, source, recording date/equipment, license + holder, processing, rate/depth, normalization, loop points, layer mapping — "know where every byte came from". Clean-room rule (entry 16, E5): no ROM dumps and no third-party samples in the shipped set — synth-from-scratch or own recordings only, with licence paperwork per sample. Tune/morph split explicitly: knob→layer position, layer interpolation (~8-position triangular crossfade, `[HYPOTHESIS]`), and pitch scaling are separate definitions, never conflated (precedent: entry 15, E2 layer-per-voice schema). Accent model (none ×1.0; acc1 accent-layers ×1.15 + shelf; acc2 flam on capable voices else = acc1), per-voice knob table incl. crash/ride accent-no-op quirk, monophonic retrigger, idle-only mod swap — retained as `[HYPOTHESIS]` under TC-2.4.x.

## 12. PCF (demoted — black-box plan + own ledger gate)

The 54×16 pattern table is **STATUS UNVERIFIED placeholder dataset** and is NOT part of the normative core. Two routes to lock it (either satisfies the gate): (a) measured black-box behavior — record reference input/output for a spread of patterns under controlled conditions (fixed input bus, documented base/Q/amount, 16th-grid captures) and fit table rows to captures; or (b) explicit demotion of individual patterns to a later milestone, each with its own ledger row. Every pattern needs source/confidence/capture/reconstruction/validation rows before it locks; until then no implementation may hardcode pattern contents as fact — the engine reads them from a data file the ledger verifies. Locked requirements regardless of route: 12 dB SVF engine shape (candidate, Appendix B family), free-running transport clock, fixed-seed determinism, 16th-grid stepping, 54-tile picker UI. Contract `struct PCF` + `pcf_render` + count/table accessors live in Appendix D as sketch.

## 13. Mixer, FX, sequencer GUI, formats, MIDI

- Mixer: 4 section buses + master, mute/solo matrix, faders/pan/sends/inserts/meters. Meter ballistics (~20 dB/s peak-hold) and fader law are Appendix A parameters (P-16–P-17), verified under TC-2.6.x — not reference measurements.
- FX: Delay (BPM-sync), Distortion (asymmetric tanh, drive+Shape), Compressor (~4:1, auto make-up), PCF — each gets parameter mapping, processing equation, state init, latency, interpolation behavior, sample-rate behavior, regression fixture (spectral/numerical, not prose).
- GUI: functional workflow parity is the requirement — grouping, relative positioning, keyboard/mouse operation, feedback, workflow, value access. Classic does NOT promise pixel or feel parity with the reference artwork. Layout analysis against reference panels (control centers ±2 px @1024×768, proportions ±1%, style-matched type — never pixel-copying) is an E0 design acceptance threshold, not a measurement claim.
- M2.1 numeric interaction values `[LOCKED]` as E0 design decisions owned by this project (verified by TCs, not by reference measurement):
  - Knob (`RKnB`, Zune MCC subclass of `MUIC_Area`): vertical AND horizontal drag (ReBirth vertical kept + horizontal added per owner amendment 2026-09-24 — ReBirth manual says "drag up/down", owner finds left/right natural, both win), **150 px = full 0..127 on either axis** (~0.85 units/px; diagonal counts double, documented in `knob_logic.h`); Shift-fine ×0.1 (1500 px full); right-click = default; double-click = type-in requester; pointer grab so drags continue outside the window; continuous notify while dragging + single commit event on release = one undo unit.
  - Fader (`RFdr`): vertical, **100 px = full travel**; same fine/commit/capture rules as knob.
  - Step button (`RStp`): click toggles; LED update ≤ 1 frame (33 ms) of pattern position; 16th-note chase stutter-free at 174 BPM.
  - 303 programming (E1 — contemporary RB-338 reviews): mini-keyboard entry with Pitch mode (auto-advance per step) + Step/Back manual mode; octave up/down; per-step Slide/Accent/Rest flags; computer-keyboard rows `1..0` = step entry; double-click = accent entry (WBS quirk); no Tie button — same-pitch notes + Slide = tie.
  - 909 programming (E1 — RB-338 v2 review): per-sound single click = accent level 1 (dim LED) / double-click = accent level 2 (bright LED); AC selector = all-voice accent paint mode; Flam selector = per-step per-sound flam + resolution knob; flammed steps glow green, accented red.
  - Zoom 1x/1.5x/2x, artwork authored at 2x filtered down. Knob/step/zoom behaviors verified by TC-2.9.x–2.11.x.
  - Widget reuse (register entries 21–22, E0+E2): bind to the existing MUI classes `Numeric → Knob/Slider/Numericbutton/Levelmeter` (min/max/step + stringify) and skin ONLY via MCC subclassing with dispatcher + notify wiring; no new BOOPSI gadget classes. MCC module gate: `Classes/Zune`.
- Control IDs: stable intent (`0x030x/04xx/08xx/09xx/0Axx/0Bxx`), shared GUI/automation/MIDI/ARexx; W3 allocates new IDs with default-skin fallback.
- RBNM/RBNG: IFF-style chunking with explicit rules — big-endian `FORM[type] {ID32 size32 data + even pad}`, written via `PushChunk(IFFSIZE_UNKNOWN)` semantics (register entry 23, E0): readers MUST skip unknown optional chunks (length-delimited), reject unsupported mandatory features (flagged chunks), validate lengths/ranges/references/nesting, never trust chunk sizes for allocation. Skins/samples load through `picture.class`/`sound.class` datatypes; `8SVX/SMUS`-style conventions reused for samples/patterns where they fit. `format_major/minor + feature_flags`: major mismatch → reject; minor newer → load known, preserve unknown; unsupported feature → degrade/reject per flag. Unknown-chunk preservation across round-trip where supported. CRC32 = corruption detection; SHA-256 = content identity (song MODR stores name + SHA-256 + vers, not CRC32-only). RBNG/RBNM layouts, shuffle-as-tick-offset, AUTO (legacy form; automation lanes record at 32nd = ppq/8 with shared knob IDs), `PROGDIR:Songs/` + `SYS:Storage/` + `SYS:Classes/ReIncarnation/Mods/` + `DEVS:ReIncarnation/MIDI/`, datatypes, CPRG-on-load — retained.
> **Status: Outdated (2026-09-26, automation r2):** "AUTO ppq/24" above is superseded — the lane grid is the 32nd note = ppq/8 (E1 p. 84); AUTO stays byte-identical as the legacy form, ATRK (v1.2) carries larger lanes.
- MIDI: CAMD backend, note→step, CC→ctl_id learn, clock/MMC; hot-unplug survival under TC-2.8.x.
- Day-one mods: "Classic" default; second skin "808-RI" `[LOCKED]` (decision 2026-09-20, user-proposed); blank template + AmigaGuide SDK.
- Asset production pipeline `[LOCKED]` (process, not values):
  - 909 samples: newly recorded/synthesized layers only — 44.1 kHz/24-bit masters, peaks −3 dBFS, no dither until export; 2–4 layers per voice spanning tune 0..127; processing log kept per layer. 808 stays synthesized (mod ships tune/decay/level tables only). Waveform tables for S303 optional oversampled alternates.
  - Artwork: original pixels authored at 2x; panel-geometry doc → control-map sheet → MAPS chunks; frame counts declared per control (min 32, classic look 64); style-match, never copy.
  - Provenance manifest (mandatory — build fails without it): per sample/layer: sample ID, source, recording date, equipment, license + holder, processing chain, rate/depth, normalization, loop points, layer mapping. Stored in RBNM S909/S808/S303 + `reference/evidence/`.
  - Tooling: `tools/inspect` validates RBNM (chunk lengths, frame counts, CPRG present, manifest complete); `tools/render` offline-renders A/B fixtures; `tools/compare` reports SHA-256 + error metrics + event-stream diff.
  - Acceptance: TC-2.12.x (round-trip, 500-file fuzz, partial-art fallback, CPRG-on-load hook, S909 render diff) + DoD provenance-completeness gate.

## 14. Parity matrix (replaces binary "W2 done")

| Feature | Implemented | Measured | Verified | Confidence |
|---------|-------------|----------|----------|------------|
| 303 pitch/glide | — | — | — | LOW (P-01–P-03 pending; gate table §8 frozen) |
| 303 filter/VCA/accent | — | — | — | LOW (Appendix B candidate UNVALIDATED) |
| 808 voices (15) | — | — | — | LOW (Appendix C candidates; P-07–P-12 pending) |
| 909 layer morph | — | — | — | LOW (Appendix C candidate; P-13–P-15 pending) |
| PCF patterns | — | — | — | LOW (UNVERIFIED dataset; §12 black-box plan) |
| 909 accent/flam quirks | — | — | — | LOW (P-05 flam pending) |
| Sequencer/song/shuffle | — | — | — | LOW (event + timing rules frozen; PPQ P-20 pending) |
| Mixer/FX ranges | — | — | — | LOW (P-16–P-17 pending) |
| RBNM/RBNG round-trip | — | — | — | LOW (rules defined, tests unwritten) |
| PCF envelope (attack/decay, LP/BP) | — | — | — | LOW (adopted D-d; patterns OPEN-04) |
| Delay steps/triplet + fb 1.0 + pan | — | — | — | LOW (adopted; §3.2 target) |
| Compressor ratio + GR meter | — | — | — | LOW (adopted; §3.2 target) |
| 808 slots/switches + MA | — | — | — | LOW (adopted D-b; §2.3 item 6) |
| 909 instruments (11) + flam knob | — | — | — | LOW (adopted; §4.3 target) |
| Song mode + transport machine | — | — | — | LOW (adopted; §3.1 target) |
| Pattern edit ops | — | — | — | LOW (adopted; pure-function target) |

Cells move only on evidence (E3+ measurement, E4+ regression). This matrix and the evidence ledger are the ONLY places that declare progress. "Sounds close / looks right / worked once" are never completion criteria.

## 15. Determinism (three levels, honestly)

- D0 — Musical determinism: same project ⇒ same event stream. Classic MUST achieve D0 `[LOCKED]`.
- D1 — Platform determinism: same binary/architecture/runtime ⇒ identical PCM. Default audio regression requirement `[LOCKED]`. D1 enforcement list (register entries 30–31): render-path transcendentals routed through one allowlisted kernel set owned by this project (`engine/dsp/kernels.c`: `ri_tanh`, `ri_exp`, `ri_sin`, `ri_pow2` — pure-C, no libm, no FMA, no `-ffast-math`; the CI grep gate of §4 also fails on any libm transcendental symbol in the render path), fixed summation/reduction order, **f32 section buses summed into an f64 master accumulator with a single final f32 rounding** (this is the one place the W1 "float32" statement is widened; nothing else in the path is f64), denormals honored (no FTZ/DAZ), scalar voice ticking as reference. Golden fixtures are keyed by `(compiler, flags, arch)`; a D1 golden from another toolchain is a D2 claim and is not used.
- D2 — Cross-platform bit determinism: different CPUs/compilers/SIMD/libm ⇒ identical PCM. Optional, requires explicit engineering; never claimed implicitly. Path enabler if ever pursued: lane-identical deterministic transcendental kernels + cross-OS SHA-256 golden grid (entry 31). An offline D2 reference renderer may be added later `[DEFERRED]`.

v1 "same RBNG + mod + engine ⇒ same PCM, always" is withdrawn as stated; replaced by D0+D1 for Classic.

## 16. Testing hierarchy (T1–T5, failure-capable)

- T1 Unit: envelopes, oscillators, filter steps, pitch, mixer math — assert peak freq/gain/RMS bands, not "approximately similar".
- T2 Property: no NaN/Inf, bounded output, finite phase/state, monotonic sample counter, valid event ordering — always true or fail.
- T3 Golden fixtures in `tests/golden/{303,808,909,pcf,songs}/` + `tests/expected/`: lossless reference recordings with sample-exact compare as truth; waveform/spectrum PNGs generated ONLY on failure for diagnosis; missing or corrupt golden fails as broken-pipeline, never as pass (register entry 32, E2+E4 pattern). Reference recordings carry max-abs/RMS/spectral/DC/peak/crest/zero-crossing bands; event-stream compares (NOTE_ON/OFF, SLIDE, ACCENT, FLAM, PARAM, PATTERN_CHANGE) — what changed, not just "MD5 changed". SHA-256 of PCM payload for exact regression; error metrics for numerical regression.
- T4 Integration: patterns, songs, automation, MIDI, mods, export, reload, unknown-chunk preservation.
- T5 Soak: 30/60 min/overnight, max event density, tempo changes, pattern switching, mod ops, zero underruns.

Every test must be capable of failing on a meaningful implementation error (review §26). WBS TC-2.1.x–TC-2.16.x are retained as the gate list **after** re-expression in these terms (MD5-only gates become SHA-256 + metrics + event-stream gates). CI runs automatable TCs per commit.

## 17. Degraded / recovery behavior `[LOCKED]` (Terry rule: no silent failure)

| # | Failure | Behavior |
|---|---------|----------|
| 1 | Render underrun (worker miss / CPU over budget) | Audible glitch preferred over deadlock: missed bus renders silence for that buffer, underrun counter increments, T5 counts it. Never block, never tear down the graph. |
| 2 | Corrupt RBNG chunk (bad length, nesting, range) | Load aborts with a requester naming the chunk ID + byte offset; nothing half-loaded — song state rolls back to pre-load. Fuzz suite (500 mutated files) asserts this. |
| 3 | Missing sample / missing mod on song load | Warn "song was made with mod X — locate it?", offer file requester; on decline, load with the default mod and mark the song dirty. Never substitute silently. |
| 4 | MIDI flood (CAMD storm) | Bridge task sheds load at a documented cap (events per buffer, Appendix A P-19); excess events are counted and dropped oldest-first; playback timing never stalls on MIDI. |
| 5 | AHI buffer underrun / device stall | Backend reports `AUQA_XRUN_COUNT`; engine keeps the master clock running (musical time never rewinds); GUI shows a latched xrun indicator until cleared. |
| 6 | Memory pressure on low-end machines | All audio memory is preallocated at `AuStart` against declared buffer classes; allocation failure fails `AuStart` loudly with required-vs-available bytes. The render path has no fallback allocator. |
| 7 | Fractional tick error accumulation (long sessions) | Re-anchored at every tempo event by construction (§7); T2 24-hour monotonicity property test guards it. Any measured drift > 0.5 sample per tempo change is a P0 bug. |
| 8 | USB-MIDI hot-unplug mid-playback | No crash, no hang: bridge task times out the device, pending MIDI events for it are dropped and counted, transport continues (TC-2.8.4). |

## 18. Milestones, risks, build order, DoD

- W1 staged (§3); W2 M2.1 (specs incl. numeric interaction values above + evidence ledger seed + frozen RIEvent + single-thread ownership + asset pipeline + bench procedure — all five prior OPEN gates now closed in this spec) → M2.2 (2.1+2.2+2.7, 303 end-to-end + blind A/B, slide-tension verdict recorded) → M2.3 (2.3+2.4+2.9 partial) → M2.4 (2.5+2.6+2.9–2.11) → M2.5 (2.12–2.14) → M2.6 (2.8+2.15+2.16 beta). Timeline/risks per v1 inputs, with legal risk reworded per §1. Milestone names denote gate sets only — parity is declared exclusively via §14 + ledger.
- Strict build gates `[LOCKED]`: no 808 DSP work starts until the 303 voice + event system + single-thread render path has passing T1–T3 fixtures; no GUI work starts until the offline renderer produces golden 303 + 808 files. Enforced in the WBS before M2.3/M2.4 open.
- W1.1 report gate `[LOCKED]`: the benchmark report (named machine, compiler flags, measured %) is written into this document's §10 before any CPU budget number is treated as real. Until then all budgets are Appendix A hypotheses.
- Evidence ledger at `docs/evidence/{303,808,909,pcf,sequencer,gui,formats}/` is mandatory: claim, source, evidence class, confidence, method, fixture, status — and no claim moves hypothesis→normative without a ledger row. The directory ships with this spec.
- Definition of Done (W2 Classic): every advertised feature exists; every compatibility claim has evidence; mandatory tests pass; live/offline share one engine; no realtime allocation/blocking; RBNG/RBNM round-trips pass; sample provenance complete; fixtures reproducible; long playback zero underruns; open questions explicitly documented.

## 19. What left v1 normative core (deleted/reduced per review §31)

Blanket "ReBirth-compatible" as fact; legal conclusions; cross-platform "bit-identical"; ops/sample counts; unverified PCF table as fact; finished-ABI implication; W3 detail in Classic contract; unmeasured "feel" requirements; "14 voices" wording; unevidenced constants as truth.

## 20. Adopted review decisions (2026-09-24 improvement review)

Source: `docs/2026-09-24-improvement-opportunities.md` §11. Each row is normative from this spec version on; execution order is §12 of the review (tracked in `docs/2026-09-24-improvement-todo.md`).

| ID | Decision | Status |
|----|----------|--------|
| D-a | ReBirth 2.0.1 Owner's Manual added as E1 source in the Prior-Art Register | Adopted; register entry to be added with the PCF-pattern work (§12 item 8) |
| D-b | 808 model = 11 slots / 16 sounds + 5 switches, MA included | Adopted; recorded in §2.3 item 6, supersedes the item-1 lock |
| D-c | 808 = real-time synthesis now + mod sample override; offline-render fallback stays the §17 degraded profile | Adopted; RBNM must cover 808 sounds (§12 items 5, 8) |
| D-d | PCF = envelope model, LP/BP only, patterns from manual Appendix D (E1); HP moves to Power Mode | Adopted (§12 item 8) |
| D-e | Tempo range 20–500 BPM everywhere (delay/PCF buffers sized for 20 BPM) | Adopted (§12 items 7, 8) |
| D-f | Stereo engine, stereo export, stereo AHI output | Adopted (§12 item 3) |
| D-g | Solo is NOT a ReBirth feature: kept as a marked UX extension, never persisted in RBNG | Adopted (audit Phase 11 solo-vs-four gate stays green) |
| D-h | Gate length = E0 fraction now, measured from ReBirth later | Adopted (§12 item 4) |
| D-i | `.rbs` import = new OPEN-10 row, legal review first | Adopted above |
| D-j | Kernels are total over all finite floats; documented domains are precision statements, not safety boundaries | Adopted; enforced by T2 property tests from §12 item 1 on |
| D-k | Device architecture (owner requirement 2026-09-24) | Extensible **device rack**: class + instance model, `(instance, control)` addressing, per-instance mixer channels, user-selectable active devices applied via snapshot swap. Classic = default 4-device rack preset, bit-identical to the fixed engine. Other racks = Power Mode (review §5.6). Engine skeleton (§12.3 section mask + reserved bits) is compatible; rack iteration lands with §12.7/RBNG v2 |
| D-l | Device loading | Compile-time class registry now (bounded instance count, static allocation). Loadable device libraries only after the OPEN-07 ABI freeze |

## Appendix A — Pending measurement ledger (the ONLY home for unverified numbers)

Every row: ID, value, status, locked-by gate. Nothing here is normative until its gate passes.

| ID | Parameter | Current value | Status | Locked by |
|----|-----------|---------------|--------|-----------|
| P-01 | 303 amp τ | 350 ms | HYPOTHESIS | M2.2 step-response TC-2.2.2 |
| P-02 | 303 accent τ / VCA clamp | 60 ms / 1.2 | HYPOTHESIS | M2.2 step-response TC-2.2.2 |
| P-03 | 303 slide τ | 40 vs 60 ms | OPEN-01 | M2.2 blind A/B verdict |
| P-04 | Ladder resonance range | 0..~3.8 coeff | HYPOTHESIS | M2.2 filter TC-2.2.1 |
| P-05 | 909 flam timing / amp | 35 ms nominal, 30–40 range, ×0.75 | HYPOTHESIS | TC-2.4.2 loopback |
| P-06 | 303 decay-knob τ range | 80 ms..4 s; Open303-lineage 200–2000 ms exp | HYPOTHESIS | M2.2 A/B |
| P-07 | 808 kick sweep/decay | f_start≈170 Hz·tune ±7 st → 48 Hz; τ_pitch 22 ms; τ_amp 0.18–2.8 s; 35 Hz floor | HYPOTHESIS | TC-2.3.1/2.3.2 |
| P-08 | 808 snare partials/tone | 185+330 Hz (tolerance: 173.3/336.0 also pass); tone BP 1.4↔2.3 kHz | HYPOTHESIS | TC-2.3.1 FFT |
| P-09 | 808 toms/congas freqs | LT 75→50, MT 115→75, HT 155→100; LC 200, MC 250, HC 310 Hz | HYPOTHESIS | TC-2.3.1 |
| P-10 | 808 hats/cymbal/cowbell | 6-osc ratios, 7 kHz HP, CH τ35 ms, OH 0.2–1.2 s, CY τ1.6 s, CB 540+800 Hz | HYPOTHESIS | TC-2.3.1 FFT |
| P-11 | 808 clap bursts/tail | 3×(9 ms-spaced 2 ms bursts @1.1 kHz Q2.5) + tail τ120 ms + separate ~100 ms tail param | HYPOTHESIS | TC-2.3.3 envelope |
| P-12 | 808 accent mapping | binary (verify three-state off→weak→strong per E3) | OPEN verify | M2.1 interaction check |
| P-13 | 909 layer crossfade width | ~8 knob positions triangular | HYPOTHESIS | TC-2.4.1 RMS continuity |
| P-14 | 909 accent gains | acc1 ×1.15 + shelf; crash/ride no-op quirk | HYPOTHESIS | TC-2.4.2/2.4.3 |
| P-15 | PCF response law | `fc = base·2^((v−64)/64·amt)`, amt ±4 oct, Q 0.7..8 | HYPOTHESIS | TC-2.5.2 |
| P-16 | Meter ballistics | ~20 dB/s peak-hold | HYPOTHESIS | TC-2.6.3 sign-off |
| P-17 | Fader law | v2.0 feel curve, 9 anchor points | HYPOTHESIS | TC-2.6.1 |
| P-18 | Knob/fader travel | 150 px / 100 px full, fine ×0.1 | E0 LOCKED design | TC-2.9.2 (design, not measurement) |
| P-19 | MIDI flood cap | events-per-buffer shed cap | OPEN value | TC-2.8 soak (§17 failure 4) |
| P-20 | PPQ | 96 | HYPOTHESIS | Scheduler freeze review |
| P-21 | Render task priority / device buffer floor | task pri 10 (above GUI, below input.device); device buffer floor 128 clean, 64 marginal (1–2 xruns/~10k bufs); default 1024 until owner listens at 256/128 (recommended 256) | OPEN-09 | Dell E6320 2026-09-26: 0 xruns @1024/512/256/128 scripted takes, 5-min soak 59038 bufs 0 xruns @256; evidence docs/evidence/audio/live-render-task.md |

PCF pattern contents (54×16) are NOT rows here — each pattern gets its own ledger row under `docs/evidence/pcf/` per §12.

## Appendix B — 303 filter candidate (E0, UNVALIDATED — do not implement as fact)

Candidate recurrence, per-sample, in this exact update order:

```
x  = tanh(DRIVE * (in - k * s2))   // input saturation + resonance feedback tap
s0 += g * (x        - tanh(s0))
s1 += g * (tanh(s0) - tanh(s1))
s2 += g * (tanh(s1) - tanh(s2))
out = s2
```

with `g = tan(π·fc/fs)` (ZDF form, 2× oversampled in the candidate), `k` = resonance feedback 0..~3.8, `DRIVE` = input drive scaling (ledger row to be added at M2.2), feedback-loop HPF 150 Hz on the `k·s2` tap, post-chain HP 44.486 Hz + HP 24.167 Hz + allpass 14.008 Hz + notch 7.5164 Hz/BW 4.7. State init: all zero; denormals flushed per-block with a −140 dBFS dither guard (candidate detail, verify in T1); parameters interpolated per-sample, never per-block stepped. Stability limit: clamp `fc < fs/6` (Chamberlin bound for the PCF sibling; ladder candidate re-verified in T1 sweep). This appendix becomes normative ONLY after TC-2.2.x pass.

## Appendix C — 808/909 equation skeletons (E0 candidates, one per voice)

Each voice implements this skeleton with its row's blocks filled; empty blocks are explicitly N/A, never silently skipped:

```
TRIGGER:  phase=0; env_pitch=1; env_amp=1  (plus voice click where listed)
PER SAMPLE:
  f     = PITCH(f_start(tune), f_end, env_pitch)   // voice curve, Appendix A row
  osc   = OSCILLATOR(phase, f)                      // sine/tri/square-cluster
  nz    = NOISE() → FILTER(chain)                   // voices with noise paths
  y     = MIX(osc, nz, env) → SATURATE(point)       // saturation point named per voice
  out   = y · env_amp · EXCITE(accent)              // accent pre-envelope (E0 model)
VOICE ROWS (blocks filled per voice; full constants = Appendix A P-07–P-11):
  BD: sine + 6 ms attack bump + slow sweep + HP click; SD: 2 partials + BP noise + Tone/Snappy;
  LT/MT/HT + LC/MC/HC: sweep family at row freqs; RS: 5 ms BP pulse + click; CL: RS-family row;
  CP: 3+1 bursts + separate tail; CH/OH: 6-square cluster + 7 kHz HP (+bleed for OH);
  CY: cluster at low base + long τ; CB: 540+800 Hz squares + 800 Hz BP.
909 layer voice: sample = Σ_layers lerp(data)·triangular_weight(tune); pos += rate·pitch_mult/sr;
  pitch_mult, layer_weight, and accent/flam paths are separate definitions (P-13–P-15).
```

## Appendix D — Interface sketches (NOT ABI — hard gate before freeze)

```c
/* framework/ridevice.h — SKETCH */
struct RIDevice *RiDeviceRegister(const struct RIDeviceClass *class,
                                  const struct RIDSPOps *dsp,
                                  const struct RIPanelDesc *panel,
                                  struct Library *AudioBase);
void  RiDeviceEvent(struct RIDevice *dev, const struct RIEvent *ev);
void  RiDeviceSetParam(struct RIDevice *dev, uint32_t ctl_id, uint8_t value);
void  RiDeviceRender(struct RIDevice *dev, float **bus, uint32_t frames, float sr);

/* engine/fx/rifx.h — SKETCH */
struct RIFX *RiFXCreate(struct Library *AudioBase, uint32_t fx_type, struct TagItem *);
void RiFXSetParam(struct RIFX *, uint32_t id, uint8_t value);
void RiFXRender(struct RIFX *, float *in, float *out, uint32_t frames, float sr, float bpm);

/* engine/fx/pcf.h — SKETCH */
struct PCF { struct SVF svf; uint8_t pattern, mode; float base_fc, q, env_amt, accent_amt, beat_pos; };
void pcf_render(struct PCF *p, float *in, float *out, uint32_t n, float sample_rate);

/* engine/dsp/rb909.h — SKETCH (see §11 for the model) */
void rb909_render(struct RB909Voice *v, const struct RBStepEvent *ev,
                  float *out, uint32_t n, float sample_rate);
```

ABI freeze checklist (ALL required before any signature is treated as stable): struct size/versioning, ownership, lifetime, threading, reentrancy, alignment, error handling, ABI packing, capability negotiation, NULL semantics, sample format, channel ownership, vtable shape. No production code against the sketch until the freeze review signs every box. OPEN-07.
