/* t209_loop_render — R9: a loop that a DAW can warp.
 *
 * "Render N bars from a chosen bar, with an optional tail, so exported loops
 * warp cleanly in a DAW." Everything hard about that sentence is the ADVERB.
 *
 * **THE LOOP BODY'S LENGTH IS COMPUTED, NOT OBSERVED.** Before R9 the tool
 * sized its render from `ev[nev-1].sample + g_rate` — the last event plus a
 * hardcoded one second. That is a length nobody chose: it is whatever the
 * last note happened to be plus a magic constant, so it lands mid-bar and a
 * DAW's warp grid has nothing to sit on. R9 computes the bar boundary from
 * the tempo map and renders EXACTLY that many samples.
 *
 * **THE TAIL IS ADDITIVE AND SEPARATE.** A loop for a DAW is the body; the
 * decay after the last bar is the tail. They are different lengths and
 * conflating them is why loops get one extra click of reverb at the seam.
 * So `ri_loop_span` reports the body and the caller adds the tail.
 *
 * **THE TAIL IS OPTIONAL AND ZERO IS NOT A TAIL.** `--tail 0` means "no
 * tail", not "a tail of zero samples that happens to render" — and the
 * refusal for a negative or absurd tail is a refusal, because a tail longer
 * than the body is a different request wearing a tail's name.
 *
 * **THE SPINE IS AN INTEGER DIVISION OF THE STEP GRID.** Bars are counted in
 * steps, and a bar is 16 steps of 16ths; so `from` and `bars` are in STEPS
 * and the span is `bars * 16` steps. Refusing a `from` past the pattern
 * rather than wrapping is deliberate: a caller asking for bar 5 of a
 * 4-step pattern has made a mistake, and silently wrapping to bar 1 exports
 * music they did not ask for — the same law as the unsorted-event refusal in
 * t205.
 *
 * **THE SAMPLE POSITION COMES FROM THE TEMPO MAP, NOT FROM A DIVISION.**
 * The map is the authority on ticks->samples (one rounding, `ri_map_tick`),
 * and this module goes through it rather than computing
 * `samples_per_step * step` itself. A second answer to that question is a
 * second place to be wrong, and the two disagree by a sample per bar at
 * some tempos -- which is precisely the off-by-a-few-samples that makes a
 * loop seam click.
 */
#include <stdio.h>
#include <string.h>
#include "tests/helpers/ri_assert.h"
#include "engine/seq/clock.h"
#include "engine/seq/sched.h"
#include "project/loop_render.h"

#define RATE 48000u
#define PPQ 96u
#define STEPS_PER_BAR 16u

static struct RISegment seg;
static struct RITempoMap map;

static void tempo_map(uint32_t bpm) {
    seg.start_tick = 0;
    seg.ns_per_quarter = 60000000000ULL / (uint64_t)bpm;
    map.segs = &seg;
    map.n = 1u;
    map.ppq = PPQ;
    map.sr = RATE;
}

int main(void) {
    struct RIStep steps[32];
    struct RIEvent ev[64];
    struct RILoopSpan sp;
    struct RISchedOpts opts;
    uint32_t i, nev;
    /* SIXTEEN STEPS, i.e. ONE BAR. I first wrote this fixture with FOUR
     * steps and asserted it was one bar, which is the module being RIGHT: a
     * bar is 16 sixteenths, and 4 steps is a quarter of one. Worth
     * recording because "4 steps" reads like a bar to anyone who has not
     * counted. */
    memset(steps, 0, sizeof steps);
    for (i = 0u; i < 16u; i++) {
        steps[i].note = (uint8_t)(36u + (i % 12u));
        steps[i].flags = 0u;
    }
    memset(&opts, 0, sizeof opts);
    opts.flam_ms = RI_FLAM_MS_DEFAULT;

    /* --- 1. THE BODY IS WHOLE BARS, AND NOTHING ELSE ------------------ */
    /* The map must exist BEFORE the first call. It did not: §1 ran against a
     * zeroed RITempoMap, so `map.sr == 0` and the module's own "a bar
     * shorter than a sample cannot be looped" refusal fired -- the refusal
     * being right and the test's setup being the thing that was wrong. */
    tempo_map(120u);
    RI_ASSERT(ri_loop_span(&sp, &map, PPQ, steps, 16u, 0u, 1u, 0u) == 0u,
        "one bar of a 1-bar pattern");
    /* 16 steps at 96ppq, 4/4: one bar is 4 * 96 = 384 ticks, and at 120 BPM
     * a quarter is half a second, so a bar is 2 seconds. */
    RI_ASSERT(sp.bar_ticks == 384u, "a bar is 384 ticks (%llu)",
        (unsigned long long)sp.bar_ticks);
    RI_ASSERT(sp.body_ticks == 384u, "one bar is one bar of ticks (%llu)",
        (unsigned long long)sp.body_ticks);
    RI_ASSERT(sp.body_samples == 96000u, "which is 96000 samples at 120 BPM "
        "(%llu)", (unsigned long long)sp.body_samples);

    /* The whole POINT: the body is a multiple of the bar, so a DAW's warp
     * grid has a bar line to sit on. */
    RI_ASSERT(sp.body_samples % sp.bar_samples == 0u,
        "the body is a whole number of bars (%llu of %llu)",
        (unsigned long long)sp.body_samples,
        (unsigned long long)sp.bar_samples);

    /* --- 2. THE SAMPLE COUNT COMES FROM THE MAP ----------------------- */
    /* 100 BPM: a quarter is 0.6 s, a bar 2.4 s, so 115200 samples. Computed
     * by hand here as a cross-check of the module, which itself must use
     * ri_map_tick. */
    tempo_map(100u);
    RI_ASSERT(ri_loop_span(&sp, &map, PPQ, steps, 16u, 0u, 1u, 0u) == 0u,
        "one bar at 100 BPM");
    RI_ASSERT(sp.body_samples == 115200u, "2.4 s at 48 kHz is 115200 (%llu)",
        (unsigned long long)sp.body_samples);

    /* --- 3. A TAIL IS ADDITIVE, AND NOT PART OF THE BODY ------------- */
    tempo_map(120u);
    RI_ASSERT(ri_loop_span(&sp, &map, PPQ, steps, 16u, 0u, 2u, 500u) == 0u,
        "two bars plus a 500 ms tail");
    RI_ASSERT(sp.body_samples == 192000u,
        "the BODY is still exactly two bars (%llu)",
        (unsigned long long)sp.body_samples);
    RI_ASSERT(sp.tail_samples == 24000u, "and the tail is 500 ms (%llu)",
        (unsigned long long)sp.tail_samples);
    RI_ASSERT(sp.total_samples == sp.body_samples + sp.tail_samples,
        "the total is the two added, not the body overwritten (%llu)",
        (unsigned long long)sp.total_samples);
    /* A tail must NOT move the seam. If adding a tail changed the body, a
     * DAW would warp to the wrong place -- the exact failure R9 exists to
     * prevent. */
    {
        struct RILoopSpan bare;
        RI_ASSERT(ri_loop_span(&bare, &map, PPQ, steps, 16u, 0u, 2u, 0u) == 0u,
            "the same span with no tail");
        RI_ASSERT(bare.body_samples == sp.body_samples,
            "the body is the same with and without a tail");
        RI_ASSERT(bare.total_samples == bare.body_samples,
            "and with no tail the total IS the body");
    }

    /* --- 4. A TAIL OF ZERO IS NO TAIL ------------------------------- */
    RI_ASSERT(ri_loop_span(&sp, &map, PPQ, steps, 16u, 0u, 1u, 0u) == 0u,
        "tail 0 is legal");
    RI_ASSERT(sp.tail_samples == 0u, "and means no tail (%llu)",
        (unsigned long long)sp.tail_samples);
    RI_ASSERT(sp.total_samples == sp.body_samples, "so the total is the body");

    /* --- 5. FROM A CHOSEN BAR ---------------------------------------- */
    /* Starting at bar 1 of a 4-step (1-bar) pattern must be REFUSED rather
     * than wrapped: bar 1 does not exist, and wrapping would export music
     * the caller did not ask for. Same law as t205's unsorted refusal. */
    RI_ASSERT(ri_loop_span(&sp, &map, PPQ, steps, 16u, 1u, 1u, 0u) != 0u,
        "from-bar 1 of a 1-bar pattern is REFUSED, not wrapped");
    /* But a pattern of 2 bars may be rendered from bar 1. */
    for (i = 0u; i < 32u; i++) {
        steps[i].note = (uint8_t)(36u + (i % 12u));
        steps[i].flags = 0u;
    }
    RI_ASSERT(ri_loop_span(&sp, &map, PPQ, steps, 32u, 1u, 1u, 0u) == 0u,
        "from-bar 1 of a 2-bar pattern");
    RI_ASSERT(sp.start_tick == 384u, "starts a bar in (%llu)",
        (unsigned long long)sp.start_tick);
    RI_ASSERT(sp.body_samples == sp.bar_samples,
        "and is still one whole bar (%llu)",
        (unsigned long long)sp.body_samples);

    /* --- 6. A PATTERN THAT IS NOT WHOLE BARS IS REFUSED -------------- */
    /* 20 steps is 1.25 bars. A loop cannot be cut mid-bar and still be
     * loop-exact, so this is refused rather than rounded either way: up to
     * the caller (it would drop music), down to the DAW (it would clip). */
    for (i = 0u; i < 20u; i++) {
        steps[i].note = (uint8_t)(36u + (i % 12u));
        steps[i].flags = 0u;
    }
    RI_ASSERT(ri_loop_span(&sp, &map, PPQ, steps, 20u, 0u, 1u, 0u) != 0u,
        "a 20-step pattern is NOT whole bars");
    for (i = 0u; i < 5u; i++) {
        steps[i].note = 36u;
        steps[i].flags = 0u;
    }
    RI_ASSERT(ri_loop_span(&sp, &map, PPQ, steps, 5u, 0u, 1u, 0u) != 0u,
        "and a 5-step one either");

    /* --- 7. ZERO BARS IS REFUSED, NOT AN EMPTY LOOP ----------------- */
    RI_ASSERT(ri_loop_span(&sp, &map, PPQ, steps, 16u, 0u, 0u, 0u) != 0u,
        "zero bars");
    /* THE BOUND IS THE BODY'S SAMPLES, NOT A BAR COUNT. At 120 BPM a bar is
     * 96000 samples, so 2^32 samples is about 44739 bars: that many is
     * refused, and the bar count just below it is legal. I first wrote the
     * test asserting 4000 bars was refused, having done the arithmetic in
     * my head -- it is 384,000,000 samples and perfectly renderable. The
     * module was right and the expectation was wrong.
     * (My first LIMIT was also wrong in the other direction: 1048576 bars,
     * about two years, which is far too permissive to be a real bound.) */
    RI_ASSERT(ri_loop_span(&sp, &map, PPQ, steps, 16u, 0u, 44739u, 0u) == 0u,
        "44739 bars is 2^32 samples and is the largest legal body");
    RI_ASSERT(ri_loop_span(&sp, &map, PPQ, steps, 16u, 0u, 44740u, 0u) != 0u,
        "and one bar more is over the limit");
    /* A long-ish loop that fits is plainly legal, so this is a bound and not
     * a refusal of anything merely large. */
    RI_ASSERT(ri_loop_span(&sp, &map, PPQ, steps, 16u, 0u, 64u, 0u) == 0u,
        "64 bars is fine");

    /* --- 7b. A TEMPO WHERE A BAR IS NOT A WHOLE SAMPLE MULTIPLE ------- */
    /* LH SURVIVED FIRST, and checking it by hand showed the whole-bars proof
     * is genuinely reachable rather than redundant. At 140 BPM a bar is
     * 1714285 samples -- not a round number -- so the body of two bars is
     * 3428571, which is NOT a multiple of one bar. The map's single rounding
     * does not divide evenly at these tempos, and the whole point of R9 is a
     * body a DAW's warp grid can sit on. Handing over 3428571 as "two bars"
     * is exactly the lie this module exists not to tell. */
    tempo_map(140u);
    RI_ASSERT(ri_loop_span(&sp, &map, PPQ, steps, 16u, 0u, 2u, 0u) != 0u,
        "at 140 BPM two bars is not a whole number of bar-lengths");
    /* And at 120 BPM, where it IS exact, the same call succeeds -- so the
     * refusal is about the tempo and not about two bars being wrong. */
    tempo_map(120u);
    RI_ASSERT(ri_loop_span(&sp, &map, PPQ, steps, 16u, 0u, 2u, 0u) == 0u,
        "but at 120 BPM two whole bars are exact");

    /* --- 7c. A TAIL THAT IS NOT A WHOLE NUMBER OF SAMPLES ------------ */
    /* LG SURVIVED FIRST because no test asked for one. 48000 Hz: a
     * millisecond is 48 samples exactly, so the fractions come from other
     * rates -- and at 44100 a millisecond is 44.1 samples. The caller asked
     * for a length in milliseconds and the file is in samples; when those
     * two do not describe the same number, the honest answer is to say so
     * rather than round a length nobody chose into a file they will trust. */
    tempo_map(120u);
    {
        struct RISegment s44;
        struct RITempoMap m44;
        s44.start_tick = 0;
        s44.ns_per_quarter = 60000000000ULL / 120u;
        m44.segs = &s44;
        m44.n = 1u;
        m44.ppq = PPQ;
        m44.sr = 44100u;      /* 44.1 samples per millisecond */
        RI_ASSERT(ri_loop_span(&sp, &m44, PPQ, steps, 16u, 0u, 1u, 1u) != 0u,
            "a 1 ms tail at 44100 is 44.1 samples and is REFUSED");
        /* 10 ms is 441 samples exactly, so it is fine. */
        RI_ASSERT(ri_loop_span(&sp, &m44, PPQ, steps, 16u, 0u, 1u, 10u) == 0u,
            "and a 10 ms tail is a whole number of samples");
    }

    /* --- 8. NULLs ----------------------------------------------------- */
    RI_ASSERT(ri_loop_span(0, &map, PPQ, steps, 16u, 0u, 1u, 0u) != 0u,
        "NULL span");
    RI_ASSERT(ri_loop_span(&sp, 0, PPQ, steps, 16u, 0u, 1u, 0u) != 0u,
        "NULL map");
    RI_ASSERT(ri_loop_span(&sp, &map, 0u, steps, 16u, 0u, 1u, 0u) != 0u,
        "ppq 0 would divide by zero");
    RI_ASSERT(ri_loop_span(&sp, &map, PPQ, 0, 16u, 0u, 1u, 0u) != 0u,
        "NULL steps");
    RI_ASSERT(ri_loop_span(&sp, &map, PPQ, steps, 0u, 0u, 1u, 0u) != 0u,
        "no steps");

    /* --- 9. AN EMITTED RUN FITS INSIDE THE BODY ---------------------- */
    /* The span is only useful if the events the scheduler produces for
     * those bars land inside it. A span that stops short of its own events
     * would render a truncated loop that looks fine and plays short. */
    for (i = 0u; i < 16u; i++) {
        steps[i].note = (uint8_t)(36u + (i % 12u));
        steps[i].flags = 0u;
    }
    RI_ASSERT(ri_loop_span(&sp, &map, PPQ, steps, 16u, 0u, 2u, 0u) == 0u,
        "two bars of a 1-bar pattern");
    nev = ri_sched_emit_timed(&map, sp.start_tick, PPQ, steps, 16u, 0u,
        &opts, ev, 64u);
    RI_ASSERT(nev > 0u, "the run emits events (%u)", (unsigned)nev);
    /* Every event's NOTE-ON must be at or before the body's last sample: a
     * note triggered in the tail is not part of the loop body. */
    for (i = 0u; i < nev; i++)
        if (ev[i].type == RI_EV_NOTE_ON)
            RI_ASSERT(ev[i].sample < sp.body_samples,
                "event %u at %llu is inside the body of %llu", (unsigned)i,
                (unsigned long long)ev[i].sample,
                (unsigned long long)sp.body_samples);

    RI_RESULT("loop-render");
}