# OH — 808 open hat (measured E0 candidate, Task 8 gate G8)

**Date:** 2026-09-20. **Status:** HYPOTHESIS (locks at TC-2.3.1 FFT).
**Spec:** §10, Appendix A P-10, Appendix C skeleton.

| Parameter | Value | Evidence |
|-----------|-------|----------|
| Cluster | 6 squares, base 400 Hz × ratios 1.00/1.30/1.62/1.93/2.27/2.63 | P-10 6-osc nominals |
| HP | 7 kHz one-pole (`RI_808_HP_METAL`) | P-10 7 kHz HP |
| τ_amp | 0.4 s default, range 0.2–1.2 s (max 1.2) | P-10 OH 0.2–1.2 s |
| Bleed | HP-noise transient exp(−t/35 ms) ×0.04 | Appendix C "+bleed for OH" (E0) |

Appendix C blocks: SOURCE = six-square cluster + bleed noise;
OSCILLATOR = square cluster; PITCH = fixed base; AMPLITUDE ENVELOPE =
exp(−t/τa); NOISE (bleed) → FILTER = 7 kHz HP; DISTORTION = tanh;
MIX = cluster + bleed; OUTPUT = y·env_amp + bleed·env_bleed.

**ACCENT (excitation pre-envelope):** EXCITE scales cluster + bleed at
the source (×1.0 / ×1.5). Measured +3.51 dB RMS (t1_808, want +3.52±0.5).
P-12 three-state OPEN (binary hold).
