/* t22_303slide — TC-2.2.3 slide legato (WBS 2.2 property pin).
 * Two-note slide retrigger: envelopes do not reset, pitch slews (not
 * jumps) and reaches target within 5*tau_slide. Covers both entry
 * forms: rb303_note(..., slide=1) on a held voice, and
 * rb303_slide_to CONTINUE. Voice struct fields read directly (white
 * box); sample-stepped render (1-frame steps) for trajectory asserts.
 * tau_slide default 0.040 s -> 5*tau = 9600 samples @48 kHz.
 */
#include <stdio.h>
#include <stdint.h>
#include <math.h>
#include "engine/dsp/rb303.h"

#define T22S_SR 48000.0f
/* 5*tau_slide at default tau 0.040 s, 48 kHz = 9600 samples. */

static int fails = 0;
#define CHECK(cond, ...) do { \
    if (!(cond)) { printf("FAIL "); printf(__VA_ARGS__); printf("\n"); fails++; } \
} while (0)

static void setup(struct RB303Voice *v) {
    float s[64u];
    rb303_init(v);
    rb303_set_param(v, RI_CTL_303A_CUTOFF, 80);
    rb303_set_param(v, RI_CTL_303A_RESO, 40);
    rb303_set_param(v, RI_CTL_303A_ENVMOD, 64);
    rb303_set_param(v, RI_CTL_303A_DECAY, 64);
    rb303_set_param(v, RI_CTL_303A_ACCENT, 96);
    rb303_set_param(v, RI_CTL_303A_WAVE, 0);
    rb303_set_param(v, RI_CTL_303A_VOLUME, 127);
    rb303_note(v, 45, 0, 0);
    rb303_render(v, s, 64u, T22S_SR);
    (void)s;
}

int main(void) {
    struct RB303Voice v;
    float s[1u];
    float meg0, veg0, tgt;
    uint32_t n;
    int reached = 0;
    setup(&v);

    /* Retrigger with slide: envelopes must NOT reset, gate stays high. */
    meg0 = v.meg;
    veg0 = v.veg;
    CHECK(v.gate == 1.0f, "gate %f want 1", (double)v.gate);
    rb303_note(&v, 52, 1, 0);
    CHECK(v.gate == 1.0f, "gate after slide %f", (double)v.gate);
    CHECK(v.meg == meg0 && v.veg == veg0, "env reset meg %f was %f veg %f was %f",
        (double)v.meg, (double)meg0, (double)v.veg, (double)veg0);
    tgt = v.target_freq;

    /* Slew, not jump: after 1 sample still far from target. */
    rb303_render(&v, s, 1u, T22S_SR);
    CHECK(fabsf((v.freq - tgt) / tgt) > 0.10f, "jumped instantly %f vs %f",
          (double)v.freq, (double)tgt);

    /* Reach within 5*tau (9600 samples), envs keep decaying (no reset). */
    for (n = 0u; n < 9600u; n++) {
        float mprev = v.meg, vprev = v.veg;
        rb303_render(&v, s, 1u, T22S_SR);
        if (v.meg > mprev * 1.001f + 1e-6f || v.veg > vprev * 1.001f + 1e-6f) {
            printf("FAIL env reset at %u (meg %f -> %f veg %f -> %f)\n", n,
                   (double)mprev, (double)v.meg, (double)vprev, (double)v.veg);
            fails++;
            break;
        }
        if (fabsf((v.freq - tgt) / tgt) <= 0.01f) {
            reached = 1;
            break;
        }
    }
    CHECK(reached, "no reach within 5tau");
    /* slide_to form: target-only, nothing reset. */
    {
        float m1 = v.meg, v1 = v.veg;
        rb303_slide_to(&v, 45);
        CHECK(v.meg == m1 && v.veg == v1 && v.gate == 1.0f, "slide_to touched env/gate");
    }

    if (fails)
        printf("FAIL %d\n", fails);
    else
        printf("PASS t22_303slide\n");
    return fails != 0;
}
