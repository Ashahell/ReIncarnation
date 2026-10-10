# The instrument was the problem: retiring MIDIRX, and a spread that cannot be measured (2026-10-10)

- Source: ReIncarnation session (OpenCode lane), 2026-10-10. Plan `docs/superpowers/plans/2026-10-08-levi-wiring-and-midi-opencode-prompt.md`; 7 commits tagged `[levi-midi]`, `959d478`…`686d07b`.
- Collected: 2026-10-10
- Published: 2026-10-10
- Evidence: `docs/evidence/midi/ledger.md` (§M5f–§M5j, the E0 default of every law), `docs/evidence/midi/m5-cloop-proof.md`, `docs/evidence/midi/m5-cloop-riqemu1.log`, `docs/evidence/midi/m5-cloop-dell.log` (the raw lane artifacts), tests `t190_midi_interval`, `t191_mmc_out`, `t192_clkout_led`.
- Prior: [2026-10-10-midi-clock-out-and-mmc-in.md](2026-10-10-midi-clock-out-and-mmc-in.md) (the schedule itself and MIDIRX's four crashes), [2026-10-09-levi-wiring-and-midi-w0-w4-m0-m4.md](2026-10-09-levi-wiring-and-midi-w0-w4-m0-m4.md) (the same plan's first half), [2026-10-02-midi-interop-r1-follower-core-wire-parser-sync-source.md](2026-10-02-midi-interop-r1-follower-core-wire-parser-sync-source.md)
- Related: `docs/superpowers/specs/2026-09-29-interop-requirement.md` (R2/R3/R4), `docs/2026-09-24-improvement-todo.md`

## What this slice is

**The second half of M5: the proof, and everything it cost.** The previous article ends with a schedule that cannot drift and a proof tool that has never run. This one retires that tool, measures the schedule on two real machines, and then establishes that **the last remaining question — the spread — is not measurable with this architecture at all.**

## The owner's diagnosis was right and mine was wrong

MIDIRX crashed four times and I misdiagnosed every one. Four faults, all in the poll loop:

1. **`CreateIORequest` builds a request with `io_Device = NULL` and `io_Command = 0`**, and it was never opened on `timer.device`. `SendIO` then dereferences NULL. Four debugging rounds went into a self-inflicted NULL that was visible in the source.
2. **`GetMsg` followed by `WaitIO` removes the same message twice**, corrupting the port's list.
3. **The request was on `UNIT_ECLOCK`**, so its "2 ms" delay was 2000 EClock ticks — about 55 µs at 36 MHz, not 2 ms.
4. **Intervals were stamped when the poll drained the queue, not when bytes arrived.** At 140 BPM, where ticks are ~17.9 ms apart, that quantisation would have been most of the reported spread.

And my register reading was wrong too: on this ABI the library base is the **last C argument**, in `RSI` for `SendIO(io)`. The `RDX = 0x50` I kept pointing at was a symptom of the NULL request, not a missing library base.

**The defect that mattered was the design, and it is the title of this article: the instrument was timing itself.** A measurement instrument that timestamps its own draining is not a measurement instrument.

## The fix was not a better instrument

MIDIRX was deleted. The listener became `MIDISEND ... LISTEN` and `MIDISEND ... CLOCKLOOP`, riding the M2 receiver task that already waits on the CAMD signal and stamps each message with EClock when it wakes. The statistics moved into `midi_io/midi_interval.c` — pure C, host-tested, **and it never reads a clock**, because arrival stamps are the caller's to supply.

That last point is the whole design. It is also the thing that made the next discovery possible.

## The measurement, and what it does not settle

| | mean interval | counts |
|---|---|---|
| riqemu1 (ABIv1) | 17856 µs | 1708 sent / 1707 received / 1 lost |
| Dell (ABIv11) | 17853 µs | 3584 sent / 3583 received / 1 lost |
| theory, 140 BPM at 24 ppqn | 17857.14 µs | — |

**0.04 % and 0.02 % out**, on two machines, two ABIs, one with real-time pacing and one without. The single loss is the last byte in flight when the receiver closes.

**And the spread is the instrument again.** Both lanes report `min_us` of 3–5 against `max_us` of ~40100, and both put about 89 % of their arrivals in the **last** slice. A spread with the same shape on a paced host and an unpaced one is measuring neither: `min_us=3` says several ticks were stamped microseconds apart, which is exactly what a **batch drain** looks like — the receiver task stamps each message when it wakes, so a batch shares a timestamp.

**So per-message wire arrival is not measurable here.** The PAL exposes drain-time stamps, not arrival-time ones. That is a structural limit and is recorded as one, rather than left looking like a run that has not happened yet. The physical USB-MIDI cable remains the owner's proof: Live following RIAPP's clock for five minutes, watching Live's incoming tempo for steadiness.

The same reasoning killed an earlier, worse result. A previous CLOCKLOOP reported `min 20011 / max 40080` on the Dell and `min 20025 / max 40143` on riqemu1 — **the same spread to within fifteen microseconds, on a lane with no real-time pacing and a lane that has it.** A spread that does not move when the host's pacing changes is not measuring the host; it was `send; Delay(1)`. **A measurement that survives every change to the thing it measures is a measurement of itself.**

## Four attempts at a 1 ms timer, and none of them needed

In order, and every one cost a wedged lane:

1. Reusing the request passed to `OpenDevice` — hung.
2. `SetSignal` + `Wait` on an allocated bit — hung, because **this SDK's `struct IORequest` has no `io_Signal` field**, so nothing ever raises that bit and `Wait(sigbit)` is an unconditional hang.
3. A fresh request per iteration — `SendIO` reached `Exec_77_SendIO` with **`RSI = 0`**, and faulted on its first dereference (`mov -0x28(%rsi),%rax`).
4. `Delay(1)` — works.

The lesson is not that the fourth was right. **It is that none of it was required:** `ReadEClock` needs the timer's library base and its frequency, both of which come from the `OpenDevice` handshake, not from a running timer stream. There is now no `SendIO` in the loop at all.

**How any of it was found, since every symptom lied.** No crash requester on the Dell; the agent simply never finished. And **three of the first four attempts produced no output at all** — not even setup prints that had certainly executed — because the lane agent captures script output through a pipe, so a child's stdout is block-buffered and nothing flushes while the tool is hung. A tool that only speaks on exit says nothing when it does not exit. Progress now goes to a file through dos `Write()`, which is exactly why MIDIRX wrote `MIDIRX.LOG`.

The bisect was the only thing that worked: replacing the timer wait with `Delay(1)` — known-good — made the loop complete, placing the fault in the wait and clearing the producer, the EClock read and the pump in one run.

## riqemu1 is the right lane to diagnose on and the wrong lane to measure on

Its console is legible, where the Dell renders its own crash text as garbage; and it reproduces a Dell hang **without costing an owner reboot**. That is now the standing rule, and it is the same asymmetry as MIDIRX: the lane that can prove the fault is the one that cannot prove the answer.

## A lane fact that cost three reboots, and it was mine

**The Dell runs `-no-reboot`; riqemu1 does not.** So `system_reset` on the Dell makes **QEMU exit** rather than rebooting the guest. Every "recovery" performed on that lane **destroyed the VM**, and I twice reported it as needing an owner reboot while my own command was the cause — then read the owner's correct fix as evidence for my theory. The Dell is recovered by **relaunching QEMU**, and the flags are worth checking before assuming one lane's recovery is another's.

The "identity has been claimed by another connection" message was the same story seen from inside: the guest agent was crash-looping, each new instance racing the server's still-held registration, four `ATCPBIN` windows accumulated, and a fresh boot cleared them. One session and one connection throughout — the server was never at fault.

## Three checks that cannot fail

**A stale binary, staged three times.** The v11 MIDIRX-adjacent build on disk was 61768 B while the working v1 build was 66408 B — still carrying the hanging timer code. Caught by comparing sizes *before* the put.

**`sent` reported 0 while 1700 bytes arrived**, and `lost` is computed from it, so every run had been silently reporting `lost=0`. A check that cannot fail is not a check. Two independent sources now exist for that number and are compared at run end.

**A guard that checked a derived copy against itself.** `ri_ctlreg_find` bounded its scan with hand-written `RI_CTLREG_SEC_LO/HI` literal arrays. Adding one control row invalidated them and **six unrelated pattern sections silently resolved the wrong rows** — caught only because a draw-hash pin moved, since a window shifted by one still *contains* its own rows. `t170`'s header claimed those literals were "VERIFIED AGAINST THE TABLE by t170"; `t170` derives each range from the table and never compares it to them. **This is the third time in this slice that a comment asserted a property the code did not have** — and the fourth, counting a comment reading "WaitIO removes it too, so not both" directly above code that did both. The literals are deleted; the bounds are computed from the table, so there is nothing left to keep in sync.

## MMC out, and the lamp that had to lie

**MMC out is on its own E0**, not the clock-out switch: driving another machine's transport is a bigger consequence than sending it a clock, and one switch for both makes the safe choice inexpressible. Its law is the **round trip** — t191 emits a locate and feeds the bytes back through the *in* path, which is the only way to catch a unit mismatch between a writer and a reader of one field, and that is M3b's SPP bug arriving from the opposite direction. Inbound intents are suppressed from echoing, or a locate would bounce between two machines.

**The clock-out lamp follows bytes, not the setting.** A lamp wired to "is it enabled" stays green through exactly the failure it exists to show: requested on, sender task refused, producer failed closed, nothing on the wire. It has no timeout, and it is deliberately unreachable from the generic indicator setter.

## Upstream

Checked eight times across the slice. camd's newest remain `cb8c4c5f39` and `28ec43a517` (2026-10-05), both already carried in the v1 and v11 carriages; `workbench/devs/midi` has **zero** commits since 2021's detab. The only USB-side work in 2026 is `arch/arm-native/soc/broadcom/2708/` — the Raspberry Pi tree, not either lane's x86-64 target. **Nothing to carry.**

## Status

**Landed, audited `0/0 PASS`:** the schedule measured on both lanes to within 0.04 %; counts exact on both; MMC out; the clock-out lamp.

**Known unmeasurable here, not merely unmeasured:** per-message wire-arrival spread. It needs arrival timestamps from camd itself, which this PAL does not expose.

**Not done:** the physical USB-MIDI proof (the owner's); the lamp's on-panel position, placed by arithmetic and awaiting an eye; R5 (note input for the 303s, 808 and 909), R6, R7, P3, P4.

## See Also

- [MIDI clock out and MMC in: a schedule that cannot drift, and a proof tool that would not start](2026-10-10-midi-clock-out-and-mmc-in.md) (the first half; MIDIRX's four crashes, diagnosed wrongly there and corrected here)
- [Leviasynth wiring W0–W4 and MIDI interop M0–M4](2026-10-09-levi-wiring-and-midi-w0-w4-m0-m4.md) (the same plan's earlier phases)
- [The item cull that threw away 98 percent](2026-10-05-the-item-cull-that-threw-away-98-percent.md) ("equivalent in production is not tested" — the family every finding here belongs to)
- [AHI honest rate proven on the Dell; `avail flush` crashes AROS in a ROM expunge](2026-10-08-ahi-rate-dell-proof-avail-flush-crash-and-ahi-v7-verdict.md) (the same lanes, and the same lesson: a lane result can be the tooling, not the code)