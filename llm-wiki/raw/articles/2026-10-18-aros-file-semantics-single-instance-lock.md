# AROS file semantics that a single-instance lock has to respect

- Source: Dell E6320 lane e6320, scratch probe `RAM:CLAIMPROBE`, 2026-10-18
- Collected: 2026-10-18
- Published: 2026-10-18
- Related: `2026-10-18-aros-gui-app-autostart-wbstartup.md`, `2026-10-18-s7-song-skin-completion.md`

Every item here cost a lane round-trip. None of them is guessable, and all
of them are silent — the wrong behaviour looks exactly like "it worked".

## 1. `MODE_READWRITE` does not create; `MODE_NEWFILE` does

`Open(name, MODE_READWRITE)` on a **missing** file fails. `MODE_NEWFILE`
creates or truncates. A lock whose heartbeat is written through a
`MODE_READWRITE` open therefore silently never writes the heartbeat: the
directory gets created, the file never appears, and every later process
reads "no heartbeat" forever.

Use the platform's own write helper rather than a second write path. The
repo already knew this (`ri_pal_write_file` opens `MODE_NEWFILE`); the bug
was a *new* write path that did not.

## 2. A file held open for writing cannot be re-opened by another process

While one process has a file open for writing, a second process cannot
open it `MODE_NEWFILE` (nor write it). Consequences for any lock file:

- Reopening the heartbeat on every refresh makes a contender's probe
  *succeed* — and then the contender concludes the holder is dead and
  steals the claim. The exact inversion the lock exists to prevent.
- Holding the handle open for the process lifetime fixes it, and turns
  the contender's inability to create the file into **positive evidence
  of a live holder**. Failing safe here is deliberate.
- Reads of a file another process holds open for writing are fine.

## 3. `Rename()` returns FALSE (0) on success

AmigaDOS convention, and AROS keeps it — the opposite of every other call
in the same API. Code written as "if rename failed, try the next name"
stops at the first *successful* rename, and the object can then never be
displaced again. Rename onto an existing **non-empty directory** also
fails, so a fixed ".dead" name only works once.

The robust shape: attempt the displacement, then let the platform's
**atomic** primitive verify it (`CreateDir` succeeding is the proof).
Nothing about the result code has to be trusted. This SDK also has **no
`RemoveDir`**, so a displaced lock directory cannot be deleted from a
program at all — keep displaced copies few, named, and obviously dead.

## 4. `Examine()` is not an existence test

`Examine()` on a missing name reports "here it is" here, so "is the
heartbeat there?" answered with `Examine` turns a missing file into a
permanent refusal. Use a create attempt as the existence test instead —
one primitive, honest semantics.

## 5. Clocks: `DateStamp` is per-boot, so age arithmetic must clamp

`ds_Minute * TICKS_PER_SECOND + ds_Tick` is minutes **since boot**, not a
wall clock, and it differs between boots. A stamp from the previous boot
is therefore "in the future" relative to a fresh one: an unclamped
`age = now - stamp` is negative, and a naive `age > threshold` staleness
test would call a *fresh* lock stale. Clamp negative ages to zero. (Also:
`long` is 8 bytes on this AROS x86_64 build — a heartbeat's width is
whatever `sizeof long` is on the target, not 4.)

## 6. Design rule that fell out of all of the above

Two dead-looking states need **opposite** answers, and conflating them is
how a lock becomes either useless or unusable:

| heartbeat | meaning | answer |
|---|---|---|
| absent | nobody is heartbeating (a live holder writes it at acquire) | **stale → take over** |
| present, unreadable | an I/O problem, not evidence of death | **busy → refuse** |
| present, fresh | a live holder | **busy → refuse** |
| present, older than the window | crashed or wedged holder | **stale → take over** |

Put the claim on a **volatile** volume (RAM:) so a reboot clears it, and
let the heartbeat handle the case a reboot does not cover: a crash.

## 7. Prove the decision table, not the happy path

The failure modes live in the paths nobody exercises. A scratch probe
(`RAM:CLAIMPROBE`, ~40 lines, linked against the real backend) walks the
whole table on the target and prints the decision *and its reason* for
each row. Two probe rows were themselves wrong before the code was:
renaming a file the probe itself holds open fails, so "no holder" cannot
be faked from inside a holder — release first, then arrange the state.
A probe that cannot arrange the state proves nothing about it.