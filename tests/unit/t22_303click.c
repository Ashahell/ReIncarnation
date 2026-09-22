/* t22_303click — TC-2.2.5 waveform-switch click flag (WBS 2.2 feature).
 * Classic mode (flag on): hard switch, click present. Flag off: 0.5 ms
 * crossfade, smooth. Both voices play note 45 to phase ~0.75, flip wave,
 * render 64: assert classic max step EXCEEDS smooth max step (ordering —
 * phase-independent truth) and smooth max step < 0.1 (crossfade spreads
 * <=1.5 over 24 samples). Classic absolute bound set after
 * characterization (see article).
 */
#include <stdio.h>
#include <stdint.h>
#include <math.h>
#include "engine/dsp/rb303.h"

#define T22C_SR 48000.0f

static int fails = 0;
#define CHECK(cond, ...) do { \
    if (!(cond)) { printf("FAIL "); printf(__VA_ARGS__); printf("\n"); fails++; } \
} while (0)

static float maxstep(float *b, uint32_t n) {
    float m = 0.0f;
    uint32_t i;
    for (i = 1u; i < n; i++) {
        float d = (float)fabs((double)b[i] - (double)b[i - 1u]);
        if (d > m)
            m = d;
    }
    return m;
}

static float run_to_phase(struct RB303Voice *v, float lo, float hi) {
    float s[1u];
    uint32_t guard = 0u;
    for (;;) {
        rb303_render(v, s, 1u, T22C_SR);
        if (v->phase >= lo && v->phase < hi)
            break;
        if (++guard > 48000u)
            break;
    }
    return v->phase;
}

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
    rb303_render(v, s, 64u, T22C_SR);
    (void)s;
}

int main(void) {
    struct RB303Voice vc, vs;
    float bc[64u], bs[64u];
    float mc, ms;
    setup(&vc);
    setup(&vs);
    vc.classic_click = 1;
    vs.classic_click = 0;
    run_to_phase(&vc, 0.70f, 0.80f);
    run_to_phase(&vs, 0.70f, 0.80f);
    vc.wave_square = 1;
    vs.wave_square = 1;
    rb303_render(&vc, bc, 64u, T22C_SR);
    rb303_render(&vs, bs, 64u, T22C_SR);
    mc = maxstep(bc, 64u);
    ms = maxstep(bs, 64u);
    printf("INFO classic maxstep %.6g smooth maxstep %.6g\n",
           (double)mc, (double)ms);
    CHECK(ms < 0.1f, "smooth not smooth: %f", (double)ms);
    CHECK(mc > ms, "classic %.6g not above smooth %.6g", (double)mc,
          (double)ms);
    /* Absolute click presence, characterization-grounded 2026-09-22:
     * measured 0.0965 post-ladder (raw 1.0 jump smoothed); bound 0.05
     * keeps 2x downward margin while staying far above smooth 0.019. */
    CHECK(mc > 0.05f, "classic click missing: %f", (double)mc);

    if (fails)
        printf("FAIL %d\n", fails);
    else
        printf("PASS t22_303click\n");
    return fails != 0;
}
