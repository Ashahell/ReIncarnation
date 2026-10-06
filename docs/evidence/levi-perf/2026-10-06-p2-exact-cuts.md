# levi-perf P2: exact cuts (one section per cut)

Base for all before/after: P1 head `dd31d19` objects in `/tmp/ri/before*`
vs the cut. Bench binary identical. Medians of 7, min/max in the logs.

## P2.1 C3 (H5): empty-matrix fast path in levi_mod_apply [numbers corrected]

Correction (2026-10-06, second session): the first C3/C1/C2 before/after
tables compared binaries with mismatched bench-TU headers (the bench TU
was compiled against newer headers than its objects), which corrupted the
before runs. All tables below are re-measured matched pairs (objects and
bench TU from the same tree). The mechanism and proofs are unchanged.

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

Bench (us/voice-sample, median of 7, matched P1 vs P2.1 pairs):

| row | -O2 before | -O2 after |
|---|---|---|
| matrix empty (16op) | 0.5918 | 0.5495 (-7.1%) |
| matrix 8 routes | 0.8782 | 0.8603 (-2.0%) |
| worst | 0.8807 | 0.8745 (-0.7%) |
| worst-nomx | 0.5931 | 0.5418 (-8.6%) |
| pan centred (DUO) | 0.3215 | 0.2717 (-15.5%) |
| dfilt VOWEL (DUO) | 0.4991 | 0.4499 (-9.8%) |

Routed rows move only via the extra refresh scan in setters (within
noise). Unrouted rows gain 7-16%. (-O0 pairs below in the cumulative
table.)

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

Bench (us/voice-sample, median of 7, matched P2.1 vs P2.2 pairs; before =
C3-after):

| row | -O2 before | -O2 after |
|---|---|---|
| matrix empty (16op) | 0.5495 | 0.5021 (-8.6%) |
| matrix 8 routes | 0.8603 | 0.8371 (-2.7%) |
| worst | 0.8745 | 0.8518 (-2.6%) |
| worst-nomx | 0.5418 | 0.4956 (-8.5%) |
| pan centred (DUO) | 0.2717 | 0.2179 (-19.8%) |
| dfilt VOWEL (DUO) | 0.4499 | 0.3264 (-27.5%) |

## P2.3 C2 (H4): sine computed only on the branches that use it

P1 lead: `ri_levi_wave` computed `s = ri_sin(ph*TAU)` before the switch, so
saw/pulse/triangle/SYNC/PULSE/WARP pays for a sine it never uses.

What it does: the sine moved into the NOSC/SINE/HARM/FOLD/RING/CHEBY arms
that read it (same expression on the same input). 12 wave ids now skip it.

Exactness: same expression, same input, pure function. t172 wave cases
(all 16 classic + 7 families) hold bit-identical.

Proofs: t172 pins unchanged. Wrong variant (RING sine at 2x phase) FAILs
waves-family. Song hashes unchanged (demo/zombie/knife as in P0).

Bench (new non-sine rows; sine rows show branch-layout noise only;
matched P2.2 vs P2.3 pairs):

| row | -O2 before | -O2 after |
|---|---|---|
| waves saw x8 (ALLPAR) | 0.2979 | 0.2292 (-23.1%) |
| waves pulse x8 (ALLPAR) | 0.3103 | 0.2426 (-21.8%) |
| worst (sine) | 0.8518 | 0.8544 (+0.3%) |
| matrix empty (sine) | 0.5021 | 0.4986 (-0.7%) |
| pan centred (sine) | 0.2179 | 0.2161 (-0.8%) |

## P2.4 C5 (H6): dual-mono filter collapse

P1 numbers: stereo runs two full chains always; `dual` counter reads 1.000
on every centred row, 0.000 spread.

What it does: when `mixL == mixR` bitwise and the L/R filter states are
bitwise equal (a `dual_lock` flag carries the verdict across samples; the
inputs are checked every sample and the flag only skips the state compare),
one `voice_chain` runs and its output and end states copy to R. Spread
voices (`vspread != 0`, hence `panoff != 0` on every voice) can never match
and skip the check entirely on the old path.

Exactness: equal inputs + equal states through the same deterministic chain
give equal outputs and equal end states; the copy reproduces them. Vintage
runs per-channel afterwards, untouched. Correctness never depends on the
flag (inputs re-checked per sample).

Proofs: t172 pins unchanged. New corpus segment (centre-then-spread on one
held note, no re-trigger) catches the prescribed wrong variant (output
copied, states not): FAIL pans. Song hashes unchanged.

Bench, back-to-back medians of 7 (before = P2.3 matched objects):

| row | -O2 before | -O2 after | | -O0 before | -O0 after |
|---|---|---|---|---|---|
| matrix empty (16op) | 0.4976 | 0.4849 (-2.6%) | | 1.4017 | 1.3114 (-6.4%) |
| matrix 8 routes | 0.8307 | 0.8017 (-3.5%) | | 2.1817 | 2.0853 (-4.4%) |
| worst | 0.8310 | 0.8138 (-2.1%) | | 2.2105 | 2.1092 (-4.6%) |
| worst-nomx | 0.4905 | 0.4765 (-2.9%) | | 1.4041 | 1.3145 (-6.4%) |
| worst-silB | 0.3223 | 0.3075 (-4.6%) | | 0.9469 | 0.8564 (-9.6%) |
| pan centred (DUO) | 0.2178 | 0.2034 (-6.6%) | | 0.7369 | 0.6404 (-13.1%) |
| dfilt VOWEL (DUO) | 0.3212 | 0.2593 (-19.3%) | | 0.9743 | 0.7751 (-20.4%) |
| dfilt HP_MOD (DUO) | 0.2935 | 0.2411 (-17.8%) | | - | - |
| panmode spread 0/1/2 | +0.4/+0.3/+0.0% (tax gone) | | - | - |

## P2.5 C6 (H1 exact part): skip bank-B voice_pass when liveB[] is empty

P1 numbers: SILENCE-bank rows render 8/2 ops in bank B for nothing but an
8-iteration no-op loop.

What it does: `bank_load` (the single writer of `live[]`) maintains
`liveB_empty`; both render twins skip bank B's `voice_pass` (mix 0) when
set. `voice_pass` with no live op touches only its local `opout`, so the
skip changes no output and no state.

Exactness: provably unused computation. t172 pins unchanged. Wrong variant
(inverted condition) FAILs morph-held/morph-move/worst. Song hashes
unchanged.

Bench, matched pairs (general prize under the 2% P2 stop line; SILENCE
content only):

| row | -O2 before | -O2 after | | -O0 before | -O0 after |
|---|---|---|---|---|---|
| A8+B0 slot SILENCE | 0.3149 | 0.3061 (-2.8%) | | 0.8682 | 0.8547 (-1.6%) |
| A2+B0 slot SILENCE | 0.1702 | 0.1605 (-5.7%) | | 0.5287 | 0.5149 (-2.6%) |
| morph0 SILENCE-B | 0.2825 | 0.2743 (-2.9%) | | 0.8477 | 0.8235 (-2.8%) |
| worst-silB | 0.3068 | 0.2994 (-2.4%) | | 0.8570 | 0.8335 (-2.7%) |
| worst (live B) | 0.8097 | 0.8142 (+0.6%) | | 2.1110 | 2.1050 (-0.3%) |
| matrix empty (live B) | 0.4835 | 0.4807 (-0.6%) | | 1.3109 | 1.3076 (-0.3%) |

P2 STOP: the next candidates (C7-inline call overhead, C8 pan memo, C9 env
fast paths, C10 hoisting) were bounded below the 2% line by the P1 unit
costs (pan POWER sins ~= 1%, sustain ticks ~= 0, hoisted flops ~= 10 of
~1000s); the general C6 prize is itself under 2%. The general bank-B skip
(19%) is sound-changing and belongs to P3 O2.

Note: without the spread gate the first C5 cut taxed spread patches
+3.3-3.7% (non-overlapping min/max); the gate returns them to baseline.
Method note: an earlier before/after compared binaries with mismatched
bench-TU headers (stale-object trap, second occurrence); all C5 numbers
above are matched pairs. The C3/C1/C2 tables below were re-measured the
same way; the superseded mismatched figures are struck through, not
deleted.

## Cumulative P1 -> P2.3 (matched pairs, median of 7)

| row | -O2 P1 | -O2 P2.3 | | -O0 P1 | -O0 P2.3 |
|---|---|---|---|---|---|
| matrix empty (16op) | 0.5918 | 0.4986 (-15.7%) | | 1.6821 | 1.4057 (-16.4%) |
| matrix 8 routes | 0.8782 | 0.8413 (-4.2%) | | 2.2820 | 2.1892 (-4.1%) |
| worst | 0.8807 | 0.8544 (-3.0%) | | 2.3153 | 2.2204 (-4.1%) |
| worst-nomx | 0.5931 | 0.4912 (-17.2%) | | 1.6861 | 1.4061 (-16.6%) |
| pan centred (DUO) | 0.3215 | 0.2161 (-32.8%) | | 1.0114 | 0.7373 (-27.1%) |
| dfilt VOWEL (DUO) | 0.4991 | 0.3217 (-35.5%) | | 1.4031 | 0.9763 (-30.4%) |
| waves saw x8 | 0.4157 | 0.2292 (-44.9%) | | 1.3459 | 0.7554 (-43.9%) |
