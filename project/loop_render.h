/* project/loop_render.h — R9: a loop that a DAW can warp.
 *
 * "Render N bars from a chosen bar, with an optional tail, so exported loops
 * warp cleanly in a DAW." The difficulty is not rendering; it is that the
 * render has to be a WHOLE NUMBER OF BARS, chosen rather than observed.
 *
 * THE ADVERB IS THE WHOLE REQUIREMENT. Before this, `tools/render` sized a
 * song from `ev[nev-1].sample + g_rate`: the last event plus a hardcoded one
 * second. That is a length nobody chose. It lands wherever the last note
 * happens to fall plus a magic constant, so it is usually mid-bar, and a
 * DAW's warp grid has no bar line to sit on -- which is the click at the
 * seam that "loop-exact" exists to prevent.
 *
 * **THE BODY IS COMPUTED FROM THE TEMPO MAP, AND THE TAIL IS ADDITIVE.**
 * A loop for a DAW is the body; the decay after the last bar is the tail.
 * They are different lengths, and conflating them is why exported loops get
 * a stray chunk of reverb welded to the end. `total_samples` is the sum, and
 * `body_samples` is untouched by any tail -- so adding a tail can never move
 * the seam, which is the failure a tail would cause if it were folded in.
 *
 * **A PATTERN MUST BE A WHOLE NUMBER OF BARS.** A 20-step pattern is 1.25
 * bars and cannot be cut loop-exact, so it is REFUSED rather than rounded:
 * rounding up drops music the caller wrote, rounding down clips the end. The
 * caller decides, not this module.
 *
 * **A `from` PAST THE PATTERN IS REFUSED, NOT WRAPPED.** Asking for bar 1 of
 * a one-bar pattern is a caller mistake, and silently exporting bar 0's
 * music instead is worse than refusing -- the same law as t205's refusal to
 * sort the caller's out-of-order events.
 *
 * **THE SAMPLE POSITION COMES FROM THE MAP, NOT FROM A DIVISION.** Ticks to
 * samples is `ri_map_tick`'s job, with its one documented rounding; this
 * module does not compute `samples_per_step * step` as a second answer. They
 * disagree by a sample per bar at some tempos, and a per-bar disagreement is
 * a seam that drifts.
 *
 * Pure arithmetic: no allocation, no IO, no engine. The caller still renders.
 */
#ifndef RI_LOOPRENDER_H
#define RI_LOOPRENDER_H
#include <stdint.h>
#include "engine/seq/clock.h"
#include "engine/seq/sched.h"

/* 16 sixteenth-note steps to the 4/4 bar. Named here rather than left as a
 * bare 16 so the refusal message can say what it means. */
#define RI_LOOP_STEPS_PER_BAR 16u

struct RILoopSpan {
    uint64_t start_tick;    /* the chosen bar's first tick */
    uint64_t bar_ticks;     /* one bar, from the map's ppq */
    uint64_t body_ticks;    /* `bars` bars, never including the tail */
    uint64_t body_samples;  /* EXACT: the loop a DAW warps */
    uint32_t tail_samples;  /* 0 when there is no tail */
    uint64_t total_samples; /* body + tail: what to render */
    uint64_t bar_samples;   /* one bar in samples; body_samples is a multiple */
};

/* Returns 0 on success, non-zero on any refusal: NULL span/map/steps, a ppq
 * of 0 (which would divide by zero), no steps, a pattern that is not a whole
 * number of bars, a `from` past the end, zero or absurd `bars`, or a
 * `tail_ms` that does not land on a whole number of samples.
 *
 * `tail_ms` is milliseconds because that is what a person types; the refusal
 * for a fractional sample is the same one the module would make for a
 * fraction of a bar. */
int ri_loop_span(struct RILoopSpan *sp, const struct RITempoMap *map,
    uint32_t ppq, const struct RIStep *steps, uint32_t nsteps,
    uint32_t from_bar, uint32_t bars, uint32_t tail_ms);

#endif