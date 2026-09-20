# CY — 808 cymbal (measured E0 candidate, Task 8 gate G8)

**Date:** 2026-09-20. **Status:** HYPOTHESIS (locks at TC-2.3.1 FFT).
**Spec:** §10, Appendix A P-10, Appendix C skeleton.

| Parameter | Value | Evidence |
|-----------|-------|----------|
| Cluster | 6 squares, LOW base 250 Hz × ratios 1.00/1.30/1.62/1.93/2.27/2.63 | P-10 low-base cluster |
| HP | 5 kHz one-pole (E0; hats use 7 kHz) | executor choice |
| τ_amp | 1.6 s (`RI_808_CY_TAU`) / 2.5 s max | P-10 CY τ1.6 s |

Appendix C blocks: SOURCE = six-square cluster; OSCILLATOR = square
cluster; PITCH = fixed low base; AMPLITUDE ENVELOPE = exp(−t/τa);
NOISE = N/A; FILTER = 5 kHz HP; DISTORTION = tanh (drive 1.2);
MIX = cluster sum; OUTPUT = y·env_amp. Oversampling/warp-correction
test for the cluster chain: deferred to M2.3 (explicit).

**ACCENT (excitation pre-envelope):** EXCITE scales the cluster sum at
the source (×1.0 / ×1.5). Measured +3.50 dB RMS (t1_808, want +3.52±0.5).
P-12 three-state OPEN (binary hold).
