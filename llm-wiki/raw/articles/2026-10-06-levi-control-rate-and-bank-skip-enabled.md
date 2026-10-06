# Levi control rate (N = 8) and the static-morph bank skip are always on: static sound unchanged, Zombie Nation bit-exact, and the Dell A/B (2026-10-06)

- Source: ReIncarnation session (advisor lane) and the opencode lane, 2026-10-06. Opencode commits `bde2c05` (Q1 + Q2) and `521ab7e` (Q3), reviewed by the advisor, pushed, and deployed to the Dell stick. Dispatch: `docs/superpowers/plans/2026-10-06-levi-options-enable-opencode-prompt.md`.
- Evidence: `docs/evidence/levi-perf/2026-10-06-q-enable.md` (verbatim tool output); the Dell A/B below is the advisor's own run.
- Collected: 2026-10-06
- Published: 2026-10-06
- Related: [2026-10-06-levi-perf-exact-cuts-review-and-options.md](2026-10-06-levi-perf-exact-cuts-review-and-options.md) (the exact cuts and the priced options this record turns on)

## What the owner decided

- **O1 control rate:** matrix fold, pitch and filter targets are evaluated
  every `RI_LEVI_CTRL_N` = 8 samples and interpolated in between.
- **O2 morph-bank skip:** gated on static morph.
- **Not taken:** O3 float kernels, O4 voice cap, and the O5 subnormal probe.
  Their code is removed, and the evidence stays recoverable at `a284347`.

## Three prototype defects fixed before shipping

1. **Note-start smear.**
   - `ctl_init` was never reset, so a re-triggered or stolen voice interpolated
     from the previous note's targets.
   - Now `filt_clear` clears `ctl_init`, `ctl_k`, `morph_hold` and
     `bank_skipped`. Every note start (`levi_trigger`, voice steal) and every
     deactivation runs it.
   - **Legato retune deliberately does not reset:** there is no envelope
     restart, so pitch keeps sliding. The owner may overrule this; the advisor
     agrees with it.
2. **O2 clicks on LFO-driven morph.**
   - The skip now requires the per-matrix `mx_has_algo` cache to be 0 and
     `emorph` to have sat exactly at the endpoint for at least 64 samples.
   - An LFO → ALGO route therefore never skips, and the t174 probe is
     bit-exact.
3. **Stepped gains.** `evlevel`, `eoplevel` and `edlevel` now interpolate like
   `dc`/`ac`/`vpitch`. `emorph` stays held, so the endpoint test sees an exact
   `0.0f`/`100.0f`.

Also fixed: `RI_LEVI_CTRL_N` has a negative-array power-of-two check, because
the `ctl_k % N` wrap is safe only for a power of two.

## The existing gated tests caught two bugs in the unification

- **t135 glide-0:** the evaluated pitch must land an instant glide (time 0) at
  once.
- **t147 mono twins:** the mono render applied the P9b filter velocity and
  pressure amounts twice.

Both were fixed. The mono render's double `levi_mod_apply` from the prototype
was removed in the same pass.

**Pattern:** unifying two code paths (per-sample and control-rate, mono and
stereo) is where the old tests earn their keep.

## Exactness after enabling

- **The narrowed rule:** cases with no modulation motion and no morph movement
  must keep their original pins. Moving cases are re-pinned deliberately, each
  with its deviation.
- **Static cases:** 13 static and 2 static-in-effect corpus cases are bit-exact
  to the `e125db8` reference (max = 0).
- **Re-pinned:** only 6 moving cases changed:

| Case | Max abs | RMS dBFS (rel) |
|---|---|---|
| morph-move | 0.564 | −22.09 (−11.67) |
| matrix-families | 0.2844 | −56.37 (−50.82) |
| matrix-heavy | 0.107 | −32.50 (−21.65) |
| lfo | 0.02012 | −50.11 (−38.90) |
| pitch | 0.002618 | −65.51 (−55.66) |
| worst | 0.08978 | −34.14 (−28.83) |

- **Block sizes:** 64- and 256-sample blocks still hash equal for every case.
- **Songs:**
  - demo `238113a7962c112f` and Zombie Nation `26eb9a9911173c20` are
    **bit-exact**;
  - The Knife `13dc42d73d85998b` differs by at most 0.001968 (−111.87 dBFS).
  - The advisor re-rendered all three hashes independently on `521ab7e`.
- **The P3 Zombie Nation deviation was the prototype's defect, not the
  control rate.** P3 measured 0.050 abs on Zombie Nation at N = 8. With the
  note-start reset and gain interpolation, the song is bit-exact.
- **New gated tests:**
  - `t173_levi_ctrlrate`: control points, stale-state law, VCA slope;
  - `t174_levi_bankskip`: skip after the hold, never with an ALGO route,
    knob-move resync, mono between stereo.
  - Mutation proofs: M-stale, M-gain, M-algo, M-hold and M-repin each FAIL as
    mutated.

## Speed

- **Host** (`-O2`, `levi_bench` median of 7, µs per voice-sample, `e125db8` → `521ab7e`):

| Row | Before | After | Change |
|---|---|---|---|
| voices | 0.2000 | 0.1556 | −22.2 % |
| matrix 8 routes | 0.7924 | 0.6672 | −15.8 % |
| worst | 0.8050 | 0.6724 | −16.5 % |
| morph 0 | 0.4008 | 0.2700 | −32.6 % |
| pan centred | 0.2025 | 0.1597 | −21.1 % |

- **Code size:** `levi.c` went 5466 → 5345 → 5310 lines.

## Dell, ABIv11: before/after on real songs (advisor run, 2026-10-07)

- **Builds:** A = `e125db8` (`RIAPP.prev`), B = `521ab7e` (`RIAPP`), both from
  the stick.
- **Runs:** each cell is a fresh launch with `SONG=` (which autoplays) and the
  log in `RAM:`. Zombie Nation ran A,B,B,A at 285 s per cell; The Knife ran
  A,B at 225 s.
- **Every cell played the full song:** `stg_playing` 48862–48867 for Zombie
  Nation and 38113 for The Knife, with xruns 0 and overloads 0.

| cell | build | lev-voice avg | levi avg | block avg | stg_total_avg | render_max |
|---|---|---|---|---|---|---|
| ZA1 | e125db8 | 180 us | 231 us | 537 us | 2276 us | 3572 us |
| ZB1 | 521ab7e | 151 us | 203 us | 508 us | 2157 us | 3271 us |
| ZB2 | 521ab7e | 151 us | 203 us | 509 us | 2161 us | 3292 us |
| ZA2 | e125db8 | 179 us | 231 us | 536 us | 2272 us | 3562 us |
| KA1 | e125db8 | 298 us | 370 us | 699 us | 2938 us | 4115 us |
| KB1 | 521ab7e | 231 us | 302 us | 631 us | 2662 us | 3715 us |

- **Zombie Nation:** `lev-voice` 180/179 → 151/151 µs, B < A in all four
  pairings (151/180 = 0.84). The whole buffer (`stg_total_avg`) drops
  2276/2272 → 2157/2161 µs, and `render_max` 3572/3562 → 3271/3292 µs. The
  output is bit-identical, so this is pure saved work.
- **The Knife:** `lev-voice` 298 → 231 µs (231/298 = 0.78), `stg_total_avg`
  2938 → 2662 µs, `render_max` 4115 → 3715 µs.
- **Peak heartbeat `load` is too noisy for A/B** (ZA 548/627 against ZB
  553/564). Use the averaged stage figures.
- **Cross-session drift:** this run's A (`e125db8`, Zombie Nation `lev-voice`
  180 µs) is higher than opencode's P4 B (`4421b9a`, 168 µs). The two builds
  have the same Levi cost; `e125db8` only adds one store in the unused mono
  path. **Absolute Dell figures move between sessions; compare only within one
  A,B,B,A run.**

## Deploy state

- `Vk4aros:ReIncarnation/RIAPP` = `521ab7e` (1063160 B, md5
  `0682b552c6ce3264f96f75ff4d4aabaf`, read back from the stick).
- `RIAPP.prev` = `e125db8`.

## Lane notes

- **`SONG=` starts playback on load** (the event log shows `SONG load` then
  `TR PLAY` at once). Do not inject a space key in protocol runs: it toggles
  playback off. That ruined the first A cell, which played only 1045 buffers.
- **A clean per-run log:** `SetEnv RIAPP_LOG RAM:` before launch, `--get RAM:RIAPP.LOG`
  after `--ui-close "RIAPP live panel"`, then `UnSetEnv`.
- **Do not `pkill -f` a pattern that matches your own shell's command line.**
  It killed the caller (exit 144).
