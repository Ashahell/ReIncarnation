# riqemu1 AHI open hang: sb128 DriverInit fault + bounded-open fix

- Source: ReIncarnation session, 2026-09-26 (riqemu1 lane, ABIv1, no audio hardware)
- Collected: 2026-09-26
- Published: 2026-09-26
- Commit: `7379d61` (fix + owner decisions batch)
- Evidence: `docs/evidence/gui/riapp-panel.md` (failure + fix section)

## Symptom

Every AHI open on riqemu1 stuck: the render task logged 6 startup
lines then silence inside `OpenDevice("ahi.device")`; the GUI sat in
`wait_state` forever, so the null fallback never engaged and RIAPP
never opened. The stock M1.1 `probe_ahi` hung identically —
device-level, not app code.

## Diagnosis trail

- RAM-log probes (per-step open/append/close) bracketed the stall to
  the `OpenDevice` call itself: neither the failure nor the success
  branch ever logged.
- `Status FULL` showed no render task — but it lists processes, not
  tasks, so a hung task is invisible there (lesson).
- HMP `screendump` (full 1280x1024, vs the agent's 1/2-scale mush) made
  the requester readable: `Task: RIAPP render`, `Error 0x80000008`
  privilege violation, `Module sb128.audio`, `Function DriverInit
  +0x135` — the known codec-less-guest driver fault (same +0x135 as the
  2026-09-21 sb128 movaps finding), hit during AHI's mode scan.
- Unified story: the first open crashes the task in the driver scan;
  later opens hang on the half-initialized device state.

## Fix (in-repo, `audio_ahi_live.c`)

- The open handshake is bounded (10 s timer): timeout abandons via an
  open-generation counter and returns err 7; the caller falls back to
  null. A late task unwinds quietly (frees its AHI objects, touches no
  shared state, never signals); `AHIBase`/`s_render`/`TimerBase` writes
  are generation-guarded against clobbering a newer stream.
- Proven on riqemu1: `err 7` logged, null panel opens 904x587, Space
  plays, closes clean via the close gadget.
- Debris left on the lane (4 stuck GUIs, hung tasks, 1 requester) is
  harmless and dies with the next lane reboot.

## Lane lessons banked this slice

- The Dell wedges after `--ui-capture`: prove with LOG + window
  listings + owner eyes, no captures.
- The MUI panel ignores Q: quit via close gadget or `Break <cli> C`
  (Status disambiguates same-titled instances).
- Rawkey press/release pairs stay adjacent (a 1 s hold stormed).
- HMP `screendump` over port 4477 reads requesters; HMP `sendkey esc`
  does not dismiss an AROS Alert.
- Open Dell question: two display freezes launching the panel binary
  (text `RIAPP 256` is the known-good control: run it first after a
  reboot, then the panel).
