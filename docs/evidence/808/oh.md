# OH — 808 open hat (measured E0 candidate, Task 8 gate G8)

**Date:** 2026-09-20 (cluster locked 2026-09-23). **Status:** LOCKED (TC-2.3.1 FFT).
**Spec:** §10, Appendix A P-10, Appendix C skeleton.

| Parameter | Value | Evidence |
|-----------|-------|----------|
| Cluster | 6 squares, base 1000 Hz × ratios 0.83/1.48/2.26/2.92/3.94/5.31 (830/1480/2260/2920/3940/5310 Hz) | classic TR-808 set; WBS ratios + deep-dive pt 3 |
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

**Re-baseline 2026-09-24 (§12.1 kernel totality):** golden re-rendered deliberately.
Bleed-term exp is now exact 0 past t≈1.1 s (old ±32-clamp garbage put audible
hash into the tail); envelope tail decays to exact 0. First diff at ≈1.13 s
of the 2.0 s golden, tail-only. Voice deactivates on its own past
−100 dBFS (t35).
