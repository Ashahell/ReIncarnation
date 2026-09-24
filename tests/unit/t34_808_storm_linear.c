/* t34_808_storm_linear.c — section-sum linearity (§2.4 fix proof, §12.5a
 * slot-aware update).
 * Storm the 11 slot-selected sounds (accent, max decay): the mix MUST equal
 * the bit-exact sum of their solo renders (float headroom, no clipper).
 * Switch-pair partners keep silent unless selected (last-wins asserted
 * separately in t41). RED-history: the tanh soft-clip broke equality;
 * the slot rewrite keeps the same equality on the selected set.
 */
#include <stdio.h>
#include <math.h>
#include "tests/helpers/ri_assert.h"
#include "engine/dsp/rb808.h"

#define SR 48000.0f
#define N (48000u * 2u) /* 2 s storm */

/* Slot-selected set after triggering sounds 0..15 in order (last-wins). */
static const uint32_t SEL[11] = { 0, 1, 5, 6, 7, 9, 15, 14, 13, 12, 11 };

static float mix[N], sum[N], solo[N];

int main(void) {
    struct RB808Set all, one;
    uint32_t v, i;
    float peak = 0.0f;
    rb808_init_set(&all);
    rb808_max_decay(&all);
    for (v = 0; v < RI_808_NSOUNDS; v++)
        rb808_trigger(&all, v, 1u, 0.0f);
    for (i = 0; i < N; i++)
        sum[i] = 0.0f;
    rb808_render_mix(&all, mix, N, SR);
    for (v = 0; v < 11u; v++) {
        rb808_init_set(&one);
        rb808_max_decay(&one);
        if (SEL[v] == 12u)
            rb808_trigger(&one, 11u, 1u, 0.0f); /* OH solo reproduces the
            storm's same-step pre-roll (CH first, unrendered) bit-exactly */
        rb808_trigger(&one, SEL[v], 1u, 0.0f);
        for (i = 0; i < N; i++) {
            solo[i] = rb808_voice_render(&one.v[SEL[v]], SR);
            sum[i] += solo[i];
        }
    }
    for (i = 0; i < N; i++) {
        float a = sum[i] < 0.0f ? -sum[i] : sum[i];
        if (a > peak)
            peak = a;
        RI_ASSERT(mix[i] == sum[i], "nonlinear at %u: mix %g sum %g", i, mix[i], sum[i]);
    }
    RI_ASSERT(peak > 1.2f, "storm never clips: peak %g", peak);
    RI_RESULT("808_storm_linear");
}
