/* t47_fx_delay_parity.c — delay Steps/triplet/fb-infinite (§12.8b1).
 * (a) STEPS: 32 straight @120 BPM/48 kHz echoes at 8 beats (192000).
 * (b) Triplet: 3 triplet-steps == 4 straight steps (1 beat), same echo.
 * (c) fb 127 = infinite: echo train bit-identical across repeats.
 * (d) Sustain: silence-in after a ring keeps echoing (no self-clear).
 * RED-first: 4-value beats table; fb capped 0.8.
 */
#include <stdio.h>
#include <stdint.h>
#include <math.h>
#include "tests/helpers/ri_assert.h"
#include "engine/fx/fx.h"

#define SR 48000.0f

static float LINE[2097152];
static float IN[300000];
static float OUT[300000];

static uint32_t echo_at(float *out, uint32_t n, uint32_t from) {
    uint32_t i;
    for (i = from; i < n; i++)
        if (fabsf(out[i]) > 0.3f)
            return i;
    return 0;
}

int main(void) {
    uint32_t i, e;
    for (i = 0; i < 300000u; i++) {
        IN[i] = 0.0f;
        OUT[i] = 0.0f;
    }
    IN[0] = 1.0f;
    /* (a) 32 straight steps @120 = 8 beats = 192000 samples. */
    {
        struct RIFX *dl = RiFXCreateDelay(LINE, 2097152u);
        RI_ASSERT(dl != 0, "delay create");
        RiFXSetParam(dl, RI_FXID_DELAY_STEPS, 32);
        RiFXSetParam(dl, RI_FXID_DELAY_TRIPLET, 0);
        RiFXSetParam(dl, RI_FXID_DELAY_MIX, 127);
        RiFXSetParam(dl, RI_FXID_DELAY_FB, 0);
        RiFXRender(dl, IN, OUT, 300000u, SR, 120.0f);
        e = echo_at(OUT, 300000u, 100u);
        RI_ASSERT(e >= 191998u && e <= 192002u, "32-step echo at %u (want 192000)", e);
        RiFXDestroy(dl);
    }
    /* (b) triplet equivalence. */
    {
        struct RIFX *a = RiFXCreateDelay(LINE, 2097152u);
        struct RIFX *b = RiFXCreateDelay(LINE, 2097152u);
        RI_ASSERT(a && b, "creates");
        RiFXSetParam(a, RI_FXID_DELAY_STEPS, 4);
        RiFXSetParam(a, RI_FXID_DELAY_TRIPLET, 0);
        RiFXSetParam(b, RI_FXID_DELAY_STEPS, 3);
        RiFXSetParam(b, RI_FXID_DELAY_TRIPLET, 1);
        RiFXSetParam(a, RI_FXID_DELAY_MIX, 127);
        RiFXSetParam(b, RI_FXID_DELAY_MIX, 127);
        RiFXSetParam(a, RI_FXID_DELAY_FB, 0);
        RiFXSetParam(b, RI_FXID_DELAY_FB, 0);
        RiFXRender(a, IN, OUT, 60000u, SR, 120.0f);
        e = echo_at(OUT, 60000u, 100u);
        RI_ASSERT(e >= 23998u && e <= 24002u, "straight-4 echo at %u (want 24000)", e);
        RiFXReset(b);
        RiFXRender(b, IN, OUT, 60000u, SR, 120.0f);
        e = echo_at(OUT, 60000u, 100u);
        RI_ASSERT(e >= 23998u && e <= 24002u, "triplet-3 echo at %u (want 24000)", e);
        RiFXDestroy(a);
        RiFXDestroy(b);
    }
    /* (c) infinite feedback: repeats bit-identical. */
    {
        struct RIFX *dl = RiFXCreateDelay(LINE, 2097152u);
        uint32_t e1, e2;
        RiFXSetParam(dl, RI_FXID_DELAY_STEPS, 4);
        RiFXSetParam(dl, RI_FXID_DELAY_TRIPLET, 0);
        RiFXSetParam(dl, RI_FXID_DELAY_MIX, 127);
        RiFXSetParam(dl, RI_FXID_DELAY_FB, 127);
        RiFXRender(dl, IN, OUT, 300000u, SR, 120.0f);
        e1 = echo_at(OUT, 300000u, 100u);
        e2 = 0;
        for (i = e1 + 12000u; i < 300000u; i++)
            if (fabsf(OUT[i]) > 0.3f) {
                e2 = i;
                break;
            }
        RI_ASSERT(e1 == 24000u, "first echo at %u", e1);
        RI_ASSERT(e2 > e1 && OUT[e2] == OUT[e1] && OUT[e2 + 1] == OUT[e1 + 1],
            "infinite repeats decay at %u", e2);
        RiFXDestroy(dl);
    }
    /* (d) silence-in sustains (stops don't clear the line). */
    {
        struct RIFX *dl = RiFXCreateDelay(LINE, 2097152u);
        static float zero[60000], tail[60000];
        uint32_t k;
        for (k = 0; k < 60000u; k++)
            zero[k] = 0.0f;
        RiFXSetParam(dl, RI_FXID_DELAY_STEPS, 4);
        RiFXSetParam(dl, RI_FXID_DELAY_MIX, 127);
        RiFXSetParam(dl, RI_FXID_DELAY_FB, 96);
        RiFXRender(dl, IN, OUT, 60000u, SR, 120.0f);
        RiFXRender(dl, zero, tail, 60000u, SR, 120.0f);
        e = echo_at(tail, 60000u, 100u);
        RI_ASSERT(e >= 11998u && e <= 12002u, "no sustain after stop: %u", e);
        RiFXDestroy(dl);
    }
    RI_RESULT("fx_delay_parity");
}
