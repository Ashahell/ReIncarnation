/* t25_delay — Module 2.5, TC-2.5.3 (delay tempo sync):
 *
 *   - Sync error < 0.1% at 120/140/174 BPM (0.75 beats) with the
 *     impulse echo landing exactly on delay_smp (mirrors t1_fx §3).
 *   - No drift over 5 min (TC-literal, beyond t1): a beat-locked
 *     impulse train (period == delay_smp, fb = 0, full wet) renders
 *     14,400,000 samples (5 min @48 kHz); the output must equal the
 *     input delayed by exactly delay_smp, bit-exactly, end to end —
 *     any accumulated fractional-pointer drift would misalign the
 *     tail. Chunked (48 k/sample windows, previous-tail overlap) so
 *     the check is exact with small buffers.
 */
#include <stdio.h>
#include <stdint.h>
#include <math.h>
#include "engine/fx/fx.h"

#define T25_SR 48000.0f
#define T25_CHUNK 48000u
#define T25_FIVE_MIN (5u * 60u * 48000u) /* 14,400,000 */

static int fails = 0;
#define CHECK(cond, ...) do { \
    if (!(cond)) { printf("FAIL "); printf(__VA_ARGS__); printf("\n"); fails++; } \
} while (0)

static float DLBUF[96000];
static float IN[T25_CHUNK], OUT[T25_CHUNK], TAIL[24000];

int main(void) {
    static const float bpms[3] = { 120.0f, 140.0f, 174.0f };
    uint32_t i, k;
    struct RiFXDelay d;

    /* --- sync error < 0.1% + echo exactly on delay_smp --- */
    for (i = 0; i < 3; i++) {
        float want, err;
        uint32_t s, echo = 0;
        if (ri_fxdelay_init(&d, DLBUF, 96000u) != 0) {
            printf("FAIL dl init\n");
            fails++;
            continue;
        }
        ri_fxdelay_set(&d, 0, 127);
        s = ri_fxdelay_sync(&d, bpms[i], 0.75f, T25_SR);
        want = 0.75f * 60.0f * T25_SR / bpms[i];
        err = (float)fabs((double)s - (double)want) / want * 100.0f;
        CHECK(err < 0.1f, "bpm %.0f sync err %.5g%%",
            (double)bpms[i], (double)err);
        for (k = 0; k < 40000u; k++)
            IN[k] = (k == 0) ? 1.0f : 0.0f;
        ri_fxdelay_render(&d, IN, OUT, 40000u);
        for (k = 1; k < 40000u; k++) {
            if (OUT[k] > 0.5f || OUT[k] < -0.5f) {
                echo = k;
                break;
            }
        }
        CHECK(echo == s, "bpm %.0f echo %u want %u",
            (double)bpms[i], echo, s);
        printf("bpm %.0f: delay %u smp, sync err %.5g%%\n",
            (double)bpms[i], s, (double)err);
    }

    /* --- 5-minute no-drift: beat-locked impulse train, full wet,
     * fb = 0 → out[t] must equal in[t-s] bit-exactly for all t --- */
    {
        uint32_t s, n, bad = 0;
        if (ri_fxdelay_init(&d, DLBUF, 96000u) != 0) {
            printf("FAIL drift init\n");
            fails++;
        } else {
            ri_fxdelay_set(&d, 0, 127);
            s = ri_fxdelay_sync(&d, 120.0f, 0.75f, T25_SR);
            CHECK(s == 18000u, "120bpm/0.75 delay %u want 18000", s);
            for (k = 0; k < 24000u; k++)
                TAIL[k] = 0.0f;
            for (n = 0; n < T25_FIVE_MIN / T25_CHUNK; n++) {
                /* impulse every s samples, aligned to absolute time */
                for (k = 0; k < T25_CHUNK; k++) {
                    uint32_t t = n * T25_CHUNK + k;
                    IN[k] = (t % s == 0u) ? 1.0f : 0.0f;
                }
                ri_fxdelay_render(&d, IN, OUT, T25_CHUNK);
                /* samples [0,s): compare vs previous tail;
                 * samples [s, CHUNK): compare vs this input shifted */
                for (k = 0; k < s && bad < 8u; k++) {
                    float want = TAIL[k];
                    if (OUT[k] != want) {
                        printf("FAIL drift t=%u got %.6g want %.6g\n",
                            n * T25_CHUNK + k, (double)OUT[k], (double)want);
                        bad++;
                    }
                }
                for (k = s; k < T25_CHUNK && bad < 8u; k++) {
                    if (OUT[k] != IN[k - s]) {
                        printf("FAIL drift t=%u got %.6g want %.6g\n",
                            n * T25_CHUNK + k, (double)OUT[k],
                            (double)IN[k - s]);
                        bad++;
                    }
                }
                /* roll tail: last s INPUTS feed the next window start
                 * (fb = 0, so out[t] == in[t-s]: the reference for the
                 * next window's first s samples is this window's
                 * input tail, not its output) */
                for (k = 0; k < s; k++)
                    TAIL[k] = IN[T25_CHUNK - s + k];
                if (bad > 0)
                    break;
            }
            CHECK(bad == 0u, "5-min drift: %u misaligned samples", bad);
            if (bad == 0u)
                printf("5-min drift: 14400000 samples exact\n");
        }
    }

    if (fails)
        printf("FAIL %d\n", fails);
    else
        printf("PASS t25_delay\n");
    return fails != 0;
}
