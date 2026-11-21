# Review of the wiring census (W0–W3) and the ARexx port (X1–X2)

- Source: advisor review of Opencode commits `fd700b4`..`8ef90ef`, 2026-11-21
- Collected: 2026-11-21
- Published: 2026-11-21

## Census (t243)

The first t243 reported verdicts it never measured (see the correction on
[the W0–W3 article](2026-11-20-wiring-census-w0-w3-complete-full-audit-pass.md)).
The rewrite measures, per registry row, against D1 (panel → plane → engine):

- **D3 MIDI in:** the real G7 map (`ri_midi_msg` on a bound panel, t232's
  fixture) then the push RIAPP does; DIVERGE if the CC reaches another row.
- **D5 automation:** `ri_auto_touch` RECORD, the event replayed.
- **D6 save/load:** the event through an RBNG ATRK write + read, replayed.
- **D7 startup:** RIAPP's startup burst sends every 303/808/909/FX/mixer/
  master/Levi row at its panel value, so a panel default that differs from
  the engine's own is adopted by design; only rows outside the burst can read
  DEFAULT (none do).
- D2/D4/D8–D12 read **NOTMEASURED**, never pass or fail.

A row is DEAD only if **neither** end of its range moves a fresh engine (the
first version tried one end, so every Level row whose engine default is 127
read DEAD). Controls: bogus reg_id → DEAD; D3 compared at a wrong value →
DIVERGE; t224's unbound strip switch → UNEXPLAINED; 303A Cutoff → OK.

Result (510 rows): 22 bad verdicts at first, 16 after the Waveform fix, all
in the ratchet baseline (a fixed entry left in the baseline also fails):
- **0000/0100 Waveform DEAD (D1, D3, D5) — fixed 2026-11-21, owner
  approved.** The switch sent 0/1; `params.c` sets square at `value >= 64`
  (the ReBirth CC law), so the panel switch never changed the waveform.
  `ri_panel_ctl_send` now sends a BIND_303 0..1 switch as 0/127; the
  registry range and the panel value stay 0..1, and recording (after the
  bridge, `engine/live.c`) records 0/127. Lanes recorded before the fix hold
  0/1 and still play saw. (Opencode's unapproved version widened the
  registry range to 0..127 instead.) D5/D6 now record the bridged value,
  as live.c does.
- **12ce Levi Seq Clear DEAD** — a one-shot with no engine state.
- **BINDNONE_UNEXPLAINED (16):** 808 tom/conga switches + Instrument
  Selection; 909 AC Level, BD Attack, SD Tone/Snappy, Flam, Flam button,
  Instrument Selection; Levi Lfo Step. Unwired controls — the W1 backlog.

Output goes to `$RI_TMP/run/t243`, never into `docs/`. The four allow-list
generator scripts were removed and `RI_SCRIPTS_OK` restored.

## ARexx port

Checked against the v1 AROS tree (Zune `application.c` ARexx handling) and
the rexxsyslib protos in the v1 SDK. Found and fixed:

- `mp_Flags = PA_IGNORE` overrode `CreateMsgPort`'s PA_SIGNAL: the port never
  woke RIAPP, every sender hung.
- Command read from `rm_Args[1]`; ARexx puts it in `rm_Args[0]`. Every
  command was empty.
- rc came from `strtol` on the reply text ("OK…"/"ERROR…" both 0); now the rc
  `ri_arexx_handle` returns. `rm_Result2` only on rc 0 with RXFF_RESULT.
- One message drained per signal, no `IsRexxMsg` check: now a loop.
- The port was created before the instance claim; a refused second copy
  returned without removing it (a public port in freed memory). Now created
  after the claim and removed in `riapp_teardown` on every path.
- OPENSONG had its own loader (stack `RISong`/banks left dangling in the
  core, automation cast wrong); now `song_load_path`, the menu's loader.
- EXPORTWAV rendered through the live session on the main task. Removed:
  the dispatcher replies rc 20 "not implemented"; `ri_render` owns export.
- SETCTL sent to the engine only (panel, lamp and repaint never moved) with no
  row/range/BIND_NONE checks; GETCTL indexed `s_ui` by section, not canvas.
  Now: `c_sections` → canvas, `ri_sui_set`, push the settled value, repaint.
- PLAY/STOP/TEMPO bypassed the transport panel, which `sync_transport()`
  re-reads every frame — TEMPO was reverted on the next frame. Now they press
  / set the panel.

**Not lane-proven:** there is no ARexx interpreter on the v1 stick (`rx`:
"Could not start RexxMast"). X3 needs RexxMast (Regina is in AROS contrib)
or a small RexxMsg sender.

## Verified

v1 RIAPP builds (`ri_build_aros.sh riapp`), `mov %rax,%r12` count 0;
`ri_audit.sh` AUDIT 0/0 PASS with t243 gated. Nothing committed.
