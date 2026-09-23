/* t24_909retrig — Module 2.4, TC-2.4.4 (retrigger cuts previous):
 *
 * Monophonic retrigger resets both playheads: a voice triggered
 * mid-ring renders bit-identically to a fresh trigger from the cut
 * point on (post-cut maxdiff == 0.0 — no voice overlap, no tail
 * leak). (Mirrors t1_909 §4.)
 *
 * Green pin on frozen code; the RED driver for the slice is
 * t24_909xfade (rb909_set_param does not exist yet).
 */
#include <stdio.h>
#include <stdint.h>
#include <math.h>
#include "engine/dsp/rb909.h"

#define T24_SR 48000.0f
#define T24_SRU 48000u

static int fails = 0;
#define CHECK(cond, ...) do { \
    if (!(cond)) { printf("FAIL "); printf(__VA_ARGS__); printf("\n"); fails++; } \
} while (0)

static float LD[T24_SRU];
static float OUTA[14400], OUTB[14400], OUTC[14400];

static void render_voice(struct RB909Set *s, uint32_t v, uint32_t n, float *out) {
    uint32_t i;
    for (i = 0; i < n; i++)
        out[i] = rb909_voice_render(&s->v[v], T24_SR);
}

int main(void) {
    static const struct RISampleLayer DL[1] = {
        { LD, T24_SRU, T24_SRU, 0, 127, { 0, 0 } }
    };
    struct RB909Set s;
    uint32_t i;
    float md = 0.0f;
    for (i = 0; i < T24_SRU; i++) {
        double t = (double)i / 48000.0;
        LD[i] = (float)(0.8 * sin(2.0 * 3.141592653589793 * 440.0 * t) *
            exp(-t / 0.20));
    }
    rb909_init_set(&s);
    if (rb909_set_layers(&s, RB909_BD, DL, 1) != 0) {
        printf("FAIL set layers\n");
        fails++;
    }
    rb909_trigger(&s, RB909_BD, 0, 64, 0);
    render_voice(&s, RB909_BD, 14400, OUTA);
    rb909_trigger(&s, RB909_BD, 0, 64, 0); /* retrigger cuts */
    render_voice(&s, RB909_BD, 14400, OUTB);
    rb909_trigger(&s, RB909_BD, 0, 64, 0); /* fresh reference */
    render_voice(&s, RB909_BD, 14400, OUTC);
    for (i = 0; i < 14400; i++) {
        float d = (float)fabs((double)OUTB[i] - (double)OUTC[i]);
        if (d > md)
            md = d;
    }
    CHECK(md == 0.0f, "retrigger tail leaks (maxdiff %.6g)", (double)md);

    if (fails)
        printf("FAIL %d\n", fails);
    else
        printf("PASS t24_909retrig\n");
    return fails != 0;
}
