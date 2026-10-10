/* midi_io/midi_interval.c — arrival-interval statistics (M5 proof).
 * See midi_interval.h. No clock is read here, by design.
 */
#include <string.h>

#include "midi_io/midi_interval.h"

/* The verdict bound, stated once: a spread within 5% of the mean plus a
 * 200 us allowance is steady. Both halves are needed — a percentage alone
 * blesses a 40 ms mean with 2 ms of wobble, and a fixed allowance alone
 * blesses a very fast clock for being very fast. */
#define JITTER_PCT 20u        /* spread*100/mean <= 20  ->  spread <= mean/5 */
#define JITTER_FLOOR_US 200u

void midi_interval_init(struct RIMidiInterval *s, uint32_t window_us) {
    uint32_t i;
    if (!s)
        return;
    memset(s, 0, sizeof *s);
    s->window_us = window_us;
    if (window_us) {
        for (i = 0u; i < RI_MIDIINT_SLICES; i++)
            s->slice[i] = 0u;
    }
}

void midi_interval_add(struct RIMidiInterval *s, uint64_t arrival_us) {
    if (!s)
        return;
    if (s->count == 0u) {
        s->first_us = arrival_us;
        s->prev_us = arrival_us;
        s->count = 1u;
        return;
    }
    {
        /* A clock that does not advance is clamped to 1 us and counted.
         * Unsigned subtraction here would otherwise produce an enormous
         * interval and poison the mean and the spread for the whole run. */
        uint64_t gap = (arrival_us > s->prev_us) ? (arrival_us - s->prev_us) : 1ULL;
        uint32_t g;
        if (arrival_us <= s->prev_us)
            s->backwards++;
        g = (uint32_t)(gap > 0xFFFFFFFFULL ? 0xFFFFFFFFULL : gap);
        if (s->intervals == 0u) {
            s->min_us = g;
            s->max_us = g;
            s->first_int_us = g;
        } else {
            if (g < s->min_us)
                s->min_us = g;
            if (g > s->max_us)
                s->max_us = g;
        }
        s->last_int_us = g;
        s->sum_us += (uint64_t)g;
        s->intervals++;
        if (s->window_us) {
            uint32_t span = s->window_us / RI_MIDIINT_SLICES;
            if (span) {
                /* Slice by ELAPSED time since the first arrival, not by the
                 * raw stamp. The first version offset the edges against an
                 * absolute base truncated to 32 bits, which put slice 0
                 * wherever the run happened to start and would have wrapped
                 * on a long take — and an interval attributed to the wrong
                 * slice is exactly the drift this exists to show. */
                uint32_t idx;
                if (arrival_us > s->first_us)
                    idx = (uint32_t)((arrival_us - s->first_us) / span);
                else
                    idx = 0u;
                /* A run that overruns its own window piles into the last
                 * slice rather than being lost: the totals must always add
                 * up to the interval count, or the drift view is a lie. */
                if (idx >= RI_MIDIINT_SLICES)
                    idx = RI_MIDIINT_SLICES - 1u;
                s->slice[idx]++;
                s->slice_idx = idx;
            }
        }
    }
    s->prev_us = arrival_us;
    s->count++;
}

uint32_t midi_interval_count(const struct RIMidiInterval *s) {
    return s ? s->count : 0u;
}

uint32_t midi_interval_intervals(const struct RIMidiInterval *s) {
    return s ? s->intervals : 0u;
}

uint32_t midi_interval_min_us(const struct RIMidiInterval *s) {
    return s ? s->min_us : 0u;
}

uint32_t midi_interval_max_us(const struct RIMidiInterval *s) {
    return s ? s->max_us : 0u;
}

uint32_t midi_interval_mean_us(const struct RIMidiInterval *s) {
    if (!s || s->intervals == 0u)
        return 0u;
    return (uint32_t)(s->sum_us / (uint64_t)s->intervals);
}

uint32_t midi_interval_jitter_us(const struct RIMidiInterval *s) {
    if (!s || s->intervals == 0u)
        return 0u;
    return s->max_us - s->min_us;
}

uint32_t midi_interval_backwards(const struct RIMidiInterval *s) {
    return s ? s->backwards : 0u;
}

uint32_t midi_interval_slice(const struct RIMidiInterval *s, uint32_t idx) {
    if (!s || idx >= RI_MIDIINT_SLICES)
        return 0u;
    return s->slice[idx];
}

uint32_t midi_interval_slice_total(const struct RIMidiInterval *s) {
    uint32_t i, k = 0u;
    if (!s)
        return 0u;
    for (i = 0u; i < RI_MIDIINT_SLICES; i++)
        k += s->slice[i];
    return k;
}

uint32_t midi_interval_verdict(const struct RIMidiInterval *s) {
    uint32_t spread, mean, bound;
    if (!s || s->intervals == 0u)
        return RI_MIDIINT_NONE;
    spread = midi_interval_jitter_us(s);
    mean = midi_interval_mean_us(s);
    if (mean == 0u)
        return RI_MIDIINT_JITTERY;
    bound = (mean / JITTER_PCT) + JITTER_FLOOR_US;
    return (spread <= bound) ? RI_MIDIINT_STEADY : RI_MIDIINT_JITTERY;
}
