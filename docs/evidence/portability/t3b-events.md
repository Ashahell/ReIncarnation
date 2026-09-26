# T3b — Input normalisation: canvas events to the core (rest of plan T3)

- Status: GREEN 2026-09-26.
- `app/core/canvas_events.h/.c` (new, pure): `RICevState` (drag + repeat),
  `ri_cev_tick` (4th-tick repeat), `ri_cev_key` (owner/decode/route),
  `ri_cev_button` (focus/hit/reset/arrows/legend/drag/press),
  `ri_cev_move` (150-px accumulate/clamp/quantize). Branch-for-branch port
  of the `rsection.mcc.c` handler; `to_n`/`from_n` duplicated small.
- `gui/widgets/rsection.mcc.c` is now a thin mapper: IDCMP class/code →
  kind ints, canvas geometry, shift qualifier; results mapped to
  `changed()` + `MUI_EventHandlerRC_Eat` (the CHANGED/EAT bits never leak
  to MUI). Instance drag/rep fields replaced by `RICevState`; `from_n`
  deleted (only the drag path used it).
- Behaviour notes: focus runs on select-down inside (unchanged, incl. the
  background-click-changes-focus-but-returns-0 case); menu-down eats only
  with a control under it; select-up is quiet without a drag.

## Tests

- `tests/unit/t91_canvas_events.c` (RED: `red-t91.txt` — no pure unit):
  idle/null ticks; key owner/panel/mapped (space = transport, eaten);
  background focus click (changed, not eaten, focus lands on 808);
  knob drag (armed → +150 px moves value → up clears → move quiet);
  transport tempo-stepper repeat (down → ticks 1-3 quiet → 4th steps →
  up clears); menu reset eats; idle up quiet.
- GREEN: `PASS canvas_events`.
- Vacuous-coverage catch (t76 lesson, recorded): steppers exist only in
  FX/transport layouts — the first version scanned SYNTH1, silently skipped
  the repeat block, and SURVIVED the `>= 400` mutant. Moved to transport;
  mutant now `FAIL tick4 steps` (killed).
- AROS: `rsection.mcc.c` + `canvas_events.c` compile clean; RISECT/RIAPP
  re-link with the core object (see audit).
- Device click/drag feel re-proof: open (owner, Dell) — logic identical by
  port, but feel is a human gate.
