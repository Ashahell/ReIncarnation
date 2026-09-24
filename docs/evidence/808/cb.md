# CB — 808 cowbell (measured E0 candidate, Task 8 gate G8)

**Date:** 2026-09-20. **Status:** HYPOTHESIS (locks at TC-2.3.1 FFT).
**Spec:** §10, Appendix A P-10, Appendix C skeleton.

| Parameter | Value | Evidence |
|-----------|-------|----------|
| Oscillators | 540 + 800 Hz squares (`RI_808_CB_F1/F2`) | P-10 CB 540+800 Hz |
| BP | HP 600 Hz + LP 1100 Hz (≈800 Hz BP) | P-10 800 Hz BP |
| τ_amp | 0.2 s default / 0.5 s max | E0 |

Appendix C blocks: SOURCE = two squares; OSCILLATOR = square pair;
PITCH = fixed 540/800 Hz; AMPLITUDE ENVELOPE = exp(−t/τa);
NOISE = N/A; FILTER = 800 Hz band; DISTORTION = tanh (drive 1.5
on the band sum, peak |x| ≤ 0.23); MIX = square sum; OUTPUT = y·env_amp.

**ACCENT (excitation pre-envelope):** EXCITE scales the square sum at
the source (×1.0 / ×1.5). Measured +3.49 dB RMS (t1_808, want +3.52±0.5).
P-12 three-state OPEN (binary hold).

**Re-baseline 2026-09-25 (§12.5b metal):** golden re-rendered deliberately.
540 + 800 Hz pair kept (already the E1 pair); squares are PolyBLEP'd.
