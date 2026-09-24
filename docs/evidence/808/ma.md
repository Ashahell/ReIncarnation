# 808 maracas (MA) — per-voice ledger row (§12.5a, §4.2 action 1)

**Status:** E0 synthesis model (new sound; no ReBirth capture yet).

Appendix C blocks: SOURCE = deterministic white noise (xorshift32, fixed
seed at trigger: D1) + 1 ms click transient; OSCILLATOR = N/A (unpitched);
PITCH = N/A (pitch query returns the 35 Hz floor); PITCH ENVELOPE = N/A;
AMPLITUDE ENVELOPE = exp(−t/40 ms) + click exp(−t/0.4 ms); NOISE = the source;
FILTER = one-pole HP 6 kHz; DISTORTION = none (linear); MIX = HP noise;
OUTPUT = y·env_amp with EXCITE(accent) at the source.

**ACCENT (excitation pre-envelope):** EXCITE scales noise + click amplitudes
at the source (×1.0 / ×(1+amt)). Measured +3.52 dB RMS (t41, want +3.52±0.5).
P-12 three-state OPEN (binary hold).

**Slot:** CP/MA switched slot (default CP; trigger selects). Voice rest:
envelope-bound deactivation like the RS family (t35 covers all 16 sounds).

**Re-baseline 2026-09-24 (§12.5a):** new golden (no predecessor). First of the
16-sound set; storm renders 11 slots.
