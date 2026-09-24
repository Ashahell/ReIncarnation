/* t21_storm — TC-2.1.4 DSP-side storm budget (WBS 2.1).
 * All 4 sections at once into one 64-frame buffer @48 kHz: 303A + 303B
 * (distinct notes/accents/slides), all 16 808 voices (max decay,
 * accented), all 6 909 voices (baked sine layers, accented). Timed with
 * clock(); budget 0.5x buffer (666.7 us). Energy guard (peak > 0.01:
 * a silent storm is a broken storm) + determinism (re-init/re-trigger/
 * re-render bit-identical, D1 pattern). Host proxy for the AROS-box
 * budget (method mirrors t1_808's storm gate).
 */
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <sys/utsname.h>
#include "engine/dsp/rb303.h"
#include "engine/dsp/rb808.h"
#include "engine/dsp/rb909.h"

#define T21S_SR 48000.0f
#define T21S_N 64u
#define T21S_BUDGET (0.5 * (double)T21S_N / 48000.0)
/* Timing repeats: one 64-frame storm is sub-tick on coarse clocks
 * (AROS clock()), so the budget loop repeats it; ratio uses per-storm
 * cpu. Functional asserts (energy, determinism) use single renders. */
#define T21S_REPS 1000u

static float L0[48000u], L1[48000u];
static float mix1[64u], mix2[64u], mixSlide[64u];
static float sec303a[64u], sec303b[64u], sec808[64u], sec909[64u];

static float peak_of(const float *b) {
    float peak = 0.0f;
    uint32_t i;
    for (i = 0u; i < T21S_N; i++) {
        float a = b[i] < 0.0f ? -b[i] : b[i];
        if (a > peak)
            peak = a;
    }
    return peak;
}

static void bake(void) {
    uint32_t i;
    for (i = 0u; i < 48000u; i++) {
        double t = (double)i / 48000.0;
        L0[i] = (float)(0.5 * sin(2.0 * 3.141592653589793 * 440.0 * t));
        L1[i] = (float)(0.5 * sin(2.0 * 3.141592653589793 * 880.0 * t));
    }
}

static void storm(float *out, int slide) {
    struct RB303Voice v303a, v303b;
    struct RB808Set s808;
    struct RB909Set s909;
    struct RISampleLayer pair[2];
    uint32_t v, i;
    for (i = 0u; i < T21S_N; i++)
        out[i] = 0.0f;
    /* 303A + 303B (voice defaults mirror audio.c auf_voice_defaults). */
    rb303_init(&v303a);
    rb303_set_param(&v303a, RI_CTL_303A_CUTOFF, 80);
    rb303_set_param(&v303a, RI_CTL_303A_RESO, 40);
    rb303_set_param(&v303a, RI_CTL_303A_ENVMOD, 64);
    rb303_set_param(&v303a, RI_CTL_303A_DECAY, 64);
    rb303_set_param(&v303a, RI_CTL_303A_ACCENT, 96);
    rb303_set_param(&v303a, RI_CTL_303A_WAVE, 0);
    rb303_set_param(&v303a, RI_CTL_303A_VOLUME, 127);
    rb303_note(&v303a, 45, 0, 1);
    rb303_render(&v303a, sec303a, T21S_N, T21S_SR);
    rb303_init(&v303b);
    rb303_set_param(&v303b, RI_CTL_303A_CUTOFF, 80);
    rb303_set_param(&v303b, RI_CTL_303A_RESO, 40);
    rb303_set_param(&v303b, RI_CTL_303A_ENVMOD, 64);
    rb303_set_param(&v303b, RI_CTL_303A_DECAY, 64);
    rb303_set_param(&v303b, RI_CTL_303A_ACCENT, 96);
    rb303_set_param(&v303b, RI_CTL_303A_WAVE, 0);
    rb303_set_param(&v303b, RI_CTL_303A_VOLUME, 127);
    rb303_note(&v303b, 50, 0, 0);
    rb303_render(&v303b, sec303b, 32u, T21S_SR);
    if (slide) {
        rb303_slide_to(&v303b, 52);
    }
    rb303_render(&v303b, sec303b + 32u, T21S_N - 32u, T21S_SR);
    /* 808: all 16, max decay, accented. */
    rb808_init_set(&s808);
    rb808_max_decay(&s808);
    for (v = 0u; v < RI_808_NSOUNDS; v++)
        rb808_trigger(&s808, v, 1u, 0.0f);
    rb808_render_mix(&s808, sec808, T21S_N, T21S_SR);
    /* 909: all 6, baked layers, accented. */
    pair[0].data = L0; pair[0].frames = 48000u; pair[0].rate = 48000u;
    pair[0].lo = 0; pair[0].hi = 63;
    pair[1].data = L1; pair[1].frames = 48000u; pair[1].rate = 48000u;
    pair[1].lo = 64; pair[1].hi = 127;
    rb909_init_set(&s909);
    for (v = 0u; v < RI_909_NVOICES; v++) {
        if (rb909_set_layers(&s909, v, pair, 2) != 0)
            continue;
        rb909_trigger(&s909, v, 1u, 64, 0);
    }
    rb909_render_mix(&s909, sec909, T21S_N, T21S_SR);
    for (i = 0u; i < T21S_N; i++)
        out[i] = sec303a[i] + sec303b[i] + sec808[i] + sec909[i];
}

int main(void) {
    struct utsname un;
    clock_t c0, c1;
    double cpu, ratio, peak = 0.0;
    uint32_t i, r;
    int fails = 0;
    static float rep[64u];
    bake();
    c0 = clock();
    for (r = 0u; r < T21S_REPS; r++)
        storm(rep, 0);
    c1 = clock();
    cpu = (double)(c1 - c0) / (double)CLOCKS_PER_SEC / (double)T21S_REPS;
    ratio = cpu / T21S_BUDGET;
    uname(&un);
    printf("INFO storm machine=%s/%s cpu=%.6gs budget=%.6gs ratio=%.4f reps=%u\n",
           un.sysname, un.machine, cpu, T21S_BUDGET, ratio, T21S_REPS);
    if (!(ratio <= 1.0)) {
        printf("FAIL storm ratio %.4f > 1.0 (budget 0.5x buffer)\n", ratio);
        fails++;
    }
    /* Functional single renders (timing used rep above). */
    storm(mix1, 0);
    for (i = 0u; i < T21S_N; i++) {
        double a = fabs((double)mix1[i]);
        if (a > peak)
            peak = a;
    }
    if (!(peak > 0.01)) {
        printf("FAIL storm peak %.6g (silent storm)\n", peak);
        fails++;
    }
    /* Per-section energy: every section must actually render (a missing
     * family must fail here, not hide in the mix). */
    if (!(peak_of(sec303a) > 0.01)) {
        printf("FAIL 303A silent (peak %.6g)\n", peak_of(sec303a));
        fails++;
    }
    if (!(peak_of(sec303b) > 0.01)) {
        printf("FAIL 303B silent (peak %.6g)\n", peak_of(sec303b));
        fails++;
    }
    if (!(peak_of(sec808) > 0.01)) {
        printf("FAIL 808 silent (peak %.6g)\n", peak_of(sec808));
        fails++;
    }
    if (!(peak_of(sec909) > 0.01)) {
        printf("FAIL 909 silent (peak %.6g)\n", peak_of(sec909));
        fails++;
    }
    storm(mix2, 0);
    if (memcmp(mix1, mix2, sizeof mix1) != 0) {
        printf("FAIL storm not bit-identical\n");
        fails++;
    }
    /* Held-note slide (303B): gate open from the plain note above, slew
     * to 52 mid-buffer. Must render sound AND differ from plain. */
    storm(mixSlide, 1);
    if (!(peak_of(mixSlide) > 0.01)) {
        printf("FAIL slide storm silent (peak %.6g)\n", peak_of(mixSlide));
        fails++;
    }
    if (memcmp(mixSlide, mix1, sizeof mix1) == 0) {
        printf("FAIL slide inaudible (identical to plain)\n");
        fails++;
    }
    if (fails)
        printf("FAIL %d\n", fails);
    else
        printf("PASS t21_storm\n");
    return fails != 0;
}
