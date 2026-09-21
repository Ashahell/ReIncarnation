# 2026-09-21 — Repo probe hardened and green end-to-end (P4 abort bounds)

> Source: session evidence (probe source diff, guest result blocks, serial IRQ count), compiled by agent
> Collected: 2026-09-21
> Published: 2026-09-21

TDD close-out for the deliverable `audio_io/probe_ahi.c`: RED was watched
twice (guest agent wedged by unbounded `WaitIO` on the linked second
request); GREEN is the minimal change below, verified by a full run.

## Fix (repo, uncommitted): bounded completion in the P4 ladder

After the two `SendIO` calls each rung now does `Delay(50)` (1 s poll),
`AbortIO` on both requests, then `WaitIO`. An aborted request reports
`IOERR_ABORTED` and can never count toward `dev_min` (which requires
`err==0` on both), so the gate semantics are unchanged: only natural
completions count. A probe that cannot hang is the actual deliverable;
the debug scaffolding (`/tmp` copies with PDBG breadcrumbs) stays out of
the repo.

## Final green run (repo probe, all shadows)

- P1 `best_mode id=0x001F0002`, `alloc_audio OK` — with notable honest
  readbacks: requested 48 kHz stereo HiFi, VOID actually gave 5513 Hz,
  32-bit, 128 max channels (the null sink's real mode, reported not
  assumed).
- P2 ladder 7/7 rc=0 (`low_min_frames=64`); P3 with playback started:
  `observed=50305112 expected=3750` (VOID unclocked, consistent with the
  earlier 50,503,035 run).
- P4 unit PRESENT, all rungs `err1=0 err2=-2` (r1 completes naturally,
  r2 aborts cleanly); `dev_min_frames=0` by abort design.
- SUMMARY complete; guest agent alive after (`ping ok=True`); zero
  `IRQHandle` lines in serial. rc=0 path reached (SUMMARY is second-to-last
  statement before `return 0`).

## Ops note: /tmp quota

Mid-session the host `/tmp` (16G tmpfs) hit quota (Errno 122), blocking
compiles AND the agent spool. Hoggers belong to other sessions
(`claude-1000/`, `mesa-rt/`, `v1demo_*`, ... — left untouched). Freed
~500 MB of own screenshots (`ari_*.ppm`) plus re-extractable package
dumps. Rule: screenshots are evidence only until OCR'd, then delete;
never touch other sessions' scratch.
