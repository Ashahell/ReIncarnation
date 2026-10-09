/* t185_midi_clockout — M5a: the clock-out schedule is a pure function of
 * the render's sample clock.
 *
 * The rule this pins is the one MIDI 1.0 asks for and the one an
 * accumulator gets wrong: 24 ppqn must not drift over a long take. The
 * design that cannot drift is not an accumulator at all — the tick count
 * at an absolute sample position is computed from that position, so there
 * is nothing to accumulate and nothing to round twice.
 *
 * The tempo law is M3f's, applied to the output side before it bites: a
 * tempo change re-anchors at the change position, so ticks already
 * emitted keep the sample positions they were emitted at. An accumulator
 * that merely divides by the new rate re-times the past, which is exactly
 * what made the M3 owner's ear hear "playback speed suffers".
 */
#include <stdio.h>
#include "tests/helpers/ri_assert.h"
#include "midi_io/midi_clockout.h"

#define SR 48000u
#define TENSEC ((uint64_t)SR * 600u)

/* Ticks owed between two absolute positions. */
static uint64_t owed(const struct RIMidiClockOut *c, uint64_t from,
    uint64_t to) {
    return (uint64_t)midi_clockout_ticks(c, to) -
        (uint64_t)midi_clockout_ticks(c, from);
}

int main(void) {
    static struct RIMidiClockOut c;
    uint64_t s, prev, d, n;
    uint32_t maxd, mind;
    uint32_t total;

    /* --- 120 BPM at 48 kHz is exactly 1000 samples per tick ------------- */
    midi_clockout_init(&c, SR, 120000u, RI_MIDICLK_PPQ, 0u);
    RI_ASSERT(midi_clockout_ticks(&c, 48000u) == 0u,
        "a freshly initialised clock is stopped and owes nothing");
    midi_clockout_start(&c, 0u);
    RI_ASSERT(midi_clockout_ticks(&c, 0u) == 0u, "no tick before the anchor");
    RI_ASSERT(midi_clockout_ticks(&c, 1u) == 0u, "one sample is not a tick");
    RI_ASSERT(midi_clockout_ticks(&c, 999u) == 0u, "999 samples is not a tick");
    RI_ASSERT(midi_clockout_ticks(&c, 1000u) == 1u, "1000 samples is one tick");
    RI_ASSERT(midi_clockout_ticks(&c, 1000000u) == 1000u, "a million samples");

    /* Ten minutes is the plan's no-drift bound: 28800 ticks, exact. */
    total = (uint32_t)midi_clockout_ticks(&c, TENSEC);
    RI_ASSERT(total == 28800u, "10 min at 120 BPM = %u ticks", total);

    /* --- 140 BPM: 56 ticks per 48000 samples, and the intervals alternate
     * 857/858 rather than drifting ------------------------------------ */
    midi_clockout_init(&c, SR, 140000u, RI_MIDICLK_PPQ, 0u);
    midi_clockout_start(&c, 0u);
    RI_ASSERT(midi_clockout_ticks(&c, 48000u) == 56u, "one second at 140 BPM");
    total = (uint32_t)midi_clockout_ticks(&c, TENSEC);
    RI_ASSERT(total == 33600u, "10 min at 140 BPM = %u ticks", total);

    /* Walk the whole ten minutes sample by sample. Every tick fires
     * exactly once and in order (a dropped or repeated tick breaks that),
     * and every interval between consecutive ticks is floor(48000/56)=857
     * or 858 — a third value is drift, and a zero is a burst. */
    maxd = 0u;
    mind = 1000u;
    prev = 0u;
    n = 0u;
    for (s = 1u; s <= TENSEC; s++) {
        uint32_t t = (uint32_t)midi_clockout_ticks(&c, s);
        if (t == (uint32_t)n)
            continue;
        RI_ASSERT(t == (uint32_t)n + 1u,
            "tick %u->%u at sample %llu (skipped or repeated)",
            (unsigned)n, (unsigned)t, (unsigned long long)s);
        d = s - prev;
        RI_ASSERT(d == 857u || d == 858u,
            "tick interval %llu at tick %llu is not 857/858",
            (unsigned long long)d, (unsigned long long)n);
        if (d > maxd)
            maxd = d;
        if (d < mind)
            mind = d;
        prev = s;
        n = t;
    }
    RI_ASSERT(n == 33600u, "walked %llu ticks", (unsigned long long)n);
    /* Both interval values must occur: a schedule that only ever emits
     * 857 would have drifted a whole tick per second, and one that only
     * ever emits 858 would have run fast. Exactly 56 x 857 = 47992 samples
     * per second, so 8 of every 56 intervals are the long one. */
    RI_ASSERT(mind == 857u && maxd == 858u, "interval range %u..%u", mind, maxd);

    /* --- Start anchors where it is told, not at sample 0 ---------------- */
    midi_clockout_init(&c, SR, 120000u, RI_MIDICLK_PPQ, 0u);
    midi_clockout_start(&c, 500000u);
    RI_ASSERT(midi_clockout_ticks(&c, 500000u) == 0u, "tick 0 at the anchor");
    RI_ASSERT(midi_clockout_ticks(&c, 501000u) == 1u, "one tick after 1000");
    RI_ASSERT(midi_clockout_ticks(&c, 499000u) == 0u,
        "nothing is owed before the anchor (%llu)",
        (unsigned long long)midi_clockout_ticks(&c, 499000u));

    /* Stop owes nothing; Continue re-anchors and resumes from zero. */
    midi_clockout_stop(&c);
    RI_ASSERT(midi_clockout_ticks(&c, 900000u) == 0u, "stopped owes nothing");
    midi_clockout_start(&c, 900000u);
    RI_ASSERT(midi_clockout_ticks(&c, 901000u) == 1u,
        "continue restarts the count");

    /* --- M3f's law on the output side: a tempo change must not re-time
     * the past. Ticks already emitted keep their positions. ----------- */
    midi_clockout_init(&c, SR, 120000u, RI_MIDICLK_PPQ, 0u);
    midi_clockout_start(&c, 0u);
    RI_ASSERT(midi_clockout_ticks(&c, 10000u) == 10u, "ten ticks before");
    midi_clockout_set_bpm(&c, 140000u, 10000u);
    RI_ASSERT(midi_clockout_ticks(&c, 10000u) == 10u,
        "the change does not move the past (%llu)",
        (unsigned long long)midi_clockout_ticks(&c, 10000u));
    /* One second after the change at 140 BPM is 56 more ticks, with no
     * burst at the change itself to "catch up" the old rate. */
    RI_ASSERT(owed(&c, 10000u, 11000u) == 1u,
        "1000 samples at 140 BPM = %llu ticks",
        (unsigned long long)owed(&c, 10000u, 11000u));
    RI_ASSERT(owed(&c, 10000u, 58000u) == 56u,
        "one second after the change = %llu ticks",
        (unsigned long long)owed(&c, 10000u, 58000u));

    /* --- output latency compensation leads, it does not lag ----------- */
    midi_clockout_init(&c, SR, 120000u, RI_MIDICLK_PPQ, 512u);
    midi_clockout_start(&c, 0u);
    RI_ASSERT(midi_clockout_ticks(&c, 1000u) == 1u,
        "with 512 samples of latency the first tick comes early (%llu)",
        (unsigned long long)midi_clockout_ticks(&c, 1000u));
    /* The tick is due AT 488 (0 + 1000 - 512), not one sample later. */
    RI_ASSERT(midi_clockout_ticks(&c, 487u) == 0u, "not before its time");
    RI_ASSERT(midi_clockout_ticks(&c, 488u) == 1u, "due exactly on time");
    midi_clockout_init(&c, SR, 120000u, RI_MIDICLK_PPQ, 0u);
    midi_clockout_start(&c, 0u);
    RI_ASSERT(midi_clockout_ticks(&c, 488u) == 0u, "no latency, no lead");

    /* --- bad arguments fail closed, never divide by zero --------------- */
    RI_ASSERT(midi_clockout_ticks(0, 0u) == 0u, "null clock");
    midi_clockout_init(&c, 0u, 120000u, RI_MIDICLK_PPQ, 0u);
    midi_clockout_start(&c, 0u);
    RI_ASSERT(midi_clockout_ticks(&c, 48000u) == 0u,
        "a zero sample rate owes nothing");
    midi_clockout_init(&c, SR, 0u, RI_MIDICLK_PPQ, 0u);
    midi_clockout_start(&c, 0u);
    RI_ASSERT(midi_clockout_ticks(&c, 48000u) == 0u,
        "a zero tempo owes nothing");
    midi_clockout_init(&c, SR, 120000u, 0u, 0u);
    midi_clockout_start(&c, 0u);
    RI_ASSERT(midi_clockout_ticks(&c, 48000u) == 0u,
        "a zero ppq is fail-closed, not a divide by zero");

    RI_RESULT("midi-clockout");
}
