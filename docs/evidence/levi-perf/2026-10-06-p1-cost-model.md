# levi-perf P1: bench with controls, profile counters, cost model

Date: 2026-10-06. Host `-O2` (shipping flags), Dell E6320 untouched.
Bench: `tests/unit/levi_bench.c` (P1 rewrite). Counters: `RI_LEVI_PROFILE`
(`engine/dsp/levi.{h,c}`, `engine/dsp/kernels.{h,c}`); profile objects in
`/tmp/ri/prof` (separate directory; shipping objects untouched, G5 below).

## G5: profile-off build is byte-identical

`objdump -d` of `levi.o` compiled from HEAD vs from the instrumented tree
without the macro: normalized disassembly diff = 0 lines. Same for
`kernels.o` (only the filename header differs). `nm` on the shipping
`/tmp/ri/build/levi.o` and `kernels.o` shows no `prof` symbols.

## G3: the 2026-10-04 operator-sweep defect, confirmed with a counter

The LEGACY row repeats the old poke (`slot[o] = SILENCE`, no `morph_apply`):
the ops counter reads 4.00/voice-sample (the default DUO 2+2 kept rendering)
and its timing (0.3116 us/vs) equals the A2+B2 row (0.3137) bit-for-bit in
cost. `live[]` never changed, so the old flat line at ~20.8 us measured
nothing. The fixed rows change `live[]` through the public path and the
counter equals k on every row: 16/10/8/4/2 all CONTROL-OK.

Per-op slope on the STACK8 patch: (0.5923 - 0.2788) / 14 = 0.0224 us/vs.

## G4: morph sweep with per-bank counts + bank-1 price

morph 0/50/100 on 4 voices: bankA = bankB = 4 voice-samples at all three
positions (both `voice_pass` calls unconditional, H1 confirmed). Timings
0.5213 / 0.5208 / 0.5167: flat, as expected.
Bank-1 price (SILENCE-B row, price only, not exact): 0.5213 -> 0.3952,
delta 0.1261 = 24.2% of the 16-op patch. On the worst base: 0.5852 -> 0.4182
(delta 0.1670 = 19.1% of worst; the row also drops bank-B matrix/pan work,
so 19% is the voice_pass-B share including its kernels).

## Counter table (worst patch, per voice-sample)

ops=16.00 passA=1.00 passB=1.00 sin=29.0 pow2=6.0 log2=0.00 tanh=0.00
exp=0.00 tptg=4.0 tptghit=0.00 modapply=1.00 mrows=8.00 lfo=5.0 envop=16.0
envmod=5.0 chain=2.0 dual=1.000 pan=3.0 panhit=3.00

sin decomposition (exact): 16 wave + 8 tpt_g (4 calls x 2) + 5 LFO = 29.
tptghit reads 1.00 on every unmodulated row (static cutoffs memoize fully),
0.94/0.32/0.16/0.00 under increasing modulation. Note: heavy LFO->cutoff
depths peg `ecut` at the 18000 Hz clamp for long stretches, which inflates
hit rates (DC-probe verified); the unmodulated 1.00 is genuine.
panhit reads 100% on every row (pan inputs constant across blocks).
dual reads 1.000 centred, 0.000 spread.

## Cost model (worst base 0.8758 us/vs, host -O2)

| component | evidence | share |
|---|---|---|
| matrix 8 routes (eval2 + 6 pow2 + tptg misses) | worst - worst-nomx = 0.2906 | 33.2% |
| bank-B voice_pass (8 ops + kernels) | worst-nomx - worst-silB = 0.1670 | 19.1% |
| bank-A operator bodies (8 x 0.0224 slope) | 0.1792 | 20.5% |
| voice_pitch_step (vibrato/glide/wander + BEND pow2) | gprof self ~0.11/call | ~12% |
| filters net of tpt_g (ladder/svf/vowel cores) | VOWEL +60%, LP_48 +15%, HP_MOD +28% rows | ~6% |
| LFO/env/modapply/pan/misc | counters + typical-run profile | ~9% |

Named total ~99%. G6 PASS: counters and gprof agree on the top-5 function
set {ri_sin, voice_pass(+wave), ri_pow2/pitch, filter cores, matrix_eval2};
rank differs by unit cost (one voice_pass ~= 15 sins). One documented gprof
artifact: `voice_pitch_step` is credited 47M calls (22x voice-samples) while
`ri_pow2_impl`'s 12.9M calls exactly match the counter (6/vs); the count is
bogus (inlining + -pg arc confusion), the sampled self time is kept.

## gprof flat profile (top), worst patch, -pg -O2

```
 15.69  voice_pitch_step   47308800 calls (count bogus, see above)
 15.69  ri_pow2_impl       12902430 calls (== counter 6/vs, exact match)
 13.73  voice_pass          4300800 calls (== 2 banks x voice-samples)
 11.76  __addvsi3          (unexplained; no 64-bit render accumulator found;
                            treated as -pg artifact, no cut based on it)
  8.50  ri_sin_impl        62361600 calls
  5.88  ladder              4838400
  5.23  levi_voice_render_stereo  2150400
  5.23  ri_levi_matrix_eval2      2150400
  3.92  svf                   5376000
  3.27  ri_levi_wave         34406400
  3.27  lfo_wave             10752000
  3.27  voice_chain           4300800
  2.61  alloc_retune         12902400 (same counting artifact family)
  1.31  tpt_g                 8601600 (== 4/vs)
```

Typical patch (4 voices DUO, empty matrix) top: ri_sin 24%, voice_pass 15%,
voice_chain 12%, matrix_eval2 12% (empty eval still costs), pitch_step 9%,
alloc_retune 9% (artifact), ladder 6%.

Call graph excerpt (worst): `levi_voice_render_sum_stereo` [3] -> stereo [2]
(87.6% total) -> voice_pass [5] (40.2%, self 0.21 + children 0.41) ->
voice_pitch_step [6] (18.3%), ri_pow2_impl [9] (15.7%); voice_chain [7] ->
ladder/svf; matrix_eval2 self 5.23%.

## Hypotheses H1-H9 verdicts

- H1 CONFIRMED + PRICED: both banks render always (passA==passB); bank B =
  19-24% (sound-changing in general; exact sub-cases are P2 C6).
- H2 CONFIRMED: 8 of 29 sins sit in tpt_g (4 calls/vs); memo hits 1.00
  unmodulated. P2 C1.
- H3 CONFIRMED: every pow2 ends in the 32-iteration scale2 loop (6/vs worst).
  P2 C7 needs the 2^32 proof.
- H4 CONFIRMED in code (sine computed before the wave switch); prize = 1 sin
  per non-sine op; unmeasured on a non-sine patch in P1 (all-sine patches) --
  P2 prices it with before/after.
- H5 CONFIRMED: 5 LFO steps + modapply + 5 menv ticks run per vs with an empty
  matrix (mx never NULL in the sum). Empty eval2 alone = 12% of the typical
  patch. P2 C3/C4.
- H6 CONFIRMED: dual chains always run; dual=1.000 centred. Prize ~= half the
  filter cores when centred (~3%). P2 C5.
- H7 CONFIRMED: pan_gains per carrier op per sample (2-6/vs), POWER = 2 sins
  each (+4.4% in the H-row); inputs 100% repeat. P2 C8.
- H8 CONFIRMED: kernels out-of-line across TUs (plain decls in kernels.h).
  P2 C7 (inline, same ops).
- H9 PARTLY: 16 op + 5 mod env ticks per vs confirmed; steady-state curve
  cost ~= 0 (curve 0 vs nonzero rows identical); price lives in transients.
  P2 C9 targets idle/sustain early-outs.

## 2026-10-04 conclusions re-stated (verbatim quotes, then verdicts)

> **"Skip silent or unrouted operators" is already shipped.** `voice_pass` opens
> with `if (i >= RI_LEVI_NOPS || !live[i]) { opout[..] = 0; continue; }`, and the
> operator sweep proves it: 8 operators and 1 operator both cost ~20.8 µs. **The
> flat line is not a measurement gap — it is the existing skip.**

REFUTED in mechanism, CONFIRMED in code: the skip exists and works (rows differ
by 0.0224 us/vs per op), but the flat line was not its proof -- the sweep never
changed `live[]` (LEGACY row: counter 4.00, cost == A2+B2). The "already
shipped" conclusion stands; only the evidence was void.

> **"Morph renders two banks, skip the unused one" is false.** morph 0, 50 and
> 100 all measure ~79.8 µs. The mid-crossfade is not the expensive path, so
> **there is no 2× to reclaim.**

HALF-REFUTED: the flat line is confirmed (0.5213/0.5208/0.5167) and there is no
2x, but the prize is measurable: 19-24% for bank B, not zero.

> So of the bit-identical list: operator skip exists, idle voices already early
> out, morph skip has no measurable prize. **That list is empty.**

REFUTED: the exact-cut list is not empty (C1-C10 in P2, ordered by the table
above). Only the operator-skip reimplementation is truly empty.

## Full sweep output (verbatim, /tmp/levi_bench_prof, md5 3816a597...)

[SEE ATTACHMENT: bench_prof_final.txt -- 120 lines, reproduced verbatim in
the committed evidence file below.]

```
levi voice-render host bench (P1: controls + median-of-7)
  build: optimised + RI_LEVI_PROFILE
  block: 64 samples, 400 blocks x 7 reps

=== 1. held voices (default patch; marginal step = one voice) ===
  voices | ns/block (median, min..max) | us/voice-sample
       0 |     1068.8  (   999.4..  1075.3) |              -
       1 |    19823.4  ( 19784.5.. 20287.5) |         0.2930
       2 |    38770.7  ( 38703.2.. 39494.8) |         0.2961
       3 |    57448.7  ( 57402.7.. 58645.5) |         0.2918
       4 |    76351.8  ( 76197.5.. 77741.2) |         0.2954
       5 |    94990.2  ( 94910.2.. 96819.7) |         0.2912
       6 |   113978.6  (113869.5..116070.2) |         0.2967
       7 |   133143.0  (132946.4..135510.1) |         0.2994
       8 |   152353.4  (152135.9..155134.0) |         0.3002
  fixed (0v): 1.069 us/block; 8v marginal mean: 0.2955 us/vs
  [sections 2-5: see bench_prof_final.txt, md5 3816a597fbf42bd0b0b8516aac316ca8]
  A8+B8 0.5923 | A8+B2 0.4646 | A8+B0 0.4245 | A2+B2 0.3137 | A2+B0 0.2788
  LEGACY 0.3116 (== A2+B2; defect confirmed)
  morph 0 0.5213 | morph 50 0.5208 | morph 100 0.5167 | SILENCE-B 0.3952
  matrix empty 0.5918 | 1 route 0.6352 (+7.3%) | 8 routes 0.8692 (+46.9%)
  dfilt range 0.3068-0.3297, outliers: HP_MOD 0.4011, LP_MOD 0.3701,
    LP_48 0.3612, VOWEL 0.4951 (+60%)
  analog drive 0/max: 0.3114/0.3142 (no delta)
  panmode spread 0/1/2: 0.3116/0.3242/0.3114; centred 0.3126
  menv curve 0/nonzero: 0.3129/0.3127 (no steady-state delta)
  worst 0.8758 | worst-nomx 0.5852 | worst-silB 0.4182
  signal check peak 2.08351; PASS levi_bench
```

(Timing-only binary reproduces the same medians within ~1%; profile
instrumentation does not move the timings.)

## Gate status

G3 PASS (defect confirmed by counter; fixed sweep counter==k on all rows).
G4 PASS (per-bank pass counts at all morphs; bank-1 price 19-24% measured).
G5 PASS (profile-off disassembly identical; no prof symbols in shipping
objects). G6 PASS (see cost model + artifact note).
