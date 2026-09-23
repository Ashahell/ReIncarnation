/* t24_909accent — Module 2.4, TC-2.4.2 (accent levels + flam):
 *
 *   - acc1 amp x1.15 +-0.2 dB (ratio bounds 1.124..1.178), rendered
 *     RMS BD accent 0 vs 1 at tune 64 (mirrors t1_909 §2 value).
 *   - flam second hit at +35 ms +-5 ms (1680 +- 240 samples @48 kHz)
 *     x0.75 amp (+-0.10 on matched-age peaks), decaying fixture
 *     (mirrors t1_909 §3 values).
 *   - acc2 on a non-flam voice (CH) == acc1 within 2% RMS.
 *
 * Green pin on frozen code (property pin over landed Task 9
 * behaviour); the RED driver for the slice is t24_909xfade
 * (rb909_set_param does not exist yet).
 *
 * Analysis may use libm (tests/ only).
 */
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <math.h>
#include "engine/dsp/rb909.h"

#define T24_SR 48000.0f
#define T24_SRU 48000u

static int fails = 0;
#define CHECK(cond, ...) do { \
    if (!(cond)) { printf("FAIL "); printf(__VA_ARGS__); printf("\n"); fails++; } \
} while (0)

static float LD2[T24_SRU];
static float OUTA[24000], OUTB[24000], OUTC[24000];

static float rms(const float *b, uint32_t n) {
    double s = 0.0;
    uint32_t i;
    for (i = 0; i < n; i++)
        s += (double)b[i] * b[i];
    return (float)sqrt(s / (double)n);
}

static void render_voice(struct RB909Set *s, uint32_t v, uint32_t n, float *out) {
    uint32_t i;
    for (i = 0; i < n; i++)
        out[i] = rb909_voice_render(&s->v[v], T24_SR);
}

int main(void) {
    struct RB909Set s;
    uint32_t i;
    for (i = 0; i < T24_SRU; i++) {
        double t = (double)i / 48000.0;
        LD2[i] = (float)(0.8 * sin(2.0 * 3.141592653589793 * 660.0 * t) *
            exp(-t / 0.008));
    }

    /* --- acc1 x1.15 +-0.2 dB on BD --- */
    {
        static const struct RISampleLayer ONE = {
            LD2, T24_SRU, T24_SRU, 0, 127, { 0, 0 }
        };
        float r0, r1, ratio;
        rb909_init_set(&s);
        if (rb909_set_layers(&s, RB909_BD, &ONE, 1) != 0) {
            printf("FAIL set layers\n");
            fails++;
        }
        rb909_trigger(&s, RB909_BD, 0, 64, 0);
        render_voice(&s, RB909_BD, 14400, OUTA);
        r0 = rms(OUTA, 14400);
        rb909_trigger(&s, RB909_BD, 1, 64, 0);
        render_voice(&s, RB909_BD, 14400, OUTB);
        r1 = rms(OUTB, 14400);
        ratio = r1 / r0;
        CHECK(ratio >= 1.124f && ratio <= 1.178f,
            "acc1 ratio %.5g want 1.15 +-0.2dB", (double)ratio);
        printf("acc1 ratio: %.5g\n", (double)ratio);
    }

    /* --- flam onset 1680 +- 240, ratio 0.75 +- 0.10 --- */
    {
        static const struct RISampleLayer ONE = {
            LD2, T24_SRU, T24_SRU, 0, 127, { 0, 0 }
        };
        int onset2 = -1;
        float p1 = 0.0f, p2 = 0.0f;
        rb909_init_set(&s);
        if (rb909_set_layers(&s, RB909_BD, &ONE, 1) != 0) {
            printf("FAIL flam layers\n");
            fails++;
        }
        rb909_trigger(&s, RB909_BD, 2, 64, 1680);
        render_voice(&s, RB909_BD, 24000, OUTA);
        for (i = 1000; i < 24000; i++) {
            if ((float)fabs((double)OUTA[i]) > 0.1f) {
                onset2 = (int)i;
                break;
            }
        }
        CHECK(onset2 > 0, "flam second onset not found");
        if (onset2 > 0)
            CHECK(abs(onset2 - 1680) <= 240, "flam onset %d want 1680+-240",
                onset2);
        for (i = 64; i < 900; i++)
            if ((float)fabs((double)OUTA[i]) > p1)
                p1 = (float)fabs((double)OUTA[i]);
        for (i = 1680 + 64; i < 1680 + 900; i++)
            if ((float)fabs((double)OUTA[i]) > p2)
                p2 = (float)fabs((double)OUTA[i]);
        CHECK(p1 > 0.3f, "flam first peak %.5g too small", (double)p1);
        CHECK(fabs((double)(p2 / p1) - 0.75) <= 0.10, "flam ratio %.4g want 0.75",
            (double)(p2 / p1));
        printf("flam onset %d, ratio %.4g\n", onset2, (double)(p2 / p1));
    }

    /* --- acc2 on non-flam voice (CH) == acc1 --- */
    {
        static const struct RISampleLayer ONE = {
            LD2, T24_SRU, T24_SRU, 0, 127, { 0, 0 }
        };
        float ra, rb, rr;
        if (rb909_set_layers(&s, RB909_CH, &ONE, 1) != 0) {
            printf("FAIL ch layers\n");
            fails++;
        }
        rb909_trigger(&s, RB909_CH, 1, 64, 0);
        render_voice(&s, RB909_CH, 14400, OUTB);
        ra = rms(OUTB, 14400);
        rb909_trigger(&s, RB909_CH, 2, 64, 1680);
        render_voice(&s, RB909_CH, 14400, OUTC);
        rb = rms(OUTC, 14400);
        rr = rb / ra;
        CHECK(fabs((double)rr - 1.0) <= 0.02, "ch acc2 != acc1 (%.5g)",
            (double)rr);
    }

    if (fails)
        printf("FAIL %d\n", fails);
    else
        printf("PASS t24_909accent\n");
    return fails != 0;
}
