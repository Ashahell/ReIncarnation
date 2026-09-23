# CH — 808 closed hat (measured E0 candidate, Task 8 gate G8)

**Date:** 2026-09-20 (cluster locked 2026-09-23). **Status:** LOCKED (TC-2.3.1 FFT).
**Spec:** §10, Appendix A P-10, Appendix C skeleton.

| Parameter | Value | Evidence |
|-----------|-------|----------|
| Cluster | 6 squares, base 1000 Hz × ratios 0.83/1.48/2.26/2.92/3.94/5.31 (830/1480/2260/2920/3940/5310 Hz) | classic TR-808 set; WBS ratios + deep-dive pt 3 |
| HP | 7 kHz one-pole (`RI_808_HP_METAL`) | P-10 7 kHz HP |
| τ_amp | 35 ms (`RI_808_CH_TAU`) / 0.1 s max | P-10 CH τ35 ms |
| FFT gate | Goertzel peak at each of the 6 contract partials within ±0.5% | `t23_808hat` green (all 6, CH + OH) |

Appendix C blocks: SOURCE = six-square cluster; OSCILLATOR =
square cluster; PITCH = fixed base (partials = base × ratio);
AMPLITUDE ENVELOPE = exp(−t/τa); NOISE = N/A; FILTER = 7 kHz HP;
DISTORTION = tanh (drive 1.2 on HP sum, peak |x| ≤ 0.36);
MIX = cluster sum; OUTPUT = y·env_amp.

**ACCENT (excitation pre-envelope):** EXCITE scales the cluster sum at
the source (×1.0 / ×1.5). Measured +3.51 dB RMS (t1_808, want +3.52±0.5).
P-12 three-state OPEN (binary hold).
