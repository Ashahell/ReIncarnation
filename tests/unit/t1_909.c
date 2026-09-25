/* t1_909 — Task 9 (gate G9): 909 sampler tests (TC-2.4 equivalents).
 * Gates (brief order): tune sweep 0..127 step 1 on a synthetic 2-layer
 * sine fixture -> max adjacent RMS step <= 1 dB (P-13 continuity); acc1
 * x1.15 +/-0.2 dB, crash/ride accent-no-op; flam +35 ms +/-5 ms x0.75
 * (P-05/P-14); retrigger cuts previous (post-cut diff energy < -60 dB,
 * here bit-exact by full state reset); swap-while-active refused
 * (RI_909_BUSY asserted), idle swap click-free (first-sample edge
 * < -80 dBFS on start-at-zero layers, the pack recipe rule); hat steal
 * (CH kills OH); decay-shortens-with-tune monotonic on CR; S909
 * render-diff (pack-style layers differ from default); validator good +
 * 4 mutants; determinism (D1) double-render identical.
 * Analysis may use libm (tests/ only; engine/ stays kernels-only).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "tests/helpers/ri_assert.h"
#include "engine/dsp/rb909.h"
#include "project/rbnm.h"

#define SR 48000.0f
#define SRU 48000u

/* Fixture layers: 1 s sines baked at trigger (start-at-zero pack rule:
 * sin(0) = 0, so idle-swap onset has no DC step). */
static float L0[SRU], L1[SRU], LD[SRU]; /* 440 / 880 / decaying-440 */
static float LD2[SRU]; /* decaying second layer for flam fixture */
static float OUTA[24000], OUTB[24000], OUTC[24000];

static float rms(const float *b, uint32_t n) {
    double s = 0.0;
    uint32_t i;
    for (i = 0; i < n; i++)
        s += (double)b[i] * b[i];
    return (float)sqrt(s / (double)n);
}

static void bake(void) {
    uint32_t i;
    for (i = 0; i < SRU; i++) {
        double t = (double)i / 48000.0;
        L0[i] = (float)(0.5 * sin(2.0 * 3.141592653589793 * 440.0 * t));
        L1[i] = (float)(0.5 * sin(2.0 * 3.141592653589793 * 880.0 * t));
        LD[i] = (float)(0.8 * sin(2.0 * 3.141592653589793 * 440.0 * t) *
            exp(-t / 0.20));
        LD2[i] = (float)(0.8 * sin(2.0 * 3.141592653589793 * 660.0 * t) *
            exp(-t / 0.008));
    }
}

static void render_voice(struct RB909Set *s, uint32_t v, uint32_t n, float *out) {
    uint32_t i;
    for (i = 0; i < n; i++)
        out[i] = rb909_voice_render(&s->v[v], SR);
}

static const struct RISampleLayer PAIR2[2] = {
    { L0, SRU, SRU, 0, 63, { 0, 0 } },
    { L1, SRU, SRU, 64, 127, { 0, 0 } }
};

int main(void) {
    struct RB909Set s;
    uint32_t i;
    bake();

    /* --- 0. split-model spot checks (P-13/P-14/P-05 plumbing) --- */
    RI_ASSERT(rb909_pitch_mult(64) == 1.0f, "clock 64 != 1");
    RI_ASSERT(fabs((double)rb909_pitch_mult(127) - pow(2.0, 63.0 / 48.0)) < 1e-5,
        "clock 127 off-model");
    RI_ASSERT(rb909_pitch_mult(0) < 1.0f && rb909_pitch_mult(127) > 1.0f,
        "clock not monotonic");
    RI_ASSERT(rb909_decay_scale(RB909_CR, 0) > rb909_decay_scale(RB909_CR, 64) &&
        rb909_decay_scale(RB909_CR, 64) > rb909_decay_scale(RB909_CR, 127),
        "cr decay not shortening with tune");
    RI_ASSERT(rb909_decay_scale(RB909_RD, 0) > rb909_decay_scale(RB909_RD, 64) &&
        rb909_decay_scale(RB909_RD, 64) > rb909_decay_scale(RB909_RD, 127),
        "rd decay not shortening with tune");
    RI_ASSERT(rb909_decay_scale(RB909_BD, 127) == 1.0f, "bd decay scaled");
    RI_ASSERT(rb909_layer_weight(64, 0, 63) >= 0.0f &&
        rb909_layer_weight(64, 0, 63) <= 1.0f, "weight range");

    rb909_init_set(&s);
    RI_ASSERT(rb909_set_layers(&s, RB909_BD, PAIR2, 2) == 0, "set bd");
    RI_ASSERT(rb909_set_layers(&s, RB909_SD, PAIR2, 2) == 0, "set sd");
    RI_ASSERT(rb909_set_layers(&s, RB909_CH, PAIR2, 2) == 0, "set ch");
    RI_ASSERT(rb909_set_layers(&s, RB909_OH, PAIR2, 2) == 0, "set oh");
    RI_ASSERT(rb909_set_layers(&s, RB909_CR, PAIR2, 2) == 0, "set cr");
    RI_ASSERT(rb909_set_layers(&s, RB909_RD, PAIR2, 2) == 0, "set rd");
    RI_ASSERT(rb909_set_layers(&s, 11, PAIR2, 2) == RI_909_BADARG, "bad voice");
    RI_ASSERT(rb909_set_layers(&s, RB909_BD, PAIR2, 0) == RI_909_BADARG, "bad n");
    RI_ASSERT(rb909_set_layers(&s, RB909_BD, 0, 2) == RI_909_BADARG, "null");

    /* --- 1. tune sweep RMS continuity (P-13): max adjacent step <= 1 dB --- */
    {
        float prev = 0.0f, worst = 0.0f;
        int t;
        for (t = 0; t <= 127; t++) {
            float r, step = 0.0f;
            rb909_trigger(&s, RB909_BD, 0, (uint8_t)t, 0);
            render_voice(&s, RB909_BD, 14400, OUTA);
            r = rms(OUTA, 14400);
            RI_ASSERT(r > 0.05f, "tune %d rms %.6g too quiet", t, (double)r);
            if (t > 0) {
                step = (float)fabs(20.0 * log10((double)r / (double)prev));
                if (step > worst)
                    worst = step;
            }
            prev = r;
        }
        RI_ASSERT(worst <= 1.0f, "tune sweep worst adjacent step %.4g dB", (double)worst);
        printf("tune sweep worst adjacent step: %.4g dB\n", (double)worst);
    }

    /* --- 2. acc1 x1.15 +/-0.2 dB; CR/RD accent-no-op quirk --- */
    {
        float r0, r1, ratio;
        rb909_trigger(&s, RB909_BD, 0, 64, 0);
        render_voice(&s, RB909_BD, 14400, OUTA);
        r0 = rms(OUTA, 14400);
        rb909_trigger(&s, RB909_BD, 1, 64, 0);
        render_voice(&s, RB909_BD, 14400, OUTB);
        r1 = rms(OUTB, 14400);
        ratio = r1 / r0;
        RI_ASSERT(ratio >= 1.124f && ratio <= 1.178f,
            "acc1 ratio %.5g want 1.15 +/-0.2dB", (double)ratio);
        printf("acc1 ratio: %.5g\n", (double)ratio);
        /* crash quirk: accent is a no-op (bit-identical, shelf skipped) */
        rb909_trigger(&s, RB909_CR, 0, 64, 0);
        render_voice(&s, RB909_CR, 14400, OUTA);
        rb909_trigger(&s, RB909_CR, 1, 64, 0);
        render_voice(&s, RB909_CR, 14400, OUTB);
        {
            float md = 0.0f;
            for (i = 0; i < 14400; i++) {
                float d = (float)fabs((double)OUTA[i] - (double)OUTB[i]);
                if (d > md)
                    md = d;
            }
            RI_ASSERT(md == 0.0f, "cr accent not no-op (maxdiff %.6g)", (double)md);
        }
        /* ride quirk, direct (fix round 1: no CH/CR proxy for RD) */
        rb909_trigger(&s, RB909_RD, 0, 64, 0);
        render_voice(&s, RB909_RD, 14400, OUTA);
        rb909_trigger(&s, RB909_RD, 1, 64, 0);
        render_voice(&s, RB909_RD, 14400, OUTB);
        {
            float md = 0.0f;
            for (i = 0; i < 14400; i++) {
                float d = (float)fabs((double)OUTA[i] - (double)OUTB[i]);
                if (d > md)
                    md = d;
            }
            RI_ASSERT(md == 0.0f, "rd accent not no-op (maxdiff %.6g)", (double)md);
        }
    }

    /* --- 3. flam +35 ms +/-5 ms x0.75 (P-05) on decaying fixture --- */
    {
        static const struct RISampleLayer ONE = {
            LD2, SRU, SRU, 0, 127, { 0, 0 }
        };
        int onset2 = -1;
        float p1 = 0.0f, p2 = 0.0f;
        /* idle first (previous section left voices spent) */
        rb909_init_set(&s);
        RI_ASSERT(rb909_set_layers(&s, RB909_BD, &ONE, 1) == 0, "flam layers");
        rb909_trigger(&s, RB909_BD, 2, 64, 1680);
        render_voice(&s, RB909_BD, 24000, OUTA);
        for (i = 1000; i < 24000; i++) {
            if ((float)fabs((double)OUTA[i]) > 0.1f) {
                onset2 = (int)i;
                break;
            }
        }
        RI_ASSERT(onset2 > 0, "flam second onset not found");
        if (onset2 > 0)
            RI_ASSERT(abs(onset2 - 1680) <= 240, "flam onset %d want 1680+/-240",
                onset2);
        /* matched-age peaks: the 32-sample onset ramp suppresses the
         * first hit's earliest peak, so compare windows at the same hit
         * age (ramp-free, envelope alive): [64,900] per hit. */
        for (i = 64; i < 900; i++)
            if ((float)fabs((double)OUTA[i]) > p1)
                p1 = (float)fabs((double)OUTA[i]);
        for (i = 1680 + 64; i < 1680 + 900; i++)
            if ((float)fabs((double)OUTA[i]) > p2)
                p2 = (float)fabs((double)OUTA[i]);
        RI_ASSERT(p1 > 0.3f, "flam first peak %.5g too small", (double)p1);
        RI_ASSERT(fabs((double)(p2 / p1) - 0.75) <= 0.10, "flam ratio %.4g want 0.75",
            (double)(p2 / p1));
        printf("flam onset %d, ratio %.4g\n", onset2, (double)(p2 / p1));
        /* acc2 on non-flam voice (CH) == acc1 */
        RI_ASSERT(rb909_set_layers(&s, RB909_CH, &ONE, 1) == 0, "ch flam lay");
        rb909_trigger(&s, RB909_CH, 1, 64, 0);
        render_voice(&s, RB909_CH, 14400, OUTB);
        {
            float ra = rms(OUTB, 14400);
            rb909_trigger(&s, RB909_CH, 2, 64, 1680);
            render_voice(&s, RB909_CH, 14400, OUTC);
            {
                float rb = rms(OUTC, 14400), rr = rb / ra;
                RI_ASSERT(fabs((double)rr - 1.0) <= 0.02, "ch acc2 != acc1 (%.5g)",
                    (double)rr);
            }
        }
    }

    /* --- 4. monophonic retrigger cuts previous (post-cut bit-exact) --- */
    {
        static const struct RISampleLayer DL[1] = {
            { LD, SRU, SRU, 0, 127, { 0, 0 } }
        };
        float md = 0.0f;
        rb909_init_set(&s);
        RI_ASSERT(rb909_set_layers(&s, RB909_BD, DL, 1) == 0, "retrig layers");
        rb909_trigger(&s, RB909_BD, 0, 64, 0);
        render_voice(&s, RB909_BD, 14400, OUTA);
        rb909_trigger(&s, RB909_BD, 0, 64, 0); /* retrigger cuts */
        render_voice(&s, RB909_BD, 14400, OUTB);
        rb909_trigger(&s, RB909_BD, 0, 64, 0); /* fresh reference */
        render_voice(&s, RB909_BD, 14400, OUTC);
        for (i = 0; i < 14400; i++) {
            float d = (float)fabs((double)OUTB[i] - (double)OUTC[i]);
            if (d > md)
                md = d;
        }
        RI_ASSERT(md == 0.0f, "retrigger tail leaks (maxdiff %.6g)", (double)md);
    }

    /* --- 5. idle-only swap: BUSY while active, click-free when idle --- */
    {
        static const struct RISampleLayer DL[1] = {
            { LD, SRU, SRU, 0, 127, { 0, 0 } }
        };
        rb909_init_set(&s);
        RI_ASSERT(rb909_set_layers(&s, RB909_BD, DL, 1) == 0, "swap layers");
        rb909_trigger(&s, RB909_BD, 0, 64, 0);
        RI_ASSERT(rb909_set_layers(&s, RB909_BD, DL, 1) == RI_909_BUSY,
            "swap-while-active not refused");
        render_voice(&s, RB909_BD, 24000, OUTA);
        /* voice still active (LD rings past 0.5 s at tune 64): still BUSY */
        RI_ASSERT(s.v[RB909_BD].active != 0, "voice went idle too early");
        RI_ASSERT(rb909_set_layers(&s, RB909_BD, DL, 1) == RI_909_BUSY,
            "swap-while-active(2) not refused");
        /* drain to idle, then swap must succeed and start click-free */
        while (s.v[RB909_BD].active)
            render_voice(&s, RB909_BD, 24000, OUTA);
        RI_ASSERT(rb909_set_layers(&s, RB909_BD, DL, 1) == 0, "idle swap refused");
        rb909_trigger(&s, RB909_BD, 0, 64, 0);
        render_voice(&s, RB909_BD, 64, OUTA);
        RI_ASSERT((float)fabs((double)OUTA[0]) <= 1e-4f,
            "idle-swap edge %.6g > -80 dBFS", (double)OUTA[0]);
    }

    /* --- 6. shared-hat-ROM steal rule (CH <-> OH) --- */
    {
        rb909_init_set(&s);
        RI_ASSERT(rb909_set_layers(&s, RB909_CH, PAIR2, 2) == 0, "steal ch");
        RI_ASSERT(rb909_set_layers(&s, RB909_OH, PAIR2, 2) == 0, "steal oh");
        rb909_trigger(&s, RB909_OH, 0, 64, 0);
        RI_ASSERT(s.v[RB909_OH].active != 0, "oh not active");
        rb909_trigger(&s, RB909_CH, 0, 64, 0);
        RI_ASSERT(s.v[RB909_OH].active == 0, "oh not stolen by ch");
        RI_ASSERT(s.v[RB909_CH].active != 0, "ch not active");
        rb909_trigger(&s, RB909_OH, 0, 64, 0);
        RI_ASSERT(s.v[RB909_CH].active == 0, "ch not stolen by oh");
    }

    /* --- 7. S909 render-diff: pack-style layers differ from default --- */
    {
        static float PK[SRU];
        static const struct RISampleLayer DEF[1] = {
            { L0, SRU, SRU, 0, 127, { 0, 0 } }
        };
        static const struct RISampleLayer PCK[1] = {
            { PK, SRU, SRU, 0, 127, { 0, 0 } }
        };
        uint32_t ndiff = 0;
        for (i = 0; i < SRU; i++) {
            double t = (double)i / 48000.0;
            PK[i] = (float)(0.5 * sin(2.0 * 3.141592653589793 * 660.0 * t));
        }
        rb909_init_set(&s);
        RI_ASSERT(rb909_set_layers(&s, RB909_BD, DEF, 1) == 0, "diff def");
        rb909_trigger(&s, RB909_BD, 0, 64, 0);
        render_voice(&s, RB909_BD, 14400, OUTA);
        RI_ASSERT(rb909_set_layers(&s, RB909_SD, PCK, 1) == 0, "diff pck");
        rb909_trigger(&s, RB909_SD, 0, 64, 0);
        render_voice(&s, RB909_SD, 14400, OUTB);
        for (i = 100; i < 14400; i++)
            if (OUTA[i] != OUTB[i])
                ndiff++;
        RI_ASSERT(ndiff * 100u / 14300u > 90u, "pack render too close to default");
    }

    /* --- 8. validator: good file + 4 mutants --- */
    {
        static int16_t pcm[256];
        static const struct RBNMWriteLayer WL[2] = {
            { "T-KICK", 0, 48000u, 256u, 0, 63, pcm },
            { "T-SNAR", 1, 48000u, 256u, 64, 127, pcm }
        };
        static const char MANF[] =
            "T-KICK src=synth;date=2026-09-20;equip=host-gcc;lic=CC0;holder=RI;"
            "proc=sine440;fmt=48000/16;norm=-3dBFS;loop=none;map=bd:0-63\n"
            "T-SNAR src=synth;date=2026-09-20;equip=host-gcc;lic=CC0;holder=RI;"
            "proc=sine880;fmt=48000/16;norm=-3dBFS;loop=none;map=sd:64-127\n";
        static const char MANF_GAP[] =
            "T-KICK src=synth;date=2026-09-20;equip=host-gcc;lic=CC0;holder=RI;"
            "proc=sine440;fmt=48000/16;norm=-3dBFS;loop=none\n"
            "T-SNAR src=synth;date=2026-09-20;equip=host-gcc;lic=CC0;holder=RI;"
            "proc=sine880;fmt=48000/16;norm=-3dBFS;loop=none;map=sd:64-127\n";
        char err[192];
        uint32_t i;
        for (i = 0; i < 256; i++)
            pcm[i] = (int16_t)(i * 100);
        RI_ASSERT(rbnm_write_pack("/tmp/ri/run/t9_good.rbnm", WL, 2, MANF) == 0,
            "write good pack");
        RI_ASSERT(rbnm_validate_file("/tmp/ri/run/t9_good.rbnm", err, sizeof err) == 0,
            "good pack rejected: %s", err);
        RI_ASSERT(rbnm_validate_manifest_text(MANF, err, sizeof err) == 0,
            "good manifest rejected: %s", err);
        /* mutant 1: manifest gap (missing map key) */
        RI_ASSERT(rbnm_validate_manifest_text(MANF_GAP, err, sizeof err) == 1,
            "gap manifest accepted");
        printf("gap reason: %s\n", err);
        /* mutant 2: truncated file (120-byte prefix rewrite) */
        RI_ASSERT(rbnm_write_pack("/tmp/ri/run/t9_trunc.rbnm", WL, 2, MANF) == 0,
            "write trunc pack");
        {
            static unsigned char buf[4096];
            FILE *g = fopen("/tmp/ri/run/t9_good.rbnm", "rb");
            FILE *h = 0;
            uint32_t got = 0;
            if (g) {
                got = (uint32_t)fread(buf, 1, 120, g);
                fclose(g);
            }
            RI_ASSERT(got == 120, "prefix read");
            h = fopen("/tmp/ri/run/t9_trunc.rbnm", "wb");
            RI_ASSERT(h != 0, "rewrite trunc");
            if (h) {
                fwrite(buf, 1, got, h);
                fclose(h);
            }
        }
        RI_ASSERT(rbnm_validate_file("/tmp/ri/run/t9_trunc.rbnm", err, sizeof err) == 1,
            "truncated pack accepted");
        printf("trunc reason: %s\n", err);
        /* mutant 3: SMPL frames lie (patch S909 frames of T-KICK +1) */
        {
            static unsigned char img[4096];
            FILE *g = fopen("/tmp/ri/run/t9_good.rbnm", "rb");
            uint32_t got = 0, k;
            int patched = 0;
            if (g) {
                got = (uint32_t)fread(img, 1, sizeof img, g);
                fclose(g);
            }
            for (k = 0; k + 6u < got; k++) {
                if (memcmp(img + k, "T-KICK", 6) == 0 && k >= 2u) {
                    /* S909 entry: ...id rate(4) frames(4) lo hi; SMPL has
                     * its own copy — patch the FIRST (S909) occurrence. */
                    img[k + 6u + 4u + 3u] ^= 0x01u;
                    patched = 1;
                    break;
                }
            }
            RI_ASSERT(patched, "mutant patch site found");
            g = fopen("/tmp/ri/run/t9_lie.rbnm", "wb");
            RI_ASSERT(g != 0, "write lie");
            if (g) {
                fwrite(img, 1, got, g);
                fclose(g);
            }
        }
        RI_ASSERT(rbnm_validate_file("/tmp/ri/run/t9_lie.rbnm", err, sizeof err) == 1,
            "frame-lie pack accepted");
        printf("lie reason: %s\n", err);
        /* mutant 4: gap pack (MANF missing a layer row) */
        {
            static char manf_short[1024];
            const char *nl = strchr(MANF, '\n');
            RI_ASSERT(nl != 0, "manf split");
            memcpy(manf_short, nl + 1, strlen(nl + 1) + 1u);
            RI_ASSERT(rbnm_write_pack("/tmp/ri/run/t9_short.rbnm", WL, 2,
                manf_short) == 0, "write short pack");
        }
        RI_ASSERT(rbnm_validate_file("/tmp/ri/run/t9_short.rbnm", err, sizeof err) == 1,
            "short-manifest pack accepted");
        printf("short reason: %s\n", err);
    }

    /* --- 9. determinism (D1): double-render identical --- */
    {
        uint32_t n = 14400;
        rb909_init_set(&s);
        RI_ASSERT(rb909_set_layers(&s, RB909_BD, PAIR2, 2) == 0, "det layers");
        rb909_trigger(&s, RB909_BD, 1, 90, 0);
        render_voice(&s, RB909_BD, n, OUTA);
        rb909_trigger(&s, RB909_BD, 1, 90, 0);
        render_voice(&s, RB909_BD, n, OUTB);
        for (i = 0; i < n; i++)
            RI_ASSERT(OUTA[i] == OUTB[i], "nondet at %u", i);
    }

    RI_RESULT("909");
}
