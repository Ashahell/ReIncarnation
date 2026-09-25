# Transport state machine Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Implement the owner-approved transport state machine (first §12.9 sub-slice) as pure, tested C.

**Architecture:** New `engine/seq/transport.{h,c}` holds the state machine (stdint.h only, no alloc/IO); `RISeq` gains the state structs plus a tick cursor; one new test file `t58_transport`; audit wiring in the existing seq block.

**Tech Stack:** C99, host gcc + `x86_64-aros-gcc` cross-check via the existing audit, `ri_assert.h` test helpers.

**Spec:** `docs/superpowers/specs/2026-09-25-transport-design.md` — the plan argues from the spec; executors read both. No internet research was needed: every requirement is E1 text already digested in the spec; no external facts are used anywhere below.

## Transport laws (normative — implementation, tests, and integration must preserve them)

### Ownership
* `RITransport` owns exactly `state` and `clicks`.
* `RILoop` owns loop geometry and `on`.
* The record target owns `armed`.
* `RISeq` owns the transport, loop, pending loop, and tick cursor.
* `transport.h` depends only on `<stdint.h>`; never includes `riseq.h`.
* `transport.c` has no mutable static state.
* Transport transitions (`ri_tr_play/stop/record`) vs cursor operations
  (`ri_tr_seek_bars`): documented separately, never mixed.

### Cursor
* The authoritative cursor is always in ticks.
* Samples are derived only through `ri_map_tick()`.
* Transport state changes never convert ticks to samples.
* Display is a pure projection of `(tick, ppq)` and never changes the cursor.

### Geometry
* Song geometry uses zero-based bar indices.
* `song_bars` is a COUNT, not the maximum bar index.
* For `song_bars > 0`, valid bar starts are `0 .. song_bars - 1`.
* The end boundary `song_bars` is NOT a valid bar start.
* `#define RI_SEQ_MAX_BARS 999u` (spec §3 ceiling).
* Geometry PRIMITIVES (`tick_of_bar/bar_at_tick/display`) never clamp —
  their valid-domain preconditions are explicit (bar `0..RI_SEQ_MAX_BARS`,
  normalized PPQ). POLICY functions (`seek/loop_clamp/loop_stage`)
  normalize geometry to `0..RI_SEQ_MAX_BARS` BEFORE calling primitives.

### PPQ
* `ppq == 0` means the engine default `96` (`RI_PPQ_DEFAULT`).
* Normalized PPQ is non-zero AND divisible by 4 (`RI_PPQ_MIN 4u` —
  anything smaller normalizes to the default; this keeps `ppq/4` exact
  and `sixteenth` division total).
* All bar/display helpers use the one shared `ri_ppq_or_default()`.

### Integer safety
* Unsigned arithmetic is modulo arithmetic. `-ftrapv` traps SIGNED
  overflow only (GCC-documented) and is NOT an unsigned-overflow guard.
* Safety comes from domain bounds: every `ri_seq_tick_of_bar()` call
  carries `bar <= RI_SEQ_MAX_BARS`, so `bar*4*ppq` is bounded below
  `UINT64_MAX` for the permitted PPQ domain.
* No policy path converts an unbounded `uint64_t` to `int64_t`
  (implementation-defined result) to establish bounds — seeks branch
  on sign WITHOUT narrowing the tick-derived bar (see Task 2 impl).
* `bar_now + 1` staging saturates at `RI_SEQ_MAX_BARS + 1` (never wraps).

### Transport
* `clicks == 0` whenever `state != STOPPED` (established explicitly by
  every transition — never by assumption; see Task 1 impl).
* `PLAYING/RECORD -> STOPPED` sets `clicks = 1`, cursor held.
* `STOPPED + clicks == 0 + Stop` sets `clicks = 1`, cursor held
  (first stop while stopped arms the next jump, moves nothing).
* `STOPPED + clicks == 1 + Stop` moves to loop start, `clicks = 2`.
* `STOPPED + clicks == 2 + Stop` moves to song start, `clicks = 0`.
* A further Stop from `clicks == 0` therefore restarts with click 1
  (DELIBERATE EXTENSION — E1 ends at the 3rd click; keeps every press
  meaningful and the machine total).
* Any transition from STOPPED to another transport state clears `clicks`.

### Seeking
* Seek preserves transport state. Seek clears `clicks`.
* Negative seeks clamp to bar 0. Positive seeks beyond the song clamp to
  the LAST VALID bar start (`song_bars - 1`), never the end boundary.
* An empty song always seeks to tick 0.

### Loops
* `len_bars == 0` forces `on == 0`. `song_bars == 0` forces `on == 0`
  with canonical `{0, 0, 0}` (empty song → empty loop, self-describing).
* A loop starting at/beyond the song end snaps to the last valid bar
  with length 1 (non-empty song).
* A loop extending beyond the song shortens to fit; never rejected.
* Staged edits go live only at their bar boundary; staging never mutates
  live; polling an empty slot is a no-op.
* `apply_bar == RI_SEQ_MAX_BARS + 1` (staged past the ceiling) never
  applies — the song cannot reach it.

### Proof discipline
Every invariant has (1) one named enforcement point, (2) executable
tests, (3) at least one boundary test, (4) at least one mutation test.

## Global Constraints

- `-std=c99 -Wall -Wextra -Werror -pedantic -ftrapv` (repo `CFLAGS`).
- No allocation, IO, libm, time, or global RNG in `engine/`.
- TDD: watch each test fail first (RED) for a BEHAVIORAL reason (wrong value/branch), then GREEN. Missing-file / implicit-declaration scaffolding failures (Tasks 1-3 Steps 2) prove only that the new symbol is absent — the behavioral RED is the failing RI_ASSERT on first execution (record its line in the commit message body).
- Full `scripts/ri_audit.sh` 0/0 before every commit.
- Commit messages end with `[§12.9a]` plus the `Co-Authored-By: OpenCode <noreply@opencode.ai>` line.
- Existing goldens must not move (this slice adds none).

## Review Focus

- 4th consecutive Stop restarts the sequence as click 1 (cursor held, clicks == 1) — pinned in Task 1.
- Seek past the song end clamps to the LAST VALID bar start (100-bar song → bar 99, never the end boundary 100) — pinned in Task 2.
- Loop edit longer than the song clamps `len_bars` down (never rejects) — pinned in Task 3.
- `ppq == 0` falls back to 96 in all four helpers — pinned in Task 2.
- RECORD + Stop lands STOPPED with click 1 (capture ends dead, no resume) — pinned in Task 1.
- Bar helpers never touch a struct; display never feeds engine math; `RISeq` is not included by `transport.h` — pinned in Task 2 (include-guard test) and Task 4 (one-direction include).
- `transport.c` has no `static` mutable state (reentrancy: two `RISeq` instances step independently) — pinned in Task 4.

---

### Task 0: Preconditions (no code)

**Files:** none (read-only).

- [ ] **Step 1: Confirm a clean tree for the touched paths**
```bash
git status --short engine/seq scripts/ tests/unit/t58_transport.c
```
Expected: empty output.

- [ ] **Step 2: Baseline audit 0/0, save the log**
```bash
bash scripts/ri_audit.sh > /tmp/ri/audit-baseline-129a.log 2>&1; echo "AUDIT_RC=$?"
```
Expected: `AUDIT_RC=0`, tail `AUDIT 0/0 PASS`.

- [ ] **Step 3: Re-read the paths the cursor will touch**
```bash
sed -n '1,80p' engine/seq/riseq.c; grep -n "ri_map_tick" engine/seq/clock.h
```
Expected: `RiSeqInit` zeroes plain fields; `ri_map_tick(map, tick)` is the only tick→sample conversion (cursor stays ticks; samples convert at render).

---

### Task 1: States + transitions (`transport.h`, `transport.c`, t58 part 1)

**Files:**
- Create: `engine/seq/transport.h`
- Create: `engine/seq/transport.c`
- Create: `tests/unit/t58_transport.c` (grows in later tasks; this task owns the transition matrix only)

**Interfaces:**
- Consumes: nothing (stdint.h only).
- Produces (exact signatures, used by Tasks 3–5):
```c
enum RI_TRANSPORT { RI_TR_STOPPED = 0, RI_TR_PLAYING = 1, RI_TR_RECORD = 2 };
#define RI_SEQ_MAX_BARS 999u
#define RI_PPQ_DEFAULT 96u
#define RI_PPQ_MIN 4u
struct RITransport { uint8_t state; uint8_t clicks; };
/* One normalization: 0 -> default; sub-minimum -> default (keeps ppq/4 exact). */
static inline uint32_t ri_ppq_or_default(uint32_t ppq) {
    if (ppq == 0u || ppq < RI_PPQ_MIN) return RI_PPQ_DEFAULT;
    return ppq;
}
void ri_tr_play(struct RITransport *t, uint64_t *cursor_ticks);
void ri_tr_stop(struct RITransport *t, uint64_t *cursor_ticks,
                uint64_t loop_start_tick, uint64_t song_start_tick);
void ri_tr_record(struct RITransport *t, uint64_t *cursor_ticks);
void ri_tr_seek_bars(struct RITransport *t, uint64_t *cursor_ticks, uint32_t ppq,
                     int32_t delta_bars, uint64_t song_bars);
```

- [ ] **Step 1: Write the failing test** (transition matrix; `armed` is a caller-side sentinel proving transitions never touch it)

```c
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "tests/helpers/ri_assert.h"
#include "engine/seq/transport.h"

int main(void) {
    struct RITransport t;
    uint64_t cur;
    int armed = 7;
    /* Law probe: after EVERY transition below that leaves STOPPED,
    * clicks must read 0. Checked inline at each site (not once at the
    * end) so a regression names its own transition. */
#define RI_T58_LAW(tt) RI_ASSERT((tt).state == RI_TR_STOPPED || (tt).clicks == 0u, "law")
    /* Play from STOPPED: continue, clicks 0. */
    t.state = RI_TR_STOPPED; t.clicks = 0; cur = 12345ULL;
    ri_tr_play(&t, &cur);
    RI_ASSERT(t.state == RI_TR_PLAYING, "play state");
    RI_ASSERT(cur == 12345ULL, "play cursor moved");
    RI_ASSERT(t.clicks == 0u, "play clicks");
    RI_T58_LAW(t);
    /* PLAYING + Play: no-op. */
    ri_tr_play(&t, &cur);
    RI_ASSERT(t.state == RI_TR_PLAYING && cur == 12345ULL, "play idempotent");
    /* RECORD + Stop: STOPPED click 1, cursor held. */
    t.state = RI_TR_RECORD; t.clicks = 0; cur = 999ULL;
    ri_tr_stop(&t, &cur, 0ULL, 0ULL);
    RI_ASSERT(t.state == RI_TR_STOPPED && t.clicks == 1u && cur == 999ULL, "rec stop");
    /* Stop law (ReBirth-inspired + deliberate extension — E1 ends at
    * the 3rd click; the 4th restarting as click 1 keeps every Stop press
    * meaningful and the machine total). The FIRST stop while STOPPED
    * (clicks==0) moves nothing — it only arms the next jump. */
    t.state = RI_TR_STOPPED; t.clicks = 1u; cur = 5000ULL;
    ri_tr_stop(&t, &cur, 777ULL, 0ULL);
    RI_ASSERT(cur == 777ULL && t.clicks == 2u, "stop2");
    ri_tr_stop(&t, &cur, 777ULL, 0ULL);
    RI_ASSERT(cur == 0ULL && t.clicks == 0u, "stop3");
    ri_tr_stop(&t, &cur, 777ULL, 0ULL);
    RI_ASSERT(cur == 0ULL && t.clicks == 1u, "stop4 restarts");
    /* Record button matrix. */
    t.state = RI_TR_STOPPED; t.clicks = 2u; cur = 11ULL;
    ri_tr_record(&t, &cur);
    RI_ASSERT(t.state == RI_TR_RECORD && cur == 11ULL && t.clicks == 0u, "rec from stop");
    RI_T58_LAW(t);
    t.state = RI_TR_PLAYING; cur = 11ULL;
    ri_tr_record(&t, &cur);
    RI_ASSERT(t.state == RI_TR_RECORD && cur == 11ULL, "punch in");
    RI_T58_LAW(t);
    ri_tr_record(&t, &cur);
    RI_ASSERT(t.state == RI_TR_PLAYING && cur == 11ULL, "punch out");
    RI_T58_LAW(t);
    /* Null-safe, armed untouched by every call above. Law holds on this
    * path too: the PLAYING exit zeroed clicks. */
    ri_tr_play(0, &cur); ri_tr_play(&t, 0); ri_tr_stop(0, 0, 0, 0); ri_tr_record(0, 0);
    RI_ASSERT(armed == 7, "armed touched");
#undef RI_T58_LAW
    RI_RESULT("transport");
}
```

- [ ] **Step 2: Run test to verify it fails**

Run: `bash scripts/ri_build_host.sh test t58_transport 2>&1 | head -3`
Expected: FAIL with `engine/seq/transport.h: No such file or directory`

- [ ] **Step 3: Write minimal implementation** (`engine/seq/transport.h` + `transport.c`)

```c
/* transport.h — transport state machine (spec 2026-09-25 §1).
 * Pure, stdint.h only, no alloc, no IO. Owns state + clicks only;
 * loop bounds arrive as read-only tick inputs, never stored. */
#ifndef RI_TRANSPORT_H
#define RI_TRANSPORT_H
#include <stdint.h>
enum RI_TRANSPORT { RI_TR_STOPPED = 0, RI_TR_PLAYING = 1, RI_TR_RECORD = 2 };
struct RITransport { uint8_t state; uint8_t clicks; };
void ri_tr_play(struct RITransport *t, uint64_t *cursor_ticks);
void ri_tr_stop(struct RITransport *t, uint64_t *cursor_ticks,
                uint64_t loop_start_tick, uint64_t song_start_tick);
void ri_tr_record(struct RITransport *t, uint64_t *cursor_ticks);
void ri_tr_seek_bars(struct RITransport *t, uint64_t *cursor_ticks, uint32_t ppq,
                     int32_t delta_bars, uint64_t song_bars);
static inline uint32_t ri_ppq_or_default(uint32_t ppq) { return ppq ? ppq : 96u; }
#endif
```

```c
/* transport.c — geometry is clamped to [0, 999] bars by all public entry
 * points (spec §3 clamp law); the helpers below stay total so -ftrapv
 * never fires inside the proof. Shared ppq fallback lives in
 * transport.h as ri_ppq_or_default (one definition, used by every helper). */
#include "engine/seq/transport.h"
void ri_tr_play(struct RITransport *t, uint64_t *cursor_ticks) {
    (void)cursor_ticks;
    if (!t)
        return;
    if (t->state == RI_TR_STOPPED) {
        t->state = RI_TR_PLAYING;
        t->clicks = 0u;
    }
}
void ri_tr_stop(struct RITransport *t, uint64_t *cursor_ticks,
                uint64_t loop_start_tick, uint64_t song_start_tick) {
    if (!t || !cursor_ticks)
        return;
    if (t->state != RI_TR_STOPPED) {
        t->state = RI_TR_STOPPED;
        t->clicks = 1u;
        return; /* cursor held (click 1) */
    }
    if (t->clicks == 0u) {
        /* First stop while already STOPPED: arm only (clicks 0->1),
        * cursor held. The visible jump comes on the 2nd press. */
        t->clicks = 1u;
    } else if (t->clicks == 1u) {
        *cursor_ticks = loop_start_tick;
        t->clicks = 2u;
    } else {
        *cursor_ticks = song_start_tick;
        t->clicks = 0u; /* sequence restarts */
    }
}
void ri_tr_record(struct RITransport *t, uint64_t *cursor_ticks) {
    (void)cursor_ticks;
    if (!t)
        return;
    if (t->state == RI_TR_STOPPED) {
        t->state = RI_TR_RECORD;
        t->clicks = 0u;
    } else if (t->state == RI_TR_PLAYING) {
        t->state = RI_TR_RECORD;
        t->clicks = 0u; /* law established locally, never assumed */
    } else {
        t->state = RI_TR_PLAYING;
        t->clicks = 0u; /* law established locally, never assumed */
    }
}
```

(`ri_tr_seek_bars` arrives in Task 2 with the bar helpers it needs.)

- [ ] **Step 4: Wire the build, then run test to verify it passes**

Do the `MOD_sched` edit from Step 5 FIRST (the `sched` target needs
`transport.c` before the test links):

Run: `bash scripts/ri_build_host.sh sched && bash scripts/ri_build_host.sh test t58_transport 2>&1 | tail -2`
Expected: `PASS transport`

- [ ] **Step 5: Commit**

The `sched` target currently builds `transport.c` before it exists —
so wire it in Step 4 of THIS task, not here. Edit
`scripts/ri_build_host.sh` line 11:

`MOD_sched="... engine/seq/pattern_emit.c engine/seq/transport.c"`.

```bash
git add engine/seq/transport.h engine/seq/transport.c tests/unit/t58_transport.c scripts/ri_build_host.sh
git commit -m "feat: transport states + stop-click law + record matrix [§12.9a]

Co-Authored-By: OpenCode <noreply@opencode.ai>"
```

---

### Task 2: Bar helpers + seeks + display (t58 part 2)

**Files:**
- Modify: `engine/seq/transport.h` (append API), `engine/seq/transport.c` (append impl), `tests/unit/t58_transport.c` (append cases before `RI_RESULT`)

**Interfaces:**
- Consumes: `RITransport` (clicks zeroing on seek).
- Produces:
```c
uint64_t ri_seq_tick_of_bar(uint32_t ppq, uint64_t bar);
uint64_t ri_seq_bar_at_tick(uint64_t tick, uint32_t ppq);
struct RIBarPos { uint16_t bar; uint8_t beat; uint8_t sixteenth; };
struct RIBarPos ri_seq_bar_display(uint64_t tick, uint32_t ppq); /* pure projection, no clamp */
```
(`ri_tr_seek_bars` from Task 1 is implemented here. POLICY vs PRIMITIVE:
seek/loop normalize geometry first, then call the total primitives —
primitives never clamp.)

- [ ] **Step 1: Write the failing test** (append before `RI_RESULT("transport");`)

```c
    /* Bar helpers: ppq 96, bar = 384 ticks. */
    RI_ASSERT(ri_seq_tick_of_bar(96u, 0u) == 0u, "bar0 tick");
    RI_ASSERT(ri_seq_tick_of_bar(96u, 3u) == 1152u, "bar3 tick");
    RI_ASSERT(ri_seq_bar_at_tick(0u, 96u) == 0u, "tick0 bar");
    RI_ASSERT(ri_seq_bar_at_tick(383u, 96u) == 0u, "floor");
    RI_ASSERT(ri_seq_bar_at_tick(384u, 96u) == 1u, "rollover");
    RI_ASSERT(ri_seq_tick_of_bar(0u, 5u) == 5u * 384u, "ppq0 tick");
    RI_ASSERT(ri_seq_bar_at_tick(384u * 7u + 1u, 0u) == 7u, "ppq0 bar");
    /* Seeks take song_bars (not song_ticks): the caller owns the bar
    * count, so no tick->bar re-derivation and no tempo-map window. */
    t.state = RI_TR_STOPPED; t.clicks = 2u; cur = ri_seq_tick_of_bar(96u, 20u);
    ri_tr_seek_bars(&t, &cur, 96u, -10, 100u);
    RI_ASSERT(cur == ri_seq_tick_of_bar(96u, 10u) && t.clicks == 0u &&
        t.state == RI_TR_STOPPED, "seek back");
    RI_T58_LAW(t);
    ri_tr_seek_bars(&t, &cur, 96u, -50, 100u);
    RI_ASSERT(cur == 0u, "seek clamps front");
    RI_T58_LAW(t);
    ri_tr_seek_bars(&t, &cur, 96u, 500, 100u);
    RI_ASSERT(cur == ri_seq_tick_of_bar(96u, 99u), "seek clamps end");
    RI_T58_LAW(t);
    /* End-boundary pair: exactly-to-end vs one-beyond (inclusive/exclusive). */
    cur = ri_seq_tick_of_bar(96u, 99u);
    ri_tr_seek_bars(&t, &cur, 96u, 1, 100u);
    RI_ASSERT(cur == ri_seq_tick_of_bar(96u, 99u), "seek at end holds");
    cur = ri_seq_tick_of_bar(96u, 98u);
    ri_tr_seek_bars(&t, &cur, 96u, 1, 100u);
    RI_ASSERT(cur == ri_seq_tick_of_bar(96u, 99u), "seek into end");
    /* Empty / singleton songs. */
    cur = 55555ULL;
    ri_tr_seek_bars(&t, &cur, 96u, 10, 0u);
    RI_ASSERT(cur == 0u, "empty song tick0");
    cur = ri_seq_tick_of_bar(96u, 0u);
    ri_tr_seek_bars(&t, &cur, 96u, 5, 1u);
    RI_ASSERT(cur == 0u, "singleton holds");
    /* Malformed internal state: PLAYING must still exit with clicks 0. */
    t.state = RI_TR_PLAYING; t.clicks = 17u; cur = 0ULL;
    ri_tr_record(&t, &cur);
    RI_ASSERT(t.state == RI_TR_RECORD && t.clicks == 0u, "malformed record");
    RI_T58_LAW(t);
    t.state = RI_TR_PLAYING;
    ri_tr_seek_bars(&t, &cur, 96u, 1, 100u);
    RI_ASSERT(t.state == RI_TR_PLAYING, "seek keeps state");
    RI_T58_LAW(t);
    /* Property loops: round-trip every legal bar + interior tick stays
    * in-bar (stronger than the hand-picked 0/3/383/384 pins above). */
    {
        uint64_t b;
        for (b = 0u; b <= 999u; b++) {
            uint64_t tk = ri_seq_tick_of_bar(96u, b);
            RI_ASSERT(ri_seq_bar_at_tick(tk, 96u) == b, "bar roundtrip %llu",
                (unsigned long long)b);
        }
        for (b = 0u; b < 999u; b++) {
            uint64_t tk = ri_seq_tick_of_bar(96u, b) + 383u;
            RI_ASSERT(ri_seq_bar_at_tick(tk, 96u) == b, "bar interior %llu",
                (unsigned long long)b);
        }
    }
    /* Layer guard (cheap textual tripwire, NOT a dependency proof —
    * the compiler include structure in Task 4 is the real enforcement): — transport.h includes
    * stdint.h and nothing else (no RISeq visibility). Breaks here if
    * anyone smuggles a struct into the free functions. */
    {
        FILE *fh = fopen("engine/seq/transport.h", "r");
        char line[256]; int bad = 0;
        RI_ASSERT(fh != 0, "open header");
        if (fh) {
            while (fgets(line, sizeof line, fh)) {
                if (strstr(line, "#include") && !strstr(line, "stdint.h"))
                    bad = 1;
                if (strstr(line, "RISeq"))
                    bad = 1;
            }
            fclose(fh);
        }
        RI_ASSERT(!bad, "layer leak");
    }
    /* Display is a pure projection of (tick, ppq) — no clamp inside.
    * Clamping belongs to the cursor owner: the caller clamps the tick
    * (seek path) before projecting. Past-end therefore pins as raw bar
    * math here; the clamped-panel case is pinned by the caller-side
    * sequence below it. */
    {
        struct RIBarPos dp = ri_seq_bar_display(0u, 96u);
        RI_ASSERT(dp.bar == 1u && dp.beat == 1u && dp.sixteenth == 1u, "1.1.1");
        dp = ri_seq_bar_display(383u, 96u);
        RI_ASSERT(dp.bar == 1u && dp.beat == 4u && dp.sixteenth == 4u, "1.4.4");
        dp = ri_seq_bar_display(384u, 96u);
        RI_ASSERT(dp.bar == 2u && dp.beat == 1u && dp.sixteenth == 1u, "2.1.1");
        dp = ri_seq_bar_display(384u * 100u + 57u, 96u);
        RI_ASSERT(dp.bar == 101u, "past-end projects raw %u", dp.bar);
        dp = ri_seq_bar_display(0u, 0u);
        RI_ASSERT(dp.bar == 1u && dp.beat == 1u && dp.sixteenth == 1u, "ppq0 disp");
        dp = ri_seq_bar_display(96u, 1u); /* sub-minimum -> default, no div0 */
        RI_ASSERT(dp.bar == 2u && dp.beat == 1u && dp.sixteenth == 1u, "ppq1 safe");
        dp = ri_seq_bar_display(0u, 4u);
        RI_ASSERT(dp.bar == 1u && dp.beat == 1u && dp.sixteenth == 1u, "ppq4 min");
        /* Caller-side clamp: song of 100 bars, tick past the end shows
        * the last bar (spec §4 past-end display law). */
        {
            uint64_t song_ticks = ri_seq_tick_of_bar(96u, 100u);
            uint64_t past = song_ticks + 57u;
            uint64_t shown = past >= song_ticks
                ? ri_seq_tick_of_bar(96u, 100u - 1u) : past;
            dp = ri_seq_bar_display(shown, 96u);
            RI_ASSERT(dp.bar == 100u, "panel clamps %u", dp.bar);
        }
    }

- [ ] **Step 2: Run test to verify it fails**

Run: `bash scripts/ri_build_host.sh test t58_transport 2>&1 | grep -E "implicit|error" | head -2`
Expected: FAIL with implicit declaration of `ri_seq_tick_of_bar`

- [ ] **Step 3: Write minimal implementation** (append to `transport.c`)

```c
/* Shared fallback lives in the header (ri_ppq_or_default); no local copy. */
uint64_t ri_seq_tick_of_bar(uint32_t ppq, uint64_t bar) {
    return bar * 4u * (uint64_t)ri_ppq_or_default(ppq);
}
uint64_t ri_seq_bar_at_tick(uint64_t tick, uint32_t ppq) {
    return tick / (4u * (uint64_t)ri_ppq_or_default(ppq));
}
/* Pure projection: (tick, ppq) -> 1-based panel struct. No clamp, no
 * song knowledge — the caller clamps the tick first (see test). */
struct RIBarPos ri_seq_bar_display(uint64_t tick, uint32_t ppq) {
    struct RIBarPos d;
    uint64_t p = (uint64_t)ri_ppq_or_default(ppq);
    uint64_t inbar;
    d.bar = (uint16_t)(ri_seq_bar_at_tick(tick, (uint32_t)p) + 1u);
    inbar = tick - ri_seq_tick_of_bar((uint32_t)p, (uint64_t)(d.bar - 1u));
    d.beat = (uint8_t)(inbar / p + 1u);
    d.sixteenth = (uint8_t)((inbar % p) / (p / 4u) + 1u);
    return d;
}
/* Seek takes song_bars: the caller owns the bar count, so this path
 * never re-derives bars from ticks and never touches the tempo map. */
void ri_tr_seek_bars(struct RITransport *t, uint64_t *cursor_ticks, uint32_t ppq,
                     int32_t delta_bars, uint64_t song_bars) {
    uint64_t cur_bar, max_bar, target;
    if (!t || !cursor_ticks)
        return;
    /* Normalize FIRST (policy): empty song -> tick 0, no primitive called. */
    if (song_bars == 0u) { *cursor_ticks = 0u; t->clicks = 0u; return; }
    song_bars = song_bars > RI_SEQ_MAX_BARS ? RI_SEQ_MAX_BARS : song_bars;
    max_bar = song_bars - 1u; /* last VALID start; end boundary is not a bar */
    cur_bar = ri_seq_bar_at_tick(*cursor_ticks, ppq);
    if (cur_bar > max_bar)
        cur_bar = max_bar; /* cursor invariant repair: never seek FROM past-end */
    if (delta_bars < 0) {
        /* Unsigned-only: -(INT32_MIN) is computed in int64_t, then bounded. */
        int64_t dneg = -(int64_t)delta_bars; /* 1..2147483648, always positive */
        uint64_t dist = (uint64_t)dneg;
        target = (dist > cur_bar) ? 0u : cur_bar - dist;
    } else {
        uint64_t dist = (uint64_t)(int64_t)delta_bars;
        /* max_bar - cur_bar cannot underflow (cur_bar <= max_bar above). */
        target = (dist > max_bar - cur_bar) ? max_bar : cur_bar + dist;
    }
    *cursor_ticks = ri_seq_tick_of_bar(ppq, target);
    t->clicks = 0u; /* cursor intent replaces the stop sequence */
}
```
Note: `song_bars == 0` seeks to tick 0 without touching primitives.
`song_bars` normalizes to `RI_SEQ_MAX_BARS` first, so `song_bars - 1`
cannot underflow. `cur_bar` repairs to `max_bar` (a past-end cursor —
e.g. freshly shortened song — seeks FROM the last valid bar, never from
past-end). `dist` for `INT32_MIN` is `2147483648` via `int64_t` negation
(no unsigned wrap, no implementation-defined `uint64_t->int64_t` narrowing).
`tick_of_bar` receives `target <= 999`, so `bar*4*ppq` is bounded far below
`UINT64_MAX` (unsigned safety from the domain, not `-ftrapv`).

- [ ] **Step 4: Run test to verify it passes**

Run: `bash scripts/ri_build_host.sh sched && bash scripts/ri_build_host.sh test t58_transport 2>&1 | tail -2`
Expected: `PASS transport`

- [ ] **Step 5: Commit**

```bash
git add engine/seq/transport.h engine/seq/transport.c tests/unit/t58_transport.c
git commit -m "feat: bar helpers + clamped seeks + one-way display [§12.9a]

Co-Authored-By: OpenCode <noreply@opencode.ai>"
```

---

### Task 3: Loop model + clamp + staging (t58 part 3)

**Files:**
- Modify: `engine/seq/transport.h`, `engine/seq/transport.c`, `tests/unit/t58_transport.c`

**Interfaces:**
- Produces:
```c
struct RILoop { uint8_t on; uint16_t start_bar; uint16_t len_bars; };
struct RILoopPending { uint8_t valid; uint64_t apply_bar; struct RILoop loop; };
void ri_loop_clamp(struct RILoop *loop, uint64_t song_bars);
void ri_loop_stage(struct RILoopPending *p, const struct RILoop *loop, uint64_t bar_now, uint64_t song_bars);
int ri_loop_poll(struct RILoopPending *p, struct RILoop *live, uint64_t bar_now);
```

- [ ] **Step 1: Write the failing test** (append before `RI_RESULT`)

```c
    /* Loop clamp law. */
    {
        struct RILoop l = { 1, 90, 20 };
        ri_loop_clamp(&l, 100u);
        RI_ASSERT(l.start_bar == 90u && l.len_bars == 10u && l.on == 1u, "clamp len");
        l.start_bar = 150; l.len_bars = 5;
        ri_loop_clamp(&l, 100u);
        RI_ASSERT(l.start_bar == 99u && l.len_bars == 1u, "clamp start");
        l.on = 1; l.start_bar = 0; l.len_bars = 0;
        ri_loop_clamp(&l, 100u);
        RI_ASSERT(l.on == 0u, "zero len kills loop");
        l.on = 1; l.start_bar = 99; l.len_bars = 999;
        ri_loop_clamp(&l, 100u);
        RI_ASSERT(l.start_bar == 99u && l.len_bars == 1u, "tail clamp");
        l.on = 1; l.start_bar = 0; l.len_bars = 1;
        ri_loop_clamp(&l, 1u);
        RI_ASSERT(l.on == 1u && l.start_bar == 0u && l.len_bars == 1u, "singleton");
        l.on = 1; l.start_bar = 99; l.len_bars = 1;
        ri_loop_clamp(&l, 99u);
        RI_ASSERT(l.start_bar == 98u && l.len_bars == 1u, "past-end snap");
        l.on = 1; l.start_bar = 5; l.len_bars = 3;
        ri_loop_clamp(&l, 0u);
        RI_ASSERT(l.on == 0u && l.start_bar == 0u && l.len_bars == 0u, "empty canon");
        l.on = 1; l.start_bar = 0; l.len_bars = 1;
        ri_loop_clamp(&l, 5000u);
        RI_ASSERT(l.on == 1u && l.start_bar == 0u && l.len_bars == 1u, "ceiling norm");
        ri_loop_clamp(0, 100u);
    }
    /* Staging swaps at the next bar line. */
    {
        struct RILoop live = { 1, 0, 4 }, nl = { 1, 8, 4 };
        struct RILoopPending pend = { 0, 0, { 0, 0, 0 } };
        ri_loop_stage(&pend, &nl, 3u, 100u);
        RI_ASSERT(pend.valid == 1u && pend.apply_bar == 4u, "stage");
        RI_ASSERT(ri_loop_poll(&pend, &live, 3u) == 0, "early poll");
        RI_ASSERT(live.start_bar == 0u, "early applied?");
        RI_ASSERT(ri_loop_poll(&pend, &live, 4u) == 1, "bar poll");
        RI_ASSERT(live.start_bar == 8u && pend.valid == 0u, "applied");
        RI_ASSERT(ri_loop_poll(&pend, &live, 99u) == 0, "empty poll");
        ri_loop_stage(&pend, &nl, 999u, 100u);
        RI_ASSERT(pend.valid == 1u && pend.apply_bar == 1000u, "ceiling stage");
        RI_ASSERT(ri_loop_poll(&pend, &live, 999u) == 0, "ceiling never fires");
        ri_loop_stage(0, &nl, 3u, 100u); ri_loop_stage(&pend, 0, 3u, 100u);
    }
```

- [ ] **Step 2: Run test to verify it fails**

Run: `bash scripts/ri_build_host.sh test t58_transport 2>&1 | grep -E "implicit|error" | head -2`
Expected: FAIL with implicit declaration of `ri_loop_clamp`

- [ ] **Step 3: Write minimal implementation** (append to `transport.c`)

```c
void ri_loop_clamp(struct RILoop *loop, uint64_t song_bars) {
    if (!loop)
        return;
    song_bars = song_bars > RI_SEQ_MAX_BARS ? RI_SEQ_MAX_BARS : song_bars;
    if (song_bars == 0u) {
        loop->on = 0u; loop->start_bar = 0u; loop->len_bars = 0u;
        return;
    }
    if (loop->len_bars == 0u) {
        loop->on = 0u; /* zero length = loop off, never an empty region */
        return;
    }
    if ((uint64_t)loop->start_bar >= song_bars) {
        loop->start_bar = (uint16_t)(song_bars - 1u);
        loop->len_bars = 1u;
        return;
    }
    if ((uint64_t)loop->start_bar + (uint64_t)loop->len_bars > song_bars)
        loop->len_bars = (uint16_t)(song_bars - (uint64_t)loop->start_bar);
}
void ri_loop_stage(struct RILoopPending *p, const struct RILoop *loop, uint64_t bar_now,
                   uint64_t song_bars) {
    if (!p || !loop)
        return;
    song_bars = song_bars > RI_SEQ_MAX_BARS ? RI_SEQ_MAX_BARS : song_bars;
    if (bar_now > RI_SEQ_MAX_BARS)
        bar_now = RI_SEQ_MAX_BARS;
    p->loop = *loop;
    ri_loop_clamp(&p->loop, song_bars);
    p->apply_bar = (bar_now >= RI_SEQ_MAX_BARS) ? (uint64_t)RI_SEQ_MAX_BARS + 1u : bar_now + 1u;
    p->valid = 1u;
}
int ri_loop_poll(struct RILoopPending *p, struct RILoop *live, uint64_t bar_now) {
    if (!p || !live || !p->valid)
        return 0;
    if (bar_now < p->apply_bar)
        return 0;
    *live = p->loop;
    p->valid = 0u;
    return 1;
}
```
Note: policy normalizes `song_bars`/`bar_now` to `RI_SEQ_MAX_BARS` on entry, so `song_bars - 1` cannot underflow and `bar_now + 1` cannot wrap. `apply_bar == 1000` (staged past the ceiling) never fires — the song cannot reach it. Primitives stay total; policy owns every bound.

- [ ] **Step 4: Run test to verify it passes**

Run: `bash scripts/ri_build_host.sh sched && bash scripts/ri_build_host.sh test t58_transport 2>&1 | tail -2`
Expected: `PASS transport`

- [ ] **Step 5: Commit**

```bash
git add engine/seq/transport.h engine/seq/transport.c tests/unit/t58_transport.c
git commit -m "feat: loop clamp law + next-bar staging [§12.9a]

Co-Authored-By: OpenCode <noreply@opencode.ai>"
```

---

### Task 4: RISeq integration + record matrix

**Files:**
- Modify: `engine/seq/riseq.h` (struct fields), `engine/seq/riseq.c` (`RiSeqInit` zeroes), `tests/unit/t58_transport.c` (integration pins)

**Interfaces:**
- Consumes: all Task 1–3 APIs.
- Produces: `RISeq.transport`, `RISeq.loop`, `RISeq.loop_staged`, `RISeq.cursor_ticks`, all zero-initialized by the existing `RiSeqInit`.

- [ ] **Step 1: Write the failing test** (append before `RI_RESULT`)

```c
    /* RISeq owns transport state; init is neutral. */
    {
        struct RISeq sq;
        RiSeqInit(&sq, 0, 96);
        RI_ASSERT(sq.transport.state == RI_TR_STOPPED, "init state");
        RI_ASSERT(sq.transport.clicks == 0u, "init clicks");
        RI_ASSERT(sq.loop.on == 0u, "init loop");
        RI_ASSERT(sq.loop_staged.valid == 0u, "init staged");
        RI_ASSERT(sq.cursor_ticks == 0ULL, "init cursor");
        RI_ASSERT(sq.ppq == 96u, "init ppq kept");
        /* Drive a full stop-play-stop cycle through the struct. */
        ri_tr_play(&sq.transport, &sq.cursor_ticks);
        RI_T58_LAW(sq.transport);
        sq.cursor_ticks = ri_seq_tick_of_bar(sq.ppq, 30u);
        ri_tr_stop(&sq.transport, &sq.cursor_ticks, 0ULL, 0ULL);
        ri_tr_stop(&sq.transport, &sq.cursor_ticks, 0ULL, 0ULL);
        RI_ASSERT(sq.transport.clicks == 2u && sq.cursor_ticks == 0ULL, "struct cycle");
    }
    /* Reentrancy: two instances step independently (no static state). */
    {
        struct RISeq a, b;
        RiSeqInit(&a, 0, 96); RiSeqInit(&b, 0, 96);
        ri_tr_play(&a.transport, &a.cursor_ticks);
        a.cursor_ticks = ri_seq_tick_of_bar(96u, 10u);
        RI_ASSERT(b.transport.state == RI_TR_STOPPED && b.cursor_ticks == 0ULL,
            "b untouched");
        RI_ASSERT(a.transport.state == RI_TR_PLAYING && a.cursor_ticks == 3840u,
            "a stepped");
        ri_loop_stage(&a.loop_staged, &a.loop, ri_seq_bar_at_tick(a.cursor_ticks, 96u), 100u);
        RI_ASSERT(b.loop_staged.valid == 0u, "b staging clean");
    }
```

- [ ] **Step 2: Run test to verify it fails**

Run: `bash scripts/ri_build_host.sh test t58_transport 2>&1 | grep -E "no member|error" | head -2`
Expected: FAIL with `has no member named 'transport'`

- [ ] **Step 3: Write minimal implementation**

In `engine/seq/riseq.h`, add `#include "engine/seq/transport.h"` (one direction only — `transport.h` stays `stdint.h`-only per the Task 2 guard) and extend the struct:
```c
struct RISeq {
    struct AudioObject *ao;
    uint32_t ppq;
    uint64_t samples;
    struct RISegment seg;
    struct RITempoMap map;
    const struct RISeqSnapshot *snap;
    const struct RISeqSnapshot *pending;
    struct RITransport transport;   /* §12.9a: stop/play/record + clicks */
    struct RILoop loop;             /* song geometry (bars, 0-based) */
    struct RILoopPending loop_staged; /* next-bar swap staging */
    uint64_t cursor_ticks;          /* held cursor (ticks; samples via ri_map_tick) */
};
```
In `engine/seq/riseq.c`, extend `RiSeqInit` after the existing zeroes:
```c
    s->transport.state = RI_TR_STOPPED;
    s->transport.clicks = 0u;
    s->loop.on = 0u;
    s->loop.start_bar = 0u;
    s->loop.len_bars = 1u;
    s->loop_staged.valid = 0u;
    s->loop_staged.apply_bar = 0ULL;
    s->cursor_ticks = 0ULL;
```

- [ ] **Step 4: Run test to verify it passes**

Run: `bash scripts/ri_build_host.sh sched && bash scripts/ri_build_host.sh test t58_transport 2>&1 | tail -2`
Expected: `PASS transport`

- [ ] **Step 5: Commit**

```bash
git add engine/seq/riseq.h engine/seq/riseq.c tests/unit/t58_transport.c
git commit -m "feat: RISeq owns transport/loop/cursor state [§12.9a]

Co-Authored-By: OpenCode <noreply@opencode.ai>"
```

---

### Task 5: Audit wiring + gate + records

**Files:**
- Modify: `scripts/ri_audit.sh` (seq block), `docs/2026-09-24-improvement-todo.md`, `llm-wiki/raw/articles/2026-09-25-transport.md` (new), `llm-wiki/log.md`, `llm-wiki/index.md`

- [ ] **Step 1: Wire t58 into the audit** (seq block beside the t21 lines)

Edit `scripts/ri_audit.sh` after the `t21_swaprender` line:
```bash
bash "$ROOT/scripts/ri_build_host.sh" test t58_transport >/dev/null || { echo "FAIL: t58_transport"; exit 1; }
```

- [ ] **Step 2: Run the FULL gate**

Run: `bash scripts/ri_audit.sh > /tmp/ri/audit-129a.log 2>&1; echo "AUDIT_RC=$?"`
Expected: `AUDIT_RC=0`, tail `AUDIT 0/0 PASS`. Any FAIL names the culprit — fix under TDD, never by weakening a pin.

- [ ] **Step 3: Prove the new gate is load-bearing** (three mutants, all recorded with failure lines in the wiki article: (a) `t->clicks = 0u` → `1u` in `ri_tr_play` must FAIL the Task 1 law probe; (b) temporary `#include "engine/seq/riseq.h"` in `transport.h` must FAIL the Task 2 layer-guard case; (c) `max_bar` → `song_bars` in `ri_tr_seek_bars` must FAIL the Task 2 `seek clamps end` case (bar 99 vs 100 — the one-past-end regression). Each: mutate, run `bash scripts/ri_build_host.sh test t58_transport`, confirm FAIL, revert, confirm PASS)

Run: temporarily break one transition (`- [ ]` only if needed — mutate `t->clicks = 0u` to `1u` in `ri_tr_play`, run `bash scripts/ri_build_host.sh test t58_transport`, confirm FAIL, revert, confirm PASS. Record the mutation and its failure line in the wiki article.

- [ ] **Step 4: Records + tracker, commit, push**

Tick the todo file (`Transport state machine` done under §12.9; song track/automation stay open). Write the raw article (transition matrix outcome, ledger rows touched: none new — §5 open items carry over verbatim; test counts; audit result; the mutation proof). Update `llm-wiki/log.md` + `llm-wiki/index.md`.
```bash
git add docs/2026-09-24-improvement-todo.md scripts/ri_audit.sh llm-wiki/raw/articles/2026-09-25-transport.md llm-wiki/log.md llm-wiki/index.md
git commit -m "docs: transport record + tracker + audit wiring [§12.9a]

Co-Authored-By: OpenCode <noreply@opencode.ai>"
git push origin main
git status --short  # must be empty
```

---

## Acceptance checklist

- [ ] Full transition matrix green (9-row table incl. malformed PLAYING/clicks=17 → RECORD/clicks=0, per-site law probes, null-safe, armed sentinel).
- [ ] Seeks clamp to the LAST VALID bar (100-bar song → bar 99, never the end boundary 100), zero clicks, keep state; empty song → tick 0.
- [ ] Bar/tick round-trips exact over 0..999 + interior ticks; display pins 1.1.1 / 1.4.4 / 2.1.1 / raw past-end projection / caller-side panel clamp / ppq 0-1-4 normalization (no div0).
- [ ] Loop clamp + next-bar staging green (incl. tail/singleton/past-end/empty-canonical/ceiling cases + apply-1000-never-fires); song-shorten path reuses the same clamp.
- [ ] RISeq init neutral; struct-driven cycle green.
- [ ] Mutation proof recorded; `ri_audit.sh` 0/0; tree clean.
