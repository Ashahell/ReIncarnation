# FX meter closeout: the COMP GR bar gets its wire, and displaced meters go dark (2026-10-15)

- Source: ReIncarnation session (OpenCode lane), 2026-10-15. Work is **UNCOMMITTED** (7 files, states below); parent commit `860072f` (P-FX3).
- Evidence: `tests/unit/t219_fx_meters.c` (displacement + OFF sections), `tests/unit/t218_fx_enable.c` (displaced-COMP-GR section), `tests/unit/t50_fx_dist_comp.c` (wrapper release section), `scripts/ri_audit.sh` (COMP-GR shell-wire source gate), `docs/evidence/midi/ledger.md` (P-FX1, ear check, P-FX2, P-FX3 sections).
- Collected: 2026-10-15
- Published: 2026-10-15
- Spec: `docs/superpowers/specs/2026-09-20-reincarnation-spec.md` (§13 FX)
- Prior: [2026-09-25-gui-parity-sections-g1-g4.md](2026-09-25-gui-parity-sections-g1-g4.md) (whose "Dist exclusivity (owner decision)" open point the 2026-10-14 ruling closes — see Status note added there); `docs/evidence/midi/ledger.md` §§P-FX1/P-FX2/P-FX3 (the arc this closes)

## What the owner reported

> "in COMP, the horizontal led bar isn't active. PCF and Delay leds remain active/on when COMP is activated"

"leds" is meters, not lamps — settled vocabulary, not a guess: P-FX2's commit message records owner "the leds top right of the panel don't work" as "confirmed as the METERS, not the on/off lamps" (`b5400b6`). So both symptoms are meter symptoms: the COMP Level Reduction bar (the horizontal 9-seg `ri_art_gr_row`) is dark, and the PCF/Delay input meters stay lit after COMP displaces them. The on/off lamp model was verified correct first (t222 passes: lamps are rack-derived, displacement darkens them in state) and is not implicated.

## Symptom 1: the GR value arrived every frame and was discarded

`app/riapp.c:1776`, in `meter_round`'s FX block, ten lines below the working input-meter push:

```c
(void)fgr; /* COMP's level-reduction meter (RI_SFX_COMP_GR) stays
             * unwired: comp_gr arrives in dB and choosing the
             * dB->0..127 scale is a look decision, not a wire.
             * Recorded rather than invented. */
```

The engine publishes `comp_gr` (`ri_engine_comp_gr` → `live_meters_update` → `ri_core_fx_meters`, which t219 pins as `<= 0 dB`), the core reads it, and the shell threw it away. The deferred "look decision" had in fact already been made elsewhere: `ri_live_gr_level()` in `gui/livestate.c` maps the reduction onto 0..127 at `RI_LIVE_GR_FULL_DB` (20) (`gui/livestate.h:23`), and it had a caller nowhere. The fix is the same push shape as the input meters — `ri_sfx_meter_set(&u->u.fx, RI_SFX_COMP_GR, ri_live_gr_level(fgr))` on the COMP canvas (`C_FX3`) with change detection (`s_fxgr_shown`) and a GR-box-only repaint via the existing `MX(RI_SEC_COMP, 4)` geometry row (`gui/panelgeo.c:485`).

Verification for the AROS-only half follows project practice for shell code (t219's own comment names it): a source gate in `scripts/ri_audit.sh` asserting the conversion, the push, and the repaint each exist as statements, not comments — proven non-vacuous by stashing the fix and watching it fail. Plus the real compile: `app/riapp.c` builds clean (`rc=0`) under `x86_64-aros-gcc -Werror` with the v11 SDK. The ear proof on the lane is still owed, stated here rather than left to be discovered.

## Symptom 2: a bypassed unit's peak freezes, because feed is measure and decay in one call

`ri_meter_feed()` (`engine/mixer/mixer.c:23`) both measures (`peak *= coef` per sample, then max) and decays — there is no other decay path, and `ri_meter_peak()` returns the held value raw. Both feed sites sit inside the `fx_on` render guard (`engine/engine.c:171-184`, `:252-262`, `:956-959`). So a unit that stops being live is never fed again and its peak freezes at its last level; the panel keeps publishing the frozen value and the meter stays lit on a module that is audibly off.

Measured RED (new t219 sections, engine + wrapper rebuilt first — the harness links a stale `$OUT/*.o` otherwise, a trap already recorded in the 2026-10-02 breakdown record):

- `displaced PCF slot must go dark (113)`
- `displaced PCF engine peak must be zero, not held (0.673521)`
- `switched-off PCF slot must go dark (113)`

Reproduce recipe (also how the numbers above were captured): `git stash push -- engine/engine.c engine/fx/fx.c`, rebuild the `engine` and `fx` modules, run the three tests. The re-engage assertion in the same block (PCF lights again) passes throughout, so the fixture is valid and the failures are the defect, not the setup.

Fix, in `ri_fx_engage` (`engine/engine.c:346`): zero the meter peak on both transitions — the OFF path zeroes its own, the displacement loop zeroes every non-live unit (no liveness diffing; zeroing an already-dark peak is a no-op and keeps it one loop with no extra state). Peak state is pure telemetry — nothing on any render path reads it — so this cannot change audio. One fix covers all four units including Delay, which rides `e->dline` rather than an insert and would otherwise need its own. GREEN, then t222/t218/t52/t26/t1/t50/t67/t140/t216 all still pass.

## Found en route: `gr_min_g` had no restart anywhere

`ri_fxcomp_gr_reset` had **zero callers** in the tree. So the GR hold was a lifetime latch, not the documented "block min gain ... read/reset" (`engine/fx/fx.h:156-159`): the deepest gain ever seen sticks and the bar pegs after the first loud passage. Proven RED at wrapper level (t50): hot render reads `-9.503 dB`, then 30 quiet invocations (3 s, see below for why that long) still read `-9.503`.

Placement was the whole problem, and the stereo insert path decides it: that path renders L then R through one `e->comp` per block (`engine/engine.c:182-183`), so a restart *inside* the render fn would wipe L's min and publish R only — a metering error introduced while fixing a metering bug. Restarts therefore live where block boundaries are visible: once per slice in `ri_engine_render`'s loop (next to the `rb808_voice_counters_reset` precedent, same "figures describe THIS block" discipline), once per `RiFXRender` invocation in the wrapper. The slice restart is unconditional, which also makes bypassed-always-dark fall out (reset with no render reads 0 dB) — but the `ri_fx_engage` reset stays regardless, because it covers the transition instant itself: without it the last reduction freezes on screen until the next block renders. Proven RED at engine level (t218: engaged COMP reads `-11.25 dB`, displaced still reads `-11.25`).

## My own wrong test, kept as the method note

The first t50 version expected release after ONE quiet block and failed on correct code at `-9.502 dB`. That number is the first sample's gain, and it is truthful: the follower (10 ms attack / 100 ms release, `engine/fx/fx.h:63-65`) leaves `env` near 0.9 through the first ~220 ms of quiet (crosses the −19.8 dB threshold at ≈10,450 samples: `0.9·e^(−t/0.1) = 0.1019`), so the comp really is still compressing. Latched-vs-restarted only becomes observable after release — hence 30 × 4800 samples (3 s ≫ 100 ms). A one-block expectation fails on correct code; the failing value told me which one of us was wrong.

## State and verification

- **UNCOMMITTED.** 7 files: `app/riapp.c`, `engine/engine.c`, `engine/fx/fx.c`, `scripts/ri_audit.sh`, `tests/unit/t218_fx_enable.c`, `tests/unit/t219_fx_meters.c`, `tests/unit/t50_fx_dist_comp.c`. Parent `860072f`.
- Full `scripts/ri_audit.sh`: **AUDIT 0/0 PASS** (host tests, mutants, goldens, AROS compiles).
- Fixes attempted: 2, both green first time. No architecture question triggered.
- Owed on the lane: the ear proof (GR bar moves under compression; displaced meters go dark). Everything else is host-proven.

## Open, explicitly not observed

- Sibling canvases are never told to repaint on a rack change (a click redraws only the clicked canvas, `changed(obj, d)`). The lamp *model* is provably correct, so any staleness self-corrects on the next repaint of that canvas — but no test watches a repaint happen, and the MCC layer is AROS-only. Noted because it is the same shape as this session's defects, not because it was seen.
  - **Status 2026-10-18: closed generally, by the coupling rather than by a third per-caller patch.** `sync_values()` now repaints the box of whatever changed, and `t228_press_repaint` pins the one decision that is not plumbing — which controls get one, with METER rows excluded because the meter loops own them (P-RPN1, `514f4ee`); the displaced-canvas half stays separate in `t223`, because a canvas displaced by another canvas's press receives **no** event of its own. The line above is left standing because it records what was known on 2026-10-15.
- `t219`'s header claims its shell half "is gated by source inspection in ri_audit.sh" — no such gate existed until this session's three GR greps. The claim predates the gate; trust the gate.
