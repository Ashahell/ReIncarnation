/* pattern_emit.c — pattern -> event emission (§12.7a Tasks 6-7).
 * 303 rows expand through the shared timed-emit loop; drum rows emit
 * per-lane one-shots. Pure; no allocation, no IO.
 */
#include "engine/seq/pattern.h"
#include "engine/seq/clock.h"

uint32_t ri_pattern303_to_steps(const struct RIPattern *p, int cyclic,
    struct RIStep out[RI_PATTERN_STEPS]) {
    uint32_t i, len;
    if (!p || !out || p->kind != RI_PATTERN_KIND_303)
        return 0;
    if (ri_pattern_valid(p) != 0)
        return 0;
    len = p->length;
    for (i = 0; i < len; i++) {
        uint8_t rf = p->row.r303[i].flags;
        uint8_t rest = (rf & RI_STEP_REST) != 0u;
        uint8_t up = (rf & RI_STEP_UP) != 0u;
        uint8_t dn = (rf & RI_STEP_DOWN) != 0u;
        out[i].note = ri_p303_note(&p->row.r303[i]);
        out[i].flags = 0u;
        if (rest) {
            /* Pause: the row's own slide/accent/octave flags are inert,
             * but the shifted tie from row i-1 still applies (a slide
             * into a Pause holds the gate: NOTE_CONTINUE). */
            out[i].flags = RI_STEP_REST;
            if (i > 0u) {
                uint8_t pf = p->row.r303[i - 1u].flags;
                if ((pf & RI_STEP_SLIDE) && !(pf & RI_STEP_REST))
                    out[i].flags |= RI_STEP_SLIDE;
            }
            continue;
        }
        if (rf & RI_STEP_ACCENT)
            out[i].flags |= RI_STEP_ACCENT;
        if (up != dn)
            out[i].flags |= (uint8_t)(up ? RI_STEP_UP : RI_STEP_DOWN);
        /* Walker SLIDE on step i <=> row i-1 is a sounding slide
         * (E1 "tie to next", shifted to the walker's "slide into").
         * A slide flag on a Pause row ties nothing. */
        if (i == 0) {
            /* Seam handled by carry (TIE_OUT below), never by SLIDE. */
        } else {
            uint8_t pf = p->row.r303[i - 1u].flags;
            if ((pf & RI_STEP_SLIDE) &&
                !(pf & RI_STEP_REST))
                out[i].flags |= RI_STEP_SLIDE;
        }
    }
    if (cyclic && len > 0u) {
        uint8_t lf = p->row.r303[len - 1u].flags;
        if ((lf & RI_STEP_SLIDE) && !(lf & RI_STEP_REST))
            out[len - 1u].flags |= RI_STEP_TIE_OUT;
    }
    return len;
}

static void drum_tick_params(const struct RISchedOpts *opts, uint32_t ppq,
    uint32_t *step_ticks, uint32_t *shuffle_ticks, uint32_t *flam_samples,
    const struct RITempoMap *map) {
    uint32_t st = (ppq > 0) ? ppq / 4u : 24u;
    uint32_t sh = 0u;
    uint32_t fl = 0u;
    if (st == 0u)
        st = 24u;
    if (opts) {
        uint32_t pct = opts->shuffle_pct > 100u ?
            100u : (uint32_t)opts->shuffle_pct;
        sh = (pct * st + 50u) / 100u;
        if (opts->flam_ms > 0.0 && map && map->sr > 0u)
            fl = (uint32_t)(opts->flam_ms * (double)map->sr / 1000.0 +
                0.5);
    }
    *step_ticks = st;
    *shuffle_ticks = sh;
    *flam_samples = fl;
}

static uint32_t drum_emit(const struct RIPattern *p, uint16_t device,
    const struct RITempoMap *map, uint64_t start_tick, uint32_t ppq,
    const struct RISchedOpts *opts, struct RIEvent *out, uint32_t cap) {
    uint32_t step_ticks, shuffle_ticks, flam_samples;
    uint32_t n = 0, seq = 0, i;
    uint32_t L;
    drum_tick_params(opts, ppq, &step_ticks, &shuffle_ticks, &flam_samples,
        map);
    if (cap > RI_SCHED_MAX_EVENTS)
        cap = RI_SCHED_MAX_EVENTS;
    for (i = 0; i < p->length; i++) {
        uint64_t tick = start_tick + (uint64_t)i * (uint64_t)step_ticks;
        uint64_t sample;
        uint16_t on, hi, fl;
        if ((i & 1u) != 0u)
            tick += (uint64_t)shuffle_ticks;
        sample = ri_map_tick(map, tick);
        if ((p->row.drum[i].flags & RI_DRUM_AC) && n < cap) {
            out[n].sample = sample;
            out[n].type = RI_EV_ACCENT;
            out[n].device = device;
            out[n].voice = RI_VOICE_ALL;
            out[n].value = 1;
            out[n].flags = 0;
            out[n].seq = seq++;
            n++;
        }
        on = p->row.drum[i].on;
        hi = p->row.drum[i].high;
        fl = p->row.drum[i].flam;
        for (L = 0; L < RI_DRUM_LANES && n < cap; L++) {
            uint16_t bit = (uint16_t)(1u << L);
            if (!(on & bit))
                continue;
            out[n].sample = sample;
            out[n].type = RI_EV_NOTE_ON;
            out[n].device = device;
            out[n].voice = (uint16_t)L;
            out[n].value = (uint16_t)L;
            out[n].flags =
                (hi & bit) ? RI_EVFLAG_ACCENT : 0u;
            out[n].seq = seq++;
            n++;
        }
        for (L = 0; L < RI_DRUM_LANES && n < cap; L++) {
            uint16_t bit = (uint16_t)(1u << L);
            if (!(fl & bit))
                continue;
            out[n].sample = sample + (uint64_t)flam_samples;
            out[n].type = RI_EV_FLAM;
            out[n].device = device;
            out[n].voice = (uint16_t)L;
            out[n].value = flam_samples > 65535u ? 65535u :
                (uint16_t)flam_samples;
            out[n].flags = RI_EVFLAG_FLAM2;
            out[n].seq = seq++;
            n++;
        }
    }
    /* Insertion sort by the §8 total key (bounded). */
    {
        uint32_t a, b;
        for (a = 1; a < RI_SCHED_MAX_EVENTS; a++) {
            struct RIEvent key;
            if (a >= n)
                break;
            key = out[a];
            b = a;
            for (;;) {
                if (b == 0)
                    break;
                if (!ri_event_less(&key, &out[b - 1]))
                    break;
                out[b] = out[b - 1];
                b--;
            }
            out[b] = key;
        }
    }
    return n;
}

uint32_t ri_sched_emit_pattern(const struct RIPattern *p, uint16_t device,
    const struct RITempoMap *map, uint64_t start_tick, uint32_t ppq,
    const struct RISchedOpts *opts,
    const struct RISchedCarry *carry_in, struct RISchedCarry *carry_out,
    struct RIEvent *out, uint32_t cap) {
    struct RIStep steps[RI_PATTERN_STEPS];
    uint32_t nsteps;
    if (!p || !out)
        return 0;
    if (ri_pattern_valid(p) != 0)
        return 0; /* fail closed on hand-corrupted structs */
    if (p->kind == RI_PATTERN_KIND_303) {
        /* Patterns always loop in song play: cyclic seam is on. */
        nsteps = ri_pattern303_to_steps(p, 1, steps);
        if (nsteps == 0u)
            return 0;
        return ri_sched_emit_timed_carry(map, start_tick, ppq, steps,
            nsteps, device, opts, carry_in, carry_out, out, cap);
    }
    if (p->kind == RI_PATTERN_KIND_DRUM)
        return drum_emit(p, device, map, start_tick, ppq, opts, out,
            cap);
    return 0;
}
