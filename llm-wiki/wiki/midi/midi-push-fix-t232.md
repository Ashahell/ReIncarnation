# MIDI push fix: t232_midi_push — all 99 mapped CCs now report

> Sources: ReIncarnation session, 2026-11-19
> Raw: [2026-11-19-midi-push-fix-t232-mapped-cc-report.md](../../raw/articles/2026-11-19-midi-push-fix-t232-mapped-cc-report.md)
> Updated: 2026-11-19

## Overview

Fixed `t232_midi_push` — all 99 mapped MIDI CCs now report their touch. Previously 6 mapped CCs (7, 11, 14, 17, 20, 109) for mixer Level faders were not reporting their touch when the CC value equaled the control's default (127). The `cc()` function in `midimap.c` only reported a touch if `ri_sui_set()` returned 1 (value changed). For mixer Level faders with default=127, sending CC value 127 didn't change the value, so no touch was reported.

This is the purest instance of "looks live but isn't": for a G7 Control Change the **lamp moves and the engine never hears it**. The shell needs to know which control was addressed to push its current value.

## The fix

Modified `gui/midimap.c` and `gui/midimap.h`:

1. Added `last_cc_val[128]` array to `RIMidiIn` struct to track last value per CC (0xFF = never seen)
2. Modified `ri_midi_init()` to initialize the array to 0xFF
3. Modified `cc()` function to:
   - Suppress repeated identical CCs (no-op detection via `last_cc_val`)
   - Always report on first CC after reset, even if value equals default

## Affected CCs

All 6 mixer Level faders (default=127) now report on first CC 127:
- CC 7 → SYNTH1 Level (reg 0802)
- CC 11 → SYNTH2 Level (reg 0402)
- CC 14 → 808 Level (reg 0502)
- CC 17 → 909 Level (reg 0602)
- CC 20 → MASTER Level (reg 0702)
- CC 109 → LEVI Level (reg 1402)

## Test verification

`t232_midi_push` now passes:
- CC 109 reports registry id 1402 and value 127
- All 99 mapped CCs report something on first send
- Repeated identical CCs suppressed (no stale push)
- Wrong channel, clock, SysEx correctly report nothing

## Files changed

- `gui/midimap.h` — added `last_cc_val[128]` to `RIMidiIn`
- `gui/midimap.c` — updated `ri_midi_init()` and `cc()` function
- `tests/unit/t232_midi_push.c` — added LEVI section initialization for test

## Root cause analysis

The G7 MIDI path (`cc()` in midimap.c) changes the PANEL only (lamp moves). The M4 Leviasynth path (`midi_drain` → `ri_ctl_send`) reaches the ENGINE. The G7 change was never pushed because `sync_values()` gates on per-canvas `MUIA_RSection_Changes`, and a MIDI message never goes through the canvas event handler. `s_panel.changes` is read only in `midi_drain` and only triggers redraw, not push.

## See Also
- [Dell ABIv1 deployment](../hardware/dell-abiv1-deployment.md)
- [Wiring census complete](../wiring/wiring-census-w0-w3-complete.md)