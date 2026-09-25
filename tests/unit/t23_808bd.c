/* t23_808bd — Module 2.3, TC-2.3.1 (BD pitch trajectory) + TC-2.3.2 (BD
 * decay range across the Decay knob travel, 35 Hz clamp):
 *
 *   - TC-2.3.1: BD pitch f(t) = 48 + (170 - 48) e^(-t/0.022) within +-5%
 *     at 5 envelope points (mirrors t1_808 §1); tune maps f_start (P-07).
 *   - TC-2.3.2: the Decay knob (rib-id RI_CTL_808_DECAY, params.c 808
 *     section) must cover 0.18..2.8 s within +-10% across its travel:
 *     knob 0 -> tau_amp 0.18 s, knob 127 -> tau_amp 2.8 s (white-box on the
 *     struct field, t22 style), AND the rendered decay must show the same
 *     time constant: windowed-RMS (5 ms windows) 1/e crossing of the
 *     envelope measured from a 20 ms reference, within +-10% of each tau.
 *   - TC-2.3.2: no pitch query below the 35 Hz floor (clamp verified).
 *
 * RED status: rb808_set_param / RI_CTL_808_DECAY do not exist yet, so this
 * file fails to BUILD on the current tree (feature-absent manifest, same
 * shape as the t22 slices that pinned new params.c surfaces).
 *
 * Analysis may use libm (tests/ only).
 */
#include <stdio.h>
#include <stdint.h>
#include <math.h>
#include "engine/dsp/rb808.h"

#define T23_SR 48000.0f
#define T23_E 2.7182818284590452354

static int fails = 0;
#define CHECK(cond, ...) do { \
    if (!(cond)) { printf("FAIL "); printf(__VA_ARGS__); printf("\n"); fails++; } \
} while (0)

static float rms(const float *b, uint32_t n) {
    double s = 0.0;
    uint32_t i;
    for (i = 0; i < n; i++)
        s += (double)b[i] * b[i];
    return (float)sqrt(s / (double)n);
}

/* White-box: what tau_amp does the Decay knob select? */
static float bd_tau_after(uint8_t knob) {
    struct RB808Set s;
    rb808_init_set(&s);
    rb808_set_param(&s.v[RB808_BD], RI_CTL_808_DECAY, knob);
    return s.v[RB808_BD].tau_amp;
}

static float buf[168000]; /* 3.5 s @48k (need > 2.8 s for the long decay) */

/* Render the BD voice at a Decay knob position. */
static void render_bd(uint8_t knob, float secs) {
    struct RB808Set s;
    uint32_t n = (uint32_t)(secs * T23_SR);
    uint32_t i;
    rb808_init_set(&s);
    rb808_set_param(&s.v[RB808_BD], RI_CTL_808_DECAY, knob);
    rb808_trigger(&s, RB808_BD, 0, 0.0f);
    for (i = 0; i < n; i++)
        buf[i] = rb808_voice_render(&s.v[RB808_BD], T23_SR);
}

/* Windowed-RMS 1/e crossing: envelope rms(t) ~ exp(-t/tau). Measures the
 * window-center time where rms drops to rms_ref/e; returns that time in s
 * (or -1 if never reached within the render). Window must span >= 3 cycles
 * at the BD settle pitch (48 Hz -> period 20.8 ms), else a partial-cycle
 * window reads phase-dependent RMS; 64 ms (3072 samples) keeps the phase
 * average error under ~1.3%. */
static double measure_crossing(uint32_t t1s, uint32_t w, uint32_t n) {
    float r_ref = rms(buf + t1s, w);
    uint32_t t;
    for (t = t1s + 1; t + w <= n; t++) {
        if (rms(buf + t, w) <= r_ref / (float)T23_E)
            return (double)(t + w / 2u) / 48000.0;
    }
    return -1.0;
}

int main(void) {
    uint32_t i, w = 3072u /* 64 ms */, t1s = 960u /* 20 ms */;
    uint32_t n = (uint32_t)(3.5f * T23_SR);
    /* --- TC-2.3.1 BD trajectory +-5% at 5 envelope points (P-07 as re-based
     * §12.5c: 62 Hz start, 4 ms sigh) --- */
    {
        static const float tp[5] = { 0.010f, 0.025f, 0.050f, 0.100f, 0.200f };
        for (i = 0; i < 5; i++) {
            float q = rb808_pitch_hz(RB808_BD, tp[i], 0.0f);
            double m = 48.0 + (62.0 - 48.0) * exp((double)-tp[i] / 0.004);
            CHECK(fabs((double)q - m) / m <= 0.05,
                "bd traj t=%.3f q=%.4g model=%.4g", tp[i], (double)q, m);
        }
    }
    /* --- TC-2.3.2 Decay knob travel: 0.18 (knob 0) .. 2.8 s (knob 127),
     * each +-10% (white-box tau_amp + rendered 1/e crossing) --- */
    {
        static const uint8_t knobs[2] = { 0, 127 };
        static const float taus[2] = { 0.18f, 2.8f };
        uint32_t k;
        for (k = 0; k < 2; k++) {
            float tau = bd_tau_after(knobs[k]);
            double crossing, expect;
            CHECK(fabs((double)tau - (double)taus[k]) / (double)taus[k] <= 0.10,
                "bd knob %u tau_amp %.4g (want %.2f +-10%%)",
                knobs[k], (double)tau, (double)taus[k]);
            render_bd(knobs[k], 3.5f);
            crossing = measure_crossing(t1s, w, n);
            /* window centers: reference at 20 ms + 2.5 ms, crossing at its
             * center; e-fold between them == tau. */
            expect = ((double)t1s + (double)w / 2.0) / 48000.0 + (double)taus[k];
            CHECK(crossing > 0.0,
                "bd knob %u: no 1/e crossing in window (%.4g s)",
                knobs[k], crossing);
            CHECK(fabs(crossing - expect) / (double)taus[k] <= 0.10,
                "bd knob %u rendered 1/e at %.4g s (want %.4g +-10%%)",
                knobs[k], crossing, expect);
        }
    }
    /* --- TC-2.3.2 35 Hz clamp verified on BD, all tunes/times --- */
    {
        static const float tunes[3] = { -7.0f, 0.0f, 7.0f };
        static const float times[4] = { 0.0f, 0.05f, 0.5f, 2.0f };
        uint32_t ti, kj;
        for (ti = 0; ti < 3; ti++)
            for (kj = 0; kj < 4; kj++) {
                float q = rb808_pitch_hz(RB808_BD, times[kj], tunes[ti]);
                CHECK(q >= 35.0f, "bd floor %.4g < 35 (tune %.1f t %.3f)",
                    (double)q, (double)tunes[ti], (double)times[kj]);
            }
    }
    if (fails)
        printf("FAIL %d\n", fails);
    else
        printf("PASS t23_808bd\n");
    return fails != 0;
}