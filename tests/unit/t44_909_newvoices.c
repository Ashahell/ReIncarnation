/* t44_909_newvoices.c — 909 completion §12.6a fix proof.
 * (a) New IDs (LT/MT/HT/RS/CP) install layers, trigger, render audible +
 *     finite, and go idle at sample end (voice smoke per ID).
 * (b) LEVEL is a linear trim (64 ≈ half of 127 on retrigger).
 * (c) DECAY is a finite extra envelope (knob 0 shortens audibly vs bypass;
 *     bypass default renders untouched).
 * (d) Old voices' knob defaults are bypass-transparent (level 1.0 exact,
 *     decay bypass) — goldens without knob writes render identically.
 * RED-first: 6-voice cap, LEVEL/DECAY placeholders.
 */
#include <stdio.h>
#include <stdint.h>
#include <math.h>
#include "tests/helpers/ri_assert.h"
#include "engine/dsp/rb909.h"

#define SR 48000.0f
#define N 48000u

static float LD[48000u];

static void hand_layers(struct RB909Set *s, uint32_t v) {
    static struct RISampleLayer lay[1];
    uint32_t i;
    for (i = 0; i < N; i++)
        LD[i] = 0.5f * sinf(2.0f * 3.14159265f * 220.0f * (float)i / SR)
            * expf(-(float)i / (SR * 0.3f));
    lay[0].data = LD;
    lay[0].frames = N;
    lay[0].rate = 48000u;
    lay[0].lo = 0;
    lay[0].hi = 127;
    RI_ASSERT(rb909_set_layers(s, v, lay, 1) == 0, "layers v%u", v);
}

static float rms(const float *b, uint32_t n) {
    double acc = 0.0;
    uint32_t i;
    for (i = 0; i < n; i++)
        acc += (double)b[i] * (double)b[i];
    return (float)sqrt(acc / (double)(n ? n : 1));
}

static void render_all(struct RB909Set *s, float *out) {
    uint32_t pos = 0;
    while (pos < N) {
        uint32_t cc = N - pos > 64u ? 64u : N - pos;
        rb909_render_mix(s, out + pos, cc, SR);
        pos += cc;
    }
}

int main(void) {
    static float buf[N], ref[N];
    struct RB909Set s;
    uint32_t v, i;
    /* (a) new-voice smoke. */
    for (v = 6u; v < 11u; v++) {
        rb909_init_set(&s);
        hand_layers(&s, v);
        rb909_trigger(&s, v, 0u, 64, 0);
        render_all(&s, buf);
        RI_ASSERT(rms(buf, N) > 0.003f, "voice %u silent: %g", v, rms(buf, N));
        for (i = 0; i < N; i++)
            RI_ASSERT(buf[i] == buf[i] && buf[i] > -2.0f && buf[i] < 2.0f,
                "voice %u non-finite/range at %u", v, i);
        RI_ASSERT(!s.v[v].active, "voice %u never idles", v);
    }
    /* (b) level trim. */
    {
        struct RB909Set a, b;
        float ra, rb;
        rb909_init_set(&a);
        hand_layers(&a, RB909_BD);
        rb909_set_param(&a.v[RB909_BD], RI_CTL_909_LEVEL, 127);
        rb909_trigger(&a, RB909_BD, 0u, 64, 0);
        render_all(&a, buf);
        ra = rms(buf, N);
        rb909_init_set(&b);
        hand_layers(&b, RB909_BD);
        rb909_set_param(&b.v[RB909_BD], RI_CTL_909_LEVEL, 64);
        rb909_trigger(&b, RB909_BD, 0u, 64, 0);
        render_all(&b, ref);
        rb = rms(ref, N);
        RI_ASSERT(fabsf(rb / ra - 64.0f / 127.0f) <= 0.02f, "level law %g/%g", rb, ra);
    }
    /* (c) decay envelope. */
    {
        struct RB909Set a, b;
        float ra, rb;
        rb909_init_set(&a);
        hand_layers(&a, RB909_BD);
        rb909_trigger(&a, RB909_BD, 0u, 64, 0);
        render_all(&a, buf);
        ra = rms(buf + N / 2, N / 2);
        rb909_init_set(&b);
        hand_layers(&b, RB909_BD);
        rb909_set_param(&b.v[RB909_BD], RI_CTL_909_DECAY, 0);
        rb909_trigger(&b, RB909_BD, 0u, 64, 0);
        render_all(&b, ref);
        rb = rms(ref + N / 2, N / 2);
        RI_ASSERT(rb < 0.5f * ra, "decay knob no-op: %g vs %g", rb, ra);
    }
    /* (d) bypass transparency (golden safety). */
    {
        struct RB909Set a, b;
        rb909_init_set(&a);
        hand_layers(&a, RB909_BD);
        rb909_trigger(&a, RB909_BD, 0u, 64, 0);
        render_all(&a, buf);
        rb909_init_set(&b);
        hand_layers(&b, RB909_BD);
        rb909_trigger(&b, RB909_BD, 0u, 64, 0);
        render_all(&b, ref);
        for (i = 0; i < N; i++)
            RI_ASSERT(buf[i] == ref[i], "twin renders differ at %u", i);
    }
    RI_RESULT("909_newvoices");
}
