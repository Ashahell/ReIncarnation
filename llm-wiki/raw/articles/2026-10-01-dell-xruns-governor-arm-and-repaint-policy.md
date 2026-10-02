# Dell xruns while playing and switching tabs: the governor's trip law and the repaint policy (owner 2026-10-01)

- Source: ReIncarnation session, 2026-10-01 (opencode lane, the P9 Leviasynth slice, paused mid-P9e)
- Collected: 2026-10-01
- Published: 2026-10-01
- Plan: `docs/superpowers/plans/2026-09-30-leviasynth-fidelity-plan.md` (§P9 interlude), ledger in `docs/2026-09-24-improvement-todo.md` §12.11
- Prior: [2026-10-01-clipping-comp-limiter-tab-stall-priority.md](2026-10-01-clipping-comp-limiter-tab-stall-priority.md) (the pri-21 render task), [2026-10-01-dell-zn-telemetry-song-load-diagnosis.md](2026-10-01-dell-zn-telemetry-song-load-diagnosis.md) (the ZN load baselines)
- Commit: `3dadda7` (unpushed at collection)

## What the owner reported, and what the two logs said

The owner heard dropouts on the Dell while playing and switching tabs. The deployed image was `RAM:RIPP9`, whose on-device hash was verified as `cdcf85c`. The app holds `RAM:RIAPP.LOG` locked while running, so the owner quit the app to let both logs be pulled:

- `/tmp/opencode/ri_p9_ev.log` — 484 B, 13 events, 2 KiB RAM event volume (`RIAPP_EVLOG=RAM:`)
- `/tmp/opencode/ri_p9_dell4.log` — 16630 B, 170 lines, closing with `RIAPP closed: buffers=216272 xruns=1408 render_max=100583 us render_total=54548 ms period=5333 us`

**The pri-21 fix from the earlier lane held.** All seven `TAB` events logged `xruns+0`:

```
ev  4 30338 TAB page=2 us=18495 xruns+0     (stopped)
ev  6 57792 TAB page=4 us=30672 xruns+0     (playing)
ev  7 58120 TAB page=2 us=20840 xruns+0
ev  8 58407 TAB page=1 us=83474 xruns+0
ev  9 58803 TAB page=0 us=23310 xruns+0     (TR STOP at 59286)
ev 12 211195 TAB page=1 us=20518 xruns+0    (after TR PLAY at 209690)
ev 13 211461 TAB page=2 us=17038 xruns+0
```

So six of the seven switches were while playing and cost nothing, and 13 minutes of playing with no tab switch were also clean. **A single tab switch does not cause the dropouts.** The xruns are not in any `TAB` line; they are in the heartbeat's cumulative counter, and they only move in two windows, both of which are "playing *and* switching tabs":

| window | buffers | xruns | `overloads` |
|---|---|---|---|
| 1 | 55227 → 59586 | 0 → 485 | 0 → 9 |
| 2 (13 min later) | 209690 → 216272 | 485 → 1408 (**+923**) | 9 → 22 |

## The measurement that identifies the cause

1. **923 xruns against 630 full repaints is 1.47 xruns per repaint** (window 2's `draw:` lines, `n=` summed over the six heartbeats in the window). That ratio is the whole diagnosis: a repaint costs about one and a half dropouts, which is what a *pre-emption* looks like and not what a CPU shortage looks like.
2. **Repaints are far too small to be CPU exhaustion.** The same `draw:` lines put `full_avg` at 2708..6548 µs per full repaint and `part_avg` at 185..525 µs per partial one, at 10 Hz, against a 5.3 ms buffer period. Window 2's six heartbeats cover ~2.3 s, so the repaints are a few per cent of one core in total; `blit_max` 73872 µs and `full_max` 26787 µs are single worst cases, not a duty cycle.
3. **So the repaint is pre-empting the render task rather than competing with it.** The only configuration in which that happens is the load governor's fallback: `AU_LIVE_PRI_YIELD -1` puts the render task *below* the UI. Under it every repaint wins the CPU.

> **Status: Disputed** (2026-10-02)
> Steps 1 and 2 hold; step 3's exclusion of the cost explanation does not. The on-target proof of both fixes came back on 2026-10-02: with Fix A live, `overloads` fell 22 → 0 and with Fix B full repaints fell 1037 → 15, and the owner then reported a *less* responsive GUI and *choppier* audio. The remaining xruns (407, all inside a 7.8 s play burst) follow a window of 119 partial repaints averaging 4036 µs — 76 % of one buffer period — plus one at 303098 µs. The priority inversion was real and was a second cause, but the repaint *cost* underneath it was the larger one and had been masked by the pri -1 fallback. See [the 2026-10-02 record](2026-10-02-dell-xrun-proof-result-and-the-cost-per-repaint.md).
4. **And the governor was tripping far too often to be a safety net.** `load` in the heartbeat read 585..1083 per mille against `RI_LIVEDRV_OVER_PM` 850, with the 1/8 EMA smoothing, so the threshold was being crossed constantly; the run logged 22 overload entries in 19 minutes (9 in the first window, 13 in the second), i.e. a trip roughly every 52 s, with each trip holding the task at pri -1 for `RI_LIVEDRV_OVER_US` (2 s).

Note the shape of the evidence: the pri-21 change is *confirmed* by the per-switch `xruns+0` lines, and the pri -1 fallback is *implicated* by a ratio and a duty cycle rather than by any direct log line. There is no "governor yielded" line in the heartbeat — the inference is arithmetic over fields that were already there.

## Fix A — a trip needs a *sustained* overload (`app/core/live_driver.h/.c`)

The threshold, the per-buffer load cap (1200), the 2 s hold and the pri -1 floor are all unchanged: a machine that genuinely cannot keep up must still yield. What changed is *when a trip may start*.

- `struct RILiveDriver` gains `uint64_t over_run_us` — task-side, the continuous over-budget time.
- `#define RI_LIVEDRV_ARM_US 2000000u` — the arm: 2 s.
- The governor accumulates `over_run_us += period` while `load_pm >= RI_LIVEDRV_OVER_PM`, and returns while `over_run_us < RI_LIVEDRV_ARM_US`. **Any** buffer under the threshold resets the arm to 0 and returns.
- The arm is zeroed in `ri_livedrv_init`, cleared when the probe ends the hold, and zeroed again at the trip itself, so the hold-then-probe path cannot inherit burnt time and each entry needs its own 2 s.

The owner was asked about the pri -1 floor itself and deferred it ("Leave it, diagnose first"), so the floor stays and this fix is deliberately the narrow one.

## Fix B — the panel repaints only what can show the change (`gui/panelui.h/.c`, `app/riapp.c`)

The 100 ms tick (`meter_round`) full-refreshed **all 18 non-chase, non-MASTER canvases** while playing, to service a live feed whose answer was a single boolean. The first job was to find out what the artwork actually reads from that feed, and the answer is very small:

- `panel->playhead`, through `ri_art_chase` — called from the 808 and 909 sections and nowhere else. The 303 pattern pages have no step keys (`RI_S303_STEP 26` is a keyboard step cursor, and each `RI_SEC_PAT_*` page has only 5 controls), and `RI_FOCUS_COUNT` is 4, so the Levi has no chase lamp at all.
- `panel->focus` — in the click path (`gui/draw/art_pat.c:60`), not in the tick.
- the transport's Song Position display, through `ri_str_follow`.
- `panel->playing` — **read by no art**.

So the feed now says *why* it changed, and the policy is a separate pure function:

```c
#define RI_PANEL_CH_PLAYHEAD 0x1u   /* a focus step moved: the drum lamps */
#define RI_PANEL_CH_PLAYING  0x2u   /* the transport playing edge */
#define RI_PANEL_CH_FOLLOW   0x4u   /* the Song Position display followed */
#define RI_PANEL_CH_TAP      0x8u   /* a held delete-tap edited a step row */

#define RI_STALE_NONE  0
#define RI_STALE_STEPS 1
#define RI_STALE_BAR   2
#define RI_STALE_ALL   3
int ri_panel_live_stale(uint32_t mask, uint32_t section);
```

- `RI_PANEL_CH_TAP` → `RI_STALE_ALL` for exactly the three sections that own step keys (808, 909, Levi, derived from the registry), else `NONE`. A delete-tap is the one change that edits pattern data, so it can appear anywhere on its page.
- `RI_PANEL_CH_FOLLOW` → `RI_STALE_BAR`, transport only.
- `RI_PANEL_CH_PLAYHEAD` → `RI_STALE_STEPS`, 808 and 909 only.
- `RI_PANEL_CH_PLAYING` → nothing, and a mask with no known bit → nothing.

`meter_round` dispatches on it: `NONE` skips the canvas entirely, `STEPS` keeps the existing old-lamp-plus-new-lamp box repaint (including the `st < 0 || st > 15 || !g` fallback to a full refresh), `BAR` refreshes the `RI_STR_BAR` box from `ri_geo_bbox` (which unions the display rect with the two arrow steppers, so the box is slightly larger than the digits and blits one small rectangle — `ri_rsection_refresh_box` rebuilds the display list on the CPU and replays it clipped to the box), and `ALL` does the full refresh. The blanket refresh survives only behind a delete-tap.

## Proof

- `scripts/ri_audit.sh` → `AUDIT 0/0 PASS` (includes the AROS RIAPP link gate, which is what covers `app/riapp.c` — there is no host test for it).
- ASan/UBSan clean on `t71_panelui`, `t72_livestate`, `t88_live_driver`, `t60_ctlreg`, `t61_panelgeo`, `t69_secttr`.
- Mutation: **11/11 killed** in `mut_fixA.txt` (t88) and **14/14 killed** in `mut_fixB.txt` (t71), full `all` rebuild per mutant.
- t72 followed `ri_panel_live`'s new return type (three `== 1` assertions became bit tests).
- **On-target proof is still open.** The fix has not been on the Dell; the owner's symptom is not closed until a redeploy under a new RAM name and a play-and-switch run compared against the baseline above.

> **Status: Outdated** (2026-10-02)
> Answered, and the answer is a qualified no. `RAM:RIPP9F` (`build=3dadda7`) was deployed and the owner played a 7.8 s session on it: `TR PLAY`, four `TAB` switches all `xruns+0`, 45 `CTL` knob events, `TR STOP`. Both fixes proved out on the metric each targeted — `overloads` 22 → 0, full repaints 1037 → 15, worst full repaint 115270 → 30601 µs, tab-switch xruns gone — and the owner still reported a sluggish GUI and choppy audio. Neither fix is wrong; the design that motivated both is.

## Laws

`t88_live_driver` (Fix A): a light load never trips; a capped burst (12 buffers of 84 ms, then 2000 at 562 per mille) never trips; 843 per mille sustained stays normal; a half second at 938 per mille stays normal; a 938 per mille run trips at buffer 393 (18 to cross 850 from 0 plus 376 of arm); the hold is ~375 buffers; the re-trip after a probe needs a fresh 2 s of its own; 1.9 s over budget, one light buffer, then 1.9 s more does **not** trip — the light buffer resets the arm.

`t71_panelui` (Fix B): each reason bit alone in a fresh fixture; a still playhead is silent; a change counts once on `RIPanelUI.changes` (the proof harness re-polls on that counter, `app/sectproof.c`) and a silent tick does not; the policy over **all 21 sections** for each mask; the tap set pinned against the registry's `RI_CK_STEP` sections, so a new step row cannot join the art without joining the repaint; an unknown bit such as `0x40` → `NONE`.

## Method findings (portable)

- **A boolean "something changed" cannot be turned into a repaint policy.** The information needed to decide *what* to repaint is destroyed at the producer, and the consumer then has to guess. Returning a *reason mask* costs four bits and makes the whole decision a pure function that can be tabled and unit-tested against the registry. The blanket refresh was not a lazy consumer; it was the only thing the boolean allowed.
- **Prove the shape of the live feed from the art, not from the struct.** `RIPanelUI` has plenty of live fields, and reading the struct would have said "repaint everything". Counting the call sites of `ri_art_chase` (two sections) and the one `ri_art_focus_bar` call is what produced the actual sets — and it found that `RI_FOCUS_COUNT` being 4 (no LEVI focus) is exactly why the Levi needs no chase repaint.
- **An "xrun per event" event log misses cumulative pauses.** The `TAB` lines were the most reassuring thing in the file — all seven clean — and the dropouts were only visible in the heartbeat's monotonic counter, in windows bounded by the transport events. When an event log says "no error here" and the owner says "it glitched", diff the cumulative counter against the event timeline before believing the event log.
- **Xruns per repaint, not xruns per second, named the bug.** The same two windows yield "1.47 xruns per repaint" and "5-12 % of a core". The first number says *pre-emption* (a cost per event), the second rules out *saturation* (a cost per time). Only together do they point at a priority inversion rather than at "make the drawing cheaper".

> **Status: Disputed** (2026-10-02)
> The method is sound and the ratio survives — re-read over the whole baseline run it is 1.12 xruns per full repaint (1165 / 1037), against 1.47 from the six-heartbeat slice. The inference drawn from it was wrong: "pre-emption" does not imply "not a cost problem". The render task averages 32.8 µs of every 5333 µs, i.e. it is *late*, not *crowded*, and a single long GUI operation is charged one dropout per audio period it spans — a 303 ms repaint is ~57 of them. "Rules out saturation" and "rules out per-event cost" are different claims and only the first was supported. Two further corrections from the same record: the `draw:` line carries **two** fields named `n=` (full, then partial), so a keyed parse silently reads the wrong one; and a damage-box repaint still pays a full `build_dl`, so avoiding the blit is not the same as avoiding the cost.
> **Status: Disputed** (2026-10-02, later the same day)
> The reasoning in the block above is sound and it is the reasoning that went wrong: a ratio of ~1 xruns per event plus a low duty cycle does exclude *saturation*, but it does not exclude a *per-event cost* as the mechanism, and this record read "excludes saturation" as "excludes cost". The same day, a third-party review established that a pri-21 render task cannot be pre-empted by the GUI at all, so the pre-emption story needed a named blocking point and none was found. Numbers survive, mechanism does not.

- **Two equivalent mutants in the Fix B set, both worth naming.** Replacing the `mask == 0` guard with `if (0)` survived, because with no bits set every later test is false anyway — the guard is redundant, not load-bearing. And a `--ARM_US 0u` mutant died as a `-Werror=type-limits` build break rather than behaviourally, which proves nothing about the laws; `1u` is the same mutation with a real effect. Both were replaced.

## Standing gaps

- On-target proof of the fix (needs the owner). **Answered 2026-10-02 and qualified: both fixes landed, the symptom did not close.** See the Status block under *Proof*.
- **Status: Refuted** (2026-10-02, later the same day) — the two fixes split cleanly under measurement. A scripted A,B,B,A (arm A = `4167e32` with the stall cap, arm B = this work) gave: **Fix A (the arm) is a large win** — xruns 618/620 → **274/274**, `overloads` 8 → **0**, `render_max` 85-136 ms → 6.4 ms; and **Fix B (the repaint policy) is a GUI regression** — five tab switches cost 120 ms → 279-294 ms, 2.3x slower. The two were shipped as a pair and would each have been reported as a partial success. Also: **no tab switch caused a dropout in any of the twenty switches** in either arm, so the tab-switch xruns that motivated this whole line of work are not reproduced by a scripted tab cycle. Record: [the A,B,B,A](2026-10-02-dell-scripted-ab-abba-governor-arm-wins-repaint-policy-regresses.md).
- The 303 pattern pages have no on-panel step row, so a 303 delete-tap still changes data that nothing on the panel shows; this predates the fix and is untouched by it.
- A held Delete during playback still repaints three canvases. That is the pre-fix behaviour, now bounded to the canvases that can show the edit.
