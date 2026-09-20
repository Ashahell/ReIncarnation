# CP — 808 clap (measured E0 candidate, Task 8 gate G8)

**Date:** 2026-09-20. **Status:** HYPOTHESIS (locks at TC-2.3.3 envelope).
**Spec:** §10, Appendix A P-11, Appendix C skeleton.

| Parameter | Value | Evidence |
|-----------|-------|----------|
| Bursts | 3 × 2 ms gates at 0/9/18 ms + tail onset at 27 ms = 4 humps | P-11 3+1 bursts |
| Burst BP | HP 800 Hz + LP 1600 Hz (≈1.1 kHz Q2.5 region) | P-11 1.1 kHz Q2.5 |
| Tail τ | 120 ms (`RI_808_CLAP_TAIL_TAU`) | P-11 tail τ120 ms |
| Tail param | separate tail level (`tail` field, default 1.0) | P-11 separate ~100 ms tail param |
| Burst gate | exactly 4 peaks on rectified envelope (prominence-counted) | `t1_808` §3 green |

Appendix C blocks: SOURCE = white LFSR noise; OSCILLATOR = N/A;
PITCH = N/A (band centre 1.1 kHz); AMPLITUDE ENVELOPE = inherent in the
burst/tail function (env_amp = 1); NOISE → FILTER = HP→LP band;
DISTORTION = tanh (drive 0.30·(gate + tail)); MIX = gated noise;
OUTPUT = y. Repetition/width trims: N/A (deferred).

**ACCENT (excitation pre-envelope):** EXCITE scales the noise drive at
the source (×1.0 / ×1.5). Measured +3.49 dB RMS (t1_808, want +3.52±0.5).
P-12 three-state OPEN (binary hold).
