# Note input for the ReBirth voices, and a note that would not stop (2026-10-11)

- Source: ReIncarnation session (OpenCode lane), 2026-10-11. Commits `cf3ad14`, `74614c6`, `4624efb`, tag `[levi-midi]`.
- Evidence: `docs/evidence/midi/ledger.md` (R5a, R5b, R5c), `docs/2026-09-24-improvement-todo.md`, `docs/superpowers/specs/2026-09-29-interop-requirement.md` (R5, P-19).
- Collected: 2026-10-11
- Published: 2026-10-11
- Spec: `docs/superpowers/specs/2026-09-29-interop-requirement.md` (R5, P-19)
- Prior: [2026-10-02-midi-interop-r1-follower-core-wire-parser-sync-source.md](2026-10-02-midi-interop-r1-follower-core-wire-parser-sync-source.md) (the wire this rides), [2026-10-10-midi-clock-out-and-mmc-in.md](2026-10-10-midi-clock-out-and-mmc-in.md) (the outbound half, R6/R6f, whose conventions R5 had to agree with), [2026-10-11-offline-export-stems-mid-and-loop-exact-renders.md](2026-10-11-offline-export-stems-mid-and-loop-exact-renders.md) (the same slice's other half)

## What this slice was

R5 — an external sequencer plays the 303s, 808 and 909 — was the last open item in the interop requirement. The Leviasynth already had note input (channel 2, its own chart, `RI_MCHAN_LEVI`); the four ReBirth devices did not.

**Four owner decisions were taken first, and all four are still the reason the code reads the way it does**: the same shape as R6f/R6e (drums implicit and counted, 303s opt-in), GM percussion layout, a slide is legato from overlap, and build it last — accepted with eyes open that it **cannot be ear-proven**, because neither lane has a USB-MIDI device.

That last one shapes everything below: this is the largest piece of new surface in the plan with **no way to hear it working**. It is host-tested and mutation-tested only.

## Device 0 is 303A, so a zeroed struct is not an empty one

The first version stored the device number directly in `dev_of_ch` with `0xFF` for "unclaimed". **Device 0 is a legal value**, so every zeroed entry reads as "channel bound to 303A" and no channel is ever claimable.

The fix I reached for first — rewriting zeros to `0xFF` inside `bind()` — **cannot work**, and the first test run said exactly why: channel 1 bound successfully, then read back as unclaimed. After binding 303A that entry *is* legitimately zero, and the next bind swept it away.

The real fix is the **+1 encoding**: store `dev + 1`, so a zeroed array honestly means "nothing claimed" and a forgotten `init` gives a *silent* module rather than a wrong one.

Then I made the same class of mistake again. I added a `ready` flag, guarded every entry point with it, and set `ready` inside `bind()`'s **drum branch only** — so every melodic bind was refused by its own guard. And when the mutant "drop the guard" **survived**, because with the +1 encoding a zeroed struct already refuses everything, **I deleted the flag rather than manufacturing a test for it.** A second way of saying "nothing is claimed" is a second thing to keep true.

## The engine is driven by events, not by a second note path

R5b's first decision was what a "live note" becomes. `RI_EV_NOTE_ON` already reaches `rb303_note(v, pitch, slide, accent)` for devices 0 and 1 and `rb808_trigger`/`rb909_trigger` for 2 and 3 — so a live note is a **real `RIEvent`**, not a new call into the voices. A second path into the machines would be a second answer to "when does a 303 sound", wrong in a way nothing reports.

**The stamp is the cursor, not "now".** MIDI arrives in a drain *between* audio buffers. An event stamped at the drain's own sample position lands in the gap between buffers, where the cursor will never be — silent. The live tap queues and pumps from `e->cursor`, and `seq` comes from the cursor too, because two notes at one cursor with one `seq` have no defined order under the §8 key, and the engine **stops** on an out-of-order event.

Two placements that are easy to get wrong and are pinned separately:

- **the drum LANE goes in `voice`, not in `value`** — that is how `ev->voice = ev->value = lane` arrives and how the engine reads it. In `value` alone, **every drum is the bass drum**.
- **the live path carries the lane, not the 909 voice** — the engine does `RI_LANE_TO_RB909_VOICE[lane]` itself; doing it here too would be the same map in two places. The two lane orders differ: lane 6 is the 808's low cowbell and the 909's **low ride**.

## A refused note leaves dev at 0, which is 303A

R5a refuses a drum release, an unmapped GM note and an unclaimed channel. On every one of those, `ev->dev` is left at **0** — so a refused drum release, *the note the player is trying to stop*, would have been queued as a 303A strike.

I fixed it with an `ev->resolved` flag, and then **got it wrong again**: set once near the top of the resolver, which is *before* every refusal path. Each success path sets it at its own `return 0` now.

**A single flag set at the top of a function with six exits is a flag that means "I got past the first check."** That is the second time in two phases that a readiness-style flag set in one place and read in another produced a wrong answer; both times the fix was to make the *encoding* carry the fact rather than add a second thing to keep true.

## The note that would not stop

R5b **deliberately refused** to queue a 303 release, and recorded the consequence in its own commit message: **a live 303 note rang on until its envelope finished, every time.** That was left in place rather than hidden, on the reasoning that the alternative — queueing a release as a note-on — would re-strike the note the player is lifting their finger off, which is the most audible thing the module could do.

R5c closes it. The release carries **the pitch that was held**: `rb303_release` takes only the voice, so the pitch is not what makes the sound, but the note tap records `value` and *a release recorded against the wrong pitch is a record nobody can use*. R5a already knew that pitch — it is the same gate state that makes a slide detectable — so throwing it away at the boundary would have been the only place the information was lost.

A drum still never gets a release: it has no gate.

## P-19: the flood cap

The spec named the mechanism — "oldest dropped and counted" — and left the number OPEN. Implemented at **64 pending notes per device**, recommended rather than decided.

- **64 is a little over four bars** of sixteenths (96 to the bar) at any plausible tempo, so a legitimate roll never loses a hit.
- **Per device, not global.** A flood on the 808 must not consume the 303A's budget; otherwise one loud channel silences a melodic one and the symptom is *"my bass line disappeared"*, which nobody can diagnose from a global counter.
- **A fixed depth means a flood cannot grow a heap** — the stall the spec actually names.

**The ring depth and the flood cap are different numbers.** R5b had one constant doing both jobs, and calling it a depth while it silently acted as a rate policy is how it would have been mistuned later. They are now `RI_NOTEIN_RING_CAP` (a **memory** bound) and `RI_NOTEIN_FLOOD_CAP` (a **rate** bound), settable at runtime, with an absurd value **refused rather than clamped** and the previous cap left in place — clamping is a silent policy choice, and zeroing leaves a session with no cap at all.

## Two more counting bugs

**The dropped slot's device budget was not decremented.** The ring removed one of *that device's* notes, so `pending[dev]` had to come down with it. Without that, `pending[dev]` climbs past the cap forever while the ring stays the right size — the cap stops meaning anything after the first overflow.

**An unresolved note was counted against the 909.** I attributed "dropped before any device is known" to `dropped[RI_NOTEIN_DEV_COUNT - 1u]`, which silently incremented the **909's flood counter** for every refusal — reporting a flooded channel nobody had played. It now has its own `dropped_unresolved` slot, and the reason it *cannot* be attributed is the same `dev = 0` fact above: on a refusal, **0 is 303A**, so any device attribution is a guess.

## The mutants

Across R5a–R5c: **34 mutants, 34 killed**, plus one deleted as redundant. The survivors that mattered before the fixes landed:

- **An assertion I wrote as `... || 1`** — a tautology that can never fail, so the 909/808 voice check proved nothing. The replacement names the actual lane orders and the cowbell as the clearest case.
- **A drum-roll test with two hits**, when a roll is three. The module never tracked *drum* gates, so the second hit had nothing to change.
- **A pump test that never advanced the cursor.** `e->cursor` *starts* at 0, so "stamp at the cursor" and "stamp at zero" are the same number until the song has played — and every case pumped at sample 0.
- One mutation that failed to build (`-Werror` catching an orphaned helper) and one that was rewritten so it tested the behaviour rather than the build.

## Open, and it is the honest part

- **R5d — the `midi_drain` wiring.** `note_in_app` produces events and **nothing calls it from the app**. That is mechanical and it is the last piece between this and a device that can hear anything.
- **No lane can prove any of this.** Neither has a USB-MIDI device, so "five devices sound from an external sequencer" is unobservable today. Everything above is host-tested and mutation-tested; none of it is heard.
- **R5 is built on R6e's shape**, which is the owner's decision, not a measurement. If the implicit-drum-channel choice turns out to be wrong in use, it is a one-line change in `ri_notein_bind` — but it will be wrong *quietly*, in the sense that an unclaimed channel is refused rather than defaulted.