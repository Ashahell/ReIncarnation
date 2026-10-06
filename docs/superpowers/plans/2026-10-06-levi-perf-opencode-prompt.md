# Prompt for OpenCode: Leviasynth voice-render performance (measure, exact cuts, then options for the owner)

> Written 2026-10-06 by the Claude advisor session. The owner's request is:
> *"Leviasynth performance. Measure it on this PC by voice count, operator count
> and Morph on/off. Make the cuts that leave the sound unchanged first, then
> bring any sound-changing options to the owner with their measured savings."*
>
> The work has **five phases (P0–P4)**. Each phase ends at a commit and a short
> report. If you run short of context, stop at a phase boundary and write the
> handoff block (§9).
>
> **How you know you are done:** every gate in §8 reads PASS, and each one has
> its evidence in the place it names. A phase is not finished because the code
> looks right. It is finished when its gates are green.

---

## 0. Context

ReIncarnation is a clean-room instrument rack for AROS x86-64. The live app
`RIAPP` plays on a Dell E6320 (ABIv11, Sandy Bridge, **no FMA**, real HDA
audio). The engine is built at `-O2` and the app and GUI at `-O0` (the "mixed
build").

Leviasynth ("Levi") is an 8-voice, 8-operator FM/PD synth with two morph banks,
5 LFOs and 5 mod envelopes per voice, a mod matrix, a digital filter and an
analog filter per channel, and a per-device FX chain.

What is already established (all in the wiki, see §1):

| Fact | Value | Source |
|---|---|---|
| LEVI's share of the engine block on the Dell (`-O0`, The Knife) | 64 % | 2026-10-03 Dell article |
| Share of LEVI inside `levi_voice_render_sum_stereo` | 90 % (1,036 of 1,139 µs, after the timer-tax correction) | same |
| Host cost at `-O2`, per voice-sample | 0.307 µs; 8 voices × 64 samples = 159 µs/block | 2026-10-04 host bench |
| Host cost at `-O0`, per voice-sample | 1.022 µs | same |
| Dell cost at `-O2`, from the paired-counter fit | 1.97 µs per voice-sample, 13 µs fixed per block, r = 0.9983 | same |
| Dell ÷ host at `-O2` | 6.42× | same |
| FX chain (delay + comp) on the block | about 1 % | 2026-10-02 render-stage article |
| Dell during Zombie Nation at `-O2` | xruns 0; heartbeat load up to about 730/1000 | Dell telemetry, 2026-10-01 to 10-05 |

So **the cost is per sounding voice, inside the voice render**. That is the
target. Every new Levi feature eats into the Dell's remaining margin.

### 0.1 Two earlier bench conclusions are unproven. Treat them as open.

The 2026-10-04 record says the list of exact cuts is "empty". It rests on two
flat lines in `tests/unit/levi_bench.c`. **Neither line measured what it
claims.**

1. **The operator sweep never changed the operators.**
   - It writes `S.v[0].slot[o] = RI_LEVI_SLOT_SILENCE`.
   - Slots only take effect through `morph_apply()` → `bank_load()`, which
     rebuilds `live[]`/`liveB[]` (`engine/dsp/levi.c:815-884`). The bench never
     calls either.
   - So `live[]` was unchanged and all 8 operators kept rendering. The flat line
     at ~20.8 µs means nothing changed. It does not mean the skip works.
   - **Verify this first** (G3). If it holds, the operator count's real price is
     unknown.
2. **The morph sweep cannot detect the bank-skip prize.**
   - `levi_voice_render_stereo` always calls `voice_pass` for bank 0 **and**
     bank 1 (`levi.c`, the two `voice_pass` calls just before the
     `emorph <= 0.0f` test), whatever the morph value.
   - So morph 0, 50 and 100 do the same work, and a flat line is the expected
     result. The prize, the cost of the second `voice_pass`, was never
     measured.
   - The bench's own final printout says "a bank BOTH banks render but only ONE
     uses is already being computed and thrown away". That line contradicts the
     article's reading.

A third note: the 2026-10-04 "LFO path never runs" counter (`vc_lfo_iters`)
only counts the **shared/global** LFOs (`s->glfo`). In
`levi_voice_render_sum_stereo` every voice is rendered with `mx = &s->mx`, which
is never NULL. So `lfo_on` is always true, and **each voice steps its own 5 LFOs
and runs `levi_mod_apply` (matrix eval plus memsets) on every sample**, even
with an empty matrix. That counter never saw those per-voice LFOs.

### 0.2 Hypotheses from reading the code (measure them; do not assume)

These are leads, not findings. Each must be confirmed by a counter **and** a
timing before you act on it.

| ID | Lead | Where |
|---|---|---|
| H1 | Bank B's `voice_pass` runs every sample even when `emorph` is exactly 0 or 100 and its result is discarded. | `levi_voice_render_stereo` |
| H2 | `tpt_g()` calls `ri_sin` twice (double-precision order-13 polynomial). It runs per sample, per channel, per filter: dfilt L/R plus afilt L/R is at least 4 calls (8 sines) per voice-sample, and the vowel model adds 3 per channel. The cutoff rarely changes between samples. | `tpt_g`, `dfilt_step`, `afilt_step`, `vowel` |
| H3 | `ri_pow2()` ends in `ri_scale2()`, a **fixed 32-iteration loop** even for a scale of 2^0. It is called from cutoff modulation, pitch, envelope times and LFO rates. | `engine/dsp/kernels.c:28-50, 211-256` |
| H4 | `ri_levi_wave()` computes `s = ri_sin(ph * TAU)` before it switches on the wave, so saw, pulse, triangle and SYNC waves pay for a sine they never use. | `levi.c` `ri_levi_wave` |
| H5 | Per-voice LFOs (5) and `levi_mod_apply` run per sample even when the matrix has no active routes and no pre-wired amount listens (see §0.1). | `levi_voice_render_stereo`, `levi_mod_apply` |
| H6 | The stereo path runs **two** full filter chains (`df`/`af` and `dfR`/`afR`). When every pan is centred, `mixL == mixR` bit for bit, so the two chains carry identical state and do the same work twice. | `voice_chain` ×2 |
| H7 | `pan_gains()` runs per operator, per sample. In POWER mode that is 2 sines each, and its inputs are usually constant across a block. | `voice_pass` |
| H8 | `kernels.c` functions are out-of-line across translation units (no LTO), so every `ri_sin`/`ri_pow2` is a call. | build |
| H9 | 5 mod envelopes are ticked per voice per sample (`env_tick_b`), and `seg_shape` with a non-zero curve costs a `ri_log2` plus 2 `ri_pow2`. | `env_tick_b`, `seg_shape` |

---

## 1. Read first (in this order)

1. `llm-wiki/index.md`, then these raw articles in
   `llm-wiki/raw/articles/` (and the evidence files they link):
   - `2026-10-04-host-bench-and-the-overload-guard.md` (and
     `../evidence/2026-10-04-host-bench-and-the-overload-guard.md`, verbatim
     bench output);
   - `2026-10-03-on-the-dell-levi-is-64-percent-of-the-block-and-91-percent-is-one-call.md`;
   - `2026-10-03-the-sub-split-charges-levi-6us-per-stage-it-contains.md`
     (the timer tax: about 4 µs per stage pair on the Dell, 6 µs on riqemu1);
   - `2026-10-04-the-lfo-path-never-runs.md`;
   - `2026-10-02-five-sections-one-culprit-levi-is-32-percent.md`;
   - `2026-10-02-render-stage-breakdown-voices-are-94-percent-not-the-fx-chain.md`
     (read "the instrumentation was measuring itself");
   - `2026-10-03-the-governor-arm-is-load-bearing-at-o0-and-irrelevant-at-o2.md`;
   - `2026-10-05-review-latency-instrumentation-and-advice-log.md`, the
     "Levi performance" bullet: the advisor's ordering, which this prompt
     implements;
   - `2026-10-01-dell-deploy-abiv11-usb-stick-layout.md` and
     `2026-10-05-stick-log-lost-lines-fat-readwrite-refused-while-read.md`
     (Dell lane facts).
2. `docs/superpowers/specs/2026-09-20-reincarnation-spec.md`:
   - §4 realtime (LOCKED);
   - §5 one renderer;
   - §15 determinism;
   - Appendix A, the pending-measurement ledger.
3. `docs/superpowers/plans/2026-09-26-g9-opencode-prompt.md` §3 (hard rules)
   and §4 (lanes). They apply here unchanged unless §3 below says otherwise.
4. `docs/superpowers/plans/2026-09-28-gui-round3-opencode-prompt.md` §4.6
   (the ABIv1 gate, then the ABIv11 build) and §4.7 (Dell deploy).
5. Code:
   - `engine/dsp/levi.c`: `voice_pass`, `levi_mod_apply`, `voice_pitch_step`,
     `voice_chain`, `levi_voice_render_stereo`, `levi_voice_render_sum_stereo`,
     `env_tick_b`, `seg_shape`, `ri_levi_wave`, `tpt_g`, `svf`, `ladder`,
     `dfilt_step`, `afilt_step`, `morph_apply`, `bank_load`;
   - `engine/dsp/levi.h`, `engine/dsp/levi_matrix.c` (`ri_levi_matrix_eval2`),
     `engine/dsp/levi_fx.c`;
   - `engine/dsp/kernels.c` and `engine/dsp/kernels.h`;
   - `tests/unit/levi_bench.c`, `tests/unit/t103_levi_dsp.c`,
     `t129`–`t137` (Levi), `tests/property/t2_kernels.c`,
     `tests/property/t3_kernels_total.c`;
   - `tools/songplay.c` (headless song render through the app core);
   - `scripts/ri_build_host.sh` (host flags: note `-ffp-contract=off
     -fno-unsafe-math-optimizations`), `scripts/ri_build_v11.sh`,
     `scripts/ri_audit.sh`.

---

## 2. What "sound unchanged" means here: bit-exact, nothing weaker

A cut is **exact** only if, for every input in the oracle corpus (§4 P0), the
float output of `levi_voice_render_sum_stereo` is **bit-identical**, compared as
`uint32_t` bit patterns, to the pre-change output. "Inaudible", "−120 dB" and
"max error 1e-7" are **not** exact. Those cuts belong in P3.

Rules that follow from this:

1. **No change to floating-point evaluation order.** No reassociation, no
   factoring (`a*b + a*c` → `a*(b+c)`), no replacing a division with a
   reciprocal multiply, no `-ffast-math`, `-Ofast`, `-ffinite-math-only`,
   `-freciprocal-math`, `-march=`/`-mfma`/`-mavx` flags.
   - The Dell has no FMA. The AROS build does not pass `-ffp-contract=off`,
     and it stays exact only because the baseline x86-64 target cannot emit
     FMA. Do not add any `-m` flag that would change this.
2. **What is allowed:**
   - **memoising** a pure function of identical inputs (same bits in → same
     bits out);
   - **skipping** a computation whose result is provably unused, while
     preserving every side effect on state;
   - **moving** code (`static inline` into a header) with identical operations;
   - **replacing a loop with an equivalent exact operation**, proven
     exhaustively (see 3).
3. **Kernel rewrites need an exhaustive proof.** Any change to `ri_sin`,
   `ri_pow2`, `ri_scale2`, `ri_log2`, `ri_exp` or `ri_tanh` must be proven
   bit-identical over **every finite float input** (2^32 bit patterns,
   including NaN/Inf/subnormal handling, which must match too).
   - Run this as a host tool and record its runtime and result in the
     evidence. It runs once per change; it does not belong in the audit.
   - Keep the existing `t2`/`t3` kernel tests green.
   - `ri_scale2`'s stepwise ×2/×0.5 can round differently from a single
     power-of-two multiply when the result is **subnormal**. That is exactly
     the case the exhaustive proof exists to catch.
4. **State matters, not just output.** A cut that leaves this sample's output
   identical but changes any state a later sample reads is **not exact**.
   - Examples: filter states, phases, envelope values, `o->last`, `o->amp`,
     `o->ps`, LFO `wrapped`, `any_on`, `rmul`/`lmod`/`smod`/`stmod`,
     `opm_on`, `melmod`, `vcount`/`vhold`.
   - The oracle (P0) must therefore run long scripted sequences, not single
     samples.
5. **Host and target must agree.** After the exact cuts, one ABIv1 or ABIv11
   build of the oracle corpus must produce the same hashes as the host (G11),
   or you must record precisely why not.

---

## 3. Hard rules

1. **Clean-room:**
   - no ASM (the original hardware maker's) content of any kind;
   - manuals stay out of the repo;
   - cite manual pages by number only, as the existing comments do.
2. **Realtime contract (spec §4, LOCKED).** In the render path:
   - no allocation, no DOS/Intuition calls, no locks/`Forbid`/`Disable`;
   - no unbounded loops, no mutable statics; caches live **in the voice or set
     struct**, never in a `static`;
   - the audit greps render code literally, so do not write the string
     `free(` in engine code or comments.
3. **One renderer (spec §5).** Live and offline must stay sample-identical. The
   existing one-renderer tests must stay green.
4. **TDD with a behavioural RED,** as in the G9 prompt §3.1:
   - every new test is first seen failing for the right reason;
   - every invariant gets a **mutation proof**: mutate, see FAIL, revert, see
     PASS;
   - the mutant must compile: with `-Werror`, a mutant that fails to build
     silently re-runs the old binary;
   - `ri_build_host.sh test NAME` links the objects already in `/tmp/ri/build`
     and does **not** rebuild them, so run `ri_build_host.sh all` (or
     `engine`) first, every time.
5. **`bash scripts/ri_audit.sh` must end in `AUDIT 0/0 PASS` before every
   commit.**
   - If another session's WIP is in the tree, verify in a scratch
     `git worktree` at HEAD plus your files. The audit needs the sibling
     symlink `<scratch>/Vulkan4Aros → /home/miller/Work/projects/Vulkan4Aros`.
   - If you redirect `/tmp/ri` for isolation, revert every redirect before
     committing. Then `git show HEAD | grep -c '<your redirect path>'` must
     print 0.
   - `scripts/` must keep exactly the files it has now.
6. **Commits:**
   - commit only your own files (never `git add -A`); check `git status` for
     foreign modified files before every commit and never stage them;
   - message tag `[levi-perf]`, trailer
     `Co-Authored-By: OpenCode <noreply@opencode.ai>`;
   - **do not push.** The advisor reviews first.
7. **Benchmarks are ungated.** `levi_bench` is on the audit's
   `UNGATED_ALLOW` list on purpose, because it asserts no machine-dependent
   bound. Keep it that way, and put new timing harnesses on the same list
   with a one-line reason.
   - Correctness and counter checks belong in **gated** tests.
8. **No instrumentation cost in the shipping build.** Profiling counters are
   compiled in only under `#ifdef RI_LEVI_PROFILE` (host only).
   - With the macro undefined, the engine objects must be **byte-identical**
     to a build without your counter code. Compare `objdump -d` of `levi.o`
     with and without the patch, after removing the counter lines; or show
     that the `#ifdef` regions are the only diff.
   - The existing `vc_*` counters stay as they are.
9. **Songs.** `songs/local/` is git-ignored (covers of other people's music)
   and the repo is public:
   - never commit audio, WAVs or rendered excerpts of those songs;
   - **hashes and timing numbers are fine.**
   - `songs/demo/riapp-demo.rbng` is in the repo and may be used by gated
     tests.
10. **Do not install system packages.** `perf` and `valgrind` are not
    installed on this host; `gprof` is.
    - Use `gprof` (`-pg`), the `RI_LEVI_PROFILE` counters, and
      `clock_gettime` timing.
    - If you believe `perf` is essential, say so in the report and let the
      owner decide.
11. **Lanes:**
    - **Dell rules:**
      - only the owner reboots the Dell;
      - never quit an RIAPP you did not start; check `--ui-windows` first and
        stop and ask if the owner's RIAPP is up;
      - deploy test binaries to `RAM:` (e.g. `RAM:RIAPPLP`), never over
        `Vk4aros:ReIncarnation/RIAPP`;
      - **one `--ui-capture` per job**, never inside a long action list;
      - read logs with `--get`, never by polling `Type`;
      - do not read `RIAPP.LOG` while RIAPP is running if you can avoid it.
    - **Do not touch lanes other sessions use.** On riqemu1 the spooler is on
      port 9295, and riqemu1 must come back at 1280x1024 if anything restarts
      it (verify with a screendump).
    - The Dell does not need sound for this work.
12. **Owner decisions are the owner's.** P3 builds, measures and **reports**
    the sound-changing options. It does **not** enable any of them by default,
    and it does not pick one.

---

## 4. Phases

### P0: the bit-exactness oracle (before any optimisation)

Goal: a gated test that fails if Levi's output changes by a single bit. Every
later phase depends on it.

1. **New gated test `tests/unit/t172_levi_bitexact.c`.** If 172 is taken by
   the time you start, use the next free number and keep the name.
   - **Corpus.** Each case is a fixed script of setter calls and note events
     at fixed sample offsets, rendered through `levi_voice_render_sum_stereo`
     in blocks of **64** and also of **256** (the device buffer), at 48000 Hz,
     for at least 2 s. Include release tails. Cover at least:
     - every algorithm preset;
     - every wave: all 16 classic waves plus one member of each family
       (PULSE, HARM, FOLD, WARP, SYNC, RING, CHEBY, including the longest
       CHEBY);
     - operator modes FM, PM, PWM, SYNC, PDSAW, PDSQ and the PD default; self
       feedback; invert; direct out; mute; solo;
     - every digital filter type (`RI_LEVI_NDF` of them, including VOWEL and
       both SVF morph types) with `dpost` 0/1 and drive 0/max; the analog
       filter with drive 0 and > 0 and resonance near self-oscillation;
     - **morph:**
       - 0, 50 and 100 held;
       - morph **moved during a held note** (0 → 60 → 0);
       - a matrix route to ALGO;
       - a slot list with 3 entries crossing a step (the
         `st[0]`↔`st[1]` copy in `morph_apply`);
     - the matrix: empty; one route per destination family (OSC, ENV,
       LFO, DFILT, AFILT, VCA, ALGO, VOICE, DELAY, REVERB, PREFX,
       POSTFX, ARP, SEQ); 8 routes at once; macros;
     - per-voice LFOs consumed and not consumed; shared LFOs with
       `trig` 1 and 2, with and without stagger; LFO-triggered mod
       envelopes;
     - mod envelopes with non-zero curves (`seg_shape`); envelope time
       scaling through the matrix;
     - pan modes 0/1/2, with width, spread, per-op pan, and centred pans
       (the dual-mono case);
     - vintage (bits < 16, decimation > 1) and its exact bypass;
     - voice pitch: detune, analog feel, vibrato with delay, glide and
       glissando, bend, glide hold;
     - velocity and aftertouch amounts; zones; chord mode; mono and
       legato allocation; voice stealing with all 8 voices busy;
     - the FX chain bypassed and active;
     - one 8-voice, 8-op, morph-50, matrix-heavy "worst case" patch.
   - **Hash.** FNV-1a 64 over the `uint32_t` bit pattern of every L and R
     sample (via `memcpy`, no type punning UB), one hash per case.
   - **Pins.** Generate the pins **on the current HEAD before you change any
     engine file**, and commit them as constants in the test.
     - Record the HEAD hash you generated them on in a comment and in the
       evidence.
     - This test passes on HEAD by construction. Its RED is the mutation
       proof (below).
   - **Mutation proofs (record them in the commit body):**
     - change `0.1591549f` in `mod_warp` to `0.1591550f` → FAIL;
     - make `tpt_g` clamp at `1.44f` → FAIL;
     - skip bank 1's `voice_pass` when `emorph <= 0` (the naive H1 cut) →
       must FAIL on the "morph moved during a held note" case. **This
       proves the oracle sees state, not just output.**
     - swap the order of `ml += vl` accumulation over voices (v descending)
       → FAIL (proves accumulation order is pinned).
   - Add it to the audit, next to the other Levi gates, with the failure text
     `FAIL: t172_levi_bitexact (levi output must be bit-identical to the pinned corpus)`.
2. **Song-level oracle (manual, not gated).** Add a `--f32 FILE` option to
   `tools/songplay.c` that also writes the raw float L/R stream, or a
   `--hash` option that prints FNV-1a 64 over it.
   - Record the hash of `songs/demo/riapp-demo.rbng` (gated-safe, so pin it
     in a test or the audit if it contains Levi), and of
     `songs/local/zombie-nation/zombie-nation.rbng` and
     `songs/local/the-knife/the-knife.rbng`. The last two go in the evidence
     only; they are git-ignored.
   - These three hashes must not move in P2.

**P0 commit:** `levi-perf P0: bit-exact oracle over a Levi corpus (t172) + songplay float hash [levi-perf]`.

### P1: a bench and profile that measure what they claim

1. **Fix the operator sweep.** It must change `live[]` for real: set slots
   and then call the public path that re-runs `morph_apply`, or set the
   algorithm or custom feeds through `levi_set_feeds`/`levi_set_route`.
   - Assert with a counter that the number of operators actually rendered
     per voice-sample equals k for each row.
   - **A sweep row without a positive control is not a result.**
2. **Fix the morph sweep.** Report bank-0 and bank-1 `voice_pass` calls per
   voice-sample (counter) next to the timing.
   - Add a row that prices the second bank directly: time `voice_pass` for
     bank 1 alone, or time the render with a temporary bench-only build that
     skips bank 1.
   - Label that row clearly as "not exact, price only".
3. **Counters under `RI_LEVI_PROFILE`**, all reported per voice-sample:
   - operators rendered;
   - `voice_pass` calls per bank;
   - `ri_sin`, `ri_pow2`, `ri_log2`, `ri_tanh` and `ri_exp` calls (wrap them
     with counting macros in the profile build only);
   - `tpt_g` calls, and how many had the same `(fc, sr)` bits as that filter
     instance's previous call (the H2 memo hit rate);
   - `levi_mod_apply` calls, and matrix rows evaluated;
   - per-voice LFO steps;
   - `env_tick_b` calls (op envs and mod envs separately);
   - filter-chain calls L and R, and how often `mixL == mixR` **and** both
     channel states were bit-equal (the H6 hit rate);
   - `pan_gains` calls, and how many had unchanged inputs (the H7 hit rate).
4. **Sweeps (host, `-O2`, the shipping flags of `ri_build_host.sh`).** Print
   ns per block and µs per voice-sample, as the **median of 7 runs**, with
   min and max:
   - voices 0..8 (as now);
   - operators 1..8 (fixed, with the control);
   - morph 0/50/100 with the bank counters;
   - matrix empty / 1 route / 8 routes;
   - each digital filter type, and analog drive 0 vs > 0;
   - pan mode 0/1/2 and centred vs spread;
   - LFO consumers none vs one;
   - mod-envelope curve 0 vs non-zero;
   - the "worst case" patch from P0.
5. **Profile.** Build the bench with `-pg` (and `-O2`), run it on the
   worst-case patch and on 4 voices of a typical patch, and record the flat
   profile and the call graph in the evidence. Cross-check gprof's
   attribution against the counters, because gprof's sampling is coarse.
   - **Deliverable:** a cost model table, **% of voice-render time by
     function and by hypothesis H1–H9**, with each row's evidence (counter
     plus profile).
6. Re-state the 2026-10-04 conclusions in the evidence file as **confirmed**,
   **refuted**, or **unmeasured** with this better instrument, quoting the old
   lines verbatim.

**P1 commit:** `levi-perf P1: bench sweeps with positive controls, RI_LEVI_PROFILE counters, cost model [levi-perf]`.

### P2: exact cuts, one per commit, largest measured prize first

Order them by the P1 cost model, not by this list. For each cut:

1. Name the hypothesis and quote its P1 numbers (counter and share).
2. State the **exactness argument** in the commit body: why the output and
   every piece of state are bit-identical (§2).
3. `t172` and every Levi test are green with **unchanged pins**. The song
   hashes from P0 are unchanged; quote them.
4. Show the bench before and after (median of 7, min/max), at `-O2` **and**
   `-O0` (the app is `-O0`, the engine `-O2`, but report both).
5. Give a mutation proof that the oracle catches a deliberately *wrong* version
   of this cut: for example, a memo keyed on `fc` only and not `sr`, or a
   dual-mono copy that ignores the state check. **If no wrong version is
   caught, the oracle is too weak: extend the corpus first.**
6. Run the audit to `AUDIT 0/0 PASS`, then commit
   `levi-perf P2.n: <cut> (<-x %> voice-sample) [levi-perf]`.

Candidate cuts and their exactness conditions:

| Cut | What it does | Exact when |
|---|---|---|
| C1 (H2) | Memoise `tpt_g` per filter instance: store the last `fc` and `sr` bits and the result, in the voice struct, per channel and per filter (and per vowel formant). | The key is the full bit pattern of both inputs. `filt_clear` and voice trigger must reset the memo to "invalid", not to 0. |
| C2 (H4) | Compute `s = ri_sin(...)` in `ri_levi_wave` only on the branches that use it. | Always, since it is the same expression on the same input. Keep `ri_levi_wave`'s public behaviour for every `w`. |
| C3 (H5) | Empty-matrix fast path in `levi_mod_apply`. | Every side effect of the full path is reproduced: `rmul = 1`, `lmod`/`smod`/`stmod = 0`, the `*_on` flags cleared together with their arrays and `susmod`/`cmod`, `melmod` zeroed, and every `e*` output clamped exactly as now. Note that the clamps change values (e.g. `ecut` into [40, 18000]), so a voice whose base cutoff is outside that range must keep being clamped. |
| C4 (H5) | Per-voice LFOs: skip **evaluating** a voice LFO no consumer reads this sample. | Its phase and wrap state still advance exactly as before (`lfo_advance` stays). Anything that reads `l->value`, `wrapped`, or sample-and-hold state later must see identical values. If this cannot be shown exact, it moves to P3. |
| C5 (H6) | Dual-mono: when `mixL == mixR` bitwise and the L and R filter states are bitwise equal, run one chain and copy its output and its state to the other channel. | Keep a per-voice "channels in lockstep" flag and drop it the moment the inputs differ. Correctness must not depend on the flag being right: check the inputs every sample, and use the flag only to skip the state compare. |
| C6 (H1, exact part only) | Skip bank 1's `voice_pass` only where that is provably exact: for example, when bank 1's configuration (`feedsB`, `orderB`, `liveB`) equals bank 0's and the two state arrays are bitwise equal, render once and copy. | **The general skip when `emorph` is 0 is not exact** (bank 1's state goes stale and is heard when morph moves). It belongs in P3, priced. Note also that `any_on` ORs both banks' envelopes, so bank 1 can keep a voice alive. |
| C7 (H3, H8) | `ri_scale2`: replace the fixed 32-iteration loop with an exact equivalent. Then make `ri_sin`/`ri_pow2`/`ri_log2` `static inline` in `kernels.h` (or a `kernels_inl.h` that `kernels.c` also uses, so there is one definition). | The exhaustive 2^32 proof of §2.3, per function changed. The audit's libm and kernel rules still hold (no libm calls). |
| C8 (H7) | Memoise `pan_gains` per op on `(pp bits, pmode)`. | Keyed on the full bit pattern. |
| C9 (H9) | Envelope fast paths: a segment with curve 0 already returns `x`. Look for exact early-outs such as sustain-stage ticks that change nothing, or idle mod envelopes. | No change to `value`, `stage`, `stage_t` or `relpend` sequencing. |
| C10 | Hoist per-block invariants out of the per-sample loop: `hc`, `hc2`, `panoff`, the `vel01`-derived terms, `RI_LEVI_NVOICES` arithmetic. | Hoist only expressions whose inputs cannot change inside the block; setters run between blocks (one block behind, by design). Prove it from the call graph, not by assumption. |

Stop P2 when the next candidate's measured prize is under 2 % of voice-render
time, or the list is exhausted. Report the cumulative result as µs per
voice-sample before and after, at `-O2` and `-O0`, on host.

### P3: sound-changing options, measured and reported, not enabled

For each option:

- build it behind a **default-off** switch: a compile-time
  `RI_LEVI_OPT_<NAME>` for the bench, and, if it needs a run-time knob for
  the Dell, an `ENVARC` variable read once at start, like `RIAPP_AUDIO_PRI`,
  defaulting to off;
- measure its **saving**: host bench µs per voice-sample, and the Dell
  `lev-voice`/`levi` per block if you get to P4 with it;
- measure its **deviation** against the exact build over the P0 corpus and the
  three songs:
  - the max absolute sample difference;
  - the RMS of the difference signal in dBFS, and relative to the signal's
    own RMS;
  - the worst case and where it occurs;
- describe the **audible risk** in one plain sentence (e.g. "zipper noise on
  fast filter sweeps", "a click when morph leaves 0 during a held note").

**Do not enable any option, and do not change the default output.** `t172`
must stay green with every option off.

Options to build and price (add others the profile suggests):

| Option | What it changes |
|---|---|
| O1 Control rate | Update envelopes, LFOs, matrix, `levi_mod_apply`, pitch `ri_pow2` and filter coefficients every N samples (N = 8, 16, 32), with linear interpolation of the smoothed parameters. Price each N. |
| O2 Bank skip | Skip bank 1 when `emorph` is exactly 0 (and bank 0 when exactly 100), with a defined resync rule when morph moves: either copy bank 0's state into bank 1 on resume, or restart bank 1's phases. Price it, and give the click risk on a moving morph. |
| O3 Float kernels | Single-precision `ri_sin`/`ri_pow2` (lower-order polynomial in float), or a table sine with linear or cubic interpolation. Give the max error in ulps and in dB, and the saving. |
| O4 Polyphony / quality switch | A per-device voice cap (4/6/8) or a "lite" mode. **Cutting operators from 8 to 4 or 6 is the last resort** (advisor guidance 2026-10-05); price it but list it last. |
| O5 Denormal handling | Flush-to-zero/denormals-are-zero via MXCSR in the render task. It changes bits only in subnormal ranges, and `ftz()` already does part of this by hand. Measure whether subnormals occur in the corpus at all, with a counter, before pricing it. |

Write the options table to
`docs/evidence/levi-perf/2026-10-XX-sound-changing-options.md` with columns:
option, switch name, saving (host µs/voice-sample and %), deviation (max abs,
RMS dB), audible risk, and recommendation. Add one line per option to
`docs/2026-09-24-improvement-todo.md` under a "Levi performance: owner
decisions" heading, unticked. The owner picks.

**P3 commit:** `levi-perf P3: sound-changing options behind default-off switches, priced [levi-perf]`.

### P4: prove it on the Dell (ABIv11)

1. Build the mixed configuration with `scripts/ri_build_v11.sh` **from the repo
   checkout** (a `git archive` export stamps `build=?`). Confirm:
   - the build prints `AROS RIAPP MIXED VERIFIED: ...`;
   - the startup log line reads `RIAPP LOG build=<your hash> diag=...`.
2. Build two binaries:
   - **A** = the commit before P2 (P1 head);
   - **B** = P2 head (exact cuts only, every P3 option off).
3. Deploy both to `RAM:` (`RAM:RIAPPA`, `RAM:RIAPPB`). Never overwrite the
   stick's `RIAPP`.
4. Run an **A,B,B,A** protocol on Zombie Nation
   (`Vk4aros:ReIncarnation/songs/local/zombie-nation/zombie-nation.rbng`).
   - Each cell plays for at least 180 s: the Levi part does not sound in the
     first ~32 s (2026-10-03 lesson).
   - Use `RIAPP_DIAG=1` in `ENV:` only, **never `ENVARC:`**, and unset it
     afterwards.
   - Collect from the logs: `hb:` load, `render_max`, xruns, overloads, and
     `dstg levi` / `lev-voice` with their `n`.
   - Subtract the timer tax (about 4 µs per inner stage pair on the Dell)
     before quoting shares, and say that you did.
5. Repeat once on The Knife.
6. **Pass condition:** B's `lev-voice` and the heartbeat load are lower than
   A's in both B cells against both A cells, with xruns 0 in all cells.
   - Quote the A/B ratio.
   - If the Dell ratio disagrees with the host ratio by more than 25 %, say so
     and give the likely reason; do not hide it.
7. **Optional, only if the owner says yes in chat:** run O1 at the N the owner
   names on the Dell, as a third arm.

Leave the Dell as you found it:
- `RAM:` test binaries removed;
- `RIAPP_DIAG` unset;
- no RIAPP of yours running.

---

## 5. Evidence layout

`docs/evidence/levi-perf/`:

- `2026-10-XX-p0-oracle.md`:
  - the corpus list;
  - the HEAD hash the pins were generated on;
  - the song hashes;
  - the mutation-proof outputs, verbatim.
- `2026-10-XX-p1-cost-model.md`:
  - all sweep tables, verbatim, with the controls;
  - the gprof flat profile (top 30) and call graph excerpt;
  - the counter table;
  - the re-statement of the 2026-10-04 conclusions.
- `2026-10-XX-p2-exact-cuts.md`: one section per cut, with before/after,
  exactness argument, and mutation proof.
- `2026-10-XX-sound-changing-options.md` (P3).
- `2026-10-XX-p4-dell.md`:
  - verbatim log lines per cell;
  - the build hashes;
  - the A,B,B,A table.

Every number in an evidence file is either verbatim tool output, or derived
with its components shown.

---

## 6. Things that will waste a day if you forget them

- `ri_build_host.sh test NAME` does not rebuild engine objects. Run
  `ri_build_host.sh all` first, or you will measure the old code.
- A bench without a positive control can print a confident flat line over
  nothing (§0.1). Every row needs a counter that proves the swept quantity
  changed.
- The `dstg` table is a cumulative average with no notion of which part of the
  song it covered. Play long enough, and quote `n`.
- Every enclosing stage reads high by its child count × the per-pair timer cost.
- `render_max` is not a health metric. Use xruns, load and overloads.
- `Type` output over the agent is capped, and a reader holding `RIAPP.LOG`
  on the stick's FAT handler used to drop lines (fixed in `fdcea19`, but keep
  reads out of live runs anyway). Use `--get`.
- With `-Werror`, a mutant that does not compile re-runs the old binary and
  "survives".
- `songs/local` must never reach git.

---

## 7. Out of scope

- The 303/808/909 engines. 909's tail is a separate item.
- GUI and tab-switch work.
- Changing the mixed-build split (engine `-O2`, app and GUI `-O0`). That is
  the owner's debuggability rule.
- SMP or threading (spec §4.1 defers it).
- Any default change to Levi's sound.

---

## 8. Success gates (all must PASS)

| Gate | PASS condition | Evidence |
|---|---|---|
| G1 | `t172_levi_bitexact` exists, is gated in the audit, pins generated on a named pre-P2 HEAD, covers every item in P0.1's list | P0 evidence, corpus list |
| G2 | The four P0 mutation proofs each FAIL as mutated and PASS as reverted, including the naive bank-skip mutant | P0 evidence, verbatim |
| G3 | The 2026-10-04 operator-sweep defect is confirmed or refuted with a counter, and the fixed sweep's operator counter equals k on every row | P1 evidence |
| G4 | The morph sweep reports per-bank `voice_pass` counts, and the bank-1 price is measured | P1 evidence |
| G5 | `RI_LEVI_PROFILE` off ⇒ `levi.o` disassembly unchanged apart from the `#ifdef` regions (shown) | P1 evidence |
| G6 | A cost model attributes ≥ 85 % of host voice-render time to named functions or hypotheses, with counters and the gprof profile agreeing in rank order on the top 5 | P1 evidence |
| G7 | Every P2 cut: t172 pins unchanged, song hashes unchanged, an exactness argument written, a wrong-variant mutant caught, audit 0/0 | P2 evidence, commit bodies |
| G8 | Any kernel rewrite: exhaustive 2^32 bit-identity proof recorded (runtime and mismatch count = 0) | P2 evidence |
| G9 | Cumulative host improvement reported as µs/voice-sample before → after at `-O2` and `-O0`, median of 7 with min/max | P2 evidence |
| G10 | Every P3 option is default-off, t172 green with all options off, and the table has saving, deviation and risk for each | P3 evidence |
| G11 | One target build (ABIv1 or ABIv11) reproduces the host t172 hashes, or the reason it cannot is recorded precisely | P2 or P4 evidence |
| G12 | Dell A,B,B,A: B < A on `lev-voice` and load in all four pairings, xruns 0 in every cell, build hashes logged per cell | P4 evidence |
| G13 | Dell left clean: no `RAM:` test binaries, `RIAPP_DIAG` unset, no RIAPP of yours running | P4 evidence (a `dir RAM:` and a `getenv` line) |
| G14 | `AUDIT 0/0 PASS` at every commit; no foreign files staged; nothing pushed; no `songs/local` content in git | `git log --stat`, `git status` |

---

## 9. Reporting and handoff

After each phase, report in at most 15 lines:

- the gates passed, with a one-line number for each;
- the commit hash;
- anything that surprised you, and anything you could not prove.

If you stop mid-way, end with:

```
HANDOFF levi-perf
  last green phase : P?
  last commit      : <hash>
  next step        : <one line>
  open questions   : <list>
  lanes touched    : <Dell? riqemu1?> and state left in
```

## 10. Owner decisions (do not decide; list them in the final report)

1. Which P3 options to enable, and at what setting (e.g. control rate N).
2. Whether a "lite"/quality switch should exist in the GUI at all.
3. Whether to ship the exact cuts to the stick (`Vk4aros:ReIncarnation/RIAPP`).
   The advisor deploys after review.
