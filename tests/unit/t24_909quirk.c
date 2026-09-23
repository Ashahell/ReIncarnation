/* t24_909quirk — Module 2.4, TC-2.4.3 (cymbal/ride accent quirk):
 *
 * Accent input on CR and RD produces NO level change (parity with
 * v2.0): accent-0 vs accent-1 renders are bit-identical (maxdiff ==
 * 0.0), each voice tested directly (no CH/CR proxy).
 * (Mirrors t1_909 §2 quirk values.)
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

static float L0[T24_SRU], L1[T24_SRU];
static float OUTA[14400], OUTB[14400];

static void render_voice(struct RB909Set *s, uint32_t v, uint32_t n, float *out) {
    uint32_t i;
    for (i = 0; i < n; i++)
        out[i] = rb909_voice_render(&s->v[v], T24_SR);
}

static const struct RISampleLayer PAIR2[2] = {
    { L0, T24_SRU, T24_SRU, 0, 63, { 0, 0 } },
    { L1, T24_SRU, T24_SRU, 64, 127, { 0, 0 } }
};

static void quirk_voice(struct RB909Set *s, uint32_t v, const char *name) {
    uint32_t i;
    float md = 0.0f;
    rb909_trigger(s, v, 0, 64, 0);
    render_voice(s, v, 14400, OUTA);
    rb909_trigger(s, v, 1, 64, 0);
    render_voice(s, v, 14400, OUTB);
    for (i = 0; i < 14400; i++) {
        float d = (float)fabs((double)OUTA[i] - (double)OUTB[i]);
        if (d > md)
            md = d;
    }
    CHECK(md == 0.0f, "%s accent not no-op (maxdiff %.6g)", name, (double)md);
}

int main(void) {
    struct RB909Set s;
    uint32_t i;
    for (i = 0; i < T24_SRU; i++) {
        double t = (double)i / 48000.0;
        L0[i] = (float)(0.5 * sin(2.0 * 3.141592653589793 * 440.0 * t));
        L1[i] = (float)(0.5 * sin(2.0 * 3.141592653589793 * 880.0 * t));
    }
    rb909_init_set(&s);
    if (rb909_set_layers(&s, RB909_CR, PAIR2, 2) != 0 ||
        rb909_set_layers(&s, RB909_RD, PAIR2, 2) != 0) {
        printf("FAIL set layers\n");
        fails++;
    }
    quirk_voice(&s, RB909_CR, "cr");
    quirk_voice(&s, RB909_RD, "rd");

    if (fails)
        printf("FAIL %d\n", fails);
    else
        printf("PASS t24_909quirk\n");
    return fails != 0;
}
