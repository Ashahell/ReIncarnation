/* t40_logslide.c — log-domain glide (§12.4c fix proof).
 * Slide slews log2(f) (RC on pitch CV): the octave rate is constant, so a
 * fifth up and a fifth down cover symmetric pitch ground in equal time.
 * At t=τ into a 3-octave slide the pitch sits 2^(3·(1−e^−1)) above/below
 * the start — NOT at the linear-Hz 63% point.
 * RED-first: linear-Hz slew reaches the 63% point instead.
 */
#include <stdio.h>
#include <stdint.h>
#include <math.h>
#include "tests/helpers/ri_assert.h"
#include "engine/dsp/rb303.h"

#define SR 48000.0f
/* 3 octaves from 55 Hz in τ=40 ms units: 55·2^(3(1−e^−1)) and mirror. */
#define UP_WANT 204.653f
#define DOWN_WANT 118.164f

static void render_n(struct RB303Voice *v, uint32_t n) {
    static float buf[2400];
    while (n > 0u) {
        uint32_t cc = n > 2400u ? 2400u : n;
        rb303_render(v, buf, cc, SR);
        n -= cc;
    }
}

int main(void) {
    struct RB303Voice v;
    /* up a fifth-and-octave (55 -> 440): log-domain point at t=τ. */
    rb303_init(&v);
    rb303_note(&v, 33, 0, 0); /* A1 55 Hz */
    render_n(&v, 4800u); /* settle */
    rb303_note(&v, 69, 1, 0); /* slide to A4 440 Hz */
    render_n(&v, 1920u); /* exactly τ = 40 ms */
    RI_ASSERT(fabsf(v.freq - UP_WANT) / UP_WANT <= 0.02f,
        "up-glide not log-domain: %f want %f", v.freq, (double)UP_WANT);
    /* down (440 -> 55): mirror point at t=τ. */
    rb303_init(&v);
    rb303_note(&v, 69, 0, 0);
    render_n(&v, 4800u);
    rb303_note(&v, 33, 1, 0);
    render_n(&v, 1920u);
    RI_ASSERT(fabsf(v.freq - DOWN_WANT) / DOWN_WANT <= 0.02f,
        "down-glide not log-domain: %f want %f", v.freq, (double)DOWN_WANT);
    /* τ preserved: reach within 7τ (3 octaves need it: e^-5 leaves 4.7%
     * against the small target; e^-7 leaves 0.6%). */
    render_n(&v, 6u * 1920u);
    RI_ASSERT(fabsf(v.freq - 55.0f) / 55.0f <= 0.01f, "no reach: %f", v.freq);
    RI_RESULT("logslide");
}
