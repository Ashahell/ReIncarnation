# OH — 909 open hat (layered sampler, Task 9 gate G9)

**Date:** 2026-09-20 (locked 2026-09-23). **Status:** LOCKED (TC-2.4.1/2.4.3/2.4.4/2.4.5).
**Spec:** §11 (shared-hat-ROM steal rule), Appendix A P-13/P-14.

| Parameter | Value | Evidence |
|-----------|-------|----------|
| Layers | 2 (OH-A 0–63 / OH-B 64–127) | pack spans |
| Crossfade | triangular, 8-position feather, equal-power norm | `t1_909` §1 green (shared mixer path) |
| Accent | acc1 ×1.15 + shelf; acc2 = acc1 (not flam-capable) | `t1_909` §3 (CH-measured, same code path); `t24_909accent` green (CH-measured, same path) |
| Steal rule | OH wins ties; OH trigger always steals CH; CH trigger cuts OH only past the 50 ms same-step window (§12.6b, manual p. 35; was symmetric kill) | `t1_909` §6 + `t45_909_decouple` green |
| Retrigger/swap | monophonic / idle-only | `t1_909` §4/§5 |

Appendix C blocks: same skeleton as BD (see `bd.md`); 6-square
cluster + 7 kHz HP baked into layers (τ 0.6 s).

## Provenance manifest (per layer; full rows in `reference/packs/classic-01/`)

| Layer | Source | Date/Equip | License/Holder | Processing | Rate/Depth | Norm | Loop | Map |
|-------|--------|------------|----------------|------------|------------|------|------|-----|
| OH-A | synth (6-square cluster base 400 Hz × 1.00/1.30/1.62/1.93/2.27/2.63, HP 7 kHz, τ0.6 s, seed 0x9091+3) | 2026-09-20/host-python3 | CC0/RI | OH-A | 44100/16 | −3 dBFS | none | oh:0-63 |
| OH-B | synth (6-square cluster base 520 Hz × same ratios, HP 7 kHz, τ0.6 s, seed 0x9091+3) | 2026-09-20/host-python3 | CC0/RI | OH-B | 44100/16 | −3 dBFS | none | oh:64-127 |

Masters: 44100/24 WAV, peaks −3 dBFS, no dither; working payloads
44100/16 in `pack.rbnm` SMPL. Clean-room (entry 16, E5): squares via
sign(sine), noise-free, 64-sample fade-in (d[0]=0).
