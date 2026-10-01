/* t140_levi_modfx — Levi P7c pre/post 9-type mod engines.
 * Laws: bypassed/defaults bit-identical to clean; bypass with extreme
 * knobs inert; dry-only identical; all 9 types audible at full wet;
 * P1 moves; P2 moves; preset == hand-set tuple; dry/wet; DM_PREFX/POSTFX
 * routes; pre feeds delay, post follows reverb; keys/pages/texts;
 * null-safe; finite extremes.
 */
#include <stdio.h>
#include <string.h>
#include "tests/helpers/ri_assert.h"
#include "engine/dsp/levi.h"
#include "engine/dsp/levi_fx.h"
#include "engine/dsp/levi_matrix.h"
#include "engine/seq/autolane.h"
#include "gui/sectlevi.h"
#include "gui/ctlreg.h"

#define SR 48000.0f
#define N 4800u

static struct RILeviSet A, B;
static float oa[N], ob[N];
static float la[N], lb[N];

static void stereo(struct RILeviSet *s, float *l, float *r, uint32_t n) {
    levi_voice_render_sum_stereo(s, l, r, n, SR);
}

static int differ(const float *a, const float *b, uint32_t n) {
    uint32_t i;
    for (i = 0u; i < n; i++)
        if (a[i] != b[i])
            return 1;
    return 0;
}

static void pre_on(struct RILeviSet *s) {
    levi_set_param_ui(s, 0u, RI_CTL_LEVI_PREBYPASS & 0xFFu, 1u);
}

static void post_on(struct RILeviSet *s) {
    levi_set_param_ui(s, 0u, RI_CTL_LEVI_POSTBYPASS & 0xFFu, 1u);
}

/* Per-type (p1, p2) that must be clearly audible at full wet. */
static uint8_t tp1(uint32_t t) {
    static const uint8_t v[RI_LEVI_MT_N] = { 64, 64, 64, 64, 64, 64, 127, 0, 127 };
    return v[t];
}

static uint8_t tp2(uint32_t t) {
    static const uint8_t v[RI_LEVI_MT_N] = { 64, 64, 64, 64, 64, 127, 0, 127, 64 };
    return v[t];
}

static void set_pre(struct RILeviSet *s, uint32_t type) {
    pre_on(s);
    levi_set_param_ui(s, 0u, RI_CTL_LEVI_PTYPE & 0xFFu, (uint8_t)type);
    levi_set_param_ui(s, 0u, RI_CTL_LEVI_PP1 & 0xFFu, tp1(type));
    levi_set_param_ui(s, 0u, RI_CTL_LEVI_PP2 & 0xFFu, tp2(type));
    levi_set_param_ui(s, 0u, RI_CTL_LEVI_PDRYWET & 0xFFu, 127u);
}

int main(void) {
    uint32_t t;

    /* Bypassed by default: bit-identical to a clean twin. */
    levi_init_set(&A);
    levi_trigger(&A, 0u, 60u);
    stereo(&A, oa, ob, N);
    levi_init_set(&B);
    levi_trigger(&B, 0u, 60u);
    stereo(&B, la, lb, N);
    RI_ASSERT(!differ(oa, la, N) && !differ(ob, lb, N), "bypass default");

    /* Bypass with extreme knobs: still identical (knobs inert). */
    levi_init_set(&B);
    levi_set_param_ui(&B, 0u, RI_CTL_LEVI_PTYPE & 0xFFu, RI_LEVI_MT_DISTORT);
    levi_set_param_ui(&B, 0u, RI_CTL_LEVI_PP1 & 0xFFu, 127u);
    levi_set_param_ui(&B, 0u, RI_CTL_LEVI_PP2 & 0xFFu, 127u);
    levi_set_param_ui(&B, 0u, RI_CTL_LEVI_PDRYWET & 0xFFu, 127u);
    levi_set_param_ui(&B, 0u, RI_CTL_LEVI_OTYPE & 0xFFu, RI_LEVI_MT_LOFI);
    levi_set_param_ui(&B, 0u, RI_CTL_LEVI_OP1 & 0xFFu, 127u);
    levi_set_param_ui(&B, 0u, RI_CTL_LEVI_OP2 & 0xFFu, 127u);
    levi_set_param_ui(&B, 0u, RI_CTL_LEVI_ODRYWET & 0xFFu, 127u);
    levi_trigger(&B, 0u, 60u);
    stereo(&B, la, lb, N);
    RI_ASSERT(!differ(oa, la, N) && !differ(ob, lb, N), "bypass knobs inert");

    /* Dry-only (wet 0, on) == bypassed. */
    levi_init_set(&B);
    pre_on(&B);
    levi_set_param_ui(&B, 0u, RI_CTL_LEVI_PDRYWET & 0xFFu, 0u);
    post_on(&B);
    levi_set_param_ui(&B, 0u, RI_CTL_LEVI_ODRYWET & 0xFFu, 0u);
    levi_trigger(&B, 0u, 60u);
    stereo(&B, la, lb, N);
    RI_ASSERT(!differ(oa, la, N), "dry-only identical");

    /* All 9 types audible (pre slot, full wet). */
    for (t = 0u; t < RI_LEVI_MT_N; t++) {
        levi_init_set(&B);
        set_pre(&B, t);
        levi_trigger(&B, 0u, 60u);
        stereo(&B, la, lb, N);
        RI_ASSERT(differ(oa, la, N), "pre type %u audible", t);
    }
    /* All 9 types audible (post slot, full wet). */
    for (t = 0u; t < RI_LEVI_MT_N; t++) {
        levi_init_set(&B);
        post_on(&B);
        levi_set_param_ui(&B, 0u, RI_CTL_LEVI_OTYPE & 0xFFu, (uint8_t)t);
        levi_set_param_ui(&B, 0u, RI_CTL_LEVI_OP1 & 0xFFu, tp1(t));
        levi_set_param_ui(&B, 0u, RI_CTL_LEVI_OP2 & 0xFFu, tp2(t));
        levi_set_param_ui(&B, 0u, RI_CTL_LEVI_ODRYWET & 0xFFu, 127u);
        levi_trigger(&B, 0u, 60u);
        stereo(&B, la, lb, N);
        RI_ASSERT(differ(oa, la, N), "post type %u audible", t);
    }

    /* P1 moves; P2 moves (chorus, full wet). */
    levi_init_set(&A);
    set_pre(&A, RI_LEVI_MT_CHORUS);
    levi_set_param_ui(&A, 0u, RI_CTL_LEVI_PP1 & 0xFFu, 0u);
    levi_trigger(&A, 0u, 60u);
    stereo(&A, oa, ob, N);
    levi_init_set(&B);
    set_pre(&B, RI_LEVI_MT_CHORUS);
    levi_set_param_ui(&B, 0u, RI_CTL_LEVI_PP1 & 0xFFu, 127u);
    levi_trigger(&B, 0u, 60u);
    stereo(&B, la, lb, N);
    RI_ASSERT(differ(oa, la, N), "p1 moves");
    levi_set_param_ui(&B, 0u, RI_CTL_LEVI_PP1 & 0xFFu, 0u);
    levi_set_param_ui(&B, 0u, RI_CTL_LEVI_PP2 & 0xFFu, 127u);
    levi_trigger(&B, 0u, 60u);
    stereo(&B, la, lb, N);
    RI_ASSERT(differ(oa, la, N), "p2 moves");

    /* Every knob moves every type (P1 sweep, then P2 sweep). */
    for (t = 0u; t < RI_LEVI_MT_N; t++) {
        levi_init_set(&A);
        pre_on(&A);
        levi_set_param_ui(&A, 0u, RI_CTL_LEVI_PTYPE & 0xFFu, (uint8_t)t);
        levi_set_param_ui(&A, 0u, RI_CTL_LEVI_PP1 & 0xFFu, 0u);
        levi_set_param_ui(&A, 0u, RI_CTL_LEVI_PP2 & 0xFFu, tp2(t));
        levi_set_param_ui(&A, 0u, RI_CTL_LEVI_PDRYWET & 0xFFu, 127u);
        levi_trigger(&A, 0u, 60u);
        stereo(&A, oa, ob, N);
        levi_init_set(&B);
        pre_on(&B);
        levi_set_param_ui(&B, 0u, RI_CTL_LEVI_PTYPE & 0xFFu, (uint8_t)t);
        levi_set_param_ui(&B, 0u, RI_CTL_LEVI_PP1 & 0xFFu, 127u);
        levi_set_param_ui(&B, 0u, RI_CTL_LEVI_PP2 & 0xFFu, tp2(t));
        levi_set_param_ui(&B, 0u, RI_CTL_LEVI_PDRYWET & 0xFFu, 127u);
        levi_trigger(&B, 0u, 60u);
        stereo(&B, la, lb, N);
        RI_ASSERT(differ(oa, la, N), "p1 sweeps type %u", t);
        levi_init_set(&A);
        pre_on(&A);
        levi_set_param_ui(&A, 0u, RI_CTL_LEVI_PTYPE & 0xFFu, (uint8_t)t);
        levi_set_param_ui(&A, 0u, RI_CTL_LEVI_PP1 & 0xFFu, tp1(t));
        levi_set_param_ui(&A, 0u, RI_CTL_LEVI_PP2 & 0xFFu, 0u);
        levi_set_param_ui(&A, 0u, RI_CTL_LEVI_PDRYWET & 0xFFu, 127u);
        levi_trigger(&A, 0u, 60u);
        stereo(&A, oa, ob, N);
        levi_init_set(&B);
        pre_on(&B);
        levi_set_param_ui(&B, 0u, RI_CTL_LEVI_PTYPE & 0xFFu, (uint8_t)t);
        levi_set_param_ui(&B, 0u, RI_CTL_LEVI_PP1 & 0xFFu, tp1(t));
        levi_set_param_ui(&B, 0u, RI_CTL_LEVI_PP2 & 0xFFu, 127u);
        levi_set_param_ui(&B, 0u, RI_CTL_LEVI_PDRYWET & 0xFFu, 127u);
        levi_trigger(&B, 0u, 60u);
        stereo(&B, la, lb, N);
        RI_ASSERT(differ(oa, la, N), "p2 sweeps type %u", t);
    }

    /* Preset == hand-set tuple (pure accessor + engine agree). */
    for (t = 0u; t < RI_LEVI_MT_N; t++) {
        uint8_t k, q1, q2, qw;
        for (k = 0u; k < 4u; k++) {
            RI_ASSERT(ri_levi_mod_preset(t, k, &q1, &q2, &qw) == 0, "preset ok %u/%u", t, k);
            levi_init_set(&A);
            pre_on(&A);
            levi_set_param_ui(&A, 0u, RI_CTL_LEVI_PTYPE & 0xFFu, (uint8_t)t);
            levi_set_param_ui(&A, 0u, RI_CTL_LEVI_PPRESET & 0xFFu, k);
            levi_set_param_ui(&A, 0u, RI_CTL_LEVI_PDRYWET & 0xFFu, 127u);
            levi_trigger(&A, 0u, 60u);
            stereo(&A, oa, ob, N);
            levi_init_set(&B);
            pre_on(&B);
            levi_set_param_ui(&B, 0u, RI_CTL_LEVI_PTYPE & 0xFFu, (uint8_t)t);
            levi_set_param_ui(&B, 0u, RI_CTL_LEVI_PP1 & 0xFFu, q1);
            levi_set_param_ui(&B, 0u, RI_CTL_LEVI_PP2 & 0xFFu, q2);
            levi_set_param_ui(&B, 0u, RI_CTL_LEVI_PDRYWET & 0xFFu, 127u);
            levi_trigger(&B, 0u, 60u);
            stereo(&B, la, lb, N);
            /* Preset dry/wet may differ from full: compare shape via wet-only
             * renders is overkill; the knob-write law is exact instead: */
            RI_ASSERT(A.fx.pre.p1 == B.fx.pre.p1 && A.fx.pre.p2 == B.fx.pre.p2,
                "preset writes knobs %u/%u", t, k);
        }
    }
    RI_ASSERT(ri_levi_mod_preset(9u, 0u, 0, 0, 0) == 2, "bad type refused");
    RI_ASSERT(ri_levi_mod_preset(0u, 4u, 0, 0, 0) == 2, "bad preset refused");
    RI_ASSERT(ri_levi_mod_preset(0u, 0u, 0, 0, 0) == 2, "null refused");
    {
        /* Factory content pinned (own tuples; guards table edits). */
        uint8_t q1 = 0u, q2 = 0u, qw = 0u;
        RI_ASSERT(ri_levi_mod_preset(RI_LEVI_MT_CHORUS, 0u, &q1, &q2, &qw) == 0 &&
            q1 == 16u && q2 == 64u && qw == 48u, "chorus preset 0");
        RI_ASSERT(ri_levi_mod_preset(RI_LEVI_MT_DISTORT, 3u, &q1, &q2, &qw) == 0 &&
            q1 == 127u && q2 == 100u && qw == 120u, "distort preset 3");
    }

    /* DM_PREFX/POSTFX: 3 params each; LFO->P1 moves the tail. */
    RI_ASSERT(ri_levi_dm_nparam(RI_LEVI_DM_PREFX) == 3u, "prefx nparam");
    RI_ASSERT(ri_levi_dm_nparam(RI_LEVI_DM_POSTFX) == 3u, "postfx nparam");
    levi_init_set(&A);
    set_pre(&A, RI_LEVI_MT_CHORUS);
    levi_trigger(&A, 0u, 60u);
    stereo(&A, oa, ob, N);
    levi_init_set(&B);
    set_pre(&B, RI_LEVI_MT_CHORUS);
    levi_set_mx_ui(&B, 0u, 0u, (uint8_t)ri_levi_ms_to_ui(RI_LEVI_MS_LFO0));
    levi_set_mx_ui(&B, 0u, 1u, RI_LEVI_DM_PREFX);
    levi_set_mx_ui(&B, 0u, 2u, RI_LEVI_DX_P1);
    levi_set_mx_ui(&B, 0u, 3u, 100u);
    levi_trigger(&B, 0u, 60u);
    stereo(&B, la, lb, N);
    RI_ASSERT(differ(oa, la, N), "dm prefx p1 moves");

    /* Chain order: pre feeds delay (distort+echo differs from echo alone);
     * post follows reverb (differs from reverb alone). */
    levi_init_set(&A);
    pre_on(&A);
    levi_set_param_ui(&A, 0u, RI_CTL_LEVI_PTYPE & 0xFFu, RI_LEVI_MT_DISTORT);
    levi_set_param_ui(&A, 0u, RI_CTL_LEVI_PP1 & 0xFFu, 127u);
    levi_set_param_ui(&A, 0u, RI_CTL_LEVI_PDRYWET & 0xFFu, 127u);
    levi_set_param_ui(&A, 0u, RI_CTL_LEVI_DLYTIME & 0xFFu, 40u);
    levi_set_param_ui(&A, 0u, RI_CTL_LEVI_DLYDRYWET & 0xFFu, 127u);
    levi_set_param_ui(&A, 0u, RI_CTL_LEVI_DBYPASS & 0xFFu, 1u);
    levi_trigger(&A, 0u, 60u);
    stereo(&A, oa, ob, N);
    levi_init_set(&B);
    levi_set_param_ui(&B, 0u, RI_CTL_LEVI_DLYTIME & 0xFFu, 40u);
    levi_set_param_ui(&B, 0u, RI_CTL_LEVI_DLYDRYWET & 0xFFu, 127u);
    levi_set_param_ui(&B, 0u, RI_CTL_LEVI_DBYPASS & 0xFFu, 1u);
    levi_trigger(&B, 0u, 60u);
    stereo(&B, la, lb, N);
    RI_ASSERT(differ(oa, la, N), "pre feeds delay");
    levi_init_set(&A);
    levi_set_param_ui(&A, 0u, RI_CTL_LEVI_RTIME & 0xFFu, 64u);
    levi_set_param_ui(&A, 0u, RI_CTL_LEVI_RDRYWET & 0xFFu, 127u);
    levi_set_param_ui(&A, 0u, RI_CTL_LEVI_RBYPASS & 0xFFu, 1u);
    post_on(&A);
    levi_set_param_ui(&A, 0u, RI_CTL_LEVI_OTYPE & 0xFFu, RI_LEVI_MT_TREMOLO);
    levi_set_param_ui(&A, 0u, RI_CTL_LEVI_OP1 & 0xFFu, 64u);
    levi_set_param_ui(&A, 0u, RI_CTL_LEVI_OP2 & 0xFFu, 127u);
    levi_set_param_ui(&A, 0u, RI_CTL_LEVI_ODRYWET & 0xFFu, 127u);
    levi_trigger(&A, 0u, 60u);
    stereo(&A, oa, ob, N);
    levi_init_set(&B);
    levi_set_param_ui(&B, 0u, RI_CTL_LEVI_RTIME & 0xFFu, 64u);
    levi_set_param_ui(&B, 0u, RI_CTL_LEVI_RDRYWET & 0xFFu, 127u);
    levi_set_param_ui(&B, 0u, RI_CTL_LEVI_RBYPASS & 0xFFu, 1u);
    levi_trigger(&B, 0u, 60u);
    stereo(&B, la, lb, N);
    RI_ASSERT(differ(oa, la, N), "post follows reverb");

    /* Keys / allow-list / pages / texts. */
    RI_ASSERT(ri_auto_allowed(RI_CTL_LEVI_PTYPE), "ptype allowed");
    RI_ASSERT(ri_auto_allowed(RI_CTL_LEVI_PREBYPASS), "prebypass allowed");
    RI_ASSERT(ri_auto_allowed(RI_CTL_LEVI_OTYPE), "otype allowed");
    RI_ASSERT(ri_auto_allowed(RI_CTL_LEVI_POSTBYPASS), "postbypass allowed");
    RI_ASSERT(!ri_auto_allowed(0x0ECAu), "0x0ECA refused (P9c)");
    {
        const struct RICtlDef *d = ri_ctlreg_find((uint16_t)((RI_SEC_LEVI << 8) | RI_SLEVI_FXPRE));
        RI_ASSERT(d && d->engine_id == RI_CTL_LEVI_PREBYPASS, "fxpre binds bypass");
    }
    {
        const struct RICtlDef *d = ri_ctlreg_find((uint16_t)((RI_SEC_LEVI << 8) | RI_SLEVI_FXPOST));
        RI_ASSERT(d && d->engine_id == RI_CTL_LEVI_POSTBYPASS, "fxpost binds bypass");
    }
    {
        struct RISectLevi lv;
        char tx[16];
        memset(&lv, 0, sizeof lv);
        ri_slevi_init(&lv);
        RI_ASSERT(ri_slevi_set_value(&lv, RI_SLEVI_MODULE, (int)RI_SLEVI_M_PREFX) == 1, "prefx module");
        RI_ASSERT(ri_slevi_page_count(&lv) == 1u, "prefx 1 page");
        RI_ASSERT(ri_slevi_enc_live(&lv, 0u) && !strcmp(ri_slevi_enc_name(&lv, 0u), "ON"), "slot0 on");
        RI_ASSERT(ri_slevi_enc_live(&lv, 1u) && !strcmp(ri_slevi_enc_name(&lv, 1u), "TYPE"), "slot1 type");
        RI_ASSERT(ri_slevi_enc_live(&lv, 2u) && !strcmp(ri_slevi_enc_name(&lv, 2u), "PRESET"), "slot2 preset");
        lv.page = 0u;
        ri_slevi_enc_text(&lv, 1u, tx, sizeof tx);
        RI_ASSERT(!strcmp(tx, "CHORUS"), "type text %s", tx);
        RI_ASSERT(ri_slevi_set_value(&lv, RI_SLEVI_MODULE, (int)RI_SLEVI_M_POSTFX) == 1, "postfx module");
        RI_ASSERT(ri_slevi_page_count(&lv) == 1u, "postfx 1 page");
        RI_ASSERT(ri_slevi_enc_live(&lv, 0u) && !strcmp(ri_slevi_enc_name(&lv, 0u), "ON"), "post slot0 on");
    }

    /* Null-safe entry points. */
    {
        float x = 1.0f, y = 1.0f;
        levi_fx_mod(0, SR, 0.5f, 0.5f, &x, &y);
        RI_ASSERT(x == 0.5f && y == 0.5f, "mod null passthrough");
        RI_ASSERT(ri_levi_mod_preset(0u, 0u, 0, 0, 0) == 2, "preset null refused");

    /* Type clamps to DISTORT (8) at the engine door. */
    levi_init_set(&A);
    pre_on(&A);
    levi_set_param_ui(&A, 0u, RI_CTL_LEVI_PTYPE & 0xFFu, 200u);
    levi_set_param_ui(&A, 0u, RI_CTL_LEVI_PP1 & 0xFFu, 64u);
    levi_set_param_ui(&A, 0u, RI_CTL_LEVI_PP2 & 0xFFu, 64u);
    levi_set_param_ui(&A, 0u, RI_CTL_LEVI_PDRYWET & 0xFFu, 127u);
    levi_trigger(&A, 0u, 60u);
    stereo(&A, oa, ob, N);
    levi_init_set(&B);
    pre_on(&B);
    levi_set_param_ui(&B, 0u, RI_CTL_LEVI_PTYPE & 0xFFu, RI_LEVI_MT_DISTORT);
    levi_set_param_ui(&B, 0u, RI_CTL_LEVI_PP1 & 0xFFu, 64u);
    levi_set_param_ui(&B, 0u, RI_CTL_LEVI_PP2 & 0xFFu, 64u);
    levi_set_param_ui(&B, 0u, RI_CTL_LEVI_PDRYWET & 0xFFu, 127u);
    levi_trigger(&B, 0u, 60u);
    stereo(&B, la, lb, N);
    RI_ASSERT(!differ(oa, la, N), "ptype clamps");
    }

    /* Finite/bounded extremes storm (all types, max everything, both slots). */
    {
        uint32_t bad = 0;
        for (t = 0u; t < RI_LEVI_MT_N; t++) {
            int i, slot;
            for (slot = 0; slot < 2; slot++) {
                levi_init_set(&A);
                pre_on(&A);
                post_on(&A);
                levi_set_param_ui(&A, 0u, RI_CTL_LEVI_PTYPE & 0xFFu, (uint8_t)t);
                levi_set_param_ui(&A, 0u, RI_CTL_LEVI_PP1 & 0xFFu, 127u);
                levi_set_param_ui(&A, 0u, RI_CTL_LEVI_PP2 & 0xFFu, 127u);
                levi_set_param_ui(&A, 0u, RI_CTL_LEVI_PDRYWET & 0xFFu, 127u);
                levi_set_param_ui(&A, 0u, RI_CTL_LEVI_OTYPE & 0xFFu, (uint8_t)t);
                levi_set_param_ui(&A, 0u, RI_CTL_LEVI_OP1 & 0xFFu, 127u);
                levi_set_param_ui(&A, 0u, RI_CTL_LEVI_OP2 & 0xFFu, 127u);
                levi_set_param_ui(&A, 0u, RI_CTL_LEVI_ODRYWET & 0xFFu, 127u);
                levi_trigger(&A, 0u, (uint8_t)(20 + t * 10u));
                for (i = 0; i < 4800; i++) {
                    float l, r;
                    levi_voice_render_sum_stereo(&A, ob, la, 1u, SR);
                    l = ob[0];
                    r = la[0];
                    if (!((l > -8.0f && l < 8.0f) && (r > -8.0f && r < 8.0f)))
                        bad = 1;
                }
            }
        }
        RI_ASSERT(!bad, "extremes bounded");
    }

    RI_RESULT("t140_levi_modfx");
}
