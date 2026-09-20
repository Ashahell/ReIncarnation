# SD — 909 snare (layered sampler, Task 9 gate G9)

**Date:** 2026-09-20. **Status:** HYPOTHESIS (locks at TC-2.4.1/2.4.2).
**Spec:** §11, Appendix A P-13/P-14, Appendix C 909 skeleton.

| Parameter | Value | Evidence |
|-----------|-------|----------|
| Layers | 3 (SD-LO 0–42 / SD-MID 43–84 / SD-HI 85–127) | pack spans |
| Crossfade | triangular, 8-position feather, equal-power norm | `t1_909` §1 green (shared mixer path) |
| Tune clock | 2^((tune−64)/48) | `t1_909` §0 |
| Accent | acc1 ×1.15 + shelf; flam-capable (acc2 = second hit) | `t1_909` §2/§3 (BD-measured, same code path) |
| Retrigger | monophonic | `t1_909` §4 |
| Swap | idle-only | `t1_909` §5 |

Appendix C blocks: same skeleton as BD (see `bd.md`); partials +
noise baked into layers.

## Provenance manifest (per layer; full rows in `reference/packs/classic-01/`)

| Layer | Source | Date/Equip | License/Holder | Processing | Rate/Depth | Norm | Loop | Map |
|-------|--------|------------|----------------|------------|------------|------|------|-----|
| SD-LO | synth (190·0.92+340·0.92 Hz partials τ0.22 s + HP1200 noise, seed 0x9091+1) | 2026-09-20/host-python3 | CC0/RI | SD-LO | 44100/16 | −3 dBFS | none | sd:0-42 |
| SD-MID | synth (190+340 Hz partials τ0.18 s + HP1200 noise, seed 0x9091+1) | 2026-09-20/host-python3 | CC0/RI | SD-MID | 44100/16 | −3 dBFS | none | sd:43-84 |
| SD-HI | synth (190·1.08+340·1.08 Hz partials τ0.15 s + HP1200 noise, seed 0x9091+1) | 2026-09-20/host-python3 | CC0/RI | SD-HI | 44100/16 | −3 dBFS | none | sd:85-127 |

Masters: 44100/24 WAV, peaks −3 dBFS, no dither; working payloads
44100/16 in `pack.rbnm` SMPL. Clean-room (entry 16, E5): sine/noise
recipes only, d[0]=0 (noise beds carry a 64-sample fade-in).
