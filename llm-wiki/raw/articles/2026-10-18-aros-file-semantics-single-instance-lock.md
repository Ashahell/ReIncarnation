# AROS file semantics that a single-instance lock has to respect

- Source: Dell E6320 lane e6320, scratch probe `RAM:CLAIMPROBE`, 2026-10-18
- Collected: 2026-10-18
- Published: 2026-10-18
- Related: `2026-10-18-aros-gui-app-autostart-wbstartup.md`, `2026-10-18-s7-song-skin-completion.md`

Every item here cost a lane round-trip. Most were silent — the wrong
behaviour looks exactly like "it worked".

**Two of the four claims in the first version of this article were wrong,
and both had to be retracted by a probe that printed raw return values**
(RAM:APIPROBE, one short-lived file per line). The corrections are in
place below; the wrong versions are named so they are recognisable if
somebody quotes them:

| first claim | verdict | probe evidence |
|---|---|---|
| "`MODE_READWRITE` does not create" | **wrong** — it does | `Open` on a missing name: `MODE_READWRITE` handle OK, IoErr 0 |
| "`Rename` returns FALSE on success" | **wrong** — that is DOSTRUE(-1); the real trap is elsewhere | file rename `ret=-1, IoErr 0`; missing source `ret=0, IoErr 205`; onto an existing name `ret=0, IoErr 203` |
| "`Examine` answers 'here it is' for a missing name" | **unproven, withdrawn** | never tested; the real cause was ERROR_LOCKED (202), see below |
| "a held-open file cannot be re-opened" | **right** | second `MODE_NEWFILE` open → NULL, IoErr **202** |

## 1. `MODE_READWRITE` **does** create — the docs are right

`Open(name, MODE_READWRITE)` on a missing name returns a handle with
`IoErr 0`. The first version of this article claimed the opposite and that
claim sent a whole debugging round down a dead end (the symptom — a claim
directory with no heartbeat in it — was real, the explanation was not).

Keep one rule that survives the correction: **use the platform's own write
helper instead of a second write path.** `ri_pal_write_file` opens
`MODE_NEWFILE`, which is explicit about create-or-truncate. The original
bug was a *new* write path that behaved differently from the established
one, whatever the mode semantics turn out to be.

## 2. A file held open for writing can be neither re-opened nor read

While one process has a file open for writing, a second process gets
`IoErr 202` (`ERROR_LOCKED`) from `MODE_NEWFILE`, **and `MODE_OLDFILE`
returns NULL as well** — the first version of this article said reads were
fine, and that error is what actually made the busy/stale split work.
Consequences for any lock file:

- Reopening the heartbeat on every refresh makes a contender's probe
  *succeed* — and then the contender concludes the holder is dead and
  steals the claim. The exact inversion the lock exists to prevent.
- Holding the handle open for the process lifetime fixes it, and turns
  the contender's inability to read *or* create the file into **positive
  evidence of a live holder**. Failing safe here is deliberate.
- After a crash the handle is gone, both operations succeed, and the age
  decides. So "busy" is structural (a lock), not a lucky race — and the
  takeover path is reachable **only** once nothing holds the file.

## 3. `Rename()`: DOSTRUE(-1) on success — and a directory rename is worse

Raw values from the probe: a successful **file** rename returns **-1**
with `IoErr 0`; a missing source returns **0** with `IoErr 205`; renaming
**onto an existing name** returns **0** with `IoErr 203`. So the docs are
right and the first version of this article was wrong.

The trap is not the sign, it is **`203`**: a displaced lock directory left
from an earlier run makes the next displacement fail, so a fixed `.dead`
name makes a stale claim unrecoverable for the rest of the session.

And the sign is *still* not a safe test: after fixing the name, a
**directory** rename came back **0 while still having moved the
directory** — the same value as failure. A correct-looking
`if (Rename(...) != 0) fail;` therefore reported "displace failed" on a
displacement that had worked, and the caller answered *busy* for a claim
it had just taken.

The robust shape, which survives both: **never test the return value.**
Attempt the displacement with a unique destination, then let the
platform's **atomic** primitive verify it (`CreateDir` succeeding is the
proof that the name is free and ours). Retry loops are worse than useless
here — a competitor that wins between your rename and your `CreateDir` gets
its brand-new claim renamed away on your next iteration. One attempt, then
`CreateDir` decides, and losing that race is *busy*.

This SDK also has **no `RemoveDir`**, so a displaced lock directory cannot
be deleted from a program at all — keep displaced copies few, uniquely
named, and obviously dead.

## 4. Don't test existence by reading — and don't trust a single observation

The original code asked `Examine()` whether the heartbeat existed, on the
theory that it "answers 'here it is' for a missing name". **That theory
was never tested** and is withdrawn; the symptom it explained was
`ERROR_LOCKED` (fact 2), i.e. a *locked* file is not a *missing* one.

The lesson that survives: a failed read cannot distinguish "absent" from
"locked", so use a **create attempt** as the existence test (it fails when
a live holder has the file open, succeeds when nobody does).

**Both RE-TESTs were re-run on a clean machine and both were wrong** — the
reviewer's reading of each was the right one:

| RE-TEST claim | clean-machine result |
|---|---|
| "`Lock()` on a missing name blocks indefinitely" | **wrong** — `lock=NULL, IoErr=205`, returns at once (the probe stayed alive) |
| "a directory rename returns 0 but still moves it" | **wrong** — dir rename to a fresh name: `ret=-1, IoErr=0`; the source was re-creatable, i.e. it *had* moved, with the documented success value |

The earlier "0 but moved" reading has an innocent explanation: the
destination `.dead-<stamp>-<seq>` already existed from an earlier run, so
the rename **failed** — and `IoErr()` was read after intervening calls, so
it showed 0 instead of 203. The displacement had not happened; the code
that reported "displace failed" was right and its caller was wrong to
conclude otherwise.

Neither answer changes the code: it ignores `Rename`'s return value and
lets `CreateDir` decide.

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

| heartbeat | meaning | answer | reachable? |
|---|---|---|---|
| absent | nobody is heartbeating | **take over** | yes — clean exit, or RAM: wipe |
| present, unreadable (202) | held open by somebody | **busy** | yes — the live-holder case |
| present, fresh | a live holder | **busy** | yes (only when unlocked) |
| present, older than the window | crashed or wedged holder | **take over** | **host only, and on AROS probably never** |

### The hole, and what closed it

Because the age was only ever read from the locked heartbeat, a wedged or
crashed holder locked its own age away and the only recovery was a reboot.
The lane measurement that opened the way out: **a claim directory can be
renamed while the file inside it is held open** (`Rename ret=-1, IoErr=0`,
source immediately re-creatable). So the takeover was always physically
possible; only the *age* was unreachable.

The fix is a second file. `hb` stays the liveness lock (opened, never
closed while held), and `hb.age` is a plain stamp, rewritten on every
refresh and **never held open**:

- live instance → `hb` is locked → `busy`, structurally, with no age
  arithmetic at all (the common path, and the cheap one);
- wedged instance → `hb` still locked, but `hb.age` stops moving → once it
  passes `RI_CLAIM_TAKEOVER_SEC` (twice the staleness window, so several
  missed refreshes at 10 s apart) the contender takes over, renaming the
  directory out of the way and re-creating it.

Measured on the Dell with a real wedger (heartbeat held open, stamp 130 s
old): `acquire → rc=0 (locked, but the stamp is 130 s old -> takeover)`.
The same probe had reported `busy` before this change, with the same state.

The threshold is pinned by a test at the *margin*, not just past it: a
stamp 40 s old must still be busy. That margin is what stops a contender
that read the stamp microseconds before a refresh from stealing a live
claim, and a mutant that halves the threshold dies on exactly that
assertion.

## 8. Two more defects the two-file change introduced (review, round four)

Both were found by reading the code against the new design rather than by
a probe, and both are the kind that only bite under concurrency:

**A live instance was evictable while a requester was up.** The main loop
stops running inside `EasyRequestArgs` and `AslRequest`, so the stamp stops
moving while the instance is perfectly healthy — and after
`RI_CLAIM_TAKEOVER_SEC` a second start would take it over, leaving two
writers on the prefs. Before the two-file design this was impossible,
because "the heartbeat is locked" *was* the busy answer. The fix is a
marker the holder stamps **before** it blocks: a modal deadline, past which
no takeover happens. The honest limit: "alive but blocked" and "wedged" are
indistinguishable from outside a process here, so the holder's own marker is
the only signal available — which is why `scripts/ri_audit.sh` now fails the
build when a blocking UI call appears in `app/` without one. A guard whose
coverage depends on remembering is not a guard.

**Loss detection was dead.** "Write by path" is not ownership. After a
takeover, the displaced instance's refresh wrote `hb.age` straight into the
**new** owner's directory, the write succeeded, refresh returned success —
and both instances went on believing they held the claim. The stamp now
names its holder (task address + acquire stamp) and every refresh reads it
back first; a stamp that is not ours returns a distinct code
(`RI_CLAIM_REFRESH_LOST`) so the caller can **stop writing prefs** rather
than merely log. The PAL clears its own held state on that path too: a
backend that knows it lost must stop acting like a holder.

Each got a mutant: skipping the token check, and ignoring the modal
deadline, both die on their own assertions.

## 9. Three "AROS does X" findings that were all caller bugs

Worth keeping together, because the pattern is the lesson and the
individual facts were worth nothing:

| claimed | reality |
|---|---|
| "`Lock()` on a missing name blocks indefinitely" | returns `NULL`/205 at once; the hang was elsewhere |
| "`Rename` returns FALSE on success" | it returns DOSTRUE(-1); the code had compared against the wrong convention and read `IoErr()` after other calls had overwritten it |
| "`Seek()` does not reposition a buffered handle" | two vocabularies: dos.library is `OFFSET_BEGINNING/CURRENT/END` = **-1/0/1**, stdio's `SEEK_SET/CUR/END` = **0/1/2**. `Seek(fh, 0, SEEK_SET)` was `OFFSET_CURRENT` — zero bytes from here — so every write appended |

Each one was recorded as a property of the platform because the symptom was
real, the probe was on a machine that was already misbehaving, and nobody
opened the header. The rule now attached to every future "AROS does X":
**rule out the caller first** — constants from the wrong header,
return-value conventions, a NULL lock where a lock was expected, an
`IoErr()` read too late. Three of three were dull and local.

`scripts/ri_audit.sh` now fails on `SEEK_SET`/`SEEK_CUR`/`SEEK_END` in any
file that calls dos.library `Seek()`, so the third one cannot recur
unnoticed.

## 8. Two rounds of correction, from review and from the machine

Round one (machine): two claims retracted by raw-value probes, after the
host-side review pointed at the pattern rather than the code.
Round two (review): the reviewer's own inference was wrong in the
*reviewer's* favour — "the unreadable path is the busy path, so the
refusal was luck" — because that path is exactly what a live holder
produces. Being reviewed is not the same as being corrected, and the fix
still had to be proved by a probe rather than by an argument either way.
Round three (review): a crash-path hole nobody had asked about, and two
single-observation facts that needed re-testing on a clean machine — both
of which the re-tests then refuted, in the reviewer's favour. The review's
*inference* about the crash path ("a crashed instance keeps hb locked, so
the takeover is unreachable") was also correct, and both RE-TESTs it asked
for refuted my claims.

**Attribution correction (2026-10-18): the two-file design — `hb` as the
liveness lock, `hb.age` as an unlocked stamp — is mine, not the review's.**
The review said the age branch looked dead on AROS and asked for a test of
the crashed-holder case; the question "if the rename works, is an age-based
takeover reachable at all?" was written into this article by me, and the
split into two files is what I then built in response to my own question.
I attributed the design to the review in the ledger and in a report; that
was wrong, and an idea borrowed from a reviewer is worth less in the record
than one that was actually reasoned out.

The transferable lesson: **the number of independent samples behind a
claim is part of the claim.** A fact recorded from one observation on a
machine that was already misbehaving deserves the same RE-TEST marker as
the two that were outright wrong.

## 9. Earlier section, kept for the argument

An owner-side review of this work caught three real defects that the lane
had not: the clock expression was not seconds and went *backwards* every
minute (so a 0.6 s old heartbeat read as stale and a live claim could be
taken over), the takeover loop could rename away a competitor's fresh
claim, and the existence probe leaked a handle. Two of them were invisible
to the passing tests because the tests used the same wrong shape.

The third defect had a character worth naming: the reviewer's inference
that "the refusal must have come from the unreadable path, so the refusal
was luck" was **wrong in the reviewer's favour** — the unreadable path is
exactly what a live holder produces, structurally (fact 2). Being
reviewed is not the same as being corrected, and the fix still had to be
proved by a probe rather than by an argument either way.