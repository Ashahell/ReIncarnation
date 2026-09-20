/* sched.c — minimal walker (Task 4, gate G4): sorted-array emit for ONE
 * section only. Insertion order = song order (seq = emit index); output is
 * insertion-sorted by the §8 key via ri_event_less. All loops trip-count
 * bounded by RI_SCHED_MAX_EVENTS (constant), kernels.c style. No allocation.
 * Full shuffle/legato/flam walker is Task 7 — NOT here.
 */
#include "engine/seq/sched.h"
#include "engine/seq/clock.h"

uint32_t ri_sched_emit_sorted(const struct RITempoMap *map, uint64_t start_tick,
    uint32_t ppq, const struct RIStep *steps, uint32_t nsteps,
    uint16_t device, struct RIEvent *out, uint32_t cap) {
    uint32_t n = 0; /* emitted count */
    uint32_t seq = 0;
    uint32_t i, j;
    uint32_t step_ticks = (ppq > 0) ? ppq / 4u : 24u; /* 16th grid */
    uint8_t held = 0;   /* held pitch for NOTE_CONTINUE */
    int gate = 0;       /* gate currently high */
    if (step_ticks == 0)
        step_ticks = 24u;
    if (cap > RI_SCHED_MAX_EVENTS)
        cap = RI_SCHED_MAX_EVENTS;
    if (nsteps > RI_SCHED_MAX_EVENTS)
        nsteps = RI_SCHED_MAX_EVENTS;
    for (i = 0; i < RI_SCHED_MAX_EVENTS; i++) {
        uint64_t tick, sample;
        int is_rest, is_slide, is_accent;
        if (i >= nsteps)
            break;
        tick = start_tick + (uint64_t)i * (uint64_t)step_ticks;
        sample = ri_map_tick(map, tick);
        is_rest = (steps[i].flags & RI_STEP_REST) != 0;
        is_slide = (steps[i].flags & RI_STEP_SLIDE) != 0;
        is_accent = (steps[i].flags & RI_STEP_ACCENT) != 0;
        if (!is_rest) {
            if (gate && !is_slide && n < cap) {
                /* gate breaks: previous note off before the new attack
                 * (§8 order NOTE_OFF < NOTE_ON sorts it first anyway) */
                out[n].sample = sample;
                out[n].type = RI_EV_NOTE_OFF;
                out[n].device = device;
                out[n].voice = 0;
                out[n].value = held;
                out[n].flags = 0;
                out[n].seq = seq++;
                n++;
            }
            if (n < cap) {
                out[n].sample = sample;
                out[n].type = RI_EV_NOTE_ON;
                out[n].device = device;
                out[n].voice = 0;
                out[n].value = steps[i].note;
                out[n].flags = (uint16_t)((is_slide ? RI_EVFLAG_SLIDE : 0u) |
                    (is_accent ? RI_EVFLAG_ACCENT : 0u));
                out[n].seq = seq++;
                n++;
            }
            held = steps[i].note;
            gate = 1;
        } else if (is_slide) {
            /* rest + slide: gate stays high, pitch slews to held pitch */
            if (gate && n < cap) {
                out[n].sample = sample;
                out[n].type = RI_EV_NOTE_CONTINUE;
                out[n].device = device;
                out[n].voice = 0;
                out[n].value = held;
                out[n].flags = RI_EVFLAG_SLIDE;
                out[n].seq = seq++;
                n++;
            }
        } else {
            /* rest, no slide: gate low */
            if (gate && n < cap) {
                out[n].sample = sample;
                out[n].type = RI_EV_NOTE_OFF;
                out[n].device = device;
                out[n].voice = 0;
                out[n].value = held;
                out[n].flags = 0;
                out[n].seq = seq++;
                n++;
            }
            gate = 0;
        }
        if (is_accent && n < cap) {
            out[n].sample = sample;
            out[n].type = RI_EV_ACCENT;
            out[n].device = device;
            out[n].voice = 0;
            out[n].value = 1; /* accent level 1 (P-12 three-state is Task 8+) */
            out[n].flags = 0;
            out[n].seq = seq++;
            n++;
        }
    }
    /* final NOTE_OFF at the pattern end tick when the gate is still high */
    if (gate && n < cap) {
        out[n].sample = ri_map_tick(map, start_tick + (uint64_t)nsteps * (uint64_t)step_ticks);
        out[n].type = RI_EV_NOTE_OFF;
        out[n].device = device;
        out[n].voice = 0;
        out[n].value = held;
        out[n].flags = 0;
        out[n].seq = seq++;
        n++;
    }
    /* insertion sort by the §8 total key (bounded: n <= cap <= 256) */
    for (i = 1; i < RI_SCHED_MAX_EVENTS; i++) {
        struct RIEvent key;
        if (i >= n)
            break;
        key = out[i];
        j = i;
        for (;;) {
            if (j == 0)
                break;
            if (!ri_event_less(&key, &out[j - 1]))
                break;
            out[j] = out[j - 1];
            j--;
        }
        out[j] = key;
    }
    return n;
}
