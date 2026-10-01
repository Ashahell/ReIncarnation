/* t132_levi_mod — Levi ENV 1-5 and full LFOs (fidelity plan P5a, owner
 * 2026-09-30; manual pp. 71-82, clean-room: own laws and wave set).
 * Laws: ENV 1 / ENV 2 amounts sweep the digital / analog cutoff (0 is
 * inert, bit for bit); ENV 3 is the VCA (Initial Level 127 makes it
 * inert; its release closes the voice); trigger sources (note, LFO cycle,
 * none) and level; the 11 LFO waves keep their shapes and bounds; rate
 * ranges, level, quantize, smooth, delay/fade, one-shot on/step, start
 * phase; trig sync poly/single/off (stagger); keys; UI pages and texts.
 */
#include <stdio.h>
#include <string.h>
#include "tests/helpers/ri_assert.h"
#include "engine/dsp/levi.h"
#include "engine/seq/autolane.h"
#include "gui/sectlevi.h"
#include "gui/panelgeo.h"
#include "gui/ctlreg.h"

#define SR 48000.0f
#define N 24000u

static struct RILeviSet A, B;
static float oa[N], ob[N];

static void render(struct RILeviSet *s, float *o, uint32_t n) {
    uint32_t i;
    for (i = 0u; i < n; i++)
        o[i] = levi_voice_render(&s->v[0], 0, SR);
}

static float peak(const float *o, uint32_t from, uint32_t n) {
    uint32_t i;
    float p = 0.0f;
    for (i = from; i < from + n; i++) {
        float a = o[i] < 0.0f ? -o[i] : o[i];
        if (a > p)
            p = a;
    }
    return p;
}

static float energy(const float *o, uint32_t from, uint32_t n) {
    uint32_t i;
    float e = 0.0f;
    for (i = from; i < from + n; i++)
        e += o[i] * o[i];
    return e;
}

static float bright_of(const float *o, uint32_t from, uint32_t n) {
    uint32_t i;
    float d = 0.0f;
    for (i = from + 1u; i < from + n; i++)
        d += (o[i] - o[i - 1u]) * (o[i] - o[i - 1u]);
    return d / (energy(o, from, n) + 1e-20f);
}

/* A bright DUO voice through a closed digital LP. */
static void base(struct RILeviSet *s) {
    levi_init_set(s);
    levi_set_op_ui(s, 0u, 1u, RI_LEVI_OP_ENVL, 110u);
    levi_set_param(s, 0u, RI_LEVI_CUTOFF, 200.0f);
    levi_set_param(s, 0u, RI_LEVI_RESO, 0.0f);
    levi_set_param(s, 0u, RI_LEVI_CUTOFF2, 18000.0f);
    levi_set_param(s, 0u, RI_LEVI_RESO2, 0.0f);
}

/* LFO 0 of voice 0 stepped n times into o. */
static void lfo_run(struct RILeviSet *s, float *o, uint32_t n) {
    uint32_t i;
    for (i = 0u; i < n; i++)
        o[i] = ri_levi_lfo_step(&s->v[0].lfo[0], SR);
}

static void lfo_set(struct RILeviSet *s, uint32_t p, uint8_t v) {
    levi_set_lfo_ui(s, 0u, 0u, p, v);
}

int main(void) {
    struct RISectLevi u;
    uint32_t k, w, n;
    char t[32];
    uint16_t key;
    int val;

    /* ---- ENV 1 sweeps the digital cutoff; amount 0 is inert. ---- */
    base(&A);
    base(&B);
    levi_set_param_ui(&B, 0u, RI_CTL_LEVI_DENV1 & 0xFFu, 64u);
    levi_trigger(&A, 0u, 48u);
    levi_trigger(&B, 0u, 48u);
    render(&A, oa, N);
    render(&B, ob, N);
    RI_ASSERT(!memcmp(oa, ob, sizeof oa), "ENV1 amount 0 inert");       /* ob: the closed filter */
    base(&A);
    levi_set_menv_ui(&A, 0u, 0u, RI_LEVI_OP_ATTACK, 40u);          /* ~0.35 s */
    levi_set_param_ui(&A, 0u, RI_CTL_LEVI_DENV1 & 0xFFu, 127u);
    levi_trigger(&A, 0u, 48u);
    render(&A, oa, N);
    RI_ASSERT(bright_of(oa, N - 4800u, 4800u) > 2.0f * bright_of(ob, N - 4800u, 4800u), "ENV1 opens the filter %g > %g",
        (double)bright_of(oa, N - 4800u, 4800u), (double)bright_of(ob, N - 4800u, 4800u));
    /* ---- ENV 2 sweeps the analog cutoff. ---- */
    base(&A);
    levi_set_param(&A, 0u, RI_LEVI_CUTOFF, 18000.0f);
    levi_set_param(&A, 0u, RI_LEVI_CUTOFF2, 200.0f);
    levi_set_param_ui(&A, 0u, RI_CTL_LEVI_AKEYTRK & 0xFFu, 64u);
    levi_set_menv_ui(&A, 0u, 1u, RI_LEVI_OP_ATTACK, 40u);
    levi_set_param_ui(&A, 0u, RI_CTL_LEVI_AENV2 & 0xFFu, 127u);
    levi_trigger(&A, 0u, 48u);
    render(&A, oa, N);
    base(&B);
    levi_set_param(&B, 0u, RI_LEVI_CUTOFF, 18000.0f);
    levi_set_param(&B, 0u, RI_LEVI_CUTOFF2, 200.0f);
    levi_set_param_ui(&B, 0u, RI_CTL_LEVI_AKEYTRK & 0xFFu, 64u);
    levi_trigger(&B, 0u, 48u);
    render(&B, ob, N);
    RI_ASSERT(bright_of(oa, N - 4800u, 4800u) > 2.0f * bright_of(ob, N - 4800u, 4800u), "ENV2 opens the analog filter");

    /* ---- ENV 3 is the VCA. ---- */
    base(&A);
    levi_set_menv_ui(&A, 0u, 2u, RI_LEVI_OP_ATTACK, 70u);        /* slow VCA attack */
    levi_trigger(&A, 0u, 48u);
    render(&A, oa, N);
    RI_ASSERT(peak(oa, 0u, 2400u) < 0.25f * peak(oa, N - 2400u, 2400u), "ENV3 attack shapes the level");
    base(&A);
    base(&B);
    levi_set_menv_ui(&A, 0u, 2u, RI_LEVI_OP_ATTACK, 70u);
    levi_set_param_ui(&A, 0u, RI_CTL_LEVI_VINIT & 0xFFu, 127u);
    levi_set_param_ui(&B, 0u, RI_CTL_LEVI_VINIT & 0xFFu, 127u);
    levi_trigger(&A, 0u, 48u);
    levi_trigger(&B, 0u, 48u);
    render(&A, oa, N);
    render(&B, ob, N);
    RI_ASSERT(!memcmp(oa, ob, sizeof oa), "Initial Level 127 holds the VCA open");
    base(&A);
    for (k = 0u; k < RI_LEVI_NOPS; k++)
        levi_set_op_ui(&A, 0u, k, RI_LEVI_OP_RELEASE, 127u);      /* 60 s oscillator tails */
    levi_trigger(&A, 0u, 48u);
    render(&A, oa, 2400u);
    levi_release(&A, 0u);
    render(&A, oa, N);
    RI_ASSERT(!A.v[0].active && peak(oa, N - 100u, 100u) == 0.0f, "ENV3 release closes the voice");

    /* ---- Trigger sources and level. ---- */
    levi_init_set(&A);
    levi_set_menv_ui(&A, 0u, 3u, RI_LEVI_ME_TRIG1, RI_LEVI_TS_OFF);
    levi_trigger(&A, 0u, 60u);
    render(&A, oa, 4800u);
    RI_ASSERT(levi_menv_value(&A.v[0], 3u) == 0.0f && levi_menv_value(&A.v[0], 4u) > 0.5f, "no source, no start");
    levi_set_menv_ui(&A, 0u, 4u, RI_LEVI_ME_LEVEL, 64u);
    render(&A, oa, 4800u);
    RI_ASSERT(levi_menv_value(&A.v[0], 4u) == A.v[0].menv[4].value * (64.0f / 127.0f), "level 64 scales: %f",
        (double)levi_menv_value(&A.v[0], 4u));
    levi_init_set(&A);
    levi_set_menv_ui(&A, 0u, 3u, RI_LEVI_ME_TRIG1, RI_LEVI_TS_LFO1);
    levi_set_menv_ui(&A, 0u, 3u, RI_LEVI_OP_ATTACK, 30u);
    lfo_set(&A, RI_LEVI_LP_SPEED, 1u);
    lfo_set(&A, RI_LEVI_LP_RATE, 0u);                               /* 5 Hz */
    levi_trigger(&A, 0u, 60u);
    for (k = 0u, n = 0u, w = RI_LEVI_SEG_IDLE; k < N; k++) {
        (void)levi_voice_render(&A.v[0], 0, SR);
        if (A.v[0].menv[3].stage == RI_LEVI_SEG_A && w != RI_LEVI_SEG_A)   /* delay 0: straight to attack */
            n++;
        w = A.v[0].menv[3].stage;
    }
    RI_ASSERT(n >= 2u && n <= 3u, "LFO 1 retriggers ENV 4 each cycle: %u starts in 0.5 s", n);

    /* ---- LFO waves: bounds and shapes. ---- */
    for (w = 0u; w < RI_LEVI_NLW; w++) {
        levi_init_set(&A);
        lfo_set(&A, RI_LEVI_LP_WAVE, (uint8_t)w);
        lfo_set(&A, RI_LEVI_LP_SPEED, 1u);
        lfo_set(&A, RI_LEVI_LP_RATE, 0u);                           /* 5 Hz: 9600 samples a cycle */
        levi_trigger(&A, 0u, 60u);
        lfo_run(&A, oa, 9600u);
        for (k = 0u; k < 9600u; k++)
            if (!(oa[k] >= -1.0f && oa[k] <= 1.0f))
                break;
        RI_ASSERT(k == 9600u, "%s bounded", ri_levi_lfo_wave_name(w));
    }
    levi_init_set(&A);
    lfo_set(&A, RI_LEVI_LP_WAVE, RI_LEVI_LW_SAWUP);
    lfo_set(&A, RI_LEVI_LP_SPEED, 1u);
    lfo_set(&A, RI_LEVI_LP_RATE, 0u);
    levi_trigger(&A, 0u, 60u);
    lfo_run(&A, oa, 9000u);
    for (k = 1u; k < 9000u; k++)
        if (oa[k] < oa[k - 1u])
            break;
    RI_ASSERT(k == 9000u, "saw up rises through the cycle (%u)", k);
    lfo_set(&A, RI_LEVI_LP_WAVE, RI_LEVI_LW_PULSE27);
    levi_trigger(&A, 0u, 60u);
    lfo_run(&A, oa, 9600u);
    for (k = 0u, n = 0u; k < 9600u; k++)
        n += oa[k] > 0.0f;
    RI_ASSERT(n > 2500u && n < 2700u, "pulse 27 duty %u/9600", n);
    lfo_set(&A, RI_LEVI_LP_WAVE, RI_LEVI_LW_SH);
    levi_trigger(&A, 0u, 60u);
    lfo_run(&A, oa, 9000u);
    for (k = 1u; k < 9000u; k++)
        if (oa[k] != oa[0])
            break;
    RI_ASSERT(k == 9000u, "S&H holds within a cycle");
    lfo_set(&A, RI_LEVI_LP_WAVE, RI_LEVI_LW_NOISE);
    lfo_run(&A, oa, 100u);
    for (k = 1u, n = 0u; k < 100u; k++)
        n += oa[k] != oa[k - 1u];
    RI_ASSERT(n > 90u, "noise moves every sample");
    lfo_set(&A, RI_LEVI_LP_WAVE, RI_LEVI_LW_RANDOM);
    levi_trigger(&A, 0u, 60u);
    lfo_run(&A, oa, 19200u);
    for (k = 1u; k < 19200u; k++)
        if (oa[k] - oa[k - 1u] > 0.01f || oa[k - 1u] - oa[k] > 0.01f)
            break;
    RI_ASSERT(k == 19200u, "random glides");
    lfo_set(&A, RI_LEVI_LP_WAVE, RI_LEVI_LW_STEP);
    lfo_set(&A, RI_LEVI_LP_STEPS, 5u);
    levi_trigger(&A, 0u, 60u);
    lfo_run(&A, oa, 9600u);
    RI_ASSERT(oa[100] == -1.0f && oa[2400] == -0.5f && oa[9500] == 1.0f, "step 5: %f %f %f", (double)oa[100],
        (double)oa[2400], (double)oa[9500]);

    /* ---- Rates, level, quantize, smooth, delay/fade, one-shot, phase. ---- */
    RI_ASSERT(ri_levi_lfo_hz(1u, 127u) > 149.0f && ri_levi_lfo_hz(1u, 127u) < 151.0f &&
        ri_levi_lfo_hz(1u, 0u) == 5.0f && ri_levi_lfo_hz(0u, 127u) == 25.0f && ri_levi_lfo_hz(0u, 0u) == 0.0f,
        "rate ranges");
    levi_init_set(&A);
    lfo_set(&A, RI_LEVI_LP_WAVE, RI_LEVI_LW_SQUARE);
    lfo_set(&A, RI_LEVI_LP_LEVEL, 64u);
    levi_trigger(&A, 0u, 60u);
    lfo_run(&A, oa, 100u);
    RI_ASSERT(oa[50] > 0.50f && oa[50] < 0.51f, "level 64 = half: %f", (double)oa[50]);
    lfo_set(&A, RI_LEVI_LP_LEVEL, 127u);
    lfo_set(&A, RI_LEVI_LP_SMOOTH, 100u);
    levi_trigger(&A, 0u, 60u);
    lfo_run(&A, oa, 10u);
    RI_ASSERT(oa[0] > 0.0f && oa[0] < 0.5f && oa[9] > oa[0], "smooth slews the square: %f", (double)oa[0]);
    lfo_set(&A, RI_LEVI_LP_SMOOTH, 0u);
    lfo_set(&A, RI_LEVI_LP_WAVE, RI_LEVI_LW_SAWUP);
    lfo_set(&A, RI_LEVI_LP_QUANT, 2u);                              /* 3 levels */
    levi_trigger(&A, 0u, 60u);
    lfo_run(&A, oa, 48000u / 3u);
    for (k = 0u, n = 0u; k < 16000u; k++)
        n += oa[k] != -1.0f && oa[k] != 0.0f && oa[k] != 1.0f && oa[k] != -0.6666667f && oa[k] != 0.6666667f;
    RI_ASSERT(n == 0u, "quantize snaps (%u off-grid)", n);
    lfo_set(&A, RI_LEVI_LP_QUANT, 0u);
    lfo_set(&A, RI_LEVI_LP_DELAY, 20u);                             /* ~37 ms, then a ~0.4 s fade */
    lfo_set(&A, RI_LEVI_LP_FADE, 20u);
    levi_trigger(&A, 0u, 60u);
    lfo_run(&A, oa, N);
    RI_ASSERT(peak(oa, 0u, 100u) == 0.0f && peak(oa, N - 2400u, 2400u) > 0.0f, "delay silent, then fades in");
    levi_init_set(&A);
    lfo_set(&A, RI_LEVI_LP_SPEED, 1u);
    lfo_set(&A, RI_LEVI_LP_RATE, 0u);
    lfo_set(&A, RI_LEVI_LP_ONESHOT, 1u);
    levi_trigger(&A, 0u, 60u);
    lfo_run(&A, oa, N);
    RI_ASSERT(oa[N - 1u] == oa[N - 1000u] && oa[5000] != oa[5001], "one-shot runs one cycle, then holds");
    lfo_set(&A, RI_LEVI_LP_WAVE, RI_LEVI_LW_STEP);
    lfo_set(&A, RI_LEVI_LP_ONESHOT, 2u);
    lfo_set(&A, RI_LEVI_LP_STEPS, 4u);
    for (k = 0u; k < 4u; k++) {
        levi_trigger(&A, 0u, 60u);
        lfo_run(&A, oa + k, 1u);
    }
    RI_ASSERT(oa[1] > oa[0] && oa[2] > oa[1] && oa[3] > oa[2], "step one-shot advances per note");
    levi_init_set(&A);
    lfo_set(&A, RI_LEVI_LP_WAVE, RI_LEVI_LW_SAWUP);
    lfo_set(&A, RI_LEVI_LP_PHASE, 64u);                             /* 180 deg */
    levi_trigger(&A, 0u, 60u);
    lfo_run(&A, oa, 1u);
    RI_ASSERT(oa[0] > -0.01f && oa[0] < 0.01f, "start phase 180: %f", (double)oa[0]);

    /* ---- Trig sync: single shares, off staggers, poly restarts per voice. ---- */
    levi_init_set(&A);
    for (k = 0u; k < RI_LEVI_NVOICES; k++) {
        levi_set_lfo_ui(&A, k, 0u, RI_LEVI_LP_TRIG, 1u);
        levi_trigger(&A, k, (uint8_t)(48u + k));
    }
    levi_voice_render_sum(&A, oa, 1000u, SR);
    RI_ASSERT(A.v[0].lfo[0].value == A.v[5].lfo[0].value && A.v[0].lfo[0].value != 0.0f, "single: one LFO for all");
    levi_trigger(&A, 2u, 60u);                                      /* a new note restarts the shared LFO */
    RI_ASSERT(A.glfo[0].phase == 0.0f, "single: each note retriggers");
    for (k = 0u; k < RI_LEVI_NVOICES; k++) {
        levi_set_lfo_ui(&A, k, 0u, RI_LEVI_LP_TRIG, 2u);
        levi_set_lfo_ui(&A, k, 0u, RI_LEVI_LP_STAGGER, 32u);
    }
    levi_voice_render_sum(&A, oa, 10u, SR);
    RI_ASSERT(A.v[0].lfo[0].value != A.v[1].lfo[0].value, "off + stagger: voices offset");
    levi_init_set(&A);
    levi_trigger(&A, 0u, 48u);
    levi_voice_render_sum(&A, oa, 1000u, SR);
    levi_trigger(&A, 1u, 50u);
    levi_voice_render_sum(&A, oa, 1u, SR);
    RI_ASSERT(A.v[1].lfo[0].phase < A.v[0].lfo[0].phase, "poly: each note restarts its own LFO");

    /* ---- Keys. ---- */
    RI_ASSERT(ri_auto_allowed(RI_LEVI_MEKEY(4u, RI_LEVI_OP_RELEASE)) && ri_auto_allowed(RI_LEVI_MEKEY(0u, 0u)) &&
        !ri_auto_allowed(RI_LEVI_MEKEY(0u, RI_LEVI_OP_WAVE + 4u)) && !ri_auto_allowed(RI_LEVI_MEKEY(0u, 30u)) && ri_auto_allowed(RI_LEVI_LFOKEY(4u, 14u)) &&
        !ri_auto_allowed(RI_LEVI_LFOKEY(4u, 15u)) && !ri_auto_allowed(0x10F0u) &&
        ri_auto_allowed(RI_CTL_LEVI_DENV1) && ri_auto_allowed(RI_CTL_LEVI_VINIT), "P5 keys");

    /* ---- UI: ENV / LFO pages, keys, conditional slots, texts, knobs. ---- */
    ri_slevi_init(&u);
    ri_slevi_set_value(&u, RI_SLEVI_MODULE, (int)RI_SLEVI_M_ENV1 + 1);
    RI_ASSERT(ri_slevi_page_count(&u) == 4u && !strcmp(ri_slevi_page_title(&u), "ENV 2  1/4"), "env pages");
    RI_ASSERT(ri_slevi_ctl_key(&u, RI_SLEVI_ENC0, &key, &val) == 1 && key == RI_LEVI_MEKEY(1u, RI_LEVI_OP_ATTACK) &&
        val == ri_levi_menv_default(1u, RI_LEVI_OP_ATTACK), "ENV 2 attack key %04x", key);
    RI_ASSERT(ri_slevi_set_value(&u, RI_SLEVI_ENC0, 127) == 1 && u.mev[1][RI_LEVI_OP_ATTACK] == 127, "enc edits env");
    ri_slevi_enc_text(&u, 0u, t, sizeof t);
    RI_ASSERT(!strcmp(t, "36.00S"), "attack max text %s", t);
    RI_ASSERT(ri_slevi_reset(&u, RI_SLEVI_ENC0) == 1 && u.mev[1][RI_LEVI_OP_ATTACK] == 14, "reset to default");
    ri_slevi_press(&u, RI_SLEVI_PAGEDN);
    ri_slevi_press(&u, RI_SLEVI_PAGEDN);
    ri_slevi_press(&u, RI_SLEVI_PAGEDN);
    ri_slevi_enc_text(&u, 0u, t, sizeof t);
    RI_ASSERT(!strcmp(ri_slevi_page_title(&u), "ENV 2  4/4") && !strcmp(t, "NOTE ON"), "trigger page: %s", t);
    ri_slevi_set_value(&u, RI_SLEVI_MODULE, (int)RI_SLEVI_M_LFO1);
    RI_ASSERT(ri_slevi_page_count(&u) == 3u, "lfo pages");
    ri_slevi_enc_text(&u, 1u, t, sizeof t);
    RI_ASSERT(!strcmp(t, "0.57HZ"), "default rate text %s", t);
    ri_slevi_press(&u, RI_SLEVI_PAGEDN);
    RI_ASSERT(!ri_slevi_enc_live(&u, 0u) && !ri_slevi_enc_live(&u, 5u) && ri_slevi_enc_live(&u, 3u),
        "steps and stagger hidden by default");
    u.lfv[0][RI_LEVI_LP_WAVE] = RI_LEVI_LW_STEP;
    u.lfv[0][RI_LEVI_LP_TRIG] = 2u;
    RI_ASSERT(ri_slevi_enc_live(&u, 0u) && ri_slevi_enc_live(&u, 5u), "steps on step wave, stagger on trig off");
    RI_ASSERT(ri_slevi_ctl_key(&u, RI_SLEVI_ENC0 + 5u, &key, &val) == 1 && key == RI_LEVI_LFOKEY(0u, RI_LEVI_LP_STAGGER),
        "stagger key");
    {
        const struct RIGeoSection *g = ri_geo_section(RI_SEC_LEVI);
        uint32_t i, f = 0u;
        for (i = 0u; i < g->nitems; i++)
            f |= ((g->items[i].reg_id & 0xFFu) == RI_SLEVI_DENV1 ? 1u : 0u) |
                ((g->items[i].reg_id & 0xFFu) == RI_SLEVI_AENV2 ? 2u : 0u);
        RI_ASSERT(f == 3u, "ENV 1 / ENV 2 top-panel knobs live");
    }
    RI_RESULT("levi_mod");
}
