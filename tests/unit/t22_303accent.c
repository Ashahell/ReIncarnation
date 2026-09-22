/* t22_303accent — TC-2.2.2 accent envelope (WBS 2.2 property pin).
 * Trigger with accent: accent_env peaks exactly 1.0, then decays with
 * tau 60 ms (P-02) — assert measured 1/e time within +-10% (0.054..0.066
 * s @48 kHz = 2592..3168 samples) by sample-stepped render, plus
 * monotone nonincreasing decay (no reset bumps). White-box on struct
 * fields; output-agnostic (VCA math is covered by voice tests).
 */
#include <stdio.h>
#include <stdint.h>
#include <math.h>
#include "engine/dsp/rb303.h"

#define T22A_SR 48000.0f
#define T22A_TAU_LO 2592u
#define T22A_TAU_HI 3168u

static int fails = 0;
#define CHECK(cond, ...) do { \
    if (!(cond)) { printf("FAIL "); printf(__VA_ARGS__); printf("\n"); fails++; } \
} while (0)

int main(void) {
    struct RB303Voice v;
    float s[1u];
    uint32_t n;
    float prev;
    int reached = 0;
    rb303_init(&v);
    rb303_set_param(&v, RI_CTL_303A_CUTOFF, 80);
    rb303_set_param(&v, RI_CTL_303A_RESO, 40);
    rb303_set_param(&v, RI_CTL_303A_ENVMOD, 64);
    rb303_set_param(&v, RI_CTL_303A_DECAY, 64);
    rb303_set_param(&v, RI_CTL_303A_ACCENT, 96);
    rb303_set_param(&v, RI_CTL_303A_WAVE, 0);
    rb303_set_param(&v, RI_CTL_303A_VOLUME, 127);
    rb303_note(&v, 45, 0, 1);
    CHECK(v.accent_env == 1.0f, "accent peak %f want 1.0",
          (double)v.accent_env);
    prev = v.accent_env;
    for (n = 0u; n < 4800u; n++) {
        rb303_render(&v, s, 1u, T22A_SR);
        if (v.accent_env > prev) {
            printf("FAIL accent reset at %u (%f -> %f)\n", n,
                   (double)prev, (double)v.accent_env);
            fails++;
            break;
        }
        prev = v.accent_env;
        if (v.accent_env <= 0.3678794412f) {
            if (n + 1u < T22A_TAU_LO || n + 1u > T22A_TAU_HI) {
                printf("FAIL tau %u samples, want 2592..3168\n", n + 1u);
                fails++;
            }
            reached = 1;
            break;
        }
    }
    CHECK(reached, "no 1/e crossing in 4800 samples");

    if (fails)
        printf("FAIL %d\n", fails);
    else
        printf("PASS t22_303accent\n");
    return fails != 0;
}
