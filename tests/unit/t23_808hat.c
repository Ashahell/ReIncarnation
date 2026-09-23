/* t23_808hat — Module 2.3, TC-2.3.1 (hat spectra): the six-oscillator metal
 * cluster must match the WBS ratio set (0.83, 1.48, 2.26, 2.92, 3.94, 5.31
 * within +-0.5%) by FFT of an isolated hit. At the contract cluster base
 * (1000 Hz) the hat partials sit at 830/1480/2260/2920/3940/5310 Hz.
 *
 * Method: exact mirror of t1_808 §2 — Goertzel magnitude at fc vs the
 * +-0.5% skirts (fc*0.995, fc*1.005), asserting m0 > mlo && m0 > mhi (local
 * prominence only; the 7 kHz one-pole HP attenuates 830 Hz heavily, so an
 * absolute floor is deliberately NOT used and the +-0.5% skirt comparison is
 * what the TC's "+-0.5%" bounds). Contract freqs are hardcoded WBS numbers,
 * never code macros.
 *
 * Pinned voices: CH and OH (both hats share the cluster path; CY is a cymbal
 * with its own LOW base 250 and is out of the hat contract's scope).
 *
 * RED today: the cluster sits at 400*{1.0,1.30,1.62,1.93,2.27,2.63} =
 * {400,520,648,772,908,1052} Hz — none within +-0.5% of the contract.
 *
 * Analysis may use libm (tests/ only).
 */
#include <stdio.h>
#include <stdint.h>
#include <math.h>
#include "engine/dsp/rb808.h"

#define T23_SR 48000.0f
#define T23_N 24000u /* 0.5 s @48k */

static int fails = 0;
#define CHECK(cond, ...) do { \
    if (!(cond)) { printf("FAIL "); printf(__VA_ARGS__); printf("\n"); fails++; } \
} while (0)

static float buf[T23_N];

static void render_voice(uint32_t v, uint32_t accent, float secs, float *out) {
    struct RB808Set s;
    uint32_t n = (uint32_t)(secs * T23_SR);
    uint32_t i;
    rb808_init_set(&s);
    rb808_trigger(&s, v, accent, 0.0f);
    for (i = 0; i < n; i++)
        out[i] = rb808_voice_render(&s.v[v], T23_SR);
}

/* Goertzel magnitude at f Hz over T23_N samples at 48 kHz. */
static double goertzel(const float *b, double f) {
    double w = 2.0 * 3.141592653589793 * f / 48000.0;
    double cw = cos(w), sw = sin(w);
    double u0 = 0.0, u1 = 0.0;
    uint32_t i;
    for (i = 0; i < T23_N; i++) {
        double u2 = (double)b[i] + 2.0 * cw * u1 - u0;
        u0 = u1;
        u1 = u2;
    }
    {
        double re = u1 * cw - u0;
        double im = u1 * sw;
        return sqrt(re * re + im * im);
    }
}

/* WBS ratio x 1000 Hz contract base: hardcoded, never code macros. */
static void audit_hat(uint32_t voice, const char *name) {
    static const double fc[6] = {
        830.0, 1480.0, 2260.0, 2920.0, 3940.0, 5310.0
    };
    uint32_t i;
    render_voice(voice, 0, 0.5f, buf);
    for (i = 0; i < 6; i++) {
        double m0 = goertzel(buf, fc[i]);
        double mlo = goertzel(buf, fc[i] * 0.995);
        double mhi = goertzel(buf, fc[i] * 1.005);
        CHECK(m0 > mlo && m0 > mhi,
            "%s partial %u (%.3f Hz): m0=%.6g not above -0.5%% %.6g / +0.5%% %.6g",
            name, i, fc[i], m0, mlo, mhi);
        printf("INFO %s partial %u (%.3f Hz): m0=%.6g mlo=%.6g mhi=%.6g\n",
            name, i, fc[i], m0, mlo, mhi);
    }
}

int main(void) {
    audit_hat(RB808_CH, "ch");
    audit_hat(RB808_OH, "oh");
    if (fails)
        printf("FAIL %d\n", fails);
    else
        printf("PASS t23_808hat\n");
    return fails != 0;
}