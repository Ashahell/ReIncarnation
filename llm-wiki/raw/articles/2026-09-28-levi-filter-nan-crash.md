# Levi filter NaN crash (Dell silence, 2026-09-28)

- Source: ReIncarnation session, 2026-09-28 (opencode lane, owner symptom report + host root-cause)
- Collected: 2026-09-28
- Published: 2026-09-28
- Fix commit: `bc9237c`
- Repo record: `docs/evidence/portability/levi-filter-nan.md`

## Symptom (owner, E6320, build `657a990`)

Play: a very short sound, then nothing. Stop + play again: nothing.

## Root cause (host-reproduced, deterministic)

`lp_step` (`engine/dsp/levi.c`, clean-room 2-pole Chamberlin SVF):
the tuning clamp admitted `f` up to 1.8, past the topology's stability
limit. At the init default cutoff (12000 Hz @ 48000 Hz, `f = 1.414`)
output grows geometrically from trigger: inf at sample ~296, NaN at
sample ~297 (voice-direct probe; full demo render 44,326/48,000 NaN on
both channels; 4-device mask renders 0 NaN). NaN latches in `lp1/lp2`
forever (only release heals it), poisons the mix bus, AHI goes silent.
Stop/play cannot recover: state persists. First ~6 ms stay finite —
exactly the reported symptom.

## Fix (`bc9237c`)

- `f` clamped to `<= 1.0` (sr-independent: higher rates only lower `f`).
- Latch guard in `levi_voice_render`: non-finite output (`!(out >
  -1e20f && out < 1e20f)`, catches NaN and inf) flushes filter states
  and returns 0 — self-heal, never mute the mix.
- No RIAPP change: stable defaults make knob-apply ordering moot.

## Proof

- t103: 1 s defaults block finite + bounded ±8; 72-combo UI-extremes
  grid (cutoff/reso/mode/ratio incl. max) finite — no amplitude bound
  there, max reso is meant to scream; clamp-reverted mutant FAILs.
- t95: full 5-device demo render (4096 samples) finite ±8;
  silent-lane mutant FAILs.
- Host demo render: 44,326/48,000 NaN before, 0 after, mix fully
  nonzero (first samples near-identical).
- Audit 0/0. Owner re-proof APPROVED on device.

## Lessons

- Never assert sound via nonzero-sample counts alone: inf/NaN pass.
  t103/t104 passed poisoned; t95's 512-sample 2-device render never
  touched the Levi bit.
- A filter tuning clamp is a stability claim: validate against the
  topology limit, not a round number (1.8 admitted, 1.0 holds).
- Poison latches: any DSP state that can go non-finite needs a
  self-heal path, or one bad block mutes the device forever.
