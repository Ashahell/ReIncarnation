# T2 device pixel-identity (riqemu1) + T4 Dell run (2026-09-26 lane proofs)

## T2: old painters vs display-list replayer — IDENTICAL

- Lane riqemu1 (1280×1024, ABIv1). OLD = RISECT built at `626d217`
  (pre-move painters), NEW = RISECT at HEAD (`6345908`, replayer).
  All six demo modes (`303/808/909/mix/fx/tr demo`), same window rects
  (e.g. RI-303 760×286 at 0,0), scale-2 captures cropped to the window.
- Result: **303/mix/fx 0 differing pixels**; 808/909/tr initially showed
  20/36/32 diffs — but an OLD-vs-OLD control run reproduced the EXACT same
  pixels (full overlap), and **NEW-vs-OLD2 is byte-identical (0 diffs)**.
  The first OLD run was the outlier (lane had just come off the sb128
  Software Failure episode below). The port contributes zero diffs.
- Captures: `docs/evidence/gui/img/2026-09-26-t2-identity-{old,new}-{303,808,909,mix,fx,tr}.png`.
- Scope note: demo modes at default zoom, Classic (procedural) skin —
  riqemu1 carries no Mods. Zoom sweep + 808-RI skin proof needs the Dell
  (owner lane); the replayer is zoom-agnostic (same path, PX in canvas).

## Lane incident (not our code)

- A `Software Failure` requester was found open: task `RIAPP render`,
  privilege violation in `sb128.audio DriverInit+0x135` — the KNOWN
  riqemu1 sb128 fault (record `2026-09-26-riqemu1-sb128-open-hang.md`).
  Another session has `RAM:PROBE_AHI`/`RIAPP_Q` processes on the lane;
  their audio open tripped it. Dismissed via HMP `sendkey esc` (visual
  only, no reset); lane verified clean after. Lesson: check `Status` for
  foreign processes before trusting a lane capture.

## T5: G7 remote re-proof — MATCH

- Current RISECT + MIDISEND (PAL send/receive) on riqemu1 through real
  camd.library: 41 + 2 script messages, same `remote1/remote2.mid.txt`.
- Final trace state EQUALS the committed G7 trace field-for-field
  (FOCUS 3, ST 0, BD `x...x...x...x...`, MIDI 43/1 with 1 ignored,
  LEDs off, SEL8 1, P909 7).
- Trace: `docs/evidence/gui/2026-09-26-riremote-trace-t5-reproof.txt`;
  capture: `docs/evidence/gui/img/2026-09-26-riremote-t5-reproof.png`
  (orange focus bar = replayer paints on device).

## T4: Dell RIAPP on hardware — 0 xruns

- v11 RIAPP (`/tmp/ri/aros-v11/new/RIAPP`, 64 TUs, 0 UND) on the Dell E6320:
  window `RIAPP live panel` 904×587, panel painted (cream/black/orange
  focus/green LEDs; capture
  `docs/evidence/gui/img/2026-09-26-dell-riapp-t4.png`, log
  `docs/evidence/portability/dell-riapp-t4.log`).
- `RAM:RIAPP.LOG`: AHI low-level `mode=0x003E0001 mix=48000 buffer=256`,
  `RIAPP play`, closed `buffers=2885 xruns=0 render_max=1748us
  render_total=786ms period=5333us`. The T4 portable driver streams on
  real hardware with headroom (worst buffer 33% of period).
- 909 unbound as designed (no pack; GUI says so). 5-minute soak + owner
  by-ear listening + knob/click feel still open (owner).

## Soak rerun (2026-09-27): PASS

- 68,657 buffers (~6.1 min), 0 xruns, render_max 1811 us (34% of period),
  graceful close. Unattended (no interaction this run).
- The earlier freeze never reproduced across four subsequent interactive
  runs. Leading theory: that session ran two RIAPP instances against one
  AHI device (duplicate-window era before close-verify discipline).
- Full log: `docs/evidence/portability/dell-soak-rerun.log`.
