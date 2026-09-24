/* t23_808accent — Module 2.3, TC-2.3.4 (accent +1.5x +-0.5 dB on every
 * accent-capable voice): all RI_808_NSOUNDS = 16 voices must show accent
 * (excite acc=1) raising the rendered RMS by 20*log10(1.5) = 3.52 dB within
 * +-0.5 dB. Mirror of t1_808 §4 (same render lengths, same helpers).
 * Green-pin expectation: t1_808 §4 already passes this on the current tree.
 *
 * Analysis may use libm (tests/ only).
 */
#include <stdio.h>
#include <stdint.h>
#include <math.h>
#include "engine/dsp/rb808.h"

#define T23_SR 48000.0f

static int fails = 0;
#define CHECK(cond, ...) do { \
    if (!(cond)) { printf("FAIL "); printf(__VA_ARGS__); printf("\n"); fails++; } \
} while (0)

/* Per-voice render seconds for accent/golden parity (t1_808 §4). */
static float voice_secs(uint32_t v) {
    if (v == RB808_CY)
        return 3.0f;
    if (v == RB808_OH)
        return 2.0f;
    if (v == RB808_BD || v == RB808_LT || v == RB808_MT || v == RB808_HT ||
        v == RB808_CB)
        return 1.5f;
    if (v == RB808_RS || v == RB808_CL || v == RB808_CH)
        return 0.5f;
    return 1.0f;
}

static float rms(const float *b, uint32_t n) {
    double s = 0.0;
    uint32_t i;
    for (i = 0; i < n; i++)
        s += (double)b[i] * b[i];
    return (float)sqrt(s / (double)n);
}

static float buf[144000]; /* 3 s @48k */
static float buf2[144000];

static void render_voice(uint32_t v, uint32_t accent, float secs, float *out) {
    struct RB808Set s;
    uint32_t n = (uint32_t)(secs * T23_SR);
    uint32_t i;
    rb808_init_set(&s);
    rb808_trigger(&s, v, accent, 0.0f);
    for (i = 0; i < n; i++)
        out[i] = rb808_voice_render(&s.v[v], T23_SR);
}

int main(void) {
    uint32_t v, i;
    for (v = 0; v < RI_808_NSOUNDS; v++) {
        uint32_t n = (uint32_t)(voice_secs(v) * T23_SR);
        float r0, r1, db;
        render_voice(v, 0, voice_secs(v), buf);
        render_voice(v, 1, voice_secs(v), buf2);
        for (i = 0; i < n; i++) {
            CHECK(buf[i] == buf[i] && buf2[i] == buf2[i],
                "voice %u non-finite at %u", v, i);
        }
        r0 = rms(buf, n);
        r1 = rms(buf2, n);
        CHECK(r0 > 1e-6f, "voice %u (%s) silent rms=%.6g", v, rb808_name(v), (double)r0);
        db = 20.0f * (float)log10((double)r1 / (double)r0);
        CHECK(fabs((double)db - 3.522) <= 0.5,
            "voice %u (%s) accent %+.3f dB (want +3.52+-0.5)", v, rb808_name(v), (double)db);
        printf("INFO voice %-2s accent %+.3f dB rms0=%.6g\n", rb808_name(v), (double)db, (double)r0);
    }
    if (fails)
        printf("FAIL %d\n", fails);
    else
        printf("PASS t23_808accent\n");
    return fails != 0;
}