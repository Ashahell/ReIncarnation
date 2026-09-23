/* t24_909swap — Module 2.4, TC-2.4.5 (mod swap guard):
 *
 *   - Changing the S909 layer set while the voice is active is
 *     refused (rb909_set_layers returns RI_909_BUSY), checked twice:
 *     immediately after trigger and mid-ring (the LD fixture still
 *     rings past 0.5 s at tune 64, so the voice must still be
 *     active there).
 *   - After draining to idle the swap succeeds (rc 0) and the fresh
 *     trigger starts click-free (first-sample edge <= 1e-4, -80 dBFS
 *     on start-at-zero layers, the pack recipe rule).
 * (Mirrors t1_909 §5.)
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
static float OUTA[24000];

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
    CHECK(rb909_set_layers(&s, RB909_BD, DL, 1) == RI_909_BUSY,
        "swap-while-active not refused");
    render_voice(&s, RB909_BD, 24000, OUTA);
    /* voice still active (LD rings past 0.5 s at tune 64): still BUSY */
    CHECK(s.v[RB909_BD].active != 0, "voice went idle too early");
    CHECK(rb909_set_layers(&s, RB909_BD, DL, 1) == RI_909_BUSY,
        "swap-while-active(2) not refused");
    /* drain to idle, then swap must succeed and start click-free */
    while (s.v[RB909_BD].active)
        render_voice(&s, RB909_BD, 24000, OUTA);
    CHECK(rb909_set_layers(&s, RB909_BD, DL, 1) == 0, "idle swap refused");
    rb909_trigger(&s, RB909_BD, 0, 64, 0);
    render_voice(&s, RB909_BD, 64, OUTA);
    CHECK((float)fabs((double)OUTA[0]) <= 1e-4f,
        "idle-swap edge %.6g > -80 dBFS", (double)OUTA[0]);

    if (fails)
        printf("FAIL %d\n", fails);
    else
        printf("PASS t24_909swap\n");
    return fails != 0;
}
