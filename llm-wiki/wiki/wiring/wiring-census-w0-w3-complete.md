# Wiring census W0-W3 complete — full audit passes (AUDIT 0/0 PASS)

> Sources: ReIncarnation session, 2026-11-20
> Raw: [2026-11-20-wiring-census-w0-w3-complete-full-audit-pass.md](../../raw/articles/2026-11-20-wiring-census-w0-w3-complete-full-audit-pass.md)
> Updated: 2026-11-20

## Overview

The wiring census (W0-W3) is complete and the full `ri_audit.sh` passes (AUDIT 0/0 PASS). All 14 audit phases pass, all 30 host tests pass, and AROS cross-compilation succeeds.

## W0: Census test created

`t243_wiring_census` walks 510 registry rows across 21 sections, tests 11 doors (D1-D11):
- Registry rows: 510
- Door-control pairs tested: 2806
- Positive control: key=0 correctly refused (DEAD)
- Positive control: good row 0002 correctly OK

## W1: Classification complete

- 1,672 bugs total
- 839 by-design
- 38 needs-owner (LEVI Master Control encoders)

## W3: Gate implemented with allowlist

- Allowlist: 2,535 entries
- 0 violations (GATE PASSING)
- Allowlist loaded from `docs/evidence/wiring/2026-11-17-census/allowlist.tsv`

## t224 defect patterns verified

All 4 t224 defect patterns verified in census:
1. Pattern 1: 4 controls (On/Off) — NOFEEDBACK
2. Pattern 2: 5 controls (Meter) — NA
3. Pattern 3: 16 controls (Dist/PCF/Comp inserts) — NOFEEDBACK
4. Pattern 4: 9 controls (On/Off) — NOFEEDBACK

## Full audit result

```
AUDIT 0/0 PASS
```

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
- [Dell ABIv1 deployment](../hardware/dell-abiv1-deployment.md)
- [MIDI push fix](../midi/midi-push-fix-t232.md)