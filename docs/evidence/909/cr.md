# CR — 909 crash (layered sampler, Task 9 gate G9)

**Date:** 2026-09-20. **Status:** HYPOTHESIS (locks at TC-2.4.1/2.4.3).
**Spec:** §11 (crash/ride accent-no-op quirk + decay-shortens-with-Tune),
Appendix A P-13/P-14.

| Parameter | Value | Evidence |
|-----------|-------|----------|
| Layers | 2 (CR-A 0–63 / CR-B 64–127) | pack spans |
| Crossfade | triangular, 8-position feather, equal-power norm | `t1_909` §1 green (shared mixer path) |
| Accent | NO-OP quirk: gain pinned 1.0, shelf skipped | `t1_909` §2: acc1/accent0 bit-identical |
| Decay × Tune | extra exp envelope, τ = 1.2 s × 2^(−(tune−64)/48) | `t1_909` §0: monotonic shortening asserted |
| Retrigger/swap | monophonic / idle-only | `t1_909` §4/§5 |

Appendix C blocks: same skeleton as BD (see `bd.md`); low cluster +
long decay baked into layers; voice-side extra decay envelope only.

## Provenance manifest (per layer; full rows in `reference/packs/classic-01/`)

| Layer | Source | Date/Equip | License/Holder | Processing | Rate/Depth | Norm | Loop | Map |
|-------|--------|------------|----------------|------------|------------|------|------|-----|
| CR-A | synth (6-square cluster base 300 Hz × 1.00/1.30/1.62/1.93/2.27/2.63, HP 5 kHz, τ1.2 s, seed 0x9091+4) | 2026-09-20/host-python3 | CC0/RI | CR-A | 44100/16 | −3 dBFS | none | cr:0-63 |
| CR-B | synth (6-square cluster base 360 Hz × same ratios, HP 5 kHz, τ1.2 s, seed 0x9091+4) | 2026-09-20/host-python3 | CC0/RI | CR-B | 44100/16 | −3 dBFS | none | cr:64-127 |

Masters: 44100/24 WAV, peaks −3 dBFS, no dither; working payloads
44100/16 in `pack.rbnm` SMPL. Clean-room (entry 16, E5): squares via
sign(sine), noise-free, 64-sample fade-in (d[0]=0).
