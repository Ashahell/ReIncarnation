/* pattern_emit.c — pattern -> event emission (§12.7a Tasks 6-7).
 * 303 rows expand through the shared timed-emit loop; drum rows emit
 * per-lane one-shots. Pure; no allocation, no IO.
 */
#include "engine/seq/pattern.h"
#include "engine/seq/clock.h"
#include "engine/dsp/levi_arp.h"

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

/* Levi chord emission (owner 2026-09-28, v1): per sounding lane
 * NOTE_ON(device, voice=lane, value=note) at the step tick; NOTE_OFF
 * (value = held pitch) at the next step boundary for lanes that stop,
 * and at the occurrence end for still-sounding lanes (§8 gate law,
 * retrigger timing). Shuffle follows the drum grid. */
static uint32_t levi_emit(const struct RIPattern *p, uint16_t device,
    const struct RITempoMap *map, uint64_t start_tick, uint32_t ppq,
    const struct RISchedOpts *opts, struct RIEvent *out, uint32_t cap) {
    uint32_t step_ticks, shuffle_ticks, flam_samples;
    uint32_t n = 0, seq = 0, i;
    uint8_t prev = 0u;
    uint16_t held[RI_LEVI_LANES] = { 0u, 0u, 0u, 0u, 0u, 0u };
    drum_tick_params(opts, ppq, &step_ticks, &shuffle_ticks, &flam_samples,
        map);
    (void)flam_samples;
    if (cap > RI_SCHED_MAX_EVENTS)
        cap = RI_SCHED_MAX_EVENTS;
    for (i = 0u; i < p->length; i++) {
        uint64_t tick = start_tick + (uint64_t)i * (uint64_t)step_ticks;
        uint64_t sample;
        uint8_t cur;
        uint32_t L;
        if ((i & 1u) != 0u)
            tick += (uint64_t)shuffle_ticks;
        sample = ri_map_tick(map, tick);
        cur = p->row.levi[i].on;
        for (L = 0u; L < RI_LEVI_LANES && n < cap; L++) {
            uint8_t bit = (uint8_t)(1u << L);
            if (!(cur & bit))
                continue;
            held[L] = (uint16_t)p->row.levi[i].note[L];
            out[n].sample = sample;
            out[n].type = RI_EV_NOTE_ON;
            out[n].device = device;
            out[n].voice = (uint16_t)L;
            out[n].value = held[L];
            out[n].flags = 0u;
            out[n].seq = seq++;
            n++;
        }
        for (L = 0u; L < RI_LEVI_LANES && n < cap; L++) {
            uint8_t bit = (uint8_t)(1u << L);
            if (!(prev & bit) || (cur & bit))
                continue;
            out[n].sample = sample;
            out[n].type = RI_EV_NOTE_OFF;
            out[n].device = device;
            out[n].voice = (uint16_t)L;
            out[n].value = held[L];
            out[n].flags = 0u;
            out[n].seq = seq++;
            n++;
        }
        prev = cur;
    }
    {
        uint64_t tick = start_tick + (uint64_t)p->length * (uint64_t)step_ticks;
        uint64_t sample = ri_map_tick(map, tick);
        uint32_t L;
        for (L = 0u; L < RI_LEVI_LANES && n < cap; L++) {
            uint8_t bit = (uint8_t)(1u << L);
            if (!(prev & bit))
                continue;
            out[n].sample = sample;
            out[n].type = RI_EV_NOTE_OFF;
            out[n].device = device;
            out[n].voice = (uint16_t)L;
            out[n].value = held[L];
            out[n].flags = 0u;
            out[n].seq = seq++;
            n++;
        }
    }
    /* Insertion sort by the §8 total key (bounded, drum_emit shape). */
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

/* Levi arp rewrite (v2 feature 3b): post-pass over a Levi event window.
 * Groups NOTE_ONs by sample (one chord step each); input NOTE_OFFs are
 * subsumed by the legato chain (each strike releases the previous
 * voice, the tail releases all struck voices at end_sample), so gates
 * balance on the output alone. Strike density from the STEPSQ map
 * (4/quarter = every step, 2 = every 2nd, 1 = every 4th); sub-audible
 * rate falls back to passthrough. Seed = group sample: deterministic. */
uint32_t ri_levi_arp_rewrite(const struct RIEvent *in, uint32_t nin,
    struct RIEvent *out, uint32_t cap, const struct RILeviArpCfg *cfg,
    uint64_t end_sample) {
    uint64_t gsamp[RI_SCHED_MAX_EVENTS];
    uint8_t gnote[RI_SCHED_MAX_EVENTS][RI_LEVI_ARP_MAXNOTES];
    uint8_t gcount[RI_SCHED_MAX_EVENTS];
    uint16_t gdev = 0u;
    uint32_t ng = 0u, i, g, n = 0u;
    uint32_t stepsq, every;
    if (!in || !out || !cfg || cap == 0u)
        return 0u;
    stepsq = RI_LEVI_ARP_STEPSQ(cfg->rate);
    if (!cfg->on || stepsq == 0u || nin == 0u) {
        /* Passthrough (bit-identical when it fits, never partial). */
        if (nin > cap)
            return 0u;
        for (i = 0u; i < nin; i++)
            out[i] = in[i];
        return nin;
    }
    if (cfg->mode >= RI_LEVI_ARP_NMODES)
        return 0u;
    every = 4u / stepsq; /* steps between strikes: 1, 2, 4 */
    /* Group NOTE_ONs by sample (input is §8-sorted: same-sample ONs
     * are adjacent). */
    for (i = 0u; i < nin; i++) {
        if (in[i].type != RI_EV_NOTE_ON)
            continue;
        if (ng > 0u && gsamp[ng - 1u] == in[i].sample &&
            gcount[ng - 1u] < RI_LEVI_ARP_MAXNOTES) {
            gnote[ng - 1u][gcount[ng - 1u]++] = (uint8_t)(in[i].value & 127u);
            continue;
        }
        if (ng >= RI_SCHED_MAX_EVENTS)
            return 0u;
        gsamp[ng] = in[i].sample;
        gnote[ng][0] = (uint8_t)(in[i].value & 127u);
        gcount[ng] = 1u;
        gdev = in[i].device;
        ng++;
    }
    if (ng == 0u)
        return 0u;
    /* Step duration = smallest positive group gap (16th grid). */
    {
        uint64_t step_dur = 0u;
        uint32_t nstr = 0u;
        for (g = 1u; g < ng; g++) {
            uint64_t d = gsamp[g] - gsamp[g - 1u];
            if (d > 0u && (step_dur == 0u || d < step_dur))
                step_dur = d;
        }
        for (g = 0u; g < ng; g++) {
            uint64_t span = (g + 1u < ng ? gsamp[g + 1u] : end_sample > gsamp[g] ? end_sample : gsamp[g]) - gsamp[g];
            uint64_t steps = step_dur ? span / step_dur : 1u;
            uint64_t strikes = steps / every + 1u;
            struct RILeviArp arp;
            uint64_t k;
            uint8_t chord[RI_LEVI_ARP_MAXNOTES];
            uint32_t cn;
            for (cn = 0u; cn < gcount[g]; cn++)
                chord[cn] = gnote[g][cn];
            ri_levi_arp_init(&arp);
            arp.on = 1u;
            arp.rate = cfg->rate;
            if (ri_levi_arp_start(&arp, chord, gcount[g], cfg->mode,
                    (uint32_t)(gsamp[g] & 0xFFFFFFFFu)) != 0)
                return 0u;
            for (k = 0u; k < strikes; k++) {
                uint64_t at = strikes > 1u ? gsamp[g] + span * k / (strikes - 1u) : gsamp[g];
                uint8_t note, v;
                if (n + 2u > cap)
                    return 0u;
                if (ri_levi_arp_step(&arp, &note) != 0)
                    return 0u;
                v = (uint8_t)(nstr % RI_LEVI_LANES);
                if (nstr > 0u) {
                    /* Legato: release the previous strike voice first
                     * (same-sample OFF+ON mirrors the v1 gate shape). */
                    uint8_t pv = (uint8_t)((nstr - 1u) % RI_LEVI_LANES);
                    out[n].sample = at;
                    out[n].type = RI_EV_NOTE_OFF;
                    out[n].device = gdev;
                    out[n].voice = pv;
                    out[n].value = 0u;
                    out[n].flags = 0u;
                    out[n].seq = n;
                    n++;
                }
                out[n].sample = at;
                out[n].type = RI_EV_NOTE_ON;
                out[n].device = gdev;
                out[n].voice = v;
                out[n].value = note;
                out[n].flags = 0u;
                out[n].seq = n;
                n++;
                nstr++;
            }
        }
        /* Tail: only the final strike voice is still held (every
         * earlier strike was released by the next strike's legato);
         * release it at end_sample. */
        {
            uint8_t lv = (uint8_t)((nstr - 1u) % RI_LEVI_LANES);
            uint64_t tail = end_sample > gsamp[ng - 1u] ? end_sample : gsamp[ng - 1u];
            if (n + 1u > cap)
                return 0u;
            out[n].sample = tail;
            out[n].type = RI_EV_NOTE_OFF;
            out[n].device = gdev;
            out[n].voice = lv;
            out[n].value = 0u;
            out[n].flags = 0u;
            out[n].seq = n;
            n++;
        }
    }
    return n;
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
    if (p->kind == RI_PATTERN_KIND_LEVI)
        /* Occurrence-scoped gate like drums (no carry across the seam
         * in v1; retrigger timing): still-sounding lanes close at the
         * occurrence end tick. */
        return levi_emit(p, device, map, start_tick, ppq, opts, out,
            cap);
    return 0;
}
