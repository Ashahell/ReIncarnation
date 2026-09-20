# CH — 808 closed hat (measured E0 candidate, Task 8 gate G8)

**Date:** 2026-09-20. **Status:** HYPOTHESIS (locks at TC-2.3.1 FFT).
**Spec:** §10, Appendix A P-10, Appendix C skeleton.

| Parameter | Value | Evidence |
|-----------|-------|----------|
| Cluster | 6 squares, base 400 Hz × ratios 1.00/1.30/1.62/1.93/2.27/2.63 | P-10 6-osc nominals |
| HP | 7 kHz one-pole (`RI_808_HP_METAL`) | P-10 7 kHz HP |
| τ_amp | 35 ms (`RI_808_CH_TAU`) / 0.1 s max | P-10 CH τ35 ms |
| FFT gate | Goertzel peak at each ratio within ±0.5% (above ±1% neighbours) | `t1_808` §2 green |

Appendix C blocks: SOURCE = six-square cluster; OSCILLATOR =
square cluster; PITCH = fixed base (partials = base × ratio);
AMPLITUDE ENVELOPE = exp(−t/τa); NOISE = N/A; FILTER = 7 kHz HP;
DISTORTION = tanh (drive 1.2 on HP sum, peak |x| ≤ 0.36);
MIX = cluster sum; OUTPUT = y·env_amp.

**ACCENT (excitation pre-envelope):** EXCITE scales the cluster sum at
the source (×1.0 / ×1.5). Measured +3.51 dB RMS (t1_808, want +3.52±0.5).
P-12 three-state OPEN (binary hold).
