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

Two observations below rest on **one sample each, taken on a machine that
already had a Software Failure window up and a wedged process** — the same
situation that produced two claims already retracted here. Both are marked
**RE-TEST** and must be re-run on a clean machine before anybody builds on
them:

- **RE-TEST — "`Lock()` on a missing name blocks indefinitely."** A Lock
  on a missing name is *expected* to return 0/205 at once. A hang points
  at a stuck handler or at a conflict with the crashed process's own lock,
  not at Lock. The symptom was real (a probe never finished and its log
  stayed empty); the attribution is not established.
- **RE-TEST — "a directory rename returns 0 but still moves the
  directory."** One observation. Print the raw return plus `IoErr()` for a
  directory rename onto a **fresh** name.

Ignoring `Rename`'s return value is right either way, so no code depends on
the answer.

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

### The AROS hole in that table (review, 2026-10-18 — to be tested)

Because the age is only consulted when *nothing holds the file*, and a
holder keeps it open for its lifetime, the age branch is only reachable if
a crashed task's handles are released. If AROS does not close them on a
crash (or on a wedge the task never exits from), then:

- a crashed or wedged holder leaves the heartbeat **locked forever**;
- every later start reads "held, unreadable → busy" until the machine
  reboots;
- the only takeover that can happen is the clean-exit leftover directory,
  which by definition has no heartbeat — the "absent" row.

That is acceptable *only because the claim lives on `RAM:`*, so a reboot
clears it — which means the user-facing text must say **reboot**, not
"wait N seconds". It did say the latter; that was wrong and is fixed.

Open question for the clean pass: does `Rename` of a claim directory
succeed while the heartbeat inside it is held open? If it does, an
age-based takeover becomes reachable after all (by reading the age from a
*second, unlocked* file). If it does not, the crash recovery is the
reboot, full stop, and the age branch is host-only code.

## 7. Prove the decision table, not the happy path

The failure modes live in the paths nobody exercises. A scratch probe
(`RAM:CLAIMPROBE`, linked against the real backend) walks the whole table
on the target and prints the decision *and its reason* for each row.

Three probe-authoring lessons, each of which cost a round-trip:

- **A probe that keeps its log open loses everything before a crash** —
  and re-opening a file this process holds is refused anyway (fact 2). One
  short-lived file per line survives crashes *and* matches the platform.
- **A probe cannot arrange a state it is itself standing in.** Renaming a
  file the probe holds open fails; "no live holder" must be arranged after
  releasing the handle, not while holding it.
- **Back-dated fixtures must use the fixed clock's units.** Subtracting
  "30 s" as `30 * TICKS_PER_SECOND` produced an age of 1.1 billion
  seconds and a table that looked broken. Print the age you expect.

## 8. Two rounds of correction, from review and from the machine

Round one (machine): two claims retracted by raw-value probes, after the
host-side review pointed at the pattern rather than the code.
Round two (review): the reviewer's own inference was wrong in the
*reviewer's* favour — "the unreadable path is the busy path, so the
refusal was luck" — because that path is exactly what a live holder
produces. Being reviewed is not the same as being corrected, and the fix
still had to be proved by a probe rather than by an argument either way.
Round three (review): a crash-path hole nobody had asked about, and two
single-observation facts that need re-testing on a clean machine.

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