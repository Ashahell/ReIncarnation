# `-O2` is not a free win: it removes every xrun and makes the tab cycle 2.4× slower

- Source: ReIncarnation session, 2026-10-03 (opencode lane; the `-O2` shipping question, measured as an A,B,B,A with the build flag as the arm)
- Collected: 2026-10-03
- Published: 2026-10-03
- Prior: [The `-O0` xruns are a build-flag artefact](2026-10-03-the-o0-xruns-are-a-build-flag-artefact-not-a-render-stage-regression.md) (filled the `-O2`-on-a-real-song cell), [Optimisation level alone removes every xrun](2026-10-02-optimisation-level-alone-removes-every-xrun-on-the-dell.md), [`r12moves` is an inlining counter, not an ABI hazard](2026-10-02-r12moves-is-an-inlining-counter-not-an-abi-hazard.md), [Scripted A,B,B,A](2026-10-02-dell-scripted-ab-abba-governor-arm-wins-repaint-policy-regresses.md) (the protocol), [the click map](../articles/2026-10-02-dell-lane-scripted-ab-procedure-and-click-map.md)
- Evidence: [`docs/evidence/audio/audio-failure-and-deadline-2026-10-02.md`](../../../docs/evidence/audio/audio-failure-and-deadline-2026-10-02.md) §"A,B,B,A with the BUILD FLAG as the arm" — every figure verbatim, per-tab costs, outlier named
- Arm A: `RIAPP-f0`, 1,094,648 B, `r12moves=41` · Arm B: `RIAPP-f2`, 874,744 B, `r12moves=282` — both from clean `HEAD` `a98691a`

## The experiment

Both arms built from the **same clean tree**, exported with `git archive HEAD |
tar -x` rather than built from the working directory. That detail is the whole
design: the working tree holds the sibling lane's uncommitted `NOAUDIO` change,
and a flag comparison that silently differs by someone else's patch measures
nothing. `-O0` → 41 `r12moves`, `-O2` → 282; same source, same commit, flag only.

Protocol per cell is the existing scripted one — Play, all five tabs, Stop — with
the real-song playlist rather than the demo, because the demo is the workload
that could not discriminate. Coordinates from the click-map record and
**verified per click** against the event each one produces:

```
TR PLAY TAB page=0 TAB page=1 TAB page=2 TAB page=3 TAB page=4 TR STOP
```

Identical in all five cells. No blind clicks, no destructives.

## Results

| cell | arm | buffers | **xruns** | **overloads** | `render_max` | `wake_max` | **5-tab total** |
|------|-----|---------|-----------|---------------|--------------|------------|----------------|
| A1 | `-O0` | 4,273 | **1,590** | **6** | 91,932 µs | 5,820 µs | 99,432 µs |
| A2 | `-O0` | 4,299 | **1,589** | **6** | 92,021 µs | 5,816 µs | 99,786 µs |
| B1 | `-O2` | 3,065 | **0** | **0** | 4,113 µs | 471 µs | 300,041 µs * |
| B2 | `-O2` | 3,084 | **0** | **0** | 4,101 µs | 398 µs | 241,791 µs |
| B3 | `-O2` | 6,064 | **0** | **0** | 4,117 µs | 436 µs | 241,632 µs |

\* B1's LEVI switch alone was 107,029 µs against 52,475 and 52,545 — **excluded
by name** under this guest's re-run rule, not quietly dropped. B2 and B3 then
agree to **0.07 %**; the `-O0` pair agrees to **0.36 %**.

Play windows (`TR PLAY` → `TR STOP`): 4299, 4301, 4646, 4738, 4831 ms — comparable,
so the xrun counts are not a duration artefact.

## What `-O2` buys, and what it costs

**Buys, decisively.** Zero xruns in three runs and 12,213 buffers, against
1,590/1,589 at `-O0`. `render_max` falls **22×** (92 ms → 4.1 ms), `wake_max`
falls **13×**, and `overloads` goes 6 → 0.

**Costs, just as decisively.** The five-tab repaint cycle goes from 99.6 ms to
**241.7 ms — 2.43× slower** — and per tab the penalty is uniform: DRUMS 2.2×,
LEVI 2.8×, MIX 2.4×, FX 2.5×, while SYNTH stays at 27 µs (it was never a
repaint). `full_avg` roughly doubles, 2,275 µs → 4,563 µs.

So the flag does not remove a problem, it **moves** it: from the audio deadline
to GUI responsiveness. That is a trade, and which side matters is an owner call,
not a measurement question. The measurements above are the answer to the
measurement question.

## `r12moves=282`: no functional hazard, and the mechanism was never the issue

Three `-O2` runs, every click event-verified, no misbehaviour of any kind. That
is expected rather than lucky: the count is an inlining artefact confined to the
eight AROS-only files, and the three `-O2` runs exercised all of them —
tab switches drive `rsection.mcc.o` (the largest, 52), `skin_aros.o` loads at
startup, `audio_ahi_live.o` runs the live AHI backend, `fs_aros.o` loads three
songs, and the 909 pack and the log are live throughout.

The useful conclusion is a negative one: **the reason not to ship `-O2` is
GUI latency, and it has nothing to do with `r12moves`.** The ABI question that
was framed as the blocker is answered twice over — host-side by source analysis
of `libcall.h`, and now on target by three clean runs.

## The prediction, confirmed — and it has a consequence nobody wrote down

The `-O2` record predicted that at `-O2` neither A,B,B,A fix would be measurable
because the governor arm would never engage. Confirmed: `overloads` 6 → 0,
`arm_us` 0 throughout.

But the consequence runs the other way from what was expected. The arm going
quiet does not make the repaint regression irrelevant — **it makes it visible.**
At `-O0` the render task overruns so badly that it trips the governor six times,
and those trips are what yield CPU to Intuition; the tab cycle is fast *because*
the audio path is collapsing. At `-O2` nothing overloads, the arm never fires,
and the tab cycle is exposed at its true cost.

That is a hypothesis consistent with this data, not something these runs prove —
the two effects are coupled and separating them needs an arm-disabled `-O2`
build, which is the obvious next experiment. What is established is the coupling
itself: **the audio xruns and the GUI latency are not independent problems, and
fixing the first exposes the second.**

## Standing gaps

- **`-O0` on the real song is not viable**: ~1,590 xruns per tab cycle, 6
  governor trips, `render_max` 92 ms. The deployed binary is `-O0`. That is a
  shipping problem in its own right, independent of `-O2`'s cost.
- **The arm-disabled `-O2` build** would separate "the arm was masking the
  repaint regression" from "`-O2` genuinely slowed repaint". Not run.
- **`songs/local/` is git-ignored**, so `demos.rbpl` and the songs it names exist
  only on this machine. These five runs are unreproducible from a clean clone.

## Method findings

- **An A,B,B,A where the arm is an environment variable, not a commit, needs the
  tree pinned.** Building both arms from `git archive HEAD` rather than the
  working directory is what makes the comparison mean anything when another lane
  has uncommitted work in the tree — which it did.
- **Re-run the outlier, then name it.** B1's LEVI switch was 2× its neighbours.
  The wiki's standing rule for this guest is to re-run rather than quietly drop;
  B3 settled it, and the exclusion is recorded by name and with both numbers.
- **Reproducibility is what makes the cost credible.** A 2.43× claim needs a
  tight pair on each side. 0.36 % and 0.07 % are what turn "slower" into a
  result; the first `-O2` run alone would have been an outlier and the second
  would have looked like a regression that had already fixed itself.
- **"This fixes it" and "this is better" are different claims.** `-O2` fixes the
  xruns completely and is worse for the GUI. Reporting only the first would have
  been a true record of a decision that loses.