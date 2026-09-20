#include <stdio.h>
#include <math.h>
#include <stdint.h>
#include "tests/helpers/ri_assert.h"
#include "engine/dsp/kernels.h"
static uint32_t lcg = 0x12345678;
static float frand(void) { lcg = lcg * 1664525u + 1013904223u; return (float)(lcg >> 8) / 16777216.0f; }
int main(void) {
    /* bit-determinism over 10k pseudo-random inputs per kernel */
    for (int i = 0; i < 10000; i++) {
        float x = (frand() * 2.0f - 1.0f) * 8.0f;
        RI_ASSERT(ri_tanh(x) == ri_tanh(x), "tanh nondet %f", x);
        RI_ASSERT(ri_exp(x) == ri_exp(x), "exp nondet %f", x);
    }
    /* no NaN/Inf for any finite input in documented domains */
    for (float x = -8.0f; x <= 8.0f; x += 0.125f) {
        float t = ri_tanh(x), e = ri_exp(x);
        RI_ASSERT(t == t && e == e, "NaN at %f", x);
        RI_ASSERT(t > -2.0f && t < 2.0f && e < 1e4f, "range at %f", x);
    }
    /* accuracy on operating grid: tanh ±1e-6, exp ±2e-6 vs libm reference */
    for (float x = -4.0f; x <= 4.0f; x += 0.01f) {
        RI_ASSERT(fabsf(ri_tanh(x) - tanhf(x)) <= 1e-6f, "tanh acc %f", x);
        RI_ASSERT(fabsf(ri_exp(x) - expf(x)) <= 2e-6f, "exp acc %f", x);
    }
    for (float x = -12.56f; x <= 12.56f; x += 0.02f)
        RI_ASSERT(fabsf(ri_sin(x) - sinf(x)) <= 2e-6f, "sin acc %f", x);
    for (float x = -10.0f; x <= 10.0f; x += 0.05f)
        RI_ASSERT(fabsf(ri_pow2(x) - powf(2.0f, x)) <= 4e-6f, "pow2 acc %f", x);
    /* lab rules: finite output for denormal-range inputs */
    for (float x = 1e-38f; x >= 1e-45f; x *= 0.1f) {
        float t = ri_tanh(x), e = ri_exp(x), s = ri_sin(x), p = ri_pow2(x);
        RI_ASSERT(t == t && e == e && s == s && p == p, "NaN denorm %g", x);
        RI_ASSERT(t < 1.0f && e < 2.0f && s < 1.0f && p < 2.0f, "range denorm %g", x);
    }
    RI_RESULT("kernels");
}
