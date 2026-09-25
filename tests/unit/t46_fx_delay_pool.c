/* t46_fx_delay_pool.c — delay beats-honoring + pool retirement (§12.8a).
 * (a) BEATS honored: BEATS=1.0 @140 BPM/48 kHz echoes at 20571 (not 15429).
 * (b) Pool retired: Create(DELAY) is NULL (caller-owned buffers);
 *     Destroy frees slots (create/destroy/create reuses); NULL + double
 *     destroy safe.
 * (c) Tap slew: retarget moves delay_smp partway (not jumping), converges.
 * (d) Comp follows the render rate (atk coeff at 44.1 kHz, not stale 48 k).
 * RED-first: hardcoded 0.75/beats-ignored; static pool; instant taps; 48k comp.
 */
#include <stdio.h>
#include <stdint.h>
#include <math.h>
#include "tests/helpers/ri_assert.h"
#include "engine/fx/fx.h"
#include "engine/dsp/kernels.h"

#define SR 48000.0f

static float LINE[262144];
static float IN[44100];
static float OUT[44100];

int main(void) {
    uint32_t i;
    /* (a) beats honored through the wrapper. */
    {
        struct RIFX *dl;
        float peak = 0.0f;
        uint32_t at = 0;
        for (i = 0; i < 44100u; i++) {
            IN[i] = 0.0f;
            OUT[i] = 0.0f;
        }
        IN[0] = 1.0f;
        dl = RiFXCreateDelay(LINE, 262144u);
        RI_ASSERT(dl != 0, "delay create");
        RiFXSetParam(dl, RI_FXID_DELAY_BEATS, 2);
        RiFXSetParam(dl, RI_FXID_DELAY_MIX, 127);
        RiFXSetParam(dl, RI_FXID_DELAY_FB, 0);
        RiFXRender(dl, IN, OUT, 44100u, SR, 140.0f);
        for (i = 20560u; i < 20585u; i++)
            if (fabsf(OUT[i]) > peak) {
                peak = fabsf(OUT[i]);
                at = i;
            }
        RI_ASSERT(peak > 0.3f && at >= 20569u && at <= 20573u,
            "echo at %u peak %g (want 20571)", at, peak);
        RI_ASSERT(fabsf(OUT[15429]) < 0.05f, "stale 0.75 tap leaks: %g",
            fabsf(OUT[15429]));
        RiFXDestroy(dl);
    }
    /* (b) pool retired + destroy/reuse. */
    {
        struct RIFX *h[8];
        struct RIFX *r1, *r2;
        RI_ASSERT(RiFXCreate(RI_FX_DELAY) == 0, "Create(DELAY) not retired");
        for (i = 0; i < 8u; i++) {
            h[i] = RiFXCreate((uint32_t)(i % 3u) + 1u);
            RI_ASSERT(h[i] != 0, "fill %u", i);
        }
        RI_ASSERT(RiFXCreate(RI_FX_DIST) == 0, "pool not full");
        RiFXDestroy(h[2]);
        RiFXDestroy(h[5]);
        r1 = RiFXCreate(RI_FX_DIST);
        r2 = RiFXCreate(RI_FX_COMP);
        RI_ASSERT(r1 != 0 && r2 != 0, "slots not reused");
        RiFXDestroy(r1);
        RiFXDestroy(r2);
        RiFXDestroy(r1); /* double destroy safe */
        RiFXDestroy(0); /* null safe */
        /* Free everything (reused slots double-destroyed: safe by design). */
        for (i = 0; i < 8u; i++)
            RiFXDestroy(h[i]);
    }
    /* (c) tap slew (white-box: struct is public). */
    {
        struct RIFX *dl = RiFXCreateDelay(LINE, 262144u);
        uint32_t old, mid, tgt;
        RiFXSetParam(dl, RI_FXID_DELAY_BEATS, 0); /* 0.5 beats */
        for (i = 0; i < 44100u; i++)
            IN[i] = 0.0f;
        IN[0] = 1.0f;
        RiFXSetParam(dl, RI_FXID_DELAY_MIX, 127);
        RiFXSetParam(dl, RI_FXID_DELAY_FB, 0);
        RiFXRender(dl, IN, OUT, 44100u, SR, 140.0f);
        old = dl->delay.delay_smp;
        RiFXSetParam(dl, RI_FXID_DELAY_BEATS, 3); /* 1.5 beats */
        RiFXRender(dl, IN, OUT, 10u, SR, 140.0f);
        mid = dl->delay.delay_smp;
        tgt = dl->delay.target_smp;
        RI_ASSERT(mid != old && mid != tgt, "tap jumped: %u -> %u (tgt %u)",
            old, mid, tgt);
        RiFXRender(dl, IN, OUT, 44100u, SR, 140.0f);
        RI_ASSERT(dl->delay.delay_smp == tgt, "tap never lands: %u vs %u",
            dl->delay.delay_smp, tgt);
        RiFXDestroy(dl);
    }
    /* (d) comp follows render rate. */
    {
        struct RIFX *cp = RiFXCreate(RI_FX_COMP);
        float want = ri_exp(-1.0f / (0.010f * 44100.0f));
        RiFXSetParam(cp, RI_FXID_COMP_THRESH, 64);
        for (i = 0; i < 1024u; i++) {
            IN[i] = 0.5f;
            OUT[i] = 0.0f;
        }
        RiFXRender(cp, IN, OUT, 1024u, 44100.0f, 120.0f);
        RI_ASSERT(fabsf(cp->comp.atk_a - want) < 1e-7f,
            "comp sr stale: %g want %g", cp->comp.atk_a, want);
        RiFXDestroy(cp);
    }
    RI_RESULT("fx_delay_pool");
}
