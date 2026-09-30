/* t131_levi_filters — Levi filters + VCA (fidelity plan P4, owner
 * 2026-09-30; manual pp. 62-70, clean-room: own designs and names).
 * Laws: all 18 digital models stay finite and bounded at every corner
 * (cutoff, resonance, drive pre/post, hot input); LP models darken and
 * HP models thin as expected; the SVF morph starts as its LP; the analog
 * 4-pole self-oscillates from ~110/128 at its cutoff; keytrack centres
 * on C2 (digital 0 %, analog 100 % by default); the VCA stage levels
 * (64 = unity) and LFO 1/2/3 amounts act; legacy FTYPE maps onto the
 * models; UI pages, texts and keys.
 */
#include <stdio.h>
#include <string.h>
#include "tests/helpers/ri_assert.h"
#include "engine/dsp/levi.h"
#include "engine/seq/autolane.h"
#include "gui/sectlevi.h"

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
        if (!(a < 1e30f))
            return 1e30f;
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

static uint32_t crossings(const float *o, uint32_t from, uint32_t n) {
    uint32_t i, c = 0u;
    for (i = from + 1u; i < from + n; i++)
        if ((o[i - 1u] < 0.0f) != (o[i] < 0.0f))
            c++;
    return c;
}

/* Brightness: slope energy over energy (high content weighs more). */
static float bright_of(const float *o, uint32_t from, uint32_t n) {
    uint32_t i;
    float d = 0.0f;
    for (i = from + 1u; i < from + n; i++)
        d += (o[i] - o[i - 1u]) * (o[i] - o[i - 1u]);
    return d / (energy(o, from, n) + 1e-20f);
}

/* A bright voice (DUO, strong modulator), filters open, one model. */
static void bright(struct RILeviSet *s, uint32_t dtype, float cut) {
    levi_init_set(s);
    levi_set_op_ui(s, 0u, 1u, RI_LEVI_OP_ENVL, 110u);
    levi_set_param_ui(s, 0u, RI_CTL_LEVI_DTYPE & 0xFFu, (uint8_t)dtype);
    levi_set_param(s, 0u, RI_LEVI_CUTOFF, cut);
    levi_set_param(s, 0u, RI_LEVI_RESO, 0.0f);
    levi_set_param(s, 0u, RI_LEVI_CUTOFF2, 18000.0f);
    levi_set_param(s, 0u, RI_LEVI_RESO2, 0.0f);
}

int main(void) {
    static const float CUTS[3] = { 40.0f, 1000.0f, 18000.0f };
    static const float RES[3] = { 0.0f, 0.5f, 1.0f };
    struct RISectLevi u;
    uint32_t t, c, r, d, k;
    char txt[32];

    /* ---- Every model, every corner: finite and bounded. ---- */
    for (t = 0u; t < RI_LEVI_NDF; t++)
        for (c = 0u; c < 3u; c++)
            for (r = 0u; r < 3u; r++)
                for (d = 0u; d < 3u; d++) {
                    uint32_t o;
                    levi_init_set(&A);
                    for (o = 1u; o < RI_LEVI_NOPS; o++)
                        levi_set_op_ui(&A, 0u, o, RI_LEVI_OP_ENVL, 127u);
                    levi_set_param_ui(&A, 0u, RI_CTL_LEVI_ALGO & 0xFFu, 9u);
                    levi_set_param_ui(&A, 0u, RI_CTL_LEVI_DTYPE & 0xFFu, (uint8_t)t);
                    levi_set_param(&A, 0u, RI_LEVI_CUTOFF, CUTS[c]);
                    levi_set_param(&A, 0u, RI_LEVI_RESO, RES[r]);
                    levi_set_param(&A, 0u, RI_LEVI_RESO2, RES[r]);
                    levi_set_param_ui(&A, 0u, RI_CTL_LEVI_DMORPH & 0xFFu, d ? 127u : 0u);
                    levi_set_param_ui(&A, 0u, RI_CTL_LEVI_DPOST & 0xFFu, d == 2u);
                    levi_set_param(&A, 0u, RI_LEVI_DRIVE, d ? 1.0f : 0.0f);
                    levi_trigger(&A, 0u, 40u);
                    render(&A, oa, 4800u);
                    RI_ASSERT(peak(oa, 0u, 4800u) < 64.0f, "%s cut %.0f res %.1f drive %u bounded %f",
                        ri_levi_df_name(t), (double)CUTS[c], (double)RES[r], d, (double)peak(oa, 0u, 4800u));
                }

    /* ---- LP models darken, HP models thin, at a low cutoff. ---- */
    for (t = 0u; t < RI_LEVI_NDF; t++) {
        int lp = t >= RI_LEVI_DF_LP_L12 && t <= RI_LEVI_DF_LP_48, hp = t >= RI_LEVI_DF_HP_GRIT && t <= RI_LEVI_DF_HP_12;
        if (!lp && !hp)
            continue;
        bright(&A, t, lp ? 300.0f : 6000.0f);
        bright(&B, t, 18000.0f);
        if (hp)
            levi_set_param(&B, 0u, RI_LEVI_CUTOFF, 40.0f);
        levi_trigger(&A, 0u, 48u);
        levi_trigger(&B, 0u, 48u);
        render(&A, oa, 9600u);
        render(&B, ob, 9600u);
        if (lp)
            RI_ASSERT(bright_of(oa, 2400u, 7200u) < 0.8f * bright_of(ob, 2400u, 7200u), "%s darkens %g < %g",
                ri_levi_df_name(t), (double)bright_of(oa, 2400u, 7200u), (double)bright_of(ob, 2400u, 7200u));
        else
            RI_ASSERT(energy(oa, 2400u, 7200u) < 0.5f * energy(ob, 2400u, 7200u), "%s thins",
                ri_levi_df_name(t));
        RI_ASSERT(energy(oa, 2400u, 7200u) > 0.0f, "%s passes something", ri_levi_df_name(t));
    }

    /* ---- The SVF morph at 0 is its LP, bit for bit. ---- */
    bright(&A, RI_LEVI_DF_SVF_LBH, 800.0f);
    bright(&B, RI_LEVI_DF_LP_12, 800.0f);
    levi_trigger(&A, 0u, 48u);
    levi_trigger(&B, 0u, 48u);
    render(&A, oa, 4800u);
    render(&B, ob, 4800u);
    RI_ASSERT(!memcmp(oa, ob, 4800u * sizeof oa[0]), "morph 0 == LP");
    levi_set_param_ui(&A, 0u, RI_CTL_LEVI_DMORPH & 0xFFu, 127u);
    levi_trigger(&A, 0u, 48u);
    render(&A, oa, 4800u);
    RI_ASSERT(memcmp(oa, ob, 4800u * sizeof oa[0]) != 0, "morph moves");

    /* ---- Drive: pre and post differ; no drive, no difference. ---- */
    bright(&A, RI_LEVI_DF_LP_L24, 1500.0f);
    bright(&B, RI_LEVI_DF_LP_L24, 1500.0f);
    levi_set_param_ui(&B, 0u, RI_CTL_LEVI_DPOST & 0xFFu, 1u);
    levi_trigger(&A, 0u, 48u);
    levi_trigger(&B, 0u, 48u);
    render(&A, oa, 4800u);
    render(&B, ob, 4800u);
    RI_ASSERT(!memcmp(oa, ob, 4800u * sizeof oa[0]), "drive 0: position inert");
    levi_set_param_ui(&A, 0u, RI_CTL_LEVI_DMORPH & 0xFFu, 127u);
    levi_set_param_ui(&B, 0u, RI_CTL_LEVI_DMORPH & 0xFFu, 127u);
    levi_trigger(&A, 0u, 48u);
    levi_trigger(&B, 0u, 48u);
    render(&A, oa, 4800u);
    render(&B, ob, 4800u);
    RI_ASSERT(memcmp(oa, ob, 4800u * sizeof oa[0]) != 0, "pre != post");
    bright(&A, RI_LEVI_DF_LP_L24, 1500.0f);                       /* no drive vs post drive */
    levi_trigger(&A, 0u, 48u);
    render(&A, oa, 4800u);
    RI_ASSERT(memcmp(oa, ob, 4800u * sizeof oa[0]) != 0, "post drive acts");

    /* ---- Vowel: order and position move the formants. ---- */
    bright(&A, RI_LEVI_DF_VOWEL, 300.0f);
    bright(&B, RI_LEVI_DF_VOWEL, 300.0f);
    levi_set_param_ui(&B, 0u, RI_CTL_LEVI_VORDER & 0xFFu, 3u);
    levi_trigger(&A, 0u, 48u);
    levi_trigger(&B, 0u, 48u);
    render(&A, oa, 4800u);
    render(&B, ob, 4800u);
    RI_ASSERT(energy(oa, 0u, 4800u) > 0.0f && memcmp(oa, ob, 4800u * sizeof oa[0]) != 0, "vowel order");

    /* ---- Analog 4-pole self-oscillates from ~110/128, at its cutoff. ---- */
    for (k = 0u; k < 2u; k++) {
        struct RILeviSet *s = k ? &B : &A;
        levi_init_set(s);
        levi_set_op_ui(s, 0u, 0u, RI_LEVI_OP_ENVL, 65u);          /* a whisper of input */
        levi_set_op_ui(s, 0u, 1u, RI_LEVI_OP_ENVL, 64u);
        levi_set_param_ui(s, 0u, RI_CTL_LEVI_DTYPE & 0xFFu, RI_LEVI_DF_LP_6);
        levi_set_param(s, 0u, RI_LEVI_CUTOFF, 18000.0f);
        levi_set_param(s, 0u, RI_LEVI_CUTOFF2, 1000.0f);
        levi_set_param_ui(s, 0u, RI_CTL_LEVI_AKEYTRK & 0xFFu, 64u);
        levi_set_param_ui(s, 0u, RI_CTL_LEVI_RESO2 & 0xFFu, k ? 115u : 105u);
        levi_trigger(s, 0u, 36u);
        render(s, k ? ob : oa, N);
    }
    RI_ASSERT(peak(ob, N / 2u, N / 2u) > 0.2f && peak(ob, N / 2u, N / 2u) > 10.0f * peak(oa, N / 2u, N / 2u),
        "115 oscillates %f, 105 does not %f", (double)peak(ob, N / 2u, N / 2u), (double)peak(oa, N / 2u, N / 2u));
    k = crossings(ob, N / 2u, N / 2u);                            /* 0.25 s at 1 kHz: ~500 */
    RI_ASSERT(k > 425u && k < 575u, "pitch at the cutoff: %u crossings", k);
    /* The threshold holds high up too (zero-delay loop): 105 rings out at 8 kHz. */
    levi_set_param(&A, 0u, RI_LEVI_CUTOFF2, 8000.0f);
    levi_trigger(&A, 0u, 36u);
    render(&A, oa, N);
    RI_ASSERT(peak(oa, N / 2u, N / 2u) < 0.05f, "105 at 8 kHz does not oscillate: %f", (double)peak(oa, N / 2u, N / 2u));

    /* ---- Keytrack around C2. ---- */
    levi_init_set(&A);
    levi_trigger(&A, 0u, 84u);
    RI_ASSERT(A.v[0].dktm == 1.0f && A.v[0].aktm > 15.9f && A.v[0].aktm < 16.1f, "defaults: digital 0, analog 100 %%");
    levi_set_param_ui(&A, 0u, RI_CTL_LEVI_DKEYTRK & 0xFFu, 0u);  /* -200 % */
    levi_trigger(&A, 0u, 48u);
    RI_ASSERT(A.v[0].dktm > 0.249f && A.v[0].dktm < 0.251f, "-200 %% one octave up = 2 down: %f", (double)A.v[0].dktm);
    levi_trigger(&A, 0u, 36u);
    RI_ASSERT(A.v[0].dktm == 1.0f && A.v[0].aktm == 1.0f, "C2 is the centre");

    /* ---- VCA levels: 0 silences each stage, patch level scales. ---- */
    {
        static const uint32_t STG[3] = { RI_CTL_LEVI_OSCLVL, RI_CTL_LEVI_DLEVEL, RI_CTL_LEVI_VCALVL };
        for (k = 0u; k < 3u; k++) {
            bright(&A, RI_LEVI_DF_LP_12, 5000.0f);
            levi_set_param_ui(&A, 0u, STG[k] & 0xFFu, 0u);
            levi_trigger(&A, 0u, 48u);
            render(&A, oa, 4800u);
            RI_ASSERT(peak(oa, 0u, 4800u) == 0.0f, "stage %u level 0 silent", k);
        }
    }
    bright(&A, RI_LEVI_DF_LP_12, 5000.0f);
    bright(&B, RI_LEVI_DF_LP_12, 5000.0f);
    levi_set_param_ui(&B, 0u, RI_CTL_LEVI_PATCHLVL & 0xFFu, 32u);
    levi_trigger(&A, 0u, 48u);
    levi_trigger(&B, 0u, 48u);
    render(&A, oa, 4800u);
    render(&B, ob, 4800u);
    RI_ASSERT(peak(oa, 0u, 4800u) > 0.0f, "sounds");
    for (k = 0u; k < 4800u; k++)
        if (oa[k] * 0.5f - ob[k] > 1e-6f || ob[k] - oa[k] * 0.5f > 1e-6f)
            break;
    RI_ASSERT(k == 4800u, "patch level 32 = half, sample %u", k);

    /* ---- LFO 1/2/3 amounts act. ---- */
    {
        static const uint32_t AMT[3] = { RI_CTL_LEVI_DLFO1, RI_CTL_LEVI_ALFO2, RI_CTL_LEVI_VLFO3 };
        for (k = 0u; k < 3u; k++) {
            bright(&A, RI_LEVI_DF_LP_12, 800.0f);
            bright(&B, RI_LEVI_DF_LP_12, 800.0f);
            levi_set_param(&A, 0u, RI_LEVI_CUTOFF2, 800.0f);
            levi_set_param(&B, 0u, RI_LEVI_CUTOFF2, 800.0f);
            levi_set_param_ui(&B, 0u, AMT[k] & 0xFFu, 127u);
            levi_trigger(&A, 0u, 48u);
            levi_trigger(&B, 0u, 48u);
            render(&A, oa, N);
            render(&B, ob, N);
            RI_ASSERT(memcmp(oa, ob, sizeof oa) != 0 && peak(ob, 0u, N) < 64.0f, "LFO amount %u acts", k + 1u);
        }
    }

    /* ---- Legacy FTYPE, clamps, keys. ---- */
    levi_init_set(&A);
    RI_ASSERT(levi_set_param_ui(&A, 0u, RI_CTL_LEVI_FTYPE & 0xFFu, 1u) == 0 && A.v[0].dtype == RI_LEVI_DF_HP_12,
        "legacy HP");
    RI_ASSERT(levi_set_param_ui(&A, 0u, RI_CTL_LEVI_FTYPE & 0xFFu, 3u) == 0 && A.v[0].dtype == RI_LEVI_DF_SVF_LNH &&
        A.v[0].dmorph == 64u, "legacy notch = LNH middle");
    RI_ASSERT(levi_set_param_ui(&A, 0u, RI_CTL_LEVI_DTYPE & 0xFFu, 200u) == 0 && A.v[0].dtype == RI_LEVI_NDF - 1u,
        "type clamps");
    for (k = RI_CTL_LEVI_DTYPE; k <= RI_CTL_LEVI_VLFO3; k++)
        RI_ASSERT(ri_auto_allowed((uint16_t)k), "key %04x allowed", k);

    /* ---- UI: two digital pages, model-dependent slots, texts. ---- */
    ri_slevi_init(&u);
    RI_ASSERT(u.val[RI_SLEVI_DTYPE] == (int)RI_LEVI_DF_LP_12, "default model LP 12");
    ri_slevi_set_value(&u, RI_SLEVI_MODULE, (int)RI_SLEVI_M_DFILT);
    RI_ASSERT(ri_slevi_page_count(&u) == 2u && !strcmp(ri_slevi_enc_name(&u, 1u), "DRIVE"), "drive on LP");
    ri_slevi_set_value(&u, RI_SLEVI_DTYPE, (int)RI_LEVI_DF_SVF_LNH);
    RI_ASSERT(!strcmp(ri_slevi_enc_name(&u, 1u), "MORPH"), "morph on SVF");
    ri_slevi_enc_text(&u, 0u, txt, sizeof txt);
    RI_ASSERT(!strcmp(txt, "SVF LP-NO-HP"), "model text %s", txt);
    ri_slevi_enc_text(&u, 7u, txt, sizeof txt);
    RI_ASSERT(!strcmp(txt, "0%"), "keytrack text %s", txt);
    ri_slevi_press(&u, RI_SLEVI_PAGEDN);
    RI_ASSERT(!strcmp(ri_slevi_page_title(&u), "DIGITAL FILTER  2/2"), "page 2");
    RI_ASSERT(!ri_slevi_enc_live(&u, 4u) && !ri_slevi_enc_live(&u, 1u), "no vowel order, no drive pos on SVF");
    ri_slevi_set_value(&u, RI_SLEVI_DTYPE, (int)RI_LEVI_DF_VOWEL);
    RI_ASSERT(ri_slevi_ctl_idx(&u, RI_SLEVI_ENC0 + 4u) == RI_SLEVI_VORDER, "vowel order on vowel");
    ri_slevi_set_value(&u, RI_SLEVI_VORDER, 1);
    ri_slevi_enc_text(&u, 4u, txt, sizeof txt);
    RI_ASSERT(!strcmp(txt, "UOIEA"), "order text %s", txt);
    ri_slevi_set_value(&u, RI_SLEVI_DTYPE, (int)RI_LEVI_DF_LP_L24);
    RI_ASSERT(ri_slevi_ctl_idx(&u, RI_SLEVI_ENC0 + 1u) == RI_SLEVI_DPOST, "drive pos on ladder");
    ri_slevi_enc_text(&u, 7u, txt, sizeof txt);
    RI_ASSERT(!strcmp(txt, "64"), "level text %s", txt);
    ri_slevi_set_value(&u, RI_SLEVI_MODULE, (int)RI_SLEVI_M_VCA);
    RI_ASSERT(ri_slevi_ctl_idx(&u, RI_SLEVI_ENC0 + 3u) == RI_SLEVI_PATCHLVL && !ri_slevi_enc_live(&u, 7u),
        "vca page");
    RI_ASSERT(ri_slevi_legacy(RI_SLEVI_FTYPE) && ri_slevi_page_reaches(RI_SLEVI_VORDER) &&
        ri_slevi_page_reaches(RI_SLEVI_DPOST), "legacy / reach");
    RI_RESULT("levi_filters");
}
