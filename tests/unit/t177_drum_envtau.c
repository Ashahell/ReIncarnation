/* t177_drum_envtau — the crash/ride decay time constant is cached at
 * trigger/knob time, not recomputed per sample (drum-tail A1).
 *
 * WHY. decay_env() ran ri_pow2 (via rb909_decay_scale) + ri_exp per sample
 * per playhead on CR/RD — the 2.09x per-voice price the host bench names.
 * The pow2 half depends only on (voice id, tune), both fixed between
 * trigger and the next trigger, so computing it once per trigger/knob
 * move is bit-identical and ~2.6 us/block cheaper on the host.
 *
 * The trap this pins is the stale cache: TUNE writes v->tune directly
 * outside trigger (params.c, "the knob domain IS the voice domain"), so a
 * cache refreshed only in rb909_trigger goes stale on the first knob move
 * — the same shape as every other cache bug on record. Both refresh paths
 * must agree bit-exactly, and the suite must catch a refresh that only one
 * path performs.
 *
 * Laws; RED-first: env_tau does not exist yet.
 */
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "tests/helpers/ri_assert.h"
#include "engine/dsp/rb909.h"

#define SR 48000.0f
#define N 512u

static float LAY[8192];
static const struct RISampleLayer LAY1[1] = { { LAY, 8192u, 48000u, 0,
    127, { 0, 0 } } };
static float A[N], B[N], C[N];

static void bake(void) {
    uint32_t i;
    for (i = 0u; i < 8192u; i++)
        LAY[i] = 0.5f;
}

static void bind(struct RB909Set *s) {
    uint32_t v;
    for (v = 0u; v < RI_909_NVOICES; v++)
        RI_ASSERT(rb909_set_layers(s, v, LAY1, 1) == 0, "bind %u", v);
}

static void render_cr(struct RB909Set *s, float *out) {
    uint32_t i;
    for (i = 0u; i < N;) {
        uint32_t cc = N - i > 64u ? 64u : N - i, k;
        for (k = 0u; k < cc; k++)
            out[i + k] = rb909_voice_render(&s->v[RB909_CR], SR);
        i += cc;
    }
}

int main(void) {
    bake();

    /* 1. Trigger path and knob path agree bit-exactly at a non-default tune.
     * Tune 100 (not 64): at the default the cache is never read distinctly
     * and a stale-cache mutant would pass vacuously. */
    {
        struct RB909Set s, t;
        uint32_t i;
        rb909_init_set(&s);
        bind(&s);
        rb909_trigger(&s, RB909_CR, 0u, 100u, 0);
        render_cr(&s, A);
        rb909_init_set(&t);
        bind(&t);
        rb909_trigger(&t, RB909_CR, 0u, 64u, 0);
        rb909_set_param(&t.v[RB909_CR], RI_CTL_909_TUNE, 100u);
        render_cr(&t, B);
        for (i = 0u; i < N; i++)
            RI_ASSERT(A[i] == B[i], "trigger/knob disagree at %u", i);
        /* And the tune actually matters (non-vacuous): tune 100 differs
         * from tune 64. */
        {
            int diff = 0;
            rb909_init_set(&t);
            bind(&t);
            rb909_trigger(&t, RB909_CR, 0u, 64u, 0);
            render_cr(&t, C);
            for (i = 0u; i < N; i++)
                if (C[i] != A[i]) {
                    diff = 1;
                    break;
                }
            RI_ASSERT(diff, "tune 100 must sound different from tune 64");
        }
    }

    /* 2. The cached constant follows both writers (pins the shared helper:
     * a copy of the formula in one path drifts). */
    {
        struct RB909Set s;
        float via_trigger, via_knob;
        rb909_init_set(&s);
        bind(&s);
        rb909_trigger(&s, RB909_RD, 0u, 100u, 0);
        via_trigger = s.v[RB909_RD].env_tau;
        rb909_set_param(&s.v[RB909_RD], RI_CTL_909_TUNE, 20u);
        via_knob = s.v[RB909_RD].env_tau;
        RI_ASSERT(via_trigger != via_knob, "cache must follow the tune");
        rb909_trigger(&s, RB909_RD, 0u, 20u, 0);
        RI_ASSERT(s.v[RB909_RD].env_tau == via_knob,
            "trigger and knob must share one computation");
    }

    RI_RESULT("drum_envtau");
    return 0;
}
