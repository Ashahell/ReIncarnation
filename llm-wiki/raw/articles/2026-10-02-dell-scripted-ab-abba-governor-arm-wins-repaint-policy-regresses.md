# The scripted A,B,B,A settles it: the governor arm is a real win, the repaint policy is a real regression, and "0.6 % of a core" was an idle average applied to a playing session (2026-10-02)

- Source: ReIncarnation session, 2026-10-02 (opencode lane; the advisor's A,B,B,A protocol, executed on the Dell)
- Collected: 2026-10-02
- Published: 2026-10-02
- Plan: `docs/superpowers/plans/2026-09-30-leviasynth-fidelity-plan.md` (§P9 interlude)
- Prior: [2026-10-02-dell-xrun-proof-result-and-the-cost-per-repaint.md](2026-10-02-dell-xrun-proof-result-and-the-cost-per-repaint.md) (the record this one refutes), [2026-10-02-dell-sticky-log-location-and-size-heuristic-correction.md](2026-10-02-dell-sticky-log-location-and-size-heuristic-correction.md) (the wake metric), [2026-10-02-dell-lane-abiv11-build-and-bulkget-staleness.md](2026-10-02-dell-lane-abiv11-build-and-bulkget-staleness.md) (the `-O0` requirement)
- Arms: **A** = `4167e32` (pri-21 render task, stall cap `LOAD_CAP_PM 1200`, draw-timing fix, tab probe). **B** = `b68443c` (A + the governor arm `ARM_US`, the repaint policy, the P9 features, the wake/repaint-reason metrics, the sticky log). Both built `-O0` by `build_v11.sh`, both `r12moves=41`, 1072464 B and 1081512 B
- Raw logs: `~/Work/vms/ri-p9/logs/ab-2026-10-02/` (4 ev-logs + 4 main logs, all sha-verified pulls)

## The protocol, and how it deviated

Per the advisor: A,B,B,A alternating, same song, all five tabs with a fixed gap. What was actually executed, and the two ways it is not what was asked for:

- **The gap was ~0.7 s, not 4 s.** The guest's `wait` returns in ~1.06 s but the ev-log timestamps show PLAY at 2482 → STOP at 7710 ms for five switches, i.e. ~700 ms each. So the run is a *faster* stress than specified. Both arms got the identical script, so the comparison holds; the absolute numbers do not transfer to a slower, human-paced session.
- **No song was loaded.** There is no `ev N SONG load` line in any log, so both arms played the built-in default with the 909 pack. Identical across arms, and it is the state the previous sessions also ran, but it is one song and it is not Zombie Nation.

Clicks are agent-injected, verified one by one against the ev-log, and restricted to Play, Stop and the tab row. Coordinates were **derived, not guessed**: the tab row is the advisor's and validates against a scale-2 capture; Play (513,80) and Stop (560,80) came from `gui/panelgeo.c` at the compact zoom (`q*3/8`) and were confirmed by reading `TR PLAY` / `TR STOP` back out of the ev-log. Nothing else on the panel was ever clicked.

## Results

| run | arm | xruns | `overloads` | `render_max` | 5-tab total | `wake_max` | mean wake | `arm_us` |
|---|---|---|---|---|---|---|---|---|
| A1 | `4167e32` | **618** | **8** | 84971 µs | 120575 µs | — | — | — |
| B1 | `b68443c` | **274** | **0** | 6336 µs | 279056 µs | 5802 µs | 881 µs | 0 |
| B2 | `b68443c` | **274** | **0** | 6359 µs | 294043 µs | 5810 µs | 894 µs | 261317 |
| A2 | `4167e32` | **620** | **8** | 136056 µs | 120558 µs | — | — | — |

Both B runs produced **exactly 274 xruns**. Both A runs produced 618 and 620 with **8 governor trips each**. The spread within an arm is ~0.3 %; the gap between arms is 56 %.

## What this establishes

**1. The arm works; the stall cap does not.** Arm A tripped the governor **8 times in a ~4 s play session** — roughly every 500 ms — despite `LOAD_CAP_PM` already bounding each buffer's contribution. Its `render_max` reached 85 ms and 136 ms, which is 16–26 buffer periods in a single render: the signature of the task sitting below the UI while Intuition works. Arm B tripped **zero** times and never exceeded 6.4 ms. The owner's hypothesis was that the stall cap would stop a short stall from tripping; the measurement says the cap bounds the *input* and does nothing about a load that stays over budget, which is exactly the case the arm was added for. **The two guards are not redundant after all — the cap alone is not sufficient.**

**2. The repaint policy made tab switches 2.3× slower.** The five switches cost 120575 and 120558 µs in A, against 279056 and 294043 µs in B. Per switch: 18.5–34.1 ms in A, 46.0–91.5 ms in B. This is the GUI-responsibility half of the owner's original complaint, reproduced and measured: **Fix B is a genuine regression in GUI latency**, and the damage-box `build_dl(&d->brp, d, 0, 0, &dl)` full-rebuild is the mechanism already identified for it.

**3. The repaint cost is not what causes the xruns.** This is the finding that overturns the previous record. B performs **2.3× more repaint work than A** and has **56 % fewer xruns**. If repaint cost were the cause, the arm with the slower repaints would drop out more, not less. The xruns track the governor: 8 trips → 618–620 xruns, 0 trips → 274. `render_max` says the same thing — 85–136 ms when yielding, 6.3 ms when not.

**4. No tab switch caused a dropout in any run.** All twenty switches across four runs logged `xruns+0`, in *both* arms, while the sessions accumulated 274–620 xruns between them. So the tab-switch xruns that started this investigation on 2026-10-01 **are not reproduced by this protocol at all**. Whatever produced them is still unexplained; the scripted tab cycle is not the trigger.

## The number that was wrong, and how

The withdrawn claim rested on "the render task uses 32.8 µs of every 5333 µs, about 0.6 % of a core", so it could not be starved. Recomputed from these runs, per buffer while actually playing:

```
A1  2869.3 us/buffer = 54% of the 5333 us period
A2  2872.9 us/buffer = 54%
B1  2806.2 us/buffer = 53%
B2  2806.8 us/buffer = 53%
```

**The render task consumes 53–54 % of every buffer period during playback.** The 0.6 % figure is not wrong as arithmetic — it is `render_total=153734 ms / 4683315 buffers` — but it came from a 7-hour session that was ~99 % idle, and I applied it to a playing session. Half the period is gone before the GUI gets a look in, and the wake latency says the task is still routinely late: **mean 881–894 µs, max 5802–5810 µs against a 5333 µs period**, so the worst wake is a full period late on its own.

This is the error the whole episode turns on, and it is a specific one worth naming: **an average taken over a session that was mostly idle describes the idle case, and quoting it about a busy one is a category error that no unit or formula reveals.** The `wake_latency` metric exists so that "late" and "slow" stop sharing an explanation.

> **Status: Outdated (superseded 2026-10-02 by
> [the render-stage breakdown](2026-10-02-render-stage-breakdown-voices-are-94-percent-not-the-fx-chain.md)).**
> The correction above is right that the 0.6 % figure was an idle-session average,
> but the replacement figure is *also* a session average — just a less idle one.
> These runs were 39 % idle (8515 buffers = 45.4 s, of which 27.7 s playing), so
> `2806 = 0.61 × 4590 + 0.39 × 9`. The per-playing-buffer cost is **4590 µs =
> 86 % of the 5333 µs period**, measured on a play-only harness where the idle
> fraction is 8.9 %. `render_max` agrees across every `-O0` run ever taken on this
> guest (6310–6359 µs), which is what proves the workload was constant and the
> average difference was an artefact of the protocol's idle time.
>
> The *comparison* in this record is unaffected and remains valid: both arms ran the
> same protocol, so the governor-arm win stands. Only the absolute throughput figure
> must not be quoted as a per-playing-buffer cost.
>
> A second reading has also changed. The `wake_max` 5802–5810 µs here was read as a
> scheduling problem. At `-O2` the same measurement is **37 µs**
> ([optimisation level alone removes every xrun](2026-10-02-optimisation-level-alone-removes-every-xrun-on-the-dell.md)),
> so most of it was a long render, not an unfair wake.

## `arm_us` paid for itself immediately

B2's first heartbeat reads `overloads=0 … arm_us=261317`: the load was continuously over 850 per mille for **261 ms** and the arm reset it. B1's reads `arm_us=0`. With `overloads=0` alone those two are indistinguishable from "never went over budget", which is how a 7.7 s session previously got over-read. The field cost four lines and converted an ambiguous zero into a measurement — and it also shows how close the arm is: at a 1 s arm this session would have tripped.

## Method findings

- **A counter that improved is not a fix.** The xrun count fell 56 % and the GUI got measurably worse in the same change. Two axes, one metric, and the metric was only watching one of them.
- **"Both fixes target the same symptom" was wrong, and only measurement said so.** Fix A is a large win; Fix B is a regression. Splitting them was the only way to see it — as a pair they would have been reported as a partial success twice.
- **Zero can hide a hundred events.** `overloads=0` was the single most misleading number in the previous record, and the fix is one unsynchronised integer.
- **Verify a duty cycle over the window you are reasoning about.** Averaged over 7 hours, 99 % of it idle, the render task looks free. Averaged over 4 seconds of playback, it is the largest consumer on the machine.
- **Derive injection coordinates from the geometry, then confirm them against the event log.** `ri_geo_px(q, 3) = q*3/8` plus the panel's vertical stack gives Play to within a few pixels — but the transport panel sits at screen x 347–947, not at the root's left edge, which cost three failed probes. Confirming with `TR PLAY` before running the protocol is what made it safe; a click verified by its own event log is not a blind click.
- **Never start a second instance.** Two RIAPPs contend for the sound card, and the second launch truncates the ev-log, which silently destroys the first run's evidence. Close by index (`--ui-close #0`), never by title — same-titled windows make the title form refuse.

## Standing gaps

- One song, the built-in default. ZN was not loaded, and the earlier ZN numbers are not comparable to these.
- The ~0.7 s gap is not the advisor's 4 s, and neither is a human-paced session.
- Arm B is not a clean "cap vs arm" isolation: it also carries the repaint policy and the P9 features. The repaint policy's 2.3× tab-switch cost is therefore attributable to B as a whole, not isolated by this run.
- `wake_max` is a single worst sample per run. A histogram would show whether the 5.8 ms tail is one event or a distribution, which decides whether it is worth bounding.
- The in-memory ring is still not built. With `RIAPP.LOG` now on the USB stick, live per-line writes are slower than they were on `RAM:`, so the perturbation risk is higher than when it was deferred.
- The 2026-10-01 tab-switch xruns remain unexplained: zero here, in both arms.

## See Also

- [The xrun fix worked on its own terms and still made it worse](2026-10-02-dell-xrun-proof-result-and-the-cost-per-repaint.md) — the record this one corrects on its central causal claim
- [Where RIAPP.LOG lives, and why file size cannot tell you which lane you built](2026-10-02-dell-sticky-log-location-and-size-heuristic-correction.md)
- [Dell xruns while playing and switching tabs: the governor's trip law and the repaint policy](2026-10-01-dell-xruns-governor-arm-and-repaint-policy.md)