# CL — 808 clave (measured E0 candidate, Task 8 gate G8)

**Date:** 2026-09-20. **Status:** HYPOTHESIS (locks at TC-2.3.1/2.3.2).
**Spec:** §10, Appendix C skeleton (RS-family row).

| Parameter | Value | Evidence |
|-----------|-------|----------|
| Pulse | 1100 Hz sine, short BP pulse | Appendix C CL/RS-family row (E0) |
| BP | HP 500 Hz + LP 2500 Hz cascade | E0 |
| Click | 1 ms decaying transient at trigger | Appendix C "plus click" |
| τ_amp | 0.04 s default / 0.1 s max | E0 |

Appendix C blocks: SOURCE = sine + click; OSCILLATOR = sine;
PITCH = fixed 1100 Hz; AMPLITUDE ENVELOPE = exp(−t/τa); NOISE = N/A;
FILTER = HP→LP band; DISTORTION = tanh (unity drive);
MIX = filtered osc + click; OUTPUT = y·env_amp.

**ACCENT (excitation pre-envelope):** EXCITE scales osc + click at the
source (×1.0 / ×1.5). Measured +3.45 dB RMS (t1_808, want +3.52±0.5).
P-12 three-state OPEN (binary hold).
