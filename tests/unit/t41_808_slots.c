/* t41_808_slots.c — 808 slot/switch/MA/level contract (§12.5a fix proof).
 * (a) Collision last-wins: LT then LC → mix == LC-solo, != LT-solo.
 * (b) Maracas (new 16th sound): audible, accent +3.52 dB ±0.5.
 * (c) LEVEL knob is a linear trim (64 ≈ half of 127 on retrigger).
 * (d) ACCENT knob sets excitation continuously (127 → ×2.0 vs ×1.5 default).
 * (e) Storm of all 16 == storm of the 11 slot-selected (only slots render).
 * RED-first: 15 flat voices, no MA, LEVEL/ACCENT placeholders.
 */
#include <stdio.h>
#include <stdint.h>
#include <math.h>
#include "tests/helpers/ri_assert.h"
#include "engine/dsp/rb808.h"

#define SR 48000.0f
#define N 48000u

static float mixbuf[N], refbuf[N];

static void render_mix_all(struct RB808Set *s, float *out) {
    uint32_t pos = 0;
    while (pos < N) {
        uint32_t cc = N - pos > 64u ? 64u : N - pos;
        rb808_render_mix(s, out + pos, cc, SR);
        pos += cc;
    }
}

static void render_solo(uint32_t sound, uint32_t accent, float *out) {
    struct RB808Set s;
    uint32_t pos = 0;
    rb808_init_set(&s);
    rb808_trigger(&s, sound, accent, 0.0f);
    while (pos < N) {
        uint32_t cc = N - pos > 64u ? 64u : N - pos;
        uint32_t i;
        for (i = 0; i < cc; i++)
            out[pos + i] = rb808_voice_render(&s.v[sound], SR);
        pos += cc;
    }
}

static float rms(const float *b, uint32_t n) {
    double acc = 0.0;
    uint32_t i;
    for (i = 0; i < n; i++)
        acc += (double)b[i] * (double)b[i];
    return (float)sqrt(acc / (double)(n ? n : 1));
}

int main(void) {
    struct RB808Set s;
    uint32_t i, v;
    /* (a) collision last-wins (LT=2, LC=5 share a slot). */
    rb808_init_set(&s);
    rb808_trigger(&s, 2u, 0u, 0.0f);
    rb808_trigger(&s, 5u, 0u, 0.0f);
    render_mix_all(&s, mixbuf);
    render_solo(5u, 0u, refbuf);
    for (i = 0; i < N; i++)
        RI_ASSERT(mixbuf[i] == refbuf[i], "slot not last-wins at %u", i);
    /* (b) maracas audible + accent. */
    render_solo(RB808_MA, 0u, refbuf);
    RI_ASSERT(rms(refbuf, N) > 1e-3f, "MA silent: %g", rms(refbuf, N));
    RI_ASSERT(rms(refbuf, 4800u) > 0.02f, "MA body missing (click only): %g",
        rms(refbuf, 4800u));
    render_solo(RB808_MA, 1u, mixbuf);
    {
        float db = 20.0f * log10f(rms(mixbuf, N) / rms(refbuf, N));
        RI_ASSERT(fabsf(db - 3.52f) <= 0.5f, "MA accent %+.3f dB", db);
    }
    /* (c) LEVEL linear trim (retrigger = identical excitation). */
    {
        struct RB808Set a, b;
        float ra, rb;
        rb808_init_set(&a);
        rb808_set_param(&a.v[0], RI_CTL_808_LEVEL, 127);
        rb808_trigger(&a, 0u, 0u, 0.0f);
        render_mix_all(&a, mixbuf);
        ra = rms(mixbuf, N);
        rb808_init_set(&b);
        rb808_set_param(&b.v[0], RI_CTL_808_LEVEL, 64);
        rb808_trigger(&b, 0u, 0u, 0.0f);
        render_mix_all(&b, refbuf);
        rb = rms(refbuf, N);
        RI_ASSERT(fabsf(rb / ra - 64.0f / 127.0f) <= 0.02f, "level law %g/%g", rb, ra);
    }
    /* (d) ACCENT knob: 127 -> x2.0 excitation vs x1.5 default. */
    {
        struct RB808Set a, b;
        float ra, rb;
        rb808_init_set(&a);
        rb808_trigger(&a, 0u, 1u, 0.0f);
        render_mix_all(&a, mixbuf);
        ra = rms(mixbuf, N);
        rb808_init_set(&b);
        rb808_set_param(&b.v[0], RI_CTL_808_ACCENT, 127);
        rb808_trigger(&b, 0u, 1u, 0.0f);
        render_mix_all(&b, refbuf);
        rb = rms(refbuf, N);
        RI_ASSERT(fabsf(rb / ra - 2.0f / 1.5f) <= 0.05f, "accent law %g/%g", rb, ra);
    }
    /* (e) 11 slots render (trigger order 0..15 selects the higher pair). */
    {
        static const uint32_t sel[11] = { 0, 1, 5, 6, 7, 9, 15, 14, 13, 12, 11 };
        struct RB808Set b;
        rb808_init_set(&s);
        for (v = 0; v < 16u; v++)
            rb808_trigger(&s, v, 0u, 0.0f);
        render_mix_all(&s, mixbuf);
        rb808_init_set(&b);
        for (i = 0; i < 11u; i++)
            rb808_trigger(&b, sel[i], 0u, 0.0f);
        render_mix_all(&b, refbuf);
        for (i = 0; i < N; i++)
            RI_ASSERT(mixbuf[i] == refbuf[i], "slot render differs at %u", i);
    }
    RI_RESULT("808_slots");
}
