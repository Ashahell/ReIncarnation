# The sub-split charges LEVI one stage-pair per stage it contains — verbatim, 2026-10-03

**Ingested:** 2026-10-03 into ReIncarnation `llm-wiki`
**Source:** four riqemu1 runs of the same song, differing in how many
instrumentation stage pairs LEVI contains.
**Provenance:** verbatim log lines; derived values show their components.
**Recorded in:** [the sub-split charges LEVI 6 µs per stage it contains](../articles/2026-10-03-the-sub-split-charges-levi-6us-per-stage-it-contains.md)

## 1. The question

The LEVI sub-split left a residual that no stage accounted for:

```
levi 213 - (arp 6 + seq 6 + voice 152 + mix 6 + tempo 6) = 37 us   (17 % of levi)
```

Two explanations fit that equally well:

- LEVI has real work the four sub-stages do not cover, or
- **the sub-split charges LEVI about the cost of each stage pair it contains**,
  and that shows up as unattributed because the pair's cost lands in the
  *enclosing* stage's interval, not in the stage's own reading.

A single table cannot separate these. The way to separate them is to **add one
more empty stage pair inside LEVI and see whether the residual moves.**

## 2. The second control

`lev-probe2`: an empty open/close pair placed **inside** SLEVI's interval, where
`lev-probe` sits outside it. Together they measure the same pair's cost from both
sides of the boundary.

```
RI_ESTAGE_T(e, RI_ENGINE_ST_LEVPROBE2, ts);
RI_ESTAGE_E(e, RI_ENGINE_ST_LEVPROBE2, ts);
RI_ESTAGE_E(e, RI_ENGINE_ST_SLEVI, tv);
```

## 3. The four runs

| run | inner pairs | playing buffers | levi | lev-voice | accounted | **residual** |
|---|---|---|---|---|---|---|
| 2 | 5 | 36200 | 213 | 152 | 176 | **37** |
| 3 | 5 | 17536 | 238 | 176 | 200 | **38** |
| 4 | **6** | 20107 | 263 | 189 | 219 | **44** |

Runs 2 and 3 have the same pair count and residuals of 37 and 38 — so the
residual is stable run to run. Run 4 adds one pair and the residual becomes 44.

**+6 µs for one extra stage pair.** The residual tracks the *number of stages*,
not the music: `lev-voice` varied 152 → 176 → 189 across the three runs (different
playback windows) while the residual barely moved.

Both controls read 6, inside and outside the boundary:

```
lev-probe   (outside SLEVI) : 6 us
lev-probe2  (inside SLEVI)  : 6 us
```

## 4. Run 4, verbatim

```
RIAPP dstg n=81358
RIAPP dstg zero   avg=6 us max=43 usRIAPP dstg delay  avg=6 us max=52 usRIAPP dstg comp   avg=6 us max=53 usRIAPP dstg master avg=6 us max=51 usRIAPP dstg meter  avg=6 us max=81 usRIAPP dstg limit  avg=6 us max=53 usRIAPP dstg 303a   avg=63 us max=128 usRIAPP dstg 303b   avg=60 us max=273 usRIAPP dstg 808    avg=20 us max=99 usRIAPP dstg 909    avg=7 us max=52 usRIAPP dstg levi   avg=263 us max=802 usRIAPP dstg lev-arp avg=6 us max=65 usRIAPP dstg lev-seq avg=6 us max=55 usRIAPP dstg lev-voice avg=189 us max=730 usRIAPP dstg lev-mix avg=6 us max=51 usRIAPP dstg lev-tempo avg=6 us max=73 usRIAPP dstg lev-probe avg=6 us max=46 usRIAPP dstg lev-probe2 avg=6 us max=51 usRIAPP dstg block  avg=538 us max=1061 us
RIAPP hb: buffers=3241622 xruns=0 render_max=3619 us wake_max=340 us wake_n=3241620 prio=21 arm_us=0 load=3/1000 overloads=0 snd=3/0/1/20 pend=3/0/1/20
RIAPP closed: buffers=3310225 xruns=0 render_max=3619 us render_total=118523 ms period=5333 us wake_max=340 us wake_total=53944 ms wake_n=3310223 stg_total_avg=223
```

## 5. The arithmetic

```
per-pair cost, from either control          : 6 us
5 inner stages -> 5 x 6                     = 30 us of split overhead
6 inner stages -> 6 x 6                     = 36 us of split overhead

residual with 5 pairs (runs 2 and 3)        : 37 and 38 us   -> genuine residual ~ 7-8 us
residual with 6 pairs (run 4)               : 44 us          -> 44 - 36 = 8 us
```

**So roughly 83 % of the residual was the split measuring itself, and the
genuinely unattributed part is about 8 µs — stable at 7, 8, 8 across the three
runs — which is ~3 % of LEVI.**

The intercept is the number that matters: it barely moves while the slope is
exactly one pair per stage. That is what makes this a measurement rather than a
guess.

## 6. What this does to LEVI's own figure

SLEVI's reported average **includes the cost of its own sub-split**:

```
run 4:  levi 263  -  6 pairs x 6  =  227 us of real LEVI work
        lev-voice 189  -  6       =  183 us of real voice rendering
        real LEVI share of block  :  227 / 538 = 42.2 %   (reported: 263/538 = 48.9 %)
```

So the reported LEVI share was **48.9 %** and the instrument-inflated-corrected
figure is **42.2 %**. Both are far above the 16.2 % the first (intro-window) run
produced, and above the Dell's ~30 %.

**Every stage that encloses sub-stages is inflated by one pair-cost per
sub-stage**, which is `block` (5 sections plus 6 always-stages), each section
leaf, and SLEVI. The section and block figures in every earlier table carry the
same tax and it was never subtracted.

## 7. Method, the transferable part

- **A control on both sides of a boundary measures the boundary.** One probe
  inside a region and one outside it turns "is this residual real?" into a
  subtraction.
- **Vary the instrumentation and watch the number you did not explain.** The
  residual's slope with respect to stage count is what identifies it; a single
  table cannot.
- **Enclosing stages are taxed by their children.** Any wrapper measurement that
  contains N instrumented children is high by about N x one-pair-cost, and that
  inflation is not visible from inside the wrapper.