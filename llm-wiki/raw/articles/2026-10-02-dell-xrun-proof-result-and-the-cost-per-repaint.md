# The xrun fix worked on its own terms and still made it worse: repaint *count* fell 98.6 %, repaint *cost* did not move (owner 2026-10-02)

- Source: ReIncarnation session, 2026-10-02 (opencode lane; the on-target proof of `3dadda7` that the previous record left open)
- Collected: 2026-10-02
- Published: 2026-10-02
- Plan: `docs/superpowers/plans/2026-09-30-leviasynth-fidelity-plan.md` (§P9 interlude), ledger in `docs/2026-09-24-improvement-todo.md` §12.11
- Prior: [2026-10-01-dell-xruns-governor-arm-and-repaint-policy.md](2026-10-01-dell-xruns-governor-arm-and-repaint-policy.md) (the diagnosis and the two fixes), [2026-10-01-clipping-comp-limiter-tab-stall-priority.md](2026-10-01-clipping-comp-limiter-tab-stall-priority.md) (the pri-21 render task), [2026-09-22-wbs21-storm.md](2026-09-22-wbs21-storm.md), [2026-10-01-menu-hang-tab-artifacts-load-governor.md](2026-10-01-menu-hang-tab-artifacts-load-governor.md)
- Deployed image: `RAM:RIPP9F`, ev-log `RUN frames=256 vol=RAM: build=3dadda7`; the fix under test is `3dadda7`
- Raw logs: `/tmp/opencode/fix_f.log` (346925 B, `RIAPP.LOG`, sha-verified bulk get) and `/tmp/opencode/fix_f.ev.log` (1549 B, `RIAPP-EV.LOG`) — **both since lost**: the host restarted at 07:48 and the owner rebooted the Dell, so `RAM:` went with it. The figures are preserved here and in `58b00c4`; the artefacts are not
- Revised: 2026-10-02 (later) — a third-party advisor review landed; three claims withdrawn and the causal chain downgraded to Disputed. See *The advisor's objections*, and the Status blocks
- Follows on: `5940cc3` (wake latency + repaint reason metrics)

## What the owner said, and what I did about it

The owner's report, on the running `RAM:RIPP9F`: the GUI was "way less responsive", audio "choppy", and **"this wasn't the case with claude's fix"** — the comparison being the previous lane's build.

There was no way to act on that without telemetry, and the telemetry was locked: `RAM:RIAPP.LOG` belongs to a running process. So I closed the app cleanly with `--ui-close` (window management, not pointer injection), let it write its closing line, and pulled both logs afterwards. **The app is currently not running** — a consequence worth stating plainly, since it was under the owner when I closed it.

The event log shows the owner's whole session was one gesture: `TR PLAY`, four `TAB` switches, 45 `CTL` knob events inside 564 ms, `TR STOP`. Playing lasted 7767 ms of EClock.

```
ev  4 4630864 TR PLAY
ev  5 4631677 TAB page=1 us=52890 xruns+0
ev  6 4632050 TAB page=2 us=42885 xruns+0
ev  7 4632526 TAB page=3 us=76229 xruns+0
ev  8 4632920 TAB page=4 us=52676 xruns+0
ev  9 4636542 CTL 0906=24        ... 45 CTL lines ...
ev 55 4638631 TR STOP
```

## The measurement mistake that had to be undone first

My own repaint analysis of this log family was wrong, and it was wrong in a way that inverts conclusions. The `draw:` heartbeat line carries **two** fields called `n=`:

```
RIAPP draw: full_max=30601 us full_avg=5774 us n=15 part_max=303098 us part_avg=4036 us n=119 blit_max=25012 us allocs=15
```

The first `n` counts **full** repaints, the second counts **partial** ones. Parsing the line into a dictionary keeps the *last* `n`, so every count read as "partial" is really "full" and vice versa. My first pass reported this window as "119 full repaints at 5774 µs each"; it is **15 full repaints at 5774 µs and 119 partial repaints at 4036 µs**. Every repaint ratio in the previous record needs re-reading with the two fields kept apart.

Re-read that way, the previous build's headline ratio survives — `1.12` xruns per full repaint (1165 xruns / 1037 full repaints), against the `1.47` the previous record quoted from a six-heartbeat slice of one window. The *conclusion* the ratio was used for does not survive; see the last section.

## Both fixes did exactly what they were designed to do

The same log contains the previous lane's run, so the comparison is like-for-like on one machine, one log file, one parser. Segmenting on the heartbeat's buffer counter (which resets per launch) gives three runs: a long one, a short second one, and `RAM:RIPP9F`.

**The main log carries no build tag, so the identification of the first run is an inference, not a read.** It rests on two things that do line up: run 0's overload ladder is `0 → 9` by buf 55227 and `9 → 22` by its end, which is the baseline the previous record wrote down from an earlier pull of the same file, and `RAM:RIPP9F` was deployed strictly after `RAM:RIPP9` in an append-only log. The `build=3dadda7` tag is confirmed only for the *last* run, from the ev log. Treat the left-hand column as "the previous lane's build" rather than as a proven `cdcf85c`.

| | previous lane's build | `RAM:RIPP9F` (`build=3dadda7`, this fix) |
|---|---|---|
| xruns, whole session | 1165 | **407** |
| xruns in the worst burst | +680 over 635 full repaints | +407 in 3 windows |
| duration of the playing session | ~19 min | 7.8 s |
| `overloads` (governor yields) | 22 | **0** |
| **full repaints** | **1037** | **15** |
| **partial repaints** | 119 | 182 |
| worst full repaint | 115270 µs | 30601 µs |
| **worst partial repaint** | **641 µs** | **303098 µs** |
| partial repaint average | 186..641 µs | 300..4036 µs |
| peak `load` | 1083/1000 | 928/1000 |
| windows that repainted at all | 8 of 79 | **3 of 1645** |

- **Fix A worked as specified**: `overloads` went 22 → 0. The 2 s arm never completed once, because the load never stayed over 850 for 2 s. The governor never yielded.
- **Fix B worked as specified, and further than claimed**: full repaints fell 1037 → 15, and only 3 of 1645 heartbeat windows repainted anything at all. The blanket 100 ms refresh of 18 canvases is gone. The damage-box path it substituted is the subject of the next section, and it is not cheaper.
- **The tab-switch xruns are gone**: all four `TAB` lines read `xruns+0`.

And the owner still heard choppy audio and a sluggish GUI. Both facts are true at once, and the table says why: **the count of repaints collapsed and the cost of each one did not.**

## The cost that did not move: the damage box avoids the blit, not the build

`gui/widgets/rsection.mcc.c`'s draw hook has two paths. The full path calls `draw_section` and blits the whole canvas. The damage path — the one Fix B introduced — is:

```c
if (d->dmg_valid) {
    ...
    build_dl(&d->brp, d, 0, 0, &dl); /* CPU only; cheap vs blits */
    if (replay_dl_dmg(&d->brp, &dl, ..., x0, y0, x1, y1)) {
        BltBitMapRastPort(d->bm, x0, y0, wrp, ...);   /* the small box only */
```

`build_dl` is handed the **whole** section (`0, 0`), with no damage clipping. So a "partial" repaint pays a full display-list construction and then throws away everything outside the box. The comment calling it "cheap vs blits" is true about the blit and silent about the build, and the build is the larger term: the surviving partial repaints average **4036 µs**, which is 76 % of one 5333 µs audio buffer period, against 186..641 µs for the old two-lamp box blits they replaced.

The tail is worse than the average by two orders of magnitude: one partial repaint took **303098 µs**, in the same window as `allocs=15`, i.e. where bitmaps were being rebuilt. The previous build's worst partial was 641 µs. **A path that was supposed to make repaints cheaper introduced a repaint mode that can take 303 ms**, and nothing in the design bounds it.

## Why that produces both symptoms at once

The xrun detector is not a CPU-saturation test. In `audio_io/audio_ahi_live.c` the render task waits on the AHI hook signal and reports a dropout when it wakes to find more than one half already played:

```c
if (n - processed > 1u)
    ri_livedrv_report_late(&lv->drv, n - processed - 1u); /* AHI looped a half: late */
```

> **Status: Refuted** (2026-10-02, later the same day — by measurement)
> A scripted A,B,B,A on the Dell (arms `4167e32` vs `b68443c`, both `-O0`, two runs each) **inverts** this claim. The arm carrying the repaint policy performs **2.3x more repaint work** (five tab switches cost 279-294 ms against 120 ms) and has **56 % fewer xruns** (274 in both runs against 618/620), with `overloads` 0 against 8. If repaint cost caused the xruns the slower-repainting arm would drop out more. The xruns track the **governor**: 8 trips gives 85-136 ms `render_max` (16-26 buffer periods at pri -1), zero trips never exceeds 6.4 ms. **No tab switch caused a dropout in any of the twenty switches**, in either arm. Separately, the duty cycle quoted in the block below is an **idle** average: recomputed over playback it is 53-54 % of a buffer period, not 0.6 %. Full record: [the A,B,B,A](2026-10-02-dell-scripted-ab-abba-governor-arm-wins-repaint-policy-regresses.md).
> Earlier, on the advisor's objections alone, the same claims were marked *Disputed*; the text below records that intermediate state.
> The numbers are sound; the causal claim below is not. A pri-21 task cannot be pre-empted by GUI work at pri 0–20, so charging a dropout to each audio period a GUI operation spans needs a *named blocking point* — a `Forbid`, a contended lock — and none was ever found. The repaint timings are also wall-clock, so an audio stall would inflate them: the 303 ms tail may be a **symptom** of the dropout burst rather than its cause. And 407 dropouts in 7.7 s is ~28 % of all buffers, which does not reconcile with a task using 0.6 % of a core; that tension was resolved in favour of this story instead of investigated. What survives: the measurement that repaint cost did not move, and the conclusion that the pri -1 fallback was suppressing it. What does not survive: that the repaint cost caused the dropouts. Wake latency was added (`5940cc3`) specifically to settle it.

The xrun detector is not a CPU-saturation test. In `audio_io/audio_ahi_live.c` the render task waits on the AHI hook signal and reports a dropout when it wakes to find more than one half already played:

```c
if (n - processed > 1u)
    ri_livedrv_report_late(&lv->drv, n - processed - 1u); /* AHI looped a half: late */
```

So a single long GUI operation *would* be charged one dropout per audio period it spans, and a 303 ms operation spans about 57 periods. The render task is not competing for a saturated CPU — it uses **32.8 µs of every 5333 µs** on average over the whole 7-hour session (`render_total=153734 ms` / 4683315 buffers), about 0.6 % of a core. It is *late*, not *crowded*.

The GUI's own cost is what a hand would notice, and it is large regardless of what caused the dropouts: a 4 ms repaint is a floor under every interaction, so a knob drag that repaints per step lands ~4 ms behind the pointer; the 30601 µs full repaint is about 1.8 frames at 60 Hz; 303098 µs is about 18 frames, a quarter-second freeze. 119 partials averaging 4036 µs inside one ~11.5 s window is ~480 ms of GUI work.

The timeline is tight:

```
buf=4630697  xruns=0    full n=15 avg=5774 max=30601 | part n=119 avg=4036 max=303098 | allocs=15
buf=4633944  xruns=158  (play burst starts)
buf=4635622  xruns=247  part n=62 avg=300 max=3279
buf=4638231  xruns=387  load=928
buf=4641143  xruns=407  load=2   (transport stopped; xruns stop dead)
```

Repaints happen in one window, the xruns follow in the next three, and they stop when the transport stops. That ordering is a correlation, and correlation is where this argument went wrong: at the time it was written the claim "no further explanation is required" stood, and it was the strongest-sounding and least-supported sentence in the record. A timeline does not name a mechanism, and the mechanism has to survive the priority arithmetic to be a mechanism at all.

## What this overturns in the previous record

The previous record's method finding reads: *"Xruns per repaint, not xruns per second, named the bug … Only together do they point at a priority inversion rather than at 'make the drawing cheaper'."* The ratio was right; the exclusion of the cost explanation was wrong, and the measurement that should have caught it was sitting in the same log.

The priority inversion was real — the previous build's `overloads=22` put the render task at `AU_LIVE_PRI_YIELD -1` (below the UI) for 2 s at a time, 22 times, and removing it is exactly what Fix A did. But the inversion was a *second* cause with a *smaller* coefficient than the one underneath it. Removing the second cause and leaving the first is what produced "the numbers are better and the experience is worse".

- The previous lane's build was slow because it did 1037 full repaints of 18 canvases **and** dropped the render task below the UI. The GUI felt instant because the audio paid for it.
- `RAM:RIPP9F` is fast because it does 15. The GUI no longer has the audio task as a punching bag, so it feels the true cost of a repaint — which nobody had measured, because the yield had been hiding it.

That is the whole finding: **the pri -1 fallback was a symptom-suppressor, and removing it exposed a per-repaint cost problem that was always there.**

> **Status: Refuted** (2026-10-02, later the same day — by measurement)
> The suppressor reading is right for the wrong reason, and the measurement is sharper than the hedge. The pri -1 fallback *was* suppressing the xruns — suppressing them by **being** the cause. Arm A, which keeps the fallback, drops out 2.3x more; arm B, which removes the trips, drops out 56 % less with the same render cost per buffer (2869 vs 2806 us). The suppressor was hiding the mechanism, not a second cause underneath it.
> Earlier, on the advisor's objections alone, this was marked *Disputed*; the text below records that intermediate state.
> The suppressor reading survives in its weaker form. The second sentence is the unsupported part: both symptoms being "that one cost, seen from two ends" requires the GUI to delay a pri-21 task, which needs a named blocking point that was never found, and the 303 ms repaint may itself be a consequence of the audio stall rather than its cause. Something *was* being hidden by the pri -1 fallback and the per-repaint cost was measurably large enough to be worth hiding — but what it was hiding, and what caused 407 dropouts in 7.7 s, is open.

## Method findings

- **A cumulative counter can improve while the experience gets worse.** 1165 → 407 xruns is a 65 % improvement and was reported as one. But the earlier build's 1165 were spread over 19 minutes; these 407 arrived inside 7.8 s of playing, one every 19 ms. The ear hears a rate, not a session total. Score the rate inside the window that matters, and score the *latency of the thing the hand touches* separately — a metric that cannot see the GUI cannot detect a fix that made the GUI worse.
- **The fix and the diagnosis must be scored on the same axis.** Fix A targets audio continuity and Fix B targets wasted work; neither can be validated by "xruns went down". The only way both were provable at once is to measure both axes, and the second axis (repaint latency) had no metric at all — which is precisely why the regression shipped.
- **A metric that does not exist cannot refute a theory, but a theory built without it will be stated as if it did.** The record below asserts a causal chain from repaint cost to dropouts with no blocking point named, and it reads as settled. The missing measurement did not just leave a gap — it left room for a confident sentence that the arithmetic cannot support. Instrument the question *before* answering it, and if a number is not available, the honest output is "cannot tell yet", not the most plausible mechanism available.
- **A correlation ordered correctly in time is not a mechanism.** "Repaints happen in one window, the xruns follow in the next three, they stop when the transport stops" is a real observation and it was written as though it closed the question. It closes nothing until it survives the priority arithmetic.
- **An admission-control failure silently removes test coverage.** `gui/widgets/rsection.h` is AROS-only and `#error`s on a host include, so a policy declared there would have been untestable in principle — not because the policy was hard, but because of where someone had put it. The repaint-reason codes had to move to `gui/panelui.h` to be testable at all. Worth checking before designing, not after.
- **A telemetry line with two fields of the same name will silently invert an analysis.** `n=` appears twice per `draw:` line with different meanings. It cost a full wrong reading of the owner's session and would have produced a confidently wrong article. Any keyed parse of these lines must handle the duplicate, and the format itself deserves disambiguation.
- **A "damage" optimisation that skips the blit but not the build is not a partial.** `build_dl(&d->brp, d, 0, 0, &dl)` reconstructs the whole section's display list to paint one box. Whenever a damage path is introduced, the *construction* has to be bounded too, or the optimisation only moves the cost somewhere the metrics do not look.
- **A fallback that escalates silently inherits the worst case.** `ri_rsection_refresh_box` escalates to `MUI_Redraw(o, MADF_DRAWOBJECT)` on three degenerate-input paths, and `meter_round` escalates to `ri_rsection_refresh` on four. Every one of those is counted under the cheaper label, so a 303 ms full redraw can appear in the log as a "partial repaint". The telemetry cannot distinguish what it is measuring.

## The advisor's objections, and which of them held

A third-party advisor reviewed this record the same day. Three claims did not survive contact with the repository, and two did — the two that did are the ones that changed the work.

**Wrong: "rebase onto `4167e32`, it already contains two of the things being redesigned."** It does contain them, and it is *already* an ancestor of HEAD, already on `origin/main`, dated five hours before Fix A. Traced per commit, the two guards are different layers, not duplicates: `4167e32` added `RI_LIVEDRV_LOAD_CAP_PM 1200` (how much one slow buffer may raise the smoothed load), Fix A added `RI_LIVEDRV_ARM_US` (how long the load must stay over). Both survive to HEAD. There were no conflicts to resolve because no fork was ever taken.

**Wrong: "the event log shows `build=?`, so the measurements may be from a priority-10 tree."** The RUN line reads `build=3dadda7`, and that commit postdates the pri-21 change.

**Wrong: "the diagnosis does not hold at priority 21."** *The mechanism* does not hold; the measurement survives. A pri-21 task cannot be pre-empted by GUI work at pri 0–20, so "one 303 ms repaint = 57 dropouts" needed a named blocking point — a `Forbid`, a contended lock — and **none was ever found.** Worse for the argument, those repaint timings are wall-clock, so an audio stall would *inflate* them: the 303 ms tail may be a symptom of the dropout, not its cause. And 407 dropouts in 7.7 s is ~28 % of all buffers, which does not sit with a task using 0.6 % of a core. That tension is the real finding, and it was resolved in favour of the causal story rather than investigated.

**Right, and acted on:** the fix and the diagnosis were scored on different axes, and the axis the owner feels — repaint latency — had **no metric at all**. That is why a change could improve every counter while making the experience worse. This is the failure mode named in *Method findings* above, now closed by `5940cc3`.

**Right, and acted on:** one 7.7 s session by one owner on one machine, with the earlier run's build identity inferred rather than tagged, is not a basis for tuning a governor.

Also noted: `4167e32` added `blit_max`/`allocs` to the `draw:` line whose two duplicate `n=` fields are the parsing trap above — the duplicate predates both lanes and the format was never disambiguated.

## What was built after this record (`5940cc3`)

Two metrics, both pure accounting — no engine DSP touched, so no song render moves and t92's hashes and t93's goldens are unchanged.

- **Wake latency.** The AHI hook stamps the EClock when the device wants a half; the render task reports the interval *before* rendering, with the priority it actually held. This is the metric that separates late from slow. The priority is the point: a late wake at `AU_LIVE_PRI_YIELD -1` is the governor's doing and one at 21 is not. The heartbeat gained `wake_max`, `wake_n`, `prio` and `arm_us` — the last being the governor's own accumulation, so that `overloads=0` stops conflating "never over budget" with "over budget and reset hundreds of times". Proof: `t151_wake_latency`, mutation 12/12.
- **Repaint reason.** Every box repaint is tallied against the caller that asked for it, closing this record's own gap ("nothing attributes a repaint to a cause"). The reason codes live in `gui/panelui.h` because `gui/widgets/rsection.h` is AROS-only and refuses a host include — an admission-control failure that would otherwise have made the policy untestable. Proof: `t152_repaint_reason`, mutation 6/6.

**Not built on purpose:** the in-memory ring dumped at quit. Live per-line writes during a run can perturb what they measure, and the A/B should not be the run that discovers it.

## Honest gaps

- **The remediation is still undesigned.** The metrics exist so the next diagnosis can be evidence rather than argument; no fix has been designed against them, and nothing here has been re-run on the Dell.
- The raw logs behind every number here were in `/tmp` and the host restarted at 07:48, taking them. The figures live in this record and its commit; the artefacts do not.
- `RAM:RIPP9F` is closed and not relaunched. The owner rebooted the Dell afterwards, so `RAM:` and both logs are gone. `RAM:RIPP9E` (the stray ABIv1 build) needs a Suspend click at `--ui-click l,590,531`; a reboot is the owner's call, not the lane's.
- What actually costs 4036 µs inside `build_dl`, and what makes one call cost 303098 µs, is **not** established. The 15 allocations in the same window are a hint, not a measurement. Nothing in the current telemetry attributes a repaint to a cause, so the biggest remaining question cannot be answered from the log at all.
- The owner's "less responsive" is a report, not a measurement, and remains one: no GUI latency metric exists yet in the deployed build. The repaint costs above are a proxy, consistent with the report but not a measurement of it.
- The causal chain from repaint cost to dropouts is **unsupported**, not merely unproven: the mechanism requires a blocking point that was never identified, and the 303 ms figure may be a consequence of the stall it was cited as the cause of.
- This record's numbers come from one 7.8 s play burst by one owner on one machine. The comparison holds because both runs are in the same log under one parser, but neither run is a controlled A/B, and which build produced the earlier run is inferred rather than tagged.

## See Also

- [Dell xruns while playing and switching tabs: the governor's trip law and the repaint policy](2026-10-01-dell-xruns-governor-arm-and-repaint-policy.md)
- [Clipping (Comp make-up) and tab-switch dropouts (render priority)](2026-10-01-clipping-comp-limiter-tab-stall-priority.md)
- [The Dell lane has two silent traps: the audit's link gate builds ABIv1, and `--get` serves stale bytes for a file the guest still has open](2026-10-02-dell-lane-abiv11-build-and-bulkget-staleness.md)
