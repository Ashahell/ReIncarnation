# Bounding the damage-box build: 1.9× less GUI work, and no change to the xruns — which is the point (2026-10-02)

- Source: ReIncarnation session, 2026-10-02 (opencode lane; the follow-up to the scripted A,B,B,A)
- Collected: 2026-10-02
- Published: 2026-10-02
- Plan: `docs/superpowers/plans/2026-09-30-leviasynth-fidelity-plan.md` (§P9 interlude)
- Prior: [2026-10-02-dell-scripted-ab-abba-governor-arm-wins-repaint-policy-regresses.md](2026-10-02-dell-scripted-ab-abba-governor-arm-wins-repaint-policy-regresses.md) (this acts on its Fix B finding), [2026-10-02-dell-sticky-log-location-and-size-heuristic-correction.md](2026-10-02-dell-sticky-log-location-and-size-heuristic-correction.md) (the wake metric)
- Commit: `bb1c385` (unpushed at collection). Raw logs: `~/Work/vms/ri-p9/logs/ab-2026-10-02/` (`C1` unbounded, `D2`/`D4` bounded)

## The measurement, and a bug it exposed

The A,B,B,A said `build_dl` was the suspect. It was not a number, so the first change was to make it one: split the box-repaint timer into build and replay+blit.

The reason-split metric shipped the day before had been reporting **nothing** — `box_steps=0/0/0 box_bar=0/0/0 box_other=0/0/0` across all four A,B,B,A runs, in an arm whose entire purpose was to attribute repaints. Two independent failures that cancelled:

```c
d->dmg_why = RI_RSEC_BOX_NONE;   /* cleared ... */
    ...
    int why = d->dmg_why;         /* ... and read here: always NONE */
```

and `NONE` was not one of the three buckets the heartbeat printed. Either bug alone shows zeros; together they look like *"no box repaints happened"*, the one conclusion the run could not have drawn correctly. Fixed by capturing before the clear and reporting `box_none`, with `RI_RSEC_BOX_COUNT` so the printed set provably spans every code.

With the split working, the picture:

| | value |
|---|---|
| box repaint average | 5149 µs |
| **of which `build_dl`** | **3048 µs (59 %)** |
| step-lamp boxes per window | 96 |
| step-lamp average | 11190 µs |
| step-lamp worst | 493968 µs |
| **step-lamp work per ~30 s window** | **1.07 s, while playing** |

So the chase was spending over a second of CPU per window rebuilding entire 1464×460 drum sections to paint one lamp box.

## The fix, and why it is exact rather than approximate

`ri_dlist_set_clip` / `ri_dlist_clear_clip`, and `ri_dlist_push` drops any command that cannot paint inside the box.

The equivalence argument is short: the backend's clipped replay **already** skips every command for which `ri_dcmd_hits_box` is 0 (`replay_dl_dmg`, `rsection_replay.inc`). Using the same predicate on the way in therefore yields the same surviving stream, in the same order — and a command that straddles the box edge still hits and is kept, so partial coverage is unchanged. `RI_D_CLIP` is a no-op for replay and is dropped. With no clip set the push path is byte-for-byte the old one, which is why t92's hashes and t93's pixel goldens did not move.

`build_dl` takes the box and sets the clip **after** `ri_dlist_init`, which clears it; the full-repaint path passes an inverted box, which `ri_dlist_set_clip` reads as "no clip". An inverted or empty box degrades to the old behaviour rather than to a blank canvas — the failure mode matters more than the optimisation.

## On target

| | C1 unbounded | D2/D4 bounded | ratio |
|---|---|---|---|
| box repaint average | 5149 µs | 2730 µs | **1.89×** |
| step-lamp average | 11190 µs | 6388 µs | **1.75×** |
| step-lamp worst | 493968 µs | 310630 µs | **1.59×** |
| GUI work per window | 1.35 s | 0.70 s | **1.92×** |
| **xruns** | 272 | 266 | **1.02×** |
| **wake_max** | 5792 µs | 5808 µs | **1.00×** |

**The two bold rows are the result.** GUI repaint work nearly halved, and the xruns and wake latency did not move. That is the A,B,B,A's prediction confirmed: the xruns track the governor, not repaint cost, so making the drawing ~2× cheaper cannot help them. `wake_max` is still a full 5333 µs period late at its worst.

So this closes the **GUI-responsiveness half** of the owner's original complaint with a measurement, and leaves the audio half exactly where it was. Both halves had one root in my earlier reasoning; they have different roots in fact.

Two runs excluded, both recorded: **D1 was a transient guest stall** — one tab switch at 512 ms carrying `xruns+7`, which reproduced clean as D2 — and **D3 produced only one heartbeat window** so it is not comparable. That leaves n=1 unbounded against n=2 bounded; the direction is consistent across every metric but the sample is thin, and the ratio should be treated as "about 1.9×", not as a precise factor.

## Method findings

- **A metric that reads all zeros is more often a broken metric than an absent event.** Four runs of a working-looking metric returned `0/0/0` in all three buckets, and the reading was "nothing happened". A reason destroyed before use, plus a bucket nobody printed, plus an operator assumption that a zero meant absence — three layers, none of them loud.
- **Measure the suspect before optimising it.** "59 % of a box repaint" cost one timer split; every earlier argument about where the time went was inference. It also moved the target: the fix is worth 59 % of 1.07 s, which is a number, not a hope.
- **A performance fix that does not move the symptom is a *result*, not a failure.** Cutting GUI work 1.9× and xruns 1.8 % is exactly what a governor-bound cause predicts, and reporting it as a null would have been wrong in the other direction.
- **One run is not evidence, even when it looks dramatic.** D1 showed a 5.5× tab-switch regression with `xruns+7` — alarming, entirely a guest stall, and gone on re-run. Reproducing before concluding is the only thing that separates the two.
- **This lane has now hit the same structural trap three times** (written up as a standing rule in [the testability boundary](2026-10-02-testability-boundary-aros-only-code-and-mirrored-tests.md)): `t153` mirrored the volume table, `t152` needed codes moved out of an AROS-only header, and `t154`/`t155`'s AROS-only caller cannot be reached by any host test. Anything under `gui/widgets/` or `app/` is un-unit-testable and un-mutation-testable by construction. A mirrored test survives every mutant of the thing it mirrors — which is why `mut_fixM4`/`mut_fixM5` cover only host-compiled code and the AROS wiring is proved on target instead.
- **An equivalent mutant is a fact about the design, not a gap in the test.** Dropping `ri_dlist_init`'s clip reset survived because `build_dl` always pairs `set_clip` with `clear_clip`, so nothing observable depends on it. The line stays as defensive init; the test law was moved to pin `clear_clip`, which *is* load-bearing.

## Standing gaps

- The xruns are untouched and undiagnosed. `wake_max` ~5.8 ms against a 5333 µs period with the render task already consuming 53 % of every period is the open question, and it is a scheduling/throughput question, not a drawing one.
- Tab-switch latency is unchanged: a tab switch is a **full** repaint, which the clip deliberately does not touch. Bounding full repaints is a separate problem with a separate cost profile.
- One unexplained bounded-build outlier: `build_max` rose from 121552 µs to 310630 µs. Fewer commands are built on average, yet the single worst build got slower, and the same window carries the worst repaint. Unexplained; it needs a histogram rather than a max.
- The guest shows intermittent multi-hundred-ms stalls independent of any build (D1, and a 362 ms tab switch in D4 carrying `xruns+5`). That is a confounder for every measurement taken on it and is itself uninvestigated.
- The in-memory ring is still unbuilt, and the perturbation risk is now higher: `RIAPP.LOG` writes to the USB stick.

## See Also

- [Scripted A,B,B,A: the governor arm is a real win, the repaint policy is a real regression](2026-10-02-dell-scripted-ab-abba-governor-arm-wins-repaint-policy-regresses.md)
- [The xrun fix worked on its own terms and still made it worse](2026-10-02-dell-xrun-proof-result-and-the-cost-per-repaint.md)