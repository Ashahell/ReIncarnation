/* t3_kernels_total.c — kernel totality over all finite floats (D-j, §2.1).
 * RED-first for the §2.1 fix: ri_exp/ri_sin/ri_pow2 must be finite and
 * sane for EVERY finite input, exact for under/overflow, accurate vs libm
 * where libm is finite. In-domain accuracy bounds below are the existing
 * t2 contract (must keep passing unchanged).
 */
#include <stdio.h>
#include <math.h>
#include <float.h>
#include "tests/helpers/ri_assert.h"
#include "engine/dsp/kernels.h"

static int finitef(float x) { return x == x && x != (float)(1.0 / 0.0) && x != (float)(-1.0 / 0.0); }

int main(void) {
    int i;
    /* exp: underflow floor — true exp(x) for x <= -87 is denormal-or-zero,
     * the kernel returns exactly 0 (D-j totality rule). */
    RI_ASSERT(ri_exp(-87.0f) == 0.0f, "exp(-87)=%g", ri_exp(-87.0f));
    RI_ASSERT(ri_exp(-100.0f) == 0.0f, "exp(-100)=%g", ri_exp(-100.0f));
    RI_ASSERT(ri_exp(-1e10f) == 0.0f, "exp(-1e10)=%g", ri_exp(-1e10f));
    RI_ASSERT(ri_exp(-FLT_MAX) == 0.0f, "exp(-FLT_MAX)=%g", ri_exp(-FLT_MAX));
    /* exp: overflow ceiling — matches libm (+Inf at/above ~88.73). */
    RI_ASSERT(ri_exp(100.0f) == (float)(1.0 / 0.0), "exp(100)=%g", ri_exp(100.0f));
    RI_ASSERT(ri_exp(1e10f) == (float)(1.0 / 0.0), "exp(1e10)=%g", ri_exp(1e10f));
    RI_ASSERT(ri_exp(FLT_MAX) == (float)(1.0 / 0.0), "exp(FLT_MAX)=%g", ri_exp(FLT_MAX));
    /* exp: finite everywhere finite in between, and accurate vs libm. */
    for (i = -90; i <= 88; i++) {
        float x = (float)i, e = ri_exp(x);
        RI_ASSERT(finitef(e), "exp non-finite at %d: %g", i, e);
        if (x > -87.0f && x < 88.0f) {
            float ref = expf(x), tol = ref * 1e-5f + 1e-7f;
            RI_ASSERT(fabsf(e - ref) <= tol, "exp acc %d: %g vs %g", i, e, ref);
        }
    }
    RI_ASSERT(ri_exp(89.0f) == (float)(1.0 / 0.0), "exp(89)=%g", ri_exp(89.0f));
    RI_ASSERT(ri_exp(90.0f) == (float)(1.0 / 0.0), "exp(90)=%g", ri_exp(90.0f));
    /* exp: logarithmic sweep to 1e30 — always finite, monotone in magnitude. */
    {
        float prev = 0.0f;
        for (i = -30; i <= 30; i++) {
            float x = (float)i * 3.0f, e = ri_exp(x);
            RI_ASSERT(finitef(e) || e == (float)(1.0 / 0.0), "exp bad at %g: %g", x, e);
            if (x > -87.0f && e != (float)(1.0 / 0.0)) {
                RI_ASSERT(e >= prev, "exp non-monotone at %g", x);
                prev = e;
            }
        }
    }
    /* sin: bounded for huge inputs (the old ±64-cycle clamp returned
     * garbage magnitudes, e.g. sin(500) = -7.8e15). */
    RI_ASSERT(fabsf(ri_sin(500.0f)) <= 1.0f, "sin(500)=%g", ri_sin(500.0f));
    RI_ASSERT(fabsf(ri_sin(500.0f) - sinf(500.0f)) <= 1e-5f, "sin(500) acc=%g", ri_sin(500.0f));
    for (i = 0; i < 200; i++) {
        float x = 1000.0f + (float)i * 1234567.0f;
        float s = ri_sin(x);
        RI_ASSERT(finitef(s), "sin non-finite at %g: %g", x, s);
        RI_ASSERT(fabsf(s) <= 1.0f, "sin unbounded at %g: %g", x, s);
        s = ri_sin(-x);
        RI_ASSERT(finitef(s) && fabsf(s) <= 1.0f, "sin(-) bad at %g: %g", x, s);
    }
    RI_ASSERT(finitef(ri_sin(FLT_MAX)) && fabsf(ri_sin(FLT_MAX)) <= 1.0f,
        "sin(FLT_MAX)=%g", ri_sin(FLT_MAX));
    /* pow2: full-range values (old ±32 clamp returned 2^32 for pow2(100)). */
    RI_ASSERT(fabsf(ri_pow2(100.0f) - 1.2676506e30f) / 1.2676506e30f <= 1e-6f,
        "pow2(100)=%g", ri_pow2(100.0f));
    RI_ASSERT(fabsf(ri_pow2(-100.0f) - 7.8886091e-31f) / 7.8886091e-31f <= 1e-5f,
        "pow2(-100)=%g", ri_pow2(-100.0f));
    RI_ASSERT(ri_pow2(-200.0f) == 0.0f, "pow2(-200)=%g", ri_pow2(-200.0f));
    RI_ASSERT(ri_pow2(200.0f) == (float)(1.0 / 0.0), "pow2(200)=%g", ri_pow2(200.0f));
    for (i = -130; i <= 130; i++) {
        float p = ri_pow2((float)i);
        RI_ASSERT(finitef(p) || p == (float)(1.0 / 0.0), "pow2 bad at %d: %g", i, p);
    }
    /* tanh keeps its guards and stays finite everywhere. */
    RI_ASSERT(ri_tanh(8.0f) == 1.0f && ri_tanh(-8.0f) == -1.0f, "tanh guards moved");
    RI_ASSERT(finitef(ri_tanh(1e20f)) && finitef(ri_tanh(-1e20f)), "tanh huge");
    /* log2: exact powers, accuracy vs libm on the audio range, totality. */
    RI_ASSERT(ri_log2(1.0f) == 0.0f, "log2(1)=%g", ri_log2(1.0f));
    RI_ASSERT(ri_log2(2.0f) == 1.0f, "log2(2)=%g", ri_log2(2.0f));
    RI_ASSERT(ri_log2(0.5f) == -1.0f, "log2(0.5)=%g", ri_log2(0.5f));
    RI_ASSERT(ri_log2(0.0f) == -1.0f / 0.0f, "log2(0)=%g", ri_log2(0.0f));
    RI_ASSERT(ri_log2(-3.0f) == -1.0f / 0.0f, "log2(-3)=%g", ri_log2(-3.0f));
    RI_ASSERT(finitef(ri_log2(FLT_MAX)) && fabsf(ri_log2(FLT_MAX) - 128.0f) < 0.01f,
        "log2(MAX)=%g", ri_log2(FLT_MAX));
    for (i = 1; i <= 640; i++) {
        float x = (float)i * 0.1f, g = ri_log2(x);
        RI_ASSERT(finitef(g), "log2 non-finite at %g", x);
        RI_ASSERT(fabsf(g - log2f(x)) <= 1e-6f, "log2 acc %g: %g", x, g);
    }
    for (i = 0; i < 200; i++) {
        float x = 1e-30f * (1.0f + (float)i);
        RI_ASSERT(finitef(ri_log2(x)), "log2 denorm %g", x);
    }
    RI_RESULT("kernels_total");
}
