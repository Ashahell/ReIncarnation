# P-17 — Mixer fader law (E0 design decision, Task 11 gate G11)

**Date:** 2026-09-20. **Status:** E0 design decision (locks at TC-2.6.1
measurement; the v2.0 feel curve stays HYPOTHESIS until then).

| Item | Value | Evidence |
|------|-------|----------|
| Law | `gain = (v/127)^2`, v = 0..127 knob | E0 this project (no reference lineage claimed) |
| Knobs sharing it | bus fader ×4, master, send ×4 (one law, three knob kinds) | `engine/mixer/mixer.c` `ri_fader_gain` |
| v = 0 | exactly 0 (silence, no denormal tail) | `tests/unit/t1_mixer.c` anchor asserts |
| Meter ballistics (P-16) | 20 dB/s peak-hold, ±10% | `tests/unit/t1_mixer.c` decay asserts (TC-2.6.3) |
| Mute/solo rule | no solo: audible iff !mute; any solo: audible iff solo && !mute | 16-combo truth table in `tests/unit/t1_mixer.c` |
| Toggle clicks | zipless by construction: applied gain slews full traverse in 64 samples (1.3 ms @48 kHz) | transient-bound asserts in `tests/unit/t1_mixer.c` |

**9 anchors** (measured by `t1_mixer`, ±0.5 dB gate; dB = 20·log10(gain)):

| v | gain | dB |
|---|------|----|
| 0 | 0.000000 | −inf (exact 0 asserted, not measured in dB) |
| 16 | 0.015872 | −35.99 |
| 32 | 0.063489 | −23.95 |
| 48 | 0.142848 | −16.90 |
| 64 | 0.253953 | −11.91 |
| 80 | 0.396801 | −8.03 |
| 96 | 0.571393 | −4.86 |
| 112 | 0.777730 | −2.18 |
| 127 | 1.000000 | 0.00 |

**Why exact formula, not a 9-anchor table:** `engine/dsp/params.c`
implements voice curves as tables because measurement TCs re-paste
anchors. The fader law is instead an exact closed form: linear
interpolation between square-law anchors would sit up to ~6 dB off the
square between anchors (e.g. v=8: table gives half the v=16 gain,
−41.99 dB, vs the true (8/127)² at −48.01 dB), so a table would need a
dense grid to honor its own formula. The formula is two multiplies,
bit-deterministic (D1), and the E0 record lives in the `params.c`
header pointing at `ri_fader_gain`. Voice VOLUME in `params.c` stays a
linear trim; the mixer owns loudness.

**Deferred with note (not silent):** pan (needs a stereo master — the
whole Classic path is mono, so no pan knob ships); channel inserts
(ride the generic RIFX wrapper, land with the GUI task). Sends are
post-fader into a mono send bus (E0).
