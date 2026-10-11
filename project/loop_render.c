/* project/loop_render.c — R9. See loop_render.h for the laws. */
#include "project/loop_render.h"

/* THE LIMIT IS THE RENDER, NOT AN ARBITRARY BAR COUNT. A bar count is only
 * a proxy for "will this fit", and my first limit was 1048576 bars -- about
 * two years at 120 BPM -- which is high enough to ask for a body of
 * 2^32 samples that no caller can hold. The honest bound is on the BODY'S
 * SAMPLES, which is what actually gets allocated downstream: 2^32 samples is
 * four hours at 48 kHz, far past any loop anybody exports, and small enough
 * that `total_samples` cannot overflow when the tail is added.
 *
 * So the bar count is still bounded -- `bars * bar_ticks` must not wrap a
 * uint64 -- but the check that matters is the sample one, and it happens
 * AFTER the samples are computed rather than guessed from bars. */
#define RI_LOOP_MAX_BODY_SAMPLES 4294967295ULL

/* The tail is part of the file, so it cannot be arbitrary. 10 minutes is
 * longer than any decay anyone is exporting and short enough that a typo
 * does not ask for a 40 GB render. */
#define RI_LOOP_MAX_TAIL_MS 600000u

int ri_loop_span(struct RILoopSpan *sp, const struct RITempoMap *map,
    uint32_t ppq, const struct RIStep *steps, uint32_t nsteps,
    uint32_t from_bar, uint32_t bars, uint32_t tail_ms) {
    uint64_t pattern_bars, body_ticks, start, body_end, tail;
    if (!sp || !map || !steps || ppq == 0u || nsteps == 0u)
        return 1;
    if (map->sr == 0u)
        return 1;

    /* A BAR IS A WHOLE NUMBER OF BARS OR IT IS NOT A LOOP. 20 steps is 1.25
     * bars: rounding up drops music the caller wrote, rounding down clips
     * the end. Neither is this module's decision to make. */
    if ((nsteps % RI_LOOP_STEPS_PER_BAR) != 0u)
        return 1;
    pattern_bars = (uint64_t)nsteps / (uint64_t)RI_LOOP_STEPS_PER_BAR;

    /* FROM PAST THE END IS REFUSED, NOT WRAPPED. Bar 1 of a one-bar pattern
     * is a caller mistake, and quietly exporting bar 0 instead is worse than
     * refusing -- the same law as t205's unsorted-event refusal: do not
     * render music the caller did not describe. */
    if ((uint64_t)from_bar >= pattern_bars)
        return 1;
    if (bars == 0u)
        return 1;
    if (tail_ms > RI_LOOP_MAX_TAIL_MS)
        return 1;

    /* A BAR IS FOUR QUARTERS OF ppq TICKS, and the map's ppq is the one the
     * caller renders with -- using a different ppq here would produce a bar
     * the render does not agree with. */
    if ((ppq % 4u) != 0u)
        return 1;

    sp->bar_ticks = (uint64_t)ppq * 4u;
    sp->bar_samples = ri_map_tick(map, sp->bar_ticks) -
        ri_map_tick(map, 0u);
    if (sp->bar_samples == 0u)
        return 1;   /* a bar shorter than a sample cannot be looped */

    start = (uint64_t)from_bar * sp->bar_ticks;
    body_ticks = (uint64_t)bars * sp->bar_ticks;

    /* THE BODY'S SAMPLES COME FROM THE MAP AT ITS ENDPOINTS. Not
     * `bar_samples * bars`: that is a second answer to the ticks-to-samples
     * question, and the two disagree by a sample per bar at some tempos --
     * a per-bar disagreement is a seam that drifts. */
    body_end = start + body_ticks;
    sp->start_tick = start;
    sp->body_ticks = body_ticks;
    sp->body_samples = ri_map_tick(map, body_end) - ri_map_tick(map, start);

    /* WHOLE BARS, PROVEN rather than assumed: a body that is not a multiple
     * of its own bar is not loop-exact and no caller should be handed one. */
    if ((sp->body_samples % sp->bar_samples) != 0u)
        return 1;
    /* AND THE BODY FITS IN SOMETHING. Refused AFTER the samples exist, so the
     * bound is about the render rather than about a count that only
     * approximates it. */
    if (sp->body_samples > RI_LOOP_MAX_BODY_SAMPLES)
        return 1;

    /* THE TAIL IS ADDITIVE. It cannot move the seam: `body_samples` was
     * already fixed above and nothing below writes to it. */
    if (tail_ms != 0u) {
        tail = ((uint64_t)tail_ms * (uint64_t)map->sr + 500u) / 1000u;
        /* A FRACTIONAL SAMPLE IS REFUSED, not rounded silently into the
         * file: the caller asked for a length in milliseconds and the file
         * is in samples, so the honest answer is that those two do not
         * describe the same number. */
        if (((tail_ms * (uint64_t)map->sr) % 1000u) != 0u)
            return 1;
        sp->tail_samples = (uint32_t)tail;
    } else {
        sp->tail_samples = 0u;
    }
    sp->total_samples = sp->body_samples + (uint64_t)sp->tail_samples;
    return 0;
}
