# Correction (2026-10-26 07:20): one press made two lines — the rate was 9.73/s

The first write-up (commit `4426d14`) counted the log's `TR` **lines** and called
them toggles: *1 493 800 lines at 19.45/s*. That is wrong by a factor of two, and
it puts the wrong cause on the `STOP` half. The evidence for the correction is in
this directory (`ev-replay*.log`) and in the full log's own intervals.

## What the log actually is

- 1 493 800 `TR` lines, strictly alternating `PLAY`, `STOP`, `PLAY`, `STOP` …
  (0 non-alternating adjacent pairs in 1 493 801 gaps).
- So there are **746 900 pairs**, and a pair is **one key-down**.
- Intervals over all 1 493 801 gaps, in render buffers (187.5/s):

| gap | n | mean | median | p10 | p90 |
|---|---|---|---|---|---|
| all | 1 493 801 | 9.68 | 11 | 7 | 12 |
| PLAY → STOP | 746 901 | 9.00 | **8** | 7 | 11 |
| STOP → PLAY | 746 900 | 10.35 | 11 | 8 | 12 |
| PLAY → PLAY | 746 900 | 19.35 | **19** | 15 | 23 |

A repeat stream has **one** period. Two different, tight distributions (43 ms
inside a pair, 59 ms between pairs) is the signature of *press, then something
else 43 ms later* — not of a 19 Hz toggle.

- **Direct proof, not statistical:** a single *paired* press (`0x40` down,
  `0x40,up`) on a transport stopped at the song's end produced exactly
  `TR PLAY` and then `TR STOP` five buffers later, and nothing else
  (`ev-replay.log`, ev 73/74). One press, one pair.

## The corrected numbers

| | |
|---|---|
| `TR` lines | 1 493 800 = **746 900 PLAY/STOP pairs** |
| presses | **746 900** (one per pair) |
| press rate | **9.73/s** — a repeat period of ~101 ms (19 buffers, ~5 ticks at 50 Hz) |
| log cost | 26.5 B per line = **53 B per press** |
| span | 14 396 779 buffers = 76 782.8 s = 21.33 h (unchanged) |

## Why the STOP half is not the next repeat

The transport was stopped at the song's **end** when the key-down arrived, and
nothing in the app could move the session cursor off the end (P-TR2, below). So
each press did:

1. `RI_KT_STOPPLAY` reads the PLAY LED (off) and presses PLAY → `TR PLAY`;
2. `song_poll_end` sees the published cursor still past `song_bars + 1` and
   stops the transport → `TR STOP`, a median 8 buffers (43 ms) later.

That is why the pair is tight and the between-pair gap is the repeat period.

## Two defects, not one

- **P-TR1** — a key auto-repeat acts as a press (`gui/panelui.c`, fixed in
  `54d7c95`). This is what made the stream endless.
- **P-TR2** — after the song ends the transport cannot be rewound, so Play is
  immediately stopped again (`app/riapp.c` `sync_transport()`: it early-returns
  on an unchanged *state*, so the documented second-stop rewind — the clicks law
  in `ri_tr_stop()` — never reaches the engine). This is what made each press a
  **pair** instead of a state that stayed put.

The P-TR1 fix stands on its own: with the press-edge guard the first press acts
and every repeat is ignored, so the pair storm cannot happen even while P-TR2 is
open. But the full explanation of 39.5 MB needs both, and "AROS repeats a held
key at ~19/s" was the *line* rate, not the key rate.
