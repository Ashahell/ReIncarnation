# LT — 909 low tom (layered sampler, §12.6a)

**Date:** 2026-09-25. **Status:** E0 (§12.6a starting point).
**Spec:** §11, Appendix A P-13, Appendix C 909 skeleton.

| Parameter | Value | Evidence |
|-----------|-------|----------|
| Layers | 2 detuned (a/b ×1.12), swept VCO 150→100 Hz (τ 30 ms), τ 0.30 s | `t44_909_newvoices` smoke; golden below |
| Crossfade | triangular, 8-position feather, equal-power norm (shared path) | `t24_909xfade` (same code path) |
| Tune clock | 2^((tune−64)/48) | shared path |
| Accent | acc1 ×1.15 + shelf | shared path |
| Level/Decay knobs | per-voice trim + extra tau (§12.6a) | `t44_909_newvoices` |

## Provenance manifest (default baked layers)

| Layer | Source | Date/Equip | License/Holder | Processing | Rate/Depth | Norm | Loop | Map |
|-------|--------|------------|----------------|------------|------------|------|------|-----|
| LT-A | synth (swept 150→100 Hz sine τ0.30 s, deterministic phase) | 2026-09-25/host-render | CC0/RI | LT-A | 48000/16 | peak-guard 0.7 | none | lt:0-63 |
| LT-B | synth (swept 168→112 Hz sine τ0.30 s, deterministic phase) | 2026-09-25/host-render | CC0/RI | LT-B | 48000/16 | peak-guard 0.7 | none | lt:64-127 |

Golden: `tests/golden/909/lt.wav` (+`.sha256`), 1.0 s default bake.
