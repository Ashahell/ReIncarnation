/* t26_meter — Module 2.6, TC-2.6.3 (meters):
 *
 *   - Peak-hold decay 20 dB/s ±2 (mirrors t1_mixer §4 value: 1.0 +
 *     1 s silence → rate from -20·log10(peak)).
 *   - sr fallback (beyond t1): init with sr <= 0 behaves as 48000
 *     (documented deterministic fallback) — identical peak after
 *     the same feed.
 *   - Higher-peak adoption (beyond t1's hold/cold checks): after a
 *     decayed hold, a hotter feed replaces the peak exactly.
 *   - Ballistics feel is GUI-milestone subjective sign-off, NOT
 *     host-pinnable — stated, not waved (acceptance.md owns it).
 */
#include <stdio.h>
#include <stdint.h>
#include <math.h>
#include "engine/mixer/mixer.h"

#define T26_SR 48000.0f

static int fails = 0;
#define CHECK(cond, ...) do { \
    if (!(cond)) { printf("FAIL "); printf(__VA_ARGS__); printf("\n"); fails++; } \
} while (0)

static float LONG[48000], B0[512];

int main(void) {
    struct RiMeter t, u;
    uint32_t i;
    for (i = 0; i < 48000u; i++)
        LONG[i] = (i == 0) ? 1.0f : 0.0f;

    /* --- 20 dB/s ±2 --- */
    ri_meter_init(&t, T26_SR);
    ri_meter_feed(&t, LONG, 48000);
    {
        double rate = -20.0 * log10((double)ri_meter_peak(&t));
        CHECK(fabs(rate - 20.0) <= 2.0, "decay %.4g dB/s want 20±2", rate);
        printf("meter: 1 s decay to %.6g (rate %.4g dB/s)\n",
            (double)ri_meter_peak(&t), rate);
    }

    /* --- sr fallback: sr <= 0 behaves as 48000 --- */
    ri_meter_init(&u, 0.0f);
    ri_meter_feed(&u, LONG, 48000);
    CHECK(ri_meter_peak(&u) == ri_meter_peak(&t), "sr fallback diverges");
    ri_meter_init(&u, -22050.0f);
    ri_meter_feed(&u, LONG, 48000);
    CHECK(ri_meter_peak(&u) == ri_meter_peak(&t), "neg sr diverges");

    /* --- higher-peak adoption after decay --- */
    ri_meter_init(&t, T26_SR);
    for (i = 0; i < 512; i++)
        B0[i] = 0.5f;
    ri_meter_feed(&t, B0, 512);
    CHECK(fabs((double)ri_meter_peak(&t) - 0.5) < 1e-6, "hold %g",
        (double)ri_meter_peak(&t));
    for (i = 0; i < 512; i++)
        B0[i] = 0.8f;
    ri_meter_feed(&t, B0, 512);
    CHECK(ri_meter_peak(&t) == 0.8f, "hotter peak not adopted %g",
        (double)ri_meter_peak(&t));

    if (fails)
        printf("FAIL %d\n", fails);
    else
        printf("PASS t26_meter\n");
    return fails != 0;
}
