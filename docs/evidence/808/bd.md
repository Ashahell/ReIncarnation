# BD — 808 kick (measured E0 candidate, Task 8 gate G8)

**Date:** 2026-09-20 (decay knob locked 2026-09-23). **Status:** LOCKED (TC-2.3.1/2.3.2).
**Spec:** §10, Appendix A P-07, Appendix C skeleton.

| Parameter | Value | Evidence |
|-----------|-------|----------|
| f_start | 170 Hz @ tune 0, ×2^(tune/12), tune ±7 st | P-07 nominal |
| f_end | 48 Hz | P-07 nominal |
| τ_pitch | 22 ms (`RI_808_BD_TAU_PITCH`) | P-07 nominal |
| τ_amp | 0.5 s default / 2.8 s max | P-07 range 0.18–2.8 s |
| Decay knob | 0.18 s @ knob 0 .. 2.8 s @ knob 127 via `RI_808_DECAY_TBL` (9 anchors, params.c 808 section); rendered 1/e crossing matches ±10% | `t23_808bd` green |
| Floor | 35 Hz (`RI_808_FLOOR_HZ`; `rb808_pitch_hz` clamps) | P-07 floor |
| Trajectory gate | ±5% at t = 10/25/50/100/200 ms | `t1_808` §1 green |

Appendix C blocks: SOURCE = decaying sine + ~6 ms attack transient
(2.5 kHz decaying blip, E0 stand-in for the attack frequency bump) + click
transient; OSCILLATOR = sine; PITCH = `f_end + (f_start − f_end)·exp(−t/τp)`;
PITCH ENVELOPE = exp(−t/22 ms); AMPLITUDE ENVELOPE = exp(−t/τa);
NOISE = N/A; FILTER = N/A (direct); DISTORTION = tanh, drive ≤ 0.24;
MIX = osc + click; OUTPUT = y·env_amp.
Slow leakage sweep: N/A (deferred to M2.3 measurement, explicit).

**ACCENT (excitation pre-envelope):** EXCITE scales osc + click amplitudes
at the source (×1.0 / ×1.5). Measured +3.45 dB RMS (t1_808, want +3.52±0.5).
P-12 three-state (off→weak→strong) OPEN: level 2 maps to ×1.5 (binary hold).

**Re-baseline 2026-09-24 (§12.1 kernel totality):** golden re-rendered deliberately.
Pitch-sweep exp is now exact past t≈22·τp (old ±32-clamp garbage put wrong,
audible pitch into the tail from ≈0.5 s); envelope tail decays to exact 0.
First diff at ≈0.75 s of the 1.5 s golden, tail-only. Voice deactivates on
its own past −100 dBFS (t35).
