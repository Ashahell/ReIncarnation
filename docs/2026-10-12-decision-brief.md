# Decision brief — everything waiting on the owner

**As of 2026-10-12. NOT FINAL — three errors corrected by the owner, below.**

Written because the work has run out of things that are mine to decide. Each
item says what it is, why it is blocked, and what I recommend. Nothing below
is started until you say.

**What the owner corrected, 2026-10-12:**

1. **The M4 ear test does NOT need a USB-MIDI device.** I said it did. Wrong.
   The M4 proof already drives the Leviasynth with
   `MIDISEND riapp RAM:m4-levi.mid.txt` **on the Dell itself** — CAMD into
   CAMD on the same machine, not a cable — and both sides are already staged
   in `RAM:`. R5 is heard the same way. The only real blocker is that both
   machines are unresponsive.
2. **The Link recommendation dodged the decision it was supposed to settle.**
   "Off-by-default build flag" says nothing about whether we are *allowed* to
   link the SDK, and the repo has **no LICENSE file at all**, which means all
   rights reserved.
3. **"pending" was stale** — `ac4cd7c` is committed and pushed.

**Items 3 (P4) and 5 (sync-in) stand as written.** Item 7 is new.

---

## 1. Interop decision 5 — Ableton Link: DEFERRED WITH P4

**Correction.** My recommendation was "off-by-default build flag". **That
dodged the actual question.** A build flag says when to compile something; it
says nothing about whether we may *link* it. The licensing position:

**THE REPO IS PUBLIC AND HAS NO LICENSE FILE, WHICH MEANS "ALL RIGHTS
RESERVED".** Two consequences, and the second is worse than the first:

1. **We cannot combine the GPLv2+ Link SDK with this tree** until a licence is
   chosen. Linking GPLv2+ code into a closed distribution is not a
   configuration choice.
2. **Outsiders cannot legally reuse any of it.** Public on GitHub is not the
   same as licensed: nobody can fork it, vendor it, or contribute back into a
   project of their own.

**Link also needs a working network stack**, and the interop spec said to do it
on Windows/macOS first — and the port is "not now" (item 3), so Link has
nothing to run on either.

**Recommendation: defer Link along with P4.** It is blocked on two independent
things, a licence we have not chosen and a platform we are not building.
Deferring costs nothing, because nothing in the tree assumes it.

**The licence question behind this is item 7, and it is worth deciding soon
whether or not Link ever happens.**

---

## 2. Interop decision 6 — plugin builds (VST3 / AU)

**What it is.** Loading the ReBirth voices as a plugin inside a host.

**Blocked on.** The same class of decision, plus a much larger one: a plugin is
a shipping artefact with a support surface, not a library you can drop anywhere.

**Recommendation.** **Decline for now.** A plugin needs a stable, documented
parameter surface, and ours is still moving — the R5 inbound note map was
settled yesterday. Building a plugin against a moving surface produces an
artefact nobody can support. The stems work (R8a–R8f, including the FX buses)
already covers the real DAW workflow for the same songs.

**Cost if deferred.** None. If it is wanted later, the stem export is the
fallback in the meantime.

---

## 3. Interop decision 7 — whether to spend 2–3 weeks on P4 now

**What it is.** The Windows/macOS port: WASAPI and WinMM audio, CoreAudio,
CoreMIDI on the input side.

**Correction first.** I previously wrote that P4 *"can be verified without a
lane"*. **That was wrong** and the owner was right to reject it. There is no
Windows cross-compiler and no Wine on this host, no macOS toolchain and no Mac
at all. Even with a cross-compiler, those backends only prove themselves on
real hardware — a null-host stub compiling is not a working driver. Retracted
in the ledger too.

**Recommendation.** **Not now.** The honest reason is that it would produce
code no one here can hear. R10 and the remaining P3-adjacent work are
verifiable on this machine today; P4 is not verifiable on any machine available
to us. Spending the estimate on something that cannot be tested is the
expensive kind of deferred.

**Cost if deferred.** The portability story stays "builds everywhere, proven on
one". That is already stated in the portability evidence and is honest.

---

## 4. The M4 ear test

**What it is.** The owner's ears on the M4 changes — the thing most of this
work exists to enable.

**Blocked on.** A lane. The combined **M4 + R6 + lamp** proof has been blocked
for many sessions, and both lanes are currently unresponsive.

**NO USB-MIDI DEVICE IS INVOLVED — that was my error, and the owner caught
it.** The M4 proof drives the Leviasynth with
`MIDISEND riapp RAM:m4-levi.mid.txt` **on the Dell itself**: CAMD into CAMD
on one machine, not a cable, with both sides already staged in `RAM:`. R5 is
heard the same way, with a script on channel 10 (the 808, GM percussion,
implicit) or on the 303 channel. **The only real blocker is that both
machines are unresponsive.**

**What the owner has to do, and it is a small thing:**

- **reboot the Dell**
- **`system_reset` riqemu1**, and its screen must come back at **1280x1024**
  — GRUB is pinned to it, and a wrong resolution fails *silently*, so verify
  with a monitor `screendump`

**After that the test is about ten minutes of listening.** One command does the
whole sequence — see the runbook at the end.

**Recommendation.** Keep it parked. Nothing I can do moves it forward, and a
queue that pretends otherwise wastes effort.

**When a lane returns**, the run is: deploy hashed ABIv11 binaries to `RAM:`,
read `LEVI chan remote=1 levi=2 devices=2`, `DEV out=1 ch=1 prog=38`, and
`RAM:R6LISTEN.LOG`. **Read `s_notein_played` / `s_notein_refused` first** —
those say whether notes were queued at all, which tells you where a failure is
before you listen for it. Suspect AROSTCP: real IPv4 multicast landed
upstream and our spooler transport runs over it.

---

## 5. MIDI sync-in at 140 BPM — still parked

**What it is.** Following an external MIDI clock so the sequencer tracks a
DAW's tempo.

**Why it is parked.** The Dell's CAMD delivers clock in **10 ms batches**, so
following is only *proven* there at about **60 BPM**. At 140 BPM the batch is
a large fraction of a beat. This needs real hardware measurement at tempo
before it can be claimed, and it is the same hardware that is currently
unreachable.

**Recommendation.** Keep parked, and when a lane returns, **measure before
optimising**. If the batching is the limit, the fix is a look-ahead buffer,
which is a real change and should not be built on a guess.

---

## 6. The record on R5 — corrected, and one thing you should know

You flagged that the ledger still said *"R5 — still needs an owner priority
call"* while R5a–R5e had landed. Corrected in the ledger. Stating it plainly
rather than tidying it away:

**There is no recorded "start R5" call, and R5 was built anyway.**

What the record contains is four **design** decisions from you on 2026-10-11,
and the fourth of them is *"build it last"* — a scheduling instruction about
order, not an instruction to begin. Each slice was then built against those
decisions, with further conventions at R5b–R5e.

So: you engaged with R5's design in more depth than most phases get, and the
slices followed from that — but **"the owner answered the design questions" is
not "the owner approved the phase"**, and the ledger should not have implied it
was. Nothing suggests you objected, and the ear-proof limitation was
understood before the work started. But if R5 was not meant to be prioritised,
say so and I will treat it as the record being incomplete rather than the work
being unwanted — and it is worth knowing either way.

One related question is still genuinely open and is a **one-constant change**:
with two drum devices and one GM channel, the 909 is currently opt-in via
`in909_ch`. Your alternative — both answer channel 10, with a switch choosing
the device — is equally defensible and I have a recommendation ready when you
want to hear it.

---

## 7. The repo licence — NEW, and the one worth deciding soon

**What it is.** The tree is public on GitHub with no `LICENSE` file. That is
not "open source"; it is the default of all rights reserved.

**Why it matters now, and not only for Link:**

- **Nobody can legally reuse any of it.** No fork, no vendoring, no
  contribution back into someone else's project. For a project this size that
  is a real ceiling on who touches it.
- **It gates Link** (item 1) and the **plugin builds** (item 2).
- **It makes "contributions welcome" untrue**, if anything in the docs says
  that. Worth checking before it is said again.

**Recommendation.** I am not going to recommend a licence — that is the
owner's call and it carries consequences I should not guess at. What I can say
is that **it is cheap now and expensive later**: while it is open, the tree
grows and every third-party question becomes "and what about this file". A
decision taken with the whole thing in view is easier than one taken file by
file.

**One question that would unblock it:** is the intent (a) contributors may use
and redistribute it, (b) it stays closed and public is only for browsing, or
(c) something between? Everything else follows from that answer.

---

## Not on this list, deliberately

- **The clamp ruling** — settled, implemented and **pushed**: `ac4cd7c` for the
  float option, `629a07c` for the ruling and the stems-pointer fix. Float is
  not clamped, 16/24 clamp, non-finite becomes silence and is counted.
- **R5 — approved after the fact** (owner, 2026-10-12). See the ledger's "THE
  RECORD ON APPROVAL".
- **P3's option line** — complete. 44.1/48, 16/24 and 32-bit float.
- **MIDIRX** — retired; wire spread is unmeasurable on this architecture.

## Runbook: the proof is one command

```
scripts/lane_proof.sh            # build, hash, PUT, run, report
scripts/lane_proof.sh --stage-only   # build + hash + PUT, then stop
scripts/lane_proof.sh --riqemu1      # the private lane (ABIv1) instead
```

It deploys the right ABI for the lane, prints the sha256 of everything it
stages, PUTs to `RAM:` (never `avail flush`, never `quit` to a monitor), runs
`RAM:LANE-PROOF.RUN`, fetches the logs back, and **leads with the counters**.

**Read `NOTEIN played=` / `refused=` before listening.** Those separate three
faults that all present as "it is quiet":

| counters | what it means |
|---|---|
| `played>0 refused=0` | binding is right; the fault is downstream |
| `refused>0` | messages ARE arriving and the **map** is wrong |
| `played=0 refused=0` | **nothing is arriving** — cluster or receiver fault |

No amount of listening tells those apart, and the last two mean no sound is
coming however long anyone waits.

---

## My recommendation for what happens next

**Stop.** There is no next item that is mine to start: the queue is blocked on
hardware, on a licence, or on you. Starting P4 in particular would spend weeks
producing code nobody here can hear, which is what you have just told me not to
do.

The one thing I would flag as worth doing while idle is **nothing**. The tree
is clean, `AUDIT 0/0 PASS`, and the known gaps are recorded rather than
guessed at.
