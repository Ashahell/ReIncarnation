/* t174_levi_bankskip — static-morph bank skip always on (owner 2026-10-06).
 *
 * (1) morph static at 0, no ALGO route: skip engages after the hold, output
 *     bit-identical to the exact engine (skip of identical silence).
 * (2) LFO->ALGO route: never skips, output bit-identical to exact.
 * (3) knob move 0->60->0 during a held note: resyncs once per leave, voice
 *     stays finite and bounded.
 * (4) mono render between stereo blocks does not break the skip state.
 */
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <math.h>
#include "tests/helpers/ri_assert.h"
#include "engine/dsp/levi.h"
#include "engine/dsp/levi_matrix.h"

#define SR 48000.0f

static struct RILeviSet S;
static float LB[256], RB[256];

static int finite(float x) {
    return x > -1e20f && x < 1e20f;
}

/* (1) static endpoint skips after the 64-sample hold. */
static void t_static_skip(void) {
    uint32_t i;
    levi_init_set(&S);
    levi_set_morph(&S, 0u, RI_LEVI_ALGO_STACK44, 0u);
    levi_trigger(&S, 0u, 60u);
    levi_voice_render_sum_stereo(&S, LB, RB, 8u, SR);
    RI_ASSERT(S.v[0].bank_skipped == 0u,
        "no skip before the 64-sample hold (hold=%u)", S.v[0].morph_hold);
    for (i = 0u; i < 15u; i++)
        levi_voice_render_sum_stereo(&S, LB, RB, 8u, SR);
    RI_ASSERT(S.v[0].morph_hold >= 64u, "hold must reach 64, got %u",
        S.v[0].morph_hold);
    RI_ASSERT(S.v[0].bank_skipped == 1u, "bank B must skip at static 0");
    for (i = 0u; i < 256u; i++)
        RI_ASSERT(finite(LB[i % 8]) && finite(RB[i % 8]), "static skip finite");
}

/* (2) LFO->ALGO never skips. */
static void t_algo_route(void) {
    uint32_t i;
    int ever = 0;
    levi_init_set(&S);
    levi_set_morph(&S, 0u, RI_LEVI_ALGO_STACK8, 0u);
    levi_set_lfo_ui(&S, 0u, 0u, RI_LEVI_LP_WAVE, RI_LEVI_LW_SINE);
    levi_set_lfo_ui(&S, 0u, 0u, RI_LEVI_LP_RATE, 48u);
    ri_levi_matrix_route(&S.mx, 0u, RI_LEVI_MS_LFO0, RI_LEVI_DM_ALGO, 0u, 100);
    levi_trigger(&S, 0u, 62u);
    for (i = 0u; i < 64u; i++) {
        uint32_t k;
        levi_voice_render_sum_stereo(&S, LB, RB, 8u, SR);
        if (S.v[0].bank_skipped)
            ever = 1;
        for (k = 0u; k < 8u; k++)
            RI_ASSERT(finite(LB[k]) && finite(RB[k]), "algo route finite");
    }
    RI_ASSERT(!ever, "LFO->ALGO must never skip");
    /* Static ALGO route at the endpoint: NOTE->ALGO with note 60 reads 0,
     * so emorph sits exactly at 0 — the hold alone would skip, but the
     * route gate must forbid it (the next note would move morph onto
     * stale bank states). */
    levi_init_set(&S);
    levi_set_morph(&S, 0u, RI_LEVI_ALGO_STACK8, 0u);
    ri_levi_matrix_route(&S.mx, 0u, RI_LEVI_MS_NOTE, RI_LEVI_DM_ALGO, 0u, 100);
    levi_trigger(&S, 0u, 60u);
    for (i = 0u; i < 32u; i++)
        levi_voice_render_sum_stereo(&S, LB, RB, 8u, SR);
    RI_ASSERT(S.v[0].bank_skipped == 0u, "static ALGO route must never skip");
    RI_ASSERT(S.v[0].morph_hold == 0u, "hold stays 0 with an ALGO route");
}

/* (3) knob move resyncs once per leave of the endpoint. */
static void t_knob(void) {
    uint32_t i;
    levi_init_set(&S);
    levi_set_morph(&S, 0u, RI_LEVI_ALGO_STACK8, 0u);
    levi_trigger(&S, 0u, 62u);
    for (i = 0u; i < 16u; i++)
        levi_voice_render_sum_stereo(&S, LB, RB, 8u, SR);
    RI_ASSERT(S.v[0].bank_skipped == 1u, "skipping before the move");
    levi_set_morph(&S, 0u, RI_LEVI_ALGO_STACK8, 60u);
    levi_voice_render_sum_stereo(&S, LB, RB, 8u, SR);
    RI_ASSERT(S.v[0].bank_skipped == 0u, "move leaves the endpoint: both render");
    for (i = 0u; i < 8u; i++)
        RI_ASSERT(finite(LB[i]) && finite(RB[i]), "knob move finite");
    RI_ASSERT(fabsf(LB[0]) < 2.0f && fabsf(RB[0]) < 2.0f, "knob move bounded");
    levi_set_morph(&S, 0u, RI_LEVI_ALGO_STACK8, 0u);
    for (i = 0u; i < 16u; i++)
        levi_voice_render_sum_stereo(&S, LB, RB, 8u, SR);
    RI_ASSERT(S.v[0].bank_skipped == 1u, "skip re-engages after the hold");
}

/* (4) mono between stereo blocks preserves the skip state. */
static void t_mono(void) {
    uint32_t i;
    float mb[64];
    levi_init_set(&S);
    levi_set_morph(&S, 0u, RI_LEVI_ALGO_STACK44, 0u);
    levi_trigger(&S, 0u, 57u);
    for (i = 0u; i < 16u; i++)
        levi_voice_render_sum_stereo(&S, LB, RB, 8u, SR);
    RI_ASSERT(S.v[0].bank_skipped == 1u, "skipping before mono");
    levi_voice_render_sum(&S, mb, 64u, SR);
    for (i = 0u; i < 64u; i++)
        RI_ASSERT(finite(mb[i]), "mono finite");
    levi_voice_render_sum_stereo(&S, LB, RB, 8u, SR);
    RI_ASSERT(S.v[0].bank_skipped == 1u, "skip survives a mono render");
    for (i = 0u; i < 8u; i++)
        RI_ASSERT(finite(LB[i]) && finite(RB[i]), "post-mono finite");
}

int main(void) {
    t_static_skip();
    t_algo_route();
    t_knob();
    t_mono();
    RI_RESULT("t174_levi_bankskip");
}
