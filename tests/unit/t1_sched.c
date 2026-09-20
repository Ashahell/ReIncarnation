/* t1_sched — Task 7 (gate G7): shuffle/legato/flam walker unit test.
 *
 * Asserts, at 48 kHz / 140 BPM / PPQ=96 (16th = 24 ticks):
 * - shuffle: steps at 1-based even positions (0-based odd index) are delayed
 *   by shuffle_pct*(ppq/4)/100 ticks, asserted in the tick domain exactly
 *   (expected tick -> ri_map_tick) and in the sample domain against the
 *   nominal 12-tick delay (2571 samples) within +-1; even positions untouched.
 * - legato: with legato mode on, a new note ties (gate stays high): NOTE_ON
 *   with SLIDE + LEGATO (env-continuity) flags, never NOTE_OFF between tied
 *   notes. Control: legato off keeps the Task-4 NOTE_OFF + NOTE_ON break.
 * - flam: a flammed note step emits RI_EV_FLAM with value = delay in samples
 *   and sample offset = flam_ms*sr/1000 (35.0 ms -> 1680 @48k, within the
 *   P-05 +-48 tolerance); flam_ms is a parameter (30.0 ms -> 1440); flam on
 *   a rest step emits nothing.
 * Spec refs: §7 shuffle->legato->flam order, §8 event contract + gate/slide
 * table (implemented exactly; Task-4 rows covered by t1_303walk), P-05.
 */
#include <stdio.h>
#include "tests/helpers/ri_assert.h"
#include "engine/seq/clock.h"
#include "engine/seq/sched.h"

static const struct RISegment t7_segs[] = { { 0, 428571428ULL } }; /* 140 BPM */
static const struct RITempoMap t7_map = { t7_segs, 1, 96, 48000 };

static uint32_t t7_abs_u32(uint32_t a, uint32_t b) {
    return a >= b ? a - b : b - a;
}

static uint64_t t7_abs_u64(uint64_t a, uint64_t b) {
    return a >= b ? a - b : b - a;
}

int main(void) {
    struct RIStep steps[4];
    struct RIEvent ev[RI_SCHED_MAX_EVENTS];
    struct RISchedOpts opts;
    uint32_t n, i;
    uint64_t s0 = 0, s1 = 0, s2 = 0, s3 = 0, q0, q1, q2, q3, d1, d3;

    /* ---- shuffle: 4 plain notes, shuffle 50 -> 12-tick delay on steps 1,3 */
    steps[0].note = 45; steps[0].flags = 0;
    steps[1].note = 47; steps[1].flags = 0;
    steps[2].note = 48; steps[2].flags = 0;
    steps[3].note = 50; steps[3].flags = 0;

    n = ri_sched_emit_sorted(&t7_map, 0, 96, steps, 4, 0, ev, RI_SCHED_MAX_EVENTS);
    RI_ASSERT(n == 8, "straight event count %u, want 8", n);
    if (n < 8)
        goto shuffle_timed;
    s0 = ev[0].sample;
    s1 = ev[2].sample;
    s2 = ev[4].sample;
    s3 = ev[6].sample;
    RI_ASSERT(ev[0].type == RI_EV_NOTE_ON && ev[2].type == RI_EV_NOTE_ON &&
        ev[4].type == RI_EV_NOTE_ON && ev[6].type == RI_EV_NOTE_ON,
        "straight ON positions wrong");
    RI_ASSERT(s0 == ri_map_tick(&t7_map, 0) && s1 == ri_map_tick(&t7_map, 24) &&
        s2 == ri_map_tick(&t7_map, 48) && s3 == ri_map_tick(&t7_map, 72),
        "straight onsets not on grid");

shuffle_timed:
    opts.shuffle_pct = 50;
    opts.legato = 0;
    opts.flam_ms = RI_FLAM_MS_DEFAULT;
    n = ri_sched_emit_timed(&t7_map, 0, 96, steps, 4, 0, &opts, ev, RI_SCHED_MAX_EVENTS);
    RI_ASSERT(n == 8, "shuffled event count %u, want 8", n);
    if (n < 8)
        goto legato;
    q0 = ev[0].sample;
    q1 = ev[2].sample;
    q2 = ev[4].sample;
    q3 = ev[6].sample;
    /* tick-domain exact: odd steps delayed by 50*24/100 = 12 ticks */
    RI_ASSERT(q0 == ri_map_tick(&t7_map, 0), "shuffled step0 moved");
    RI_ASSERT(q1 == ri_map_tick(&t7_map, 36), "shuffled step1 not at tick 36");
    RI_ASSERT(q2 == ri_map_tick(&t7_map, 48), "shuffled step2 moved");
    RI_ASSERT(q3 == ri_map_tick(&t7_map, 84), "shuffled step3 not at tick 84");
    /* sample-domain: 12-tick delay = nominal 2571 samples, +-1 */
    d1 = q1 >= s1 ? (uint64_t)(q1 - s1) : (uint64_t)(s1 - q1);
    d3 = q3 >= s3 ? (uint64_t)(q3 - s3) : (uint64_t)(s3 - q3);
    RI_ASSERT(t7_abs_u64(d1, 2571u) <= 1u, "step1 sample delay %llu, want 2571+-1",
        (unsigned long long)d1);
    RI_ASSERT(t7_abs_u64(d3, 2571u) <= 1u, "step3 sample delay %llu, want 2571+-1",
        (unsigned long long)d3);
    RI_ASSERT(d1 == d3, "shuffle offset not uniform (%llu vs %llu)",
        (unsigned long long)d1, (unsigned long long)d3);
    /* shuffle 100 -> full 16th (24 ticks): step1 lands on step2's tick */
    opts.shuffle_pct = 100;
    n = ri_sched_emit_timed(&t7_map, 0, 96, steps, 4, 0, &opts, ev, RI_SCHED_MAX_EVENTS);
    RI_ASSERT(n == 8, "shuffle100 count %u", n);
    if (n >= 3)
        RI_ASSERT(ev[2].sample == ri_map_tick(&t7_map, 48), "shuffle100 step1 not at tick 48");

legato:
    /* ---- legato: two new notes tie, no NOTE_OFF between ---- */
    steps[0].note = 45; steps[0].flags = 0;
    steps[1].note = 47; steps[1].flags = 0;
    opts.shuffle_pct = 0;
    opts.legato = 1;
    opts.flam_ms = RI_FLAM_MS_DEFAULT;
    n = ri_sched_emit_timed(&t7_map, 0, 96, steps, 2, 0, &opts, ev, RI_SCHED_MAX_EVENTS);
    RI_ASSERT(n == 3, "legato event count %u, want 3", n);
    if (n == 3) {
        RI_ASSERT(ev[0].type == RI_EV_NOTE_ON && ev[0].value == 45 && ev[0].flags == 0,
            "legato ev0 (%u,%u,%u)", ev[0].type, ev[0].value, ev[0].flags);
        RI_ASSERT(ev[1].type == RI_EV_NOTE_ON && ev[1].value == 47 &&
            (ev[1].flags & RI_EVFLAG_SLIDE) && (ev[1].flags & RI_EVFLAG_LEGATO),
            "legato ev1 not ON-with-slide+continuity (%u,%u,%u)",
            ev[1].type, ev[1].value, ev[1].flags);
        RI_ASSERT(ev[2].type == RI_EV_NOTE_OFF && ev[2].value == 47,
            "legato ev2 (%u,%u)", ev[2].type, ev[2].value);
    }
    /* legato + explicit slide: same tie, same flags */
    steps[1].note = 47; steps[1].flags = RI_STEP_SLIDE;
    n = ri_sched_emit_timed(&t7_map, 0, 96, steps, 2, 0, &opts, ev, RI_SCHED_MAX_EVENTS);
    RI_ASSERT(n == 3, "legato+slide count %u, want 3", n);
    if (n == 3)
        RI_ASSERT(ev[1].type == RI_EV_NOTE_ON && (ev[1].flags & RI_EVFLAG_SLIDE) &&
            (ev[1].flags & RI_EVFLAG_LEGATO), "legato+slide ev1 flags %u", ev[1].flags);
    /* control: legato off breaks the gate (Task-4 behavior) */
    steps[1].note = 47; steps[1].flags = 0;
    opts.legato = 0;
    n = ri_sched_emit_timed(&t7_map, 0, 96, steps, 2, 0, &opts, ev, RI_SCHED_MAX_EVENTS);
    RI_ASSERT(n == 4, "non-legato count %u, want 4", n);
    if (n == 4)
        RI_ASSERT(ev[1].type == RI_EV_NOTE_OFF && ev[2].type == RI_EV_NOTE_ON,
            "non-legato no OFF-before-ON (%u,%u)", ev[1].type, ev[2].type);

    /* ---- flam: second hit at flam_ms, value = delay in samples ---- */
    steps[0].note = 45; steps[0].flags = RI_STEP_FLAM;
    steps[1].note = 0; steps[1].flags = RI_STEP_REST;
    opts.flam_ms = 35.0;
    n = ri_sched_emit_timed(&t7_map, 0, 96, steps, 2, 0, &opts, ev, RI_SCHED_MAX_EVENTS);
    RI_ASSERT(n == 3, "flam event count %u, want 3", n);
    if (n == 3) {
        RI_ASSERT(ev[0].type == RI_EV_NOTE_ON && ev[0].sample == ri_map_tick(&t7_map, 0),
            "flam ev0 (%u,%llu)", ev[0].type, (unsigned long long)ev[0].sample);
        RI_ASSERT(ev[1].type == RI_EV_FLAM && ev[1].value == 1680 &&
            (ev[1].flags & RI_EVFLAG_FLAM2),
            "flam ev1 not second-hit 1680 (%u,%u,%u)", ev[1].type, ev[1].value, ev[1].flags);
        RI_ASSERT(ev[1].sample - ev[0].sample == 1680u, "flam offset %llu, want 1680",
            (unsigned long long)(ev[1].sample - ev[0].sample));
        RI_ASSERT(t7_abs_u32(ev[1].value, 1680u) <= 48u, "flam outside P-05 +-48");
        RI_ASSERT(ev[2].type == RI_EV_NOTE_OFF, "flam ev2 type %u", ev[2].type);
    }
    /* flam as parameter: 30 ms -> 1440 samples */
    opts.flam_ms = 30.0;
    n = ri_sched_emit_timed(&t7_map, 0, 96, steps, 2, 0, &opts, ev, RI_SCHED_MAX_EVENTS);
    RI_ASSERT(n == 3, "flam30 count %u", n);
    if (n == 3)
        RI_ASSERT(ev[1].type == RI_EV_FLAM && ev[1].value == 1440 &&
            ev[1].sample - ev[0].sample == 1440u,
            "flam30 (%u,%u,%llu)", ev[1].type, ev[1].value,
            (unsigned long long)(ev[1].sample - ev[0].sample));
    /* flam on a rest step emits nothing extra */
    steps[0].note = 0; steps[0].flags = RI_STEP_REST | RI_STEP_FLAM;
    opts.flam_ms = 35.0;
    n = ri_sched_emit_timed(&t7_map, 0, 96, steps, 2, 0, &opts, ev, RI_SCHED_MAX_EVENTS);
    RI_ASSERT(n == 0, "rest+flam count %u, want 0", n);

    /* ---- total-order invariant on every emitted stream ---- */
    steps[0].note = 45; steps[0].flags = RI_STEP_ACCENT | RI_STEP_FLAM;
    steps[1].note = 47; steps[1].flags = RI_STEP_SLIDE;
    steps[2].note = 0; steps[2].flags = RI_STEP_REST | RI_STEP_SLIDE;
    steps[3].note = 50; steps[3].flags = RI_STEP_FLAM;
    opts.shuffle_pct = 50;
    opts.legato = 1;
    opts.flam_ms = 35.0;
    n = ri_sched_emit_timed(&t7_map, 0, 96, steps, 4, 0, &opts, ev, RI_SCHED_MAX_EVENTS);
    RI_ASSERT(n > 0, "combo emitted nothing");
    for (i = 1; i < n; i++)
        RI_ASSERT(!ri_event_less(&ev[i], &ev[i - 1]), "combo stream not sorted at %u", i);

    RI_RESULT("sched");
}
