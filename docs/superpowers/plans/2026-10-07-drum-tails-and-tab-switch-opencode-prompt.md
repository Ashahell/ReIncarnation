# Prompt for OpenCode: drum-section tails (909/808) and the tab-switch page cost

> Written 2026-10-07 by the Claude advisor session. The owner asked for the
> two "smaller work" items on the roadmap:
>
> - **(A) 909's occasional slow render spikes:** its worst blocks run well
>   above its average.
> - **(B) The tab switch:** it still costs 20–37 ms per switch, almost all of
>   it redrawing the new page.
>
> **Read §0 first.** Both items are measured to be **small** on today's
> build. The first job in each part is to find out whether there is anything
> worth fixing, and to **stop with a written negative result** if there is
> not. A clean "no action, here is why" is a valid, complete outcome. A fix
> without a measured prize is not.
>
> The work has two independent parts, each with phases: A0–A2 and B0–B3. Each
> phase ends with a report; code phases end with a commit. If you run short of
> context, stop at a phase boundary and write the handoff block (§8).
>
> **You are done when every gate in §7 for the phases you reached reads PASS,
> with its evidence where the gate names it.**

---

## 0. What is already known (do not re-derive; verify where marked)

### 0.1 Part A: the drum tails on today's build

Dell, ABIv11, mixed build `521ab7e`, advisor run 2026-10-07. These are the
`RIAPP dstg` lines from full-song runs, per 64-sample engine block:

| Song | Section | avg | max |
|---|---|---|---|
| Zombie Nation (ZB1) | 909 | 31 µs | 86 µs |
| | 808 | 22 µs | 88 µs |
| | 303a | 76 µs | 93 µs |
| | levi | 203 µs | 455 µs |
| | block | 508 µs | 799 µs |
| The Knife (KB1) | 909 | 54 µs | 132 µs |
| | 808 | 35 µs | 86 µs |
| | block | 631 µs | 918 µs |

The engine's 256-frame buffer runs about four 64-sample blocks. Its period is
**5333 µs**, so 4 × 64 µs ≈ 1365 µs of compute budget per block.

- **The "1.82× tail" in the backlog** comes from the 2026-10-02 `-O0` run (909
  avg 174, max 316 µs per block). At `-O2` today the 909's worst block is
  **132 µs, about 10 % of one block's budget and about 2.5 % of the buffer
  period**. The 808's max/avg is in fact *larger* (88/22 = 4.0×) than the
  909's (86/31 = 2.8×).
- **Expected explanation (to test, not assume):** a drum machine's block cost
  scales with how many voices sound in that block. A block where kick, snare,
  hats and a cymbal overlap costs several times a block with one hat. The
  "tail" would then be **polyphony, not a spike**. The 909 is sample-based
  (`engine/dsp/rb909.c`: layer mix through `ri_resample_linear`, a flam second
  playhead, a decay envelope); the 808 is modelled (`engine/dsp/rb808.c`).
- **If the cost per active voice-sample is flat** (cost tracks voices,
  `r` ≥ 0.95, as Levi's did at 0.9983), there is nothing to fix. Write that
  down and stop.

### 0.2 Part B: the tab switch on today's build

- **What earlier rounds removed:**
  - the RBay plate is cached (MIX 148–178 → 37–40 ms);
  - the rail is a page group (`rail_us` 5–37 → 1.8–4.6 ms);
  - the item cull and damage-box builds exist.
- **The last measured cycle** (Dell, `d1ebf16`, Zombie Nation), per switch:

| Tab | us | page_us | rail_us |
|---|---|---|---|
| DRUMS | 24832 | 21780 | 2764 |
| LEVI | 23632 | 20614 | 2734 |
| MIX | 31980 | 29897 | 1852 |
| FX | 42402 | 37545 | 4614 |
| SYNTHS | 34803 | 30749 | 3815 |

  The cycle took about 158 ms.
- **`page_us`** is `SetAttrs(s_pages, MUIA_Group_ActivePage, g)`, which
  includes MUI's own page handling and the **full `MUIM_Draw` of every canvas on
  the new page** (`sec` 1–6 draws). That is where the cost is.
- **What a full draw does** (`gui/widgets/rsection.mcc.c`, `draw_frame`):
  - `d->dmg_valid` is cleared for non-DRAWUPDATE draws;
  - `draw_section(&d->brp, d, 0, 0)` **rebuilds the whole display list and
    replays it** into the canvas's own off-screen bitmap `d->bm`;
  - then one `BltBitMapRastPort` to the window.
  - `d->bm` **survives hide and show**: `buf_free` runs on cleanup and on
    size change, not on page switch.
  - While a canvas is hidden its damage requests are dropped ("hidden:
    MUIM_Show's full draw covers it").
- **So every switch re-renders, from scratch, pixels that are very likely
  already sitting in `d->bm`.** That is the lead.
  - **Do not assume it is the cost.** The split between MUI's own work, our
    build, our replay and the blit on a *full* draw has never been measured on
    the Dell; only partial (damage-box) repaints have the
    `dp_build`/`dp_replay`/blit split.
- **How much it matters to the owner:**
  - 20–37 ms is one to two 60 Hz frames, so it is perceptible as a slight lag
    but not a stall;
  - the switch does not affect audio (xruns+0 throughout);
  - **treat B as polish, with a firm stop rule (§B0.4).**

---

## 1. Read first

1. llm-wiki (`llm-wiki/index.md`, then these raw articles):
   - `2026-10-02-five-sections-one-culprit-levi-is-32-percent.md` (where the
     909 tail item came from; note it is `-O0`);
   - `2026-10-04-host-bench-and-the-overload-guard.md` and
     `2026-10-06-levi-perf-exact-cuts-review-and-options.md` (the method: a
     host bench with **positive controls**, and the 2026-10-04 bench that
     measured nothing);
   - `2026-10-05-tab-switch-bay-plate-cache-and-housekeeping.md` (the TAB
     probe; the plate cache, which is the closest precedent for B);
   - `2026-10-05-rail-page-group-and-log-newlines.md`;
   - `2026-10-03-the-o2-tab-cost-is-preemption-of-prebuilt-mui-code.md` and
     `2026-10-03-correction-the-o2-tab-cost-is-not-the-damage-box-build.md`
     (an earlier wrong localisation of the tab cost: do not repeat it);
   - `2026-10-05-a-ratio-not-microseconds-this-host-is-bimodal.md` (host
     timings are bimodal by ~25 %; compare within one process);
   - `2026-10-05-which-section-owns-the-build-and-why-the-lcd-is-a-floor.md`
     (a widget is never told what changed; the toolkit declares damage-rect
     contents undefined);
   - `2026-10-06-levi-control-rate-and-bank-skip-enabled.md`, the lane notes
     (`SONG=` autoplays; per-run log in `RAM:`).
2. Code:
   - Part A: `engine/dsp/rb909.c`/`.h`, `engine/dsp/rb808.c`/`.h`,
     `engine/engine.c` (the `RIEngineStages` section stages), and
     `tests/unit/levi_bench.c` as the model for a bench with controls.
   - Part B: `gui/widgets/rsection.mcc.c` (`draw_frame`, `draw_section`,
     `build_dl`, `replay_dl_dmg`, `MUIM_Draw`/`Show`/`Hide`, `changed_id`),
     `gui/widgets/rsection_replay.inc`, and `app/riapp.c` (`tab_switch`, the
     TAB probe, the RBay plate cache `rbay_plate`).
   - Both: `docs/lane/envarc.md`.
3. Rules: `docs/superpowers/plans/2026-09-26-g9-opencode-prompt.md` §3 (hard
   rules) and §4 (lanes); `docs/superpowers/plans/2026-09-28-gui-round3-opencode-prompt.md`
   §4 (how to build GUI here, especially §4.6 build and §4.7 Dell deploy).

---

## 2. Hard rules

- **Clean-room** (spec §1). **Realtime contract** (spec §4, LOCKED):
  - any engine change keeps the render path free of allocation, IO, locks and
    mutable statics;
  - no `free(` string anywhere in engine code.
- **One renderer** (spec §5): live and offline stay sample-identical.
- **Sound must not change.** Any Part A engine change must be **bit-exact**:
  - `t172_levi_bitexact` and every 808/909 golden and unit test stay green
    with **unchanged** pins;
  - the three song hashes stay as they are on `521ab7e`: demo
    `238113a7962c112f`, Zombie Nation `26eb9a9911173c20`, The Knife
    `13dc42d73d85998b` (`tools/songplay --hash`).
- **Pixels must not change.** Any Part B change must leave every GUI golden
  (`t92`, `t93`, `t169` …) green with **unchanged** pins. Any change in what
  reaches the screen is a bug, not a re-pin.
- **TDD with a behavioural RED** and a mutation proof for every new law. The
  mutant must compile; run `ri_build_host.sh all` before `test NAME`, every
  time (the stale-object trap has bitten this project repeatedly).
- **`bash scripts/ri_audit.sh` must read `AUDIT 0/0 PASS` before every
  commit.** Use an isolated worktree if the tree has foreign WIP, revert any
  path redirects before committing, and keep `scripts/` at exactly its current
  file set.
- **Commits:**
  - commit only your own files; never `git add -A`;
  - tag `[§12.11 G9]` for Part B and `[drum-tail]` for Part A;
  - trailer `Co-Authored-By: OpenCode <noreply@opencode.ai>`;
  - **do not push** and **do not deploy to the stick**. The advisor reviews
    first.
- **Diagnostics cost nothing when off.** New counters are gated (host-only
  `#ifdef`, or `RIAPP_DIAG` on AROS). The shipping objects must not grow a
  per-sample cost.
- **Dell:**
  - test binaries go to `RAM:` only;
  - check `status` before starting anything, and never quit an RIAPP you did
    not start; the owner may have one running, so if one is up and you did not
    start it, stop and ask;
  - **`SONG=` starts playback by itself**, so do not inject a space key (it
    stops playback);
  - per-run log: `SetEnv RIAPP_LOG RAM:` before launch;
    `--ui-close "RIAPP live panel"` then `--get RAM:RIAPP.LOG`; then
    `UnSetEnv RIAPP_LOG`;
  - prefer `status` over `--ui-windows` (it has reset the agent);
  - **one `--ui-capture` per job at most;**
  - read logs with `--get`, never by polling `Type`;
  - only the owner reboots the Dell.
- **Songs:** `songs/local/` content never reaches git. Hashes and numbers are
  fine.

---

## 3. Part A: drum-section tails

### A0: measure the tail and decide whether it is polyphony (no engine change yet)

1. **Counters** (host bench, and a gated-off AROS diagnostic):
   - per 64-sample block, count the **active 909 voice-samples** (the sum over
     the block of voices with `active` set, counting the flam second playhead
     separately) and the **active 808 voice-samples**;
   - follow the `vc_*` pattern Levi uses (`levi_voice_counters_reset`,
     `vc_voice_active`).
   - **Positive control:** a fixture that triggers exactly k voices must read
     exactly k × 64 per block.
2. **Host bench** `tests/unit/drum_bench.c`: ungated, on `UNGATED_ALLOW` with
   a one-line reason, modelled on `levi_bench.c`.
   - Render the 909 (then the 808) with **exactly k** voices held, k = 0…all
     voices; print ns per block, and the marginal cost per voice-sample as the
     step between rows.
   - Repeat with **each 909 voice on its own**, so a single voice that is
     expensive (long layers, flam, the cymbals' layer mix) shows by name.
   - Repeat the 909 with flam on and off.
   - Report the **median of 7** runs with min/max, and **ratios within one
     process**: this host is bimodal by ~25 %.
3. **Dell paired-counter regression,** the same method as the 2026-10-04
   Levi fit:
   - log per-block `(909 µs, 909 active voice-samples)` pairs (sampled, e.g.
     every Nth block, into a fixed ring dumped at close; nothing per-sample
     goes to the log);
   - run Zombie Nation and The Knife on `521ab7e` plus your diagnostic build;
   - fit `µs = a + b × active` and report `a`, `b` and `r`;
   - do the same for the 808.
4. **Decision rule,** written in the evidence before you look at any fix:
   - **"Polyphony":** `r` ≥ 0.95 **and** no single voice costs more than 2×
     the median voice per voice-sample on the host. Then the tail is
     polyphony. **Stop Part A:** write the negative result (§A2) and make no
     engine change.
   - **"Outlier":** otherwise, name the outlier (voice, code path, or a block
     type such as a trigger block with layer setup) and go to A1.

**Evidence:** `docs/evidence/drum-tail/2026-10-07-a0-measure.md`.
Commit the counters, the bench and the evidence as
`drum-tail A0: per-block voice counters, host bench with controls, Dell regression [drum-tail]`.

### A1 (only if A0 found an outlier): an exact fix

- Fix the named outlier **bit-exactly** (§2).
- Show before and after on the host bench (median of 7, ratio within one
  process) and on the Dell, as an A,B,B,A on Zombie Nation (909 `dstg` avg and
  max, plus `stg_total_avg`).
- **A wrong-variant mutant must be caught** by an existing or new gated test.
- Commit as `drum-tail A1: <fix> [drum-tail]`.

### A2: the record

Whichever way A0 went, state in one paragraph whether "909 spikes" was a real
defect or polyphony. Close the backlog item in
`docs/2026-09-24-improvement-todo.md` accordingly.

---

## 4. Part B: the tab-switch page cost

### B0: split the full-draw cost (measurement only)

1. **Extend the TAB probe** (`app/riapp.c` `tab_switch`) and the rsection
   diagnostics so that, for each switch, the evlog line also carries the sums
   over **full** (non-DRAWUPDATE) draws during that switch:
   - `fb` = full-draw build µs (`draw_section`'s display-list build);
   - `fr` = full-draw replay µs (replay into `d->bm`);
   - `fl` = blit µs (`BltBitMapRastPort` to the window);
   - `fn` = the number of full draws.
   - `mui` = `page_us − (fb + fr + fl)`: MUI's own page handling, layout,
     backfill and anything outside our draw. **It must not go negative;** if
     it does, the spans overlap, and the instrument is fixed before any number
     is quoted.
   - Gate the extra clock reads on `RIAPP_DIAG`, like the existing per-phase
     timing. Each `ReadEClock` is about 2.2 µs of PIT port I/O on this AROS, so
     keep the read count minimal and share samples where spans abut.
   - **Unit law** (host, the `t168` style): the four parts partition the
     measured full-draw total, using the fake-clock harness the earlier
     stage-split tests use.
2. **Run on the Dell** with `RIAPP_DIAG=1` in `ENV:` only (unset afterwards).
   - Zombie Nation playing; switch tabs by click (the map is y=165, x = 57 /
     127 / 188 / 235 / 287 for SYNTHS / DRUMS / LEVI / MIX / FX).
   - Two full cycles. Collect `RIAPP-EV.LOG` after the run with `--get`.
3. **Report a table** per tab: `us`, `page_us`, `fn`, `fb`, `fr`, `fl`, `mui`.
4. **Stop rule, written before the table is read:**
   - if `fb + fr` (the part a cache can remove) is **below 50 % of
     `page_us`** on every tab, **or below 8 ms** on every tab, then the cost is
     MUI's and the blit's. **Stop Part B** with the negative result (§B3).
   - Otherwise go to B1. (Also write down what `mui` consists of, as far as it
     can be told, but do not try to optimise prebuilt Zune code; see
     2026-10-03.)

**Evidence:** `docs/evidence/gui/tab-switch/2026-10-07-b0-split.md`.
Commit as `tab switch B0: full-draw build/replay/blit split in the TAB probe [§12.11 G9]`.

### B1: skip the re-render when the canvas would draw the same pixels

**Design constraint:** the toolkit never tells a widget what changed, and the
contents of a damage rect are undefined by contract. The skip must therefore
be decided by **what we would draw**, not by assuming nothing changed while
the page was hidden.

The recommended design follows. You may propose a better one, but it must be
exact by construction.

1. **A content key per canvas.** After building the full display list for a
   full draw, compute a 64-bit hash (FNV-1a, as in t172) over the list's
   commands, text spool, skin identity, zoom and size.
   - If the key equals the key of the last full replay into `d->bm`, **and**
     `d->bm` has not been written by anything since (no partial repaint, no
     reallocation), **skip the replay** and only blit.
   - Otherwise replay as now, then store the key.
   - The build still runs. This removes `fr`, not `fb`, and it is **exact**:
     the same command list replayed into the same bitmap yields the same
     pixels.
2. **Invalidate the key** on everything that writes or replaces `d->bm`:
   - every partial (damage-box) repaint, which leaves `d->bm` holding a mix;
     simplest is to invalidate, but you may fold the partial into the key only
     if you can prove it exact;
   - `buf_free`/reallocation and size change;
   - pen or skin changes (`MUIM_Setup`/`Cleanup`, skin switch);
   - a font change, if the text path depends on it;
   - any path where the replay bails out (the system-text or imageless-skin
     fallback draws straight to the window, so the key must not claim
     `d->bm` is current).
3. **Only if B0 showed `fb` is the larger part:** consider making the
   *build* skippable as well. That needs a cheap, exact "inputs unchanged"
   test: a hash of the section's UI state plus every live value the build
   reads (meters, LEDs, chase position, pattern step), plus skin/zoom/size.
   It is riskier, because any input the hash misses is a stale-pixel bug.
   **Do it only if B0's numbers justify it, and say so.**
   - The bay-plate cache (`rbay_plate`) is the precedent for size-keyed
     caching; it is not a precedent for state-keyed caching.
4. **Tests** (host; the replay key logic must be testable without AROS, so put
   the key computation in a pure helper next to the display-list code in
   `gui/draw/`, not in the MUI class):
   - **Law 1:** the same section state gives the same key, and any single
     command difference (one rect moved by 1 px, one text glyph, a pen)
     changes the key. Test it on every section of the panel with the host
     rasterizer.
   - **Law 2:** after a partial repaint the key is invalid. Mutant: forget the
     invalidation → FAIL.
   - **Law 3,** the pixel equivalence: for every section, replaying from the
     cached path and from scratch produce identical RGBA buffers, compared
     against a forced-full reference on the host rasterizer. `t93`-style
     hashes must be unchanged.
   - **Law 4,** a hidden-change scenario: hide a canvas, change a value its
     build reads (a meter level, an LED), show it, and assert that the key
     differs and a replay happens. Mutant: a key that ignores the text spool,
     or one live input → FAIL.
5. **Dell check:**
   - A,B,B,A tab cycles on Zombie Nation (A = `521ab7e`, B = yours), two cycles
     per cell;
   - report `page_us`, `fr` and `fn` per tab, plus xruns;
   - **one** half-scale `--ui-capture` per tab in a separate job each, to show
     the page is drawn correctly (no previous tab left on screen; the
     2026-09-30 damage-box bug is the regression to watch for).

**Commit** as `tab switch B1: skip the full replay when the display list and bitmap are unchanged [§12.11 G9]`.

### B2 (optional, only if B1's Dell result leaves `fb` as the dominant part and above 8 ms)

Propose, with measured numbers, whether a build skip (B1.3) is worth its
risk. **Do not implement it without the owner's decision.** List it under §9.

### B3: the record

- A per-tab before and after table on the Dell (or the B0 negative result),
  plus what remains and who owns it (`mui`).
- Close or update the backlog item in `docs/2026-09-24-improvement-todo.md`.

---

## 5. Evidence and wiki

- **Part A:** `docs/evidence/drum-tail/2026-10-07-*.md`.
- **Part B:** `docs/evidence/gui/tab-switch/2026-10-07-*.md`.
- Every number is verbatim tool output, or derived with its components shown.
- Do **not** ingest into the llm-wiki yourself; the advisor does it after
  review.

---

## 6. Traps (each has cost this project before)

- **A bench row without a positive control can measure nothing and look
  confident** (the 2026-10-04 Levi operator sweep). Every swept quantity
  needs a counter proving it changed.
- **`dstg` values are cumulative averages,** and Dell absolute figures drift
  between sessions (e.g. the same Levi cost read 168 µs in one session and
  180 µs in another). Compare only within one A,B,B,A run.
- **Peak heartbeat `load` is too noisy for A/B;** use `stg_total_avg` and
  the stage averages.
- **The `-O2` tab cost was once attributed to the damage-box build, wrongly**
  (2026-10-03 correction). Measure the split before choosing the cut.
- **Damage boxes queued while a canvas is hidden must not turn a page switch
  into a partial repaint** (the 2026-09-30 bug). Any cache must keep that
  invariant.
- **`ri_build_host.sh test NAME` does not rebuild objects.** Run `all` first.
- **`pkill -f` with a pattern that matches your own shell kills the caller.**

---

## 7. Success gates

| Gate | PASS condition | Evidence |
|---|---|---|
| GA0 | Voice-sample counters with a positive control (k voices → k × 64 per block); host bench median of 7 with per-voice and flam rows; Dell `(µs, active)` fit with `a`, `b`, `r` for 909 and 808; decision rule written before reading the results | A0 evidence |
| GA1 (if reached) | Bit-exact fix: t172/808/909 pins and the three song hashes unchanged; host and Dell A,B,B,A improvement on the named outlier; wrong-variant mutant caught | A1 evidence, commit |
| GA2 | Backlog item closed with the polyphony/outlier verdict | todo diff |
| GB0 | TAB probe carries `fn`/`fb`/`fr`/`fl`/`mui`; partition law on host; `mui` never negative; Dell per-tab table; stop rule written first and applied | B0 evidence |
| GB1 (if reached) | Replay skip keyed on the display-list hash plus bitmap validity; Laws 1–4 with mutants; GUI goldens unchanged; Dell A,B,B,A `page_us` lower on every tab with xruns 0; one capture per tab shows correct pages | B1 evidence, commit |
| GB3 | Backlog item closed or updated with the remaining owner of the cost | todo diff |
| G-all | `AUDIT 0/0 PASS` at every commit; own files only; nothing pushed; nothing on the stick; `RIAPP_DIAG` unset and `RAM:` cleaned on the Dell; no `songs/local` content in git | `git log --stat`, `git status`, a `dir RAM:` and `getenv` line |

---

## 8. Reporting and handoff

After each phase, report in at most 12 lines: the gates passed with a number
each, the commit hash, surprises, and anything unproven.

If you stop mid-way:

```
HANDOFF drum-tail+tab
  last green phase : A? / B?
  last commit      : <hash>
  next step        : <one line>
  open questions   : <list>
  lanes touched    : <Dell?> and state left in
```

## 9. Owner decisions (list them in the final report; do not decide)

1. Whether to deploy the result to the stick (the advisor reviews first).
2. B2: whether a build skip (state-keyed) is worth its stale-pixel risk, with
   its measured saving.
3. If A0 finds polyphony: whether any polyphony-dependent saving (e.g. a
   cheaper 909 resampler) is wanted at all. That would change the sound, so it
   needs a deliberate, measured, owner-approved trade, as O1/O2 did for Levi.
