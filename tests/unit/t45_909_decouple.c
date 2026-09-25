/* t45_909_decouple.c — flam/velocity decoupling + hat rules + linear mix (§12.6b).
 * (a) Flam bit decoupled: arm_flam + accent 0 fires the second hit at the
 *     armed width; accent==2 compat path unchanged (guard).
 * (b) OH-wins: same-step OH+CH in either order renders OH-identical; a late
 *     CH kills a ringing OH (post-kill mix == CH-solo exact).
 * (c) Shared hat level: one call moves CH and OH together.
 * (d) Linear mix: two loud voices sum bit-exact (no tanh clip).
 * RED-first: no arm/hat API; symmetric steal; tanh clip above 1.0.
 */
#include <stdio.h>
#include <stdint.h>
#include <math.h>
#include <stdlib.h>
#include "tests/helpers/ri_assert.h"
#include "engine/dsp/rb909.h"

#define SR 48000.0f
#define N 48000u

/* Layer buffers: LONG (0.4 s decay, audibility/idle) and SHORT (8 ms decay,
 * flam-onset tests — the main voice must be dead when the second hits). */
static float LDL[N];
static float LDS[N];

static void hand_layers(struct RB909Set *s, uint32_t v, float *buf, float tau) {
    static struct RISampleLayer lay[1];
    uint32_t i;
    for (i = 0; i < N; i++)
        buf[i] = 0.8f * sinf(2.0f * 3.14159265f * 440.0f * (float)i / SR) *
            expf(-(float)i / (SR * tau));
    lay[0].data = buf;
    lay[0].frames = N;
    lay[0].rate = 48000u;
    lay[0].lo = 0;
    lay[0].hi = 127;
    RI_ASSERT(rb909_set_layers(s, v, lay, 1) == 0, "layers v%u", v);
}

static void render_voice(struct RB909Set *s, uint32_t v, uint32_t n, float *out) {
    uint32_t i;
    for (i = 0; i < n; i++)
        out[i] = rb909_voice_render(&s->v[v], SR);
}

static void render_mix(struct RB909Set *s, float *out) {
    uint32_t p = 0;
    while (p < N) {
        uint32_t cc = N - p > 64u ? 64u : N - p;
        rb909_render_mix(s, out + p, cc, SR);
        p += cc;
    }
}

static int second_onset(const float *b, uint32_t from) {
    uint32_t i;
    for (i = from; i < N; i++)
        if (fabsf(b[i]) > 0.1f)
            return (int)i;
    return -1;
}

int main(void) {
    static float a[N], b[N];
    struct RB909Set s;
    uint32_t i;
    int o;
    /* (a) armed flam with accent 0 (short layers: main dead by the hit).
     * Order mirrors the event flow: NOTE trigger first, FLAM arm after
     * (a pre-trigger arm is cleared by design — no stale-flam reuse). */
    rb909_init_set(&s);
    hand_layers(&s, RB909_BD, LDS, 0.008f);
    rb909_trigger(&s, RB909_BD, 0u, 64, 0);
    rb909_arm_flam(&s, RB909_BD, 2400);
    render_voice(&s, RB909_BD, N, a);
    o = second_onset(a, 1000u);
    RI_ASSERT(o > 0, "armed flam never fired");
    if (o > 0)
        RI_ASSERT(abs(o - 2400) <= 240, "armed flam at %d want 2400+-240", o);
    /* accent==2 compat: still fires (guard). */
    rb909_init_set(&s);
    hand_layers(&s, RB909_BD, LDS, 0.008f);
    rb909_trigger(&s, RB909_BD, 2u, 64, 1680);
    render_voice(&s, RB909_BD, N, b);
    o = second_onset(b, 1000u);
    RI_ASSERT(o > 0 && abs(o - 1680) <= 240, "compat flam moved: %d", o);
    /* (b) OH-wins, both orders (OH-identical). */
    rb909_init_set(&s);
    hand_layers(&s, RB909_CH, LDL, 0.4f);
    hand_layers(&s, RB909_OH, LDL, 0.4f);
    rb909_trigger(&s, RB909_CH, 0u, 64, 0);
    rb909_trigger(&s, RB909_OH, 0u, 64, 0);
    render_voice(&s, RB909_OH, N, a);
    rb909_init_set(&s);
    hand_layers(&s, RB909_OH, LDL, 0.4f);
    rb909_trigger(&s, RB909_OH, 0u, 64, 0);
    render_voice(&s, RB909_OH, N, b);
    for (i = 0; i < N; i++)
        RI_ASSERT(a[i] == b[i], "CH-then-OH not OH-identical at %u", i);
    rb909_init_set(&s);
    hand_layers(&s, RB909_CH, LDL, 0.4f);
    hand_layers(&s, RB909_OH, LDL, 0.4f);
    rb909_trigger(&s, RB909_OH, 0u, 64, 0);
    rb909_trigger(&s, RB909_CH, 0u, 64, 0);
    render_voice(&s, RB909_OH, N, a);
    for (i = 0; i < N; i++)
        RI_ASSERT(a[i] == b[i], "OH-then-CH not OH-identical at %u", i);
    /* late CH (OH ringing 100 ms) kills it: post-kill == CH-solo. */
    {
        static float mix[N], chsolo[43200u];
        uint32_t p = 0;
        rb909_init_set(&s);
        hand_layers(&s, RB909_CH, LDL, 0.4f);
        hand_layers(&s, RB909_OH, LDL, 0.4f);
        rb909_trigger(&s, RB909_OH, 0u, 64, 0);
        while (p < 4800u) {
            uint32_t cc = 4800u - p > 64u ? 64u : 4800u - p;
            rb909_render_mix(&s, mix + p, cc, SR);
            p += cc;
        }
        rb909_trigger(&s, RB909_CH, 0u, 64, 0);
        while (p < N) {
            uint32_t cc = N - p > 64u ? 64u : N - p;
            rb909_render_mix(&s, mix + p, cc, SR);
            p += cc;
        }
        rb909_init_set(&s);
        hand_layers(&s, RB909_CH, LDL, 0.4f);
        rb909_trigger(&s, RB909_CH, 0u, 64, 0);
        p = 0;
        while (p < 43200u) {
            uint32_t cc = 43200u - p > 64u ? 64u : 43200u - p;
            for (i = 0; i < cc; i++)
                chsolo[p + i] = rb909_voice_render(&s.v[RB909_CH], SR);
            p += cc;
        }
        for (i = 4800u; i < N; i++)
            RI_ASSERT(mix[i] == chsolo[i - 4800u], "OH leaks past late choke at %u", i);
    }
    /* (c) shared hat level. */
    rb909_init_set(&s);
    hand_layers(&s, RB909_CH, LDL, 0.4f);
    hand_layers(&s, RB909_OH, LDL, 0.4f);
    rb909_set_hat_level(&s, 127);
    RI_ASSERT(s.v[RB909_CH].level == 1.0f && s.v[RB909_OH].level == 1.0f,
        "hat share not joint at full");
    rb909_set_hat_level(&s, 64);
    RI_ASSERT(s.v[RB909_CH].level == s.v[RB909_OH].level &&
        fabsf(s.v[RB909_CH].level - 64.0f / 127.0f) < 1e-6f,
        "hat share not joint at mid: %g %g",
        s.v[RB909_CH].level, s.v[RB909_OH].level);
    /* (d) linear mix of two loud voices (identical layers -> 2x single). */
    {
        struct RB909Set m, o;
        static float mx[N], sm[N];
        float peak = 0.0f;
        rb909_init_set(&m);
        hand_layers(&m, RB909_BD, LDL, 0.4f);
        hand_layers(&m, RB909_SD, LDL, 0.4f);
        rb909_trigger(&m, RB909_BD, 1u, 64, 0);
        rb909_trigger(&m, RB909_SD, 1u, 64, 0);
        render_mix(&m, mx);
        rb909_init_set(&o);
        hand_layers(&o, RB909_BD, LDL, 0.4f);
        hand_layers(&o, RB909_SD, LDL, 0.4f);
        rb909_trigger(&o, RB909_BD, 1u, 64, 0);
        rb909_trigger(&o, RB909_SD, 1u, 64, 0);
        for (i = 0; i < N; i++) {
            float x = rb909_voice_render(&o.v[RB909_BD], SR) +
                rb909_voice_render(&o.v[RB909_SD], SR);
            float ax = x < 0 ? -x : x;
            sm[i] = x;
            if (ax > peak)
                peak = ax;
            RI_ASSERT(mx[i] == sm[i], "nonlinear at %u", i);
        }
        RI_ASSERT(peak > 1.0f, "mix never exercises clip: %g", peak);
    }
    RI_RESULT("909_decouple");
}
