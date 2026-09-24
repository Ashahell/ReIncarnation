# LT — 808 low tom (measured E0 candidate, Task 8 gate G8)

**Date:** 2026-09-20. **Status:** HYPOTHESIS (locks at TC-2.3.1).
**Spec:** §10, Appendix A P-09, Appendix C skeleton.

| Parameter | Value | Evidence |
|-----------|-------|----------|
| Sweep | 75 → ~50 Hz (end = 0.65 × start) | P-09 LT 75→50 |
| τ_pitch | 30 ms (E0) | executor choice, same family MT/HT |
| τ_amp | 0.4 s default / 1.0 s max | E0 |
| Floor | 35 Hz clamp | P-07 floor (global) |

Appendix C blocks: SOURCE = sine; OSCILLATOR = sine; PITCH =
`f0·(0.65 + 0.35·exp(−t/30 ms))`; AMPLITUDE ENVELOPE = exp(−t/τa);
NOISE = N/A; FILTER = N/A; DISTORTION = tanh (drive 0.24);
MIX = osc; OUTPUT = y·env_amp.

**ACCENT (excitation pre-envelope):** EXCITE scales osc at the source
(×1.0 / ×1.5). Measured +3.37 dB RMS (t1_808, want +3.52±0.5).
P-12 three-state OPEN (binary hold).

**Re-baseline 2026-09-24 (§12.1 kernel totality):** golden re-rendered deliberately.
Pitch-sweep exp is now exact past t≈22·30 ms (old ±32-clamp garbage put wrong,
audible pitch into the tail from ≈0.7 s); envelope tail decays to exact 0.
First diff at ≈0.98 s, tail-only. Voice deactivates on its own past
−100 dBFS (t35).
