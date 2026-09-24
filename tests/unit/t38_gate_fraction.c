/* t38_gate_fraction.c — gate-length rule (D-h, E0): non-slide 303 notes
 * fall at step_start + 1/2 step; ties (slide/rest+slide/legato) hold.
 * (a) two plain notes: ON@0, OFF@half-step, ON@step, OFF@1.5 steps.
 * (b) slide tie: no fractional OFF (ON, ON+slide, end OFF only).
 * (c) rest+slide tie: ON, CONTINUE, end OFF — no fractional OFF.
 * (d) shuffled odd-step note: OFF follows its shuffled start + half step.
 * RED-first: OFFs sit at step boundaries (full-length gate).
 */
#include <stdio.h>
#include <stdint.h>
#include "tests/helpers/ri_assert.h"
#include "engine/seq/clock.h"
#include "engine/seq/sched.h"

static const struct RISegment g_segs[] = { { 0, 428571428ULL } }; /* 140 BPM */
static const struct RITempoMap g_map = { g_segs, 1, 96, 48000 };

static uint64_t tick_smp(uint64_t tick) { return ri_map_tick(&g_map, tick); }

int main(void) {
    struct RIStep steps[4];
    struct RIEvent ev[RI_SCHED_MAX_EVENTS];
    struct RISchedOpts opts = { 0, 0, RI_FLAM_MS_DEFAULT };
    uint32_t n, i;
    /* (a) two plain notes. */
    steps[0].note = 45; steps[0].flags = 0;
    steps[1].note = 47; steps[1].flags = 0;
    n = ri_sched_emit_timed(&g_map, 0, 96, steps, 2, 0, &opts, ev, RI_SCHED_MAX_EVENTS);
    RI_ASSERT(n == 4, "plain count %u, want 4", n);
    if (n == 4) {
        RI_ASSERT(ev[0].type == RI_EV_NOTE_ON && ev[0].sample == tick_smp(0), "a ev0");
        RI_ASSERT(ev[1].type == RI_EV_NOTE_OFF && ev[1].sample == tick_smp(12),
            "a frac OFF %u @%llu, want 12t", ev[1].type, (unsigned long long)ev[1].sample);
        RI_ASSERT(ev[2].type == RI_EV_NOTE_ON && ev[2].sample == tick_smp(24), "a ev2");
        RI_ASSERT(ev[3].type == RI_EV_NOTE_OFF && ev[3].sample == tick_smp(36),
            "a end OFF %u @%llu, want 36t", ev[3].type, (unsigned long long)ev[3].sample);
    }
    /* (b) slide tie holds: no fractional OFF. */
    steps[0].note = 45; steps[0].flags = 0;
    steps[1].note = 47; steps[1].flags = RI_STEP_SLIDE;
    n = ri_sched_emit_timed(&g_map, 0, 96, steps, 2, 0, &opts, ev, RI_SCHED_MAX_EVENTS);
    RI_ASSERT(n == 3, "tie count %u, want 3", n);
    if (n == 3) {
        RI_ASSERT(ev[0].type == RI_EV_NOTE_ON && ev[1].type == RI_EV_NOTE_ON &&
            ev[2].type == RI_EV_NOTE_OFF, "tie shape (%u,%u,%u)", ev[0].type, ev[1].type, ev[2].type);
        RI_ASSERT(ev[2].sample == tick_smp(48), "tie end OFF @%llu, want 48t",
            (unsigned long long)ev[2].sample);
    }
    /* (c) rest+slide tie holds. */
    steps[0].note = 45; steps[0].flags = 0;
    steps[1].note = 47; steps[1].flags = RI_STEP_SLIDE;
    steps[2].note = 0; steps[2].flags = RI_STEP_REST | RI_STEP_SLIDE;
    steps[3].note = 0; steps[3].flags = RI_STEP_REST;
    n = ri_sched_emit_timed(&g_map, 0, 96, steps, 4, 0, &opts, ev, RI_SCHED_MAX_EVENTS);
    RI_ASSERT(n == 4, "rest-tie count %u, want 4", n);
    if (n == 4) {
        RI_ASSERT(ev[0].type == RI_EV_NOTE_ON && ev[1].type == RI_EV_NOTE_ON &&
            ev[2].type == RI_EV_NOTE_CONTINUE && ev[3].type == RI_EV_NOTE_OFF,
            "rest-tie shape (%u,%u,%u,%u)", ev[0].type, ev[1].type, ev[2].type, ev[3].type);
    }
    /* (d) shuffled odd step: OFF = shuffled start + half step. */
    steps[0].note = 0; steps[0].flags = RI_STEP_REST;
    steps[1].note = 47; steps[1].flags = 0;
    steps[2].note = 0; steps[2].flags = RI_STEP_REST;
    opts.shuffle_pct = 50; /* 12-tick delay on odd steps */
    n = ri_sched_emit_timed(&g_map, 0, 96, steps, 3, 0, &opts, ev, RI_SCHED_MAX_EVENTS);
    RI_ASSERT(n == 2, "shuf count %u, want 2", n);
    if (n == 2) {
        RI_ASSERT(ev[0].type == RI_EV_NOTE_ON && ev[0].sample == tick_smp(36), "shuf ON @%llu",
            (unsigned long long)ev[0].sample);
        RI_ASSERT(ev[1].type == RI_EV_NOTE_OFF && ev[1].sample == tick_smp(48), "shuf OFF @%llu, want 48t",
            (unsigned long long)ev[1].sample);
    }
    opts.shuffle_pct = 0;
    /* sorted invariant on every stream above. */
    for (i = 1; i < n; i++)
        RI_ASSERT(!ri_event_less(&ev[i], &ev[i - 1]), "stream not sorted at %u", i);
    RI_RESULT("gate_fraction");
}
