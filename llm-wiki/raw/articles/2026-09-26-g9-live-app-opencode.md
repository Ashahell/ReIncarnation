# §12.11 G9 live app (opencode): control plane, live core, AHI shell, RIAPP, record host half

- Source: opencode session, 2026-09-26 (prompt `docs/superpowers/plans/2026-09-26-g9-opencode-prompt.md`)
- Commits: `48b552d` (G9.0 design note), `5216027` (G9.1 + t80), `97a40e3` (G9.2 + t81),
  `f1bfb7d` (G9.3 structure + G9.4 shell), `ee05365` (G9.5 host half + t82)
- Audit: `AUDIT 0/0 PASS` before every commit (final full run green).

## What landed

- **G9.0** `docs/superpowers/specs/2026-09-26-live-app-design.md`: state machine
  (STOPPED/PLAYING/RECORD, C4 Stop law, punch-out-all + chase on every
  discontinuity), control-plane format, session-at-device-rate policy (E0),
  xrun/degraded behaviour, GUI snapshot contract.
- **G9.1** `engine/seq/ctlplane.{h,c}` + `t80_ctlplane`: SPSC ring, capacity
  256, gated by `ri_auto_allowed`, coalesce-newest/drop-oldest overflow law
  (counted), refused keys counted, FIFO drain into `RI_EV_AUTOMATION` at the
  buffer start, cap-pressure resume. One path for knobs/MIDI/automation.
  RED `t80_ctlplane.c:33`; mutants killed: allow-check removal, coalesce
  counter removal.
- **G9.2** `engine/live.{h,c}` + `t81_live`: transport + player + `RIAutoPub`
  front/carry + control plane merged with the §8 sort, sample-window
  inversion (`t1` minimal with `map(t1) >= s1`, bounded search), rebase by
  the buffer start, one engine render. Bit-exact across 64/128/256/137 with
  and without an automation lane, and control-at-256 == offline AUTOMATION
  at 256. Meters + position from engine/transport; STOPPED is silence.
  RED `t81_live.c:75`; mutants killed: control-drain removal, rebase removal.
  Fixture fix on record: `ri_p303_set` takes keys 0..12, not MIDI notes.
- **G9.3** `audio_io/audio_ahi_live.{h,c}` (AROS-only): PlayerFunc
  Signal-only hook, render-task structure, f32->s16 deterministic twin,
  xrun count, clean stop. `CMD_WRITE` stays the fallback. Dell 64/128-frame
  measurement + 5-minute soak are owner-lane work
  (`docs/evidence/audio/live-render-task.md`).
- **G9.4** `app/riapp.c` → `RIAPP` (AROS-only, ABIv1 172792 B, 0 UND,
  0 r12): supersedes the bare `app/main.c` window (kept). Demo song (t81
  fixture), Space/S/C/P/L/R/Q keys, engine-meter snapshot (G6b app side),
  null-backend message, 909-silence notice. Full RISECT panel wiring lands
  after the Dell first-sound proof.
- **G9.5 host half** `ri_live_record`/`ri_live_record_touch` + `t82_live_record`:
  RECORD touch writes the back lane and sounds via the plane, Stop ends the
  pass and publishes, fresh playback chases bit-identical, FULL flag shown.
  Mutant proof (publish removal): FAIL `t82_live_record.c:87`.

## AROS-general lessons (Vulkan4AROS cross-post PENDING — tree dirty)

- Link order: `-lposixc` BEFORE `-lstdcio`, else `U __aros_getbase_StdCIOBase`
  on any image referencing stdio (`pcf.c` `fopen` pulled it in; `RISECT`
  never noticed because it makes no stdio calls).
- Audit AROS compile lines need `-fasm` (not just `-std=c99`) or the SDK
  inline headers fail on `asm volatile` under `-Werror`.
- RED tests must not index `ev[n-1]` with `n == 0` (stub-era SIGSEGV, not a
  clean FAIL); guard with `n > 0`.

## Open (owner lane)

- Dell re-verify of the 44100 Hz rate, RIAPP first sound + knob-within-a-buffer
  proof, 64/128-frame xrun choice, 5-minute soak, owner listening words.
- Full panel integration into RIAPP; per-section skin chunk (G10.1, owner
  review); 2x zoom cap E0; G10 deferred minors.
- Owner decisions from the prompt §7 remain open (dist exclusivity, G5/G7 E0
  items, 909 tap level, GR full scale, meter floor, OPEN-10, zoom policy,
  skin chunk).
