/* t133_levi_matrix2 — Levi mod matrix grown to the manual's lists, and
 * macros (fidelity plan P5b, owner 2026-09-30; manual pp. 120-127,
 * clean-room: own ids, names and laws).
 * Laws: every listed source and module/parameter has a name; a route
 * needs a source and a module; routes act on oscillators (one, all,
 * carriers, modulators), filters, VCA, envelopes, LFOs and algorithm
 * morph; a route can drive another route's depth; macro knobs scale
 * their routes (button value when the button is on; knob 0 is inert,
 * bit for bit); keytrack at C4 is inert; extreme programs stay finite;
 * keys; UI pages, texts and keys.
 */
#include <stdio.h>
#include <string.h>
#include "tests/helpers/ri_assert.h"
#include "engine/dsp/levi.h"
#include "engine/dsp/levi_matrix.h"
#include "engine/seq/autolane.h"
#include "gui/sectlevi.h"

#define SR 48000.0f
#define N 24000u

static struct RILeviSet A, B;
static float oa[N], ob[N];

static void render(struct RILeviSet *s, float *o, uint32_t n) {
    uint32_t i;
    for (i = 0u; i < n; i++)
        o[i] = levi_voice_render(&s->v[0], &s->mx, SR);
}

static uint32_t crossings(const float *o, uint32_t from, uint32_t n) {
    uint32_t i, c = 0u;
    for (i = from + 1u; i < from + n; i++)
        if ((o[i - 1u] < 0.0f) != (o[i] < 0.0f))
            c++;
    return c;
}

static float peak(const float *o, uint32_t from, uint32_t n) {
    uint32_t i;
    float p = 0.0f;
    for (i = from; i < from + n; i++) {
        float a = o[i] < 0.0f ? -o[i] : o[i];
        if (!(a < 1e30f))
            return 1e30f;
        if (a > p)
            p = a;
    }
    return p;
}

/* A plain sine carrier (modulator silent), filters open, VCA open. */
static void plain(struct RILeviSet *s) {
    levi_init_set(s);
    levi_set_op_ui(s, 0u, 1u, RI_LEVI_OP_ENVL, 64u);
    levi_set_param(s, 0u, RI_LEVI_CUTOFF, 18000.0f);
    levi_set_param(s, 0u, RI_LEVI_CUTOFF2, 18000.0f);
    levi_set_param(s, 0u, RI_LEVI_RESO, 0.0f);
    levi_set_param(s, 0u, RI_LEVI_RESO2, 0.0f);
}

/* Macro 1 route 0 -> module/param at +100 %, knob k. */
static void macro1(struct RILeviSet *s, uint32_t dmod, uint32_t dpar, uint8_t knob) {
    levi_set_mr_ui(s, 0u, 0u, 0u, (uint8_t)dmod);
    levi_set_mr_ui(s, 0u, 0u, 1u, (uint8_t)dpar);
    levi_set_mr_ui(s, 0u, 0u, 2u, 127u);
    levi_set_param_ui(s, 0u, RI_CTL_LEVI_MKNOB0 & 0xFFu, knob);
}

int main(void) {
    struct RISectLevi u;
    struct RILeviModOut mo[RI_LEVI_MODOUT_MAX];
    float src[RI_LEVI_MS_N];
    uint32_t k, d, p, n, c0;
    char t[32];
    uint16_t key;
    int val;

    /* ---- Names and lists. ---- */
    for (k = 1u; k < RI_LEVI_MS_UI_N; k++)
        RI_ASSERT(ri_levi_ms_by_ui(k) < RI_LEVI_MS_N && ri_levi_ms_name(ri_levi_ms_by_ui(k))[0] &&
            ri_levi_ms_to_ui(ri_levi_ms_by_ui(k)) == k, "source %u named and round-trips", k);
    RI_ASSERT(ri_levi_ms_by_ui(0u) == RI_LEVI_MS_N && ri_levi_ms_by_ui(1u) == RI_LEVI_MS_ENV0, "list order");
    for (d = 1u; d < RI_LEVI_DM_N; d++) {
        RI_ASSERT(ri_levi_dm_nparam(d) > 0u && ri_levi_dm_name(d)[0], "module %u", d);
        for (p = 0u; p < ri_levi_dm_nparam(d); p++)
            RI_ASSERT(ri_levi_dp_name(d, p)[0], "module %u param %u named", d, p);
        RI_ASSERT(!ri_levi_dp_name(d, ri_levi_dm_nparam(d))[0], "module %u past the end", d);
    }

    /* ---- Evaluation: a route needs a source and a module; depth routes; macros. ---- */
    levi_init_set(&A);
    for (k = 0u; k < RI_LEVI_MS_N; k++)
        src[k] = 0.5f;
    RI_ASSERT(ri_levi_matrix_eval2(&A.mx, src, mo) == 0u, "empty program, no output");
    levi_set_mx_ui(&A, 3u, 1u, RI_LEVI_DM_OSC1);                  /* module, no source yet */
    levi_set_mx_ui(&A, 3u, 2u, RI_LEVI_DO_PITCH);
    levi_set_mx_ui(&A, 3u, 3u, 127u);
    RI_ASSERT(!A.mx.slot[3].on && ri_levi_matrix_eval2(&A.mx, src, mo) == 0u, "no source, no route");
    levi_set_mx_ui(&A, 3u, 0u, 1u);                                 /* ENV 1 */
    n = ri_levi_matrix_eval2(&A.mx, src, mo);
    RI_ASSERT(n == 1u && mo[0].dmod == RI_LEVI_DM_OSC1 && mo[0].dpar == RI_LEVI_DO_PITCH && mo[0].x == 0.5f,
        "route out %u %f", n, (double)mo[0].x);
    levi_set_mx_ui(&A, 3u, 3u, 64u);                                /* depth 0 ... */
    RI_ASSERT(ri_levi_matrix_eval2(&A.mx, src, mo) == 0u, "depth 0 silent");
    levi_set_mx_ui(&A, 4u, 0u, 1u);                                 /* ... until route 5 drives it */
    levi_set_mx_ui(&A, 4u, 1u, RI_LEVI_DM_MTRX);
    levi_set_mx_ui(&A, 4u, 2u, 3u);                                 /* depth of route 4 (slot 3) */
    levi_set_mx_ui(&A, 4u, 3u, 127u);
    n = ri_levi_matrix_eval2(&A.mx, src, mo);
    RI_ASSERT(n == 1u && mo[0].x == 0.25f, "depth modulation: %f", (double)mo[0].x);
    levi_init_set(&A);
    macro1(&A, RI_LEVI_DM_AFILT, 0u, 127u);
    n = ri_levi_matrix_eval2(&A.mx, src, mo);
    RI_ASSERT(n == 1u && mo[0].dmod == RI_LEVI_DM_AFILT && mo[0].x == 1.0f, "macro knob full");
    levi_set_mr_ui(&A, 0u, 0u, 3u, 0u);
    levi_set_param_ui(&A, 0u, RI_CTL_LEVI_MBTN0 & 0xFFu, 1u);
    n = ri_levi_matrix_eval2(&A.mx, src, mo);
    RI_ASSERT(n == 1u && mo[0].x == 0.0f, "button on: its value (0)");

    /* ---- Render: macro knob 0 is inert; full knob moves the pitch 2 octaves. ---- */
    plain(&A);
    plain(&B);
    macro1(&A, RI_LEVI_DM_OSC1, RI_LEVI_DO_PITCH, 0u);
    levi_trigger(&A, 0u, 57u);
    levi_trigger(&B, 0u, 57u);
    render(&A, oa, N);
    render(&B, ob, N);
    RI_ASSERT(!memcmp(oa, ob, sizeof oa), "macro knob 0 inert");
    c0 = crossings(ob, 4800u, 19200u);
    macro1(&A, RI_LEVI_DM_OSC1, RI_LEVI_DO_PITCH, 127u);
    levi_trigger(&A, 0u, 57u);
    render(&A, oa, N);
    k = crossings(oa, 4800u, 19200u);
    RI_ASSERT(k > 38u * c0 / 10u && k < 42u * c0 / 10u, "+24 semitones: %u vs %u crossings", k, c0);
    /* All oscillators too; modulators only leaves the carrier's pitch. */
    macro1(&A, RI_LEVI_DM_ALLOSC, RI_LEVI_DO_PITCH, 127u);
    levi_trigger(&A, 0u, 57u);
    render(&A, oa, N);
    RI_ASSERT(crossings(oa, 4800u, 19200u) > 38u * c0 / 10u, "all oscillators");
    macro1(&A, RI_LEVI_DM_MODS, RI_LEVI_DO_INIT, 127u);
    levi_trigger(&A, 0u, 57u);
    render(&A, oa, N);
    RI_ASSERT(memcmp(oa, ob, sizeof oa) != 0 && A.v[0].opm[0][RI_LEVI_DO_INIT] == 0.0f &&
        A.v[0].opm[1][RI_LEVI_DO_INIT] == 1.0f, "modulators only (DUO: op 2)");
    macro1(&A, RI_LEVI_DM_OSC1 + 1u, RI_LEVI_DO_PITCH, 127u);      /* OSC 2 only */
    levi_trigger(&A, 0u, 57u);
    render(&A, oa, 10u);
    RI_ASSERT(A.v[0].opm[1][RI_LEVI_DO_PITCH] == 1.0f && A.v[0].opm[0][RI_LEVI_DO_PITCH] == 0.0f, "OSC 2 route");
    levi_set_mr_ui(&A, 0u, 0u, 0u, RI_LEVI_DM_CARR);                /* carriers: init/env level only */
    RI_ASSERT(A.mx.mroute[0][0].dpar == 0u, "carrier params clamp to the list");
    levi_set_mx_ui(&A, 9u, 1u, RI_LEVI_DM_OSC1);
    levi_set_mx_ui(&A, 9u, 2u, RI_LEVI_DO_RELEASE);
    levi_set_mx_ui(&A, 9u, 1u, RI_LEVI_DM_VCA);
    RI_ASSERT(A.mx.slot[9].dpar == 0u, "route params clamp to the new module");

    /* ---- Keytrack at C4 is inert; VCA level -1 silences. ---- */
    plain(&A);
    levi_set_mx_ui(&A, 0u, 0u, (uint8_t)ri_levi_ms_to_ui(RI_LEVI_MS_NOTE));
    levi_set_mx_ui(&A, 0u, 1u, RI_LEVI_DM_DFILT);
    levi_set_mx_ui(&A, 0u, 2u, 0u);
    levi_set_mx_ui(&A, 0u, 3u, 127u);
    plain(&B);
    levi_set_param(&A, 0u, RI_LEVI_CUTOFF, 800.0f);
    levi_set_param(&B, 0u, RI_LEVI_CUTOFF, 800.0f);
    levi_trigger(&A, 0u, 60u);
    levi_trigger(&B, 0u, 60u);
    render(&A, oa, N);
    render(&B, ob, N);
    RI_ASSERT(!memcmp(oa, ob, sizeof oa), "keytrack at C4 inert");
    plain(&A);
    levi_set_mr_ui(&A, 0u, 0u, 0u, RI_LEVI_DM_VCA);
    levi_set_mr_ui(&A, 0u, 0u, 1u, 0u);
    levi_set_mr_ui(&A, 0u, 0u, 2u, 0u);                             /* -100 % */
    levi_set_param_ui(&A, 0u, RI_CTL_LEVI_MKNOB0 & 0xFFu, 127u);
    levi_trigger(&A, 0u, 60u);
    render(&A, oa, 4800u);
    RI_ASSERT(peak(oa, 0u, 4800u) == 0.0f, "VCA level -1 silences");

    /* ---- Envelopes and LFOs as destinations. ---- */
    plain(&A);
    macro1(&A, RI_LEVI_DM_ENV1 + 2u, RI_LEVI_DE_ATTACK, 127u);     /* ENV 3 (VCA) attack x256 */
    levi_set_menv_ui(&A, 0u, 2u, RI_LEVI_OP_ATTACK, 20u);
    plain(&B);
    levi_set_menv_ui(&B, 0u, 2u, RI_LEVI_OP_ATTACK, 20u);
    levi_trigger(&A, 0u, 60u);
    levi_trigger(&B, 0u, 60u);
    render(&A, oa, 4800u);
    render(&B, ob, 4800u);
    RI_ASSERT(peak(oa, 2400u, 2400u) < 0.5f * peak(ob, 2400u, 2400u), "ENV 3 attack stretched");
    plain(&A);
    macro1(&A, RI_LEVI_DM_ENV1 + 4u, RI_LEVI_DE_SUSTAIN, 127u);    /* ENV 5 sustain + 1 */
    levi_set_menv_ui(&A, 0u, 4u, RI_LEVI_OP_SUSTAIN, 0u);
    levi_trigger(&A, 0u, 60u);
    render(&A, oa, N);
    RI_ASSERT(levi_menv_value(&A.v[0], 4u) == 1.0f, "ENV 5 sustain moved to full: %f",
        (double)levi_menv_value(&A.v[0], 4u));
    for (k = 0u; k < 2u; k++) {
        plain(&A);
        levi_set_lfo_ui(&A, 0u, 0u, RI_LEVI_LP_SPEED, 1u);         /* LFO 1 at 5 Hz */
        levi_set_lfo_ui(&A, 0u, 0u, RI_LEVI_LP_RATE, 0u);
        if (k)
            macro1(&A, RI_LEVI_DM_LFO1, 0u, 127u);                  /* rate x16 */
        levi_trigger(&A, 0u, 60u);
        for (d = 0u, n = 0u; d < N; d++) {
            (void)levi_voice_render(&A.v[0], &A.mx, SR);
            n += A.v[0].lfo[0].wrapped;
        }
        if (!k)
            c0 = n;
    }
    RI_ASSERT(c0 == 2u && n >= 38u && n <= 40u, "LFO 1 rate x16: %u vs %u cycles", n, c0);

    /* ---- Extreme programs stay finite. ---- */
    plain(&A);
    for (k = 0u; k < RI_LEVI_MX_NSLOTS; k++) {
        uint32_t dm = 1u + k % (RI_LEVI_DM_N - 1u);
        levi_set_mx_ui(&A, k, 0u, (uint8_t)(1u + k % (RI_LEVI_MS_UI_N - 1u)));
        levi_set_mx_ui(&A, k, 1u, (uint8_t)dm);
        levi_set_mx_ui(&A, k, 2u, (uint8_t)(k % ri_levi_dm_nparam(dm)));
        levi_set_mx_ui(&A, k, 3u, k & 1u ? 0u : 127u);
    }
    for (k = 0u; k < RI_LEVI_NLFO; k++)
        levi_set_lfo_ui(&A, 0u, k, RI_LEVI_LP_WAVE, RI_LEVI_LW_NOISE);
    levi_trigger(&A, 0u, 60u);
    render(&A, oa, N);
    RI_ASSERT(peak(oa, 0u, N) < 64.0f, "extreme program finite %f", (double)peak(oa, 0u, N));

    /* ---- Keys. ---- */
    RI_ASSERT(ri_auto_allowed(RI_LEVI_MXKEY(31u, 3u)) && !ri_auto_allowed(0x1180u) &&
        ri_auto_allowed(RI_LEVI_MRKEY(7u, 7u, 3u)) && ri_auto_allowed(RI_CTL_LEVI_MKNOB0 + 7u) &&
        ri_auto_allowed(RI_CTL_LEVI_MBTN0 + 7u), "P5b keys");

    /* ---- UI: matrix and macro pages. ---- */
    ri_slevi_init(&u);
    ri_slevi_set_value(&u, RI_SLEVI_MODULE, (int)RI_SLEVI_M_MATRIX);
    RI_ASSERT(ri_slevi_page_count(&u) == 16u && !strcmp(ri_slevi_page_title(&u), "MATRIX 1|2  1/16"),
        "matrix title %s", ri_slevi_page_title(&u));
    ri_slevi_enc_text(&u, 0u, t, sizeof t);
    RI_ASSERT(!strcmp(t, "---"), "empty source %s", t);
    ri_slevi_set_value(&u, RI_SLEVI_ENC0 + 5u, 127);                /* route 2 module: last */
    ri_slevi_enc_text(&u, 5u, t, sizeof t);
    RI_ASSERT(!strcmp(t, "DELAY"), "module text %s", t);
    ri_slevi_enc_text(&u, 6u, t, sizeof t);
    RI_ASSERT(!strcmp(t, "TIME"), "param follows module %s", t);
    RI_ASSERT(ri_slevi_ctl_key(&u, RI_SLEVI_ENC0 + 5u, &key, &val) == 1 && key == RI_LEVI_MXKEY(1u, 1u) &&
        val == (int)RI_LEVI_DM_DELAY, "route 2 module key %04x=%d", key, val);
    ri_slevi_set_value(&u, RI_SLEVI_ENC0, 4);                          /* 4/127 of 40 sources = the first */
    ri_slevi_enc_text(&u, 0u, t, sizeof t);
    RI_ASSERT(!strcmp(t, "ENV 1"), "source text %s", t);
    ri_slevi_enc_text(&u, 3u, t, sizeof t);
    RI_ASSERT(!strcmp(t, "0"), "depth text %s", t);
    ri_slevi_set_value(&u, RI_SLEVI_MODULE, (int)RI_SLEVI_M_MACRO);
    RI_ASSERT(ri_slevi_page_count(&u) == 34u && ri_slevi_ctl_idx(&u, RI_SLEVI_ENC0 + 7u) == RI_SLEVI_MKNOB0 + 7u,
        "macro knobs page");
    for (k = 0u; k < 2u; k++)
        ri_slevi_press(&u, RI_SLEVI_PAGEDN);
    RI_ASSERT(!strcmp(ri_slevi_page_title(&u), "MACRO 1 R1|2  3/34"), "macro route title %s", ri_slevi_page_title(&u));
    RI_ASSERT(ri_slevi_ctl_key(&u, RI_SLEVI_ENC0 + 7u, &key, &val) == 1 && key == RI_LEVI_MRKEY(0u, 1u, 3u),
        "macro 1 route 2 button value key %04x", key);
    RI_ASSERT(ri_slevi_legacy(RI_SLEVI_ROUTE0) && ri_slevi_page_reaches(RI_SLEVI_MBTN0 + 7u), "legacy / reach");
    RI_RESULT("levi_matrix2");
}
