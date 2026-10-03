# The LEVI sub-split, re-measured: the first sample was the song's intro (2026-10-03, correction)

**Ingested:** 2026-10-03 into ReIncarnation `llm-wiki`
**Source:** second riqemu1 run after adding two stages — `lev-tempo` around
`levi_set_tempo` and `lev-probe`, a deliberate empty-pair **control**.
**Provenance:** verbatim log lines; derived values show their components.
**Recorded in:** [correcting the first LEVI sub-split record](../articles/2026-10-03-the-levi-sub-split-measured-arp-seq-voice-and-mix-are-all-at-the-floor.md)
and [the 2026-09-02 five-sections record](../articles/2026-10-02-five-sections-one-culprit-levi-is-32-percent.md)

## 1. Why this run exists

The first sub-split measurement reported `lev-voice avg=8 us` and concluded the
LEVI internals sat at the floor. `levi_set_tempo` was the only call inside SLEVI
that no sub-stage covered, so it was instrumented — along with a **control**: an
open/close pair with nothing between them, whose reading *is* the cost of one
stage pair. Placed deliberately OUTSIDE SLEVI's interval so the instrument does
not inflate the region it measures.

## 2. The control, verbatim

```
RIAPP dstg levi     avg=213 us max=778 us
RIAPP dstg lev-arp  avg=6 us max=111 us
RIAPP dstg lev-seq  avg=6 us max=111 us
RIAPP dstg lev-voice avg=152 us max=715 us
RIAPP dstg lev-mix  avg=6 us max=111 us
RIAPP dstg lev-tempo avg=6 us max=91 us
RIAPP dstg lev-probe avg=6 us max=106 us
RIAPP dstg block    avg=499 us max=1065 us
RIAPP hb: buffers=919127 xruns=0 render_max=3628 us wake_max=290 us wake_n=919125 prio=21 arm_us=0 load=3/1000 overloads=0 snd=0/2/0/8 pend=0/2/0/8
RIAPP closed: buffers=1051190 xruns=0 render_max=3628 us render_total=104574 ms period=5333 us wake_max=951 us wake_total=17098 ms wake_n=1051188 stg_total_avg=2162 us stg_dsp_avg=2083 us stg_evt_avg=11
```

Full settled table (`RIAPP dstg n=146481`):

| stage | avg µs | % of block |
|---|---|---|
| zero / delay / comp / master / meter / limit | 6 each | 1.2 % each |
| 303a | 64 | 12.8 % |
| 303b | 68 | 13.6 % |
| 808 | 23 | 4.6 % |
| 909 | 7 | 1.4 % |
| **levi** | **213** | **42.7 %** |
| lev-arp | 6 | 1.2 % |
| lev-seq | 6 | 1.2 % |
| **lev-voice** | **152** | **30.5 %** |
| lev-mix | 6 | 1.2 % |
| lev-tempo | 6 | 1.2 % |
| lev-probe (control) | 6 | 1.2 % |
| block | 499 | 100 % |

```
lev-probe (one empty pair)      : 6 us
lev-tempo                       : 6 us  -> levi_set_tempo real cost = 6 - 6 = 0 us
levi                            : 213 us
arp + seq + voice + mix + tempo : 176 us
RESIDUAL inside levi            : 37 us = 17% of levi
```

`levi_set_tempo` is closed as a candidate: it reads the floor, and the control
proves the floor is the instrument, not the code. Inspection had already said it
was three float compares and a store; the number agrees.

## 3. The correction: run 1 sampled the song's first 32 seconds

`lev-voice` across **every** dump of both runs:

```
=== RIAPP.LOG  (first run) ===
            n  lev-voice     levi    block
         6093          8       57      325
        12228          8       57      342
        18438          8       57      348
        24042          8       57      351

=== RIAPP2.LOG (second run) ===
            n  lev-voice     levi    block
        35692         65      126      423
        60827        159      220      505
        87197        183      244      530
       126016        138      199      486
       142981        149      210      496
       146481        152      213      499
```

Run 1's `lev-voice` was **pinned at 8 µs for the whole run** — not a transition,
a steady state. And how far each run actually played:

```
run 1:  RIAPP stg: playing=5942   stopped=19380     (5942 buffers x 256 / 48000 = 31.7 s)
run 2:  RIAPP stg: playing=36200  stopped=882927    (36200 x 256 / 48000 = 193 s)
```

**So run 1 measured the opening ~32 s, where the Levi part of Zombie Nation does
not sound, and run 2 measured 193 s, which includes it.** Both tables are
internally valid playing-state measurements; they simply measured different parts
of the music.

The first record's headline — "LEVI is 16.2 %, third, not the outlier" — is an
artefact of that window. The better-powered figure is **42.7 %**.

## 4. What this does to the 2026-09-02 headline

| | 2026-09-02 (Dell) | first riqemu1 run | second riqemu1 run |
|---|---|---|---|
| LEVI share | ~30 % | 16.2 % | **42.7 %** |
| lev-voice | not split | 8 µs | **152 µs** |

The first record claimed, correctly for its data, that the 16.2 % did not refute
the Dell's ~30 %. **That framing was itself an artefact of the 32-second window.**
With a properly powered sample LEVI is at 42.7 %, *above* the Dell figure rather
than below it, so the two are now consistent in direction and the "does not
refute" caveat is retired.

The 17 % residual inside LEVI is the genuine remaining gap, and it is much
smaller than the 54 % the first run reported.

## 5. Method, the part worth keeping

**A cumulative average is only as meaningful as the window it accumulated.** The
`dstg` table is `sum/n` over every block since start, and `n` grew to 24042 in
run 1 and 146481 in run 2 — from the same song, minutes apart, differing only in
how long playback was left running. Nothing in the table says which part of the
arrangement it covers.

**A control stage is worth more than another subject stage.** `lev-probe` costs
one line and settles two things at once: that 6 µs is the floor (so six stages
reading 6 are unmeasurable, not cheap), and that `lev-tempo`'s 6 µs is 0 µs of
work. Without it, `lev-tempo` reading 6 would have been ambiguous.

**Place a control outside the region it measures.** `lev-probe` sits outside
SLEVI's interval specifically so SLEVI's before/after numbers stay comparable —
otherwise the instrument inflates the thing it is measuring and the old and new
SLEVI figures cannot be compared at all.

## 6. Open

- **The remaining 37 µs inside LEVI (17 %)** is still unattributed. With the floor
  established at 6 µs, this is not measurement error at the sub-stage level.
- **Run-length discipline for stage tables.** Nothing enforces playing long enough
  to cover the arrangement, and this cost a wrong conclusion once.
- **Sub-6 µs resolution** remains: no `n`-based guard on these rows, so a
  sub-stage reading 6 is indistinguishable from one that never ran.