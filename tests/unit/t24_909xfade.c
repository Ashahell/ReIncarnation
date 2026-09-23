/* t24_909xfade — Module 2.4, TC-2.4.1 (layer crossfade continuity) +
 * 909 panel Tune wiring:
 *
 *   - TC-2.4.1: tune-knob sweep 0..127 on a synthetic 2-layer sine
 *     fixture -> max adjacent-bin RMS step <= 1 dB (mirrors t1_909 §1),
 *     with the worst step inside the P-13 feather neighborhoods
 *     (lo/hi +- RI_909_XFADE_HALF) reported separately: the boundary
 *     is where a crossfade defect would live.
 *   - P-13 white-box: rb909_layer_weight is triangular with full 1.0
 *     inside [lo+4, hi-4] and 8-position linear ramps across each
 *     boundary (RI_909_XFADE_HALF = 4 each side); pin the ramp points
 *     exactly (0.0 / 0.875 / 1.0 are exact binary fractions).
 *   - Panel Tune (rib-id RI_CTL_909_TUNE, params.c 909 section) must
 *     select the engine tune byte: white-box on the struct field.
 *
 * RED status: rb909_set_param / RI_CTL_909_TUNE do not exist yet, so
 * this file fails to BUILD on the current tree (feature-absent
 * manifest, same shape as the t23 slices that pinned new params.c
 * surfaces).
 *
 * Analysis may use libm (tests/ only).
 */
#include <stdio.h>
#include <stdint.h>
#include <math.h>
#include "engine/dsp/rb909.h"

#define T24_SR 48000.0f
#define T24_SRU 48000u

static int fails = 0;
#define CHECK(cond, ...) do { \
    if (!(cond)) { printf("FAIL "); printf(__VA_ARGS__); printf("\n"); fails++; } \
} while (0)

static float L0[T24_SRU], L1[T24_SRU];
static float OUT[14400];

static float rms(const float *b, uint32_t n) {
    double s = 0.0;
    uint32_t i;
    for (i = 0; i < n; i++)
        s += (double)b[i] * b[i];
    return (float)sqrt(s / (double)n);
}

static void bake(void) {
    uint32_t i;
    for (i = 0; i < T24_SRU; i++) {
        double t = (double)i / 48000.0;
        L0[i] = (float)(0.5 * sin(2.0 * 3.141592653589793 * 440.0 * t));
        L1[i] = (float)(0.5 * sin(2.0 * 3.141592653589793 * 880.0 * t));
    }
}

static const struct RISampleLayer PAIR2[2] = {
    { L0, T24_SRU, T24_SRU, 0, 63, { 0, 0 } },
    { L1, T24_SRU, T24_SRU, 64, 127, { 0, 0 } }
};

/* White-box: what tune byte does the panel Tune knob select? */
static uint8_t tune_after(uint8_t knob) {
    struct RB909Set s;
    rb909_init_set(&s);
    rb909_set_param(&s.v[RB909_BD], RI_CTL_909_TUNE, knob);
    return s.v[RB909_BD].tune;
}

int main(void) {
    struct RB909Set s;
    uint32_t i;
    bake();

    /* --- panel Tune selects the engine tune byte (knob domain == engine) --- */
    CHECK(tune_after(0) == 0, "tune knob 0 -> %u", tune_after(0));
    CHECK(tune_after(64) == 64, "tune knob 64 -> %u", tune_after(64));
    CHECK(tune_after(127) == 127, "tune knob 127 -> %u", tune_after(127));

    /* --- P-13 feather white-box on layer [0,63]: full inside [4,59],
     * ramp 60..67 (8 positions), 0 beyond --- */
    CHECK(rb909_layer_weight(59, 0, 63) == 1.0f, "w59 %.6g != 1",
        (double)rb909_layer_weight(59, 0, 63));
    CHECK(rb909_layer_weight(67, 0, 63) == 0.0f, "w67 %.6g != 0",
        (double)rb909_layer_weight(67, 0, 63));
    CHECK(fabs((double)rb909_layer_weight(60, 0, 63) - 0.875) < 1e-6,
        "w60 %.6g != 0.875", (double)rb909_layer_weight(60, 0, 63));
    for (i = 59; i < 67; i++)
        CHECK(rb909_layer_weight((uint8_t)i, 0, 63) >=
            rb909_layer_weight((uint8_t)(i + 1), 0, 63),
            "ramp not monotone at %u", i);
    /* mirrored ramp on layer [64,127]: 0 below 60, full from 68 */
    CHECK(rb909_layer_weight(59, 64, 127) == 0.0f, "hi w59 %.6g != 0",
        (double)rb909_layer_weight(59, 64, 127));
    CHECK(rb909_layer_weight(68, 64, 127) == 1.0f, "hi w68 %.6g != 1",
        (double)rb909_layer_weight(68, 64, 127));

    /* --- TC-2.4.1 rendered sweep: worst adjacent step <= 1 dB --- */
    {
        float prev = 0.0f, worst = 0.0f, worst_edge = 0.0f;
        int t;
        rb909_init_set(&s);
        if (rb909_set_layers(&s, RB909_BD, PAIR2, 2) != 0) {
            printf("FAIL set layers\n");
            fails++;
        }
        for (t = 0; t <= 127; t++) {
            float r, step = 0.0f;
            rb909_trigger(&s, RB909_BD, 0, (uint8_t)t, 0);
            for (i = 0; i < 14400; i++)
                OUT[i] = rb909_voice_render(&s.v[RB909_BD], T24_SR);
            r = rms(OUT, 14400);
            CHECK(r > 0.05f, "tune %d rms %.6g too quiet", t, (double)r);
            if (t > 0) {
                step = (float)fabs(20.0 * log10((double)r / (double)prev));
                if (step > worst)
                    worst = step;
                /* feather neighborhoods of the 63|64 boundary */
                if ((t >= 60 && t <= 67) && step > worst_edge)
                    worst_edge = step;
            }
            prev = r;
        }
        CHECK(worst <= 1.0f, "sweep worst step %.4g dB", (double)worst);
        CHECK(worst_edge <= 1.0f, "boundary worst step %.4g dB",
            (double)worst_edge);
        printf("xfade sweep worst %.4g dB, boundary %.4g dB\n",
            (double)worst, (double)worst_edge);
    }

    if (fails)
        printf("FAIL %d\n", fails);
    else
        printf("PASS t24_909xfade\n");
    return fails != 0;
}
