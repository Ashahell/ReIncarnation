# Consultant review of the 1.36 s stall, and the two defects it found — verbatim, 2026-10-04

**Ingested:** 2026-10-04 into ReIncarnation `llm-wiki`
**Source:** third-party consultant session, 2026-10-04, run against the repository
at `/home/miller/Work/projects/ReIncarnation`. The consultant read the code, built
a host harness in `/tmp`, measured, and modified no repository file.
**Provenance:** figures below are the consultant's, quoted as returned. Anything
the main session re-verified independently is marked **[re-verified]**.
**Recorded in:** [the cache is refuted, `build_max ~= part_max` is a misread, and there is a live correctness bug](../articles/2026-10-04-consultant-cache-refuted-and-a-live-bbox-bug.md)

## 0. What was asked, and the verdict

Question asked: is instrumenting `ri_rsection_refresh_box_why` at the invalidation
site the right call, is the non-reset `dpr_run_now` coherent, is a display-list
cache the right lever for a burst of 25 real draws, Dell or host, and what is the
single cheapest discriminating experiment.

Verdict returned:

> **my step 1 is the wrong next move, and the display-list cache is already
> refuted — by your own first `boxrep` reading.**

## 1. The cache is dead — measured

Host harness, built at `-O0` (the stall's own build flag):

```
clip box (what the app asks for): 540,52..622,77  on a 632x78 compact canvas
CLIPPED build :    23.97 us  (90 commands survive)
FULL    build :    43.38 us  (829 commands)
```

Cache-key table as returned:

| cache key | hit rate | why |
|---|---|---|
| damage rect | **0 %** — MUI already collapsed 130→3 | `boxrep=3/0/130` |
| section state / `ri_dlist_hash` | **0 %** — the bar number changed every time | `ri_str_follow` |

> A cache can only skip work whose *result* is unchanged. Here the result changes
> on every single call by construction. The cache has nothing to win. **Do not
> build it.**

Prize, as bounded by the same harness:

> Even a *total* clip failure bounds `build_dl` at 43 µs. There is no input to
> this code that makes it cost 132 ms.
> 25 draws × 24 µs = **0.6 ms** of a window that cost 1.87 s.

`25 × 74648 = 1,866,200 µs`, so the stall window cost **1.87 s**: one 1.36 s draw
and ~21 ms across the other 24.

Origin of the invalidations, quoted from `gui/secttr.c:207`:

```c
int ri_str_follow(struct RISectTr *s, uint64_t start_tick, uint64_t sixteenths) {
    ...
    return bar != before;          /* gui/secttr.c:221 */
}
```

> `RI_STALE_BAR` comes only from `RI_PANEL_CH_FOLLOW`
> (`gui/panelui.c:274`), which `ri_panel_live()` sets only when `ri_str_follow()`
> returns non-zero — i.e. **only when the song bar actually advanced**. So all 133
> invalidations carried a *different bar number*, hence genuinely different pixels.

## 2. `build_max ≈ part_max` is a misread — the two rows quoted

> The evidence file has two rows:
> ```
> part_max=490609 part_avg=34551 n=15 build_max=490523 <- 99.8% build
> part_max=1364359 part_avg=74648 n=25  build_max=132813   <-  9.7% build
> ```
> `build_max ≈ part_max` holds for the 490 ms row and **fails by 10× for the 1.36 s
> row**. `dp_build_max` is reset per window (`app/riapp.c:2646`), so 132,813 µs is
> an upper bound on *any single build* in that window — including the 1.36 s draw.
> Therefore **≥1.23 s (90 %) of the worst draw was in `replay_dl_dmg` +
> `BltBitMapRastPort`, not in the build.**

And the process lesson, quoted:

> *"Check a conclusion against the tree before building on it."* The conclusion
> "the time is in the display-list build" was built on `build_max ≈ part_max`,
> which holds in one of the two rows that motivated it and fails by 10× in the
> other — the row that is actually the 1.36 s.

## 3. The instrumentation gap

> `blit_max` is written **only on the full-draw path**
> (`rsection.mcc.c:273`). The partial path records `dpw_*` and `dp_*` and returns
> at line 261 without ever timing its blit. So the one number that would have
> split your 1.36 s draw was never collected. This is a real hole, not a
> stylistic quibble.

## 4. Hypotheses, ranked, with discriminators as returned

> **H1 — preemption / a global stall. Overwhelmingly most probable.**
> Your wiki has already established this signature *twice*: "the `-O2` tab cost is
> preemption of prebuilt MUI code", and the governor article (at `-O0` the render
> task drops below the UI and the GUI looks fast; at `-O2` it preempts the GUI
> ~187×/s). The 1.36 s draw is the third instance. It is identical code doing
> 24 µs of work that took 1.36 s — a 55,000× ratio, on the same machine, same
> binary, same input. No code path produces that. Only the *absence of the CPU*
> does. […] *Cheapest discriminator, zero new instrumentation:* the stall window's
> `RIAPP stg` / `wake_max` line is already in the log you captured. If the render
> task shows a multi-hundred-ms excursion in the same window, it is a global
> stall, not your thread. **Re-read that one line before writing any code.**

> **H2 — `AllocBitMap` / bitmap churn.** `draw_frame:190-201` is the *only*
> allocation in the entire GUI draw path […] `allocs=` is already on your
> heartbeat line and was not quoted for the expensive row. […] It cannot explain
> a 25× burst though (it is one-shot per relayout), so it is a contributor at
> most.

> **H3 — `build_dl` scaling with the damage rect.** *Already falsified*, no
> experiment needed: the box is `ri_geo_bbox(TRANSPORT, RI_STR_BAR, 0)` — pure
> geometry, constant, 83×26 px, 4.4 % of a 632×78 canvas. Measured cost 24 µs,
> 90 commands. It cannot scale, and it does not.

> **H4 — ExAll-style walk.** Dismissed by your own data: MUI performed that walk
> 130 times in the quiet window and it never showed up. Not it.

## 5. The correctness bug, as found

> `app/riapp.c:1253` asks for the Song Position box at **zoom 0**:
> ```c
> ri_geo_bbox(g, (uint16_t)((RI_SEC_TRANSPORT << 8) | RI_STR_BAR), 0, &x0,&y0,&x1,&y1)
> ```
> But the transport canvas is **always** created at `RI_GEO_ZOOM_COMPACT` (= 3):
> `riapp.c:2103` and `riapp.c:1880`, both unconditional for `C_TR`. `build_dl`
> meanwhile builds at `dd->zoom` (= 3). The clip box is therefore in a different
> coordinate space from the art it is supposed to clip:
> ```
> zoom0 box (what the code asks for):      540,52..622,92  -> 90 cmds
> compact box (where the BAR really is):   404,38..467,70  -> 97 cmds
> overlap: NO -- they are disjoint
> ```
> **The `RI_STALE_BAR` repaint is refreshing a region that does not contain the
> Song Position display.** […] `box_bar` has never measured the Song Position. It
> has measured a fixed 24 µs no-op repaint of an irrelevant rectangle. The BAR
> path is a *metronome* firing ~0.83 Hz, and it is a **witness** to the stall, not
> its cause.

> Also check the same hardcoded-`0` pattern at the two other call sites:
> `app/riapp.c:1211` (master meters) and `app/riapp.c:1282` (step lamps). All
> three are wrong whenever `s_zoom != 0`, and `ri_zoom_clamp` will move it on a
> resize.

**[re-verified]** The main session reproduced the disjointness with its own host
probe before making the fix:

```
asked  (zoom 0)       : 540,52..622,92
real   (COMPACT = 3)  : 404,38..467,70
disjoint: YES - the repaint misses the art
```

and confirmed the canvas assignment at `app/riapp.c`:

```c
        for (i = 0; i < C_N; i++)
            s_zoom[i] = (i == C_TR) ? RI_GEO_ZOOM_COMPACT
                : (i == C_LEVI) ? ri_zoom_levi(z, sw, sh, RIAPP_CHROME_W, RIAPP_CHROME_H) : z;
```

with `#define RI_GEO_ZOOM_COMPACT 3` (`gui/panelgeo.h:51`).

## 6. The consultant's answers to the questions asked

> **Is instrumenting at the invalidation site the right place?**
> Right question, wrong place for *this* decision. The invalidation site sees the
> *request*; it cannot see whether the **state** changed between two identical
> requests — which is the only thing a cache could key on. That is why `dpr_new=0`
> means "the rect never changed", **not** "the work was identical". […] Keep
> `boxrep` as a *caller-spam* indicator, which is genuinely useful, and stop
> treating it as the cache oracle.

> **Is `dpr_run_now` not being reset coherent?**
> No, and it is not load-bearing either. The stated rationale […] is **already
> satisfied** by `dpr_run_max`, which is never reset. So the un-reset `dpr_run_now`
> buys nothing and costs coherence […] the first invalidation of each new window
> is classified as a **repeat** whenever it happens to match the previous window's
> last. That is +1 per canvas per window of spurious `dpr_rep`, and it lands in
> the **quiet** windows […] The bias runs toward "cache it". Reset it in the same
> block; `dpr_run_max` keeps what you wanted.

> **Dell or host?**
> Host […] The host **cannot** settle the interference half — no harness
> reproduces a descheduling event — so recognise the split explicitly: host
> settles *is the build expensive* (no), Dell settles *why did it cost 370× more*
> (scheduling). […] **not on `-O0`.** That arm produces 2505 xruns as a matter of
> course, the owner has rejected it […] Also: if H1 is right, "the stall did not
> reproduce on the mixed build" is **near-neutral evidence**, not evidence against
> — look for a sub-100 ms excursion, not 1.36 s.
>
> **And tab-switching is the wrong stimulus.** `RI_STALE_BAR` needs song mode
> *and* playback. […] The BAR invalidations are self-driving at the bar rate.

> **The single cheapest discriminating experiment**
> Re-read the `RIAPP stg` (and `wake_max`) line from the stall window you already
> captured, and compare it to `part_max`. One grep of a log you have. Zero new
> instrumentation, zero Dell time.

## 7. What the main session did with all of the above

1. Ran the free discriminator immediately on the captured log — `wake_max`
   **34 µs → 5822 µs** and xruns **0 → 2505** in the same window as the stall,
   which supports H1 (preemption) and makes the cache irrelevant.
2. Verified the bbox/zoom bug on the host and **fixed all four `ri_geo_bbox` call
   sites** to pass the canvas's own zoom (`e8b766b`), pinned by `t166_bbox_zoom`.
3. Reset `dpr_run_now` with its siblings, and relabelled `boxrep` a caller-spam
   counter rather than a cache oracle (`e8b766b`).
4. Did **not** build the display-list cache, on the measurement above.
5. Did **not** attempt the tab-switch reproduction on `-O0`, which the consultant
   identifies as the wrong stimulus in an arm the owner has already rejected.