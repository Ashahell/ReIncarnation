# Correction: the `-O2` tab cost is not the damage-box build — it is 98 % outside the section repaint

- Source: ReIncarnation session, 2026-10-03 (opencode lane; checking a claim made in this lane two commits earlier, before acting on it)
- Collected: 2026-10-03
- Published: 2026-10-03
- Prior: [The governor arm is load-bearing at `-O0`](2026-10-03-the-governor-arm-is-load-bearing-at-o0-and-irrelevant-at-o2.md) (which carries the corrected claim), [`-O2` is not a free win](2026-10-03-o2-is-not-a-free-win-it-removes-every-xrun-and-makes-the-tab-cycle-2-4x-slower.md), [Bounding the damage-box build](2026-10-02-damage-box-bounded-build-1p9x-less-gui-work-no-xrun-change.md) (the fix that already landed)
- Evidence: [`docs/evidence/audio/audio-failure-and-deadline-2026-10-02.md`](../../../docs/evidence/audio/audio-failure-and-deadline-2026-10-02.md) §"Where the `-O2` tab cost actually is — and a correction"

## The claim being corrected

Two commits ago this lane concluded:

> the damage-box full-rebuild fix **is on the critical path for shipping `-O2`**
> … shipping configuration is `-O2` **plus** the damage-box full-rebuild fixed

It was repeated in the summary to the owner as the next step. It is wrong, and
three measurements say so independently.

## 1. The bounded build is already shipped

```
git merge-base --is-ancestor bb1c385 HEAD   ->  yes
```

`bb1c385` — *"bound the display-list build to the damage box (1.9× less GUI
work)"* — is an ancestor of HEAD, and `gui/widgets/rsection.mcc.c:233` passes the
damage box:

```c
build_dl(&d->brp, d, 0, 0, &dl, x0, y0, x1, y1);
```

There is no unbounded full-rebuild left to fix. The record that named this
mechanism is a record of a fix that **already landed**; the arm article and the
`-O2` article both cited it as outstanding work.

## 2. `build_avg` is cheaper at `-O2`, not dearer

| cell | arm | `full_avg` | **`build_avg`** | `blit_max` | `part_max` | 5-tab total |
|------|-----|-----------|-----------------|------------|------------|-------------|
| A1 | `-O0` | 2,275 µs | **179 µs** | 9,914 µs | 82,049 µs | 99,432 µs |
| A2 | `-O0` | 2,274 µs | **483 µs** | 9,819 µs | 82,419 µs | 99,786 µs |
| B2 | `-O2` | 4,565 µs | **195 µs** | 27,249 µs | 4,227 µs | 241,791 µs |
| B3 | `-O2` | 2,850 µs | **62 µs** | 4,118 µs | 3,860 µs | 241,632 µs |
| T3 | `-O2` off | 4,897 µs | **194 µs** | 30,620 µs | 4,200 µs | 240,873 µs |
| T5 | `-O2` off | 4,839 µs | **195 µs** | 28,033 µs | 4,224 µs | 260,392 µs |

The metric that would move if the build were the cost **moves the other way**:
195 µs at `-O2` against 179–483 µs at `-O0`. And in most `-O2` cells
`box_none` — the unattributed full-repaint bucket the older record blamed — is
**zero**.

## 3. The section repaint is 1–2 % of the tab-switch cost

```
  A1: 5 tabs = 99,432 us;  section full_avg = 2,275 us  -> section is 2 % of the tab cost
  B2: 5 tabs = 241,791 us; section full_avg = 4,565 us  -> section is 1 % of the tab cost
```

So the 2.43× cannot be in the section repaint, and cannot be fixed by bounding a
build that is already bounded and already 1–2 % of the cost.

## 4. And it is not CPU contention either

`render_total` is the CPU the render task actually consumed, and it is **half**
at `-O2`:

| arm | buffers | xruns | xruns % | `render_total` | per buffer | `wake_total` |
|-----|---------|-------|---------|----------------|------------|--------------|
| `-O0` | 5,537 | 1,614 | 29 % | 31,540 ms | 5,696 µs | 11,547 ms |
| `-O0` | 5,540 | 1,601 | 28 % | 31,478 ms | 5,681 µs | 11,515 ms |
| `-O2` | 6,068 | 0 | 0 % | 15,933 ms | 2,625 µs | 116 ms |
| `-O2` | 6,067 | 0 | 0 % | 16,293 ms | 2,685 µs | 117 ms |
| `-O2` off | 5,879 | 0 | 0 % | 15,557 ms | 2,646 µs | 112 ms |
| `-O2` off | 5,975 | 0 | 0 % | 15,933 ms | 2,666 µs | 115 ms |

The render task does **less** work at `-O2` and the GUI is 2.43× slower. A
*busier* render task cannot be what is slowing the GUI down — which also means
the arm result has a reading worth keeping: at `-O0` the GUI was fast partly
because the render task was pathologically starved (`wake_total` 11,547 ms,
`wake_max` ~5,820 µs), and `-O2` removes that pathology. **Part of what looked
like "fast GUI at `-O0`" was the audio thread failing to get CPU at all.**

## Where the cost actually has to be

`-O2` changes **only our 81 TUs**. MUI, Intuition and `graphics.library` are
prebuilt and byte-identical in both arms. So with the section repaint ruled out
(98 % of the cost is elsewhere), contention ruled out (the render task is
*less* busy), and the arm ruled out (last commit), the candidates are our own
code around the page switch:

```c
SetAttrs(s_pages, MUIA_Group_ActivePage, (IPTR)g, TAG_DONE);
for (k = 0u; k < RI_TAB_COUNT; k++)
    if (s_tabs[k])
        SetAttrs(s_tabs[k], MUIA_RArt_Active, (IPTR)(k == g ? TRUE : FALSE), TAG_DONE);
rail_for_tab();
```

— five `SetAttrs` on the tab strip, a group page switch, and a rail rebuild —
together with the per-tab **bimodality** already recorded (MIX 357,611 µs vs
85,516; LEVI 107,029 µs vs 52,475). A bimodal, page-dependent cost in code that
runs per switch fits both observations better than a uniform slowdown in a build
that is not the build.

**Not chased here.** Naming the honest next probe: instrument the four lines
above the way `box_*` already instruments the section, and see whether the cost
is in the group switch, the five `SetAttrs`, the rail, or the bimodal tail.

### A floor caveat on the device figures quoted here (2026-10-03)

This record quotes `dstg levi` and `dstg block` — 350/660 µs at `-O2`,
737/1370 µs at `-O0`. Those are **above the timer floor** and remain usable.

It does **not** quote the `lev-*` sub-stages, which is fortunate: on the Dell
those read `lev-arp` 4 µs, `lev-seq` 4 µs and `lev-mix` 4 µs, and there is **no
`n` on those rows**, so a sub-stage reading at the floor is indistinguishable
from one that never ran — the gap
[the LEVI sub-split record](2026-10-03-the-levi-sub-split-measured-arp-seq-voice-and-mix-are-all-at-the-floor.md)
names explicitly. Only `lev-voice` (306 µs at `-O2`) clears the floor on the Dell.
**Any future attempt to split Dell LEVI further must add a count before trusting
a number.**

## Method findings

- **Check a conclusion against the tree before building on it.** `bb1c385` was
  four commits old and in the history; one `git merge-base --is-ancestor` would
  have caught it. The claim was plausible, matched a real older record, and had
  been repeated to the owner as the next step.
- **The metric that would move if a hypothesis were true is worth checking
  first.** `build_avg` existed in every log already. It moved the *other* way,
  which falsified the hypothesis in one read and with no new instrumentation.
- **"98 % of the cost is elsewhere" is a stronger statement than "the named
  culprit is innocent."** Ruling the build out is not the same as locating the
  cost, and the record says which.
- **A counterintuitive result deserves its own check, not a shrug.** `render_total`
  halving while the GUI got slower is the kind of pair that invites a rationalised
  story. It is reported as the measurement, and the reading offered (a
  pathologically starved audio thread at `-O0`) is labelled as a reading.

## See Also

- [The `-O2` tab cost is preemption of prebuilt MUI code](2026-10-03-the-o2-tab-cost-is-preemption-of-prebuilt-mui-code.md) — runs the probe this record named, and supersedes its point 4
