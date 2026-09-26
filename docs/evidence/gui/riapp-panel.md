# RIAPP panel (G9b Step 2) — evidence

## Status: BUILT, NOT YET PROVEN ON HARDWARE (lane down)

- Host half (GREEN, t83, mutants 5/5): `gui/panelctl` bridge (one
  canvas change -> exactly one control-plane message, clamp 0..127,
  unbound queues zero; 13 bound controls across all 12 bound sections) +
  live meter snapshot seqlock (render-side begin/end, GUI-side read
  0/1/2). Audit `AUDIT 0/0 PASS`.
- AROS builds: ABIv1 `RIAPP` 328528 B, 0 unresolved, 0 r12 moves
  (`bash scripts/ri_build_aros.sh riapp`); ABIv11 panel binary 0
  unresolved (same TU list + `-lmui`).
- Dell, pre-relayout binary: window "RIAPP live panel" 760x768 opens at
  (0,0), all 7 canvases draw (capture `panel-1.png`: transport, 4
  pattern sections, 303A knobs/keys/steps, 808 strip). Mixer strip
  clipped at the bottom -> relaid out side-by-side (303A | MIX808);
  relayout binary built (both ABIs) but its layout is unverified on
  screen.
## Failure found + fixed: unbounded open vs broken drivers (riqemu1)

- Symptom (riqemu1, no audio hardware): every AHI open hung or crashed
  inside `OpenDevice("ahi.device")`; the GUI stuck in `wait_state`
  forever, so the null fallback never engaged and RIAPP never opened.
  The stock M1.1 `probe_ahi` hung identically (device-level, not app
  code): HMP full-res requester reads `Task: RIAPP render`,
  `Error 0x80000008 privilege violation`, `Module sb128.audio`,
  `Function DriverInit +0x135` — the known codec-less-guest driver
  fault (same +0x135 as the 2026-09-21 sb128 movaps finding), hit
  during AHI's mode scan.
- Fix (`audio_ahi_live.c`, in-repo): the open handshake is bounded
  (10 s timer): timeout abandons via an open-generation counter (the
  task unwinds quietly: frees its AHI objects, touches no shared
  state, never signals) and returns err 7; the caller falls back to
  the null backend. Late-task shared-global clobber (AHIBase,
  s_render, TimerBase) is generation-guarded.
- Lesson: `Status FULL` lists processes, not tasks — a hung render
  task is invisible there. HMP `screendump` gives full-res captures
  for reading requesters the agent downscales into mush.

## Still OPEN (needs the Dell lane)

AHI play from the panel, knob->sound within one buffer, meters
chasing, pattern select/length/off/step routes, startup burst, and the
owner three-knob check. Null-backend panel proof rides riqemu1 once the
bounded open lands there.

## Lane lessons (paid during this slice)

- The MUI panel ignores Q (no vanilla-key handler; keys belong to the
  Appendix-E owner canvas). Quit via the close gadget or Shell
  `Break <cli> C` (the loop breaks on SIGBREAKF_CTRL_C). A Q-press
  release pair aimed at the panel quits nothing — the previous
  double-instance `AHI_AllocAudioA` err-4 came from the surviving first
  instance holding the channel, not from the binary.
- Two same-titled instances are indistinguishable to `--ui-close`;
  disambiguate with `Status FULL` + `Break <n> C` per CLI.
- Key holds auto-repeat: press/release pairs must stay adjacent (restated
  from Step 1: a separated pair stormed 12x "RIAPP play").

## Routes (one path each; placeholders labeled in `app/riapp.c`)

- Sounding values: canvas last_hit reg_id -> `ri_panel_ctl_send`.
- Transport Play/Stop: canvas state edge -> `au_live_request`
  (Record -> PLAY; the record lane is Step 4).
- PAT select: `ri_track_capture` at the snapshot bar (changeover at the
  pattern end). PAT length: `ri_pattern_set_length` on the bank slot.
  PAT off: bank length parked at 0 (player reads 0 as silent), canvas
  length restored on on. 303A steps: `ri_p303_set` on the bank slot the
  PAT_SYNTH1 selection names; the canvas mirrors the selected slot.
- Startup: all sounding canvas defaults through the bridge (whole mix
  board incl. the 303A strip level 72), drained by the first buffers.
- Meters/position: `ri_live_meters_read` only; `livestate` scales;
  100 ms timer.device tick drives the chase (+ null-backend advance).
- Placeholders (no engine route yet): transport tempo/shuffle/loop/
  rewind/FF/song-mode/record lamp, pattern shuffle, mixer on/off, 303
  programming pending state, drum taps, skins, capture keys (Step 5).
