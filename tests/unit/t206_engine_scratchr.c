/* t206_engine_scratchr — the mono path never reads `scratchR`, pinned.
 *
 * WHY THIS TEST EXISTS: I REPORTED A DEFECT THAT WAS NOT THERE.
 *
 * In the R8c slice I claimed "`e->scratchR` is never written for a mono
 * section, so the right stem for the 303s carries stale data, and a faithful
 * tap would export it." That was **wrong**, and it was wrong in a way worth
 * pinning.
 *
 * `engine.c` contains two near-identical static functions 60 lines apart:
 * `engine_section` (mono, sections 0-3) and `engine_section_stereo` (the
 * Levi, section 4). They take the SAME parameter list. Their accumulates
 * differ in exactly one token:
 *
 *     engine_section:        double s = e->scratch[i];
 *                             ml[i] += s * gl;  mr[i] += s * gr;
 *
 *     engine_section_stereo: mr[i] += e->scratchR[i] * gr;
 *
 * I read the stereo one and attributed it to the mono one. The mono function
 * meters `scratch` directly, sends `scratch`, and mirrors the mono sample to
 * BOTH outputs on purpose -- so `scratchR` is not merely "not written", it is
 * **not read at all** on that path, and its contents cannot reach the output,
 * the send, or the section meter however stale they are.
 *
 * So the laws, which is what was actually true all along:
 *
 *  - **A MONO SECTION MIRRORS TO BOTH OUTPUTS.** `mr += s * gr`, not
 *    `mr += scratchR * gr`. A 303 on its own is centred, not left-only.
 *  - **THE MONO PATH CANNOT BE CORRUPTED BY `scratchR`.** Plant any value
 *    there and the output is bit-identical. This is the property that made
 *    the R8c stem tap safe, and the property my claim denied.
 *  - **THE MONO SECTION METER READS `scratch`, NOT THE AVERAGE.** The stereo
 *    path meters `0.5 * (scratch + scratchR)`; the mono path meters
 *    `scratch`. Mixing the two up is how the original claim looked
 *    reasonable.
 *  - **THE LEVI ALWAYS WRITES `scratchR` BEFORE READING IT.** The one path
 *    that does read it fills it first, even when silent -- so the stereo
 *    path is not exposed either.
 *
 * METHOD NOTE, because this is the part worth carrying: two static functions
 * in one file with identical signatures are a specific trap for "the
 * function I am reading is the one being called". It survived a segfault
 * hunt, four probes, and a grep. What would have caught it in one step is
 * printing the *values* the probe observed against the *expression* the
 * source has -- the sentinel showed no effect at all, which was the finding,
 * and I read past it three times.
 */
#include <stdio.h>
#include <string.h>
#include "tests/helpers/ri_assert.h"
#include "engine/engine.h"
#include "engine/seq/sched.h"

#define N 256u

static struct RIEngine E;
static struct RIEvent EV[8];
static float L[N], R[N];

static void load(uint32_t device, uint32_t value, uint32_t sections) {
    uint32_t k = 0u;
    memset(EV, 0, sizeof EV);
    if (device < 4u) {          /* the 303/808/909 lane devices */
        EV[k].type = RI_EV_NOTE_ON;
        EV[k].device = (uint16_t)device;
        EV[k].voice = 0u;
        EV[k].value = value;
        EV[k].sample = 0u;
        k++;
    }
    ri_engine_init(&E);
    ri_engine_load(&E, EV, k, N, sections);
}

int main(void) {
    uint32_t done;

    /* --- 1. A MONO SECTION IS CENTRED, NOT LEFT-ONLY ---------------- */
    load(0u, 36u, RI_ENGINE_S303A);
    done = ri_engine_render(&E, L, R, N, 48000.0f);
    RI_ASSERT(done == N, "the block rendered (%u)", (unsigned)done);
    RI_ASSERT(memcmp(L, R, sizeof L) == 0,
        "a 303A-only render is CENTRED: L and R are identical");

    /* --- 2. PLANTING ANYTHING IN scratchR CHANGES NOTHING ------------ */
    /* The law my retracted claim denied. Bit-identical, not "close". */
    {
        static float L2[N], R2[N];
        uint32_t i;
        load(0u, 36u, RI_ENGINE_S303A);
        ri_engine_render(&E, L, R, N, 48000.0f);
        /* Same song, and this time scratchR is full of something awful. */
        load(0u, 36u, RI_ENGINE_S303A);
        for (i = 0u; i < RI_ENGINE_BLOCK; i++) {
            E.scratchR[i] = 1.0e30f;
            E.scratchR[(i + 1u) % RI_ENGINE_BLOCK] = -1.0e30f;
        }
        ri_engine_render(&E, L2, R2, N, 48000.0f);
        RI_ASSERT(memcmp(L, L2, sizeof L) == 0,
            "a poisoned scratchR cannot reach the left output");
        RI_ASSERT(memcmp(R, R2, sizeof R) == 0,
            "nor the right output");
        /* And the poison is still sitting there afterwards: the mono path
         * neither consumed it nor tidied it. */
        RI_ASSERT(E.scratchR[0] != 0.0f,
            "which is exactly why the path must not read it (scratchR[0]=%g)",
            (double)E.scratchR[0]);
    }

    /* --- 3. EVERY MONO SECTION, NOT JUST THE 303A -------------------- */
    {
        static const uint32_t devs[4] = { 0u, 1u, 2u, 3u };
        static const uint32_t secs[4] = {
            RI_ENGINE_S303A, RI_ENGINE_S303B, RI_ENGINE_S808, RI_ENGINE_S909
        };
        uint32_t k, i;
        for (k = 0u; k < 4u; k++) {
            static float A[N], B[N], S[N], T[N];
            load(devs[k], (k < 2u) ? 36u : 0u, secs[k]);
            ri_engine_render(&E, A, B, N, 48000.0f);
            load(devs[k], (k < 2u) ? 36u : 0u, secs[k]);
            for (i = 0u; i < RI_ENGINE_BLOCK; i++)
                E.scratchR[i] = 3.5e30f;
            ri_engine_render(&E, S, T, N, 48000.0f);
            RI_ASSERT(memcmp(A, S, sizeof A) == 0 && memcmp(B, T, sizeof B) == 0,
                "section %u is immune to scratchR", (unsigned)devs[k]);
        }
    }

    /* --- 4. THE MONO SECTION METER READS scratch, NOT THE AVERAGE --- */
    /* The stereo path meters 0.5*(scratch+scratchR); the mono path meters
     * scratch. Pin the difference: with a huge scratchR the stereo-style
     * average would read very differently, so a meter that ignores the
     * poison is the observable difference. */
    {
        uint32_t i;
        float peak_clean, peak_poison;
        load(0u, 36u, RI_ENGINE_S303A);
        ri_engine_render(&E, L, R, N, 48000.0f);
        peak_clean = ri_meter_peak(&E.sec_meter[0]);
        load(0u, 36u, RI_ENGINE_S303A);
        for (i = 0u; i < RI_ENGINE_BLOCK; i++)
            E.scratchR[i] = 3.5e30f;
        ri_engine_render(&E, L, R, N, 48000.0f);
        peak_poison = ri_meter_peak(&E.sec_meter[0]);
        RI_ASSERT(peak_clean > 0.0f, "the meter saw the 303 (%g)",
            (double)peak_clean);
        RI_ASSERT(peak_poison == peak_clean,
            "and scratchR does not move it (%g vs %g)",
            (double)peak_poison, (double)peak_clean);
    }

    /* --- 5. THE LEVI FILLS scratchR BEFORE READING IT --------------- */
    /* The one path that DOES read it, so it is the one that could have been
     * exposed. Even with no note at all, scratchR is overwritten. */
    {
        uint32_t i;
        load(4u, 0u, RI_ENGINE_SLEVI);        /* no events: silent */
        for (i = 0u; i < RI_ENGINE_BLOCK; i++)
            E.scratchR[i] = 7.25f;
        ri_engine_render(&E, L, R, N, 48000.0f);
        RI_ASSERT(E.scratchR[0] != 7.25f,
            "a silent Levi still overwrites scratchR (got %g)",
            (double)E.scratchR[0]);
    }

    /* --- 6. MONO + LEVI TOGETHER -------------------------------------- */
    /* The case a stem render will actually hit: a mono section and the
     * stereo one in the same block. */
    {
        static float L2[N], R2[N];
        uint32_t i;
        load(0u, 36u, RI_ENGINE_S303A | RI_ENGINE_SLEVI);
        ri_engine_render(&E, L, R, N, 48000.0f);
        load(0u, 36u, RI_ENGINE_S303A | RI_ENGINE_SLEVI);
        for (i = 0u; i < RI_ENGINE_BLOCK; i++)
            E.scratchR[i] = -2.5e30f;
        ri_engine_render(&E, L2, R2, N, 48000.0f);
        RI_ASSERT(memcmp(L, L2, sizeof L) == 0 && memcmp(R, R2, sizeof R) == 0,
            "and with a stereo section present, mono is still immune");
    }

    RI_RESULT("engine-scratchr");
}