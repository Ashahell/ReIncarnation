# MIDI clock out and MMC in: a schedule that cannot drift, and a proof tool that would not start (2026-10-09 – 2026-10-10)

- Source: ReIncarnation session (OpenCode lane), 2026-10-09 – 2026-10-10. Plan `docs/superpowers/plans/2026-10-08-levi-wiring-and-midi-opencode-prompt.md`; 11 commits tagged `[levi-midi]`, `e3ec539`…`36607db`.
- Collected: 2026-10-10
- Published: 2026-10-10
- Evidence: `docs/evidence/midi/ledger.md` (§M5a–§M5e2, the E0 default of every law); tests `t185_midi_clockout`, `t186_midi_out`, `t187_midi_mmc`, `t188_midi_out_pump`, `t189_live_driver_clkout`; `docs/evidence/lane/2026-10-09-dell-midirq-software-failure.png`.
- Prior: [2026-10-09-levi-wiring-and-midi-w0-w4-m0-m4.md](2026-10-09-levi-wiring-and-midi-w0-w4-m0-m4.md) (the same plan's W and M0–M4), [2026-10-02-midi-interop-r1-follower-core-wire-parser-sync-source.md](2026-10-02-midi-interop-r1-follower-core-wire-parser-sync-source.md) (the follower core this wires), [2026-09-26-gui-remote-midi-g7.md](2026-09-26-gui-remote-midi-g7.md) (the only receiver, until M4 gave it a sibling)
- Related: `docs/superpowers/specs/2026-09-29-interop-requirement.md` (R2/R3/R4), `docs/2026-09-24-improvement-todo.md` (the MIDI section)

## What this slice is

**M5: the other direction.** M0–M4 made RIAPP *listen* — CAMD up to date, MIDI clock in, a second device on its own channel. M5 makes it *speak*: 24 ppqn clock out, MMC in, both off by default, plus the proof instrument that was supposed to measure the wire.

Five phases landed and are green, mutation-checked and audited. **The sixth thing — the proof — does not work, and the article's second half is about why.** That split is deliberate: the schedule is the durable result, the proof tool is the scar tissue.

## The design decision everything else follows from

**The tick count at an absolute sample position is a pure function of that position, not an accumulator** (`midi_io/midi_clockout.h`):

```
ticks(n) = base + floor(((n - anchor) + lead) * ppq * bpm_milli / (sr * 60000))
```

Nothing accumulates, so there is no residual to carry and no second rounding to disagree with. This is the whole answer to "can 24 ppqn drift over a long take": **it cannot**, because there is nothing to drift. 48 kHz at 140 BPM is 857.14 samples per tick — precisely the case an accumulator gets wrong, and the case the tests walk sample by sample for ten minutes: exactly **33600 ticks**, every interval 857 or 858, **both values occurring** (a schedule that only ever emitted 857 would drift a tick per second; one that only ever emitted 858 would run fast).

Two laws ride along:

- **The tempo change re-anchors** at the change position and keeps the tick count already reached, so emitted ticks keep their sample positions. This is **M3f's law, applied to the output side before it bit**. Mutant W is the M3 bug verbatim — divide by the new rate without re-anchoring, and the past is re-timed. The owner's ear had already reported that as "playback speed suffers".
- **The lead is the output latency, and the clock LEADS by it.** Lagging would deliver the tick after the sound it describes has left the device. The tick is due *at* `anchor + k*T − lead`, not one sample later — a boundary I got wrong in the test first and had to correct rather than "fix" in the code.

Tempo is milli-BPM so the render path does no float, and `midi_io` speaks the wire's own **24 ppqn**, never the app's 96. That is M3b's "one authority each" again, and it is the reason there is no PPQ argument to have.

## Off by default, and what "off" has to mean

Twice in this slice the E0 needed a sharper reading, and both sharpenings are laws now:

- **"Off" means silent on the wire.** A disabled producer queues *no byte at all* — not a transport edge, not a clock. A receiver is not obliged to ignore anything, so a feature that is "not acted on locally" is not off.
- **A zero PPQ stays zero.** `init` deliberately does **not** default it to 24: a silently-correct answer hides the caller getting the wire constant wrong. Zero rate, zero tempo and zero PPQ all fail closed.

MMC follows the same posture, with **Live neither sending nor receiving MMC** (spec §0.9) recorded in the ledger — so an MMC proof against Live would prove nothing about the feature.

## One path, not two

MMC emits **the follower's intents** (`RI_FOLLOW_PLAY_START`, `_STOP`, `_SEEK`) rather than a parallel MMC set. A master sending MIDI clock and a master sending MMC therefore drive one path, and there is one Locate law instead of two.

And that Locate law is where M3b's ghost lives: **`LOCATE` is 16-bit big-endian MIDI beats = sixteenths**, the unit SPP carries. Mutant AB is the little-endian reading, which is M3b's SPP bug exactly — a seek to the wrong bar, with nothing on the wire to say so.

The rest of MMC in is framing, which is where the bugs were: SysEx is variable length and the existing parser wanted six bytes at once, so the accumulator stores the **leading `F0`** and its buffer is exactly what `midi_mmc_cmd` expects — one truth about the command bytes. An `F0` inside an `F0` **restarts** the frame (mutant AA), because nesting lets one truncated frame swallow every later command.

## The ring, and the two ways it was wrong

The outbound byte ring was implemented twice before it was right, and both wrong versions were caught by the test rather than by review:

- **Masked indices make `head − tail` mean two different things** — empty and full are the *same value* — so a full ring **silently overwrote** instead of dropping, and `dropped` stayed 0 while the oldest bytes were eaten.
- **Unmasked subtraction reads `4294967295`** the moment the reader overtakes the writer.

Monotonic counters cost one reserved slot and make occupancy exactly `head − tail`. The clock emit is likewise **cumulative, not a per-call delta**: a skipped call then emits everything it owed instead of quietly losing clock bytes, because a clock that silently loses bytes is a take that silently drifts.

**A 3-byte SPP is all-or-nothing** on the producer side *and* on the framing side — and the framing side had the same bug in a different disguise. I tested "is this status byte realtime?" with the high nibble, which treats `0xF0..0xF7` as realtime. **Realtime is `0xF8..0xFF`**; `0xF0..0xF7` is system common and does take data bytes. So SPP was being emitted as a one-byte message **with no position at all** — the half-sent-SPP lie the producer refuses to create, arriving from the other direction. Same class of defect as the M3f tempo collapse: a shared assumption, entered from a second door.

## Mutants, including the two that lived

Nine mutants, seven killed. The two survivors are recorded rather than claimed, because "all mutants killed" is a claim the evidence does not support:

- **AC** (drop the `enabled` guard from `midi_mmc_feed`) **survives** — `finish()` guards independently, so the observable law (no intent while disabled) holds and the second guard is belt-and-braces no test can see from outside.
- **AE** (a new status byte discards the half-built message) **survives for this producer** — clock out emits only 1-byte realtime and 3-byte SPP, so the path AE removes would only matter for a 1- or 2-byte system-common message, and nothing emits one yet. It is kept for F1/F3 when MMC out lands, and marked uncovered.

**Y is the one worth remembering**, because it was not a survivor at all — it was a *gap*. The first mutant set had a mutant that ignored the enable setting entirely, and it **passed**, because the test only pinned that `init` leaves the producer off. Those are different laws and the setting is read at runtime. The test grew a case for `enable(o, 0)` silencing the wire, and the mutant died. **A mutant that survives is sometimes a statement about the test, not the code.**

## The render's entire involvement is a ring

`RILiveDriver` gained an optional caller-owned `RIMidiOut`. After `ri_live_render` it hands over `session->sample_cursor`; transport commands forward Play and Stop. That is all: **no send, no CAMD**, and the confinement gate keeps AROS out of `live_driver.c`, so the plan's "no CAMD in the render path" is enforced by the build rather than by discipline. `NULL` is the default state, so every existing caller is unchanged.

The clock is fed from the **audio** clock, never a timer — which is what makes "drift over ten minutes" answerable at all. 51200 rendered samples owe exactly 51 ticks at 120 BPM, all `F8`, and 100 further blocks owe exactly 25 more.

**A process failure worth keeping.** That test **passed on its first run**, because the driver was wired before the test was ever compiled, so no RED was observed. Rather than call it done, the hook was reverted and the test re-run to produce a real one — four laws failing — then restored. *A test never seen red has not been shown to have teeth; the only way to know is to remove the thing it tests.*

## The proof tool: four crashes, and what they were worth

`MIDIRX` was the missing half of the proof set. MIDISEND plays a script into a cluster, MIDICLOCK plays a clock into one, and **nothing could listen**. It reports F8 arrival intervals — min, max, mean, spread, per-slice counts — because a count only proves bytes arrived and the question is what a wire does to an exact schedule.

**It has never completed a single run on either lane.** It cost two owner reboots of the Dell and wedged both agents. What the four crashes established, in order:

1. **NULL `CamdBase`** — inline CAMD calls through a library nothing resolved.
2. **NULL `TimerBase`** — declared by copying `app/stepproof.c` **without its assignment**. Having just fixed a NULL library base in the same file, I had copied the declaration and left out the resolution. **Declaring an inline base is not resolving it.**
3. **A v11 binary on the ABIv1 lane.** riqemu1 is ABIv1; `ri_build_v11.sh` builds ABIv11. It died in the C runtime's startup —

   ```
   Error: 0x80000003 - Illegal address access
   Function Exec_49_FindTask (0x1AA4F80) Offset 0xA
   Stack: MIDIRX __startup_fromwb -> __startup_entry -> CallEntry
   ```

   — **at a byte-identical PC across three attempts**, before `main` ever ran. That identity is what identified it: the same crash on two very different guests is not a code defect, it is a *build* defect. Fixed by giving the tool a v1 target as well.
4. **`Exec_77_SendIO` from `main`, `RDX = 0x50`** — in a v1 exec library call RDX carries the library base, and `0x50` is not one. An unresolved base again, this time for `OpenLibrary`. The pattern being copied from `app/midiclock.c` (`__TIMER_LIBBASE`, `__CAMD_LIBBASE`) is **v11-only**: midiclock has never been built for v1, so its workingness was never evidence about that lane.

Two lane facts came out of it that are worth more than the tool:

- **The Dell's Software Failure has no Cancel, no OK and no close gadget.** `ui-windows` reports `Software Failure! 355,0 656x768 [no-close]`; Escape and `--ui-click` do nothing, and while it is up the agent still answers `--ping`, `--ui-capture` and `--ui-windows` but **every `--exec` returns rc=1 with no output**. The modal requester owns the screen and the shell behind it cannot run. So a crashing tool takes the lane's exec path with it until an owner reboot, which is an unusually expensive cost for a proof tool.
- **The Dell's crash report renders its own text as garbage**, so nothing there is readable. Every diagnosis above was read from **riqemu1, whose console is legible** — the lane that *cannot* prove the measurement was the lane that could prove the fault, and the two are different questions.

## The recommendation this leaves

**Retire MIDIRX and take the intervals from inside RIAPP.** The sender task already counts what it handed to CAMD (`ri_pal_midi_sent()`), and the producer is portable and host-tested, so an interval histogram over the emitted ticks is ordinary TDD in code that has been green all along. It needs **no new AROS-only tool**, and therefore no new unresolved library base — which is where four of the five failures in this slice came from.

The lesson generalises past this tool: **on AROS, an inline library base is a runtime dependency, and a declaration is not a resolution.** Every crash here was a base that existed in source and not at run time, and the cheapest available check — is this binary the right ABI for this guest — was the one that took longest to make.

## Status

**Landed, audited `0/0 PASS`, nothing claimed about the wire:** the drift-free 24 ppqn schedule (t185), the off-by-default outbound producer and its byte ring (t186), MMC in on the follower's intents (t187), stream framing (t188), and the render's hook (t189).

**Superseded on the proof — see [The instrument was the problem](2026-10-10-the-instrument-was-the-problem.md) (2026-10-10).** MIDIRX was retired and its four faults corrected (I had misdiagnosed all four, the important one being that **it stamped its own poll drain rather than the arrival**). The schedule is now measured on **both** lanes: `mean_us=17856` (riqemu1) and `17853` (Dell) against a theoretical 17857.14 for 140 BPM at 24 ppqn. MMC out and the clock-out LED are also built.

**Still not done:** the physical USB-MIDI proof (the owner's); R5 (note input for the 303s, 808 and 909), R6, R7, P3, P4.

**AROS upstream, checked eight times across the slice:** camd's newest commits are still `cb8c4c5f3` and `28ec43a51` (2026-10-05), which this repo already carries in both ABI carriages; `workbench/devs/midi` has zero commits since 2021; and the only 2026 USB work is the Raspberry Pi `2708` tree, which neither lane targets.

## See Also

- [Leviasynth wiring W0–W4 and MIDI interop M0–M4](2026-10-09-levi-wiring-and-midi-w0-w4-m0-m4.md) (the same plan's first half; M3's tempo-map collapse is the law M5 applies to the output side)
- [MIDI interop R1: the follower core, the wire parser, the sync source](2026-10-02-midi-interop-r1-follower-core-wire-parser-sync-source.md) (Superseded — M5c wires the intents it left unwired)
- [AHI honest rate proven on the Dell; `avail flush` crashes AROS in a ROM expunge](2026-10-08-ahi-rate-dell-proof-avail-flush-crash-and-ahi-v7-verdict.md) (the same lane week, and the same lesson: a lane result can be the tooling, not the code)
- [The item cull that threw away 98 percent](2026-10-05-the-item-cull-that-threw-away-98-percent.md) ("equivalent in production is not tested" — the family this whole slice belongs to)
