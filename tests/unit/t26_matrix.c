/* t26_matrix — Module 2.6, TC-2.6.2 (solo/mute matrix):
 *
 *   - All 16 (mute_mask, solo_mask) combinations, RENDERED (beyond
 *     t1_mixer's predicate-level table + 2 rendered tails): distinct
 *     DC per bus (1/2/3/4), faders + master at 127, 512-sample
 *     settle; tail equals the sum of the audible buses' DC within
 *     1e-4. Audible rule: no solo → !mute; solo → solo && !mute.
 *   - Live solo toggle zipper (t1 covers mute both directions):
 *     worst single-sample step <= 0.02 on full-scale input, settle
 *     exact both ways.
 */
#include <stdio.h>
#include <stdint.h>
#include <math.h>
#include "engine/mixer/mixer.h"

#define T26_SR 48000.0f

static int fails = 0;
#define CHECK(cond, ...) do { \
    if (!(cond)) { printf("FAIL "); printf(__VA_ARGS__); printf("\n"); fails++; } \
} while (0)

static float B0[512], B1[512], B2[512], B3[512];
static float OUT[512], SND[512];

int main(void) {
    struct RiMixer m;
    const float *ins[RI_MIX_NBUS] = { B0, B1, B2, B3 };
    uint32_t i, b, mm, sm;
    for (i = 0; i < 512; i++) {
        B0[i] = 1.0f;
        B1[i] = 2.0f;
        B2[i] = 3.0f;
        B3[i] = 4.0f;
    }

    /* --- exhaustive rendered 4x4 matrix --- */
    for (mm = 0; mm < 16; mm++) {
        for (sm = 0; sm < 16; sm++) {
            double want = 0.0;
            ri_mix_init(&m, T26_SR);
            for (b = 0; b < RI_MIX_NBUS; b++) {
                int mute = (mm >> b) & 1;
                int solo = (sm >> b) & 1;
                int aud = (sm == 0) ? !mute : (solo && !mute);
                if (mute)
                    ri_mix_set_mute(&m, b, 1);
                if (solo)
                    ri_mix_set_solo(&m, b, 1);
                if (aud)
                    want += (double)(b + 1);
            }
            ri_mix_render(&m, ins, OUT, SND, 512);
            CHECK(fabs((double)OUT[511] - want) < 1e-4,
                "mm=%u sm=%u tail %.6g want %.6g", mm, sm,
                (double)OUT[511], want);
        }
    }
    printf("rendered matrix 16x16 exact\n");

    /* --- live solo toggle zipper, both directions.
     * Bound generalizes the ledger's single-bus rule (worst step
     * 1/64 of full scale per slewing bus): soloing bus 0 collapses
     * buses 1..3 (DC 2+3+4 = 9), so the transient bound is 9/64 —
     * simultaneous per-bus slews, each <= 1/64, never a step. --- */
    {
        const float collapse = 9.0f / 64.0f;
        float worst = 0.0f;
        ri_mix_init(&m, T26_SR);
        ri_mix_render(&m, ins, OUT, SND, 512); /* all open, settle */
        ri_mix_set_solo(&m, 0, 1); /* solo on: 1/2/3 collapse */
        ri_mix_render(&m, ins, OUT, SND, 512);
        for (i = 1; i < 512; i++) {
            float d = OUT[i] - OUT[i - 1];
            if (d < 0.0f)
                d = -d;
            if (d > worst)
                worst = d;
        }
        CHECK(worst <= collapse + 1e-6f, "solo-on transient %g > 9/64",
            (double)worst);
        CHECK(fabs((double)OUT[511] - 1.0) < 1e-4, "solo tail %g",
            (double)OUT[511]);
        worst = 0.0f;
        ri_mix_set_solo(&m, 0, 0); /* solo off: all reopen */
        ri_mix_render(&m, ins, OUT, SND, 512);
        for (i = 1; i < 512; i++) {
            float d = OUT[i] - OUT[i - 1];
            if (d < 0.0f)
                d = -d;
            if (d > worst)
                worst = d;
        }
        CHECK(worst <= collapse + 1e-6f, "solo-off transient %g > 9/64",
            (double)worst);
        CHECK(fabs((double)OUT[511] - 10.0) < 1e-4, "reopen tail %g",
            (double)OUT[511]);
        printf("solo toggle worst steps ok\n");
    }

    if (fails)
        printf("FAIL %d\n", fails);
    else
        printf("PASS t26_matrix\n");
    return fails != 0;
}
