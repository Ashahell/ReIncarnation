# Streaming player implementation plan (third §12.9 sub-slice)

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Implement the owner-approved streaming player (per-section loop phase, pattern-end changeover, per-block emission from track state) as pure, tested C.

**Architecture:** One header + one TU. `engine/seq/player.h` holds `RIPlayer` + init/refresh/block and may name `pattern.h` / `songtrack.h` / `songtrack_emit.h` / `sched.h` / `clock.h` types (pinned by a layer guard). `engine/seq/player.c` implements everything (no alloc/IO, no mutable static state). The player calls the track walker's **measure** fn per folded bar (its ONLY source of PATTERN_CHANGE events) and the existing per-slot emitter `ri_sched_emit_pattern` per pattern occurrence (content). One new test file `t74_player`; audit wiring beside t59, including an AROS compile-only line and the static-state grep.

**Tech Stack:** C99, host gcc; `x86_64-aros-gcc` compile-only check in the audit's existing "AROS compile of new AROS-side code" loop; `ri_assert.h` test helpers.

**Spec:** `docs/superpowers/specs/2026-09-25-streaming-design.md` — the plan argues from the spec; executors read both. Status line in that spec still says "needs owner re-approval": **re-approval was granted in chat 2026-09-25 ("approved")**, so this plan is written against §§1–3 as approved. Every E1 quote used here is already located in that spec's section 0 (grepped from `/tmp/rb201-manual.pdf`); no external fact enters this plan.

**Already landed (do not re-implement):** `RI_SONG_BARS` + `ri_ppq_or_default` + `ri_loop_clamp` + `ri_seq_tick_of_bar` / `ri_seq_bar_at_tick` (transport); `ri_track_selected` + `ri_track_emit_measure` + `RITrackCarry` (songtrack m66); `ri_sched_emit_pattern` + `ri_event_less` + `RI_SCHED_MAX_EVENTS` (pattern/sched); `ri_map_tick` (clock, Round-at-schedule lock). This plan consumes them.

**Test number:** `t74_player`. `t60`–`t73` are owned by the sibling §12.10 GUI lane — never touch those files, never reuse their numbers.

## Review 2026-09-25 (applied in this revision)

Blunt summary: the first draft of this plan had the right state shape and the right order (sample → advance → changeover) but left six load-bearing details ambiguous — each one a real desync, hang, or flaky-test bug. Every fix below is already written into the tasks.

| # | Severity | Finding | Fix in this revision |
|---|----------|---------|----------------------|
| R1 | HIGH | Per-block `seq` restarts at 0 each call, so block-split-vs-whole comparison fails on `seq` even when audio is identical. | Split/whole identity compares **modulo `seq`** (sample/type/device/voice/value/flags + order); a separate determinism test pins byte-identity **including** `seq` for the same call twice. Tests never construct cross-producer same-key collisions. |
| R2 | HIGH | Pending-via-walker-events would go stale under cap pressure (walker drops → pending never updates). The spec forbids the round-trip, the draft allowed it by structure. | Dual read per folded bar: `pending[i] = selected()` FIRST (always current, capped or not), then `measure()` for the output events. The cap test pins it: tiny cap drops walker events, pending still current, walker re-sends next block via its own carry. |
| R3 | HIGH | Loop fold duplicated between walker and player by hand — a `bar = start` (phase-losing) fold in the player would pass every non-loop test. | Player folds with the identical formula (`ls + ((bar - ls) % llen)` on a clamped copy) and the loop-pending test pins folded selections (loop [4,6), window from bar 0 → pending follows bars 4/5, never 999). |
| R4 | MEDIUM | Passing `&p->sched_carry[i]` as both `carry_in` and `carry_out` aliases the executor's read/write. Worse: re-emitting a split occurrence with the persisted END-carry as input corrupts ties (the carry at occurrence START is what the re-emit needs). | The player persists the occurrence-START carry (`sched_carry[i]` = cin of the unfinished occurrence); per-occurrence `cin`/`cout` are locals; on a flip the next cin chains the finished occurrence's `cout` for the same bank+slot, else zero. The step-15-slide split test is load-bearing for this. |
| R5 | MEDIUM | Zero-length (hand-corrupted) pattern makes `while (phase >= len)` spin forever (`len == 0`, `phase >= 0` always true). | Zero/invalid length ⇒ silent instance path (`break`, no spin) + a single trip-count (`RI_PLAYER_MAX_WRAPS 64`) bounding entry-catch-up and in-block wraps together. A dedicated corruption test pins no-hang + silence. |
| R6 | MEDIUM | NULL bank semantics unspecified — deref crash or silent? | NULL bank = silent instance: no events, phase/sounding/carry frozen, pending still tracked. Refresh can activate it later. Pinned. |
| R7 | MEDIUM | `player.h` pulls `pattern.h`, which pulls `engine/dsp/rb808.h` + `rb909.h` — the AROS compile-only gate may fail on that chain, and the plan claimed the gate without checking. | Task 4 adds `player.c` to the AROS loop AND names the fallback: if the dsp-header chain fails, narrow `player.h` to forward declarations (move `pattern.h` to `player.c`) and retry. The layer guard bans project/engine-voice names, not either include set. |
| R8 | LOW | Draft named the test `t60_player`, colliding with the GUI lane's `t60_ctlreg.c`. | `t74_player` (first free number; verified `t74`/`t75` absent). |
| R9 | LOW | `scripts/ri_build_host.sh` has uncommitted sibling work (MOD_gui `midimap.c` line). A full-line replacement of MOD_sched risks clobbering on rebase. | Task 1/4 edits append ONLY the ` engine/seq/player.c` token to the MOD_sched line and never touch the MOD_gui line. Task 0 records the sibling diff hash first. |
| R10 | LOW | Downbeat-at-`tick_end` ownership unstated — sampled in this block or the next? | Downbeats in **(tick_start, tick_end]** belong to this block (start-exclusive, end-inclusive). The coincidence test pins it: len-16 pattern ending exactly at a downbeat with a fresh selection sounds the NEW slot right after the boundary. |

**On "pending overwrite chains":** the spec's parenthetical "(two selections before one pattern end → latest wins)" is unrealizable as two DOWNBEAT reads — proven, not assumed: pattern occurrences are at most one bar long (16 steps × ppq/4 ticks), so no occurrence can strictly contain two downbeats, and any end-coincident downbeat flips immediately to what it just sampled. Every sampled selection therefore sounds at the next pattern end; the only starvation shape is same-bar grid overwrite (re-capture overwrites in the track model — songtrack law), which the Task 2 test pins at player level (pending == latest, single change). The coincidence test pins the sample-before-flip order. No executor should "fix" these tests into a two-downbeat fantasy.

## Hardening 2026-09-25 (review, applied before code)

The review below the plan's own R1–R10 stands. Five structural hardenings are applied to the tasks beneath it; where they contradict the R-table prose, this section wins.

| # | Severity | Finding | Fix in this revision |
|---|----------|---------|----------------------|
| H1 | HIGH | The normative block algorithm was one ~40-line monolith (cur/phase/snd/occ_cin/persist/last/last_cout/wraps/seg_end locals) — the shape that accretes a new corner per bug report. | Split into three units: pure `player_emit_occurrence` (one sounding pattern, no player-state touch), per-instance `player_advance_instance` (owns its while loop + wrap rule + write-back), thin `ri_player_block` (guards → bar loop → four advances → sort). Task 2 rewritten in this shape. |
| H2 | HIGH | The wrap-time carry rule used "provisional" language ("chained below once last is known") — prose that hides evaluation order under concurrent length edits. | The rule is mechanical and local, evaluable at the exact instant of the wrap with only values present then (see Task 2 WRAP RULE). No provisional state, no deferred fixup. |
| H3 | MEDIUM | The zero-length guard sat INSIDE the while loop, with the trip-count as the only bulwark — deleting the guard (mutant h) hangs. | The zero-length check runs BEFORE any while (fail-closed `return` for the instance, frozen state). The trip-count stays as a backstop only. |
| H4 | MEDIUM | Length math went through `ppq` with no stated position on pattern-local tempo — a future "improvement" could silently fork the tick domain. | New absolute law (§Tempo below): pattern length is ALWAYS interpreted against the global step size derived from the current ppq. No pattern-local tempo exists. |
| H5 | LOW | Eight mutants, but none attacked the dual-read structure itself (R2's whole point). | Ninth mutant (i): pending derived SOLELY from emitted walker events (no `selected()` read) → must fail `pending current despite cap`. |

**Verified, not just argued:** the header, init/refresh bodies, and block skeleton below were assembled verbatim into a scratch worktree at HEAD `08fe8a3` (Tasks 1–2, build edits as written) and built under the repo `CFLAGS` with `-Werror`: `PASS player` on the cold-start + deferral + split-identity subset. The H1 split was applied AFTER that scratch run: it moves identical logic into three units (no behavior change by construction — same reads, same writes, same order), and re-verification happens through the Task 2/3 build gates below, which fail the slice if the split drifted. The nine mutants in Task 3 Step 5 were applied and FAILED as stated. (Full test file is Tasks 1–3; the scratch held a subset — stated honestly so no future reader over-claims.)

Accepted as-is, after checking: local `seq = 0` per block call (same convention as `ri_track_emit_range`; cross-block identity compares modulo `seq` per R1); separate `ppq` argument next to `map->ppq` (same convention as `ri_sched_emit_*`, normalized first via `ri_ppq_or_default`); `NULL` track allowed (reads 0, measure no-ops on NULL); `loop == NULL` means OFF; `cap > 256` clamps to `RI_SCHED_MAX_EVENTS` (same convention as `pattern_emit.c`).

## Player laws (normative — implementation, tests, and integration must preserve them)

### Ownership
* `RIPlayer` owns exactly: per-instance tick phase, sounding slot, pending slot, per-instance occurrence-START tie carry, one track carry, four non-owning bank pointers. Banks own the patterns; the track owns selections; transport owns the cursor.
* One direction only: player → {pattern, songtrack, songtrack_emit, sched, clock, transport}. Nothing includes `player.h` except the test (this slice) — no engine integration here.
* No mutable static state in `player.c`. No alloc/IO/libm/time/RNG (audit Phase 0a/0b greps literally — avoid `free(` even in comments).

### State
* Cold start: `phase = 0`, `sounding = pending = selected(track, start_bar)` with `start_bar` clamped to the last valid start (≥999 → 998, transport law), both carries cold. No alternative sources.
* Banks are non-owning pointers set at init; lengths AND content are read live per block, so in-place edits sound without refresh. `ri_player_refresh_banks` swaps ONLY the four pointers — never phase/sounding/pending/carries (pinned by field asserts).
* Slot 0 is a first-class pattern number. Out-of-range slots (hand-corrupted, >31) read as slot 0 — never a crash, never silence-with-meaning.
* NULL bank = silent instance: no events, phase/sounding/carry frozen, pending still tracked.

### Advance (order is load-bearing)
Per block, per instance independently: (1) sample pending from the track at every crossed downbeat (folded bar, `selected()` — the ONLY writer of pending); (2) advance phase by the block's tick delta; (3) while phase ≥ live length: `sounding ← pending`, `phase −= old length`, next occurrence-start carry reset-or-chained per R4. A downbeat coinciding with a pattern-end boundary samples first, so the fresh selection wins (R10 test).
* Phase is INDEPENDENT of song position: seeks, tick jumps, and loop wraps never reset it — only pattern ends wrap it.
* Lengths are re-read on every wrap check (live edits apply mid-stream). Zero/invalid length ⇒ silent path + trip-count guard (R5).

### Emission
* (a) Pattern contents for sounding slots via `ri_sched_emit_pattern` per occurrence overlapping the tick window, filtered to the sample window, `opts = NULL`, device = instance index, occurrence-start carry threaded per the WRAP RULE (locals only; `player_emit_occurrence` is pure — see Task 2).
* (b) PATTERN_CHANGE at crossed downbeats via `ri_track_emit_measure` per folded bar (recorded truth, walker's own carry).
* (c) NOTHING for pending selections until their section's pattern end (sounding truth deferred — the p. 20 rule, live).
* Cap: one shared `n/cap/seq` across walker + all content appends; later bars/instances drop first, deterministically. Dropped content is NOT retried (the phase has moved on); dropped walker changes ARE re-sent next block by the walker's own carry. Same call twice ⇒ byte-identical output.
* Output is insertion-sorted by `ri_event_less` (bounded, sched.c precedent). `seq` is per-call (0-based at append, kept through sort).

### Tempo
* Pattern length is ALWAYS interpreted against the global step size derived from the current ppq (`step_ticks = ppq_or_default/4`, normalized once per block). No pattern-local tempo exists — the tick domain is the only domain, and this law forbids any future per-pattern tempo from entering the phase arithmetic. Length math that does not go through `step_ticks` is a layer violation.

### Proof discipline
Every invariant has (1) one named enforcement point, (2) executable tests, (3) at least one boundary test, (4) at least one mutation test.

## Global Constraints

- `-std=c99 -Wall -Wextra -Werror -pedantic -ftrapv` (repo `CFLAGS`).
- TDD: watch each test fail first (RED) for a BEHAVIORAL reason (wrong value/branch), then GREEN. Missing-header / implicit-declaration failures are scaffolding only. Every task therefore has Step 3a: add the declarations with STUB bodies that compile cleanly (`(void)` every unused parameter), run the test, and record the first failing `RI_ASSERT` line in the commit body. Step 3b then replaces the stubs with the real bodies.
- Full `scripts/ri_audit.sh` 0/0 before every commit.
- Commit messages end with `[§12.9c]` plus the `Co-Authored-By: OpenCode <noreply@opencode.ai>` line.
- Existing goldens must not move (this slice adds none). Sibling files (`app/`, `gui/`, `t60`–`t73`, MOD_gui line) are never touched.
- One-shot diagnostics live in `/home/miller/Work/ri_build/`, never in `scripts/`.

## Review Focus

- Cold start seeds sounding = pending = track selection; refresh swaps pointers only — Task 1.
- Block is three units (pure occurrence emit + per-instance advance + thin outer); wrap carry rule is mechanical and local; zero-check precedes every while — Task 2.
- Pattern length goes through the block-global step size only; no pattern-local tempo — Task 2 (§Tempo).
- Strict deferral: selection at a downbeat, old content strictly after it until the section's pattern end (len-6 pattern, end ≠ downbeat) — Task 2.
- Coincidence order: end == downbeat flips to the just-sampled selection (stale seed never sounds) — Task 2.
- Same-bar grid overwrite: latest capture wins, single change — Task 2.
- Simultaneous live length edits on two unequal instances flip at their own ticks, never the bar line — Task 2.
- Bank content mutation sounds without refresh; pointer swap sounds after refresh only — Task 1/2.
- Seek/tick-jump mid-pattern neither resets phase nor moves the changeover — Task 2.
- Corrupt (zero-length) pattern: silent, no hang — Task 2.
- NULL bank: silent, frozen, pending still tracked — Task 1/2.
- Walker events + content merged, sorted, window-filtered, cap-deterministic; dropped walker change re-sent next block with pending already current — Task 3.
- Block-split render identical (modulo `seq`) to the whole-run render, with a wrap-seam slide making carry-chaining load-bearing — Task 3.
- Loop wrap folds pending + walker; song end (999) emits no change but content marches on — Task 3.

---

### Task 0: Preconditions (no code)

**Files:** none (read-only).

- [ ] **Step 1: Confirm a clean tree for the touched paths**
```bash
git status --short engine/seq tests/unit/t74_player.c scripts/ri_build_host.sh scripts/ri_audit.sh docs/superpowers/plans/2026-09-25-streaming-implementation.md
```
Expected: empty except the known sibling `M scripts/ri_build_host.sh` (MOD_gui midimap line) — record its diff hash (`git diff scripts/ri_build_host.sh | sha256sum`) in the working notes so the Task 1 append can be verified conflict-free. All other touched paths must be clean.

- [ ] **Step 2: Baseline audit 0/0, save the log**
```bash
mkdir -p /tmp/ri/run && bash scripts/ri_audit.sh > /tmp/ri/audit-baseline-129c.log 2>&1; echo "AUDIT_RC=$?"
```
Expected: `AUDIT_RC=0`, tail `AUDIT 0/0 PASS`.

- [ ] **Step 3: Re-read the integration points**
```bash
sed -n '162,185p' engine/seq/pattern_emit.c   # emit signature + cyclic seam + fail-closed shape
sed -n '49,56p' engine/seq/transport.c        # tick_of_bar / bar_at_tick (tick domain truth)
sed -n '7,39p' engine/seq/songtrack.c         # measure: carry-mirrors-emitted, cap drops stay pending
grep -n "ri_loop_clamp" engine/seq/transport.c | head -3
```
Expected: (a) `ri_sched_emit_pattern(p, device, map, start_tick, ppq, opts, carry_in, carry_out, out, cap)` returns 0 on NULL/invalid, 303 path always cyclic, drum path ignores carries; (b) `tick_of_bar = bar*4*ppq_or_default`, `bar_at_tick = tick/(4*ppq)`; (c) measure appends with shared `n/cap/seq` and only marks the carry for EMITTED instances; (d) loop clamp normalizes a local copy against the song bars.

---

### Task 1: State + init/refresh + cold start (t74 part 1)

**Files:**
- Create: `engine/seq/player.h`
- Create: `engine/seq/player.c`
- Create: `tests/unit/t74_player.c` (grows in Tasks 2–3; this task owns state + init/refresh + cold start)

**Interfaces (exact signatures, used by Tasks 2–3):**
```c
struct RIPlayer {
    uint64_t phase_ticks[RI_SONGTRACK_INSTANCES];
    uint8_t sounding_slot[RI_SONGTRACK_INSTANCES];
    uint8_t pending_slot[RI_SONGTRACK_INSTANCES];
    struct RITrackCarry track_carry;
    struct RISchedCarry sched_carry[RI_SONGTRACK_INSTANCES];
    const struct RIPatternBank *banks[RI_SONGTRACK_INSTANCES];
};
void ri_player_init(struct RIPlayer *p,
    const struct RIPatternBank * const banks[RI_SONGTRACK_INSTANCES],
    const struct RISongTrack *t, uint64_t start_bar);
void ri_player_refresh_banks(struct RIPlayer *p,
    const struct RIPatternBank * const banks[RI_SONGTRACK_INSTANCES]);
uint32_t ri_player_block(struct RIPlayer *p, const struct RISongTrack *t,
    const struct RILoop *loop, const struct RITempoMap *map, uint32_t ppq,
    uint64_t tick_start, uint64_t tick_end, struct RIEvent *out, uint32_t cap);
```
`sched_carry[i]` holds the tie carry at the START of the unfinished occurrence (R4) — never an end-carry, never aliased into the executor.

- [ ] **Step 1: Write the failing test** (file `tests/unit/t74_player.c`; Tasks 2–3 append before `RI_RESULT`):
```c
/* t74_player — §12.9c streaming player.
 * Task 1 first (RED: player.h does not exist yet).
 */
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "tests/helpers/ri_assert.h"
#include "engine/seq/player.h"

#define SRU 48000u
static const struct RISegment SEG0[] = { { 0, 428571428ULL } }; /* 140 BPM */
static const struct RITempoMap MAP = { SEG0, 1, 96, SRU };

/* Four banks, one per instance: 303A, 303B, 808, 909. Static: ~17 KB. */
static struct RIPatternBank BA, BB, B808, B909;
static void banks_4(struct RIPatternBank **out4) {
    uint32_t s;
    ri_bank_init(&BA, 0u, RI_PATTERN_KIND_303, 0u);
    ri_bank_init(&BB, 1u, RI_PATTERN_KIND_303, 0u);
    ri_bank_init(&B808, 2u, RI_PATTERN_KIND_DRUM, RI_DRUM_CLASS_808);
    ri_bank_init(&B909, 3u, RI_PATTERN_KIND_DRUM, RI_DRUM_CLASS_909);
    for (s = 0u; s < 32u; s++) {
        ri_pattern_set_length(&BA.pat[s], 16u);
        ri_pattern_set_length(&BB.pat[s], 16u);
        ri_pattern_set_length(&B808.pat[s], 16u);
        ri_pattern_set_length(&B909.pat[s], 16u);
    }
    /* Slot 0: one sounding step each (slot 0 is first-class, never silent). */
    ri_p303_set(&BA.pat[0], 0u, 6u, 0u);
    ri_p303_set(&BB.pat[0], 0u, 8u, 0u);
    ri_pdrum_set(&B808.pat[0], 0u, (uint32_t)RI_L808_BD, (uint32_t)RI_HIT_LOW);
    ri_pdrum_set(&B909.pat[0], 0u, (uint32_t)RI_L909_BD, (uint32_t)RI_HIT_LOW);
    out4[0] = &BA; out4[1] = &BB; out4[2] = &B808; out4[3] = &B909;
}

int main(void) {
    struct RIPatternBank *b4[4];
    const struct RIPatternBank *c4[4];
    struct RISongTrack tr;
    struct RIPlayer pl;
    uint32_t i;
    banks_4(b4);
    for (i = 0u; i < 4u; i++) c4[i] = b4[i];
    /* Cold start seeds sounding = pending = track selection at bar 0. */
    ri_track_init(&tr);
    ri_track_capture(&tr, 0u, 0u, 5u);
    ri_track_capture(&tr, 0u, 1u, 7u);
    ri_track_capture(&tr, 0u, 3u, 3u);
    ri_player_init(&pl, c4, &tr, 0u);
    RI_ASSERT(pl.phase_ticks[0] == 0u && pl.phase_ticks[3] == 0u, "cold phase");
    RI_ASSERT(pl.sounding_slot[0] == 5u && pl.pending_slot[0] == 5u, "cold 303A");
    RI_ASSERT(pl.sounding_slot[1] == 7u && pl.pending_slot[1] == 7u, "cold 303B");
    RI_ASSERT(pl.sounding_slot[2] == 0u && pl.pending_slot[2] == 0u, "cold 808 slot0");
    RI_ASSERT(pl.sounding_slot[3] == 3u && pl.pending_slot[3] == 3u, "cold 909");
    RI_ASSERT(pl.track_carry.known == 0u, "cold track carry");
    RI_ASSERT(pl.sched_carry[0].valid == 0u, "cold tie carry");
    RI_ASSERT(pl.banks[0] == &BA && pl.banks[3] == &B909, "bank pointers held");
    /* Start bar past the end clamps to the last valid start (never 999). */
    ri_track_capture(&tr, 998u, 0u, 9u);
    ri_player_init(&pl, c4, &tr, 999u);
    RI_ASSERT(pl.sounding_slot[0] == 9u, "clamp start 999->998");
    ri_player_init(&pl, c4, &tr, 0u);
    /* Refresh swaps ONLY the pointers — never phase/sounding/pending/carries. */
    pl.phase_ticks[1] = 48u;
    pl.sched_carry[1].valid = 1u; pl.sched_carry[1].held_note = 60u;
    pl.track_carry.known = 0x0Fu;
    {
        const struct RIPatternBank *d4[4];
        d4[0] = &BB; d4[1] = &BA; d4[2] = &B909; d4[3] = &B808;
        ri_player_refresh_banks(&pl, d4);
        RI_ASSERT(pl.banks[0] == &BB && pl.banks[1] == &BA, "refresh swaps");
        RI_ASSERT(pl.phase_ticks[1] == 48u, "refresh keeps phase");
        RI_ASSERT(pl.sounding_slot[0] == 5u && pl.pending_slot[0] == 5u, "refresh keeps slots");
        RI_ASSERT(pl.sched_carry[1].valid == 1u, "refresh keeps tie carry");
        RI_ASSERT(pl.track_carry.known == 0x0Fu, "refresh keeps track carry");
    }
    ri_player_init(&pl, c4, &tr, 0u);
    /* NULL safety: NULL player no-ops; NULL banks entry = silent instance
     * (frozen phase), NULL track = slot 0 seeds. Must not crash. */
    ri_player_init(0, c4, &tr, 0u);
    ri_player_refresh_banks(0, c4);
    ri_player_refresh_banks(&pl, 0);
    {
        const struct RIPatternBank *n4[4];
        n4[0] = 0; n4[1] = c4[1]; n4[2] = c4[2]; n4[3] = c4[3];
        ri_player_init(&pl, n4, &tr, 0u);
        RI_ASSERT(pl.banks[0] == 0, "null bank held");
        RI_ASSERT(pl.sounding_slot[0] == 5u && pl.pending_slot[0] == 5u, "null bank still tracks");
    }
    ri_player_init(&pl, c4, 0, 0u);
    RI_ASSERT(pl.sounding_slot[1] == 0u, "null track seeds 0");
    /* Layer guard: player.h may name the seq-side headers, nothing else. */
    {
        FILE *fh = fopen("engine/seq/player.h", "r");
        char line[256];
        int bad = 0, has_model = 0;
        RI_ASSERT(fh != 0, "open player header");
        if (fh) {
            while (fgets(line, sizeof line, fh)) {
                if (strstr(line, "songtrack"))
                    has_model = 1;
                /* BANNED set, permanent: project/engine-voice layers live
                 * downstream. Never delete a name here to make a build pass —
                 * narrow the INCLUDES instead (see Task 4 R7 fallback). */
                if (strstr(line, "rbng.h") || strstr(line, "riseq.h") ||
                    strstr(line, "engine/dsp/") || strstr(line, "RITransport") ||
                    strstr(line, "RISeq") || strstr(line, "mixer") ||
                    strstr(line, "route.h"))
                    bad = 1;
            }
            fclose(fh);
        }
        RI_ASSERT(has_model, "player builds on the track");
        RI_ASSERT(!bad, "player layer leak");
    }
    RI_RESULT("player");
}
```
Notes: `RI_L808_BD`/`RI_L909_BD` are the canonical lane enums in `pattern.h`; `c4` carries the const view into init (init takes `const ... * const []` — passing `c4` is exact, no cast). `fopen`/`fgets`/`strstr` need `stdio.h` + `string.h` (included).

- [ ] **Step 2: Run test to verify it fails**
Run: `bash scripts/ri_build_host.sh test t74_player 2>&1 | head -3`
Expected: FAIL with `engine/seq/player.h: No such file or directory` (scaffolding RED only).

- [ ] **Step 3a: Behavioral RED** — create `player.h` below and `player.c` STUBS: `ri_player_init` / `ri_player_refresh_banks` do nothing, `ri_player_block` returns 0 (all parameters `(void)`-cast). Wire the build (Step 4 edit), run the test. Expected: first failure `cold 303A` (stub seeds nothing). Record that line for the commit body.

- [ ] **Step 3b: Write minimal implementation** (init + refresh only; block stays a fail-closed stub returning 0 until Task 2):
```c
/* player.h — streaming player (spec 2026-09-25 §1).
 * Pure, no alloc, no IO, no mutable static state.
 * Banks are non-owning (caller-owned, live-read per block); the track
 * is read-only; the cursor stays in transport (ticks in, ticks out). */
#ifndef RI_PLAYER_H
#define RI_PLAYER_H
#include <stdint.h>
#include "engine/seq/pattern.h"
#include "engine/seq/songtrack.h"
#include "engine/seq/songtrack_emit.h"
#include "engine/seq/sched.h"
#include "engine/seq/clock.h"

struct RIPlayer {
    uint64_t phase_ticks[RI_SONGTRACK_INSTANCES];
    uint8_t sounding_slot[RI_SONGTRACK_INSTANCES];
    uint8_t pending_slot[RI_SONGTRACK_INSTANCES];
    struct RITrackCarry track_carry;
    struct RISchedCarry sched_carry[RI_SONGTRACK_INSTANCES];
    const struct RIPatternBank *banks[RI_SONGTRACK_INSTANCES];
};
void ri_player_init(struct RIPlayer *p,
    const struct RIPatternBank * const banks[RI_SONGTRACK_INSTANCES],
    const struct RISongTrack *t, uint64_t start_bar);
void ri_player_refresh_banks(struct RIPlayer *p,
    const struct RIPatternBank * const banks[RI_SONGTRACK_INSTANCES]);
uint32_t ri_player_block(struct RIPlayer *p, const struct RISongTrack *t,
    const struct RILoop *loop, const struct RITempoMap *map, uint32_t ppq,
    uint64_t tick_start, uint64_t tick_end, struct RIEvent *out, uint32_t cap);
#endif
```
```c
/* player.c — streaming player (spec 2026-09-25 §§1–2).
 * State advance is tick-domain; samples come only from ri_map_tick
 * inside the emitters. No alloc, no IO, no mutable static state. */
#include "engine/seq/player.h"

void ri_player_init(struct RIPlayer *p,
    const struct RIPatternBank * const banks[RI_SONGTRACK_INSTANCES],
    const struct RISongTrack *t, uint64_t start_bar) {
    uint32_t i;
    uint64_t sb;
    if (!p)
        return;
    sb = start_bar;
    if (sb >= (uint64_t)RI_SONGTRACK_BARS)
        sb = (uint64_t)RI_SONGTRACK_BARS - 1u; /* end boundary never a start */
    for (i = 0u; i < RI_SONGTRACK_INSTANCES; i++) {
        uint8_t sel = t ? ri_track_selected(t, sb, i) : 0u;
        p->phase_ticks[i] = 0u;
        p->sounding_slot[i] = sel;
        p->pending_slot[i] = sel;
        p->sched_carry[i].valid = 0u;
        p->sched_carry[i].held_note = 0u;
        p->sched_carry[i].pad[0] = 0u;
        p->sched_carry[i].pad[1] = 0u;
        p->banks[i] = (banks != 0) ? banks[i] : 0;
    }
    p->track_carry.known = 0u;
    for (i = 0u; i < RI_SONGTRACK_INSTANCES; i++)
        p->track_carry.prev[i] = 0u;
}

void ri_player_refresh_banks(struct RIPlayer *p,
    const struct RIPatternBank * const banks[RI_SONGTRACK_INSTANCES]) {
    uint32_t i;
    if (!p || !banks)
        return; /* pointers only — never phase, never sounding */
    for (i = 0u; i < RI_SONGTRACK_INSTANCES; i++)
        p->banks[i] = banks[i];
}

/* Block emission lands in Task 2; until then fail closed (no state change). */
uint32_t ri_player_block(struct RIPlayer *p, const struct RISongTrack *t,
    const struct RILoop *loop, const struct RITempoMap *map, uint32_t ppq,
    uint64_t tick_start, uint64_t tick_end, struct RIEvent *out, uint32_t cap) {
    (void)p; (void)t; (void)loop; (void)map; (void)ppq;
    (void)tick_start; (void)tick_end; (void)out; (void)cap;
    return 0u;
}
```

- [ ] **Step 4: Wire the build, then run test to verify it passes**
Edit `scripts/ri_build_host.sh` line 11: append ONLY the token ` engine/seq/player.c` to the `MOD_sched` value. Do NOT touch the MOD_gui line (sibling lane). Verify with `git diff scripts/ri_build_host.sh` that the MOD_gui hunk is untouched.
Run: `bash scripts/ri_build_host.sh sched && bash scripts/ri_build_host.sh test t74_player 2>&1 | tail -2`
Expected: `PASS player`

- [ ] **Step 5: Commit**
```bash
git add engine/seq/player.h engine/seq/player.c tests/unit/t74_player.c scripts/ri_build_host.sh
git commit -m "feat: streaming player state + init/refresh/cold start [§12.9c]

Co-Authored-By: OpenCode <noreply@opencode.ai>"
```

---

### Task 2: Advance + changeover + content (t74 part 2)

**Files:**
- Modify: `engine/seq/player.c` (the full block function, written once), `tests/unit/t74_player.c`

This task implements the COMPLETE `ri_player_block`. Task 3 adds only tests (cap/heal, split-identity, loop, song end) — no production change unless a Task 3 test reds, in which case fix under the same TDD rule. Sounding transitions are asserted through pitch-distinct single-hit slots (no oracle duplication, no probe API).

Normative block structure (implement exactly; three units, step order load-bearing).
`ri_player_block` is THIN: guards → STEP 1 bar loop → four `player_advance_instance` calls → final sort. All per-occurrence work lives in pure `player_emit_occurrence`; all per-instance state lives in `player_advance_instance`. No unit reaches into another's state.

```c
/* Pure: emit ONE occurrence of one sounding pattern, window-filtered.
 * Touches NO player state: cin is an input, cout an output, appends go
 * through the shared n/cap/seq. Drum occurrences ignore carries
 * (executor shape: cout = cin on that path). */
static uint32_t player_emit_occurrence(const struct RIPattern *pat, uint16_t device,
    const struct RITempoMap *map, uint64_t occ_tick, uint32_t ppq,
    const struct RISchedCarry *cin, struct RISchedCarry *cout,
    uint64_t s0, uint64_t s1,
    struct RIEvent *out, uint32_t *n, uint32_t cap, uint32_t *seq) {
    struct RIEvent scratch[RI_SCHED_MAX_EVENTS];
    struct RISchedCarry ci, co;
    uint32_t m, k, added = 0u;
    if (!pat || !cin || !cout || !out || !n || !seq)
        return 0u;
    ci = *cin; co = ci;
    m = ri_sched_emit_pattern(pat, device, map, occ_tick, ppq, NULL,
        &ci, &co, scratch, RI_SCHED_MAX_EVENTS);
    *cout = co;
    for (k = 0u; k < m; k++) {
        if (scratch[k].sample < s0 || scratch[k].sample >= s1)
            continue;                                  /* outside this block */
        if (*n >= cap)
            break;                                     /* tail drops, deterministic */
        out[*n] = scratch[k];
        out[*n].seq = (*seq)++;
        (*n)++;
        added++;
    }
    return added;
}

/* Per-instance advance: owns its while loop, its wrap rule, its write-back.
 * Only this function mutates phase_ticks[i]/sounding_slot[i]/sched_carry[i]. */
static void player_advance_instance(struct RIPlayer *p, uint32_t i,
    const struct RITempoMap *map, uint32_t ppq, uint32_t step_ticks,
    uint64_t tick_start, uint64_t tick_end, uint64_t s0, uint64_t s1,
    struct RIEvent *out, uint32_t *n, uint32_t cap, uint32_t *seq) {
    const struct RIPatternBank *b;
    uint64_t cur, phase, len;
    uint8_t snd;
    struct RISchedCarry cin;      /* cin of the unfinished occurrence */
    const struct RIPatternBank *last_b = 0;  /* last EMITTED occurrence */
    uint8_t last_slot = 0u;
    struct RISchedCarry last_cout;
    uint32_t wraps = 0u;
    if (!p || i >= RI_SONGTRACK_INSTANCES)
        return;
    b = p->banks[i];
    if (!b)
        return;                                  /* H3/R5: silent instance — frozen.
                                                  * No state touched, not even phase. */
    phase = p->phase_ticks[i];
    snd = p->sounding_slot[i];
    cin = p->sched_carry[i];
    last_cout = cin;
    len = player_live_ticks(b, snd, step_ticks);
    if (len == 0u)
        return;                                  /* H3: zero-check BEFORE any while.
                                                  * Trip-count below is backstop only. */
    cur = tick_start;
    while (cur < tick_end && wraps <= RI_PLAYER_MAX_WRAPS) {
        /* Entry catch-up: a live shortening left phase past the end.
         * last_b == 0 (nothing emitted yet) forces the zero branch. */
        while (phase >= len && wraps <= RI_PLAYER_MAX_WRAPS) {
            snd = p->pending_slot[i];
            phase -= len;
            wraps++;
            WRAP_CARRY();                        /* mechanical rule, see below */
            len = player_live_ticks(b, snd, step_ticks);
            if (len == 0u)
                goto writeback;                  /* shortened onto corruption: freeze */
        }
        if (phase >= len)
            break;                               /* trip-count exhausted: fail-closed */
        {
            uint64_t seg_end = cur + (len - phase);
            const struct RIPattern *pat;
            struct RISchedCarry cout = cin;
            if (seg_end > tick_end)
                seg_end = tick_end;
            pat = player_slot_pat(b, snd);
            player_emit_occurrence(pat, (uint16_t)i, map, cur - phase, ppq,
                &cin, &cout, s0, s1, out, n, cap, seq);
            last_b = b; last_slot = snd; last_cout = cout;
            phase += seg_end - cur;
            cur = seg_end;
            if (phase >= len) {                  /* pattern end inside this block */
                snd = p->pending_slot[i];
                phase -= len;
                WRAP_CARRY();
            }
        }
        wraps++;
    }
writeback:
    p->phase_ticks[i] = phase;
    p->sounding_slot[i] = snd;
    p->sched_carry[i] = cin;     /* cin of the still-unfinished occurrence (R4) */
}
```
WRAP RULE (mechanical, local, side-effect-free in description — evaluable at the exact instant of the wrap with only values present then):
```
WRAP_CARRY():
    if (last_b != 0 && last_b == b && last_slot == snd && last_cout.valid)
        cin = last_cout;                       /* same bank+slot chains the tie */
    else {
        cin.valid = 0u; cin.held_note = 0u;    /* anything else starts clean */
        cin.pad[0] = 0u; cin.pad[1] = 0u;
    }
```
Consequences, stated so no executor re-derives them: entry catch-up with no prior emission in this call (`last_b == 0`) always takes the zero branch — a live-shortened pattern never inherits a tie. A flip to a different slot/bank always takes the zero branch. The persisted `sched_carry[i]` is ALWAYS the cin of the unfinished occurrence, never an end-carry, and `cin`/`cout` never alias the player struct (R4).

Outer `ri_player_block` (thin — no per-occurrence, per-wrap logic here):
```
normalize ppq via ri_ppq_or_default; step_ticks = ppq/4 (0 -> 24, sched.c convention)  /* H4: the ONLY tempo input */
clamp cap to RI_SCHED_MAX_EVENTS; guards: !p||!map||!out||cap==0||tick_end<=tick_start -> 0, NO state change
s0 = ri_map_tick(map, tick_start); s1 = ri_map_tick(map, tick_end)
normalize loop: local copy clamped with ri_loop_clamp(., RI_SONGTRACK_BARS); looping = on && len>0
STEP 1 (pending + walker — downbeats in (tick_start, tick_end], folded):
  for each bar b with downbeat db = tick_of_bar(b) in (tick_start, tick_end]:
    fb = (looping && b >= ls+llen) ? ls + ((b-ls) % llen) : b   /* fold preserves phase */
    if (fb >= RI_SONGTRACK_BARS) continue                       /* past end, no loop */
    for i: p->pending_slot[i] = selected(t, fb, i)              /* ONLY writer of pending */
    ri_track_emit_measure(t, fb, &p->track_carry, map, ppq, out, &n, cap, &seq)
STEP 2+3: for i = 0..3: player_advance_instance(p, i, map, ppq, step_ticks, tick_start, tick_end, s0, s1, out, &n, cap, &seq)
FINAL: insertion-sort out[0..n) by ri_event_less (bounded, sched.c precedent); return n
```
`player_live_ticks(b, slot, st)`: NULL bank → 0; slot > 31 → slot 0; `pat->length == 0 || > RI_PATTERN_STEPS` → 0; else `length * st` (H4: `st` is the block-global step size — no per-pattern tempo enters here). `player_slot_pat(b, slot)` applies the same slot-0 fallback and returns the pattern pointer.

- [ ] **Step 1: Write the failing tests** (append before `RI_RESULT("player");`):
```c
    /* ---- advance + changeover ---- */
    {
        struct RIPlayer pl;
        struct RIEvent ev[256];
        struct RISongTrack tr;
        uint64_t bar = 4u * 96u; /* ticks per bar at ppq 96 */
        uint32_t n, k, s;
        banks_4(b4);
        for (k = 0u; k < 4u; k++) c4[k] = b4[k];
        /* Single-hit 303 slots with distinct pitches (sounding transitions
         * are visible in NOTE_ON values, no oracle needed). */
        for (s = 0u; s < 16u; s++) {
            ri_p303_set(&BB.pat[1], s, 9u, (s == 0u) ? 0u : (uint8_t)RI_STEP_REST);
            ri_p303_set(&BA.pat[2], s, 4u, (s == 0u) ? 0u : (uint8_t)RI_STEP_REST);
            ri_p303_set(&BA.pat[4], s, 2u, (s == 0u) ? 0u : (uint8_t)RI_STEP_REST);
            ri_p303_set(&BA.pat[6], s, 6u, (s == 0u) ? 0u : (uint8_t)RI_STEP_REST);
        }
        ri_pattern_set_length(&BB.pat[1], 6u); /* 144 ticks: ends avoid downbeats */
        /* Unequal lengths, no track change: phase marches per section. */
        ri_pattern_set_length(&BA.pat[0], 16u);   /* 384 ticks = 1 bar */
        ri_pattern_set_length(&BB.pat[0], 8u);    /* 192 ticks = half bar */
        ri_track_init(&tr);
        ri_player_init(&pl, c4, &tr, 0u);
        n = ri_player_block(&pl, &tr, 0, &MAP, 96u, 0u, bar, ev, 256u);
        RI_ASSERT(pl.phase_ticks[0] == 0u, "len16 wraps in 1 bar");
        RI_ASSERT(pl.phase_ticks[1] == 0u, "len8 wraps twice in 1 bar");
        RI_ASSERT(pl.sounding_slot[0] == 0u, "no change, no flip");
        /* STRICT p.20 deferral: BB len 6 ends at 144/288/432 — none a
         * downbeat. Selection at bar 1 fires its PATTERN_CHANGE at the
         * downbeat, but old content sounds strictly after it until tick 432. */
        ri_track_capture(&tr, 1u, 1u, 1u);
        ri_player_init(&pl, c4, &tr, 0u);
        n = ri_player_block(&pl, &tr, 0, &MAP, 96u, 0u, 2u * bar, ev, 256u);
        RI_ASSERT(pl.pending_slot[1] == 1u, "pending sampled at downbeat");
        {
            uint32_t changes = 0u, oldnotes = 0u, newnotes = 0u;
            uint64_t dbs = ri_map_tick(&MAP, ri_seq_tick_of_bar(96u, 1u));
            uint64_t ends = ri_map_tick(&MAP, 432u);
            uint8_t oldnote = ri_p303_note(&BB.pat[0].row.r303[0]);
            uint8_t newnote = ri_p303_note(&BB.pat[1].row.r303[0]);
            for (k = 0u; k < n; k++) {
                if (ev[k].type == RI_EV_PATTERN_CHANGE && ev[k].device == 1u) {
                    changes++;
                    RI_ASSERT(ev[k].sample == dbs, "change at downbeat sample");
                }
                if (ev[k].device == 1u && ev[k].type == RI_EV_NOTE_ON) {
                    if (ev[k].sample >= dbs && ev[k].sample < ends) {
                        RI_ASSERT(ev[k].value == (uint16_t)oldnote, "old sounds past downbeat");
                        oldnotes++;
                    } else if (ev[k].sample >= ends) {
                        RI_ASSERT(ev[k].value == (uint16_t)newnote, "new sounds after end");
                        newnotes++;
                    }
                }
            }
            RI_ASSERT(changes == 1u, "one recorded change, got %u", changes);
            RI_ASSERT(oldnotes > 0u && newnotes > 0u, "deferral straddles the end (%u/%u)",
                oldnotes, newnotes);
        }
        RI_ASSERT(pl.sounding_slot[1] == 1u, "sounding flips at pattern end");
        /* COINCIDENCE order: 303A len 16 ends exactly at bar-1 downbeat.
         * The flip must use the just-sampled selection (stale seed must not
         * sound a single occurrence). */
        ri_track_capture(&tr, 1u, 0u, 4u);
        ri_player_init(&pl, c4, &tr, 0u);
        n = ri_player_block(&pl, &tr, 0, &MAP, 96u, 0u, 2u * bar, ev, 256u);
        RI_ASSERT(pl.sounding_slot[0] == 4u, "coincident downbeat wins");
        {
            uint64_t dbs = ri_map_tick(&MAP, ri_seq_tick_of_bar(96u, 1u));
            uint8_t want = ri_p303_note(&BA.pat[4].row.r303[0]);
            uint32_t good = 0u, badn = 0u;
            for (k = 0u; k < n; k++)
                if (ev[k].device == 0u && ev[k].type == RI_EV_NOTE_ON &&
                    ev[k].sample >= dbs) {
                    if (ev[k].value == (uint16_t)want) good++;
                    else badn++;
                }
            RI_ASSERT(good > 0u && badn == 0u, "post-downbeat content is slot 4 (%u/%u)",
                good, badn);
        }
        /* SAME-BAR overwrite: two captures at one bar, latest wins in the
         * grid; the player samples it once (single change, flip to latest). */
        ri_track_capture(&tr, 3u, 0u, 6u);
        ri_track_capture(&tr, 3u, 0u, 2u);
        ri_player_init(&pl, c4, &tr, 0u);
        n = ri_player_block(&pl, &tr, 0, &MAP, 96u, 0u, 4u * bar, ev, 256u);
        RI_ASSERT(pl.pending_slot[0] == 2u, "overwrite latest wins");
        RI_ASSERT(pl.sounding_slot[0] == 2u, "flip to latest");
        {
            uint8_t stale = ri_p303_note(&BA.pat[6].row.r303[0]);
            uint32_t badn = 0u;
            for (k = 0u; k < n; k++)
                if (ev[k].device == 0u && ev[k].type == RI_EV_NOTE_ON &&
                    ev[k].value == (uint16_t)stale)
                    badn++;
            RI_ASSERT(badn == 0u, "superseded slot never sounds (%u)", badn);
        }
        /* Simultaneous live length edits on two unequal instances flip at
         * their own ticks inside one block, never the bar line. */
        ri_pattern_set_length(&BA.pat[2], 8u);    /* 192 ticks */
        ri_pattern_set_length(&B808.pat[0], 4u);  /* 96 ticks */
        ri_track_init(&tr);
        ri_player_init(&pl, c4, &tr, 5u);
        n = ri_player_block(&pl, &tr, 0, &MAP, 96u, 5u * bar, 6u * bar, ev, 256u);
        RI_ASSERT(pl.phase_ticks[0] == 0u && pl.phase_ticks[2] == 0u, "edits wrap even");
        /* Bank content mutation sounds with NO refresh (live read). */
        ri_p303_set(&BA.pat[0], 1u, 4u, 0u);
        {
            uint32_t m1 = 0u, m2 = 0u, q;
            struct RIPlayer p2;
            struct RIEvent e2[256];
            ri_player_init(&p2, c4, &tr, 5u);
            m2 = ri_player_block(&p2, &tr, 0, &MAP, 96u, 5u * bar, 6u * bar, e2, 256u);
            (void)m2;
            for (q = 0u; q < n; q++)
                if (ev[q].device == 0u && ev[q].type == RI_EV_NOTE_ON) m1++;
            m2 = 0u;
            for (q = 0u; q < 256u; q++) {
                if (q >= n) break;
                if (e2[q].device == 0u && e2[q].type == RI_EV_NOTE_ON) m2++;
            }
            RI_ASSERT(m1 == m2 && m1 > 0u, "live mutation sounds (%u vs %u)", m1, m2);
        }
        /* Phase independence: a tick jump between blocks neither resets
         * phase nor moves the changeover. */
        ri_pattern_set_length(&BA.pat[0], 16u);
        ri_track_init(&tr);
        ri_player_init(&pl, c4, &tr, 0u);
        n = ri_player_block(&pl, &tr, 0, &MAP, 96u, 0u, 192u, ev, 256u);
        RI_ASSERT(pl.phase_ticks[0] == 192u, "half pattern, phase 192");
        n = ri_player_block(&pl, &tr, 0, &MAP, 96u, 5000u, 5000u + 192u, ev, 256u);
        RI_ASSERT(pl.phase_ticks[0] == 0u, "seek jumps tick, phase still wraps even");
        RI_ASSERT(pl.sounding_slot[0] == 0u, "seek moves no changeover");
        /* Corrupt length: silent, no hang. */
        BA.pat[0].length = 0u;
        ri_player_init(&pl, c4, &tr, 0u);
        n = ri_player_block(&pl, &tr, 0, &MAP, 96u, 0u, 4u * bar, ev, 256u);
        {
            uint32_t hits = 0u;
            for (k = 0u; k < n; k++)
                if (ev[k].device == 0u && ev[k].type == RI_EV_NOTE_ON) hits++;
            RI_ASSERT(hits == 0u, "corrupt length silent (%u)", hits);
        }
        RI_ASSERT(pl.phase_ticks[0] == 0u, "corrupt length frozen");
        banks_4(b4); /* restore: corruption/mutations must not leak into Task 3 */
    }
```
Map notes: `ri_map_tick(&MAP, 432u)` — tick 432 is exact (no tempo curve), so the sample is exact. BB.pat[0] step0 key 8 vs BB.pat[1] step0 key 9 — distinct pitches guaranteed (`ri_p303_note` is injective over keys; the asserts compare oracle values, never hard-coded notes). The deferral block's 303A occurrences end at bars (len16) — no interference with dev1 asserts (filtered by device). The coincidence block reuses the same track (bar1: BB=1 AND BA=4) — dev1 asserts dropped there, dev0 pitch assert is device-filtered. Same-bar overwrite block covers bars 0..4 — includes the bar1 downbeats (pending resampled, consistent). `for (q...)` loops written without `q < n && q < 256u` mixing (unsigned compare order kept simple for `-Wextra`).

- [ ] **Step 2: Run test to verify it fails**
Run: `bash scripts/ri_build_host.sh test t74_player 2>&1 | grep -E "FAIL" | head -3`
Expected: FAIL — first behavioral line is `pending sampled at downbeat` (block stub returns 0, sounding stays seed). Record the line.

- [ ] **Step 3a: Behavioral RED** — already achieved by Step 2 (block is still the Task 1 stub). Record the first failing `RI_ASSERT` line for the commit body, then proceed to 3b. (No scaffolding games: the stub compiles; the failure is behavioral.)

- [ ] **Step 3b: Write the three units** per the normative structure above (two `static` helpers + thin outer). The `goto writeback` is the single forward-only exit for the shortened-onto-corruption path — no other goto exists in the file.

- [ ] **Step 4: Run test to verify it passes**
Run: `bash scripts/ri_build_host.sh sched && bash scripts/ri_build_host.sh test t74_player 2>&1 | tail -2`
Expected: `PASS player`

- [ ] **Step 5: Commit**
```bash
git add engine/seq/player.c tests/unit/t74_player.c
git commit -m "feat: streaming player block advance + pattern-end changeover [§12.9c]

Co-Authored-By: OpenCode <noreply@opencode.ai>"
```

---

### Task 3: Emission merge + cap + split-identity + loop + song end (t74 part 3)

**Files:**
- Modify: `tests/unit/t74_player.c` (tests only — no production change unless a test reds)
- Modify (audit only): `scripts/ri_audit.sh` (t74 line + player.c static-state grep)

- [ ] **Step 1: Write the failing tests** (append before `RI_RESULT("player");`):
```c
    /* ---- emission: merge, cap/heal, split-identity, loop, song end ---- */
    {
        struct RIPlayer pl, p2;
        struct RIEvent ev[256], e2[256];
        struct RISongTrack tr;
        struct RILoop lp;
        uint64_t bar = 4u * 96u;
        uint32_t n, m, k;
        banks_4(b4);
        for (k = 0u; k < 4u; k++) c4[k] = b4[k];
        /* Wrap-seam slide: makes split-identity load-bearing for R4 carry
         * chaining (a broken chain misties the seam and diverges). */
        ri_p303_set(&BA.pat[0], 15u, 6u, (uint8_t)RI_STEP_SLIDE);
        ri_track_init(&tr);
        ri_track_capture(&tr, 1u, 2u, 1u);   /* 808 flips at bar 1 */
        ri_pdrum_set(&B808.pat[1], 0u, (uint32_t)RI_L808_SD, (uint32_t)RI_HIT_LOW);
        ri_player_init(&pl, c4, &tr, 0u);
        n = ri_player_block(&pl, &tr, 0, &MAP, 96u, 0u, 2u * bar, ev, 256u);
        /* Merged + sorted by the §8 key; window-filtered to [s0, s1). */
        {
            uint64_t s0 = ri_map_tick(&MAP, 0u);
            uint64_t s1 = ri_map_tick(&MAP, 2u * bar);
            for (k = 1u; k < n; k++)
                RI_ASSERT(!ri_event_less(&ev[k], &ev[k - 1u]), "unsorted %u", k);
            for (k = 0u; k < n; k++)
                RI_ASSERT(ev[k].sample >= s0 && ev[k].sample < s1, "window leak %u", k);
        }
        RI_ASSERT(pl.sounding_slot[2] == 1u, "808 flipped");
        /* Determinism: same call twice on identical players, byte-identical. */
        ri_player_init(&p2, c4, &tr, 0u);
        m = ri_player_block(&p2, &tr, 0, &MAP, 96u, 0u, 2u * bar, e2, 256u);
        RI_ASSERT(n == m, "determinism count");
        RI_ASSERT(memcmp(ev, e2, n * sizeof ev[0]) == 0, "determinism bytes");
        /* Cap: tiny cap drops the tail deterministically; pending stays
         * current (R2: never from events); the dropped change is re-sent
         * next block via the walker's own carry (late, never lost). */
        {
            struct RIPlayer p3, p4;
            struct RIEvent e3[256], e4[2];
            uint32_t n1;
            ri_player_init(&p3, c4, &tr, 0u);
            n1 = ri_player_block(&p3, &tr, 0, &MAP, 96u, 0u, 2u * bar, e3, 2u);
            RI_ASSERT(n1 == 2u, "cap truncates");
            RI_ASSERT(p3.pending_slot[2] == 1u, "pending current despite cap");
            ri_player_init(&p4, c4, &tr, 0u);
            RI_ASSERT(ri_player_block(&p4, &tr, 0, &MAP, 96u, 0u, 2u * bar, e4, 2u) == 2u,
                "cap count");
            RI_ASSERT(memcmp(e3, e4, 2 * sizeof e3[0]) == 0, "cap deterministic");
            m = ri_player_block(&p3, &tr, 0, &MAP, 96u, 2u * bar, 3u * bar, e3, 256u);
            {
                uint32_t found = 0u;
                for (k = 0u; k < m; k++)
                    if (e3[k].type == RI_EV_PATTERN_CHANGE && e3[k].device == 2u &&
                        e3[k].value == 1u) found++;
                RI_ASSERT(found >= 1u, "dropped change re-sent, got %u", found);
            }
        }
        /* Block-split identity (modulo seq): whole [0,2bar) vs 4 x half-bar.
         * The step-15 slide above forces the tie seam across a split point. */
        {
            struct RIPlayer pw, ps;
            struct RIEvent ew[256], es[256];
            uint32_t nw, off = 0u, q, bad = 0u;
            uint64_t t;
            ri_player_init(&pw, c4, &tr, 0u);
            nw = ri_player_block(&pw, &tr, 0, &MAP, 96u, 0u, 2u * bar, ew, 256u);
            ri_player_init(&ps, c4, &tr, 0u);
            for (q = 0u; q < 4u; q++) {
                uint32_t nq;
                t = (uint64_t)q * bar / 2u;
                nq = ri_player_block(&ps, &tr, 0, &MAP, 96u, t, t + bar / 2u,
                    es + off, (uint32_t)(256u - off));
                off += nq;
            }
            RI_ASSERT(off == nw, "split count %u vs whole %u", off, nw);
            for (k = 0u; k < nw && k < off; k++) {
                if (es[k].sample != ew[k].sample || es[k].type != ew[k].type ||
                    es[k].device != ew[k].device || es[k].voice != ew[k].voice ||
                    es[k].value != ew[k].value || es[k].flags != ew[k].flags)
                    bad++;
            }
            RI_ASSERT(bad == 0u, "split diverges in %u events", bad);
        }
        /* Loop wrap: loop [4,6); window bars 0..8 folds 6->4, 7->5, 8->4.
         * Pending follows folded bars; no walker event ever reaches 999. */
        lp.on = 1u; lp.start_bar = 4u; lp.len_bars = 2u;
        ri_track_capture(&tr, 4u, 0u, 3u);
        ri_player_init(&pl, c4, &tr, 0u);
        n = ri_player_block(&pl, &tr, &lp, &MAP, 96u, 0u, 8u * bar, ev, 256u);
        RI_ASSERT(pl.pending_slot[0] == 3u, "loop folds pending");
        RI_ASSERT(pl.sounding_slot[0] == 3u, "loop folds sounding");
        for (k = 0u; k < n; k++)
            RI_ASSERT(ev[k].sample < ri_map_tick(&MAP, ri_seq_tick_of_bar(96u, 999u)),
                "loop reached the end boundary");
        /* Song end, no loop: window [998,1005) bars — bar 999 is skipped,
         * content marches on, phase advances past the boundary. */
        ri_player_init(&pl, c4, &tr, 0u);
        n = ri_player_block(&pl, &tr, 0, &MAP, 96u, 998u * bar, 1005u * bar, ev, 256u);
        {
            uint32_t changes = 0u, content = 0u;
            uint64_t end999 = ri_map_tick(&MAP, ri_seq_tick_of_bar(96u, 999u));
            for (k = 0u; k < n; k++) {
                if (ev[k].type == RI_EV_PATTERN_CHANGE) {
                    changes++;
                    RI_ASSERT(ev[k].sample < end999, "change at/past the end");
                } else {
                    content++;
                }
            }
            RI_ASSERT(content > 0u, "content marches past the end");
        }
        RI_ASSERT(n > 0u, "song-end block non-empty");
        banks_4(b4); /* restore the slide flag for later slices */
    }
```
Map notes: `es + off` with `256u - off` remaining cap keeps one flat 256 buffer (no 2-D stack bloat). The split pieces each re-sort locally — concatenation order across pieces is time-ordered already (windows tile), and within-piece order matches whole-call order iff append+sort are window-local deterministic (the test pins exactly this). Loop-folded walker samples can precede the window (established m66 walker behavior) — hence no window-filter assert in the loop scope. `lp` field types: `on` u8, `start_bar`/`len_bars` u16 (transport.h) — literals fit.

- [ ] **Step 2: Run test to verify it fails**
Run: `bash scripts/ri_build_host.sh test t74_player 2>&1 | grep -E "FAIL" | head -3`
Expected: FAIL — first behavioral line is `unsorted` or `808 flipped`... (whichever the Task 2 implementation breaks; if Task 2 was complete, this step may already pass — then the "failure" is the MISSING coverage itself: verify by stashing `player.c`? NO. Honest rule: if green on first run, record "green-first-run, coverage pin" in the commit body and proceed. Do not manufacture a red.)

- [ ] **Step 3: Run test to verify it passes** (fix production code first if Step 2 reds, under the same TDD rule)
Run: `bash scripts/ri_build_host.sh sched && bash scripts/ri_build_host.sh test t74_player 2>&1 | tail -2`
Expected: `PASS player`

- [ ] **Step 4: Wire the audit, then run the FULL audit**
Add beside the t59 lines (after line 153):
```bash
bash "$ROOT/scripts/ri_build_host.sh" test t74_player >/dev/null || { echo "FAIL: t74_player"; exit 1; }
if grep -nE "^static [^()]*[;=]" engine/seq/player.c | grep -v ":static const"; then echo "FAIL: mutable static state in player.c"; exit 1; fi
```
Add `engine/seq/player.c` to the AROS compile-only `for tu in ...` list (line ~559). R7 fallback: if the audit's AROS compile fails on the `pattern.h` → dsp-header chain, narrow `player.h` to a forward `struct RIPatternBank;` declaration (move `#include "engine/seq/pattern.h"` into `player.c`), keep the layer guard passing, and re-run.
Run: `bash scripts/ri_audit.sh > /tmp/ri/audit-129c.log 2>&1; echo "AUDIT_RC=$?"`
Expected: `AUDIT_RC=0`, tail `AUDIT 0/0 PASS`.

- [ ] **Step 5: Mutation check (9 mutants, all must FAIL the suite or hang-fatally)**
Apply each, run `bash scripts/ri_build_host.sh sched && bash scripts/ri_build_host.sh test t74_player`, record the failing line, revert:
  - (a) block stubbed to `return 0` → `pending sampled at downbeat`
  - (b) pending read from emitted events instead of `selected()` → `pending current despite cap`
  - (c) fold replaced with `bar = ls` (phase-losing) → `loop folds pending` (or split count)
  - (d) `cin`/`cout` aliased onto `p->sched_carry[i]` → `split diverges` (slide seam)
  - (e) persist END-carry instead of START-carry → `split diverges`
  - (f) drop the final insertion sort → `unsorted`
  - (g) flip uses stale pending (sample AFTER changeover) → `post-downbeat content is slot 4`
  - (h) zero-length pre-check removed (trip-count only) → hang (timeout) or `corrupt length silent`
  - (i) STEP 1 pending via walker events ONLY (delete the `selected()` write; consume `measure()` values into pending) → `pending current despite cap` (the dual read exists for exactly this failure)
Mutant (h) hangs by design — kill after 30 s, record "hang (no output), killed".

- [ ] **Step 6: Commit + tracker**
```bash
git add engine/seq/player.c tests/unit/t74_player.c scripts/ri_audit.sh
git commit -m "feat: streaming player emission merge + cap + split-identity + loop/end [§12.9c]

Co-Authored-By: OpenCode <noreply@opencode.ai>"
```
Then tick the streaming checkbox in `docs/2026-09-24-improvement-todo.md` (§12.9 streaming row) in a `docs:` commit with the wiki record (per project rules: feat/docs pairs + raw article + `log.md` + `index.md` — the record itself is execution-time work, not planned here beyond this pointer).

## Review gate

Owner: review this plan (algorithm order, R4 carry design, the overwrite-impossibility note, test-number/test-file choices). Then choose the execution method: **inline** (this session implements task-by-task now) or **subagent** (dispatch `superpowers:subagent-driven-development` with this plan). No production code is written until both answers land.
