# The `-O0` xruns are a build-flag artefact, not a render-stage regression: `-O2` gives 0/214,676 on the real song

- Source: ReIncarnation session, 2026-10-03 (opencode lane; reconciling two wiki records that sat in mild tension)
- Collected: 2026-10-03
- Published: 2026-10-03
- Prior: [Optimisation level alone removes every xrun on the Dell](2026-10-02-optimisation-level-alone-removes-every-xrun-on-the-dell.md) (the `-O2`/demo-only half), [HEAD does not meet the audio deadline on a real song](2026-10-02-head-misses-the-audio-deadline-on-a-real-song.md) (the `-O0`/real-song half), [Render-stage breakdown: voices are 94 %, not the FX chain](2026-10-02-render-stage-breakdown-voices-are-94-percent-not-the-fx-chain.md)
- Evidence: [`docs/evidence/audio/audio-failure-and-deadline-2026-10-02.md`](../../../docs/evidence/audio/audio-failure-and-deadline-2026-10-02.md) §"`-O2` on a REAL song" — every figure verbatim, plus the four-cell table
- Binary: `Vk4aros:ReIncarnation/RIAPP-o2`, 874,968 B, `r12moves=282`

## The tension, and why it was never a contradiction

Two records disagreed on their face:

- "`-O2` gives 0/0 where `-O0` gives 2320/3116" — one article.
- "HEAD misses the audio deadline on a real song: 19,703 xruns" — another, and
  three `-O0` runs on `demos.rbpl` agreeing with it (20,998 and 18,259).

The first record's own method note had already flagged the confound and then
measured around it. What it did not say out loud is **what the workload was**.
The raw logs answer it — `stg1`, `stg2`, `stgo2`, `stgo2b` and `stgbase` each
contain `RIAPP play` and **zero** `RIAPP playlist` / `RIAPP song` lines:

```
  stg1      playlist=0 song=0 play=1
  stg2      playlist=0 song=0 play=1
  stgo2     playlist=0 song=0 play=1
  stgo2b    playlist=0 song=0 play=1
  stgbase   playlist=0 song=0 play=1
```

So the `-O2` result was measured on the **built-in demo**, and the `-O0` result
on **The Knife and Zombie Nation**. Different workloads. Neither record was
wrong; the matrix simply had a hole in it, and the hole was exactly the cell
that decides the question.

> **Reproducibility caveat (2026-10-03, later).** The statement above describes
> **those runs**, and it remains true of them. It is no longer true of a *fresh*
> launch: the default song was changed to Zombie Nation the same day (see
> [the song-name/`T:`/default record](2026-10-03-song-name-t-log-probe-and-zombie-nation-default.md)),
> so a launch today with no `PLAYLIST=` plays Zombie Nation, not the demo. Anyone
> reproducing the demo-workload cell must say so explicitly rather than rely on
> the default having stayed put — which is the same failure the `..` records warn
> about, where the *workload* was the unstated variable.

## The missing cell

`-O2` on a real song. Built by substituting `-O2` for `-O0` in a copy of
`~/bin/build_v11.sh` (81 TUs, 0 undefined, `r12moves=282` — matching the
existing `-O2` profile of 873,888 B / 286). Same playlist, one instance, AHI
live, two runs because a single `0` is either a fix or a quiet guest:

```
run 1  buffers=109229 xruns=0 stg_total_avg=2467 us stg_dsp_avg=2389 us
       render_max=4296 us wake_max=448 us arm_us=0 overloads=0
run 2  buffers=105447 xruns=0 stg_total_avg=2805 us stg_dsp_avg=2723 us
       render_max=4414 us wake_max=43 us  arm_us=0 overloads=0
```

**214,676 buffers, 0 xruns.** The four cells, same playlist where comparable:

| flag | workload | xruns | `stg_total_avg` | % of 5333 µs | `render_max` | `wake_max` |
|------|----------|-------|-----------------|--------------|--------------|------------|
| `-O0` | `demos.rbpl` | 20,998 | 5766 µs | **108 %** | 21573 µs | 5823 µs |
| `-O0` | `demos.rbpl` | 18,259 | 5654 µs | **106 %** | 10115 µs | 5823 µs |
| `-O2` | `demos.rbpl` | **0** | 2467 µs | 46 % | 4296 µs | 448 µs |
| `-O2` | `demos.rbpl` | **0** | 2805 µs | 53 % | 4414 µs | 43 µs |

At `-O0` the *average* buffer is over budget on the heaviest workload measured.
At `-O2` the same buffer is at about half. Levi, the largest single device, goes
737 µs → 350 µs and `dstg block` 1370 µs → 660 µs: a uniform ~2.1× with the
**proportions unchanged**, which is the signature of a compiler flag and not of
an algorithmic change.

## What this does and does not retract

**It does not retract the `-O0` finding.** The controlled pair that produced
19,703 vs 0 was old-binary `-O0` against HEAD `-O0` — same flag, so
"the DSP got more expensive at `-O0`" still stands exactly where it was
claimed. That comparison is untouched.

**It does change the conclusion that was drawn from it.** Three records had
accumulated the operational reading "HEAD does not fit the audio deadline on a
real song, hand it to the render-stage lane". On the evidence now in hand that
reading is wrong about the *cause*: the DSP is not too expensive, the *build* is.
Nothing in the render stage needs fixing to make this song play; the render
stage's own breakdown was right about **where** the time goes (Levi, voices)
and that remains useful — it is the wrong lever for this symptom.

**The one thing still open is shipping `-O2`,** and it is not a measurement
question any more. `r12moves` goes 41 → 282 and the binary shrinks by ~219 KB;
the repo's own ABI recipe treats that counter as the shipping check, and 282 has
never been run on the Dell as a shipping configuration. That is an owner
decision with its own evidence, exactly as the `-O2` record already said.

## A prediction, confirmed

That record predicted: *"If the A,B,B,A arms were re-run at `-O2`, neither fix is
measurable, because the arm would never engage."* Both runs show
`arm_us=0 overloads=0`, peak `load` 526/1000 and 716/1000. The prediction holds,
on a heavier workload than it was made about — and it is the same mechanism as
its own point 3: at `-O0` the wake was late because the *work* was long.

## Method findings

- **A matrix with one filled corner is not a contradiction.** Two records
  disagreed; the reconciliation was a single grep (`playlist=0 song=0`) against
  logs that had been sitting on disk, readable, the whole time. Before calling
  anything inconsistent, ask what the *workload* was — and prefer reading it out
  of the log over inferring it from the prose.
- **"On this workload" is a load-bearing phrase.** The `-O2` record said it
  sixteen times and never once said the words *built-in demo*. A reader is
  entitled to take that as the workload that matters.
- **The heaviest workload should be the default for a performance claim.** Every
  `-O2` number on file was measured on the lightest thing the app can play. The
  flag's benefit was real and its *extent* was understated, which is the more
  dangerous direction for a record to be wrong in.
- **`arm_us=0` is a result, not an absence.** Zero arm usage is the predicted
  consequence of a faster build, and it is the cleanest available confirmation
  that the governor fixes are compensating for code running too slowly.