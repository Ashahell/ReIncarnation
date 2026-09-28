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
    /* Operator modes (owner 2026-09-28, v2 slice 1b: own definitions).
     * Every mode sounds finite; each differs from FM; bad mode refused;
     * per-op select composes (carrier FM + modulator PWM). */
    {
        static const uint32_t all[7] = { RI_LEVI_FM, RI_LEVI_PM, RI_LEVI_PWM,
            RI_LEVI_SYNC, RI_LEVI_PDSAW, RI_LEVI_PDSQ, RI_LEVI_PDPULSE };
        static float om[4800], fm[4800];
        uint32_t mi, j;
        levi_init_set(&a);
        RI_ASSERT(levi_trigger(&a, 0u, 59u) == 0, "trig");
        render_set(&a, fm, 4800u);
        for (mi = 1u; mi < 7u; mi++) {
            levi_init_set(&a);
            RI_ASSERT(levi_set_param(&a, 0u, RI_LEVI_MODE, (float)all[mi]) == 0, "mode %u", mi);
            RI_ASSERT(levi_trigger(&a, 0u, 59u) == 0, "trig");
            render_set(&a, om, 4800u);
            RI_ASSERT(energy(om, 4800u) > 1000u, "mode %u sounds", mi);
            for (j = 0u; j < 4800u; j++)
                RI_ASSERT(om[j] > -1e30f && om[j] < 1e30f, "mode %u finite", mi);
            RI_ASSERT(memcmp(om, fm, sizeof om) != 0, "mode %u differs", mi);
        }
        RI_ASSERT(levi_set_param(&a, 0u, RI_LEVI_MODE, 7.0f) == 2, "bad mode");
        RI_ASSERT(levi_set_param(&a, 0u, RI_LEVI_MODE, -1.0f) == 2, "neg mode");
        /* Per-op select: modulator PWM under an FM carrier. */
        levi_init_set(&a);
        RI_ASSERT(levi_set_op_mode(&a, 0u, 1u, RI_LEVI_PWM) == 0, "op mode");
        RI_ASSERT(a.v[0].op[1].mode == RI_LEVI_PWM && a.v[0].op[0].mode == RI_LEVI_FM, "op split");
        RI_ASSERT(levi_trigger(&a, 0u, 59u) == 0, "trig");
        render_set(&a, om, 4800u);
        RI_ASSERT(energy(om, 4800u) > 1000u && memcmp(om, fm, sizeof om) != 0, "op mode sounds");
        RI_ASSERT(levi_set_op_mode(&a, 0u, 1u, 7u) == 2, "bad op mode");
        RI_ASSERT(levi_set_op_mode(&a, 0u, 8u, RI_LEVI_PWM) == 2, "bad op");
        RI_ASSERT(levi_set_op_mode(&a, 6u, 0u, RI_LEVI_PWM) == 2, "bad voice");
        RI_ASSERT(levi_set_op_mode(0, 0u, 0u, RI_LEVI_PWM) == 2, "null set");
    }
    /* Filter types + drive (owner 2026-09-28, v2 slice 2a: SVF taps,
     * pre-drive shaper, 24 dB second stage). Each type sounds finite
     * and differs from LP; bad type refused. */
    {
        static const uint32_t types[4] = { RI_LEVI_FTYPE_LP, RI_LEVI_FTYPE_HP,
            RI_LEVI_FTYPE_BP, RI_LEVI_FTYPE_NOTCH };
        static float fo[4800], lp[4800];
        uint32_t ti, j;
        levi_init_set(&a);
        RI_ASSERT(levi_trigger(&a, 0u, 59u) == 0, "trig");
        render_set(&a, lp, 4800u);
        for (ti = 1u; ti < 4u; ti++) {
            levi_init_set(&a);
            RI_ASSERT(levi_set_param(&a, 0u, RI_LEVI_FTYPE, (float)types[ti]) == 0, "type %u", ti);
            RI_ASSERT(levi_trigger(&a, 0u, 59u) == 0, "trig");
            render_set(&a, fo, 4800u);
            RI_ASSERT(energy(fo, 4800u) > 100u, "type %u sounds", ti);
            for (j = 0u; j < 4800u; j++)
                RI_ASSERT(fo[j] > -1e30f && fo[j] < 1e30f, "type %u finite", ti);
            RI_ASSERT(memcmp(fo, lp, sizeof fo) != 0, "type %u differs", ti);
        }
        RI_ASSERT(levi_set_param(&a, 0u, RI_LEVI_FTYPE, 4.0f) == 2, "bad type");
        RI_ASSERT(levi_set_param(&a, 0u, RI_LEVI_FTYPE, -1.0f) == 2, "neg type");
        /* Drive adds harmonics (differs), stays finite; second stage
         * steepens (LP+drive vs dry LP differ). */
        levi_init_set(&a);
        RI_ASSERT(levi_set_param(&a, 0u, RI_LEVI_DRIVE, 0.8f) == 0, "drive");
        RI_ASSERT(levi_trigger(&a, 0u, 59u) == 0, "trig");
        render_set(&a, fo, 4800u);
        RI_ASSERT(energy(fo, 4800u) > 100u, "drive sounds");
        for (j = 0u; j < 4800u; j++)
            RI_ASSERT(fo[j] > -1e30f && fo[j] < 1e30f, "drive finite");
        RI_ASSERT(memcmp(fo, lp, sizeof fo) != 0, "drive differs");
        RI_ASSERT(levi_set_param(&a, 0u, RI_LEVI_DRIVE, 2.0f) == 2, "drive range");
        RI_ASSERT(levi_set_param(&a, 0u, RI_LEVI_DRIVE, -0.5f) == 2, "drive neg");
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
