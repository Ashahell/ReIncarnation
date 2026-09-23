/* t23_808clap — Module 2.3, TC-2.3.3 (clap 4-burst structure): the clap
 * envelope must show 3 loud bursts ~9 ms apart plus a 4th burst (tail onset),
 * per the WBS contract "4-burst structure (3 x 9 ms + tail)". t1_808 §3 pins
 * only the peak COUNT; this pin additionally verifies the timings (first
 * three inter-burst spacings 9 ms +-2 ms). Green-pin expectation: the engine
 * already bursts at RI_808_CLAP_BURST_GAP = 9 ms.
 *
 * Mirror of t1_808 §3's fast follower + local-peak detector (tau ~1 ms,
 * gap > 4 ms, prominence > 0.3x, scan window 0..40 ms), extended to record
 * acceptance times.
 *
 * Analysis may use libm (tests/ only).
 */
#include <stdio.h>
#include <stdint.h>
#include "engine/dsp/rb808.h"

#define T23_SR 48000.0f
#define T23_N 24000u /* 0.5 s @48k */

static int fails = 0;
#define CHECK(cond, ...) do { \
    if (!(cond)) { printf("FAIL "); printf(__VA_ARGS__); printf("\n"); fails++; } \
} while (0)

static float buf[T23_N];

int main(void) {
    uint32_t i, n = T23_N, peaks = 0, i0 = 0;
    float env = 0.0f, mx = 0.0f;
    static float fol[24000];
    static float acc_t[8]; /* accepted peak times (s) */
    float last_acc = -1.0f;
    struct RB808Set s;
    uint32_t k;

    rb808_init_set(&s);
    rb808_trigger(&s, RB808_CP, 0, 0.0f);
    for (i = 0; i < n; i++)
        buf[i] = rb808_voice_render(&s.v[RB808_CP], T23_SR);

    /* fast follower (tau ~1 ms): 2 ms bursts reach ~85% while the 7 ms gaps
     * fully decay; tail ripple valleys stay shallow so relative prominence
     * rejects them. */
    for (i = 0; i < n; i++) {
        float a = buf[i] < 0.0f ? -buf[i] : buf[i];
        env += 0.02f * (a - env);
        fol[i] = env;
        if (env > mx)
            mx = env;
    }
    for (i = 1; i + 1 < (uint32_t)(0.040f * T23_SR); i++) {
        if (fol[i] > fol[i - 1] && fol[i] >= fol[i + 1] && fol[i] > 0.2f * mx) {
            float valley = fol[i], tgap;
            uint32_t j;
            for (j = i0; j < i; j++)
                if (fol[j] < valley)
                    valley = fol[j];
            tgap = last_acc < 0.0f ? 99.0f : (float)i / T23_SR - last_acc;
            if (tgap > 0.004f && fol[i] - valley > 0.3f * fol[i]) {
                if (peaks < 8)
                    acc_t[peaks] = (float)i / T23_SR;
                peaks++;
                last_acc = (float)i / T23_SR;
            }
            i0 = i;
        }
    }
    printf("INFO clap %u peaks at:", peaks);
    for (k = 0; k < peaks; k++)
        printf(" %.4g", acc_t[k]);
    printf(" s\n");

    CHECK(peaks == 4, "clap bursts: %u peaks (want 4)", peaks);
    /* first three inter-burst spacings 9 ms +-2 ms (TC-2.3.3) */
    for (k = 0; k + 1 < peaks && k < 3; k++) {
        float gap = acc_t[k + 1] - acc_t[k];
        CHECK(gap > 0.007f && gap < 0.011f,
            "clap spacing %u: %.4f s (want 9 ms +-2 ms)", k + 1, (double)gap);
    }

    if (fails)
        printf("FAIL %d\n", fails);
    else
        printf("PASS t23_808clap\n");
    return fails != 0;
}