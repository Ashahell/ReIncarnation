# Automation Task 5b/5c: drum per-voice and strip lane keys, strip level fader (§12.9c)

- Source: ReIncarnation session, 2026-09-26 (owner "go" after the opencode report review)
- Collected: 2026-09-26
- Published: 2026-09-26
- Commit: `91d07b4`
- Spec: `docs/superpowers/specs/2026-09-26-automation-design.md` §2.2 status and §5.2 row (update 2)

## Review of opencode's automation report (before this work)
- The ten reported commits exist, and the audit passes (0/0) at `de119b2`.
- The allow-list had 30 IDs (16 303 + 14 FX, sorted for the binary search), and `t77` was wired into the audit.
- Plan items R2 (pass marker), R3 (`RIAutoPub`), R4 (`RIAutoCarry`) and R6 (caller `atrk` buffer) are present in code.
- Gap: `t78_engine_taps` was still not in the audit (fixed here).

## Why 5b/5c were blocked
- The drum params use one engine ID for every voice (`0x040p`/`0x090p`), so a `(tick, ctl)` lane cannot tell BD Level from SD Level.
- The mixer strips had no IDs, and the engine had no strip level setter.

## Lane keys (distinct from engine param IDs)
- `0x0Bsp` — channel strips:
  - s = strip + 1 (1..4 = 303A/303B/808/909, 5 = master);
  - p: 0 level, 1 pan, 2 delay send, 3/4/5 = dist/pcf/comp insert;
  - kept above the legacy Task-12 panel IDs `0x0B00..0x0B0F`.
- `0x0Cpv` — 808: p = param nibble, v = voice. The accent is section-wide, so its only key is `0x0C50`.
- `0x0Dpv` — 909: `0x0D1F` = the shared CH/OH level knob (the 909 has 11 voices, so v = 0xF is free).
- `ri_ctlreg_auto_id()` maps every registry control to its key. The strip Level rows get the new `RI_BIND_LEVEL`.
- Allow-list: 30 → 100 entries (98 registry keys plus 303 VOLUME ×2, which have no panel control and are kept for legacy AUTO data).
- `t77` checks both directions: every recordable control is allowed, and every allowed key has a control. The unrecordable census went 98 → 28. The emitted device per block is pinned (0x0C → 2, 0x0D → 3, strips/FX → 0).

## Engine
- Every `RI_EV_AUTOMATION` event is routed by its key to the knob setters, before the drum branch.
- New `ri_engine_set_level`:
  - P-17 law `(v/127)^2`; 127 = unity, the default;
  - unity at rest skips the multiply, so the neutral path stays bit-identical;
  - post-insert, pre-meter/send/pan, with a 64-sample zipless slew.
- Insert keys: on = assign; off = release only when this strip owns the unit (a stray "off" never steals another strip's insert).
- `t79_auto_delivery` was written RED-first. Five mutants were killed: voice ignored, insert-off steal, no slew, unity fast path, hat pair on one voice.

## E0 recorded
- An 808 knob is keyed to its registry voice (LT/MT/HT/RS/CP), independent of the slot switch position (LC/MC/HC/CL/MA).
- Still unrecordable (28 rows, listed in t77): 808 sound switches and Instrument Selection; the 909 knobs with no engine param (AC Level, Attack, Tone, Snappy, Flam) and 909 Instrument Selection; FX on/off; the Pattern-section selectors.
