/* t25_dist — Module 2.5, TC-2.5.4 (distortion):
 *
 *   - Unity ±0.2 dB at drive 0 on a sine probe (mirrors t1_fx §4 value).
 *   - DC normalization white-box (beyond t1): the engaged path is
 *     normalized so full-scale DC 1.0 maps to 1.0 — pin out == 1.0
 *     within 1e-6 for a drive×shape grid (the header's stated
 *     mechanism; t1 only pins the bypass cell + unity band).
 *   - Monotonic loudness growth across the drive sweep at fixed
 *     shape (TC-literal; NOT in t1): sine RMS non-decreasing with
 *     0.1% relative slack for fp noise.
 *   - Engaged drive is a real curve (RMS moves vs dry).
 *   - No NaN/Inf over the drive×shape grid on probes + sine tails.
 */
#include <stdio.h>
#include <stdint.h>
#include <math.h>
#include "engine/fx/fx.h"

#define T25_SR 48000.0f
#define T25_SRU 48000u

static int fails = 0;
#define CHECK(cond, ...) do { \
    if (!(cond)) { printf("FAIL "); printf(__VA_ARGS__); printf("\n"); fails++; } \
} while (0)

static float IN[T25_SRU], OUT[T25_SRU];

static float rms(const float *b, uint32_t n) {
    double s = 0.0;
    uint32_t i;
    for (i = 0; i < n; i++)
        s += (double)b[i] * b[i];
    return (float)sqrt(s / (double)n);
}

static void sine(float *b, uint32_t n, double f, double amp) {
    uint32_t i;
    for (i = 0; i < n; i++)
        b[i] = (float)(amp * sin(2.0 * 3.141592653589793 * f *
            (double)i / 48000.0));
}

static int finite_buf(const float *b, uint32_t n) {
    uint32_t i;
    for (i = 0; i < n; i++) {
        float v = b[i] < 0.0f ? -b[i] : b[i];
        if (!(v < 1e30f))
            return 0;
    }
    return 1;
}

int main(void) {
    struct RiFXDist d;
    uint32_t i, dv, sv;
    ri_fxdist_init(&d);

    /* --- unity ±0.2 dB at drive 0 --- */
    {
        float r0, r1, db;
        sine(IN, T25_SRU, 1000.0, 0.5);
        r0 = rms(IN, T25_SRU);
        ri_fxdist_render(&d, IN, OUT, T25_SRU);
        r1 = rms(OUT, T25_SRU);
        db = (float)fabs(20.0 * log10((double)r1 / (double)r0));
        CHECK(db <= 0.2f, "drive0 unity %.4g dB", (double)db);
        printf("dist drive0 unity: %.4g dB\n", (double)db);
    }

    /* --- DC normalization: full-scale DC 1.0 -> 1.0 (<=1e-6) --- */
    {
        static const uint8_t drives[3] = { 1, 64, 127 };
        static const uint8_t shapes[3] = { 0, 64, 127 };
        for (dv = 0; dv < 3; dv++) {
            for (sv = 0; sv < 3; sv++) {
                ri_fxdist_set(&d, drives[dv], shapes[sv]);
                for (i = 0; i < 1024u; i++)
                    IN[i] = 1.0f;
                ri_fxdist_render(&d, IN, OUT, 1024u);
                CHECK(fabs((double)OUT[512] - 1.0) <= 1e-6,
                    "dc norm d=%u s=%u out=%.9g",
                    drives[dv], shapes[sv], (double)OUT[512]);
            }
        }
        printf("dc norm 3x3 grid <= 1e-6\n");
    }

    /* --- monotonic loudness growth over drive sweep (shape 32) --- */
    {
        float prev = 0.0f;
        ri_fxdist_init(&d);
        sine(IN, T25_SRU, 1000.0, 0.5);
        for (dv = 0; dv <= 127u; dv += 8u) {
            float r;
            ri_fxdist_set(&d, (uint8_t)dv, 32);
            ri_fxdist_render(&d, IN, OUT, T25_SRU);
            r = rms(OUT, T25_SRU);
            if (dv > 0)
                CHECK(r >= prev * 0.999f,
                    "loudness dips at drive %u (%.6g < %.6g)",
                    dv, (double)r, (double)prev);
            CHECK(finite_buf(OUT, T25_SRU), "nan at drive %u", dv);
            prev = r;
        }
        printf("monotonic sweep ok, max rms %.5g\n", (double)prev);
    }

    /* --- engaged drive is a real curve + grid finite --- */
    {
        float rdry, rwet;
        sine(IN, T25_SRU, 1000.0, 0.9);
        rdry = rms(IN, T25_SRU);
        ri_fxdist_set(&d, 127, 0);
        ri_fxdist_render(&d, IN, OUT, T25_SRU);
        rwet = rms(OUT, T25_SRU);
        CHECK(fabs((double)rwet - (double)rdry) > 0.01,
            "full drive changes nothing");
        for (dv = 0; dv <= 127u; dv += 16u) {
            for (sv = 0; sv <= 127u; sv += 16u) {
                ri_fxdist_set(&d, (uint8_t)dv, (uint8_t)sv);
                sine(IN, 1024, 1000.0, 0.9);
                ri_fxdist_render(&d, IN, OUT, 1024u);
                CHECK(finite_buf(OUT, 1024u), "nan d=%u s=%u", dv, sv);
            }
        }
    }

    if (fails)
        printf("FAIL %d\n", fails);
    else
        printf("PASS t25_dist\n");
    return fails != 0;
}
