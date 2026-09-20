# MC — 808 mid conga (measured E0 candidate, Task 8 gate G8)

**Date:** 2026-09-20. **Status:** HYPOTHESIS (locks at TC-2.3.1).
**Spec:** §10, Appendix A P-09, Appendix C skeleton.

| Parameter | Value | Evidence |
|-----------|-------|----------|
| Pitch | 250 Hz fixed (`RI_808_CONGA_MC_F`) | P-09 MC 250 |
| τ_amp | 0.3 s default / 0.8 s max | E0 |
| Floor | 35 Hz clamp | P-07 floor (global) |

Appendix C blocks: SOURCE = sine; OSCILLATOR = sine; PITCH = fixed;
AMPLITUDE ENVELOPE = exp(−t/τa); NOISE = N/A; FILTER = N/A;
DISTORTION = tanh (drive 0.24); MIX = osc; OUTPUT = y·env_amp.

**ACCENT (excitation pre-envelope):** EXCITE scales osc at the source
(×1.0 / ×1.5). Measured +3.37 dB RMS (t1_808, want +3.52±0.5).
P-12 three-state OPEN (binary hold).
