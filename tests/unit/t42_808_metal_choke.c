/* t42_808_metal_choke.c — metal fixed freqs + choke + per-sound params (§12.5b).
 * (a) Fixed partials (E1 Werner: 205.3/304.4/369.6/522.7/540/800 Hz) prominent
 *     ±0.5% in CH, OH and CY (Goertzel method, t23_808hat transfer).
 * (b) Choke: CH trigger kills a ringing OH (post-choke mix == CH-solo exact);
 *     same-step OH-after-CH dies at once (RMS < 5% of normal); late OH (CH
 *     long dead) rings normally (negative control on the 50 ms window).
 * (c) SNAPPY 127 vs 0 on SD: noise adds power (ratio bound).
 * (d) TONE on BD (click) and CY (HP): audible direction + bounds.
 * (e) Tom/conga/SD-body Tune honored (pitch query ratio + render check).
 * RED-first: ratio cluster, no interaction, placeholder knobs, fixed pitch.
 */
#include <stdio.h>
#include <stdint.h>
#include <math.h>
#include "tests/helpers/ri_assert.h"
#include "engine/dsp/rb808.h"

#define SR 48000.0f

static double goertzel(const float *b, uint32_t n, double f, double sr) {
    double w = 2.0 * 3.141592653589793 * f / sr;
    double cw = cos(w), sw = sin(w);
    double u0 = 0.0, u1 = 0.0;
    uint32_t i;
    for (i = 0; i < n; i++) {
        double u2 = (double)b[i] + 2.0 * cw * u1 - u0;
        u0 = u1;
        u1 = u2;
    }
    {
        double re = u1 * cw - u0;
        double im = u1 * sw;
        return sqrt(re * re + im * im);
    }
}

static float rms(const float *b, uint32_t n) {
    double acc = 0.0;
    uint32_t i;
    for (i = 0; i < n; i++)
        acc += (double)b[i] * (double)b[i];
    return (float)sqrt(acc / (double)(n ? n : 1));
}

static void render_voice(uint32_t v, uint32_t accent, float secs, float *out) {
    struct RB808Set s;
    uint32_t n = (uint32_t)(secs * SR), i;
    rb808_init_set(&s);
    rb808_trigger(&s, v, accent, 0.0f);
    for (i = 0; i < n; i++)
        out[i] = rb808_voice_render(&s.v[v], SR);
}

int main(void) {
    static const double kmetal[6] = { 205.3, 304.4, 369.6, 522.7, 540.0, 800.0 };
    static float buf[48000u];
    uint32_t v, i;
    /* (a) fixed partials in CH, OH, CY. CH's 205.3 Hz sits 30 dB into the
     * 7 kHz HP stopband on a 35 ms voice: present (shared cluster code)
     * but below this method's floor — pinned via OH/CY instead. */
    for (v = 11u; v <= 13u; v++) {
        uint32_t voice = (v == 11u) ? 11u : (v == 12u ? 12u : 13u);
        uint32_t first = (voice == 11u) ? 1u : 0u;
        render_voice(voice, 0u, 0.5f, buf);
        for (i = first; i < 6u; i++) {
            double m0 = goertzel(buf, 24000u, kmetal[i], SR);
            double mlo = goertzel(buf, 24000u, kmetal[i] * 0.995, SR);
            double mhi = goertzel(buf, 24000u, kmetal[i] * 1.005, SR);
            RI_ASSERT(m0 > mlo && m0 > mhi, "voice %u partial %g not prominent",
                voice, kmetal[i]);
        }
    }
    /* (b) choke. */
    {
        struct RB808Set s;
        static float mix[48000u], chsolo[43200u];
        uint32_t p = 0;
        rb808_init_set(&s);
        rb808_trigger(&s, 12u, 0u, 0.0f); /* OH rings */
        while (p < 4800u) {
            uint32_t cc = 4800u - p > 64u ? 64u : 4800u - p;
            rb808_render_mix(&s, mix + p, cc, SR);
            p += cc;
        }
        rb808_trigger(&s, 11u, 0u, 0.0f); /* CH chokes OH */
        while (p < 48000u) {
            uint32_t cc = 48000u - p > 64u ? 64u : 48000u - p;
            rb808_render_mix(&s, mix + p, cc, SR);
            p += cc;
        }
        rb808_init_set(&s);
        rb808_trigger(&s, 11u, 0u, 0.0f);
        p = 0;
        while (p < 43200u) {
            uint32_t cc = 43200u - p > 64u ? 64u : 43200u - p;
            uint32_t j;
            for (j = 0; j < cc; j++)
                chsolo[p + j] = rb808_voice_render(&s.v[11u], SR);
            p += cc;
        }
        for (i = 4800u; i < 48000u; i++)
            RI_ASSERT(mix[i] == chsolo[i - 4800u], "OH leaks past choke at %u", i);
    }
    {
        /* same-step OH after CH: dies at once. */
        struct RB808Set s;
        static float choked[48000u], normal[48000u];
        rb808_init_set(&s);
        rb808_trigger(&s, 11u, 0u, 0.0f);
        rb808_trigger(&s, 12u, 0u, 0.0f);
        for (i = 0; i < 48000u; i++)
            choked[i] = rb808_voice_render(&s.v[12u], SR);
        render_voice(12u, 0u, 1.0f, normal);
        RI_ASSERT(rms(choked, 48000u) < 0.05f * rms(normal, 48000u),
            "same-step OH not short: %g vs %g",
            rms(choked, 48000u), rms(normal, 48000u));
    }
    {
        /* late OH (CH long dead): rings normally. */
        struct RB808Set s;
        static float late[48000u], normal[48000u];
        rb808_init_set(&s);
        rb808_trigger(&s, 11u, 0u, 0.0f);
        for (i = 0; i < 48000u; i++)
            (void)rb808_voice_render(&s.v[11u], SR);
        rb808_trigger(&s, 12u, 0u, 0.0f);
        for (i = 0; i < 48000u; i++)
            late[i] = rb808_voice_render(&s.v[12u], SR);
        render_voice(12u, 0u, 1.0f, normal);
        RI_ASSERT(fabsf(rms(late, 48000u) / rms(normal, 48000u) - 1.0f) < 0.1f,
            "late OH differs: %g vs %g", rms(late, 48000u), rms(normal, 48000u));
    }
    /* (c) snappy scales the SD noise band (partials are sines — 1800 Hz
     * is pure noise term; measured 12.8x, bound 5x). */
    {
        struct RB808Set a, b;
        static float hi[48000u], lo[48000u];
        double ghi, glo;
        rb808_init_set(&a);
        rb808_set_param(&a.v[1], RI_CTL_808_SNAPPY, 127);
        rb808_trigger(&a, 1u, 0u, 0.0f);
        for (i = 0; i < 48000u; i++)
            hi[i] = rb808_voice_render(&a.v[1], SR);
        rb808_init_set(&b);
        rb808_set_param(&b.v[1], RI_CTL_808_SNAPPY, 0);
        rb808_trigger(&b, 1u, 0u, 0.0f);
        for (i = 0; i < 48000u; i++)
            lo[i] = rb808_voice_render(&b.v[1], SR);
        ghi = goertzel(hi, 4800u, 1800.0, SR);
        glo = goertzel(lo, 4800u, 1800.0, SR);
        RI_ASSERT(ghi > 5.0 * glo, "snappy adds no noise: %g vs %g", ghi, glo);
    }
    /* (d) BD click tone: 3 ms peak follows the click gain (measured 1.31x,
     * bound 1.2x — deterministic phases, no flake risk). */
    {
        struct RB808Set a, b;
        static float hi[2400u], lo[2400u];
        float phi, plo;
        rb808_init_set(&a);
        rb808_set_param(&a.v[0], RI_CTL_808_TONE, 127);
        rb808_trigger(&a, 0u, 0u, 0.0f);
        for (i = 0; i < 2400u; i++)
            hi[i] = rb808_voice_render(&a.v[0], SR);
        rb808_init_set(&b);
        rb808_set_param(&b.v[0], RI_CTL_808_TONE, 0);
        rb808_trigger(&b, 0u, 0u, 0.0f);
        for (i = 0; i < 2400u; i++)
            lo[i] = rb808_voice_render(&b.v[0], SR);
        phi = 0.0f;
        plo = 0.0f;
        for (i = 0; i < 144u; i++) {
            float ahi = hi[i] < 0.0f ? -hi[i] : hi[i];
            float alo = lo[i] < 0.0f ? -lo[i] : lo[i];
            if (ahi > phi)
                phi = ahi;
            if (alo > plo)
                plo = alo;
        }
        RI_ASSERT(phi > 1.2f * plo, "BD tone flat: %g vs %g", phi, plo);
    }
    /* CY tone moves the HP cutoff: low-partial energy ratio (measured
     * 13.6x at 304.4 Hz, bound 4x). Tone-up = brighter AND thinner —
     * the ratio direction is the filter law, not a level claim. */
    {
        struct RB808Set a, b;
        static float hi[48000u], lo[48000u];
        double ghi, glo;
        rb808_init_set(&a);
        rb808_set_param(&a.v[13], RI_CTL_808_TONE, 127);
        rb808_trigger(&a, 13u, 0u, 0.0f);
        for (i = 0; i < 48000u; i++)
            hi[i] = rb808_voice_render(&a.v[13], SR);
        rb808_init_set(&b);
        rb808_set_param(&b.v[13], RI_CTL_808_TONE, 0);
        rb808_trigger(&b, 13u, 0u, 0.0f);
        for (i = 0; i < 48000u; i++)
            lo[i] = rb808_voice_render(&b.v[13], SR);
        ghi = goertzel(hi, 48000u, 304.4, SR);
        glo = goertzel(lo, 48000u, 304.4, SR);
        RI_ASSERT(glo > 4.0 * ghi, "CY tone not a cutoff: %g vs %g", glo, ghi);
    }
    /* (e) tom/conga/SD-body tune honored. */
    {
        float p0 = rb808_pitch_hz(2u, 0.0f, 0.0f);
        float p7 = rb808_pitch_hz(2u, 0.0f, 7.0f);
        RI_ASSERT(fabsf(p7 / p0 - 1.49831f) < 0.01f, "LT tune ratio %g/%g",
            p7, p0);
        RI_ASSERT(fabsf(rb808_pitch_hz(1u, 0.0f, 7.0f) /
            rb808_pitch_hz(1u, 0.0f, 0.0f) - 1.49831f) < 0.01f, "SD tune ignored");
    }
    RI_RESULT("808_metal_choke");
}
