# 2026-09-24 — Live chase: timer beat clock + playhead + self-measured lag

> Source: session work (MCC attr + timer app + device proof)
> Collected: 2026-09-24
> Published: 2026-09-24

## Disposition
New. First 2.10-full device slice: the 16 steps chase at 174 BPM
with the playhead visible and the lag budget measured live.

## What
- `gui/widgets/rstp.{h,mcc.c}`: `MUIA_RStp_Chase` (TAG_USER BOOL)
  with instance mirror + OM_GET serve + redraw-on-tag; Draw paints
  a warm white edge while chased. No new pure logic (position math
  stays in pinned `ri_chase_step`).
- `app/stepproof.c`: timer.device microhertz one-shots at the
  pinned 174 BPM 16th (86.2 ms), rearmed every fire; grid origin +
  per-fire lag vs grid, max reported; STEP + LAG readouts (reused
  count formatter, zero new logic). TimerBase borrowed from the
  open request (standard device-base trick). NewInput signal-seed
  loop (`sigs |= tsig`) so beats wake the MUI loop.
- Portability notes (caught by -Werror, not by thought): AROS
  calls it `struct timerequest` (lowercase); `period_us` needs an
  initializer for maybe-uninitialized.
- AROS -Werror clean. Full `ri_audit.sh` 0/0. Binary deployed.

## Dead chase on device (found post-commit, fixed same day)
- Symptom: playhead never moves (identical frames 30 s apart),
  STEP/LAG frozen, clicks dead — whole app paralyzed. Two runs
  disagreed (0 fires vs 3 fires then silence) → non-deterministic,
  pointing at uninitialized state, not logic.
- Suspect: `io_Flags` garbage on reused IORequests (timer rearm +
  input-device warp). CreateIORequest/stack may leave stale IOF
  bits; the device then behaves erratically. Fix: clear
  `io_Flags` at creation and before every SendIO/DoIO (both timer
  path and knob warp path). -Werror clean; audit 0/0; redeployed.
- Verdict pending: fresh-window STEP advance (fix confirmed) vs
  still frozen (suspect timer.device itself or InputBuffered
  blocking semantics — next hypotheses in that order).

## Files
- env: `gui/widgets/rstp.{h,mcc.c}`, `app/stepproof.c`
- Dell scratch: `/home/miller/Work/ri_build/dell2/ri_stepproof`
- wiki: this article + `llm-wiki/log.md` + `llm-wiki/index.md`

Remaining 2.10: accent red / flam green glows; 303-side chase.
",
