/* t156_render_stages — per-stage render accounting, and the playing/stopped
 * split that the "0.6 % of a core" error came from.
 *
 * The session render cost was only ever known as one aggregate, and that
 * aggregate was quoted as though it described a playing buffer when it
 * averaged a 7-hour session that was ~99 % idle. Recomputed over playback it
 * looked like 53-54 % of a 5333 us period — but that number was itself a
 * session average, so it could not be trusted either. These counters are per
 * stage, and RI_LIVE_ST_STOPPED is deliberately separated from the playing
 * stages so idle and playing can never be averaged together again.
 *
 * The clock below advances a fixed tick per READ. A stage opens with one read
 * and closes with the next, so a stage that behaves costs exactly ONE tick —
 * that is the arithmetic the exact-sum laws rely on, and it is what makes a
 * stage that reads the clock twice detectable.
 *
 * Laws:
 *  - with no clock injected, nothing accumulates, but the PATH taken is still
 *    counted: an un-timed caller must be able to say it took the silence path;
 *  - every PLAYING stage runs exactly once per playing buffer, so a bucket
 *    renumbered onto another is caught by the count, not by a coincidence;
 *  - one render costs each playing stage exactly one tick, and ten renders
 *    cost exactly ten: the sum accumulates rather than being overwritten;
 *  - a sample larger than 0xFFFFFFFF clamps to 0xFFFFFFFF, and is neither
 *    zeroed nor allowed to wrap the accumulator;
 *  - the playing and silence paths are disjoint: stopping accumulates only
 *    ST_STOPPED and leaves every playing stage untouched;
 *  - setting the clock back to NULL stops accumulation without a re-init.
 */
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "tests/helpers/ri_assert.h"
#include "engine/live.h"
#include "engine/seq/pattern.h"
#include "engine/seq/songtrack.h"

#define SR 48000.0f
#define BPM 120.0f
#define PPQ 96u
#define TICK 100u

static struct RIPatternBank BA;
static struct RISongTrack TR;
static struct RIEvent SCR1[512];

static uint64_t g_ticks;
static uint64_t g_tick_us = TICK;
static uint32_t g_reads; /* every clock read is counted, not just its value */

static uint64_t tick_clock(void) {
    g_reads++;
    g_ticks += g_tick_us;
    return g_ticks;
}

static void fixture(struct RILiveSession *s, int with_clock) {
    const struct RIPatternBank *b4[5];
    uint32_t q;
    ri_bank_init(&BA, 0u, RI_PATTERN_KIND_303, 0u);
    for (q = 0u; q < 32u; q++)
        ri_pattern_set_length(&BA.pat[q], 16u);
    for (q = 0u; q < 16u; q++)
        ri_p303_set(&BA.pat[0], q, 6u, (q == 0u) ? 0u : (uint8_t)RI_STEP_REST);
    ri_track_init(&TR);
    b4[0] = &BA; b4[1] = 0; b4[2] = 0; b4[3] = 0; b4[4] = 0;
    ri_live_init(s, PPQ, SR, BPM, RI_ENGINE_S303A, SCR1, 512u);
    ri_live_set_banks(s, b4, &TR, 0);
    g_ticks = 0u;
    g_reads = 0u;
    if (with_clock) {
        /* Two calls, on purpose: the hooks are independent so a simulated
         * clock is not perturbed by the engine's nested reads. */
        ri_live_set_clock(s, tick_clock);
        ri_engine_set_clock(&s->eng, tick_clock);
    }
}

static void render_n(struct RILiveSession *s, uint32_t n) {
    static float fl[256], fr[256];
    uint32_t i;
    for (i = 0u; i < n; i++)
        (void)ri_live_render(s, fl, fr, 256u);
}

int main(void) {
    struct RILiveSession s;
    const struct RILiveStages *g;

    /* Stage indices are dense from 0: the heartbeat loops over them. */
    RI_ASSERT(RI_LIVE_ST_COUNT == 8, "eight stages (%u)", (unsigned)RI_LIVE_ST_COUNT);
    RI_ASSERT(RI_LIVE_ST_STOPPED == 0u, "STOPPED is first");
    RI_ASSERT(RI_LIVE_ST_TOTAL == RI_LIVE_ST_COUNT - 1u, "TOTAL is last");
    RI_ASSERT(ri_live_stages(NULL) == 0, "NULL session -> NULL");

    /* --- no clock: no samples, but the path is still known --- */
    fixture(&s, 0);
    g = ri_live_stages(&s);
    RI_ASSERT(g->sum_us[RI_LIVE_ST_STOPPED] == 0u, "no clock, no sum");
    render_n(&s, 5u);
    g = ri_live_stages(&s);
    RI_ASSERT(g->stopped_buffers == 5u, "stopped path counted (%lu)",
        (unsigned long)g->stopped_buffers);
    RI_ASSERT(g->playing_buffers == 0u, "nothing played yet");
    RI_ASSERT(g->n[RI_LIVE_ST_STOPPED] == 0u, "no samples without a clock");
    ri_live_play(&s);
    render_n(&s, 4u);
    g = ri_live_stages(&s);
    RI_ASSERT(g->playing_buffers == 4u, "playing path counted (%lu)",
        (unsigned long)g->playing_buffers);
    RI_ASSERT(g->n[RI_LIVE_ST_TOTAL] == 0u, "still no samples without a clock");

    /* --- one render, one tick per stage --- */
    fixture(&s, 1);
    ri_live_play(&s);
    render_n(&s, 1u);
    g = ri_live_stages(&s);
    RI_ASSERT(g->playing_buffers == 1u, "one playing buffer (%lu)",
        (unsigned long)g->playing_buffers);
    RI_ASSERT(g->stopped_buffers == 0u, "the silence path was not taken");
    {
        unsigned q;
        for (q = RI_LIVE_ST_EVENTS; q < RI_LIVE_ST_TOTAL; q++) {
            RI_ASSERT(g->n[q] == 1u,
                "playing stage %u ran once (%lu)", q, (unsigned long)g->n[q]);
            /* ST_DSP is excluded: it CONTAINS ri_engine_render, which carries
             * its own sub-stage instrumentation and so reads the clock many
             * times per block. A live stage that wraps other instrumentation
             * can never cost exactly one tick, and asserting that it did
             * would be asserting a lie. It is checked by the bracket law and by
             * its own n == playing_buffers instead. */
            if (q == RI_LIVE_ST_DSP)
                continue;
            RI_ASSERT(g->sum_us[q] == g_tick_us,
                "playing stage %u cost exactly one clock tick (%lu, want %lu)", q,
                (unsigned long)g->sum_us[q], (unsigned long)g_tick_us);
            RI_ASSERT(g->max_us[q] == g_tick_us,
                "playing stage %u max is the sample (%lu)", q,
                (unsigned long)g->max_us[q]);
        }
    }

    /* --- accumulation, not overwrite --- */
    render_n(&s, 9u);
    g = ri_live_stages(&s);
    RI_ASSERT(g->playing_buffers == 10u, "ten playing buffers (%lu)",
        (unsigned long)g->playing_buffers);
    RI_ASSERT(g->n[RI_LIVE_ST_TOTAL] == 10u, "TOTAL has ten samples (%lu)",
        (unsigned long)g->n[RI_LIVE_ST_TOTAL]);
    /* TOTAL is the ENCLOSING measurement: it opens before every stage and
     * closes after, so it reads the clock many times and is deliberately not
     * one tick. What it must do is bracket its parts. */
    {
        uint64_t parts = 0u;
        unsigned q;
        for (q = RI_LIVE_ST_EVENTS; q < RI_LIVE_ST_TOTAL; q++)
            parts += g->sum_us[q];
        RI_ASSERT(g->sum_us[RI_LIVE_ST_TOTAL] >= parts,
            "TOTAL brackets the stages inside it (%lu >= %lu)",
            (unsigned long)g->sum_us[RI_LIVE_ST_TOTAL], (unsigned long)parts);
        RI_ASSERT(g->sum_us[RI_LIVE_ST_TOTAL] >= g->sum_us[RI_LIVE_ST_DSP],
            "TOTAL brackets DSP");
    }
    {
        unsigned q;
        for (q = RI_LIVE_ST_EVENTS; q < RI_LIVE_ST_TOTAL; q++) {
            RI_ASSERT(g->n[q] == 10u, "playing stage %u ran ten times (%lu)", q,
                (unsigned long)g->n[q]);
            if (q == RI_LIVE_ST_DSP)
                continue;
            RI_ASSERT(g->sum_us[q] == 10u * g_tick_us,
                "playing stage %u accumulated ten ticks (%lu)", q,
                (unsigned long)g->sum_us[q]);
            RI_ASSERT(g->max_us[q] == g_tick_us,
                "playing stage %u max stays one tick (%lu)", q,
                (unsigned long)g->max_us[q]);
        }
        /* The wrapping stage must still have moved, and it must cost strictly
         * more than a stage with no nested instrumentation: it contains the
         * engine's sub-stage reads. */
        RI_ASSERT(g->sum_us[RI_LIVE_ST_DSP] > 10u * g_tick_us,
            "DSP costs more than one tick per buffer (%lu) because it contains the\n"
            "            engine's own instrumentation", (unsigned long)g->sum_us[RI_LIVE_ST_DSP]);
    }

    /* --- a saturated sample clamps --- */
    {
        uint64_t save = g_tick_us;
        fixture(&s, 1);
        g_tick_us = 6000000000ULL; /* one tick exceeds 0xFFFFFFFF us */
        g_ticks = 0u;
        ri_live_play(&s);
        render_n(&s, 1u);
        g = ri_live_stages(&s);
        RI_ASSERT(g->max_us[RI_LIVE_ST_DSP] == 0xFFFFFFFFu,
            "a saturated sample clamps to 0xFFFFFFFF (%lu)",
            (unsigned long)g->max_us[RI_LIVE_ST_DSP]);
        RI_ASSERT(g->sum_us[RI_LIVE_ST_DSP] == 0xFFFFFFFFu,
            "and the accumulator holds the clamped value (%lu)",
            (unsigned long)g->sum_us[RI_LIVE_ST_DSP]);
        g_tick_us = save;
    }

    /* --- the paths are disjoint --- */
    {
        uint32_t tot_before, stop_before;
        uint64_t dsp_before, total_us_before;
        fixture(&s, 1);
        ri_live_play(&s);
        render_n(&s, 3u);
        g = ri_live_stages(&s);
        tot_before = g->n[RI_LIVE_ST_TOTAL];
        stop_before = g->n[RI_LIVE_ST_STOPPED];
        dsp_before = g->sum_us[RI_LIVE_ST_DSP];
        total_us_before = g->sum_us[RI_LIVE_ST_TOTAL];
        ri_live_stop(&s);
        render_n(&s, 3u);
        g = ri_live_stages(&s);
        RI_ASSERT(g->stopped_buffers == 3u, "three stopped (%lu)",
            (unsigned long)g->stopped_buffers);
        RI_ASSERT(g->n[RI_LIVE_ST_STOPPED] == stop_before + 3u,
            "STOPPED gained three samples");
        RI_ASSERT(g->n[RI_LIVE_ST_TOTAL] == tot_before,
            "playing counts untouched by the silence path");
        RI_ASSERT(g->sum_us[RI_LIVE_ST_DSP] == dsp_before,
            "DSP untouched by the silence path");
        RI_ASSERT(g->sum_us[RI_LIVE_ST_TOTAL] == total_us_before,
            "TOTAL untouched by the silence path");
    }

    /* --- the clock can be taken away without a re-init --- */
    {
        uint32_t before;
        fixture(&s, 1);
        ri_live_play(&s);
        render_n(&s, 2u);
        g = ri_live_stages(&s);
        before = g->n[RI_LIVE_ST_TOTAL];
        ri_live_set_clock(&s, 0);
        render_n(&s, 2u);
        g = ri_live_stages(&s);
        RI_ASSERT(g->playing_buffers == 4u, "the path is still counted (%lu)",
            (unsigned long)g->playing_buffers);
        RI_ASSERT(g->n[RI_LIVE_ST_TOTAL] == before,
            "but no samples accumulate once the clock is gone");
    }

    /* ---- the DSP sub-stage table, which is per BLOCK not per buffer ---- */
    RI_ASSERT(RI_ENGINE_ST_COUNT == 8, "eight DSP sub-stages");
    RI_ASSERT(RI_ENGINE_ST_TOTAL == RI_ENGINE_ST_COUNT - 1u, "block TOTAL is last");
    RI_ASSERT(ri_engine_stages(NULL) == 0, "NULL engine -> NULL");

    fixture(&s, 1);
    ri_live_play(&s);
    render_n(&s, 1u);
    {
        const struct RIEngineStages *h = ri_engine_stages(&s.eng);
        unsigned q;
        RI_ASSERT(h->n[RI_ENGINE_ST_TOTAL] >= 1u,
            "the engine's block loop ran (%lu blocks)", (unsigned long)h->n[RI_ENGINE_ST_TOTAL]);
        /* Every sub-stage runs exactly once per block, so they all share the
         * block count. A sub-stage numbered onto another, or opened without
         * being closed, breaks this immediately. */
        for (q = 0u; q < RI_ENGINE_ST_COUNT; q++)
            RI_ASSERT(h->n[q] == h->n[RI_ENGINE_ST_TOTAL],
                "DSP sub-stage %u ran once per block (%lu of %lu)", q,
                (unsigned long)h->n[q], (unsigned long)h->n[RI_ENGINE_ST_TOTAL]);
        /* The block loop is bounded by RI_ENGINE_BLOCK, so a 256-frame render
         * needs at least 256/64 blocks and can never need fewer. This is the
         * only thing standing between the reader and the assumption that the
         * block count equals the buffer count -- it does not, and the numbers
         * are only honest if the test admits that. */
        RI_ASSERT(h->n[RI_ENGINE_ST_TOTAL] >= 256u / RI_ENGINE_BLOCK,
            "at least ceil(frames/block) blocks per render (%lu)",
            (unsigned long)h->n[RI_ENGINE_ST_TOTAL]);
        /* Every LEAF sub-stage costs exactly one tick per block: it opens with
         * one read and closes with the next, and nothing nests inside a block.
         * That is what makes a stage which reads the clock twice, or is never
         * closed, visible here. */
        for (q = 0u; q < RI_ENGINE_ST_TOTAL; q++)
            RI_ASSERT(h->sum_us[q] == (uint64_t)h->n[q] * g_tick_us,
                "DSP sub-stage %u cost one tick per block (%lu, want %lu)", q,
                (unsigned long)h->sum_us[q],
                (unsigned long)((uint64_t)h->n[q] * g_tick_us));
        /* The block TOTAL opens before every leaf and closes after, so it spans
         * exactly 2*COUNT reads and therefore 2*COUNT-1 ticks. Stating it
         * exactly is what catches a sub-stage boundary that moved or vanished:
         * the total would still be a plausible number, but not this one. */
        RI_ASSERT(h->sum_us[RI_ENGINE_ST_TOTAL]
                == (uint64_t)h->n[RI_ENGINE_ST_TOTAL]
                    * (2u * RI_ENGINE_ST_COUNT - 1u) * g_tick_us,
            "the block TOTAL spans exactly %u ticks per block (%lu, want %lu)",
            2u * RI_ENGINE_ST_COUNT - 1u,
            (unsigned long)h->sum_us[RI_ENGINE_ST_TOTAL],
            (unsigned long)((uint64_t)h->n[RI_ENGINE_ST_TOTAL]
                * (2u * RI_ENGINE_ST_COUNT - 1u) * g_tick_us));
    }

    /* max >= avg for every sub-stage that has samples: without this the max
     * column is unchecked, which is how a "keep the fastest block" mutant
     * survives. */
    {
        const struct RIEngineStages *h = ri_engine_stages(&s.eng);
        unsigned q;
        for (q = 0u; q < RI_ENGINE_ST_COUNT; q++)
            if (h->n[q])
                RI_ASSERT(h->max_us[q] >= h->sum_us[q] / h->n[q],
                    "DSP sub-stage %u max >= avg (%lu >= %lu)", q,
                    (unsigned long)h->max_us[q],
                    (unsigned long)(h->sum_us[q] / h->n[q]));
    }

    /* A saturated DSP sample clamps rather than wrapping or zeroing. */
    {
        uint64_t save = g_tick_us;
        fixture(&s, 1);
        g_tick_us = 6000000000ULL; /* one tick exceeds 0xFFFFFFFF us */
        g_ticks = 0u;
        ri_live_play(&s);
        render_n(&s, 1u);
        RI_ASSERT(ri_engine_stages(&s.eng)->max_us[RI_ENGINE_ST_VOICES] == 0xFFFFFFFFu,
            "a saturated DSP sample clamps to 0xFFFFFFFF (%lu)",
            (unsigned long)ri_engine_stages(&s.eng)->max_us[RI_ENGINE_ST_VOICES]);
        g_tick_us = save;
    }

    /* Arming a table must not read the clock. A read here is invisible in the
     * measured samples (a constant offset cancels in every difference) and so
     * cannot be caught by any of the arithmetic laws -- but it is very much
     * visible to a caller whose clock is a simulation that advances per read,
     * which is how t88 was broken twice. So the read count is the law. */
    {
        uint32_t before;
        fixture(&s, 0);
        before = g_reads;
        ri_live_set_clock(&s, tick_clock);
        RI_ASSERT(g_reads == before,
            "ri_live_set_clock reads the clock %lu time(s)", (unsigned long)(g_reads - before));
        before = g_reads;
        ri_engine_set_clock(&s.eng, tick_clock);
        RI_ASSERT(g_reads == before,
            "ri_engine_set_clock reads the clock %lu time(s)",
            (unsigned long)(g_reads - before));
    }

    /* TOTAL brackets its leaves. It is deliberately NOT required to equal
     * them: the synthetic clock advances once per READ, so the wrapper's own
     * two reads and the eight boundary reads land inside it as extra ticks.
     * "No unattributed gap" is therefore a law of the DEVICE numbers, not of
     * this model -- and it is the law that caught a stage opened and closed
     * back to back on the target, where it showed up as 61 us that no stage
     * claimed. */
    {
        const struct RIEngineStages *h = ri_engine_stages(&s.eng);
        uint64_t parts = 0u;
        unsigned q;
        for (q = 0u; q < RI_ENGINE_ST_TOTAL; q++)
            parts += h->sum_us[q];
        RI_ASSERT(h->sum_us[RI_ENGINE_ST_TOTAL] >= parts,
            "the block TOTAL brackets its leaves (%lu >= %lu)",
            (unsigned long)h->sum_us[RI_ENGINE_ST_TOTAL], (unsigned long)parts);
    }

    /* Taking the clocks away stops both tables. */
    {
        uint32_t before_n;
        const struct RIEngineStages *h;
        fixture(&s, 1);
        ri_live_play(&s);
        render_n(&s, 2u);
        h = ri_engine_stages(&s.eng);
        before_n = h->n[RI_ENGINE_ST_TOTAL];
        ri_live_set_clock(&s, 0);
        ri_engine_set_clock(&s.eng, 0);
        render_n(&s, 2u);
        h = ri_engine_stages(&s.eng);
        RI_ASSERT(h->n[RI_ENGINE_ST_TOTAL] == before_n,
            "the engine's table stops with the session's (%lu)",
            (unsigned long)h->n[RI_ENGINE_ST_TOTAL]);
        RI_ASSERT(ri_live_stages(&s)->n[RI_LIVE_ST_TOTAL] == 4u - 2u,
            "and the session table stopped too");
    }

    /* The two hooks are INDEPENDENT, and that independence is load-bearing:
     * the DSP sub-stages sit INSIDE the render, so cascading one clock into
     * both would add ~16 reads per block, which for a host test whose clock
     * advances per read means a simulated render several times longer than
     * intended. t88 is calibrated for exactly two reads per buffer and its
     * governor laws all failed when the cascade was in. Asserting the
     * independence keeps it from being "tidied" back. */
    {
        const struct RIEngineStages *h;
        fixture(&s, 0);
        ri_live_set_clock(&s, tick_clock);           /* session only */
        ri_live_play(&s);
        render_n(&s, 1u);
        h = ri_engine_stages(&s.eng);
        RI_ASSERT(ri_live_stages(&s)->n[RI_LIVE_ST_TOTAL] == 1u,
            "the session table is armed");
        RI_ASSERT(h->n[RI_ENGINE_ST_TOTAL] == 0u,
            "and the session clock did NOT arm the engine's (%lu blocks)",
            (unsigned long)h->n[RI_ENGINE_ST_TOTAL]);
        fixture(&s, 0);
        ri_engine_set_clock(&s.eng, tick_clock);     /* engine only */
        ri_live_play(&s);
        render_n(&s, 1u);
        h = ri_engine_stages(&s.eng);
        RI_ASSERT(h->n[RI_ENGINE_ST_TOTAL] >= 1u, "the engine table is armed");
        RI_ASSERT(ri_live_stages(&s)->n[RI_LIVE_ST_TOTAL] == 0u,
            "and the engine clock did NOT arm the session's");
    }

    /* An engine that was never given a clock reports nothing, not garbage. */
    {
        struct RILiveSession fresh;
        fixture(&fresh, 0);
        ri_live_play(&fresh);
        render_n(&fresh, 1u);
        RI_ASSERT(ri_engine_stages(&fresh.eng)->n[RI_ENGINE_ST_TOTAL] == 0u,
            "an un-timed caller gets an empty DSP table, not a wrong one");
        RI_ASSERT(ri_engine_stages(&fresh.eng)->sum_us[RI_ENGINE_ST_VOICES] == 0u,
            "and no sums");
    }

    RI_RESULT("render_stages");
}