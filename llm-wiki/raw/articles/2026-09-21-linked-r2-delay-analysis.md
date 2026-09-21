# 2026-09-21 — Linked-r2 non-completion: NewWriter delay-branch analysis

> Source: session evidence (static trace of `Device/devcommands.c`), compiled by agent
> Collected: 2026-09-21
> Published: 2026-09-21

Static root-cause trace for the last open playback item: linked r2 never
completes naturally (stays pending until abort) while r1 and all
independent requests complete.

## Chaining design (`devcommands.c`, `NewWriter`)

- A request arriving with `ahir_Link` set while its target is
  Playing/Silent/Waiting goes to `WaitingList`, and the link is REVERSED:
  old→new (`otherioreq->ahir_Link = ioreq; ioreq->ahir_Link = NULL`,
  lines ~1160–1169; the comment at ~1156 states the user points new→old
  and the device makes old→new).
- The probe's circular links (r1↔r2) self-heal and are NOT the cause: at
  `SendIO(r1)` the target r2 is not yet queued anywhere, so `delay` is
  false and r1's link is cleared (`ioreq->ahir_Link = NULL`, ~1229–1231)
  before it plays normally. r2 then attaches behind the in-flight r1
  through the delay branch.
- The abort path (~152–231) walks `ahir_Link` chains and replies
  `IOERR_ABORTED`, which is why aborts are always clean.

## Suspect: r1-finish → r2-start promotion on VOID

r1 completes (err=0) but r2 is never promoted from `WaitingList`. Two
sub-cases, not yet distinguished:

1. Promotion rides on driver end-of-sound notification (`SoundFunc`),
   which VOID — looping its buffer with no timer — never delivers.
2. The completion path that replies r1 (`TermIO`) doesn't itself drain
   `WaitingList`.

## Decisive bisect (one guest run, not yet executed)

Send r1 unlinked → `WaitIO(r1)` (completes) → send r2 linked to the
COMPLETED r1 → `WaitIO(r2)`. This exercises the "already finished, undo
delay" branch (~1178–1185) instead of the in-flight path: if r2 completes,
the bug is specifically in-flight chaining on VOID; if it hangs too,
chaining never promotes on VOID.

## Scope verdict

Not gate-blocking and no device/driver surgery warranted: P4.0/P4.1 prove
completion, the ladder measures `dev_min_frames=64`, and abort bounds keep
linked runs clean and crash-free.

> Status (2026-09-21): Bisect executed — P4.2b `WaitIO r2 done err=0`
> (r2 linked to completed r1, TRUE WaitIO, rc=0 run). In-flight chaining
> confirmed as the broken case; the undo-delay branch works. The suspect
> is now precisely the WaitingList→playing promotion on completion
> (SoundFunc/end-notification on VOID). Scope verdict above stands.

Measured P4.2b sequence (guest serial): `P4.2b pre-SendIO r2
(link=completed r1)` → `SendIO r2 done` → `WaitIO r2 done err=0`, followed
by `CloseDevice done`, `P5 summary`, rc=0, agent alive. Full run completed
with CloseDevice enabled (no aborts in P4.2b).
