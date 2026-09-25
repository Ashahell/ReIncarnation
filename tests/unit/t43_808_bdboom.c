/* t43_808_bdboom.c — BD 808-regime remodel (§12.5c fix proof).
 * The P-07 candidate (170 → 48 Hz, 22 ms) is 909-like; the 808 is a
 * ~50–60 Hz boom with a small fast sigh (E1 Werner/Abel/Smith: ~56 Hz
 * center; bridged-T band character; pulse ping + click; damping decay).
 * (a) Pitch law exact: 48 + 14·e^(−t/0.004), ±1% at 5 points (direct query).
 * (b) Early regime: ≤4 zero-crossings in the first 20 ms (≈60 Hz, not ≈150).
 * (c) Settle: late-window rate 40–60 Hz.
 * (d) Click present (peak > 0.05 in the first 10 ms).
 * (e) Tune maps f_start (62 Hz base now).
 * RED-first: 170 Hz start, 22 ms sigh.
 */
#include <stdio.h>
#include <stdint.h>
#include <math.h>
#include "tests/helpers/ri_assert.h"
#include "engine/dsp/rb808.h"

#define SR 48000.0f

int main(void) {
    static const float tp[5] = { 0.005f, 0.010f, 0.025f, 0.050f, 0.100f };
    struct RB808Set s;
    static float buf[48000u];
    uint32_t i, zc = 0;
    /* (a) pitch law. */
    for (i = 0; i < 5; i++) {
        float q = rb808_pitch_hz(RB808_BD, tp[i], 0.0f);
        double m = 48.0 + 14.0 * exp((double)-tp[i] / 0.004);
        RI_ASSERT(fabs((double)q - m) / m <= 0.01, "bd law t=%.3f q=%.4g m=%.4g",
            tp[i], (double)q, m);
    }
    /* (b) early regime + (d) click. */
    rb808_init_set(&s);
    rb808_trigger(&s, RB808_BD, 0u, 0.0f);
    for (i = 0; i < 48000u; i++)
        buf[i] = rb808_voice_render(&s.v[RB808_BD], SR);
    for (i = 384u; i < 1440u; i++)
        if (buf[i] * buf[i + 1] < 0.0f)
            zc++;
    RI_ASSERT(zc <= 4, "early BD too fast: %u crossings (want <= 4)", zc);
    {
        float peak = 0.0f;
        for (i = 0; i < 480u; i++) {
            float a = buf[i] < 0.0f ? -buf[i] : buf[i];
            if (a > peak)
                peak = a;
        }
        RI_ASSERT(peak > 0.05f, "BD click missing: %g", peak);
    }
    /* (c) settle rate 40–60 Hz over 200–400 ms. */
    {
        uint32_t z2 = 0;
        for (i = 9600u; i < 19200u; i++)
            if (buf[i] * buf[i + 1] < 0.0f)
                z2++;
        RI_ASSERT(z2 >= 16u && z2 <= 24u, "settle rate off: %u/200ms", z2);
    }
    /* (e) tune maps the 62 Hz start. */
    {
        float q = rb808_pitch_hz(RB808_BD, 0.0f, -7.0f);
        double m = 48.0 + (62.0 * pow(2.0, -7.0 / 12.0) - 48.0);
        RI_ASSERT(fabs((double)q - m) / m <= 0.01, "bd tune base %.4g vs %.4g",
            (double)q, m);
    }
    RI_RESULT("808_bdboom");
}
