# SD — 808 snare (measured E0 candidate, Task 8 gate G8)

**Date:** 2026-09-20. **Status:** HYPOTHESIS (locks at TC-2.3.1 FFT).
**Spec:** §10, Appendix A P-08, Appendix C skeleton.

| Parameter | Value | Evidence |
|-----------|-------|----------|
| Partials | 185 + 330 Hz (`RI_808_SD_F1/F2`) | P-08 nominal |
| Revision tolerance | 173.3/336.0 pair ALSO passes | Spec §10 (E3): both revisions accepted |
| Revision tolerance | 249.6/499.0 pair ALSO passes | Spec §10 (E3): both revisions accepted |
| Tone BP | HP 1.4 kHz + LP 2.3 kHz cascade on noise | P-08 tone 1.4↔2.3 kHz |
| Snappy decay | noise env exp(−t/90 ms) | E0 (Snappy knob deferred) |
| τ_amp (tone) | 0.25 s default / 0.5 s max | E0 |

Appendix C blocks: SOURCE = two sines + white LFSR noise; OSCILLATOR =
sine pair; PITCH = fixed (no sweep); AMPLITUDE ENVELOPE = exp(−t/τa)
(tone), exp(−t/90 ms) (noise); NOISE → FILTER = HP→LP band;
DISTORTION = tanh on the tone mix (drive ≤ 0.22); MIX = tone + noise;
OUTPUT = tone·env + noise·env_nz. Tone/Snappy knob: N/A (deferred).

**ACCENT (excitation pre-envelope):** EXCITE scales partials + noise at
the source (×1.0 / ×1.5). Measured +3.43 dB RMS (t1_808, want +3.52±0.5).
P-12 three-state OPEN (binary hold).
