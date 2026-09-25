/* t1_808 — Task 8 (gate G8): sixteen-sound 808 tests (§12.5a/b updates).
 * Gates (brief order): BD trajectory ±5% at 5 envelope points; hat FFT
 * peaks at the E1 fixed partials ±0.5% (§2; was WBS ratios); clap envelope
 * shows 4 bursts; accent x1.5 ±0.5 dB on every accent-capable voice
 * (all 16, per ledger); 35 Hz floor (sweep never below); storm render-time
 * <= 0.3x buffer via clock(); determinism (D1) double-render identical.
 * Analysis may use libm (tests/ only; engine/ stays kernels-only).
 */
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <sys/utsname.h>
#include "tests/helpers/ri_assert.h"
#include "engine/dsp/rb808.h"

#define SR 48000.0f
#define SRU 48000u

/* Per-voice render seconds for accent/golden parity (storm uses its own). */
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

/* Goertzel magnitude at f Hz over n samples at SR. */
static double goertzel(const float *b, uint32_t n, double f) {
    double w = 2.0 * 3.141592653589793 * f / 48000.0;
    double cw = cos(w), sw = sin(w);
    double u0 = 0.0, u1 = 0.0;
    uint32_t i;
    for (i = 0; i < n; i++) {
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

static float buf[144000]; /* 3 s @48k */
static float buf2[144000];

static void render_voice(uint32_t v, uint32_t accent, float secs, float *out) {
    struct RB808Set s;
    uint32_t n = (uint32_t)(secs * SR);
    uint32_t i;
    rb808_init_set(&s);
    rb808_trigger(&s, v, accent, 0.0f);
    for (i = 0; i < n; i++)
        out[i] = rb808_voice_render(&s.v[v], SR);
}

int main(void) {
    uint32_t v, i;
    /* --- 1. BD trajectory ±5% at 5 envelope points (P-07 as re-based §12.5c:
     * 62 Hz start, 4 ms sigh, 48 Hz boom) --- */
    {
        static const float tp[5] = { 0.010f, 0.025f, 0.050f, 0.100f, 0.200f };
        for (i = 0; i < 5; i++) {
            float q = rb808_pitch_hz(RB808_BD, tp[i], 0.0f);
            double m = 48.0 + (62.0 - 48.0) * exp((double)-tp[i] / 0.004);
            RI_ASSERT(fabs((double)q - m) / m <= 0.05, "bd traj t=%.3f q=%.4g model=%.4g",
                tp[i], (double)q, m);
        }
        /* tune ends stay on-model too (±7 st maps f_start only) */
        {
            float qlo = rb808_pitch_hz(RB808_BD, 0.05f, -7.0f);
            double mlo = 48.0 + (62.0 * pow(2.0, -7.0 / 12.0) - 48.0) * exp(-0.05 / 0.004);
            RI_ASSERT(fabs((double)qlo - mlo) / mlo <= 0.05, "bd tune-7 q=%.4g m=%.4g",
                (double)qlo, mlo);
        }
        /* rendered BD decays and stays finite */
        {
            uint32_t n = 24000;
            float r0, r1;
            render_voice(RB808_BD, 0, 0.5f, buf);
            for (i = 0; i < n; i++)
                RI_ASSERT(buf[i] == buf[i] && buf[i] > -2.0f && buf[i] < 2.0f,
                    "bd non-finite/range at %u", i);
            r0 = rms(buf, 12000);
            r1 = rms(buf + 12000, 12000);
            RI_ASSERT(r0 > 0.01f, "bd first-half rms %.6g too quiet", (double)r0);
            RI_ASSERT(r1 < r0, "bd not decaying %.6g vs %.6g", (double)r1, (double)r0);
        }
    }
    /* --- 2. hat FFT peaks at the E1 fixed set (§12.5b; was WBS ratios) --- */
    {
        uint32_t n = 24000;
        render_voice(RB808_CH, 0, 0.5f, buf);
        for (i = 1; i < 6; i++) { /* index 0 below method floor on CH (§12.5b) */
            double fc = (double)RI_808_METAL_HZ[i];
            double m0 = goertzel(buf, n, fc);
            double mlo = goertzel(buf, n, fc * 0.995);
            double mhi = goertzel(buf, n, fc * 1.005);
            RI_ASSERT(m0 > mlo && m0 > mhi,
                "ch partial %u (%.3f Hz): peak %.6g not above -0.5%% %.6g / +0.5%% %.6g",
                i, fc, m0, mlo, mhi);
        }
    }
    /* --- 3. clap envelope shows 4 bursts (peak-count with prominence) --- */
    {
        uint32_t n = 24000, peaks = 0, i0 = 0;
        float env = 0.0f, mx = 0.0f;
        static float fol[24000];
        float last_acc = -1.0f;
        render_voice(RB808_CP, 0, 0.5f, buf);
        /* fast follower (tau ~1 ms): 2 ms bursts reach ~85% while the
         * 7 ms gaps fully decay; tail ripple valleys stay shallow so
         * relative prominence rejects them. */
        for (i = 0; i < n; i++) {
            float a = buf[i] < 0.0f ? -buf[i] : buf[i];
            env += 0.02f * (a - env);
            fol[i] = env;
            if (env > mx)
                mx = env;
        }
        for (i = 1; i + 1 < (uint32_t)(0.040f * SR); i++) {
            if (fol[i] > fol[i - 1] && fol[i] >= fol[i + 1] && fol[i] > 0.2f * mx) {
                /* valley since last accepted peak */
                float valley = fol[i], tgap;
                uint32_t j;
                for (j = i0; j < i; j++)
                    if (fol[j] < valley)
                        valley = fol[j];
                tgap = last_acc < 0.0f ? 99.0f : (float)i / SR - last_acc;
                if (tgap > 0.004f && fol[i] - valley > 0.3f * fol[i]) {
                    peaks++;
                    last_acc = (float)i / SR;
                }
                i0 = i;
            }
        }
        RI_ASSERT(peaks == 4, "clap bursts: %u peaks (want 4)", peaks);
    }
    /* --- 4. accent x1.5 ±0.5 dB on every voice --- */
    for (v = 0; v < RI_808_NSOUNDS; v++) {
        uint32_t n = (uint32_t)(voice_secs(v) * SR);
        float r0, r1, db;
        render_voice(v, 0, voice_secs(v), buf);
        render_voice(v, 1, voice_secs(v), buf2);
        for (i = 0; i < n; i++) {
            RI_ASSERT(buf[i] == buf[i] && buf2[i] == buf2[i], "voice %u non-finite at %u", v, i);
        }
        r0 = rms(buf, n);
        r1 = rms(buf2, n);
        RI_ASSERT(r0 > 1e-6f, "voice %u (%s) silent rms=%.6g", v, rb808_name(v), (double)r0);
        db = 20.0f * (float)log10((double)r1 / (double)r0);
        RI_ASSERT(fabs((double)db - 3.522) <= 0.5, "voice %u (%s) accent %+.3f dB (want +3.52±0.5)",
            v, rb808_name(v), (double)db);
        printf("INFO voice %-2s accent %+.3f dB rms0=%.6g\n", rb808_name(v), (double)db, (double)r0);
    }
    /* --- 5. 35 Hz floor: no pitch query below, all tunes/times --- */
    {
        static const float tunes[3] = { -7.0f, 0.0f, 7.0f };
        static const float times[4] = { 0.0f, 0.05f, 0.5f, 2.0f };
        uint32_t ti, kj;
        for (v = 0; v < RI_808_NSOUNDS; v++)
            for (ti = 0; ti < 3; ti++)
                for (kj = 0; kj < 4; kj++) {
                    float q = rb808_pitch_hz(v, times[kj], tunes[ti]);
                    RI_ASSERT(q >= 35.0f, "voice %u floor %.4g < 35", v, (double)q);
                }
    }
    /* --- 6. storm <= 0.3x buffer + determinism --- */
    {
        struct RB808Set s;
        struct utsname un;
        uint32_t n = 2u * SRU, blk = 64u, pos = 0;
        static float storm[96000];
        static float storm2[96000];
        clock_t c0, c1;
        double cpu, buf_dur = 2.0;
        rb808_init_set(&s);
        rb808_max_decay(&s);
        for (v = 0; v < RI_808_NSOUNDS; v++)
            rb808_trigger(&s, v, 1, 0.0f);
        RI_ASSERT(s.triggered == 0xFFFFu, "storm mask 0x%04x", s.triggered);
        c0 = clock();
        while (pos < n) {
            uint32_t cc = (n - pos > blk) ? blk : (n - pos);
            rb808_render_mix(&s, storm + pos, cc, SR);
            pos += cc;
        }
        c1 = clock();
        cpu = (double)(c1 - c0) / (double)CLOCKS_PER_SEC;
        uname(&un);
        printf("INFO storm machine=%s/%s cpu=%.4gs buffer=%.2fs ratio=%.4f flags=%s\n",
            un.sysname, un.machine, cpu, buf_dur, cpu / buf_dur, __VERSION__);
        RI_ASSERT(cpu / buf_dur <= 0.3, "storm ratio %.4f > 0.3", cpu / buf_dur);
        /* determinism: identical re-trigger re-renders bit-exact */
        rb808_init_set(&s);
        rb808_max_decay(&s);
        for (v = 0; v < RI_808_NSOUNDS; v++)
            rb808_trigger(&s, v, 1, 0.0f);
        pos = 0;
        while (pos < n) {
            uint32_t cc = (n - pos > blk) ? blk : (n - pos);
            rb808_render_mix(&s, storm2 + pos, cc, SR);
            pos += cc;
        }
        RI_ASSERT(memcmp(storm, storm2, sizeof storm) == 0, "storm not bit-identical");
    }
    RI_RESULT("808");
}
