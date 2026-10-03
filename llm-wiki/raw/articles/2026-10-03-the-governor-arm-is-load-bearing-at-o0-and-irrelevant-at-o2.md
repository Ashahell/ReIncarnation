# The governor arm is load-bearing at `-O0` and irrelevant at `-O2` — so `-O2`'s slow repaint is real

- Source: ReIncarnation session, 2026-10-03 (opencode lane; the coupling the `-O2` A,B,B,A left unresolved)
- Collected: 2026-10-03
- Published: 2026-10-03
- Prior: [`-O2` is not a free win](2026-10-03-o2-is-not-a-free-win-it-removes-every-xrun-and-makes-the-tab-cycle-2-4x-slower.md) (which recorded the coupling as a hypothesis), [Scripted A,B,B,A](2026-10-02-dell-scripted-ab-abba-governor-arm-wins-repaint-policy-regresses.md) (the arm's original evidence), [Bounding the damage-box build](2026-10-02-damage-box-bounded-build-1p9x-less-gui-work-no-xrun-change.md)
- Evidence: [`docs/evidence/audio/audio-failure-and-deadline-2026-10-02.md`](../../../docs/evidence/audio/audio-failure-and-deadline-2026-10-02.md) §"Arm-disabled cells" — every figure verbatim, per-tab costs, the missing heartbeat
- Commit: `a98691a` tree; variants `RIAPP-f2`/`RIAPP-f2noarm`, `RIAPP-f0`/`RIAPP-f0noarm`

## The question

The `-O2` A,B,B,A ended with a coupling it could not resolve:

> the arm going quiet does not make the repaint regression irrelevant, it makes
> it **visible** — at `-O0` six governor trips are what yield CPU to Intuition,
> so the tab cycle is fast *because* the audio path is collapsing

Two readings, with opposite consequences:

- **The arm was masking it.** Then `-O2`'s slow repaint is an artefact, and
  fixing the damage-box full-rebuild is *not* on the critical path for shipping
  `-O2`.
- **`-O2` genuinely slowed repaint.** Then the full-rebuild fix *is* on the
  critical path, and `-O0`'s apparent GUI advantage was bought with xruns.

## The experiment

Remove the arm; leave everything else alone. **One line, at the one place that
consults the flag** — `audio_io/audio_ahi_live.c`, where the render task drops
below the UI:

```c
-        if (ri_livedrv_overloaded(&lv->drv) != yielding) {
+        if (ri_livedrv_arm_enabled() && ri_livedrv_overloaded(&lv->drv) != yielding) {
```

`governor()` is untouched, so `overloads` and `arm_us` keep reporting what
*would* have tripped. Both binaries built from one clean `git archive HEAD` tree
so the arm gate is the only difference from its control: `-O2` 874,744 →
874,896 B, `-O0` 1,094,648 → 1,095,592 B, `r12moves` unchanged in each pair.

## The result

| arm | cell | xruns | `overloads` | `render_max` | **5-tab total** |
|-----|------|-------|-------------|--------------|-----------------|
| `-O0` ON | A1 | 1,614 | 6 | 91,932 µs | 99,432 µs |
| `-O0` ON | A2 | 1,601 | 6 | 92,021 µs | 99,786 µs |
| `-O0` OFF | T2 | **11,852** | — | 9,464 µs | **38,214,843 µs** |
| `-O0` OFF | T4 | **11,955** | — | 9,499 µs | **27,701,899 µs** |
| `-O2` ON | B2 | 0 | 0 | 4,101 µs | 241,791 µs |
| `-O2` ON | B3 | 0 | 0 | 4,117 µs | 241,632 µs |
| `-O2` OFF | T3 | 0 | 0 | 4,098 µs | 240,873 µs |
| `-O2` OFF | T5 | 0 | 0 | 4,125 µs | 260,392 µs |
| `-O2` OFF | T1 | 0 | 0 | 4,110 µs | 514,356 µs * |

\* T1's MIX tab alone was 357,611 µs against 85,516 in T5 — excluded by name;
T3 and T5 then agree to 8 %.

## What it settles

**At `-O0` the arm is load-bearing in both directions, far more strongly than
the hypothesis claimed.** Removing it multiplies xruns **7.4×** (1,601 → 11,955)
and takes the five-tab cycle from 99.6 ms to **27.7–38.2 seconds** — individual
switches of 8.4 s and 24.2 s. Not 2.4× slower; the GUI is effectively gone.

And the sharpest single statement of what the arm is for is an inversion:

```
render_max:  91,932 us (arm ON)  ->  9,464 us (arm OFF)
```

`render_max` **falls** when the arm is removed. The render task no longer yields
below the UI, so it never accumulates a 92 ms single-buffer stall — it simply
never gets to run, and the xruns and the GUI latency both rise instead. The
92 ms figure was the symptom of yielding; removing the yield does not fix the
work, it fixes the measurement and breaks the machine.

**At `-O2` the arm is irrelevant.** `overloads=0` with it on or off, and the tab
cycle is unchanged (240.9–260.4 ms against 241.6–241.8 ms). Nothing to mask:
the arm was never engaging at `-O2` in the first place.

**Therefore `-O2`'s slow repaint is genuine, not an artefact.** The second
reading is correct, and it has a consequence: **the damage-box full-rebuild fix
is on the critical path for shipping `-O2`.** There is no configuration in which
`-O2` is simply free, and the repaint work cannot be deferred as "an artefact of
the arm".

## What this means for shipping

Three facts now, each measured:

1. `-O0` is not viable — 1,601 xruns and a 92 ms `render_max` on the real song.
2. `-O2` fixes the audio completely — 0 xruns — and makes the tab cycle 2.43×
   slower.
3. **The arm must stay.** Without it `-O0` does not merely degrade, it collapses
   into tens-of-second tab switches with 7.4× the xruns.

So the shipping configuration is `-O2` **plus** the damage-box full-rebuild
fixed, and the arm intact. That is a build plan, not a flag.

## A new observation: the MIX and LEVI tabs are bimodal

Twice now a single tab switch has been an order of magnitude above its
neighbours — B1's LEVI at 107,029 µs against 52,475/52,545, and T1's MIX at
357,611 µs against 85,516. Both at `-O2`, both excluded by name, both resolved
by a re-run. It is the same phenomenon twice on two different tabs, so it is
worth naming rather than filing as two outliers: **the heaviest section
redraws are intermittently several times their median cost.** Not chased here.

## Method findings

- **The discriminating cell was not the one the hypothesis named.** The obvious
  test — arm-disabled `-O2` — is close to a no-op by construction, because
  `overloads=0` there means the arm was already never yielding. The cell that
  discriminates is arm-disabled **`-O0`**, where the arm demonstrably fires six
  times. Running only the obvious cell would have "confirmed" the hypothesis by
  measuring nothing.
- **A prediction can be too weak to be worth testing.** "The arm going quiet
  makes the regression visible" predicted a modest effect; the truth is a
  collapse. Recording the prediction as written is what made the size of the real
  effect visible.
- **`render_max` is not a health metric.** It improved by 10× in the arm-OFF
  cells while the machine got much worse. A metric that moves the right way when
  the system breaks will eventually be optimised against by mistake.
- **A missing heartbeat line is evidence.** The `-O0` arm-OFF cells contain no
  `hb:` line at all — the heartbeat is printed from the GUI task, and the GUI was
  too starved to print it. Its absence was the first sign the cells were not
  merely slow but broken.