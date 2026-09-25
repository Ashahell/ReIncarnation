# CP — 909 clap (layered sampler, §12.6a)

**Date:** 2026-09-25. **Status:** E0 (§12.6a starting point; burst trains
deferred to measurement).
**Spec:** §11, Appendix A P-13, Appendix C 909 skeleton.

| Parameter | Value | Evidence |
|-----------|-------|----------|
| Layers | 2 detuned (a/b ×1.12), 1100 Hz + noise 0.5, τ 0.150 s | `t44_909_newvoices` smoke; golden below |
| Crossfade | triangular, 8-position feather, equal-power norm (shared path) | `t24_909xfade` (same code path) |
| Tune clock | 2^((tune−64)/48) | shared path |
| Accent | acc1 ×1.15 + shelf | shared path |
| Level/Decay knobs | per-voice trim + extra tau (§12.6a) | `t44_909_newvoices` |

## Provenance manifest (default baked layers)

| Layer | Source | Date/Equip | License/Holder | Processing | Rate/Depth | Norm | Loop | Map |
|-------|--------|------------|----------------|------------|------------|------|------|-----|
| CP-A | synth (1100 Hz sine + hash noise 0.5 τ0.150 s, seed-fixed) | 2026-09-25/host-render | CC0/RI | CP-A | 48000/16 | peak-guard 0.7 | none | cp:0-63 |
| CP-B | synth (1232 Hz sine + hash noise 0.5 τ0.150 s, seed-fixed) | 2026-09-25/host-render | CC0/RI | CP-B | 48000/16 | peak-guard 0.7 | none | cp:64-127 |

Golden: `tests/golden/909/cp.wav` (+`.sha256`), 1.0 s default bake.
