/* sched.c — walker with musical-timing overlays (Task 4 minimal emit +
 * Task 7 shuffle/legato/flam, gate G7): sorted-array emit for ONE section
 * only. Insertion order = song order (seq = emit index); output is
 * insertion-sorted by the §8 key via ri_event_less. All loops trip-count
 * bounded by RI_SCHED_MAX_EVENTS (constant), kernels.c style. No allocation.
 * `ri_sched_emit_sorted` is the Task-4 entry (straight, no legato) kept
 * byte-identical; `ri_sched_emit_timed` adds the §7-ordered overlays.
 */
#include "engine/seq/sched.h"
#include "engine/seq/clock.h"

uint32_t ri_sched_emit_timed(const struct RITempoMap *map, uint64_t start_tick,
    uint32_t ppq, const struct RIStep *steps, uint32_t nsteps,
    uint16_t device, const struct RISchedOpts *opts,
    struct RIEvent *out, uint32_t cap) {
    uint32_t n = 0; /* emitted count */
    uint32_t seq = 0;
    uint32_t i, j;
    uint32_t step_ticks = (ppq > 0) ? ppq / 4u : 24u; /* 16th grid */
    uint32_t shuffle_ticks = 0;
    uint32_t flam_samples = 0;
    uint8_t legato = 0;
    uint8_t held = 0;   /* held pitch for NOTE_CONTINUE */
    int gate = 0;       /* gate currently high */
    if (step_ticks == 0)
        step_ticks = 24u;
    if (opts) {
        uint32_t pct = opts->shuffle_pct > 100u ? 100u : (uint32_t)opts->shuffle_pct;
        shuffle_ticks = (pct * step_ticks + 50u) / 100u; /* tick domain, rounded */
        legato = opts->legato != 0u ? 1u : 0u;
        if (opts->flam_ms > 0.0 && map && map->sr > 0u)
            flam_samples = (uint32_t)(opts->flam_ms * (double)map->sr / 1000.0 + 0.5);
    }
    if (cap > RI_SCHED_MAX_EVENTS)
        cap = RI_SCHED_MAX_EVENTS;
    if (nsteps > RI_SCHED_MAX_EVENTS)
        nsteps = RI_SCHED_MAX_EVENTS;
    for (i = 0; i < RI_SCHED_MAX_EVENTS; i++) {
        uint64_t tick, sample;
        int is_rest, is_slide, is_accent, is_flam, is_note;
        if (i >= nsteps)
            break;
        /* §7 first: shuffle offsets trigger times (odd 0-based index = the
         * swung off-beat 16th). Mapped through ri_map_tick like all times. */
        tick = start_tick + (uint64_t)i * (uint64_t)step_ticks;
        if ((i & 1u) != 0u)
            tick += (uint64_t)shuffle_ticks;
        sample = ri_map_tick(map, tick);
        is_rest = (steps[i].flags & RI_STEP_REST) != 0;
        is_slide = (steps[i].flags & RI_STEP_SLIDE) != 0;
        is_accent = (steps[i].flags & RI_STEP_ACCENT) != 0;
        is_flam = (steps[i].flags & RI_STEP_FLAM) != 0;
        is_note = !is_rest;
        if (is_note) {
            int gate_was_high = gate;
            if (gate && !is_slide && !legato && n < cap) {
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
                /* §7 second: legato resolution. A tied NOTE_ON (gate already
                 * high, via explicit slide OR legato mode) carries SLIDE;
                 * legato-mode ties additionally carry LEGATO env-continuity.
                 * A legato tie never emits NOTE_OFF (gate never breaks). */
                out[n].sample = sample;
                out[n].type = RI_EV_NOTE_ON;
                out[n].device = device;
                out[n].voice = 0;
                out[n].value = steps[i].note;
                out[n].flags = (uint16_t)((is_slide || (legato && gate_was_high) ? RI_EVFLAG_SLIDE : 0u) |
                    (is_accent ? RI_EVFLAG_ACCENT : 0u) |
                    ((legato && gate_was_high) ? RI_EVFLAG_LEGATO : 0u));
                out[n].seq = seq++;
                n++;
            }
            held = steps[i].note;
            gate = 1;
            /* §7 last: flam second hit as a separate timestamped event. */
            if (is_flam && n < cap) {
                out[n].sample = sample + (uint64_t)flam_samples;
                out[n].type = RI_EV_FLAM;
                out[n].device = device;
                out[n].voice = 0;
                out[n].value = flam_samples > 65535u ? 65535u : (uint16_t)flam_samples;
                out[n].flags = RI_EVFLAG_FLAM2;
                out[n].seq = seq++;
                n++;
            }
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
            /* rest, no slide: gate low (legato never ties across rests) */
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
    /* final NOTE_OFF at the pattern end tick when the gate is still high.
     * Shuffle does not move the pattern end (the grid is unswung there). */
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

uint32_t ri_sched_emit_sorted(const struct RITempoMap *map, uint64_t start_tick,
    uint32_t ppq, const struct RIStep *steps, uint32_t nsteps,
    uint16_t device, struct RIEvent *out, uint32_t cap) {
    /* Task-4 contract: straight, no legato, default flam (no FLAM bits in
     * Task-4 steps, so no FLAM events). Delegates to the timed core. */
    static const struct RISchedOpts straight = { 0, 0, RI_FLAM_MS_DEFAULT };
    return ri_sched_emit_timed(map, start_tick, ppq, steps, nsteps, device,
        &straight, out, cap);
}
