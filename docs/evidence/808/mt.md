# MT — 808 mid tom (measured E0 candidate, Task 8 gate G8)

**Date:** 2026-09-20. **Status:** HYPOTHESIS (locks at TC-2.3.1).
**Spec:** §10, Appendix A P-09, Appendix C skeleton.

| Parameter | Value | Evidence |
|-----------|-------|----------|
| Sweep | 115 → ~75 Hz (end = 0.65 × start) | P-09 MT 115→75 |
| τ_pitch | 30 ms (E0) | executor choice, same family LT/HT |
| τ_amp | 0.4 s default / 1.0 s max | E0 |
| Floor | 35 Hz clamp | P-07 floor (global) |

Appendix C blocks: SOURCE = sine; OSCILLATOR = sine; PITCH =
`f0·(0.65 + 0.35·exp(−t/30 ms))`; AMPLITUDE ENVELOPE = exp(−t/τa);
NOISE = N/A; FILTER = N/A; DISTORTION = tanh (drive 0.24);
MIX = osc; OUTPUT = y·env_amp.

**ACCENT (excitation pre-envelope):** EXCITE scales osc at the source
(×1.0 / ×1.5). Measured +3.37 dB RMS (t1_808, want +3.52±0.5).
P-12 three-state OPEN (binary hold).
