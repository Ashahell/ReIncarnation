/* t103_levi_dsp — Levi FM voice bank (owner 2026-09-28, v1 slice 3a).
 * 6 voices x 2-op FM/PM + DAHDSR + resonant LP (manual pp. 35-36, 43
 * subset): init silent, trigger sounds, release rests to exact 0,
 * polyphony sums, determinism bit-exact, params move sound, bad args
 * fail closed.
 */
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "tests/helpers/ri_assert.h"
#include "engine/dsp/levi.h"

static void render_set(struct RILeviSet *s, float *out, uint32_t n) {
    uint32_t i;
    for (i = 0u; i < n; i++) {
        float m = 0.0f;
        uint32_t v;
        for (v = 0u; v < RI_LEVI_NVOICES; v++)
            m += levi_voice_render(&s->v[v], 48000.0f);
        out[i] = m;
    }
}

static uint32_t energy(const float *b, uint32_t n) {
    uint32_t i, k = 0u;
    for (i = 0u; i < n; i++)
        if (b[i] != 0.0f)
            k++;
    return k;
}

int main(void) {
    static struct RILeviSet a, b;
    static float oa[4096], ob[4096];
    RI_ASSERT(RI_LEVI_NVOICES == 6u, "6 voices");
    levi_init_set(&a);
    levi_init_set(&b);
    render_set(&a, oa, 4096u);
    RI_ASSERT(energy(oa, 4096u) == 0u, "silent init");
    /* Trigger sounds (A440); release rests to exact 0. */
    RI_ASSERT(levi_trigger(&a, 0u, 69u) == 0, "trig rc");
    render_set(&a, oa, 2048u);
    RI_ASSERT(energy(oa, 2048u) > 1000u, "sounds %u", energy(oa, 2048u));
    levi_release(&a, 0u);
    render_set(&a, oa, 4096u);
    render_set(&a, ob, 4096u);
    render_set(&a, oa, 4096u);
    RI_ASSERT(energy(oa, 4096u) == 0u, "rests exact");
    RI_ASSERT(a.v[0].active == 0u, "voice sleeps (CPU contract)");
    /* Polyphony: two voices sum; determinism across sets. */
    levi_init_set(&a);
    levi_init_set(&b);
    RI_ASSERT(levi_trigger(&a, 0u, 60u) == 0, "trig");
    RI_ASSERT(levi_trigger(&a, 1u, 64u) == 0, "trig");
    RI_ASSERT(levi_trigger(&b, 0u, 60u) == 0, "trig");
    render_set(&a, oa, 2048u);
    render_set(&b, ob, 2048u);
    RI_ASSERT(memcmp(oa, ob, 2048u * sizeof(float)) != 0, "poly differs from solo");
    levi_init_set(&b);
    RI_ASSERT(levi_trigger(&b, 0u, 60u) == 0, "trig");
    RI_ASSERT(levi_trigger(&b, 1u, 64u) == 0, "trig");
    render_set(&b, ob, 2048u);
    RI_ASSERT(memcmp(oa, ob, 2048u * sizeof(float)) == 0, "deterministic");
    /* Filter stability (Dell 2026-09-28: default 12 kHz cutoff blew the
     * SVF to inf/NaN ~300 samples after trigger — 6 ms of sound, then
     * permanent silence. energy() counts inf/NaN as sound, so this
     * asserts finite + bounded instead). */
    {
        static const uint8_t cuts[4] = { 0u, 64u, 96u, 127u };
        static const uint8_t resos[3] = { 0u, 64u, 127u };
        static const uint8_t modes[2] = { 0u, 1u };
        static const uint8_t ratios[3] = { 0u, 64u, 127u };
        static float og[4800];
        uint32_t ci, ri, mi, ai, k;
        /* Defaults first (the device renders blocks before any knob
         * apply lands): must hold a full second. */
        levi_init_set(&a);
        RI_ASSERT(levi_trigger(&a, 0u, 59u) == 0, "trig");
        RI_ASSERT(levi_trigger(&a, 1u, 62u) == 0, "trig");
        RI_ASSERT(levi_trigger(&a, 2u, 66u) == 0, "trig");
        for (k = 0u; k < 10u; k++) {
            uint32_t j;
            render_set(&a, og, 4800u);
            for (j = 0u; j < 4800u; j++)
                RI_ASSERT(og[j] > -8.0f && og[j] < 8.0f, "default finite %u:%u %f", k, j, og[j]);
        }
        /* UI extremes grid (NaN/inf fail every comparison: one assert
         * covers non-finite; no amplitude bound — max reso is meant
         * to scream, stability means finite forever). */
        for (ci = 0u; ci < 4u; ci++)
            for (ri = 0u; ri < 3u; ri++)
                for (mi = 0u; mi < 2u; mi++)
                    for (ai = 0u; ai < 3u; ai++) {
                        uint32_t j, v;
                        levi_init_set(&a);
                        for (v = 0u; v < RI_LEVI_NVOICES; v++) {
                            RI_ASSERT(levi_set_param_ui(&a, v, RI_LEVI_CUTOFF, cuts[ci]) == 0, "cut");
                            RI_ASSERT(levi_set_param_ui(&a, v, RI_LEVI_RESO, resos[ri]) == 0, "reso");
                            RI_ASSERT(levi_set_param_ui(&a, v, RI_LEVI_MODE, modes[mi]) == 0, "mode");
                            RI_ASSERT(levi_set_param_ui(&a, v, RI_LEVI_RATIO, ratios[ai]) == 0, "ratio");
                        }
                        RI_ASSERT(levi_trigger(&a, 0u, 59u) == 0, "trig");
                        render_set(&a, og, 4800u);
                        for (j = 0u; j < 4800u; j++)
                            RI_ASSERT(og[j] > -1e30f && og[j] < 1e30f, "grid %u/%u/%u/%u:%u %f",
                                cuts[ci], resos[ri], modes[mi], ratios[ai], j, og[j]);
                    }
    }
    /* Params move sound; bad args fail closed. */
    levi_init_set(&a);
    RI_ASSERT(levi_trigger(&a, 0u, 60u) == 0, "trig");
    render_set(&a, oa, 2048u);
    RI_ASSERT(levi_set_param(&a, 0u, RI_LEVI_CUTOFF, 400.0f) == 0, "param rc");
    render_set(&a, ob, 2048u);
    RI_ASSERT(memcmp(oa, ob, 2048u * sizeof(float)) != 0, "cutoff moves");
    RI_ASSERT(levi_trigger(&a, 6u, 60u) == 2, "bad voice");
    RI_ASSERT(levi_trigger(0, 0u, 60u) == 2, "null trig");
    RI_ASSERT(levi_trigger(&a, 0u, 128u) == 2, "bad note");
    RI_ASSERT(levi_set_param(&a, 6u, RI_LEVI_CUTOFF, 400.0f) == 2, "bad param voice");
    RI_ASSERT(levi_set_param(&a, 0u, 99u, 400.0f) == 2, "bad param id");
    RI_ASSERT(levi_set_param(0, 0u, RI_LEVI_CUTOFF, 400.0f) == 2, "null param");
    levi_release(&a, 6u);
    levi_release(0, 0u);
    RI_RESULT("levidsp");
}
