# Prompt for OpenCode: turn on Levi control rate (N = 8) and morph-bank skip; remove the other options

> Written 2026-10-06 by the Claude advisor session. It follows
> `docs/superpowers/plans/2026-10-06-levi-perf-opencode-prompt.md` (your
> levi-perf P0–P4, `4529579`..`e6d96b2`) and the advisor's review fix
> `e125db8`, which is on main and deployed to the Dell stick.
>
> **The owner has decided (2026-10-06):**
> - **O1 control rate:** ON, at **N = 8**.
> - **O2 morph-bank skip:** ON.
> - **Tidy-up:** remove the code of every option that was not taken: O3 float
>   kernels, O4 voice cap, and the O5 subnormal probe. Keep their evidence.
>
> The work has **four phases (Q0–Q3)**. Each phase ends at a commit and a short
> report. If you run short of context, stop at a phase boundary and write the
> handoff block (§8).
>
> **You are done when every gate in §7 reads PASS, with its evidence where the
> gate names it.**

---

## 0. Context and what changes in kind

Until now the Levi render had one rule: **bit-exact to the pre-optimisation
engine** (t172, pinned on `ebc94ec`). The owner is now deliberately trading a
small, measured deviation for speed, so that rule ends for **some** cases and
must **keep holding** for others. Getting that split right is the core of this
task.

| Rule | Applies to |
|---|---|
| **Must stay bit-exact** (t172 pin unchanged) | Every case whose patch has **no modulation motion** (no matrix route sounding, no vibrato, no glide, no drift, no LFO or envelope → filter amount, no bend) **and** whose morph does not move off an endpoint during a held note. P3 measured both options as bit-identical on static patches, on the demo song and, for O2, on both real songs. That has to remain true after the code is made unconditional. |
| **May change** (deliberate re-pin, §3 Q2) | Cases with modulation motion (O1) and cases where morph moves during a held note (O2). Each changed pin needs its measured deviation against the exact engine and a one-line reason. |

### 0.1 Defects in the P3 prototypes that must be fixed before they ship

The advisor found these in `a284347`. They are not optional.

1. **Control-rate state is never reset on a new note.**
   - `ctl_init` is set to 1 once and never cleared (`grep -n ctl_init engine/dsp/levi.c`).
   - A voice that is re-triggered or stolen therefore interpolates `vpitch`,
     `dc` and `ac` from the **previous note's** control targets for up to
     N − 1 samples.
   - Its held `ctl_e[]` values (cut, reso, levels, morph, …) also come from
     the previous note until the next control point.
   - At note-on this is audible as a pitch and filter smear.
   - **Fix:** clear `ctl_init` (and reset `ctl_k`'s phase) everywhere a voice
     starts or restarts sounding, so the first sample of a note computes fresh
     targets. That covers `levi_trigger`, the allocator's retune and legato
     paths, voice steal, and the `any_on == 0` deactivation (`filt_clear`
     path).
   - Decide deliberately whether a **legato** retune (no envelope restart)
     keeps interpolating. It is the one case where carrying on is arguably
     right. Write the decision and the reason in the code comment and in the
     evidence.
   - A test must catch the stale state (Q1 test 3).
2. **The O2 skip clicks on LFO-driven morph.**
   - P3 measured up to **0.9258 abs (full scale)** deviation when an LFO → ALGO
     route swings morph through 0. The skip toggles every time `emorph` touches
     the endpoint, and each resume copies `st[0]` over `st[1]`.
   - **Fix:** skip a bank only when the morph value is **static at the
     endpoint**:
     - (a) no active matrix or macro route targets `RI_LEVI_DM_ALGO` (a
       per-matrix cached flag maintained exactly like `mx_empty`, refreshed by
       the same setters); **and**
     - (b) `emorph` has sat exactly at that endpoint for a hold time of at
       least one engine block (64 samples), counted per voice.
   - Anything else renders both banks, as the exact engine did. Knob and
     automation moves then pay at most one resync per move, and LFO-driven
     morph never skips.
   - Keep the existing resync rule (copy the live bank's op states into the
     resumed bank, the `levi_set_morph` join). Re-measure the deviation of the
     morph-move cases after the fix. **PASS requires the LFO → ALGO probe to be
     bit-exact** (it no longer skips), and a knob move 0 → 60 → 0 to have its
     deviation reported.
3. **O1 holds gain-like values as steps.**
   - Between control points, `evlevel`, `eoplevel` and `edlevel` (VCA level,
     operator level, digital-filter level) are **held**, not interpolated. A
     matrix route to VCA level therefore steps every 8 samples (a 6 kHz
     staircase, which is zipper noise on fast amplitude modulation).
   - **Fix:** interpolate those three linearly between control points, the same
     way `dc`/`ac`/`vpitch` are.
   - Leave `ereso`, `edrive`, `edenv`, `eaenv`, `edlfo`, `ealfo`, `evlfo` and
     `emorph` held. Note it in the comment. The resonance and drive steps are
     small at N = 8, and `emorph` must stay held so that O2's endpoint test sees
     exact values.
   - Measure the before/after deviation of a matrix-heavy case to show the fix
     helped.
4. **The `ctl_k` wrap.**
   - `ctl_k % N` stays consistent across the `uint32_t` wrap only if N divides
     2^32.
   - Fix N = 8 as a named constant (`RI_LEVI_CTRL_N 8u`).
   - Add a compile-time check that it is a power of two, using the
     negative-array `typedef` idiom (the host build is `-std=c99`, so no
     `_Static_assert`).

---

## 1. Read first

1. Your own evidence: `docs/evidence/levi-perf/2026-10-06-p0-oracle.md`,
   `…-p1-cost-model.md`, `…-p2-exact-cuts.md`, `…-p3-options.md`,
   `…-p4-dell.md`.
2. Commit `e125db8`: the dual-mono lock must be cleared by the mono render. The
   same class of bug can appear for any new per-voice state you add, so any
   per-voice cache you touch must be reset or invalidated on **both** render
   entries (`levi_voice_render` and `levi_voice_render_stereo`).
3. `docs/superpowers/plans/2026-10-06-levi-perf-opencode-prompt.md` §2 (the
   exactness rules), §3 (hard rules) and §6 (traps). **They all still apply**,
   except the bit-exact rule, which §0 narrows.
4. `docs/2026-09-24-improvement-todo.md`, the section "Levi performance: owner
   decisions".
5. Code: every `RI_LEVI_OPT_*` block in `engine/dsp/levi.c`, `levi.h`,
   `kernels.c`, `kernels.h` and `tools/songplay.c`; `tests/unit/t172_levi_bitexact.c`;
   `tests/unit/levi_bench.c`.

---

## 2. Hard rules (same as the levi-perf prompt §3, restated briefly)

- **Clean-room.** Realtime contract (spec §4, LOCKED): no allocation, IO,
  locks or mutable statics in the render path, and no `free(` string anywhere
  in engine code.
- **One renderer.** Live and offline stay sample-identical (the existing tests
  stay green).
- **TDD with a behavioural RED,** and a mutation proof for every new law. The
  mutant must compile; run `ri_build_host.sh all` before `test NAME`, every
  time.
- **`bash scripts/ri_audit.sh` must read `AUDIT 0/0 PASS` before every
  commit.** Use an isolated worktree if the tree has foreign WIP, and revert
  any path redirects before you commit.
- **Commits:**
  - commit only your own files; tag `[levi-perf]`; trailer
    `Co-Authored-By: OpenCode <noreply@opencode.ai>`;
  - **do not push** and **do not deploy to the stick**. The advisor reviews
    first.
- **Songs:** `songs/local/` content never reaches git. Hashes and numbers are
  fine.
- **Dell:**
  - test binaries go to `RAM:` only;
  - check `status` before you start anything; if an RIAPP you did not start is
    running, stop and ask;
  - never quit the owner's instance;
  - one `--ui-capture` per job at most (`--ui-windows` has reset the agent
    before, so prefer `status`);
  - read logs with `--get`;
  - only the owner reboots the Dell.
- **Keep `RI_LEVI_PROFILE`.** It is measurement infrastructure, not an
  option. Everything it gates stays host-only, and the shipping objects stay
  free of it.

---

## 3. Phases

### Q0: freeze the exact reference before anything changes

Once the options are unconditional there is no exact path left to compare
against, so build the reference first.

1. Build the **exact** engine objects from `e125db8` (HEAD at the time this
   prompt was written) into a scratch directory outside `/tmp/ri`, for example
   by checking out a worktree and redirecting `OUT`.
2. Write a host comparison tool **outside the repo** (in your scratch area).
   It links one object set at a time and writes, for every t172 case and for
   the three songs via `songplay --f32`:
   - the max abs difference;
   - the RMS of the difference in dBFS, and relative to the signal's RMS;
   - the first differing sample index.

   Run it as exact-vs-exact first. **All zero is the control**, and it proves
   the tool.
3. Classify every t172 case as **static** (must stay bit-exact) or **moving**
   (may change), from the patch definition rather than from the result. Write
   the table to the evidence before you run any new code.
4. Report and stop (no commit: nothing in the repo changed). Put the
   classification table in `docs/evidence/levi-perf/2026-10-06-q-enable.md`
   §Q0, which gets committed with Q1.

### Q1: make O1 (N = 8) and O2 unconditional, with the §0.1 fixes

Work in this order, one commit at the end of Q1 (or one per item if that is
clearer; your call, but all of Q1 must be green before Q2).

1. **Remove the `#ifdef RI_LEVI_OPT_CTRLRATE` / `#else` scaffolding in both
   render entries.** Keep the control-rate path; delete the per-sample path it
   replaced. Two cautions:
   - the per-sample `voice_pitch_step` must not survive as dead code;
   - `voice_pitch_advance`/`voice_pitch_eval` must keep the exact time
     integrals (the P3 claim, "trajectory stays exact", must still hold:
     vibrato phase, glide time and drift time advance per sample).
2. **Apply fix 0.1.1 (the reset)** and decide the legato case.
3. **Apply fix 0.1.3 (interpolate the gain-like values).**
4. **Apply fix 0.1.4:** the named `RI_LEVI_CTRL_N` and its power-of-two check.
5. **Remove the `#ifdef RI_LEVI_OPT_BANKSKIP` scaffolding.** The skip path
   becomes the only path, with fix 0.1.2 (the static-morph gate: no ALGO
   route, and endpoint held for at least 64 samples).
   - Keep the existing P2 C6 `liveB_empty` exact skip; the two must compose.
     When bank B is empty, nothing is resynced.
   - Maintain the new "an ALGO route exists" flag in the same setters that
     refresh `mx_empty`, and reset the hold counter wherever 0.1.1 resets the
     control state.
6. **New gated tests.** Use the next free numbers; the names below are the
   intent.
   - `t173_levi_ctrlrate`:
     - (1) a static patch renders bit-identical to the Q0 exact reference
       hash, embedded as a pin taken from the t172 static case;
     - (2) with an LFO → cutoff route, the filter coefficient changes only at
       control points: count distinct `dc` values per 8-sample window through
       a test hook or the `RI_LEVI_PROFILE` counter, and get exactly 1 new
       target per window plus interpolation;
     - (3) **the stale-state law:** play note A with a fast matrix pitch route,
       steal or retrigger the voice with note B mid-window, and assert that B's
       first sample equals a fresh-voice render of B. Mutant: remove the
       `ctl_init` reset → FAIL;
     - (4) VCA-level interpolation: a square-LFO → VCA route produces no
       8-sample plateaus of unequal height. Mutant: hold `evlevel` → FAIL.
   - `t174_levi_bankskip`:
     - (1) morph static at 0 with no ALGO route renders bank B zero times
       after the hold (counter) and the output is bit-identical to the exact
       reference;
     - (2) an LFO → ALGO route never skips (counter equals samples), and the
       output is bit-identical to the exact reference;
     - (3) a knob move 0 → 60 → 0 during a held note resyncs once per leave of
       the endpoint (counter), and the voice stays finite and bounded;
     - (4) a mono render between stereo blocks does not break the skip state
       (the `e125db8` lesson).
     - Mutants: drop the ALGO-route gate → (2) FAILs; drop the hold →
       (1) or (3) FAILs.
   - Add both to the audit next to the other Levi gates, with one-line FAIL
     texts.
7. Run the comparison tool against Q0's exact objects.
   - **Every case classified static must be bit-exact. If any is not, stop and
     find out why before going on.**
   - Report the deviations of the moving cases.

**Q1 commit:** `levi-perf Q1: control rate N=8 and static-morph bank skip are always on (owner 2026-10-06); note-start reset, gain interpolation [levi-perf]`.

### Q2: the deliberate re-pin of t172

1. Regenerate t172's pins on Q1 HEAD with `-DRI_T172_GEN`.
2. **Only moving cases may change.** For each pin that changes, the commit body
   and the evidence carry: the case name, its old pin and new pin (64 and 256),
   the max abs and RMS difference against the exact reference, and a one-line
   reason ("matrix motion now control-rate", "morph knob move resync").
3. **Block-size invariance stays:** each case's 64 and 256 hashes must still be
   equal. The control-rate phase is per voice and counts samples, so the block
   size must not matter. If they differ, that is a bug: find it before
   re-pinning.
4. Update t172's header comment. It is no longer "bit-identical to the
   pre-optimisation engine". It is the **regression pin of the shipping
   engine**, generated on `<Q1 hash>`, and the static cases are still equal to
   the `ebc94ec` pins (name them).
5. Mutation proof that the re-pinned oracle still bites: change one constant
   inside the control-rate interpolation → FAIL.
6. Song hashes: record the new hashes of the three songs in the evidence
   (`songs/local` hashes in the evidence only), with their deviation against
   the exact reference. The demo song is expected to be bit-exact (P3
   measured 0 for both options); if it is not, explain why.

**Q2 commit:** `levi-perf Q2: t172 re-pinned for the shipping engine; static cases unchanged [levi-perf]`.

### Q3: remove the options not taken, and close the paperwork

1. **Delete:**
   - `RI_LEVI_OPT_FLOATK` (`ri_sin_f`, `ri_pow2_f` and the macro mapping in
     `levi.c`);
   - `RI_LEVI_OPT_VOICECAP` (and `_N`);
   - `RI_LEVI_OPT_SUBNORM_PROBE` (in `levi.c`, `levi.h` and `tools/songplay.c`);
   - every leftover `RI_LEVI_OPT_*` name.

   Afterwards `grep -rn RI_LEVI_OPT_ engine tools tests app` must print
   nothing. The O3/O4/O5 code stays recoverable from `a284347`; say so in a
   one-line comment in the evidence, not in the code.
2. **Read the result as a reviewer would.** No orphaned helper functions, no
   stray `bank_skipped`-style fields, no `(void)afwob;` leftovers, no empty
   `#else` branches. Comment density should match the surrounding code: short
   comments, P-tags as elsewhere in `levi.c`.
3. **Size check:** report the `levi.c` line count before and after Q1 to Q3.
   The P3 commit added about 345 lines; most of the not-taken part should be
   gone.
4. **Todo** (`docs/2026-09-24-improvement-todo.md`, section "Levi
   performance: owner decisions"):
   - tick O1 as "enabled at N = 8 (owner 2026-10-06)";
   - tick O2 as "enabled, static-morph gated (owner 2026-10-06)";
   - mark O3 and O4 "not taken, code removed, evidence kept
     (`a284347`)";
   - mark O5 "no action (zero subnormal traffic)";
   - tick the "ship the P2 exact cuts" line (shipped as `e125db8`).
5. **Bench:** run `levi_bench` on Q3 HEAD against `e125db8` (same harness,
   median of 7, min/max) and report the voice-sample cost on the voices,
   matrix-8, worst, morph-0 and centred rows.
6. Run the audit to `AUDIT 0/0 PASS`, then commit.

**Q3 commit:** `levi-perf Q3: remove the options not taken (float kernels, voice cap, subnormal probe); todo closed [levi-perf]`.

### Q4 (optional, only with the owner present): listen on the Dell

The Dell is audible as of 2026-10-06. This phase needs the owner's ears, so do
it only if the owner says yes in chat.

1. Build ABIv11 from the repo checkout (`scripts/ri_build_v11.sh . <file>`).
   Check for `MIXED VERIFIED` and `build=<Q3 hash>`.
2. Copy the binary to `RAM:RIAPPQ`, check `status`, and launch it with Zombie
   Nation (`SONG=Vk4aros:ReIncarnation/songs/local/zombie-nation/zombie-nation.rbng`).
3. The owner listens, then The Knife. Collect the heartbeat (`load`,
   `render_max`, xruns) from the stick log with `--get` **after** the run,
   and compare with P4's B cells.
4. Clean up `RAM:RIAPPQ`. **Do not** copy it to the stick; the advisor deploys
   after review.

---

## 4. Evidence

`docs/evidence/levi-perf/2026-10-06-q-enable.md`, with:

- §Q0: the classification table, and the exact-vs-exact control output;
- §Q1: the fixes, one paragraph each with their mutation proofs, and the
  comparison tool output (static: all zero; moving: the deviations);
- §Q2: the re-pin table (case, old/new pins, deviation, reason) and the song
  hashes;
- §Q3: the removal list, the line counts, the bench table, and the todo diff;
- §Q4 (if run): verbatim heartbeat lines and the owner's verdict, quoted.

Every number is verbatim tool output, or derived with its components shown.

---

## 5. Traps

- **The stale-object trap** has bitten this work three times. Always
  `ri_build_host.sh all` before `test NAME`, and use your matched-pair protocol
  for A/B builds.
- **A pin that changes on a static case** is a bug, not a re-pin. Static means
  no modulation motion and no morph movement; P3 measured both options as
  identical there.
- **Any new per-voice state needs two resets:** note start, and both render
  entries (mono and stereo). `e125db8` is the precedent.
- **`emorph` must stay a held control value** so that O2's endpoint test sees
  exact `0.0f`/`100.0f`. Do not interpolate it.
- **The bench's numbers are host numbers.** The Dell is about 6.4× slower per
  voice-sample; quote host and Dell figures separately.

---

## 6. Out of scope

- New options or further speed work.
- Any GUI exposure of these settings. There is no switch; both are always on.
- The 303/808/909 engines.
- Pushing, and deploying to the stick.

---

## 7. Success gates

| Gate | PASS condition | Evidence |
|---|---|---|
| E1 | The exact reference objects (`e125db8`) are built, the comparison tool's exact-vs-exact control is all zero, and the static/moving classification is written before any new code runs | §Q0 |
| E2 | `ctl_init` (and phase) reset on every note start and deactivation; legato decision written; t173 stale-state law RED without the reset, GREEN with it | §Q1, test output |
| E3 | Gain-like values (`evlevel`, `eoplevel`, `edlevel`) interpolate; t173 plateau law RED when held | §Q1 |
| E4 | `RI_LEVI_CTRL_N` = 8 with a compile-time power-of-two check | code, §Q1 |
| E5 | Bank skip gated on "no ALGO route" plus a ≥ 64-sample endpoint hold; the LFO → ALGO probe is bit-exact to the reference; t174 mutants caught | §Q1, test output |
| E6 | Every static t172 case bit-exact to the `e125db8` reference; demo song bit-exact or explained | §Q1/§Q2 tool output |
| E7 | t172 re-pinned: only moving cases changed, each with deviation and reason; 64 = 256 for every case; mutation proof after re-pin | §Q2, commit body |
| E8 | `grep -rn RI_LEVI_OPT_ engine tools tests app` prints nothing; no dead code left; `levi.c` line counts reported | §Q3 |
| E9 | Todo updated exactly as §Q3.4 | `git show` of the todo |
| E10 | Bench Q3 vs `e125db8` reported (median of 7, min/max) | §Q3 |
| E11 | `AUDIT 0/0 PASS` at every commit; own files only; nothing pushed; nothing on the stick; no `songs/local` content in git | `git log --stat`, `git status` |
| E12 (only if Q4 ran) | Dell: xruns 0, load reported against P4's B cells, the owner's verdict quoted, `RAM:` cleaned | §Q4 |

---

## 8. Reporting and handoff

After each phase, report in at most 12 lines: the gates passed with a number
each, the commit hash, surprises, and anything unproven.

If you stop mid-way:

```
HANDOFF levi-options
  last green phase : Q?
  last commit      : <hash>
  next step        : <one line>
  open questions   : <list>
  lanes touched    : <Dell?> and state left in
```

## 9. Owner decisions still open (list them in the final report; do not decide)

1. Whether to deploy Q3 to the stick after the advisor's review, and whether
   to listen first (Q4).
2. The legato control-rate behaviour, if your decision under §0.1.1 is a
   judgement call. State it plainly so the owner can overrule it.
