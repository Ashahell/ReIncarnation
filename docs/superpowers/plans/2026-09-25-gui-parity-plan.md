# GUI parity with ReBirth RB-338 2.0.1 — plan (§12.10)

**Date:** 2026-09-25. **Tracker:** `docs/2026-09-24-improvement-todo.md` §12.10.
**Target:** workflow and control parity with ReBirth 2.0.1 (E1: Owner's Manual; page refs below), control order and grouping as on the original TB-303 / TR-808 / TR-909 where ReBirth follows them. Artwork stays clean-room (spec §1: measure proportions, never copy pixels).
**Coexistence rule (while the §12.7a pattern-model work runs in parallel):** this plan touches only `gui/`, `app/`, new `tests/unit/t6x_*` files and the `MOD_gui` line of `scripts/ri_build_host.sh`. It does **not** edit `engine/`, `project/`, `scripts/ri_audit.sh`, or tests t53–t56. Audit wiring for the new tests lands as one commit after §12.7a is merged (G1.6).
**Device lanes:** Dell unavailable (stick backup). AROS proofs run on the private QEMU lane `riqemu1` (v1 ABI, spike port 9295) with `ui_capture` + monitor screendumps; the Dell re-checks later.

## 0. What exists (baseline)

- Widgets (AROS MCC): `RKnB` knob (drag arc, fine, right-click default, commit-on-release — owner-approved), `RStp` step button (32 px, armed toggle), `RLbl` label, `RFdr` fader (MUIC_Slider skin, 100 px law untested on device), `RLvl` meter (MUIC_Levelmeter skin).
- Proof vehicles: `app/panel909.c` (4 knobs), `app/stepproof.c` (16 steps + chase), `app/knobproof.c`; `app/main.c` is still a bare Intuition window.
- `gui/panels.c`: 6 small tables (29 controls). ReBirth has well over 200 panel controls (below).

## 1. Control inventory (E1)

Source of truth for the inventory: Appendix C “Standard MIDI Mapping Tables” (p. 195–202) — it names every automatable panel control per section with its controller number — plus the reference chapter (p. 143–165) for switches, buttons and displays that have no controller.

| Section | Controls (ReBirth legend) |
|---------|---------------------------|
| Synth 1 / Synth 2 (p. 153–156, CC 23–29 / 30–36) | Waveform, Tune, Cutoff, Reso, Env Mod, Decay, Accent; step entry: 13 pitch keys (C…C), Down, Up, Accent, Slide, Note/Pause, Back, Step, Pitch Mode, Clear, step display |
| 808 (p. 148–150, CC 37–64) | AC Level; BD Level/Tone/Decay; SD Level/Tone/Snappy; LT Level/Tune/Switch; MT Level/Tune/Switch; HT Level/Tune/Switch; RS Level/Switch; CP Level/Switch; CB Level; CY Level/Tone/Decay; OH Level/Decay; CH Level; Instrument Selection (large knob); 16 step buttons; 11 instrument legends + AC |
| 909 (p. 151–152, CC 65–93) | AC Level; BD Level/Tune/Attack/Decay; SD Level/Tune/Tone/Snappy; LT/MT/HT Level/Tune/Decay; Hi-hat Level (CH+OH shared); RS Level; CP Level; CH Decay; OH Decay; CC Level/Tune; RC Level/Tune; Flam amount; Flam button; Instrument Selection; 16 step buttons (off/low/high/flam) |
| Mixers ×4 (p. 157–158, CC 11–22) | On/Off, meter, Level fader, Pan, Delay amount, Dist / PCF / Comp switches, Mute (not automated) |
| Master (p. 165, CC 7) | Level fader, stereo meters, Comp switch |
| PCF (p. 159–160, CC 94–99) | On/Off, meter, Pattern, Mode (LP/BP), Freq, Q, Amt, Decay |
| Delay (p. 161–162, CC 100–103) | On/Off, meter, Steps, Triplet switch, Feedback, Pan |
| Dist (p. 163, CC 104–105) | On/Off, meter, Amount, Shape |
| Comp (p. 164, CC 106–107) | On/Off, meter, Ratio, Threshold, gain-reduction meter (centre zero) |
| Transport (p. 144–146, CC 108) | Song/Pattern switch, Tempo (20–500) display + arrows, Shuffle amount, Bar display + arrows, Play, Stop, Rewind, Fast Forward, Record, Loop On/Off, Loop Start, Loop Length, MIDI-in LED, Sync LED |
| Pattern sections ×4 (p. 147) | Section off, Bank A–D, Pattern 1–8, Length display + arrows, Shuffle on/off |
| Focus bar (p. 22) | vertical orange bar beside the pattern selectors, one section at a time |

Order and grouping follow the hardware where ReBirth does: 303 knob order Waveform · Tune · Cutoff · Reso · Env Mod · Decay · Accent (TB-303 panel order), drum knobs grouped above their instrument legend (TR-808/909 layout), levels in red and parameters in white on the 808 (p. 148).

## 2. Phases

### G1 — Control registry (host, pure C) — **this session**
One table is the single source for every panel control: registry id, section, instrument group, ReBirth legend, widget kind, range, default, Standard-Mapping controller number (Appendix C), Song-automatable flag (everything except Tempo, the Mute buttons, Master Level — p. 72 — and Shuffle — p. 73; transport buttons, displays and meters are not controls at all), and the engine binding (engine control id, or unbound).
1. `gui/ctlreg.h/.c` (new; `panels.c` untouched so existing consumers keep working).
2. Kinds: `KNOB`, `FADER`, `SWITCH` (2-position), `BUTTON` (momentary), `LED`, `STEP`, `SELECTOR` (n-position rotary: 808/909 instrument select, PCF pattern), `DISPLAY` (numeric), `METER`.
3. Engine binding: existing ids only (303A/303B `0x030x/0x031x`, FX `0x0A0x`, 808/909 kit ids where the manual control matches). Every ReBirth control the engine cannot yet honour is **unbound** and the GUI must render it disabled — never a silent no-op (review §10 item 2).
4. Tests `tests/unit/t60_ctlreg.c`: per-section counts equal the manual tables; every Appendix C controller number present exactly once and equal to the manual; ids unique; ranges sane (default within range, selector sizes: 808 and 909 Instrument Selection 12 positions (AC + 11 instruments, p. 203), PCF pattern 54, delay steps 1–32, tempo 20–500); automatable flag false exactly for Tempo, Mute ×4, Master Level, Shuffle amount (and for every non-control kind); every bound engine id lies in a block the engine dispatches (303 `0x0300–0x031F`, FX `0x0A00–0x0A0F`, 808 `0x0400–0x0405`, 909 `0x0900–0x0903`); coverage report (bound/unbound per section) printed for the record.
5. `MOD_gui += gui/ctlreg.c`.
6. After §12.7a merges: add `t60` to audit Phase 12 (one line) + a gate that `ctlreg.c` is the only file defining legends.

### G2 — Panel geometry model (host)
Section rectangles and control centres in panel units, measured from the manual's panel figures and the hardware proportions (same method as the 909 geometry lock: knob pitch from hardware diameter ratio). Zoom 1× / 1.5× / 2× through the existing `ri_zoom_scaled_px`. Host tests: no overlaps, everything inside its section, hit-test round-trip, focus-bar rect per section. Ledger: `docs/evidence/gui/panel-geometry.md` (E0 numbers with sources).

### G3 — Missing widgets (AROS MCC)
`RSw` 2-position switch (waveform, triplet, LP/BP, 808 sound switches, song/pattern), `RBtn` momentary/LED button (transport, pattern 1–8, bank A–D, Up/Down/Accent/Slide/Note-Pause), `RSel` n-position rotary selector (instrument select, PCF pattern), `R7s` numeric display (tempo, bar, step, length, loop start/length — LED-segment look, own pixels), `RStp` states extended to off / on / low / high / flam / playhead, `RGr` centre-zero gain-reduction meter. All share the `RKnB` rules (own pixels, commit-on-release, right-click default where the control has one). Host logic first (state machines, click cycles from the pattern model's `ri_pdrum_click`), MCC shells second.

### G4 — Section panels on riqemu1
One app per section first (proof vehicles), then composition: 303 → 808 → 909 → mixers + master → FX row → transport + pattern sections. Each proof: `ui_capture` at scale 1, geometry check against G2 (±2 px), every control's readout changes, unbound controls visibly disabled.

### G5 — Focus bar + keyboard (Appendix E, p. 221–225)
Focus bar and focus switching (click between selector and section, up/down keys); pattern selection keys (1–8 / Q–I / A–K / Z–,); synth step programming keys (pitch keys on absolute positions, Return = Step, Backspace = Back); transport keys (numeric pad: 0 Stop, Enter Play, space Stop/Play, * Record, 7/8 bar, 1/2 loop points, +/- tempo); menu shortcuts (Cut/Copy/Paste, Shift J/K, Randomize R, Alter Y, Alter Accents U).

### G6 — Live state: meters, chase, playhead from the audio clock
Replace the stepproof `timer.device` beat with the render task's sample position (review §2.9: measured +347 µs/fire drift). Meters from the engine's per-section/FX taps; comp gain-reduction from `ri_engine_comp_gr`.

### G7 — MIDI Standard Mapping (Appendix C)
CAMD bridge maps controller numbers through the registry; pattern-select and focus notes (p. 199–204); MIDI-in and Sync LEDs.

### G8 — Skins/mods, zoom finish, acceptance
Mod art through the MCC classes, zoom crispness, the ReBirth-101 tutorial checklist in `docs/evidence/gui/acceptance.md`.

## 3. Out of scope here
Engine support for per-instrument 808/909 controls (engine slice; the registry exposes exactly which are unbound), song-mode recording, pattern hosting (§12.7).
