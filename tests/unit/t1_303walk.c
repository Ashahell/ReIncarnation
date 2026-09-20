/* t1_303walk — Task 4 Step 2: minimal-walker unit test (one section only).
 * Asserts the spec §8 gate/slide state-machine rows for a 4-step pattern:
 * new note / note+slide / rest+slide / rest, plus an accent case.
 * Full shuffle/legato/flam walker is Task 7 — NOT here.
 */
#include <stdio.h>
#include "tests/helpers/ri_assert.h"
#include "engine/seq/clock.h"
#include "engine/seq/sched.h"

int main(void) {
    static const struct RISegment segs[] = { { 0, 428571428ULL } }; /* 140 BPM */
    static const struct RITempoMap map = { segs, 1, 96, 48000 };
    struct RIStep steps[4];
    struct RIEvent ev[RI_SCHED_MAX_EVENTS];
    uint32_t n, i;
    uint64_t t0, t1, t2, t3;

    t0 = ri_map_tick(&map, 0);
    t1 = ri_map_tick(&map, 24);
    t2 = ri_map_tick(&map, 48);
    t3 = ri_map_tick(&map, 72);

    steps[0].note = 45;
    steps[0].flags = 0;
    steps[1].note = 47;
    steps[1].flags = RI_STEP_SLIDE;
    steps[2].note = 0;
    steps[2].flags = RI_STEP_REST | RI_STEP_SLIDE;
    steps[3].note = 0;
    steps[3].flags = RI_STEP_REST;

    n = ri_sched_emit_sorted(&map, 0, 96, steps, 4, 0, ev, RI_SCHED_MAX_EVENTS);
    RI_ASSERT(n == 4, "event count %u, want 4", n);
    if (n == 4) {
        RI_ASSERT(ev[0].sample == t0 && ev[0].type == RI_EV_NOTE_ON && ev[0].value == 45 && ev[0].flags == 0,
            "ev0 (%llu,%u,%u,%u)", (unsigned long long)ev[0].sample, ev[0].type, ev[0].value, ev[0].flags);
        RI_ASSERT(ev[1].sample == t1 && ev[1].type == RI_EV_NOTE_ON && ev[1].value == 47 && (ev[1].flags & RI_EVFLAG_SLIDE),
            "ev1 (%llu,%u,%u,%u)", (unsigned long long)ev[1].sample, ev[1].type, ev[1].value, ev[1].flags);
        RI_ASSERT(ev[2].sample == t2 && ev[2].type == RI_EV_NOTE_CONTINUE && ev[2].value == 47,
            "ev2 (%llu,%u,%u,%u)", (unsigned long long)ev[2].sample, ev[2].type, ev[2].value, ev[2].flags);
        RI_ASSERT(ev[3].sample == t3 && ev[3].type == RI_EV_NOTE_OFF && ev[3].value == 47,
            "ev3 (%llu,%u,%u,%u)", (unsigned long long)ev[3].sample, ev[3].type, ev[3].value, ev[3].flags);
    }

    /* Accent case: NOTE_ON + ACCENT at the same sample, ON first (§8 order). */
    steps[0].note = 45;
    steps[0].flags = RI_STEP_ACCENT;
    steps[1].note = 0;
    steps[1].flags = RI_STEP_REST;
    n = ri_sched_emit_sorted(&map, 0, 96, steps, 2, 0, ev, RI_SCHED_MAX_EVENTS);
    RI_ASSERT(n == 3, "accent event count %u, want 3", n);
    if (n == 3) {
        RI_ASSERT(ev[0].type == RI_EV_NOTE_ON && ev[0].sample == t0, "accent ev0 type %u", ev[0].type);
        RI_ASSERT(ev[1].type == RI_EV_ACCENT && ev[1].sample == t0 && ev[1].value == 1, "accent ev1 (%u,%u)",
            ev[1].type, ev[1].value);
        RI_ASSERT(ev[2].type == RI_EV_NOTE_OFF && ev[2].sample == t1, "accent ev2 (%u,%llu)",
            ev[2].type, (unsigned long long)ev[2].sample);
    }

    /* Total-order invariant on every emitted stream. */
    for (i = 1; i < n; i++)
        RI_ASSERT(!ri_event_less(&ev[i], &ev[i - 1]), "stream not sorted at %u", i);

    RI_RESULT("303walk");
}
