# Decision brief — everything waiting on the owner

**As of 2026-10-12.** Written because the work has run out of things that are
mine to decide. Each item says what it is, why it is blocked, and what I
recommend. Nothing below is started until you say.

---

## 1. Interop decision 5 — Ableton Link

**What it is.** Clock sync over Link so ReIncarnation can sit beside a DAW on a
shared timeline.

**Blocked on.** A licence decision. Link's SDK terms decide whether this is a
dependency, an optional build flag, or not something to ship. That is not a
technical question and I should not answer it.

**Recommendation.** Make it an **off-by-default build flag** (`RI_LINK=1`).
It is the only sync option that reaches a DAW the user already has open, but
it is also the only one that puts a third party's licence into the build.
Off by default keeps the licence question out of every existing build, and
Link is worthless without a peer on the network anyway — so a flag that is
rarely on is not costing anything.

**Cost if deferred.** None until you want it. Nothing in the tree assumes it.

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

**Also blocked even with a lane.** There is **no USB-MIDI device on the Dell**,
so R5 is unhearable regardless: an ear test of "five devices sound from an
external sequencer" needs something to send MIDI from.

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

## Not on this list, deliberately

- **The clamp ruling** — settled and implemented (`ac4cd7c` pending): float is
  not clamped, 16/24 clamp, non-finite becomes silence and is counted.
- **P3's option line** — complete. 44.1/48, 16/24 and 32-bit float.
- **MIDIRX** — retired; wire spread is unmeasurable on this architecture.

## My recommendation for what happens next

**Stop.** There is no next item that is mine to start: the queue is blocked on
hardware, on a licence, or on you. Starting P4 in particular would spend weeks
producing code nobody here can hear, which is what you have just told me not to
do.

The one thing I would flag as worth doing while idle is **nothing**. The tree
is clean, `AUDIT 0/0 PASS`, and the known gaps are recorded rather than
guessed at.
