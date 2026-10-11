# The offline export door: stems, SMF, and a loop a DAW can warp (2026-10-11)

- Source: ReIncarnation session (OpenCode lane), 2026-10-11. Commits `a8e4582`, `bf8a11c`, `c507b04`, `8a9c6b9`, tag `[levi-midi]`.
- Evidence: `docs/evidence/midi/ledger.md` (R8e, R8f, R9, R8g sections), `docs/2026-09-24-improvement-todo.md`.
- Collected: 2026-10-11
- Published: 2026-10-11
- Spec: `docs/superpowers/specs/2026-09-29-interop-requirement.md` (R8, R9, P3)
- Prior: [2026-10-02-midi-interop-r1-follower-core-wire-parser-sync-source.md](2026-10-02-midi-interop-r1-follower-core-wire-parser-sync-source.md), [2026-10-10-midi-clock-out-and-mmc-in.md](2026-10-10-midi-clock-out-and-mmc-in.md) (the other half of the interop requirement, and the discipline this slice follows)

## What this slice was

P3 said the offline renderer "writes one WAV per mixer strip, plus FX returns and the master" and that R9 renders "N bars from a chosen bar, with an optional tail, so exported loops warp cleanly in a DAW". Both modules were built and tested. **Neither had a caller.** R8d bridged the engine's tap to the WAV writer and stopped there; `ri_smf_write` had been built since R7a and reached by nothing at all.

This slice is the door. `--stems DIR`, `--mid FILE`, `--bars N --from BAR --tail MS`, all on `tools/render`, all one invocation.

## The engine bug the CLI found, and the class it belongs to

`--stems` produced a **structurally valid, structurally perfect, completely silent** WAV on the first run, while the mix beside it was fine:

```
mix.wav       ch=1 frames=66000 peak=22601
01-303a.wav   ch=2 frames=66000 peak=0
```

Not a crash, not a plausible wrong value: silence, which reads like "the render produced nothing" and sends you looking at the event stream instead of the writer.

`ri_engine_render` restarts its local `done` at 0 on **every call**. R8c threaded `pos = done + c` into the section functions, which is right *within* one call and wrong *across* calls — and the CLI renders in 64-frame blocks, because that is what a bounded tool does. Every block overwrote the head of the stem buffer, so only the **last** block's 64 samples survived, and the 303 had decayed to nothing by the end of the song. `pos` is now `e->cursor + c`.

**This was the second time this exact mistake cost a whole session, and the second time the test could not see it** — t204 rendered the whole song in one call, where `done` happens to equal the cursor. A stem buffer is **absolute**: it belongs to the song, not to the call. That is exactly why `out_l[done + c + i]` is right for the caller's buffer and wrong for a stem, and the two now have visibly different sources.

The second instance of the same bug was in **my own tool**, in the same session: `render_stems` passed `slots[0].l`/`slots[0].r` as the engine's output, so the stereo mix overwrote the samples the tap had just recorded, at offset 0, on every block.

## A size probe that was not a size probe

Sizing the SMF buffer, I wrote `need = ri_smf_write(&song, 0, 0)` — reading back the required size, which writers are often documented to do. **This one is not**: it refuses a NULL `out` and a `cap` under 14, so the probe returned 0 and the tool reported *"the writer refused the song"*, pointing at the song when the fault was entirely in the question.

**A size probe that returns the same value as a refusal is not a size probe.** The real bound is the writer's own internal 8192-byte per-track scratch plus 8 bytes of `MTrk` framing — which is also a real limit, now named rather than truncated around: a track past ~2000 events is refused, and the tool says so.

## The map is deliberately partial

The GM→instrument table claims the numbers this build speaks for the instruments the machines actually have. A **complete** GM table would map ~27 numbers onto 12 instruments, and the extra 15 would each be a claim that the sound exists here when it does not — a cowbell played as a clap, **silently**, which is worse than a refusal. So unmapped notes are refused and counted, and the gaps are as much a part of the answer as the entries.

**A slide cannot cross.** SMF 1.0 has no legato pitch change, and both available fakes are audible: note-off plus note-on is the re-attack R6a exists to refuse, and a bare pitch bend is a *different note* on a track that is supposed to be this pattern. It is dropped, counted, and **printed** — an SMF is opened by a human or a DAW, and a silent fidelity loss in a file that looks complete is what nobody notices until mixdown. `RI_SMF_SLIDE` was defined in the header and referenced nowhere in the writer: *"the flag exists" was not "the writer handles it"*.

## Loop-exact, and where the length came from before

The render was sized from `ev[nev-1].sample + g_rate` — **the last event plus a hardcoded one second**. That is a length nobody chose: it lands mid-bar, and a DAW's warp grid has no bar line to sit on. R9 computes the body from the tempo map at its endpoints (**not** `bar_samples × bars`, which is a second answer to ticks-to-samples and disagrees by a sample per bar at some tempos), and the tail is **additive and cannot move the seam**.

Evidence at 120 BPM, where a bar is 96000 samples:

```
--bars 1  ->  body 96000,  32 events
--bars 4 --tail 500  ->  body 384000 (4 bars of 96000), tail 24000
```

**The one-bar render is bit-identical to the first bar of the four-bar render** (max sample difference 0), and the sample delta across each bar boundary is **803, every one identical**, against a within-bar maximum of 2958. A seam that jumped more than the music around it would be the defect R9 exists to prevent, and this one is quieter than an ordinary step transition.

## I was wrong about this twice, and it cost two phases

Multi-stem output was recorded as blocked twice, on the grounds that it "needs a song format with a per-step device — an **engine data-model change**".

That was wrong. `ri_sched_emit_timed` takes a device as an **argument** and emits for one section, so **a section is a property of the step list you hand the emitter, not of an individual step**. `RIStep` needs no new field; the song text format grows a `device=N` line. The engine data model does not change at all. A thing recorded as *impossible* twice is worth re-deriving before it is carried forward a third time.

## Two runs are not one sorted run

Each device's run arrives sorted by the §8 key (`sample, type, device, voice, seq`). The engine walks the array in order and **stops** on an out-of-order event, counting it — so an unsorted merge renders a truncated song and files the rest as a defect.

The merge **aliased its own source**: destination index `i+j` against source index `i`, so every event taken from the second run overwrote an unread event of the first. The symptom was one device with 32 events and the other with **1** — not a crash, and not an obviously wrong song, just a section that had gone nearly silent.

## A module defect the tool was about to blame on itself

`stem_set_add` **appends**, so the set's index is a *running count*, not a section number. Sections 0/2/4 fill set entries **0/1/2**: asking for section 4 is out of range, and with *every* section present it would be merely **mislabelled**, which is worse. This was invisible because every case so far rendered **contiguous** sections, where the two numbers coincide.

`ri_stemr_set_index()` now answers it, and the mapping lives in `stem_render` because section identity is that module's, not the caller's and not the WAV writer's. The initial state is `RI_STEMR_NO_STEM` and not 0, because **0 is a valid set index** — a zeroed struct looks like every section pointing at the first stem's bytes.

Result: `01-303a.wav`, `03-808.wav`, `05-levi.wav`, the gap for 02/04 being the naming law working. The three hold **distinct** instruments (808-vs-Levi max sample difference 23714), which is what proves the index is right.

## The mutants, and what survived

Four rounds, and **the survivors were the most useful part of the slice**:

- **A merge ordering by sample alone survived** because the runs are visited in ascending device order, so "keep taking from the first run" *accidentally* produces device order. Three separate step fixtures then failed to produce a real type tie — the scheduler ties a note-off to the step **after** a rest, so a section releasing where another strikes puts both of the **same** type at the same sample. The law is now tested on constructed runs.
- **The set-index mutant survived twice.** The first fix added a *guard*, and a guard that does not fire kills nothing: my sparse fixture enabled sections 0/2/4 but fed the engine events for 0 and **1**, so it produced one stem and the assertion beneath it never ran.
- **The device tiebreak is genuinely equivalent**, for a reason worth writing down: both runs are sorted by §8 *before* the merge sees them, so for equal samples the order is already total. It is **kept** anyway — the merge's contract is "produces a §8-ordered array", and a function that silently depended on its inputs arriving sorted would be a precondition nobody wrote down.
- **The whole-bars proof survived** and checking it by hand showed it is *reachable*, not redundant: **at 140 BPM a bar is 1714285 samples**, so two bars is 3428571, not a multiple of one. Handing that over as "two bars" is exactly the lie the module exists not to tell.
- **One build failure and one tautology.** `-Werror` caught a mutant that orphaned a helper (a build failure is not a kill), and I had written one voice assertion as `... || 1`, which can never fail.

## Open

- **FX returns** are not exported. The spec's "plus FX returns and the master" is half-done: the master is the mix the tool already writes, the FX returns are not.
- **Nothing in the app asks for stems.** The door is a CLI; the owner's decision (2026-10-10) chose the CLI over the UI, and that stands, but no lane has run these flags.
- **The 808 and 909 have different voice numbers for the same lane** — lane 6 is the 808's low cowbell and the 909's low ride — so any future "one instrument number" shortcut plays the wrong machine's voice. Pinned by t210.