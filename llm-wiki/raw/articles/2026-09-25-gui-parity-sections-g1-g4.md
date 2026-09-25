# §12.10 GUI parity G1–G4: every ReBirth panel section laid out, behaving and rendered

- Source: ReIncarnation session work 2026-09-25 (commits below), plan `docs/superpowers/plans/2026-09-25-gui-parity-plan.md`
- Collected: 2026-09-25
- Published: 2026-09-25
- Evidence: `docs/evidence/gui/panel-geometry.md`, `docs/evidence/gui/section-canvas-proof.md`, `docs/evidence/gui/img/2026-09-25-ri{303,808,909,mix,fx,tr}-demo.png`

## Commits (main, pushed)

| Commit | Slice |
|--------|-------|
| `f130142` | G1 control registry: 243 controls, 18 sections, Appendix C MIDI controllers, automation exclusions, engine bindings (t60) |
| `8ae2c05` | G2 303 geometry measured from the p. 153 figure, hit-test + zoom (t61) |
| `32c9595` | G3 303 step-entry behaviour per the manual (t62) |
| `89a1dab` | G4 303 canvas on riqemu1, demo proof |
| `939c01f` | 808: geometry (p. 148), behaviour (t63), generic `RSection` canvas + `sectui` dispatch (t64) |
| `2892026` | 909: geometry (p. 151), off→low→high→off click cycle + Flam button + AC row (t65) |
| `ea22ce3` | 909 canvas styled after the TR-909 hardware panel (owner reference image) |
| `71c2f6c` | Section mixers (p. 157) + Master (p. 23), one shared board, radio insert routing (t66) |
| `b30e576` | FX units PCF/Delay/Dist/Comp (p. 159–164), value-display arrows `RI_GEO_STEPPER` (t67) |
| `31ed17b` | Transport (p. 144) + Pattern sections (p. 147), compact 0.75x zoom (t68, t69) |

Host tests t60–t69 all PASS and each was mutation-checked; `ri_audit.sh` 0/0 at every commit. The t6x tests are not yet wired into `ri_audit.sh` (plan G1.6, deferred until the pattern-plan session finishes so the audit script is not edited under a live run).

## Architecture

- `gui/ctlreg.{h,c}`: reg_id = section<<8 | index; kinds KNOB/FADER/SWITCH/BUTTON/LED/STEP/SELECTOR/DISPLAY/METER; bindings NONE/303/808V/808ALL/909V/909HAT/FX/PAN/SEND/INSERT/TEMPO.
- `gui/panelgeo.{h,c}`: positions in Q units (quarter figure pixel of the manual figure); base render 2 window px per figure px; zooms 1x, 1.5x, 2x and compact 0.75x (`RI_GEO_ZOOM_COMPACT`, added because the Transport is 842 px wide at 1x). Item shapes KNOB, RECT, LED, LEGEND, DIVIDER, OPTION (sets a selector value), STEPPER (up/down arrow of a SELECTOR or DISPLAY value display). Every item names its registry control, so layout and inventory cannot drift.
- Behaviour modules, pure C, host-tested: `sect303`, `sect808`, `sect909`, `sectmix` (four mixers + Master on one `RIMixBoard`), `sectfx`, `sectpat`, `secttr`; `sectui` dispatches by section (`ri_sui_init/press/set/reset/step/value/led/display`, `ri_sui_bind_board`).
- `gui/widgets/rsection.mcc.c`: one Zune canvas class (MUIC_Area subclass) renders any laid-out section from registry + geometry + `sectui`; knob/fader drag with Shift fine, right-click default, arrow hold-repeat on IntuiTicks (≈0.4 s delay).
- `app/sectproof.c` → `RISECT [303|808|909|mix|fx|tr] [demo]`, built by `scripts/ri_build_aros.sh sections` (0 unresolved, 0 `mov %rax,%r12`).

## Fidelity facts established from the manual (E1)

- 909 selects instruments by clicking legends, no selector knob (p. 151); step click off→low→high→off, Flam button makes a click toggle off↔flam (p. 30).
- Mixer on/off is the mute and is not Song-automated (p. 56); Master Level not automated (p. 24).
- PCF: only one section at a time; switching it on elsewhere turns the previous LED off (p. 62 step 16, p. 70). Comp: one section or the Master (p. 67).
- PCF Freq/Q/Amt/Decay are vertical sliders (p. 159 figure; p. 160 "the Amount slider") — registry kind changed KNOB → FADER.
- Value displays step by one per arrow click, stop at their ends, repeat while held (p. 18). PCF pattern 0–53, Delay steps 1–32, Tempo 20–500, pattern length 1–16.
- Pattern section: a Bank click only arms the bank — "No Pattern gets selected until you click one of the Pattern buttons 1 to 8" (p. 147); each pattern keeps its own length.
- Transport in Pattern mode: Rewind, Fast Forward, Record and the Bar arrows have no function (p. 144, 146).

## Open points recorded for owners (not changed from the GUI side)

- **Dist exclusivity (owner decision):** p. 59 says "four distortion units… all sections can use the distortion"; p. 157 says "One section at a time". GUI follows the engine's radio routing (`engine/fx/route.h`); changing it touches `sectmix.c unit_of()` plus the engine route.
- **Stop law vs manual (§12.9 owner):** `engine/seq/transport.c` arms on the first Stop click while already stopped; the manual (p. 145) moves to the Loop Start. The manual's exception (position before the Left Locator → song start) is not modelled.

## Zune / lane lessons (riqemu1, ABIv1)

- Obtain pens per screen in `MUIM_Setup` with `ObtainBestPen` (RGB pens are ignored on palette screens: first canvas drew nothing).
- Add the event handler in `MUIM_Setup` on `_win(obj)`; paint on every Draw.
- `GetAttr` stores a full IPTR — a LONG destination smashed the stack (Software Failure in `format_readout`).
- Remote click injection does not reach windows on the lane (agent ui-click and QEMU monitor mouse both land on the backdrop); state→pixels is proven by `demo` modes, a human click test remains open.
- Fonts do not scale with zoom: at compact zoom, legends next to controls are anchored to the control's side (`text_at`) instead of centred.
