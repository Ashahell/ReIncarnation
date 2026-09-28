# Levi filter NaN — device silence after ~6 ms (Dell 2026-09-28)

## Symptom (owner, E6320)
Play: a very short sound, then nothing. Stop + play again: nothing.

## Root cause (host-reproduced)
`levi_voice_render` SVF (`engine/dsp/levi.c:lp_step`, clean-room
2-pole Chamberlin): the tuning clamp admitted `f` up to 1.8, past the
topology's stability limit. At the init default cutoff (12 kHz @48 kHz,
`f = 1.414`) output grows geometrically from trigger: inf at sample
~296, NaN at ~297. NaN latches in `lp1/lp2` forever (release heals it,
nothing else does), poisons the mix bus, AHI goes silent. Stop/play
cannot recover: state persists. Matches the symptom exactly.

Why the suite missed it: `energy()` counts inf/NaN as nonzero, so
t103/t104 passed with a poisoned voice; t95 rendered 512 samples with
a 2-device mask (no Levi bit).

## Fix
- `f` clamped to `<= 1.0` (sr-independent: higher rates only lower `f`).
- Latch guard in `levi_voice_render`: non-finite output flushes the
  filter states and returns 0 (self-heal, never mute the mix).
- No RIAPP change needed: with stable defaults, knob-apply ordering
  no longer matters.

## Proof
- t103: 1 s defaults block finite + bounded ±8; 72-combo UI-extremes
  grid (cutoff/reso/mode/ratio incl. max) finite; clamp-reverted
  mutant FAILs.
- t95: full 5-device demo render (4096 samples) finite ±8; silent-lane
  mutant FAILs.
- Host demo render: 44,326/48,000 NaN before, 0 after, mix fully
  nonzero.
- Audit 0/0. Device re-proof: pending (redeploy + owner play).
