# The `-O2` tab cost is preemption of prebuilt MUI code — not our work, and not the section repaint

- Source: ReIncarnation session, 2026-10-03 (opencode lane; the named probe for the unlocalised repaint cost, run after a host **and** guest reboot)
- Collected: 2026-10-03
- Published: 2026-10-03
- Prior: [Correction: the `-O2` tab cost is not the damage-box build](2026-10-03-correction-the-o2-tab-cost-is-not-the-damage-box-build.md) (which named this probe), [The governor arm is load-bearing at `-O0`](2026-10-03-the-governor-arm-is-load-bearing-at-o0-and-irrelevant-at-o2.md) (the `wake_max` half of this), [`-O2` is not a free win](2026-10-03-o2-is-not-a-free-win-it-removes-every-xrun-and-makes-the-tab-cycle-2-4x-slower.md)
- Evidence: [`docs/evidence/audio/audio-failure-and-deadline-2026-10-02.md`](../../../docs/evidence/audio/audio-failure-and-deadline-2026-10-02.md) §"The page switch, split three ways" — every figure verbatim
- Binaries: `RIAPP-p0` 1,099,544 B / 41 `r12moves`; `RIAPP-p2` 878,528 B / 285 — both instrumented from `HEAD`, flag the only variable

## The probe

`tab_switch()` does three things. Time each, with the same `ReadEClock`
arithmetic the surrounding `TAB` line already uses:

```c
SetAttrs(s_pages, MUIA_Group_ActivePage, (IPTR)g, TAG_DONE);   -> page_us
for (k = 0u; k < RI_TAB_COUNT; k++)
    if (s_tabs[k])
        SetAttrs(s_tabs[k], MUIA_RArt_Active, ...);            -> tabs_us
rail_for_tab();                                                 -> rail_us
```

`app/riapp.c` in the repo is **untouched**; the instrumentation lives only in a
clean `git archive HEAD` tree, as the arm-disabled build did.

| arm | cell | `page_us` | `tabs_us` | `rail_us` | `split_us` |
|-----|------|-----------|-----------|-----------|------------|
| `-O0` | A1 | 83,435 | 1,356 | 14,595 | 99,390 |
| `-O0` | A2 | 83,423 | 1,360 | 14,584 | 99,371 |
| `-O2` | B1 | 218,735 | 1,322 | **259,907** | 479,966 |
| `-O2` | B2 | 200,469 | 4,791 | 40,937 | 246,200 |

## What it settles

**One candidate eliminated.** `tabs_us` is **identical** at both flags — 1,356 /
1,360 against 1,322 — and is ~1 % of the tab cost either way. The five
`MUIA_RArt_Active` writes are **ruled out**.

**The bulk is `page_us`, and it is a prebuilt-library call.**
`SetAttrs(MUIA_Group_ActivePage, …)` is MUI's own code. `-O2` does not change
MUI: the flag applies to our 81 TUs, and `MUI_MakeObject` and Intuition come from
the SDK unaltered. Yet the call costs **83.4 ms → 200–219 ms, 2.4–2.6×**, with
the `-O0` pair agreeing to **0.014 %**. Identical code, 2.5× the time.

**And the section repaint is three orders of magnitude short of it.** Damage-box
repaints (`box_steps`, n/sum/max) cost 613–620 µs across ~530 samples at `-O0`
and 4,373–4,374 µs across ~400–460 at `-O2` — about 1 µs and 11 µs each, against
`page_us` of 21 ms and 50 ms per switch. Unattributed full repaints (`box_none`)
are 139–600 samples at `-O0` and **zero** at `-O2`; `build_avg` is 157–520 µs
against 238–324 µs. The bounded build is neither the cost nor the fix, which
confirms the correction from a completely independent direction.

## The correction this produces — and it is mine

The previous pass concluded **"not contention"**, reasoning that `render_total`
is *half* at `-O2` (15,933 ms against 31,540 ms). That was the wrong measurement
for the question. `render_total` is CPU **consumed**; it says nothing about
**preemption pressure**. The measure that answers it is how late the render task
is — and `wake_max` inverts completely between the two arms:

```
-O0   wake_max = 5,820 us / 5,816 us     the audio task is the one WAITING
-O2   wake_max =   398 us /   436 us     it never waits: 2.6 ms of a 5,333 us
                                          period, so it preempts the GUI ~187x/s
```

At `-O0` the render task overruns its period, the arm trips six times, and the
task is pushed **below** the UI — so MUI runs unimpeded and the tab cycle is
99 ms. At `-O2` the task finishes early, **never yields**, and takes the CPU from
the GUI about 187 times a second.

**So `-O2`'s tab cost is preemption of prebuilt MUI code by an audio task that
is no longer late enough to be told to yield.** Not more work, not slower work,
not the section repaint, and not something a build can fix. It is the governor
arm doing exactly its job — and the cost surfacing somewhere it was never
measured.

This also re-reads the arm record. `arm_us=0` at `-O2` was recorded as "the arm
never engages"; the fuller statement is that **the arm's absence is the cause of
the GUI regression it was measured against.** The arm is load-bearing at `-O0`
and its correct un-necessity at `-O2` is what costs the GUI.

## The bimodality, localised

`rail_us` is **5 µs on page 4 in every cell** — one arm, one flag, both runs —
and 3,209–6,812 µs at `-O0` against 9,949–239,355 µs at `-O2`. B1's single
239,355 µs is the outlier behind its 259,907 total; B2 without it totals 40,937.
The spikes are in `rail_for_tab()`, five
`SetAttrs(s_devbtn[d], MUIA_ShowMe, …)`. B2 also shows a `tabs_us` spike (3,835
against a ~350 baseline), so **the spikes are in `SetAttrs` on our own widgets
generally, not in one call.** That is the next thing to instrument, and it is now
a bounded question rather than an open one.

## What this means for shipping

Sharper than before, and less encouraging:

- `-O0` — not viable: ~1,590 xruns, 92 ms `render_max` per tab cycle.
- `-O2` — 0 xruns, but the GUI is preempted ~187×/s by an audio task that no
  longer yields.

**There is no flag that is simply better, and the trade is now fully priced:**
audio correctness against GUI latency, where the GUI cost is scheduling and not
code. `-O2` is the only arm that plays audio correctly; the question is whether
the GUI latency is acceptable, and the next lever — if one is wanted — is to make
the render task yield *without* the arm's 2-second over-budget trigger, which is
a policy change rather than an optimisation.

## Method findings

- **"CPU consumed" is not "preemption pressure".** The previous pass used
  `render_total` to rule out contention and got the wrong answer, because the
  quantity that matters is how often and how hard the audio task takes the CPU
  *from* someone else. `wake_max` was already logged, in every run, and inverts.
- **Instrumenting three lines answered in one pass what two experiments could
  not.** `page_us` / `tabs_us` / `rail_us` eliminated a candidate, attributed the
  bulk, and localised the bimodality — none of which was reachable from totals.
- **Ruling a candidate out is a result.** `tabs_us` being *identical* across arms
  is as informative as a cost being high, and it is the one thing that was
  unambiguously clean.
- **A correction that reframes an earlier finding is worth more than a new
  number.** The previous record's "not contention" is not merely wrong; its
  replacement explains both the arm result and the repaint result at once.

## Standing gaps

- **The spikes are localised but not explained.** `SetAttrs` on our own widgets,
  bimodal, up to 239 ms. Next probe named.
- **The preemption trade has no flag-only answer.** If GUI latency at `-O2` is
  unacceptable, the lever is a yield policy that triggers on *GUI latency* rather
  than on 2 s of continuous over-budget audio — an owner decision, since it
  trades audio for responsiveness on purpose.