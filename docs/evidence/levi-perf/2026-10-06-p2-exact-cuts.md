# levi-perf P2: exact cuts (one section per cut)

Base for all before/after: P1 head `dd31d19` objects in `/tmp/ri/before*`
vs the cut. Bench binary identical. Medians of 7, min/max in the logs.

## P2.1 C3 (H5): empty-matrix fast path in levi_mod_apply

P1 numbers: empty `eval2` alone = 12% of the typical patch; modapply 1.00 +
mrows 0.00 on every unrouted voice-sample.

What it does: `RILeviMatrix.mx_empty` (1 iff no slot on and no macro route
can emit) is refreshed by every program setter (`matrix_init/set/enable/
route`, `levi_set_mx_ui/mr_ui`, the ROUTE gate). When set, `levi_mod_apply`
skips the src build, `eval2` and the fold, and runs the full path's
unconditional effects verbatim: LFO rmul/lmod/smod/stmod reset, stale-route
clears, `*_on` flag clears, melmod zeroing, and all 12 e* clamps.

Exactness: under `mx_empty`, eval2 provably yields 0 rows (every emission
needs a slot on or a live macro route), so the fold is a no-op and the
clears/flags/clamps are the same operations on the same inputs. Output and
every state bit-identical by construction.

Proofs:
- t172 pins unchanged (17/19 cases pre-date the cut); extended corpus
  (UI-path/gate/mid-note removal segments) hashed on P1-head objects and on
  C3 objects: all 19 pairs IDENTICAL.
- Wrong variants caught: stale-empty via `set_mx_ui` (refresh dropped) FAILs
  matrix-families; melmod zero dropped FAILs matrix-heavy. Both PASS reverted.
- Song hashes unchanged: demo 238113a7962c112f, zombie 26eb9a9911173c20,
  knife fe26f4ac652d2561.

Bench (us/voice-sample, median of 7):

| row | -O2 before | -O2 after | | -O0 before | -O0 after |
|---|---|---|---|---|---|
| matrix empty (16op) | 0.6011 | 0.5496 (-8.6%) | | 1.7005 | 1.5608 (-8.2%) |
| matrix 1 route | 0.6424 | 0.6322 (-1.6%) | | - | - |
| matrix 8 routes | 0.8779 | 0.8578 (-2.3%) | | 2.2993 | 2.2974 (-0.1%) |
| worst | 0.8775 | 0.8696 (-0.9%) | | 2.3390 | 2.3155 (-1.0%) |
| worst-nomx | 0.5911 | 0.5433 (-8.1%) | | 1.7056 | 1.5744 (-7.7%) |
| worst-silB | 0.4279 | 0.3781 (-11.6%) | | - | - |
| pan centred (DUO) | 0.3213 | 0.2738 (-14.8%) | | 1.0285 | 0.8844 (-14.0%) |
| dfilt VOWEL (DUO) | 0.4987 | 0.4508 (-9.6%) | | 1.4212 | 1.2770 (-10.1%) |

Routed rows move only via the extra refresh scan in setters (not measured
per-sample; within noise). Unrouted rows gain 8-15%.

## P2.2 C1 (H2): memoised tpt_g per filter instance

P1 numbers: 8 of 29 sins per voice-sample sit in tpt_g (4 calls: dfilt L/R,
afilt L/R; vowel adds 3 per channel); memo-hit counter reads 1.00 on every
unmodulated row.

What it does: per-voice cache of last `(fc, sr)` bits + `g` for 10 slots
(df L/R, af L/R, 3 vowel formants x L/R). Hits return the stored bits;
misses call `tpt_g` identically. Zero-init can never hit (fc >= 20, sr > 0
wherever it runs), so no invalidation exists; the cache is a pure function
of its key and filter states are untouched.

Exactness: same bits in, same bits out. Output and state bit-identical by
construction; no FP order change (G is recomputed from the memoised g by
the unchanged expression).

Proofs:
- t172 pins unchanged (19/19 shared cases identical on P1-head vs C1
  objects; the new sr44100 case pins the rate change).
- New oracle case c19 (held note rendered at 48 kHz then 44.1 kHz without
  re-trigger) catches the prescribed wrong variant (key on fc only, not
  sr): FAIL sr44100. A first M-C1 attempt that did not compile re-ran the
  old binary (vacuous PASS) and was redone properly.
- Song hashes unchanged (demo/zombie/knife as in P0).
- Process lesson: the memo fields grow RILeviVoice, so every TU including
  levi.h must rebuild; a partial rebuild corrupted the song path (song
  played nothing) until `ri_build_host.sh all`. Bench/t172 pins were
  unaffected (render path consistent), but all reported numbers below are
  from fully consistent builds.

Bench (us/voice-sample, median of 7; before = C3-after):

| row | -O2 before | -O2 after | | -O0 before | -O0 after |
|---|---|---|---|---|---|
| matrix empty (16op) | 0.5496 | 0.5007 (-8.9%) | | 1.5608 | 1.4223 (-8.9%) |
| matrix 8 routes | 0.8578 | 0.8316 (-3.0%) | | 2.2974 | 2.1935 (-4.5%) |
| worst | 0.8696 | 0.8425 (-3.1%) | | 2.3155 | 2.2201 (-4.1%) |
| worst-nomx | 0.5433 | 0.4939 (-9.1%) | | 1.5744 | 1.4266 (-9.4%) |
| pan centred (DUO) | 0.2738 | 0.2160 (-21.1%) | | 0.8844 | 0.7346 (-16.9%) |
| dfilt VOWEL (DUO) | 0.4508 | 0.3246 (-28.0%) | | 1.2770 | 0.9811 (-23.2%) |
