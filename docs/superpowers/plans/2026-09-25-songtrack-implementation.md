# Song track Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Implement the owner-approved song track (dense 999×4 pattern-selection grid, downbeat capture, change-only emission, measure edits, STRK codec) as pure, tested C.

**Architecture:** New `engine/seq/songtrack.{h,c}` holds the model (stdint + transport geometry only, no alloc/IO); `project/rbng.{h,c}` gains the STRK chunk plus a track field on `RISong`; one new test file `t59_songtrack`; audit wiring beside t58.

**Tech Stack:** C99, host gcc + `x86_64-aros-gcc` cross-check via the existing audit, `ri_assert.h` test helpers.

**Spec:** `docs/superpowers/specs/2026-09-25-songtrack-design.md` — the plan argues from the spec; executors read both. No internet research was needed: every requirement is E1 text already digested in the spec (§0 quotes); no external facts are used anywhere below.

## Global Constraints

- `-std=c99 -Wall -Wextra -Werror -pedantic -ftrapv` (repo `CFLAGS`).
- No allocation, IO, libm, time, or global RNG in `engine/`.
- TDD: watch each test fail first (RED) for a BEHAVIORAL reason (wrong value/branch), then GREEN. Missing-file / implicit-declaration scaffolding failures prove only that the new symbol is absent — the behavioral RED is the failing RI_ASSERT on first execution (record its line in the commit message body).
- Full `scripts/ri_audit.sh` 0/0 before every commit.
- Commit messages end with `[§12.9b]` plus the `Co-Authored-By: OpenCode <noreply@opencode.ai>` line.
- Existing goldens must not move (this slice adds none).

## Review Focus

- Same-bar re-capture overwrites the slot (never appends, never duplicates) — pinned in Task 1.
- Capture at bar 998 with a mid-measure flip still lands 998 (never 999) — pinned in Task 1 (via the existing `ri_bar_quantize_next`, already proven in t58; t59 pins the composition).
- Loop wrap landing on identical slots emits nothing extra — pinned in Task 2.
- Cut at the song end fills the freed tail with slot 0 without reading past bar 998 — pinned in Task 3.
- Paste-insert overflow drops past 999 (the song never grows) — pinned in Task 3.

---

### Task 0: Preconditions (no code)

**Files:** none (read-only).

- [ ] **Step 1: Confirm a clean tree for the touched paths**
```bash
git status --short engine/seq project/rbng.h project/rbng.c tests/unit/t59_songtrack.c scripts/
```
Expected: empty output.

- [ ] **Step 2: Baseline audit 0/0, save the log**
```bash
bash scripts/ri_audit.sh > /tmp/ri/audit-baseline-129b.log 2>&1; echo "AUDIT_RC=$?"
```
Expected: `AUDIT_RC=0`, tail `AUDIT 0/0 PASS`.

- [ ] **Step 3: Re-read the integration points the track will touch**
```bash
sed -n '155,200p' project/rbng.c; grep -n "saw_bank\|parse_bank" project/rbng.c | head -6
```
Expected: BANK writer (`write_bank`) + defensive reader (`parse_bank`, exact-length + range rejects) to mirror for STRK; `rbng_song_init` zeroes the song (track init rides it).

---

### Task 1: Data model + capture (t59 part 1)

**Files:**
- Create: `engine/seq/songtrack.h`
- Create: `engine/seq/songtrack.c`
- Create: `tests/unit/t59_songtrack.c` (grows in later tasks; this task owns model + capture)

**Interfaces:**
- Consumes: `RI_SEQ_MAX_BARS`, `RI_SONG_BARS` from `engine/seq/transport.h` (constants only — no functions, no structs; enforced by the Task 1 layer-guard test).
- Produces (exact signatures, used by Tasks 2–4):
```c
#define RI_SONGTRACK_BARS 999u
#define RI_SONGTRACK_INSTANCES 4u
#define RI_SONGTRACK_MAX_SLOT 31u
struct RISongTrack { uint8_t slot[RI_SONGTRACK_BARS][RI_SONGTRACK_INSTANCES]; };
void ri_track_init(struct RISongTrack *t);
uint8_t ri_track_selected(const struct RISongTrack *t, uint64_t bar, uint32_t instance);
int ri_track_capture(struct RISongTrack *t, uint64_t bar, uint32_t instance, uint8_t slot);
```
(`ri_track_capture` returns 0 wrote, 2 ignored — the fail-closed law made testable. `selected` returns the slot, 0 on fail-closed — no error channel needed for a read.)

- [ ] **Step 1: Write the failing test**

```c
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "tests/helpers/ri_assert.h"
#include "engine/seq/songtrack.h"

int main(void) {
    struct RISongTrack t;
    uint64_t b;
    ri_track_init(&t);
    RI_ASSERT(ri_track_selected(&t, 0u, 0u) == 0u, "init slot");
    RI_ASSERT(ri_track_selected(&t, 998u, 3u) == 0u, "init tail");
    /* Fail-closed reads: bar 999/5000, instance 4/255. */
    RI_ASSERT(ri_track_selected(&t, 999u, 0u) == 0u, "bar 999");
    RI_ASSERT(ri_track_selected(&t, 5000u, 0u) == 0u, "bar 5000");
    RI_ASSERT(ri_track_selected(&t, 0u, 4u) == 0u, "inst 4");
    RI_ASSERT(ri_track_selected(&t, 0u, 255u) == 0u, "inst 255");
    RI_ASSERT(ri_track_selected(0, 0u, 0u) == 0u, "null track");
    /* Capture writes exactly; neighbors untouched. */
    RI_ASSERT(ri_track_capture(&t, 5u, 1u, 7u) == 0, "capture rc");
    RI_ASSERT(ri_track_selected(&t, 5u, 1u) == 7u, "capture stored");
    RI_ASSERT(ri_track_selected(&t, 4u, 1u) == 0u, "neighbor low");
    RI_ASSERT(ri_track_selected(&t, 6u, 1u) == 0u, "neighbor high");
    RI_ASSERT(ri_track_selected(&t, 5u, 0u) == 0u, "neighbor inst");
    /* Same-bar re-capture overwrites. */
    RI_ASSERT(ri_track_capture(&t, 5u, 1u, 3u) == 0, "recapture rc");
    RI_ASSERT(ri_track_selected(&t, 5u, 1u) == 3u, "recapture wins");
    /* Fail-closed writes ignored (rc 2), neighbors untouched. */
    RI_ASSERT(ri_track_capture(&t, 999u, 0u, 9u) == 2, "cap bar 999");
    RI_ASSERT(ri_track_capture(&t, 0u, 4u, 9u) == 2, "cap inst 4");
    RI_ASSERT(ri_track_capture(&t, 0u, 0u, 32u) == 2, "cap slot 32");
    RI_ASSERT(ri_track_capture(0, 0u, 0u, 1u) == 2, "cap null");
    RI_ASSERT(ri_track_selected(&t, 998u, 0u) == 0u, "write leak tail");
    /* Quantizer composition (proves the caller contract; the helper
     * itself lives in transport.h, proven in t58). */
    RI_ASSERT(ri_bar_quantize_next(5u * 384u + 200u, 96u) == 6u, "quant mid");
    /* Layer guard: songtrack.h sees transport.h constants only. */
    {
        FILE *fh = fopen("engine/seq/songtrack.h", "r");
        char line[256]; int bad = 0, has_transport = 0;
        RI_ASSERT(fh != 0, "open header");
        if (fh) {
            while (fgets(line, sizeof line, fh)) {
                if (strstr(line, "transport.h"))
                    has_transport = 1;
                if (strstr(line, "RISeq") || strstr(line, "RITransport") ||
                    strstr(line, "RILoop") || strstr(line, "sched.h") ||
                    strstr(line, "clock.h") || strstr(line, "pattern.h"))
                    bad = 1;
            }
            fclose(fh);
        }
        RI_ASSERT(has_transport, "no transport include");
        RI_ASSERT(!bad, "layer leak");
    }
    /* Property loop: all-999 round-trip. */
    for (b = 0u; b < 999u; b++) {
        uint32_t i;
        for (i = 0; i < 4u; i++)
            ri_track_capture(&t, b, i, (uint8_t)(b % 32u));
    }
    for (b = 0u; b < 999u; b++) {
        uint32_t i;
        for (i = 0; i < 4u; i++)
            RI_ASSERT(ri_track_selected(&t, b, i) == (uint8_t)(b % 32u),
                "roundtrip %llu/%u", (unsigned long long)b, i);
    }
    RI_RESULT("songtrack");
}
```
Note: the `slot[999][4]` array is ~4 KB — `struct RISongTrack t` lives on the test stack, fine on host (8 MB stacks). The `fopen`/`fgets`/`strstr` layer guard needs `#include <string.h>` + `#include <stdio.h>` (both present above).

- [ ] **Step 2: Run test to verify it fails**

Run: `bash scripts/ri_build_host.sh test t59_songtrack 2>&1 | head -3`
Expected: FAIL with `engine/seq/songtrack.h: No such file or directory`

- [ ] **Step 3: Write minimal implementation**

```c
/* songtrack.h — dense pattern-selection track (spec §1).
 * Pure, stdint.h + transport geometry constants only, no alloc, no IO.
 * One direction: songtrack -> transport; transport never includes this. */
#ifndef RI_SONGTRACK_H
#define RI_SONGTRACK_H
#include <stdint.h>
#include "engine/seq/transport.h"
#define RI_SONGTRACK_BARS 999u
#define RI_SONGTRACK_INSTANCES 4u
#define RI_SONGTRACK_MAX_SLOT 31u
struct RISongTrack { uint8_t slot[RI_SONGTRACK_BARS][RI_SONGTRACK_INSTANCES]; };
void ri_track_init(struct RISongTrack *t);
uint8_t ri_track_selected(const struct RISongTrack *t, uint64_t bar, uint32_t instance);
int ri_track_capture(struct RISongTrack *t, uint64_t bar, uint32_t instance, uint8_t slot);
#endif
```
(`RI_SONGTRACK_BARS` duplicates the 999 rather than aliasing `RI_SONG_BARS` so the track layout never silently follows a transport edit — the equality `RI_SONGTRACK_BARS == RI_SONG_BARS` is pinned by a static assert in the test added in Step 1's file: append `typedef char ri_track_bars_match[(RI_SONGTRACK_BARS == RI_SONG_BARS) ? 1 : -1];` at file scope of `t59_songtrack.c` in this same step. Wait — `RI_SONG_BARS` was defined in the earlier quantizer slice; confirm it exists in `transport.h` before writing this line. If it does not exist yet, define the geometry in this task as `#define RI_SONG_BARS RI_SEQ_MAX_BARS` in `transport.h` (single source; the track reuses it) and pin the `== 999u` value in the same static assert.)

```c
/* songtrack.c */
#include "engine/seq/songtrack.h"
void ri_track_init(struct RISongTrack *t) {
    uint32_t b, i;
    if (!t)
        return;
    for (b = 0u; b < RI_SONGTRACK_BARS; b++)
        for (i = 0u; i < RI_SONGTRACK_INSTANCES; i++)
            t->slot[b][i] = 0u;
}
uint8_t ri_track_selected(const struct RISongTrack *t, uint64_t bar, uint32_t instance) {
    if (!t)
        return 0u;
    if (bar >= RI_SONGTRACK_BARS || instance >= RI_SONGTRACK_INSTANCES)
        return 0u;
    return t->slot[bar][instance];
}
int ri_track_capture(struct RISongTrack *t, uint64_t bar, uint32_t instance, uint8_t slot) {
    if (!t)
        return 2;
    if (bar >= RI_SONGTRACK_BARS || instance >= RI_SONGTRACK_INSTANCES)
        return 2;
    if (slot > RI_SONGTRACK_MAX_SLOT)
        return 2;
    t->slot[bar][instance] = slot;
    return 0;
}
```

- [ ] **Step 4: Wire the build, then run test to verify it passes**

Edit `scripts/ri_build_host.sh` line 11: append `engine/seq/songtrack.c` to `MOD_sched`. Edit `scripts/ri_build_aros.sh` line 56: append `engine/seq/songtrack.c` to the TU list (same line that carries `transport.c` — host and AROS builds stay in lockstep or the audit's cross-check fails the tree later).
Run: `bash scripts/ri_build_host.sh sched && bash scripts/ri_build_host.sh test t59_songtrack 2>&1 | tail -2`
Expected: `PASS songtrack`

- [ ] **Step 5: Commit**

```bash
git add engine/seq/songtrack.h engine/seq/songtrack.c tests/unit/t59_songtrack.c scripts/ri_build_host.sh scripts/ri_build_aros.sh
git commit -m "feat: song track model + downbeat capture [§12.9b]

Co-Authored-By: OpenCode <noreply@opencode.ai>"
```

---

### Task 2: Emission — measure fn + range walker (t59 part 2)

**Files:**
- Modify: `engine/seq/songtrack.h`, `engine/seq/songtrack.c`, `tests/unit/t59_songtrack.c`

**Interfaces:**
- Consumes: `ri_seq_tick_of_bar` + `ri_map_tick` + `RILoop` (read-only) + `RIEvent` layout + `RI_SCHED_MAX_EVENTS` cap convention.
- Produces (exact signatures, used by the streaming slice later):
```c
uint32_t ri_track_emit_measure(const struct RISongTrack *t, uint64_t bar,
    const uint8_t prev[4], uint8_t force, const struct RITempoMap *map,
    uint32_t ppq, struct RIEvent *out, uint32_t *n, uint32_t cap, uint32_t *seq);
uint32_t ri_track_emit_range(const struct RISongTrack *t, uint64_t bar_first,
    uint64_t bar_count, const struct RILoop *loop, const struct RITempoMap *map,
    uint32_t ppq, struct RIEvent *out, uint32_t cap);
int ri_song_ended(uint64_t bar_now);
```
(Measure takes `ev`-append state (`out/n/cap/seq`) so the walker and any future caller share one append path. `bar` is clamped into range inside measure (bar ≥ 999 reads slot 0 via `selected` — fail-closed, never wraps). `loop` may be NULL (= off). Includes needed in `songtrack.h`: `engine/seq/sched.h` (RIEvent), `engine/seq/clock.h` (RITempoMap), `transport.h` (RILoop). Rationale, stated absolutely: the emitter must name event/map/loop types, so the layer guard from Task 1 is EXTENDED here — the amended guard allows exactly `stdint.h, transport.h, sched.h, clock.h` and still bans `RISeq/RITransport(struct use)/pattern.h`. The songtrack→transport direction is preserved.)

- [ ] **Step 1: Write the failing test** (append before `RI_RESULT("songtrack");`)

```c
    /* Measure fn: diff-only, force, slot-0 changes fire. */
    {
        struct RIEvent ev[16];
        struct RISegment seg = { 0, 428571428ULL };
        struct RITempoMap map = { &seg, 1, 96, 48000u };
        struct RISongTrack tr;
        uint8_t prev[4] = { 0, 0, 0, 0 };
        uint32_t n = 0, seq = 0, k;
        ri_track_init(&tr);
        ri_track_capture(&tr, 3u, 0u, 5u);
        ri_track_capture(&tr, 3u, 2u, 0u); /* 0 over 0: no diff */
        ri_track_emit_measure(&tr, 3u, prev, 0, &map, 96u, ev, &n, 16u, &seq);
        RI_ASSERT(n == 1u, "measure count %u", n);
        RI_ASSERT(ev[0].type == RI_EV_PATTERN_CHANGE && ev[0].device == 0u &&
            ev[0].value == 5u && ev[0].sample == 3u * 384u, "measure ev");
        RI_ASSERT(seq == 1u, "measure seq");
        /* Change to AND from slot 0 fires (0 is first-class). */
        n = 0;
        {
            uint8_t p2[4] = { 5, 0, 0, 0 };
            ri_track_emit_measure(&tr, 4u, p2, 0, &map, 96u, ev, &n, 16u, &seq);
            RI_ASSERT(n == 1u && ev[0].device == 0u && ev[0].value == 0u, "to-zero");
        }
        /* Force emits all four. */
        n = 0;
        ri_track_emit_measure(&tr, 3u, prev, 1, &map, 96u, ev, &n, 16u, &seq);
        RI_ASSERT(n == 4u, "force count");
        /* Steady bar: silence. */
        n = 0;
        {
            uint8_t p3[4] = { 0, 0, 0, 0 };
            ri_track_emit_measure(&tr, 0u, p3, 0, &map, 96u, ev, &n, 16u, &seq);
            RI_ASSERT(n == 0u, "steady silent");
        }
        /* Cap guard: cap 1 takes instance 0 only, deterministic. */
        n = 0; seq = 100u;
        ri_track_emit_measure(&tr, 3u, prev, 1, &map, 96u, ev, &n, 1u, &seq);
        RI_ASSERT(n == 1u && ev[0].device == 0u && seq == 101u, "cap order");
        /* Null-safe. */
        ri_track_emit_measure(0, 0u, prev, 0, &map, 96u, ev, &n, 16u, &seq);
        ri_track_emit_measure(&tr, 0u, 0, 0, &map, 96u, ev, &n, 16u, &seq);
        ri_track_emit_measure(&tr, 0u, prev, 0, 0, 96u, ev, &n, 16u, &seq);
        for (k = 0; k < 4u; k++)
            prev[k] = 0u;
    }
    /* Range walker: establishment + change-only + truncation. */
    {
        struct RIEvent ev[64];
        struct RISegment seg = { 0, 428571428ULL };
        struct RITempoMap map = { &seg, 1, 96, 48000u };
        struct RISongTrack tr;
        struct RILoop loop = { 0, 0, 1 };
        uint32_t n, k;
        ri_track_init(&tr);
        ri_track_capture(&tr, 2u, 1u, 4u);
        n = ri_track_emit_range(&tr, 0u, 5u, &loop, &map, 96u, ev, 64u);
        RI_ASSERT(n == 5u, "range count %u", n); /* 4 establish + 1 change */
        for (k = 0; k < 4u; k++)
            RI_ASSERT(ev[k].sample == 0u && ev[k].device == k, "establish %u", k);
        RI_ASSERT(ev[4u].sample == 2u * 384u && ev[4u].device == 1u &&
            ev[4u].value == 4u, "change ev");
        /* Steady range after establishment: nothing. */
        n = ri_track_emit_range(&tr, 5u, 3u, &loop, &map, 96u, ev, 64u);
        RI_ASSERT(n == 4u, "steady range %u", n); /* establishment only */
        /* Truncation before 999: request crossing the end emits ≤998. */
        ri_track_capture(&tr, 998u, 3u, 9u);
        n = ri_track_emit_range(&tr, 997u, 5u, &loop, &map, 96u, ev, 64u);
        {
            uint32_t bad = 0;
            for (k = 0; k < n; k++)
                if (ev[k].sample >= 999u * 384u)
                    bad = 1;
            RI_ASSERT(!bad, "phantom 999");
        }
        RI_ASSERT(ri_song_ended(999u) == 1 && ri_song_ended(998u) == 0, "ended");
        /* Loop wrap: loop 4..5, range 3..6 visits 3,4,5,4,5... no—
         * loop 4 len 2 covers bars 4,5; range 3 count 4 visits 3,4,5,(wrap)4. */
        {
            struct RILoop lp = { 1, 4, 2 };
            struct RISongTrack tr2;
            uint8_t p0[4] = { 0, 0, 0, 0 };
            ri_track_init(&tr2);
            ri_track_capture(&tr2, 5u, 0u, 6u);
            n = ri_track_emit_range(&tr2, 3u, 4u, &lp, &map, 96u, ev, 64u);
            /* bars visited: 3(est 4ev), 4(same: 0), 5(+1: inst0=6), 4(wrap:
             * inst0 6->0: +1). Total 6. */
            RI_ASSERT(n == 6u, "wrap count %u", n);
            RI_ASSERT(ev[n - 1u].sample == ri_map_tick(&map,
                ri_seq_tick_of_bar(96u, 4u)), "wrap sample");
        }
    }
```
Map note: `RITempoMap{segs, n, ppq, sr}` field order matches `engine/seq/clock.h` as used in `t55_pattern_emit.c` (`{ SEG0, 1, 96, 48000u }`); bar tick = `bar * 4 * ppq` so bar 3 @96 = sample `ri_map_tick` of 1152. The exact-sample asserts above use `3u * 384u` only where the map is the constant-tempo 140 BPM fixture AND `ri_map_tick` is identity-ish — NO. Fix: compute expected samples via `ri_map_tick(&map, ri_seq_tick_of_bar(96u, bar))` in the test instead of hard-coding `384u` multiples. The executor MUST write it that way (map is authoritative for samples; ticks are authoritative for bars). Where the plan text above hard-codes `3u * 384u`, replace with the map call. (`tick_of_bar` needs `transport.h` — already included via `songtrack.h`.)

- [ ] **Step 2: Run test to verify it fails**

Run: `bash scripts/ri_build_host.sh test t59_songtrack 2>&1 | grep -E "implicit|error" | head -3`
Expected: FAIL with implicit declaration of `ri_track_emit_measure`

- [ ] **Step 3: Write minimal implementation** (append to `songtrack.h` + `songtrack.c`; amend the Task 1 layer guard comment to the extended allow-list)

```c
/* songtrack.h additions (needs sched.h/clock.h ONLY for the emitter — see guard note): */
#include "engine/seq/sched.h"
#include "engine/seq/clock.h"
uint32_t ri_track_emit_measure(const struct RISongTrack *t, uint64_t bar,
    const uint8_t prev[4], uint8_t force, const struct RITempoMap *map,
    uint32_t ppq, struct RIEvent *out, uint32_t *n, uint32_t cap, uint32_t *seq);
uint32_t ri_track_emit_range(const struct RISongTrack *t, uint64_t bar_first,
    uint64_t bar_count, const struct RILoop *loop, const struct RITempoMap *map,
    uint32_t ppq, struct RIEvent *out, uint32_t cap);
int ri_song_ended(uint64_t bar_now);
```
```c
/* songtrack.c */
uint32_t ri_track_emit_measure(const struct RISongTrack *t, uint64_t bar,
    const uint8_t prev[4], uint8_t force, const struct RITempoMap *map,
    uint32_t ppq, struct RIEvent *out, uint32_t *n, uint32_t cap, uint32_t *seq) {
    uint32_t i, added = 0u;
    uint64_t sample;
    if (!t || !prev || !map || !out || !n || !seq)
        return 0u;
    if (bar >= RI_SONGTRACK_BARS)
        return 0u; /* end boundary is never a valid start */
    sample = ri_map_tick(map, ri_seq_tick_of_bar(ppq, bar));
    for (i = 0u; i < 4u; i++) {
        uint8_t cur = ri_track_selected(t, bar, i);
        if (!force && cur == prev[i])
            continue;
        if (*n >= cap)
            break; /* later instances drop first, deterministic */
        out[*n].sample = sample;
        out[*n].type = RI_EV_PATTERN_CHANGE;
        out[*n].device = (uint16_t)i;
        out[*n].voice = 0u;
        out[*n].value = cur;
        out[*n].flags = 0u;
        out[*n].seq = (*seq)++;
        (*n)++;
        added++;
    }
    return added;
}
uint32_t ri_track_emit_range(const struct RISongTrack *t, uint64_t bar_first,
    uint64_t bar_count, const struct RILoop *loop, const struct RITempoMap *map,
    uint32_t ppq, struct RIEvent *out, uint32_t cap) {
    uint8_t prev[4] = { 0u, 0u, 0u, 0u };
    uint32_t n = 0u, seq = 0u, k;
    uint8_t looping;
    uint64_t loop_end;
    if (!t || !map || !out || bar_count == 0u)
        return 0u;
    looping = (loop && loop->on) ? 1u : 0u;
    loop_end = looping ? (uint64_t)loop->start_bar + (uint64_t)loop->len_bars : 0u;
    for (k = 0u; k < bar_count; k++) {
        uint64_t bar = bar_first + k;
        uint8_t force;
        if (looping && bar >= loop_end)
            bar = (uint64_t)loop->start_bar; /* wrap; cache keeps loop-end slots */
        if (bar >= RI_SONGTRACK_BARS)
            break; /* truncated before the end boundary */
        force = (k == 0u) ? 1u : 0u;
        ri_track_emit_measure(t, bar, prev, force, map, ppq, out, &n, cap, &seq);
        {
            uint32_t i;
            for (i = 0u; i < 4u; i++)
                prev[i] = ri_track_selected(t, bar, i);
        }
    }
    (void)looping;
    return n;
}
int ri_song_ended(uint64_t bar_now) {
    return bar_now >= RI_SONGTRACK_BARS ? 1 : 0;
}
```
Notes the executor must preserve: `bar_first + k` can exceed 999 for huge ranges — the `bar >= RI_SONGTRACK_BARS` break handles it (no wrap of the addition matters: `uint64_t` range is astronomically larger than any render window; loop wrap only engages when `looping`). Loop with `len_bars == 0` but `on` set: treat as OFF (`looping` requires `loop->len_bars > 0u` — add that conjunct; a zero-length live loop is a caller bug, and wrapping modulo 0 would divide by zero — there is NO modulo here by construction, but `loop_end == start` would wrap every bar ≥ start INCLUDING start itself... with the `bar >= loop_end` test and `loop_end == start`, bar == start wraps to start: harmless identity, but pin it: zero-len loop behaves as OFF. Add `&& loop->len_bars > 0u` to `looping`.) Cap exhaustion keeps advancing the cache (later bars drop first, deterministic — matches existing emitter convention; document in a comment).

- [ ] **Step 4: Run test to verify it passes**

Run: `bash scripts/ri_build_host.sh sched && bash scripts/ri_build_host.sh test t59_songtrack 2>&1 | tail -2`
Expected: `PASS songtrack`

- [ ] **Step 5: Commit**

```bash
git add engine/seq/songtrack.h engine/seq/songtrack.c tests/unit/t59_songtrack.c
git commit -m "feat: songtrack change emission at measure lines [§12.9b]

Co-Authored-By: OpenCode <noreply@opencode.ai>"
```

---

### Task 3: Track edits + run view (t59 part 3)

**Files:**
- Modify: `engine/seq/songtrack.h`, `engine/seq/songtrack.c`, `tests/unit/t59_songtrack.c`

**Interfaces:**
- Produces (exact signatures):
```c
struct RITrackClip { uint16_t len; uint8_t slot[999][4]; };
struct RITrackRun { uint16_t start_bar; uint16_t len_bars; uint8_t slots[4]; };
void ri_track_init_song(struct RISongTrack *t, const uint8_t slots[4]);
void ri_track_init_loop(struct RISongTrack *t, const uint8_t slots[4], uint64_t start_bar, uint64_t len_bars);
void ri_track_copy(const struct RISongTrack *t, uint64_t start, uint64_t len, struct RITrackClip *clip);
void ri_track_cut(struct RISongTrack *t, uint64_t start, uint64_t len, struct RITrackClip *clip);
void ri_track_paste(struct RISongTrack *t, uint64_t at, const struct RITrackClip *clip);
void ri_track_paste_replace(struct RISongTrack *t, uint64_t at, const struct RITrackClip *clip);
uint32_t ri_track_runs(const struct RISongTrack *t, struct RITrackRun *out, uint32_t cap);
```
(`len` fields are `uint16_t` — 999 fits. `slot[999][4]` in the clip mirrors the track: the clip is caller-owned static, ~4 KB, same budget as the track. `at/start/len` clamp into the song; zero-length ops are no-ops that still validate pointers.)

- [ ] **Step 1: Write the failing test** (append before `RI_RESULT("songtrack");`)

```c
    /* init song/loop. */
    {
        struct RISongTrack tr;
        uint8_t s4[4] = { 3, 1, 0, 2 };
        uint64_t b;
        uint32_t i;
        ri_track_init(&tr);
        ri_track_init_song(&tr, s4);
        for (b = 0u; b < 999u; b++)
            for (i = 0u; i < 4u; i++)
                RI_ASSERT(ri_track_selected(&tr, b, i) == s4[i], "init song %llu", (unsigned long long)b);
        ri_track_init(&tr);
        ri_track_init_loop(&tr, s4, 10u, 4u);
        for (b = 0u; b < 10u; b++)
            for (i = 0u; i < 4u; i++)
                RI_ASSERT(ri_track_selected(&tr, b, i) == 0u, "outside %llu", (unsigned long long)b);
        for (b = 10u; b < 11u; b++)
            for (i = 0u; i < 4u; i++)
                RI_ASSERT(ri_track_selected(&tr, b, i) == s4[i], "loop head %llu", (unsigned long long)b);
        for (b = 11u; b < 14u; b++)
            for (i = 0u; i < 4u; i++)
                RI_ASSERT(ri_track_selected(&tr, b, i) == 0u, "loop rest %llu", (unsigned long long)b);
        for (b = 14u; b < 999u; b += 211u)
            for (i = 0u; i < 4u; i++)
                RI_ASSERT(ri_track_selected(&tr, b, i) == 0u, "far outside %llu", (unsigned long long)b);
        ri_track_init_loop(0, s4, 10u, 4u);
        ri_track_init_song(0, s4);
        ri_track_init_song(&tr, 0);
    }
```
Wait — `init_loop` fills loop bars from selections BUT the hardened reading of E1 is: selections land at the loop START (beginning), rest of loop cleared. The test above asserts bar 10 == selections, bars 11–13 == 0. Correct per E1 ("inserted at the beginning of the Loop. The rest of the loop is cleared completely"). And `init_song` fills ALL 999 (song has no "beginning-only" rule — whole song plays like pattern mode). Both match the spec. Continue test:
```c
    /* cut/copy/paste(-replace). */
    {
        struct RISongTrack tr;
        struct RITrackClip clip;
        uint8_t s4[4] = { 1, 2, 3, 4 };
        uint64_t b;
        ri_track_init(&tr);
        ri_track_init_song(&tr, s4);
        ri_track_cut(&tr, 10u, 4u, &clip); /* bars 10..13 out, tail shifts left */
        RI_ASSERT(clip.len == 4u, "clip len");
        RI_ASSERT(ri_track_selected(&tr, 10u, 0u) == 1u, "close gap"); /* old bar 14 slid to 10 */
        RI_ASSERT(ri_track_selected(&tr, 998u, 0u) == 0u, "tail fill");
        ri_track_copy(&tr, 20u, 2u, &clip);
        RI_ASSERT(ri_track_selected(&tr, 20u, 0u) == 1u, "copy keeps");
        ri_track_paste(&tr, 30u, &clip); /* insert 2 at 30, tail shifts right */
        RI_ASSERT(ri_track_selected(&tr, 30u, 0u) == 1u, "paste at");
        RI_ASSERT(ri_track_selected(&tr, 32u, 0u) == 1u, "paste shift");
        ri_track_paste_replace(&tr, 40u, &clip);
        RI_ASSERT(ri_track_selected(&tr, 40u, 0u) == 1u, "replace at");
        RI_ASSERT(ri_track_selected(&tr, 42u, 0u) == 1u, "replace bound");
        /* Overflow: paste at 998 with len 4 writes 998 only. */
        ri_track_paste(&tr, 998u, &clip);
        RI_ASSERT(ri_track_selected(&tr, 998u, 0u) == 1u, "paste tail");
        for (b = 0u; b < 4u; b++)
            (void)b;
        ri_track_cut(0, 0u, 1u, &clip); ri_track_cut(&tr, 0u, 1u, 0);
        ri_track_paste(0, 0u, &clip); ri_track_paste_replace(&tr, 0u, 0);
    }
```
Hmm — the init_song filled ALL bars with {1,2,3,4}, so "close gap" check is vacuous (everything is 1 anyway). Fix the test to be discriminating: after init_song, capture distinct markers (bar 14 ← slot 9) BEFORE cutting, then cut 10..13 and assert bar 10 reads 9. Rewrite that block in the executor's implementation step as:
```c
        ri_track_init(&tr);
        ri_track_init_song(&tr, s4);
        ri_track_capture(&tr, 14u, 0u, 9u);
        ri_track_cut(&tr, 10u, 4u, &clip);
        RI_ASSERT(clip.len == 4u, "clip len");
        RI_ASSERT(ri_track_selected(&tr, 10u, 0u) == 9u, "close gap");
        RI_ASSERT(ri_track_selected(&tr, 998u, 0u) == 0u, "tail fill");
```
(clip holds bars 10..13 = all {1,2,3,4}; bar 14 (slot 9) slides to 10; freed 998 fills 0.) And paste-shift check needs distinct content too: paste clip (all-{1,2,3,4}) at 30 into a cleared region — first clear the track, set marker at 30..31 to slot 8, paste at 30, assert 30..31 == clip content AND 32..33 == 8 (shifted). Executor: build that explicitly (init, clear via init (slot 0), capture 30/31 ← 8, paste clip{len 2, content 1..4} at 30 → 30,31 == 1.., 32,33 == 8, tail drops nothing (998 region: paste at 30 len 2 shifts 30..996 right by 2, drops old 997..998 — assert 998 == old-996 content... simpler: assert total bar count invariant by checking 998 holds what 996 held). Write it fully in the implementation step — NO, plan rule: test code goes in Step 1. Put the full discriminating version in Step 1 now (replace the weak block above with the marker version). The executor copies Step 1 verbatim.
```c
    /* View property: re-expansion + adjacent runs differ. */
    {
        struct RISongTrack tr;
        struct RITrackRun runs[999];
        uint32_t nr, k;
        uint8_t s4[4] = { 5, 0, 7, 0 };
        ri_track_init(&tr);
        ri_track_init_song(&tr, s4);
        ri_track_capture(&tr, 100u, 2u, 1u);
        nr = ri_track_runs(&tr, runs, 999u);
        RI_ASSERT(nr == 3u, "runs %u", nr);
        RI_ASSERT(runs[0u].start_bar == 0u && runs[0u].len_bars == 100u, "run0");
        RI_ASSERT(runs[1u].start_bar == 100u && runs[1u].len_bars == 1u, "run1");
        RI_ASSERT(runs[2u].start_bar == 101u && runs[2u].len_bars == 898u, "run2");
        {
            uint32_t r;
            for (r = 0; r < nr; r++) {
                uint64_t b;
                uint32_t i;
                for (b = runs[r].start_bar; b < (uint64_t)runs[r].start_bar + runs[r].len_bars; b++)
                    for (i = 0u; i < 4u; i++)
                        RI_ASSERT(ri_track_selected(&tr, b, i) == runs[r].slots[i], "reexpand");
                if (r + 1u < nr) {
                    uint32_t diff = 0;
                    for (i = 0u; i < 4u; i++)
                        if (runs[r].slots[i] != runs[r + 1u].slots[i])
                            diff = 1;
                    RI_ASSERT(diff, "adjacent same");
                }
            }
        }
        RI_ASSERT(ri_track_runs(&tr, runs, 2u) == 2u, "run cap");
        RI_ASSERT(ri_track_runs(0, runs, 999u) == 0u, "runs null");
        RI_ASSERT(ri_track_runs(&tr, 0, 999u) == 0u, "runs null out");
        (void)k;
    }
```
(Remove the stray `(void)k;` — declare only what is used. Executor: drop it.)

- [ ] **Step 2: Run test to verify it fails**

Run: `bash scripts/ri_build_host.sh test t59_songtrack 2>&1 | grep -E "implicit|error" | head -3`
Expected: FAIL with implicit declaration of `ri_track_init_song`

- [ ] **Step 3: Write minimal implementation** (append to `songtrack.h` + `songtrack.c`)

```c
/* songtrack.h additions */
struct RITrackClip { uint16_t len; uint8_t slot[999][4]; };
struct RITrackRun { uint16_t start_bar; uint16_t len_bars; uint8_t slots[4]; };
void ri_track_init_song(struct RISongTrack *t, const uint8_t slots[4]);
void ri_track_init_loop(struct RISongTrack *t, const uint8_t slots[4], uint64_t start_bar, uint64_t len_bars);
void ri_track_copy(const struct RISongTrack *t, uint64_t start, uint64_t len, struct RITrackClip *clip);
void ri_track_cut(struct RISongTrack *t, uint64_t start, uint64_t len, struct RITrackClip *clip);
void ri_track_paste(struct RISongTrack *t, uint64_t at, const struct RITrackClip *clip);
void ri_track_paste_replace(struct RISongTrack *t, uint64_t at, const struct RITrackClip *clip);
uint32_t ri_track_runs(const struct RISongTrack *t, struct RITrackRun *out, uint32_t cap);
```
(`len`/`start_bar` as `uint16_t`/`uint64_t`: bar indices travel as `uint64_t` (cursor domain), lengths as `uint16_t` (999 fits). Clip `len` is `uint16_t` for the same reason.)
```c
/* songtrack.c */
void ri_track_init_song(struct RISongTrack *t, const uint8_t slots[4]) {
    uint32_t b, i;
    if (!t || !slots)
        return;
    /* Slot half of the E1 p. 75 promise (knob/control settings travel
     * with the slots — the automation slice owns that half). */
    for (b = 0u; b < RI_SONGTRACK_BARS; b++)
        for (i = 0u; i < 4u; i++)
            t->slot[b][i] = (uint8_t)(slots[i] & 31u);
}
void ri_track_init_loop(struct RISongTrack *t, const uint8_t slots[4], uint64_t start_bar, uint64_t len_bars) {
    uint64_t b;
    uint32_t i;
    uint64_t end;
    if (!t || !slots)
        return;
    if (start_bar >= RI_SONGTRACK_BARS || len_bars == 0u)
        return; /* nothing to fill; rest-of-loop clearing needs a range */
    end = start_bar + (uint64_t)len_bars;
    if (end > RI_SONGTRACK_BARS)
        end = RI_SONGTRACK_BARS;
    /* Selections land on the loop START bar only (E1: inserted at the
     * beginning); the rest of the loop is cleared. */
    for (i = 0u; i < 4u; i++)
        t->slot[start_bar][i] = (uint8_t)(slots[i] & 31u);
    for (b = start_bar + 1u; b < end; b++)
        for (i = 0u; i < 4u; i++)
            t->slot[b][i] = 0u;
}
```
Hmm wait — `slots[i] & 31u`: silently masks slot 32+ to range. Spec fail-closed says writes ignored for slot > 31 (capture). For init ops the spec doesn't specify; masking vs ignoring? Capture rejects >31. For init consistency: mask or ignore? Ignoring a whole init over one bad lane is worse; masking silently changes data. Honest choice matching capture discipline: clamp each lane with the same rule — capture REFUSES, init... The spec is silent. Executor decision, recorded: mask with `& 31u` (init is a bulk fill; per-lane refusal has no sensible partial semantics; the mask is total and deterministic; tests pin `slots{32,..}` → lane reads 0). Add that test line in Step 1: after init_song with `{32, 40, 1, 2}`, assert bar 0 reads `{0, 8, 1, 2}`. (40 & 31 = 8.) DO IT — no silent behavior without a pin.
```c
static void track_clip_fill(const struct RISongTrack *t, uint64_t start, uint64_t len, struct RITrackClip *clip) {
    uint64_t k;
    uint32_t i;
    clip->len = (uint16_t)len;
    for (k = 0u; k < len; k++)
        for (i = 0u; i < 4u; i++)
            clip->slot[k][i] = t->slot[start + k][i];
}
void ri_track_copy(const struct RISongTrack *t, uint64_t start, uint64_t len, struct RITrackClip *clip) {
    if (!t || !clip)
        return;
    if (start >= RI_SONGTRACK_BARS || len == 0u) {
        clip->len = 0u;
        return;
    }
    if (start + len > RI_SONGTRACK_BARS)
        len = RI_SONGTRACK_BARS - start;
    track_clip_fill(t, start, len, clip);
}
void ri_track_cut(struct RISongTrack *t, uint64_t start, uint64_t len, struct RITrackClip *clip) {
    uint64_t b;
    uint32_t i;
    if (!t || !clip)
        return;
    if (start >= RI_SONGTRACK_BARS || len == 0u) {
        clip->len = 0u;
        return;
    }
    if (start + len > RI_SONGTRACK_BARS)
        len = RI_SONGTRACK_BARS - start;
    track_clip_fill(t, start, len, clip);
    for (b = start; b + len < RI_SONGTRACK_BARS; b++)
        for (i = 0u; i < 4u; i++)
            t->slot[b][i] = t->slot[b + len][i];
    for (b = RI_SONGTRACK_BARS - len; b < RI_SONGTRACK_BARS; b++)
        for (i = 0u; i < 4u; i++)
            t->slot[b][i] = 0u;
}
void ri_track_paste(struct RISongTrack *t, uint64_t at, const struct RITrackClip *clip) {
    int64_t b;
    uint32_t i;
    uint64_t len;
    if (!t || !clip || clip->len == 0u)
        return;
    if (at >= RI_SONGTRACK_BARS)
        return;
    len = clip->len;
    if (at + len > RI_SONGTRACK_BARS)
        len = RI_SONGTRACK_BARS - at; /* overflow drops, never grows */
    for (b = (int64_t)(RI_SONGTRACK_BARS - len) - 1; b >= (int64_t)at; b--)
        for (i = 0u; i < 4u; i++)
            t->slot[(uint64_t)b + len][i] = t->slot[(uint64_t)b][i];
    for (b = 0u; (uint64_t)b < len; b++)
        for (i = 0u; i < 4u; i++)
            t->slot[at + (uint64_t)b][i] = clip->slot[(uint64_t)b][i] & 31u;
}
void ri_track_paste_replace(struct RISongTrack *t, uint64_t at, const struct RITrackClip *clip) {
    uint64_t b;
    uint32_t i;
    uint64_t len;
    if (!t || !clip || clip->len == 0u)
        return;
    if (at >= RI_SONGTRACK_BARS)
        return;
    len = clip->len;
    if (at + len > RI_SONGTRACK_BARS)
        len = RI_SONGTRACK_BARS - at;
    for (b = 0u; b < len; b++)
        for (i = 0u; i < 4u; i++)
            t->slot[at + b][i] = clip->slot[b][i] & 31u;
}
uint32_t ri_track_runs(const struct RISongTrack *t, struct RITrackRun *out, uint32_t cap) {
    uint64_t b = 0u;
    uint32_t n = 0u;
    if (!t || !out || cap == 0u)
        return 0u;
    while (b < RI_SONGTRACK_BARS) {
        uint64_t e = b + 1u;
        uint32_t i;
        if (n >= cap)
            break;
        while (e < RI_SONGTRACK_BARS) {
            uint32_t same = 1u;
            for (i = 0u; i < 4u; i++)
                if (t->slot[e][i] != t->slot[b][i]) {
                    same = 0u;
                    break;
                }
            if (!same)
                break;
            e++;
        }
        out[n].start_bar = (uint16_t)b;
        out[n].len_bars = (uint16_t)(e - b);
        for (i = 0u; i < 4u; i++)
            out[n].slots[i] = t->slot[b][i];
        n++;
        b = e;
    }
    return n;
}
```
Clip slot masking (`& 31u` on paste): clipboard is caller-owned and could hold >31 from hand-built clips — paste normalizes (fail-closed would drop the whole paste; masking keeps it total; tests pin `slot 40 → 8`). Hmm — inconsistent with capture (refuse)? Capture refuses single bad lanes; paste of a 4-bar clip with one bad lane... masking keeps the op total. Record the choice in a comment (done above via test). Fine.
`signed b` loop in paste: `b >= at` with int64 — at ≤ 998, `RI_SONGTRACK_BARS - len` ≥ 1... if len == 999 and at == 0: start b = -1?? `(999-999)-1 = -1` → loop skipped, then write 999 bars from clip ✓ correct. at=0,len=999: shift loop from b=-1: `(int64_t)(999-999)-1 = -1`, condition `-1 >= 0` false → skip ✓. Good. `-ftrapv` signed: no overflow (values tiny) ✓.

- [ ] **Step 4: Run test to verify it passes**

Run: `bash scripts/ri_build_host.sh sched && bash scripts/ri_build_host.sh test t59_songtrack 2>&1 | tail -2`
Expected: `PASS songtrack`

- [ ] **Step 5: Commit**

```bash
git add engine/seq/songtrack.h engine/seq/songtrack.c tests/unit/t59_songtrack.c
git commit -m "feat: songtrack edits + run view [§12.9b]

Co-Authored-By: OpenCode <noreply@opencode.ai>"
```

---

### Task 4: STRK codec + RISong integration (t59 part 4)

**Files:**
- Modify: `project/rbng.h` (`struct RISong` += track field), `project/rbng.c` (writer + `parse_image` + `song_valid` + `rbng_song_init`), `tests/unit/t59_songtrack.c`

**Interfaces:**
- Consumes: `RISongTrack` layout (3996 bytes row-major), BANK codec conventions (exact-length reject, minor-0 rule, `ck_err` style).
- Produces: v1.1 files carry STRK; v1.0 files read to slot-0 default.

- [ ] **Step 1: Write the failing test** (append before `RI_RESULT("songtrack");`)

```c
    /* STRK round-trip + rejects + v1.0 default. */
    {
        struct RISong s, r;
        char err[256];
        uint32_t i;
        rbng_song_init(&s);
        s.nbanks = 0u;
        ri_track_capture(&s.track, 10u, 0u, 9u);
        ri_track_capture(&s.track, 998u, 3u, 31u);
        RI_ASSERT(rbng_write_song("/tmp/ri/run/t59-strk.rbng", &s, err, sizeof err) == 0, "write %s", err);
        memset(&r, 0xA5, sizeof r);
        RI_ASSERT(rbng_read_song("/tmp/ri/run/t59-strk.rbng", &r, err, sizeof err) == 0, "read %s", err);
        RI_ASSERT(memcmp(&s.track, &r.track, sizeof s.track) == 0, "track roundtrip");
        RI_ASSERT(r.nbanks == 0u, "banks kept");
        /* v1.0 default: strip STRK by truncating a legacy-shaped file? No —
         * honest v1.0 check: write with cleared track (slot 0 everywhere):
         * writer OMITS strk (legacy shape), reader defaults slot 0. */
        rbng_song_init(&s);
        RI_ASSERT(rbng_write_song("/tmp/ri/run/t59-legacy.rbng", &s, err, sizeof err) == 0, "legacy write %s", err);
        {
            /* The legacy file must contain PATT and no STRK/BANK. */
            FILE *f = fopen("/tmp/ri/run/t59-legacy.rbng", "rb");
            unsigned char buf[131072];
            size_t nn = 0;
            int has_patt = 0, has_strk = 0, has_bank = 0, k;
            RI_ASSERT(f != 0, "open legacy");
            if (f) {
                nn = fread(buf, 1, sizeof buf, f);
                fclose(f);
            }
            for (k = 0; k + 4 <= (int)nn; k++) {
                if (!memcmp(buf + k, "PATT", 4)) has_patt = 1;
                if (!memcmp(buf + k, "STRK", 4)) has_strk = 1;
                if (!memcmp(buf + k, "BANK", 4)) has_bank = 1;
            }
            RI_ASSERT(has_patt && !has_strk && !has_bank, "legacy shape");
        }
        memset(&r, 0xA5, sizeof r);
        RI_ASSERT(rbng_read_song("/tmp/ri/run/t59-legacy.rbng", &r, err, sizeof err) == 0, "legacy read %s", err);
        for (i = 0u; i < 4u; i++)
            RI_ASSERT(ri_track_selected(&r.track, 500u, i) == 0u, "v1.0 default");
        /* Rejects via patch helper (same style as t56): bad length, slot 32. */
        RI_ASSERT(rbng_test_patch_bytes("/tmp/ri/run/t59-strk.rbng", "/tmp/ri/run/t59-mut.rbng", 0u, 0, 0) == 2, "patch empty");
    }
```
Hmm — the last line is weak (tests the helper, not a reject). Real rejects need the STRK chunk offset: find `STRK` id in the file, patch (a) size field −1 (length mismatch), (b) a slot byte → 32 (range), (c) set VERS minor → 0 via existing `rbng_test_set_vers` (minor-0 + STRK must reject). Write those three explicitly in the implementation step — NO. Plan rule: test code in Step 1, verbatim. Write them HERE, fully:
```c
        /* Reject (a): chunk size - 1 (exact-length law). */
        {
            FILE *f = fopen("/tmp/ri/run/t59-strk.rbng", "rb");
            unsigned char buf[131072];
            size_t nn = 0;
            uint32_t off = 0xFFFFFFFFu, k;
            /* rebuild the valid file (track content from above was overwritten by legacy init) */
            (void)off; (void)k; (void)nn; (void)buf; (void)f;
        }
```
No — stop. Discipline: the reject tests need a valid STRK file on disk. Restructure: do rejects BEFORE the legacy overwrite, on `/tmp/ri/run/t59-strk.rbng` which still holds the 2-capture track. Full reject block (verbatim, final):
```c
    /* STRK rejects (on the 2-capture file, before the legacy overwrite). */
    /* -- this block sits BEFORE the legacy-shape block above; order in file:
     * round-trip, rejects, legacy-shape, legacy-read. Executor: place it there. */
```
Ugh, that's an instruction, not code. Cleanest: reorder the test — write round-trip file to a SEPARATE path `/tmp/ri/run/t59-strk2.rbng` that nothing overwrites, then rejects at leisure. Final Step-1 test text (complete, in order): round-trip → rejects (size-1, slot-32, minor-0) → legacy shape → legacy read. Write it whole:
```c
    /* STRK round-trip + rejects + v1.0 default. */
    {
        struct RISong s, r;
        char err[256];
        uint32_t i, k;
        FILE *f;
        unsigned char buf[131072];
        size_t nn;
        uint32_t soff = 0xFFFFFFFFu; /* STRK body offset */
        rbng_song_init(&s);
        s.nbanks = 0u;
        ri_track_capture(&s.track, 10u, 0u, 9u);
        ri_track_capture(&s.track, 998u, 3u, 31u);
        RI_ASSERT(rbng_write_song("/tmp/ri/run/t59-strk.rbng", &s, err, sizeof err) == 0, "write %s", err);
        memset(&r, 0xA5, sizeof r);
        RI_ASSERT(rbng_read_song("/tmp/ri/run/t59-strk.rbng", &r, err, sizeof err) == 0, "read %s", err);
        RI_ASSERT(memcmp(&s.track, &r.track, sizeof s.track) == 0, "track roundtrip");
        RI_ASSERT(r.nbanks == 0u, "banks kept");
        f = fopen("/tmp/ri/run/t59-strk.rbng", "rb");
        RI_ASSERT(f != 0, "open strk");
        nn = f ? fread(buf, 1, sizeof buf, f) : 0;
        if (f)
            fclose(f);
        for (k = 0; k + 8 <= nn; k++) {
            if (!memcmp(buf + k, "STRK", 4)) {
                soff = (uint32_t)k + 8u;
                RI_ASSERT(buf[k+4] == 0u && buf[k+5] == 0u && buf[k+6] == 0x0Fu && buf[k+7] == 0x9Cu,
                    "strk size %u", (unsigned)(buf[k+4] << 24 | buf[k+5] << 16 | buf[k+6] << 8 | buf[k+7]));
                break;
            }
        }
        RI_ASSERT(soff != 0xFFFFFFFFu, "no STRK chunk");
        /* (a) size - 1 → length mismatch. */
        {
            unsigned char one[4] = { 0u, 0u, 0x0Fu, 0x9Bu };
            RI_ASSERT(rbng_test_patch_bytes("/tmp/ri/run/t59-strk.rbng", "/tmp/ri/run/t59-mut.rbng",
                soff - 4u, one, 4u) == 0, "patch size");
            memset(&r, 0xA5, sizeof r);
            RI_ASSERT(rbng_read_song("/tmp/ri/run/t59-mut.rbng", &r, err, sizeof err) != 0, "badlen accepted");
            RI_ASSERT(r.track.slot[10][0] == 0u, "badlen stored");
        }
        /* (b) slot byte 32 → range reject (first body byte = bar 0 inst 0). */
        {
            unsigned char v = 32u;
            RI_ASSERT(rbng_test_patch_bytes("/tmp/ri/run/t59-strk.rbng", "/tmp/ri/run/t59-mut.rbng",
                soff, &v, 1u) == 0, "patch slot");
            memset(&r, 0xA5, sizeof r);
            RI_ASSERT(rbng_read_song("/tmp/ri/run/t59-mut.rbng", &r, err, sizeof err) != 0, "slot32 accepted");
        }
        /* (c) minor-0 + STRK → reject (BANK precedent). */
        RI_ASSERT(rbng_test_set_vers("/tmp/ri/run/t59-strk.rbng", "/tmp/ri/run/t59-mut.rbng", 1u, 0u, 0u) == 0, "setvers");
        memset(&r, 0xA5, sizeof r);
        RI_ASSERT(rbng_read_song("/tmp/ri/run/t59-mut.rbng", &r, err, sizeof err) != 0, "minor0+STRK accepted");
        /* Legacy shape: cleared track omits STRK (PATT only). */
        rbng_song_init(&s);
        RI_ASSERT(rbng_write_song("/tmp/ri/run/t59-legacy.rbng", &s, err, sizeof err) == 0, "legacy write %s", err);
        f = fopen("/tmp/ri/run/t59-legacy.rbng", "rb");
        {
            int has_patt = 0, has_strk = 0, has_bank = 0;
            RI_ASSERT(f != 0, "open legacy");
            nn = f ? fread(buf, 1, sizeof buf, f) : 0;
            if (f)
                fclose(f);
            for (k = 0; k + 4 <= nn; k++) {
                if (!memcmp(buf + k, "PATT", 4)) has_patt = 1;
                if (!memcmp(buf + k, "STRK", 4)) has_strk = 1;
                if (!memcmp(buf + k, "BANK", 4)) has_bank = 1;
            }
            RI_ASSERT(has_patt && !has_strk && !has_bank, "legacy shape");
        }
        memset(&r, 0xA5, sizeof r);
        RI_ASSERT(rbng_read_song("/tmp/ri/run/t59-legacy.rbng", &r, err, sizeof err) == 0, "legacy read %s", err);
        for (i = 0u; i < 4u; i++)
            RI_ASSERT(ri_track_selected(&r.track, 500u, i) == 0u, "v1.0 default");
    }
```
(`size` var unused — dropped. `nn` is `size_t`; `k + 4 <= nn` mixes uint32/size_t — comparison warning? `k` is uint32_t, `nn` size_t (64-bit): usual promotions → fine, no -Werror issue (no sign-compare: both unsigned). `buf[k+4]` etc: uint8 arithmetic → int, shifts fine. `(unsigned)(...)` ok. `soff - 4u` = chunk size field offset ✓ (soff points at body).)
`rbng_test_patch_bytes` + `rbng_test_set_vers` exist (t56 uses both — same signatures). `memset`/`memcmp`/`fopen`/`fread` need string.h + stdio.h in the test (add includes).

- [ ] **Step 2: Run test to verify it fails**

Run: `bash scripts/ri_build_host.sh test t59_songtrack 2>&1 | grep -E "no member|error" | head -3`
Expected: FAIL with `has no member named 'track'` (test references `s.track` before `struct RISong` gains it)

- [ ] **Step 3: Write minimal implementation**

In `project/rbng.h`, extend `struct RISong` (needs `#include "engine/seq/songtrack.h"` — check rbng.h's current includes first; BANK precedent already pulls pattern types, follow it):
```c
    /* v1.1 song track (slot-0 everywhere = legacy shape). */
    struct RISongTrack track;
```
In `project/rbng.c`:
- `rbng_song_init`: `ri_track_init(&s->track);` beside the existing zeroes.
- `song_valid`: after the existing range checks, validate the track (all 3996 slots ≤ 31):
```c
    for (k = 0; k < 999u; k++) {
        uint32_t i;
        for (i = 0u; i < 4u; i++) {
            if (s->track.slot[k][i] > 31u) {
                put_err(err, errcap, "STRK slot out of range");
                return 1;
            }
        }
    }
```
(`k` exists as `uint32_t k` in `song_valid` — reuse it; add `uint32_t i;` or reuse pattern from the file. Match the file's declaration style.)
- Writer: track-is-default check + conditional STRK. Default test helper (static, beside `write_bank`):
```c
/* v1.1 STRK chunk: full 3996-byte grid, written only when the track
 * differs from slot-0-everywhere (else the file keeps legacy shape). */
static int track_is_default(const struct RISong *s) {
    uint32_t b, i;
    for (b = 0u; b < 999u; b++)
        for (i = 0u; i < 4u; i++)
            if (s->track.slot[b][i] != 0u)
                return 0;
    return 1;
}
```
Writer total accounting: `if (!track_is_default(s)) total += 8u + 3996u;` (3996 even → no pad byte; assert that invariant in a comment). VERS minor: `1` when banks OR non-default track, else `0` (extends the Task-8 rule: `s->nbanks > 0u ? 1u : 0u` becomes `(s->nbanks > 0u || !track_is_default(s)) ? 1u : 0u`). Chunk emission beside the BANK loop:
```c
    if (!track_is_default(s)) {
        uint32_t b, i;
        chunk_head(f, "STRK", 3996u);
        for (b = 0u; b < 999u; b++)
            for (i = 0u; i < 4u; i++)
                fputc((int)s->track.slot[b][i], f);
    }
```
- Reader (`parse_image`): add `saw_strk` + branch mirroring `parse_bank` style:
```c
        } else if (memcmp(cid, "BANK", 4) == 0) {
            ... existing ...
        } else if (memcmp(cid, "STRK", 4) == 0) {
            uint32_t k;
            if (saw_strk) { ck_err(... "duplicate STRK"); return 1; }
            saw_strk = 1;
            if (!saw_vers) { ck_err(... "STRK before VERS"); return 1; }
            if (file_minor == 0u) { ck_err(... "STRK requires 1.1"); return 1; }
            if (size != 3996u) { ck_err(... "STRK length mismatch"); return 1; }
            for (k = 0u; k < 3996u; k++) {
                uint8_t v = img[doff + k];
                if (v > 31u) { ck_err(... "STRK slot out of range"); return 1; }
                s->track.slot[k / 4u][k % 4u] = v;
            }
```
(`k/4`, `k%4`: row-major bar→instance ✓. Division exactness: no constraint issue. `saw_vers`/`file_minor`/`ck_err`/`doff` names follow the existing BANK branch — executor: copy the branch's guard style verbatim.)
No end-of-parse requirement for STRK (missing = slot-0 default, already inited).

- [ ] **Step 4: Run test to verify it passes**

Run: `bash scripts/ri_build_host.sh all && bash scripts/ri_build_host.sh test t59_songtrack 2>&1 | tail -2`
Expected: `PASS songtrack` (use `&&` chaining — a failed `all` must never silently feed a stale link, per the m64 lesson)

- [ ] **Step 5: Commit**

```bash
git add project/rbng.h project/rbng.c tests/unit/t59_songtrack.c
git commit -m "feat: STRK codec + song integration [§12.9b]

Co-Authored-By: OpenCode <noreply@opencode.ai>"
```

---

### Task 5: Audit wiring + gate + records

**Files:**
- Modify: `scripts/ri_audit.sh`, `docs/2026-09-24-improvement-todo.md`, `llm-wiki/raw/articles/2026-09-25-songtrack.md` (new), `llm-wiki/log.md`, `llm-wiki/index.md`

- [ ] **Step 1: Wire t59 into the audit** (seq block beside the t58 line at `scripts/ri_audit.sh:152`)

```bash
bash "$ROOT/scripts/ri_build_host.sh" test t59_songtrack >/dev/null || { echo "FAIL: t59_songtrack"; exit 1; }
```

- [ ] **Step 2: Run the FULL gate**

Run: `bash scripts/ri_audit.sh > /tmp/ri/audit-129b.log 2>&1; echo "AUDIT_RC=$?"`
Expected: `AUDIT_RC=0`, tail `AUDIT 0/0 PASS`. Any FAIL names the culprit — fix under TDD, never by weakening a pin.

- [ ] **Step 3: Prove the new gates load-bearing** (three mutants, all recorded with failure lines in the wiki article)

Run each: mutate, `bash scripts/ri_build_host.sh test t59_songtrack`, confirm FAIL, revert, confirm PASS:
  - (a) `≠3996` → `>=3996` in the STRK length check → must FAIL the badlen case;
  - (b) force establishment dropped (range walker starts with `force` never set) → must FAIL the steady-range case;
  - (c) `>= 999` → `> 999` in `ri_track_selected` → must FAIL the bar-999 case.

- [ ] **Step 4: Records + tracker, commit, push**

Tick the todo file (song track done; automation + streaming stay open). Write the raw article (what landed, E1 timing split, deviations if any, test counts, audit result, the three mutation lines). Update `llm-wiki/log.md` + `llm-wiki/index.md`.
```bash
git add docs/2026-09-24-improvement-todo.md scripts/ri_audit.sh llm-wiki/raw/articles/2026-09-25-songtrack.md llm-wiki/log.md llm-wiki/index.md
git commit -m "docs: songtrack record + tracker + audit wiring [§12.9b]

Co-Authored-By: OpenCode <noreply@opencode.ai>"
git push origin main
git status --short  # must be empty
```

---

## Acceptance checklist

- [ ] Model + capture green (fail-closed, overwrite, property round-trip, layer guard).
- [ ] Measure + walker green (diff-only, force, slot-0 absolutism, establishment, wrap, truncation, ended).
- [ ] Edits + view green (init variants, clipboard algebra, re-expansion, adjacency).
- [ ] STRK codec green (round-trip, 3 rejects, legacy shape, v1.0 default).
- [ ] Mutation proofs recorded; `ri_audit.sh` 0/0; tree clean.
