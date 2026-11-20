# Full audit pipeline

> Sources: ReIncarnation session, 2026-11-20
> Raw: [2026-11-20-wiring-census-w0-w3-complete-full-audit-pass.md](../../raw/articles/2026-11-20-wiring-census-w0-w3-complete-full-audit-pass.md)
> Updated: 2026-11-20

## Overview

The full `ri_audit.sh` passes (AUDIT 0/0 PASS). All 14 audit phases pass, all 30 host tests pass, and AROS cross-compilation succeeds.

## Audit phases

All 14 phases pass:
- Phase 0: render-path hygiene, no platform transcendentals, evidence dirs, no ungated tests
- Phase 1: first-light goldens (G4)
- Phase 5: M1.1 probe hygiene (G5)
- Phase 6: W1 one-renderer proof (G6)
- Phase 7: sched shuffle/legato/flam (G7)
- Phase 8: 808 fifteen voices (G8)
- Phase 9: 909 sampler + clean pack + provenance (G9)
- Phase 10: PCF black-box route + FX trio (G10)
- Phase 11: mixer + RIDevice registry (G11)
- Phase 12: GUI logic + MCC shells + panels (G12)
- Phase 13: formats full + MIDI + automation + ARexx (G13)
- Phase 14: REL soak/docs/installer/beta-exit (G14)

## Key test passes

All 30 host tests pass:
- Wiring census: `t243_wiring_census`
- MIDI push: `t232_midi_push` (fixed)
- Panel wiring: `t116_panel_wiring`
- LEVI osc: `t129_levi_osc`
- FX/dist/comp: `t1_fx`, `t25_dist`, `t50_fx_dist_comp`, `t222_fx_exclusive`
- All GUI phases G1-G8: `t60_ctlreg` through `t76_zoom`

## AROS builds verified

- `ri_build_aros.sh` (stub + probes) ✓
- `ri_build_aros.sh sections` (RISECT + MIDISEND) ✓
- `ri_build_aros.sh riapp` (RIAPP mixed build) ✓
- `ri_build_v11.sh` (ABIv11 RIAPP/MIDISEND/MIDICLOCK) ✓

## See Also
- [Wiring census W0-W3 complete](../wiring/wiring-census-w0-w3-complete.md)
- [Dell ABIv1 deployment](../hardware/dell-abiv1-deployment.md)