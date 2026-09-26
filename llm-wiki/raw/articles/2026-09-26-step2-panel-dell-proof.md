# The real panel in RIAPP: knobs sound, meters chase (§12.11 G9b Step 2)

- Source: ReIncarnation session, 2026-09-26 (owner listening + watching on the Dell E6320, ABIv11; null-backend smoke on riqemu1)
- Collected: 2026-09-26
- Published: 2026-09-26
- Commits: `eadf0ba` (host half: panelctl bridge + meter seqlock + t83), `526cc18` (panel, honest UNPROVEN), `7379d61` (bounded open + decisions), plus the proof close-out
- Evidence: `docs/evidence/gui/riapp-panel.md`

## Host half (t83, 5/5 mutants)

- `gui/panelctl` bridge: one canvas change -> exactly one control-plane
  message with the right lane key and value (13 bound controls across
  all 12 bound sections), unbound/unknown/refused queue zero, values
  clamp 0..127. RED at `t83_panelctl.c:53`.
- Live meter snapshot seqlock: render-side begin/end around every meters
  update, GUI-side read 0 ok / 1 busy-torn (drop the tick) / 2 bad args.
  The GUI never reads session meters directly (closes G6b).

## Panel routes (one path each)

- Sounding values: canvas last_hit reg_id (geometry returns full
  reg_ids) -> bridge. Transport edges -> `au_live_request` (Record
  plays until Step 4). PAT select -> `ri_track_capture` at the snapshot
  bar; length -> bank slot; off -> bank length parked at 0 (player reads
  0 as silent); 303A steps -> `ri_p303_set` on the selected bank slot.
  Startup bursts all sounding defaults through the bridge (whole board
  incl. the 303A strip level 72). Snapshot + livestate drive meters and
  `ri_panel_live` chase on a 100 ms tick. Placeholders labeled in
  `app/riapp.c` (tempo/shuffle/loop/rewind/FF/song-mode, PAT shuffle,
  mixer on/off, 303 programming pending state, drum taps, skins,
  capture keys for Step 5).

## Bugs found by lane evidence

- Startup burst skipped the 303A strip (engine would have played the
  default, not the owner-approved 72): burst the whole board (review).
- `sync_pat` canvas/bank variable aliasing (review).
- sb128 `DriverInit+0x135` privilege violation hangs any AHI open on
  riqemu1 (stock probe hangs identically): bounded 10 s open handshake,
  open-generation abandon, err 7 null fallback. Doubles as Dell
  hardening. Note: `Status FULL` lists processes, not tasks.

## Dell proof (owner)

- Session 1: 68826 buffers (~9.8 min), 0 xruns, max 916 us. Cutoff and
  level "heard clearly"; pan "unsure" (speakers); meter not looked at.
- Session 2: 18787 buffers, 0 xruns, max 898 us. Pan + meter "both
  work" on retry. Transport Play/Stop logged throughout.
- Playhead chase not explicitly eyeballed (same snapshot mechanism).

## Rulings

- Abandoned render tasks leak at most one task + stack (no driver
  resources held that early); documented, never a hang.
- Owner decisions taken during the slice: dist one-owner radio; G5/G7
  E0s kept; 909 tap Low (E1 p.32/p.226); GR 20 dB; floor -36; OPEN-10
  multi-format plan; zoom cap-to-fit + scrollbar; skin chunk at G10.1.
- Lane rule learned: the Dell wedges after `--ui-capture` — prove with
  LOG + listings + owner eyes, no captures.
