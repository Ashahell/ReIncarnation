/* t136_levi_stereo — Levi P6c stereo + scales/microtuning/vintage.
 * Laws: center dual-mono bit-identical to the mono sum; hard pan
 * isolates; width 0 collapses; modes differ; per-osc pan + DO_PAN move
 * the image; unison spread; vintage bypass identical / degrades bounded;
 * scale quantize (ties lower), keylock off chromatic, micro tables move
 * pitch; DM_VOICE PAN route; keys/pages; finite extremes.
 */
#include <stdio.h>
#include <string.h>
#include "tests/helpers/ri_assert.h"
#include "engine/dsp/levi.h"
#include "engine/dsp/levi_matrix.h"
#include "engine/seq/autolane.h"
#include "gui/sectlevi.h"
#include "gui/ctlreg.h"

#define SR 48000.0f
#define N 4800u

static struct RILeviSet A, B;
static float oa[N], ob[N], oc[N], od[N];

static void mono(struct RILeviSet *s, float *o) {
    levi_voice_render_sum(s, o, N, SR);
}

static void stereo(struct RILeviSet *s, float *l, float *r) {
    levi_voice_render_sum_stereo(s, l, r, N, SR);
}

static int differ(const float *a, const float *b, uint32_t n) {
    uint32_t i;
    for (i = 0u; i < n; i++)
        if (a[i] != b[i])
            return 1;
    return 0;
}

static int silent(const float *a, uint32_t n) {
    uint32_t i;
    for (i = 0u; i < n; i++)
        if (a[i] != 0.0f)
            return 0;
    return 1;
}

static float peak(const float *o, uint32_t n) {
    uint32_t i;
    float p = 0.0f;
    for (i = 0u; i < n; i++) {
        float x = o[i] < 0.0f ? -o[i] : o[i];
        if (x > p)
            p = x;
    }
    return p;
}

int main(void) {
    /* Center dual-mono: L and R both equal the mono sum, bit for bit. */
    levi_init_set(&A);
    levi_trigger(&A, 0u, 60u);
    mono(&A, oa);
    levi_init_set(&B);
    levi_trigger(&B, 0u, 60u);
    stereo(&B, ob, oc);
    RI_ASSERT(!differ(oa, ob, N), "stereo L == mono");
    RI_ASSERT(!differ(oa, oc, N), "stereo R == mono");

    /* Hard right: L silent, R carries the mix. */
    levi_init_set(&A);
    levi_set_param_ui(&A, 0u, RI_CTL_LEVI_VPAN & 0xFFu, 127u);
    levi_trigger(&A, 0u, 60u);
    stereo(&A, ob, oc);
    RI_ASSERT(silent(ob, N), "hard right L silent");
    RI_ASSERT(!differ(oa, oc, N), "hard right R == mix");

    /* Width 0 collapses widely-panned ops to the voice pan. */
    levi_init_set(&A);
    levi_set_param_ui(&A, 0u, RI_CTL_LEVI_VOSCPAN1 & 0xFFu, 0u);
    levi_set_param_ui(&A, 0u, RI_CTL_LEVI_VWIDTH & 0xFFu, 0u);
    levi_trigger(&A, 0u, 60u);
    stereo(&A, ob, oc);
    RI_ASSERT(!differ(ob, oc, N), "width 0 collapses");
    levi_set_param_ui(&A, 0u, RI_CTL_LEVI_VWIDTH & 0xFFu, 127u);
    levi_trigger(&A, 0u, 60u);
    stereo(&A, oa, ob);
    RI_ASSERT(differ(oa, ob, N), "width 1 spreads");

    /* Pan modes differ (same pan position). */
    levi_init_set(&A);
    levi_set_param_ui(&A, 0u, RI_CTL_LEVI_VPAN & 0xFFu, 96u);
    levi_set_param_ui(&A, 0u, RI_CTL_LEVI_VPANMODE & 0xFFu, 0u);
    levi_trigger(&A, 0u, 60u);
    stereo(&A, ob, oc);
    {
        uint32_t i;
        for (i = 0u; i < N; i++)
            od[i] = ob[i];   /* balance L (R is law-identical across modes) */
    }
    levi_init_set(&B);
    levi_set_param_ui(&B, 0u, RI_CTL_LEVI_VPAN & 0xFFu, 96u);
    levi_set_param_ui(&B, 0u, RI_CTL_LEVI_VPANMODE & 0xFFu, 1u);
    levi_trigger(&B, 0u, 60u);
    stereo(&B, oa, ob);
    RI_ASSERT(differ(od, oa, N), "power != balance");
    levi_set_param_ui(&B, 0u, RI_CTL_LEVI_VPANMODE & 0xFFu, 2u);
    levi_trigger(&B, 0u, 60u);
    stereo(&B, oa, ob);
    RI_ASSERT(differ(od, oa, N), "wide != balance");
    /* Out-of-range pan mode clamps to WIDE (2). */
    levi_set_param_ui(&B, 0u, RI_CTL_LEVI_VPANMODE & 0xFFu, 200u);
    levi_trigger(&B, 0u, 60u);
    stereo(&B, oa, ob);
    levi_set_param_ui(&B, 0u, RI_CTL_LEVI_VPANMODE & 0xFFu, 2u);
    levi_trigger(&B, 0u, 60u);
    stereo(&B, od, ob);
    RI_ASSERT(!differ(oa, od, N), "panmode clamps");

    /* Unison spread opens the image; 0 keeps it shut. */
    levi_init_set(&A);
    levi_set_alloc_ui(&A, 5u);
    levi_trigger(&A, 0u, 60u);
    levi_note_on(&A, 60u);
    stereo(&A, ob, oc);
    RI_ASSERT(!differ(ob, oc, N), "spread 0 shut");
    levi_init_set(&B);
    levi_set_alloc_ui(&B, 5u);
    levi_set_param_ui(&B, 0u, RI_CTL_LEVI_VSPREAD & 0xFFu, 127u);
    levi_note_on(&B, 60u);
    stereo(&B, oa, ob);
    RI_ASSERT(differ(oa, ob, N), "spread opens image");

    /* DO_PAN matrix destination is live. */
    levi_init_set(&A);
    levi_trigger(&A, 0u, 60u);
    stereo(&A, ob, oc);
    levi_init_set(&B);
    levi_set_mx_ui(&B, 0u, 0u, (uint8_t)ri_levi_ms_to_ui(RI_LEVI_MS_LFO0));
    levi_set_mx_ui(&B, 0u, 1u, RI_LEVI_DM_OSC1);
    levi_set_mx_ui(&B, 0u, 2u, RI_LEVI_DO_PAN);
    levi_set_mx_ui(&B, 0u, 3u, 100u);
    levi_trigger(&B, 0u, 60u);
    stereo(&B, oa, ob);
    RI_ASSERT(differ(ob, oa, N) || differ(oc, ob, N), "do_pan moves image");

    /* DM_VOICE PAN destination is live. */
    levi_init_set(&B);
    levi_trigger(&B, 0u, 60u);
    stereo(&B, od, ob);
    levi_set_mx_ui(&B, 0u, 0u, (uint8_t)ri_levi_ms_to_ui(RI_LEVI_MS_LFO0));
    levi_set_mx_ui(&B, 0u, 1u, RI_LEVI_DM_VOICE);
    levi_set_mx_ui(&B, 0u, 2u, RI_LEVI_DVO_PAN);
    levi_set_mx_ui(&B, 0u, 3u, 100u);
    levi_trigger(&B, 0u, 60u);
    stereo(&B, oa, ob);
    RI_ASSERT(differ(od, oa, N), "dm voice pan moves image");

    /* Vintage: bypass identical; degradation differs but stays bounded. */
    levi_init_set(&A);
    levi_trigger(&A, 0u, 60u);
    stereo(&A, ob, oc);
    {
        uint32_t i;
        for (i = 0u; i < N; i++)
            od[i] = ob[i];   /* clean L */
    }
    levi_init_set(&B);
    levi_set_param_ui(&B, 0u, RI_CTL_LEVI_VINTAGE & 0xFFu, 0u);
    levi_trigger(&B, 0u, 60u);
    stereo(&B, oa, ob);
    RI_ASSERT(!differ(od, oa, N), "vintage bypass");
    levi_set_param_ui(&B, 0u, RI_CTL_LEVI_VINTAGE & 0xFFu, 64u);
    levi_trigger(&B, 0u, 60u);
    stereo(&B, oa, ob);
    RI_ASSERT(differ(od, oa, N), "vintage degrades");
    RI_ASSERT(peak(oa, N) < 8.0f && peak(ob, N) < 8.0f, "vintage bounded");
    levi_set_param_ui(&B, 0u, RI_CTL_LEVI_VINTAGE & 0xFFu, 127u);
    levi_trigger(&B, 0u, 60u);
    stereo(&B, oa, ob);
    RI_ASSERT(peak(oa, N) < 8.0f && peak(ob, N) < 8.0f, "vintage extremes bounded");
    {
        /* 1-bit depth: every sample a multiple of 4 (drive the voice
         * hot: a ±0.6 voice sits below the 1-bit LSB, honestly silent). */
        uint32_t i;
        float pk;
        levi_init_set(&B);
        levi_set_param_ui(&B, 0u, RI_CTL_LEVI_VINTAGE & 0xFFu, 127u);
        levi_set_param_ui(&B, 0u, RI_CTL_LEVI_OSCLVL & 0xFFu, 127u);
        levi_set_param_ui(&B, 0u, RI_CTL_LEVI_VCALVL & 0xFFu, 127u);
        levi_set_param_ui(&B, 0u, RI_CTL_LEVI_PATCHLVL & 0xFFu, 127u);
        levi_trigger(&B, 0u, 60u);
        stereo(&B, oa, ob);
        pk = peak(oa, N) > peak(ob, N) ? peak(oa, N) : peak(ob, N);
        RI_ASSERT(pk > 0.01f, "vintage 1-bit sounds");
        for (i = 0u; i < N; i++) {
            float a = oa[i] < 0.0f ? -oa[i] : oa[i];
            float b = ob[i] < 0.0f ? -ob[i] : ob[i];
            float qa = (float)(int)(a / 4.0f + 0.5f) * 4.0f;
            float qb = (float)(int)(b / 4.0f + 0.5f) * 4.0f;
            float da = a - qa < 0.0f ? qa - a : a - qa;
            float db = b - qb < 0.0f ? qb - b : b - qb;
            if (da > 0.01f || db > 0.01f)
                break;
        }
        RI_ASSERT(i == N, "vintage 1-bit quanta");
    }

    /* Scales: C# snaps down to C in MAJOR with keylock; off stays put. */
    levi_init_set(&A);
    levi_set_param_ui(&A, 0u, RI_CTL_LEVI_VSCALE & 0xFFu, 1u); /* MAJOR */
    levi_set_param_ui(&A, 0u, RI_CTL_LEVI_VKEYLOCK & 0xFFu, 1u);
    levi_trigger(&A, 0u, 61u);
    RI_ASSERT(A.v[0].note == 60u, "c# snaps to c, got %u", A.v[0].note);
    levi_init_set(&B);
    levi_set_param_ui(&B, 0u, RI_CTL_LEVI_VSCALE & 0xFFu, 1u);
    levi_set_param_ui(&B, 0u, RI_CTL_LEVI_VKEYLOCK & 0xFFu, 0u);
    levi_trigger(&B, 0u, 61u);
    RI_ASSERT(B.v[0].note == 61u, "keylock off chromatic");
    /* Whole-tone tie goes to the lower degree. */
    levi_init_set(&B);
    levi_set_param_ui(&B, 0u, RI_CTL_LEVI_VSCALE & 0xFFu, 13u);
    levi_set_param_ui(&B, 0u, RI_CTL_LEVI_VKEYLOCK & 0xFFu, 1u);
    levi_trigger(&B, 0u, 61u);
    RI_ASSERT(B.v[0].note == 60u, "whole-tone tie lower, got %u", B.v[0].note);
    /* Out-of-range scale clamps to ARABIAN (15). */
    levi_init_set(&B);
    levi_set_param_ui(&B, 0u, RI_CTL_LEVI_VSCALE & 0xFFu, 200u);
    levi_set_param_ui(&B, 0u, RI_CTL_LEVI_VKEYLOCK & 0xFFu, 1u);
    levi_trigger(&B, 0u, 63u);
    RI_ASSERT(B.v[0].note == 64u, "scale clamps, got %u", B.v[0].note);

    /* Microtuning: table 0 inert, table 1 moves pitch (C# moves). */
    levi_init_set(&A);
    levi_trigger(&A, 0u, 61u);
    levi_init_set(&B);
    levi_set_param_ui(&B, 0u, RI_CTL_LEVI_VMICRO & 0xFFu, 1u);
    levi_trigger(&B, 0u, 61u);
    RI_ASSERT(A.v[0].st[0][0].freq != B.v[0].st[0][0].freq, "micro moves pitch");
    {
        /* Pythagorean C# is 114 cents over equal: exact ratio law. */
        float ratio = B.v[0].st[0][0].freq / A.v[0].st[0][0].freq;
        float want = 1.0681f;
        RI_ASSERT(ratio > want - 0.001f && ratio < want + 0.001f, "micro ratio %f", ratio);
    }
    RI_ASSERT(ri_levi_scale_name(0u) && ri_levi_scale_name(0u)[0] != 0, "scale name");
    RI_ASSERT(ri_levi_micro_name(0u) && ri_levi_micro_name(0u)[0] != 0, "micro name");

    /* Keys / allow-list / pages. */
    RI_ASSERT(ri_auto_allowed(RI_CTL_LEVI_VINTAGE), "vintage allowed");
    RI_ASSERT(ri_auto_allowed(RI_CTL_LEVI_VOSCPAN1 + 7u), "oscpan8 allowed");
    RI_ASSERT(!ri_auto_allowed(0x0E93u), "0x0E93 refused (P7c)");
    {
        const struct RICtlDef *d = ri_ctlreg_find((uint16_t)((RI_SEC_LEVI << 8) | 145u));
        RI_ASSERT(d && d->engine_id == RI_CTL_LEVI_VINTAGE, "reg vintage");
    }
    {
        struct RISectLevi lv;
        memset(&lv, 0, sizeof lv);
        ri_slevi_init(&lv);
        RI_ASSERT(ri_slevi_set_value(&lv, RI_SLEVI_MODULE, (int)RI_SLEVI_M_VOICE) == 1, "voice module");
        RI_ASSERT(ri_slevi_page_count(&lv) == 4u, "voice 4 pages");
        lv.page = 2u;
        RI_ASSERT(ri_slevi_enc_live(&lv, 0u) && !strcmp(ri_slevi_enc_name(&lv, 0u), "OSCPAN 1"), "p3 slot0");
        RI_ASSERT(ri_slevi_ctl_idx(&lv, RI_SLEVI_ENC0) == RI_SLEVI_VOSCPAN1, "oscpan idx");
        lv.page = 3u;
        RI_ASSERT(ri_slevi_enc_live(&lv, 0u) && !strcmp(ri_slevi_enc_name(&lv, 0u), "VINTAGE"), "p4 slot0");
    }

    /* Fail-closed: NULL voices/outs. */
    {
        float x = 1.0f, y = 1.0f;
        levi_voice_render_stereo(0, 0, SR, &x, &y);
        RI_ASSERT(x == 0.0f && y == 0.0f, "stereo null");
        levi_voice_render_sum_stereo(0, oa, ob, N, SR);
        levi_voice_render_sum_stereo(&A, 0, ob, N, SR);
    }

    /* Finite/bounded extremes storm (stereo). */
    {
        uint8_t vals[] = { 0u, 127u };
        uint32_t a, b, bad = 0;
        for (a = 0u; a < 2u; a++)
            for (b = 0u; b < 2u; b++) {
                int i;
                levi_init_set(&A);
                levi_set_param_ui(&A, 0u, RI_CTL_LEVI_VINTAGE & 0xFFu, vals[a]);
                levi_set_param_ui(&A, 0u, RI_CTL_LEVI_VDETUNE & 0xFFu, vals[b]);
                levi_set_param_ui(&A, 0u, RI_CTL_LEVI_VPAN & 0xFFu, vals[a]);
                levi_trigger(&A, 0u, (uint8_t)(20 + a * 80 + b));
                for (i = 0; i < 2400; i++) {
                    float l, r;
                    levi_voice_render_stereo(&A.v[0], 0, SR, &l, &r);
                    if (!((l > -8.0f && l < 8.0f) && (r > -8.0f && r < 8.0f)))
                        bad = 1;
                }
            }
        RI_ASSERT(!bad, "extremes bounded");
    }

    RI_RESULT("t136_levi_stereo");
}
