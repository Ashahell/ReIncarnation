# BD — 909 kick (layered sampler, Task 9 gate G9)

**Date:** 2026-09-20. **Status:** HYPOTHESIS (locks at TC-2.4.1/2.4.2).
**Spec:** §11, Appendix A P-13/P-14, Appendix C 909 skeleton.

| Parameter | Value | Evidence |
|-----------|-------|----------|
| Layers | 3 (BD-LOW 0–42 / BD-MID 43–84 / BD-HI 85–127) | pack spans |
| Crossfade | triangular, 8-position feather (`RI_909_XFADE_HALF`), equal-power norm | `t1_909` §1: worst adjacent step 0.0128 dB (band 1.0) |
| Tune clock | 2^((tune−64)/48) (`rb909_pitch_mult`) | `t1_909` §0 spot-checked |
| Accent | acc1 ×1.15 + shelf; flam-capable (acc2 = second hit) | `t1_909` §2: ratio 1.1503 (band 1.124–1.178); §3 onset 1682/1680, ratio 0.7605 |
| Flam | +35 ms (1680 smp @48k) ×0.75 second hit | `t1_909` §3 green |
| Retrigger | monophonic: trigger resets both playheads | `t1_909` §4 bit-exact |
| Swap | idle-only (`RI_909_BUSY` while active) | `t1_909` §5 green |

Appendix C blocks: TRIGGER = pos/pos2 reset, tune/accent stored;
PER SAMPLE = sample = Σ lerp(data)·triangular_weight(tune) (equal-power
norm); pos += rate·pitch_mult/sr; accent = VCA scale (P-14); flam path
separate (P-05). NOISE/FILTER = baked into layers, N/A voice-side.

## Provenance manifest (per layer; full rows in `reference/packs/classic-01/`)

| Layer | Source | Date/Equip | License/Holder | Processing | Rate/Depth | Norm | Loop | Map |
|-------|--------|------------|----------------|------------|------------|------|------|-----|
| BD-LOW | synth (sine 50 Hz τ0.45 s + 2.5 kHz click, seed 0x9091+0) | 2026-09-20/host-python3 | CC0/RI | BD-LOW | 44100/16 | −3 dBFS | none | bd:0-42 |
| BD-MID | synth (sine 60 Hz τ0.40 s + 2.5 kHz click, seed 0x9091+0) | 2026-09-20/host-python3 | CC0/RI | BD-MID | 44100/16 | −3 dBFS | none | bd:43-84 |
| BD-HI | synth (sine 72 Hz τ0.35 s + 2.5 kHz click, seed 0x9091+0) | 2026-09-20/host-python3 | CC0/RI | BD-HI | 44100/16 | −3 dBFS | none | bd:85-127 |

Masters: 44100/24 WAV, peaks −3 dBFS, no dither; working payloads
44100/16 in `pack.rbnm` SMPL. Clean-room (entry 16, E5): no ROM dumps,
no third-party samples — sine/noise recipes only, d[0]=0 start-at-zero.
