/* t190_midi_interval — arrival-interval statistics over a clock wire.
 *
 * The point of this module is what it does NOT do: it never reads a clock.
 * Arrival stamps are handed to it by whoever actually saw the message
 * arrive. MIDIRX read its own clock at the top of a poll loop, so it
 * measured how often it looked rather than how often bytes came — at 140
 * BPM, where ticks are ~17.9 ms apart, that quantisation would have been
 * most of the reported spread. A measurement instrument that timestamps
 * itself is not a measurement instrument.
 *
 * Laws:
 * - an interval is the gap between two CONSECUTIVE ARRIVALS, so the caller
 *   must stamp on arrival, not on inspection;
 * - fewer than two arrivals is NO INTERVAL, not a zero (a zero interval
 *   would average in and mean the sender burst);
 * - stamps that do not advance must not underflow into an enormous
 *   interval: a clock that went backwards is clamped to 1 us and counted,
 *   because hiding it would corrupt the mean;
 * - drift is visible, not averaged away: per-slice counts over the run's
 *   own window, so a stream that speeds up shows as unequal slices.
 */
#include <stdio.h>
#include "tests/helpers/ri_assert.h"
#include "midi_io/midi_interval.h"

int main(void) {
    static struct RIMidiInterval s;
    uint32_t i, spread, mean;

    /* --- too few arrivals is no interval, not a zero -------------------- */
    midi_interval_init(&s, 0u);
    RI_ASSERT(midi_interval_count(&s) == 0u, "empty");
    RI_ASSERT(midi_interval_intervals(&s) == 0u, "no intervals yet");
    RI_ASSERT(midi_interval_verdict(&s) == RI_MIDIINT_NONE, "verdict NONE");

    midi_interval_add(&s, 1000000ULL);
    RI_ASSERT(midi_interval_count(&s) == 1u, "one arrival");
    RI_ASSERT(midi_interval_intervals(&s) == 0u, "still no interval");
    RI_ASSERT(midi_interval_verdict(&s) == RI_MIDIINT_NONE,
        "one arrival cannot be steady or jittery");

    /* --- two arrivals define exactly one interval ----------------------- */
    midi_interval_add(&s, 1020000ULL);
    RI_ASSERT(midi_interval_count(&s) == 2u, "two arrivals");
    RI_ASSERT(midi_interval_intervals(&s) == 1u, "one interval");
    RI_ASSERT(midi_interval_min_us(&s) == 20000u && midi_interval_max_us(&s) == 20000u,
        "the gap is 20000 us (%u..%u)", midi_interval_min_us(&s),
        midi_interval_max_us(&s));
    RI_ASSERT(midi_interval_mean_us(&s) == 20000u, "mean 20000");
    RI_ASSERT(midi_interval_jitter_us(&s) == 0u, "no jitter in one interval");
    RI_ASSERT(midi_interval_verdict(&s) == RI_MIDIINT_STEADY,
        "one interval cannot be called jittery");

    /* --- a perfectly regular stream ------------------------------------ */
    midi_interval_init(&s, 0u);
    for (i = 0u; i <= 100u; i++)
        midi_interval_add(&s, 1000000ULL + (uint64_t)i * 20000ULL);
    RI_ASSERT(midi_interval_intervals(&s) == 100u, "100 intervals (%u)",
        midi_interval_intervals(&s));
    RI_ASSERT(midi_interval_jitter_us(&s) == 0u, "a regular stream has no jitter");
    RI_ASSERT(midi_interval_verdict(&s) == RI_MIDIINT_STEADY, "STEADY");

    /* --- the 140 BPM case: 857/858 alternating, spread 1 ---------------- */
    /* 48000 samples/tick-second at 48 kHz and 56 ticks/s, so 1000 arrivals
     * span 1000*48000/56 samples = 857142 us with alternating gaps. */
    midi_interval_init(&s, 0u);
    {
        uint64_t t = 1000000ULL;
        for (i = 0u; i < 1000u; i++) {
            midi_interval_add(&s, t);
            t += 857u + ((i % 7u == 6u) ? 1u : 0u);
        }
    }
    RI_ASSERT(midi_interval_min_us(&s) == 857u, "min gap 857 (%u)",
        midi_interval_min_us(&s));
    RI_ASSERT(midi_interval_max_us(&s) == 858u, "max gap 858 (%u)",
        midi_interval_max_us(&s));
    RI_ASSERT(midi_interval_jitter_us(&s) == 1u, "spread of 1 us is the floor");
    mean = midi_interval_mean_us(&s);
    RI_ASSERT(midi_interval_verdict(&s) == RI_MIDIINT_STEADY,
        "a 1 us spread is STEADY (mean %u)", (unsigned)mean);

    /* --- the bound needs BOTH halves, so both are pinned --------------- */
    /* Modest wobble on a slow clock: 1000 us of spread on a 20 ms mean is
     * 5%, inside the percentage half. A percentage-only bound also passes
     * this, but a fixed-allowance-only bound would call it JITTERY — which
     * is right, because 1 ms of wobble on a 20 ms tick IS audible as
     * flam on a hi-hat. */
    midi_interval_init(&s, 0u);
    {
        uint64_t t = 1000000ULL;
        for (i = 0u; i <= 20u; i++) {
            midi_interval_add(&s, t);
            t += 19500ULL + (i % 2u ? 1000ULL : 0ULL);  /* 19500 / 20500 */
        }
    }
    RI_ASSERT(midi_interval_min_us(&s) == 19500u &&
        midi_interval_max_us(&s) == 20500u, "gaps 19500/20500 (%u..%u)",
        (unsigned)midi_interval_min_us(&s), (unsigned)midi_interval_max_us(&s));
    RI_ASSERT(midi_interval_jitter_us(&s) == 1000u, "1 ms of spread (%u)",
        (unsigned)midi_interval_jitter_us(&s));
    RI_ASSERT(midi_interval_verdict(&s) == RI_MIDIINT_STEADY,
        "five percent on a slow clock is STEADY");

    /* The converse, and the reason the bound has two halves. A fast clock
     * where the wobble is a large FRACTION but tiny in absolute terms:
     * gaps 375/525 give a 450 us mean and 150 us of spread — a third of
     * the interval. Judged as a percentage alone that is far too ragged;
     * judged as absolute time it is under the 200 us allowance, which is
     * exactly what a floor is for. Drop the floor from the bound and this
     * case turns JITTERY on a wire that is behaving. */
    midi_interval_init(&s, 0u);
    {
        uint64_t t = 1000000ULL;
        for (i = 0u; i <= 20u; i++) {
            midi_interval_add(&s, t);
            t += 375ULL + (i % 2u ? 150ULL : 0ULL);  /* 375 / 525 */
        }
    }
    RI_ASSERT(midi_interval_mean_us(&s) == 450u, "mean 450 (%u)",
        (unsigned)midi_interval_mean_us(&s));
    RI_ASSERT(midi_interval_jitter_us(&s) == 150u, "150 us of spread (%u)",
        (unsigned)midi_interval_jitter_us(&s));
    RI_ASSERT(midi_interval_verdict(&s) == RI_MIDIINT_STEADY,
        "a third of a very short interval is still inside the allowance");

    /* --- a genuinely drifting stream is caught ------------------------- */
    midi_interval_init(&s, 0u);
    for (i = 0u; i <= 50u; i++)
        midi_interval_add(&s, 1000000ULL + (uint64_t)i * 20000ULL);
    for (i = 0u; i <= 50u; i++)
        midi_interval_add(&s, 2000000ULL + (uint64_t)i * 40000ULL);
    spread = midi_interval_jitter_us(&s);
    RI_ASSERT(spread >= 20000u, "the doubling is visible (%u)", (unsigned)spread);
    RI_ASSERT(midi_interval_verdict(&s) == RI_MIDIINT_JITTERY,
        "and it is called JITTERY");

    /* --- a backwards clock must not underflow -------------------------- */
    midi_interval_init(&s, 0u);
    midi_interval_add(&s, 1000000ULL);
    midi_interval_add(&s, 1005000ULL);
    midi_interval_add(&s, 1002000ULL);     /* earlier than the previous */
    RI_ASSERT(midi_interval_min_us(&s) == 1u,
        "a backwards stamp clamps to 1 us, not 4294907295 (%u)",
        midi_interval_min_us(&s));
    RI_ASSERT(midi_interval_backwards(&s) == 1u, "and it is counted");

    /* --- slices partition the run by ELAPSED time ---------------------- */
    /* 2 s of 20 ms intervals over a 2 s window: 200 ms per slice, so each
     * full slice holds ten and the last absorbs the overrun. */
    midi_interval_init(&s, 2000000u);
    for (i = 0u; i <= 100u; i++)
        midi_interval_add(&s, 1000000ULL + (uint64_t)i * 20000ULL);
    RI_ASSERT(midi_interval_slice_total(&s) == 100u,
        "every interval is counted exactly once across the slices (%u)",
        (unsigned)midi_interval_slice_total(&s));
    /* The interior slices hold exactly ten. The first is one short because
     * no interval completes at elapsed zero, and the last takes the overrun
     * past the window's end rather than dropping it — which is why the
     * total, not any single slice, is the law worth pinning. */
    for (i = 1u; i < RI_MIDIINT_SLICES - 1u; i++)
        RI_ASSERT(midi_interval_slice(&s, i) == 10u,
            "interior slice %u holds ten intervals (%u)", (unsigned)i,
            (unsigned)midi_interval_slice(&s, i));
    RI_ASSERT(midi_interval_slice(&s, 0) == 9u, "slice 0 holds nine (%u)",
        (unsigned)midi_interval_slice(&s, 0));
    RI_ASSERT(midi_interval_slice(&s, RI_MIDIINT_SLICES - 1u) == 11u,
        "slice 9 absorbs the overrun (%u)",
        (unsigned)midi_interval_slice(&s, RI_MIDIINT_SLICES - 1u));

    /* Slicing must be relative to the run's own start, not to the absolute
     * stamp: the same intervals shifted a whole second must land identically.
     * The first implementation offset slice edges against the raw 32-bit
     * stamp, so this is the law it got wrong. */
    midi_interval_init(&s, 2000000u);
    for (i = 0u; i <= 100u; i++)
        midi_interval_add(&s, 4000000000ULL + (uint64_t)i * 20000ULL);
    for (i = 1u; i < RI_MIDIINT_SLICES - 1u; i++)
        RI_ASSERT(midi_interval_slice(&s, i) == 10u,
            "a run past the 32-bit wrap slices the same (slice %u = %u)",
            (unsigned)i, (unsigned)midi_interval_slice(&s, i));
    RI_ASSERT(midi_interval_slice_total(&s) == 100u,
        "and partitions the same way (%u)", (unsigned)midi_interval_slice_total(&s));

    /* --- a stream that speeds up shows up in the slices ---------------- */
    /* First half at 20 ms, second half at 10 ms: the later slices must be
     * denser even though every other statistic is an average over both. */
    midi_interval_init(&s, 1000000u);
    for (i = 0u; i <= 50u; i++)
        midi_interval_add(&s, 1000000ULL + (uint64_t)i * 20000ULL);
    for (i = 1u; i <= 50u; i++)
        midi_interval_add(&s, 2000000ULL + (uint64_t)i * 10000ULL);
    {
        uint32_t slow = midi_interval_slice(&s, 1) + midi_interval_slice(&s, 2);
        uint32_t fast = midi_interval_slice(&s, 8) + midi_interval_slice(&s, 9);
        RI_ASSERT(fast > slow, "the fast half is denser in the later slices "
            "(%u vs %u)", (unsigned)fast, (unsigned)slow);
    }

    /* --- nulls are inert ------------------------------------------------ */
    midi_interval_add(0, 1ULL);
    RI_ASSERT(midi_interval_count(0) == 0u, "null sink");
    RI_ASSERT(midi_interval_min_us(0) == 0u, "null min");

    RI_RESULT("midi-interval");
}
