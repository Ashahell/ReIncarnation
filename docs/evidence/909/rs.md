# RS — 909 rimshot (layered sampler, §12.6a)

**Date:** 2026-09-25. **Status:** E0 (§12.6a starting point).
**Spec:** §11, Appendix A P-13, Appendix C 909 skeleton.

| Parameter | Value | Evidence |
|-----------|-------|----------|
| Layers | 2 detuned (a/b ×1.12), 1800 Hz blip + click, τ 0.030 s | `t44_909_newvoices` smoke; golden below |
| Crossfade | triangular, 8-position feather, equal-power norm (shared path) | `t24_909xfade` (same code path) |
| Tune clock | 2^((tune−64)/48) | shared path |
| Accent | acc1 ×1.15 + shelf | shared path |
| Level/Decay knobs | per-voice trim + extra tau (§12.6a) | `t44_909_newvoices` |

## Provenance manifest (default baked layers)

| Layer | Source | Date/Equip | License/Holder | Processing | Rate/Depth | Norm | Loop | Map |
|-------|--------|------------|----------------|------------|------------|------|------|-----|
| RS-A | synth (1800 Hz sine τ0.030 s + click, deterministic phase) | 2026-09-25/host-render | CC0/RI | RS-A | 48000/16 | peak-guard 0.7 | none | rs:0-63 |
| RS-B | synth (2016 Hz sine τ0.030 s + click, deterministic phase) | 2026-09-25/host-render | CC0/RI | RS-B | 48000/16 | peak-guard 0.7 | none | rs:64-127 |

Golden: `tests/golden/909/rs.wav` (+`.sha256`), 0.3 s default bake.
