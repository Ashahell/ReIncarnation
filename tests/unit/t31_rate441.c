/* t31_rate441 — M2.5, TC-2.13.4 rate half (44.1 kHz native render):
 *
 * The engine DSP already takes float sr everywhere; the 48 kHz lock
 * lived in the render tool (RI_SR) plus one 1 s tail constant. This
 * pin covers the 44.1 kHz side that `--rate 44100` opens:
 *
 *   - Golden structural: tests/golden/303/dc-441.wav parses
 *     field-exact (sr 44100, byterate 88200, sizes). RED while the
 *     golden is missing (feature-absent manifest).
 *   - Song golden structural: tests/golden/songs/first-light-441.wav
 *     parses field-exact at 44100 Hz (pins the tail/map/voices path
 *     at the second rate, RED while missing).
 *   - Engine rate sanity at 44100 (properties, backend-agnostic):
 *     delay sync exact (120 BPM/0.75 beat = 16538 samples) with
 *     error < 0.1%; 303 + PCF DC gain == 1 (design invariant holds
 *     across rates); 808 BD renders finite with peak in (0, 1].
 *   - Clock at 44100 already pinned by t21_seq (10-min sim, exact
 *     115200 ticks both rates) — cited, not duplicated.
 *
 * CWD convention: repo root (golden paths, same as t1_fx/t28).
 * Analysis may use libm (tests/ only).
 */
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <math.h>
#include "engine/dsp/rb303.h"
#include "engine/dsp/rb808.h"
#include "engine/fx/pcf.h"
#include "engine/fx/fx.h"

static int fails = 0;
#define CHECK(cond, ...) do { \
    if (!(cond)) { printf("FAIL "); printf(__VA_ARGS__); printf("\n"); fails++; } \
} while (0)

static uint32_t rd32(const unsigned char *p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
        ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static long parse_wav_sr(const char *path, uint32_t want_sr,
    uint8_t want_depth) {
    unsigned char h[44];
    uint32_t data, bps, n;
    long fsize = -1;
    FILE *f = fopen(path, "rb");
    if (!f) {
        printf("FAIL missing %s\n", path);
        fails++;
        return -1;
    }
    if (fread(h, 1, 44, f) != 44) {
        printf("FAIL short header %s\n", path);
        fclose(f);
        fails++;
        return -1;
    }
    fseek(f, 0, SEEK_END);
    fsize = ftell(f);
    fclose(f);
    bps = want_depth / 8u;
    CHECK(memcmp(h, "RIFF", 4) == 0, "%s riff", path);
    CHECK(rd32(h + 24) == want_sr, "%s sr %u want %u", path,
        rd32(h + 24), want_sr);
    CHECK(rd32(h + 28) == want_sr * bps, "%s byterate", path);
    CHECK(h[34] == want_depth, "%s bits", path);
    data = rd32(h + 40);
    CHECK(rd32(h + 4) == 36u + data, "%s riff size", path);
    CHECK(fsize == (long)(44u + data), "%s filesize", path);
    n = data / bps;
    printf("%s: sr %u depth %u samples %u\n", path, want_sr, want_depth,
        n);
    return (long)n;
}

static float OUT[48000];

int main(void) {
    /* --- goldens structural at 44100 --- */
    parse_wav_sr("tests/golden/303/dc-441.wav", 44100u, 16u);
    parse_wav_sr("tests/golden/303/first-light-441.wav", 44100u, 16u);

    /* --- delay sync exact at 44100 --- */
    {
        struct RiFXDelay d;
        static float buf[96000];
        float want, err;
        uint32_t s;
        CHECK(ri_fxdelay_init(&d, buf, 96000u) == 0, "dl init");
        ri_fxdelay_set(&d, 0, 127);
        s = ri_fxdelay_sync(&d, 120.0f, 0.75f, 44100.0f);
        CHECK(s == 16538u, "44100 sync %u want 16538", s);
        want = 0.75f * 60.0f * 44100.0f / 120.0f;
        err = (float)fabs((double)s - (double)want) / want * 100.0f;
        CHECK(err < 0.1f, "44100 sync err %.5g%%", (double)err);
        printf("44100 delay: %u smp err %.5g%%\n", s, (double)err);
    }

    /* --- DC gain == 1 at 44100 (303 + PCF design invariant) --- */
    {
        struct RB303Voice v;
        struct PCF p;
        uint32_t i;
        float peak = 0.0f;
        rb303_init(&v);
        rb303_set_cutoff_hz(&v, 8000.0f);
        rb303_set_reso(&v, 0.0f);
        for (i = 0; i < 44100u; i++) {
            float y = rb303_filter_step(&v, 0.5f, 44100.0f);
            float a = y < 0.0f ? -y : y;
            if (a > peak)
                peak = a;
            CHECK(y == y, "303 non-finite at 44100");
            if (!(y == y))
                break;
        }
        CHECK(fabs((double)peak - 0.5) < 0.01, "303 dc peak %.5g",
            (double)peak);
        pcf_init(&p);
        for (i = 0; i < 44100u; i++)
            OUT[i] = 0.5f;
        pcf_render(&p, OUT, OUT, 44100u, 44100.0f);
        {
            double s = 0.0;
            for (i = 44100u - 4096u; i < 44100u; i++)
                s += (double)OUT[i] * OUT[i];
            s = sqrt(s / 4096.0);
            CHECK(fabs(s - 0.5) < 0.005, "pcf dc rms %.5g at 44100", s);
        }
        printf("dc unity holds at 44100 (303 peak %.4g)\n", (double)peak);
    }

    /* --- 808 BD renders finite + sane at 44100 --- */
    {
        struct RB808Set s;
        uint32_t i, n = 22050u; /* 0.5 s @44.1k */
        float peak = 0.0f;
        rb808_init_set(&s);
        rb808_trigger(&s, RB808_BD, 0, 0.0f);
        for (i = 0; i < n; i++) {
            float y = rb808_voice_render(&s.v[RB808_BD], 44100.0f);
            float a;
            CHECK(y == y, "808 non-finite at 44100");
            if (!(y == y))
                break;
            a = y < 0.0f ? -y : y;
            if (a > peak)
                peak = a;
        }
        CHECK(peak > 0.05f && peak <= 1.0f, "808 bd peak %.5g", (double)peak);
    }

    if (fails)
        printf("FAIL %d\n", fails);
    else
        printf("PASS t31_rate441\n");
    return fails != 0;
}
