/* t22_303indep — TC-2.2.6 two-instance independence (WBS 2.2).
 * Source audit (no file-scope mutable state in rb303.c, kernels.c,
 * params.c — all state in RB303Voice) says instances cannot share
 * state; this test pins it behaviorally: fresh voice A renders notes
 * X twice with an active different voice B in between (separate
 * buffer, discarded) — A1 must equal A2 bit-exactly (crosstalk -inf,
 * stronger than the TC's -90 dB). Energy guards on both (a silent
 * voice would pass vacuously). Deterministic, instant.
 */
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <math.h>
#include "engine/dsp/rb303.h"

#define T22_N 512u
#define T22_SR 48000.0f

static float bufA1[512u], bufA2[512u], bufB[512u];

static float peak_of(const float *b) {
    float peak = 0.0f;
    uint32_t i;
    for (i = 0u; i < T22_N; i++) {
        float a = b[i] < 0.0f ? -b[i] : b[i];
        if (a > peak)
            peak = a;
    }
    return peak;
}

static void setup_voice(struct RB303Voice *v) {
    rb303_init(v);
    rb303_set_param(v, RI_CTL_303A_CUTOFF, 80);
    rb303_set_param(v, RI_CTL_303A_RESO, 40);
    rb303_set_param(v, RI_CTL_303A_ENVMOD, 64);
    rb303_set_param(v, RI_CTL_303A_DECAY, 64);
    rb303_set_param(v, RI_CTL_303A_ACCENT, 96);
    rb303_set_param(v, RI_CTL_303A_WAVE, 0);
    rb303_set_param(v, RI_CTL_303A_VOLUME, 127);
}

int main(void) {
    struct RB303Voice va1, va2, vb;
    int fails = 0;

    setup_voice(&va1);
    rb303_note(&va1, 45, 0, 1);
    rb303_render(&va1, bufA1, T22_N, T22_SR);

    /* B lives between the two A renders, playing different material.
     * Plain note: slide-from-silence renders nothing by voice semantics
     * (pinned in the storm test), so B plays plain here. */
    setup_voice(&vb);
    rb303_note(&vb, 52, 0, 0);
    rb303_render(&vb, bufB, T22_N, T22_SR);

    setup_voice(&va2);
    rb303_note(&va2, 45, 0, 1);
    rb303_render(&va2, bufA2, T22_N, T22_SR);

    if (!(peak_of(bufA1) > 0.01f)) {
        printf("FAIL A1 silent (peak %.6g)\n", (double)peak_of(bufA1));
        fails++;
    }
    if (!(peak_of(bufB) > 0.01f)) {
        printf("FAIL B silent (peak %.6g)\n", (double)peak_of(bufB));
        fails++;
    }
    if (memcmp(bufA1, bufA2, sizeof bufA1) != 0) {
        uint32_t i, first = 0xFFFFFFFFu;
        float maxd = 0.0f;
        for (i = 0u; i < T22_N; i++) {
            float d = (float)fabs((double)bufA1[i] - (double)bufA2[i]);
            if (d > maxd) {
                maxd = d;
                if (first == 0xFFFFFFFFu)
                    first = i;
            }
        }
        printf("FAIL crosstalk: max diff %.6g first at %u\n",
               (double)maxd, first);
        fails++;
    }

    if (fails)
        printf("FAIL %d\n", fails);
    else
        printf("PASS t22_303indep\n");
    return fails != 0;
}
