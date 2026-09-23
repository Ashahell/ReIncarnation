/* t26_gainstage — Module 2.6, TC-2.6.1 (gain staging):
 *
 *   - E0 fader law gain = (v/127)^2 within ±0.5 dB at all 9 anchor
 *     points (mirrors t1_mixer §1 values); v = 0 is exactly 0,
 *     v = 127 exactly 1.0.
 *   - One law, three knob kinds (beyond t1): the send path uses the
 *     same square law — rendered send tail at send S over send tail
 *     at send 127 equals ri_fader_gain(S) (same fader, same
 *     trajectory, so the ratio isolates the send law).
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

static float DC[512], OUT[512], SND[512];

int main(void) {
    static const uint8_t ANCH[9] = { 0, 16, 32, 48, 64, 80, 96, 112, 127 };
    struct RiMixer m;
    const float *ins[RI_MIX_NBUS] = { DC, DC, DC, DC };
    uint32_t i, k;
    for (i = 0; i < 512; i++)
        DC[i] = 1.0f;

    /* --- 9 anchors ±0.5 dB, exact ends --- */
    for (i = 0; i < 9; i++) {
        uint8_t v = ANCH[i];
        float g = ri_fader_gain(v);
        if (v == 0) {
            CHECK(g == 0.0f, "v=0 gain %g want exact 0", (double)g);
        } else {
            double want_db = 40.0 * log10((double)v / 127.0);
            double got_db = 20.0 * log10((double)g);
            CHECK(fabs(got_db - want_db) <= 0.5,
                "v=%u got=%.4g dB want=%.4g", v, got_db, want_db);
        }
    }
    CHECK(ri_fader_gain(127) == 1.0f, "unity %g",
        (double)ri_fader_gain(127));

    /* --- send law == fader law, rendered ratios --- */
    {
        static const uint8_t SS[3] = { 16, 64, 112 };
        float ref;
        ri_mix_init(&m, T26_SR);
        ri_mix_set_fader(&m, 0, 100);
        ri_mix_set_fader(&m, 1, 0);
        ri_mix_set_fader(&m, 2, 0);
        ri_mix_set_fader(&m, 3, 0);
        ri_mix_set_send(&m, 0, 127);
        ri_mix_render(&m, ins, OUT, SND, 512);
        ref = SND[511];
        CHECK(ref > 0.0f, "send ref silent");
        for (k = 0; k < 3; k++) {
            double ratio, want;
            ri_mix_init(&m, T26_SR);
            ri_mix_set_fader(&m, 0, 100);
            ri_mix_set_fader(&m, 1, 0);
            ri_mix_set_fader(&m, 2, 0);
            ri_mix_set_fader(&m, 3, 0);
            ri_mix_set_send(&m, 0, SS[k]);
            ri_mix_render(&m, ins, OUT, SND, 512);
            ratio = (double)SND[511] / (double)ref;
            want = (double)ri_fader_gain(SS[k]);
            CHECK(fabs(ratio - want) / want < 1e-5,
                "send %u ratio %.8g want %.8g", SS[k], ratio, want);
        }
        printf("send law == fader law at 16/64/112\n");
    }

    if (fails)
        printf("FAIL %d\n", fails);
    else
        printf("PASS t26_gainstage\n");
    return fails != 0;
}
