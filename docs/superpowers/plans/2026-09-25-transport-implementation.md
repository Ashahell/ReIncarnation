# Transport state machine Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Implement the owner-approved transport state machine (first §12.9 sub-slice) as pure, tested C.

**Architecture:** New `engine/seq/transport.{h,c}` holds the state machine (stdint.h only, no alloc/IO); `RISeq` gains the state structs plus a tick cursor; one new test file `t58_transport`; audit wiring in the existing seq block.

**Tech Stack:** C99, host gcc + `x86_64-aros-gcc` cross-check via the existing audit, `ri_assert.h` test helpers.

**Spec:** `docs/superpowers/specs/2026-09-25-transport-design.md` — the plan argues from the spec; executors read both. No internet research was needed: every requirement is E1 text already digested in the spec; no external facts are used anywhere below.

## Global Constraints

- `-std=c99 -Wall -Wextra -Werror -pedantic -ftrapv` (repo `CFLAGS`).
- No allocation, IO, libm, time, or global RNG in `engine/`.
- TDD: watch each test fail first (RED) for the right reason, then GREEN.
- Full `scripts/ri_audit.sh` 0/0 before every commit.
- Commit messages end with `[§12.9a]` plus the `Co-Authored-By: OpenCode <noreply@opencode.ai>` line.
- Existing goldens must not move (this slice adds none).

## Review Focus

- 4th consecutive Stop restarts the sequence as click 1 (cursor held, clicks == 1) — pinned in Task 1.
- Seek past the song end clamps to the song's last bar start (never wraps, never exceeds) — pinned in Task 1.
- Loop edit longer than the song clamps `len_bars` down (never rejects) — pinned in Task 3.
- `ppq == 0` falls back to 96 in all four helpers — pinned in Task 2.
- RECORD + Stop lands STOPPED with click 1 (capture ends dead, no resume) — pinned in Task 1.

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

- [ ] **Step 3: Re-read the реad paths the cursor will touch**
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
struct RITransport { uint8_t state; uint8_t clicks; };
void ri_tr_play(struct RITransport *t, uint64_t *cursor_ticks);
void ri_tr_stop(struct RITransport *t, uint64_t *cursor_ticks,
                uint64_t loop_start_tick, uint64_t song_start_tick);
void ri_tr_record(struct RITransport *t, uint64_t *cursor_ticks);
void ri_tr_seek_bars(struct RITransport *t, uint64_t *cursor_ticks, uint32_t ppq,
                     int32_t delta_bars, uint64_t song_ticks);
```

- [ ] **Step 1: Write the failing test** (transition matrix; `armed` is a caller-side sentinel proving transitions never touch it)

```c
#include <stdio.h>
#include <stdint.h>
#include "tests/helpers/ri_assert.h"
#include "engine/seq/transport.h"

int main(void) {
    struct RITransport t;
    uint64_t cur;
    int armed = 7;
    /* Play from STOPPED: continue, clicks 0. */
    t.state = RI_TR_STOPPED; t.clicks = 0; cur = 12345ULL;
    ri_tr_play(&t, &cur);
    RI_ASSERT(t.state == RI_TR_PLAYING, "play state");
    RI_ASSERT(cur == 12345ULL, "play cursor moved");
    RI_ASSERT(t.clicks == 0u, "play clicks");
    /* PLAYING + Play: no-op. */
    ri_tr_play(&t, &cur);
    RI_ASSERT(t.state == RI_TR_PLAYING && cur == 12345ULL, "play idempotent");
    /* RECORD + Stop: STOPPED click 1, cursor held. */
    t.state = RI_TR_RECORD; t.clicks = 0; cur = 999ULL;
    ri_tr_stop(&t, &cur, 0ULL, 0ULL);
    RI_ASSERT(t.state == RI_TR_STOPPED && t.clicks == 1u && cur == 999ULL, "rec stop");
    /* 3-stop sequence: held -> loop start -> song start -> restart. */
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
    t.state = RI_TR_PLAYING; cur = 11ULL;
    ri_tr_record(&t, &cur);
    RI_ASSERT(t.state == RI_TR_RECORD && cur == 11ULL, "punch in");
    ri_tr_record(&t, &cur);
    RI_ASSERT(t.state == RI_TR_PLAYING && cur == 11ULL, "punch out");
    /* Null-safe, armed untouched by every call above. */
    ri_tr_play(0, &cur); ri_tr_play(&t, 0); ri_tr_stop(0, 0, 0, 0); ri_tr_record(0, 0);
    RI_ASSERT(armed == 7, "armed touched");
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
                     int32_t delta_bars, uint64_t song_ticks);
#endif
```

```c
/* transport.c */
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
    } else {
        t->state = RI_TR_PLAYING;
    }
}
```

(`ri_tr_seek_bars` arrives in Task 2 with the bar helpers it needs.)

- [ ] **Step 4: Run test to verify it passes**

Run: `bash scripts/ri_build_host.sh sched && bash scripts/ri_build_host.sh test t58_transport 2>&1 | tail -2` (MOD_sched edit from Step 5 must land first if the build misses `transport.c` — see Step 5)
Expected: `PASS transport`

- [ ] **Step 5: Wire the build + commit**

Edit `scripts/ri_build_host.sh`: `MOD_sched="... engine/seq/pattern_emit.c engine/seq/transport.c"`.
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
struct RIBarPos ri_seq_bar_display(uint64_t tick, uint32_t ppq, uint64_t song_bars);
```
(`ri_tr_seek_bars` from Task 1 is implemented here.)

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
    /* Seeks: +-10 bars, clamped, zeroes clicks, state kept. */
    t.state = RI_TR_STOPPED; t.clicks = 2u; cur = ri_seq_tick_of_bar(96u, 20u);
    ri_tr_seek_bars(&t, &cur, 96u, -10, ri_seq_tick_of_bar(96u, 100u));
    RI_ASSERT(cur == ri_seq_tick_of_bar(96u, 10u) && t.clicks == 0u &&
        t.state == RI_TR_STOPPED, "seek back");
    ri_tr_seek_bars(&t, &cur, 96u, -50, ri_seq_tick_of_bar(96u, 100u));
    RI_ASSERT(cur == 0u, "seek clamps front");
    ri_tr_seek_bars(&t, &cur, 96u, 500, ri_seq_tick_of_bar(96u, 100u));
    RI_ASSERT(cur == ri_seq_tick_of_bar(96u, 100u), "seek clamps end");
    t.state = RI_TR_PLAYING;
    ri_tr_seek_bars(&t, &cur, 96u, 1, ri_seq_tick_of_bar(96u, 100u));
    RI_ASSERT(t.state == RI_TR_PLAYING, "seek keeps state");
    /* Display: 1-based, one-way. */
    {
        struct RIBarPos dp = ri_seq_bar_display(0u, 96u, 100u);
        RI_ASSERT(dp.bar == 1u && dp.beat == 1u && dp.sixteenth == 1u, "1.1.1");
        dp = ri_seq_bar_display(383u, 96u, 100u);
        RI_ASSERT(dp.bar == 1u && dp.beat == 4u && dp.sixteenth == 4u, "1.4.4");
        dp = ri_seq_bar_display(384u, 96u, 100u);
        RI_ASSERT(dp.bar == 2u && dp.beat == 1u && dp.sixteenth == 1u, "2.1.1");
        dp = ri_seq_bar_display(384u * 100u + 57u, 96u, 100u);
        RI_ASSERT(dp.bar == 100u, "past-end clamps %u", dp.bar);
        dp = ri_seq_bar_display(0u, 0u, 100u);
        RI_ASSERT(dp.bar == 1u && dp.beat == 1u && dp.sixteenth == 1u, "ppq0 disp");
    }

- [ ] **Step 2: Run test to verify it fails**

Run: `bash scripts/ri_build_host.sh test t58_transport 2>&1 | grep -E "implicit|error" | head -2`
Expected: FAIL with implicit declaration of `ri_seq_tick_of_bar`

- [ ] **Step 3: Write minimal implementation** (append to `transport.c`)

```c
static uint32_t tr_ppq(uint32_t ppq) { return ppq ? ppq : 96u; }
uint64_t ri_seq_tick_of_bar(uint32_t ppq, uint64_t bar) {
    return bar * 4u * (uint64_t)tr_ppq(ppq);
}
uint64_t ri_seq_bar_at_tick(uint64_t tick, uint32_t ppq) {
    return tick / (4u * (uint64_t)tr_ppq(ppq));
}
struct RIBarPos ri_seq_bar_display(uint64_t tick, uint32_t ppq, uint64_t song_bars) {
    struct RIBarPos d;
    uint64_t p = (uint64_t)tr_ppq(ppq);
    uint64_t inbar;
    if (song_bars > 0u && ri_seq_bar_at_tick(tick, (uint32_t)p) >= song_bars)
        tick = song_bars > 0u ? ri_seq_tick_of_bar((uint32_t)p, song_bars - 1u) : 0u;
    d.bar = (uint16_t)(ri_seq_bar_at_tick(tick, (uint32_t)p) + 1u);
    inbar = tick - ri_seq_tick_of_bar((uint32_t)p, (uint64_t)(d.bar - 1u));
    d.beat = (uint8_t)(inbar / p + 1u);
    d.sixteenth = (uint8_t)((inbar % p) / (p / 4u) + 1u);
    return d;
}
void ri_tr_seek_bars(struct RITransport *t, uint64_t *cursor_ticks, uint32_t ppq,
                     int32_t delta_bars, uint64_t song_ticks) {
    int64_t bar;
    uint64_t song_bars;
    if (!t || !cursor_ticks)
        return;
    song_bars = ri_seq_bar_at_tick(song_ticks, ppq);
    bar = (int64_t)ri_seq_bar_at_tick(*cursor_ticks, ppq) + (int64_t)delta_bars;
    if (bar < 0)
        bar = 0;
    if ((uint64_t)bar > song_bars)
        bar = (int64_t)song_bars;
    *cursor_ticks = ri_seq_tick_of_bar(ppq, (uint64_t)bar);
    t->clicks = 0u; /* cursor intent replaces the stop sequence */
}
```
Note: `song_bars == 0` (empty song) seeks to bar 0 — tick 0. `p / 4u` is exact (ppq multiple of 4 per engine convention; ppq fallback 96 keeps it exact).

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
    if (loop->len_bars == 0u) {
        loop->on = 0u; /* zero length = loop off, never an empty region */
        return;
    }
    if (song_bars == 0u) {
        loop->on = 0u;
        loop->start_bar = 0u;
        loop->len_bars = 1u;
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
    p->loop = *loop;
    ri_loop_clamp(&p->loop, song_bars);
    p->apply_bar = bar_now + 1u;
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
Note: `song_bars ≤ 999` and bars fit `uint16_t` — callers pass clamped counts; struct fields bound the rest.

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
        sq.cursor_ticks = ri_seq_tick_of_bar(sq.ppq, 30u);
        ri_tr_stop(&sq.transport, &sq.cursor_ticks, 0ULL, 0ULL);
        ri_tr_stop(&sq.transport, &sq.cursor_ticks, 0ULL, 0ULL);
        RI_ASSERT(sq.transport.clicks == 2u && sq.cursor_ticks == 0ULL, "struct cycle");
    }
```

- [ ] **Step 2: Run test to verify it fails**

Run: `bash scripts/ri_build_host.sh test t58_transport 2>&1 | grep -E "no member|error" | head -2`
Expected: FAIL with `has no member named 'transport'`

- [ ] **Step 3: Write minimal implementation**

In `engine/seq/riseq.h`, add `#include "engine/seq/transport.h"` and extend the struct:
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

- [ ] **Step 3: Prove the new gate is load-bearing** (plan convention: every grep gate gets a known-bad check; t58 has no grep gates, so prove the test wiring instead)

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

- [ ] Full §1 transition matrix green (3-stop, punch in/out, click law, null-safe).
- [ ] Seeks clamp at song bounds, zero clicks, keep state.
- [ ] Bar/tick round-trips exact; display pins 1.1.1 / 1.4.4 / 2.1.1 / past-end clamp / ppq-0 fallback.
- [ ] Loop clamp + next-bar staging green; song-shorten path reuses the same clamp.
- [ ] RISeq init neutral; struct-driven cycle green.
- [ ] Mutation proof recorded; `ri_audit.sh` 0/0; tree clean.
