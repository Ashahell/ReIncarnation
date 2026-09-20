# P-05 — 909 flam timing default (measured, Task 7 gate G7)

**Date:** 2026-09-20. **Status:** HYPOTHESIS (locks at TC-2.4.2 loopback).

| Parameter | Value | Evidence |
|-----------|-------|----------|
| Nominal delay | `flam_ms = 35.0` ms (`RI_FLAM_MS_DEFAULT`, `engine/seq/sched.h`) | Spec Appendix A P-05 |
| Measured @ 48 kHz | 1680 samples, exact | `tests/unit/t1_sched.c` flam asserts |
| Tolerance | ±48 samples (±1 ms) | Task-7 brief gate |
| Range | 30–40 ms (1440–1920 samples @48k) | Spec P-05; 30.0 ms param case asserts 1440 exact |
| Amplitude ×0.75 | NOT measured here | Task 9 (909 sampler) |

**How measured:** the scheduler computes
`flam_samples = round(flam_ms * sr / 1000)` in the sample domain and emits
`RI_EV_FLAM` at `note_sample + flam_samples` with `value = flam_samples`
(≤ 65535 per §8) and the second-hit flag. The unit test pins the default
(35.0 ms → offset exactly 1680, value 1680) and the parameter path
(30.0 ms → 1440), both inside the ±48 window around nominal. The event
golden `tests/golden/songs/sched-check.events` carries two live 1680-sample
second hits (lines `9394 6 ... 1680 16` and `32537 6 ... 1680 16`), re-derived
by `tools/render` and diffed byte-exact by `tools/compare` in audit Phase 7.

**Scope note:** this row covers scheduler timing only. Voice-side flam
amplitude, per-voice 30–40 ms table, and the 909 flam storm stay open for
Task 9; loopback lock for TC-2.4.2.
