/* t23_808hat — Module 2.3, TC-2.3.1 (hat spectra), §12.5b contract update:
 * the six metal oscillators sit at the E1 fixed set (Werner/Abel/Smith
 * ICMC 2014: 205.3, 304.4, 369.6, 522.7, 540, 800 Hz, shared by CY/OH/CH),
 * superseding the WBS ratio set (0.83..5.31 x base) the test originally
 * pinned. Method unchanged: Goertzel magnitude at fc vs the +-0.5% skirts,
 * prominence only (no absolute floor — the HPs attenuate the low partials
 * heavily). Contract freqs are hardcoded E1 numbers, never code macros.
 *
 * Pinned voices: CH, OH and CY (all three share the fixed set now; CY's
 * 5 kHz HP replaces its old LOW base-250 path).
 *
 * CH exception: 205.3 Hz sits ~30 dB into the 7 kHz HP stopband on a
 * 35 ms voice — present (shared cluster code) but below this method's
 * floor. Pinned via OH/CY instead (same five upper partials asserted
 * strictly on CH).
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

/* E1 fixed set: hardcoded, never code macros. CH skips index 0 (see header). */
static void audit_hat(uint32_t voice, const char *name, int skip_first) {
    static const double fc[6] = {
        205.3, 304.4, 369.6, 522.7, 540.0, 800.0
    };
    uint32_t i, first = skip_first ? 1u : 0u;
    render_voice(voice, 0, 0.5f, buf);
    for (i = first; i < 6; i++) {
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
    audit_hat(RB808_CH, "ch", 1);
    audit_hat(RB808_OH, "oh", 0);
    audit_hat(RB808_CY, "cy", 0);
    if (fails)
        printf("FAIL %d\n", fails);
    else
        printf("PASS t23_808hat\n");
    return fails != 0;
}