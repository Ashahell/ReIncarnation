/* t139_levi_reverb — Levi P7b device reverb (types + freeze).
 * Laws: bypassed/defaults bit-identical to clean; dry-only identical;
 * tails outlive the release; max decay finite; 4 types differ; predelay
 * shifts onset; tone/damps shape the tail; freeze sustains bounded
 * (vs unfrozen decay); dry/wet; DM_REVERB routes; keys/pages/texts;
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
#define NL (48000u * 3u)
#define NL2 (48000u * 4u)

static struct RILeviSet A, B;
static float oa[N], ob[N], oc[N];
static float la[NL2], lb[NL2], lc[NL2], ld[NL2];

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

static float energy(const float *o, uint32_t from, uint32_t n) {
    uint32_t i;
    float e = 0.0f;
    for (i = from; i < from + n; i++)
        e += o[i] * o[i];
    return e;
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

static float bright_of(const float *o, uint32_t from, uint32_t n) {
    uint32_t i;
    float d = 0.0f, e = 0.0f;
    for (i = from; i < from + n; i++)
        e += o[i] * o[i];
    for (i = from + 1u; i < from + n; i++)
        d += (o[i] - o[i - 1u]) * (o[i] - o[i - 1u]);
    return d / (e + 1e-20f);
}

static void rev_on(struct RILeviSet *s) {
    /* Panel ON (FXREV row 66, 1 = on). */
    levi_set_param_ui(s, 0u, RI_CTL_LEVI_RBYPASS & 0xFFu, 1u);
}

int main(void) {
    uint32_t i;

    /* Bypassed by default: bit-identical to a clean twin. */
    levi_init_set(&A);
    levi_trigger(&A, 0u, 60u);
    stereo(&A, oa, ob, N);
    levi_init_set(&B);
    levi_trigger(&B, 0u, 60u);
    stereo(&B, la, lb, N);
    RI_ASSERT(!differ(oa, la, N) && !differ(ob, lb, N), "bypass default");

    /* Dry-only (wet 0, on) == bypassed. */
    levi_init_set(&B);
    rev_on(&B);
    levi_set_param_ui(&B, 0u, RI_CTL_LEVI_RDRYWET & 0xFFu, 0u);
    levi_trigger(&B, 0u, 60u);
    stereo(&B, la, lb, N);
    RI_ASSERT(!differ(oa, la, N), "dry-only identical");

    /* Tails outlive the release (HALL, long time, full wet). */
    levi_init_set(&B);
    rev_on(&B);
    levi_set_param_ui(&B, 0u, RI_CTL_LEVI_RTYPE & 0xFFu, RI_LEVI_RT_HALL);
    levi_set_param_ui(&B, 0u, RI_CTL_LEVI_RTIME & 0xFFu, 127u);
    levi_set_param_ui(&B, 0u, RI_CTL_LEVI_RDRYWET & 0xFFu, 127u);
    levi_trigger(&B, 0u, 60u);
    stereo(&B, la, lb, 2400u);
    levi_release(&B, 0u);
    stereo(&B, la + 2400u, lb + 2400u, NL - 2400u);
    RI_ASSERT(energy(la, 96000u, 9600u) > 0.0f, "tail lives");

    /* Max decay finite over 5 s (no runaway at any corner). */
    levi_init_set(&B);
    rev_on(&B);
    levi_set_param_ui(&B, 0u, RI_CTL_LEVI_RTYPE & 0xFFu, RI_LEVI_RT_HALL);
    levi_set_param_ui(&B, 0u, RI_CTL_LEVI_RTIME & 0xFFu, 127u);
    levi_set_param_ui(&B, 0u, RI_CTL_LEVI_RDRYWET & 0xFFu, 127u);
    levi_trigger(&B, 0u, 60u);
    stereo(&B, la, lb, NL);
    stereo(&B, la, lb, NL);
    RI_ASSERT(peak(la, NL) < 8.0f && peak(lb, NL) < 8.0f, "decay bounded %f",
        peak(la, NL) > peak(lb, NL) ? peak(la, NL) : peak(lb, NL));

    /* Type clamps to CHAMBER (3) at the engine door (hard filter: clean
     * and pingpong coincide on dual-mono, so compare wet tails). */
    levi_init_set(&A);
    rev_on(&A);
    levi_set_param_ui(&A, 0u, RI_CTL_LEVI_RTYPE & 0xFFu, 200u);
    levi_set_param_ui(&A, 0u, RI_CTL_LEVI_RTIME & 0xFFu, 64u);
    levi_set_param_ui(&A, 0u, RI_CTL_LEVI_RDRYWET & 0xFFu, 127u);
    levi_trigger(&A, 0u, 60u);
    stereo(&A, oa, ob, N);
    levi_init_set(&B);
    rev_on(&B);
    levi_set_param_ui(&B, 0u, RI_CTL_LEVI_RTYPE & 0xFFu, 3u);
    levi_set_param_ui(&B, 0u, RI_CTL_LEVI_RTIME & 0xFFu, 64u);
    levi_set_param_ui(&B, 0u, RI_CTL_LEVI_RDRYWET & 0xFFu, 127u);
    levi_trigger(&B, 0u, 60u);
    stereo(&B, la, lb, N);
    RI_ASSERT(!differ(oa, la, N), "rtype clamps");
    {
        float p0[N];
        uint32_t t, first = 1u;
        for (t = 0u; t < RI_LEVI_RT_N; t++) {
            levi_init_set(&B);
            rev_on(&B);
            levi_set_param_ui(&B, 0u, RI_CTL_LEVI_RTYPE & 0xFFu, (uint8_t)t);
            levi_set_param_ui(&B, 0u, RI_CTL_LEVI_RTIME & 0xFFu, 64u);
            levi_set_param_ui(&B, 0u, RI_CTL_LEVI_RDRYWET & 0xFFu, 127u);
            levi_trigger(&B, 0u, 60u);
            stereo(&B, la, lb, N);
            if (first) {
                for (i = 0u; i < N; i++)
                    p0[i] = la[i];
                first = 0u;
            } else {
                RI_ASSERT(differ(p0, la, N), "type %u differs", t);
            }
        }
    }

    /* Predelay shifts the wet onset (dry identical). */
    levi_init_set(&A);
    rev_on(&A);
    levi_set_param_ui(&A, 0u, RI_CTL_LEVI_RPREDLY & 0xFFu, 0u);
    levi_set_param_ui(&A, 0u, RI_CTL_LEVI_RTIME & 0xFFu, 64u);
    levi_set_param_ui(&A, 0u, RI_CTL_LEVI_RDRYWET & 0xFFu, 127u);
    levi_trigger(&A, 0u, 60u);
    stereo(&A, oa, ob, N);
    levi_init_set(&B);
    rev_on(&B);
    levi_set_param_ui(&B, 0u, RI_CTL_LEVI_RPREDLY & 0xFFu, 127u);
    levi_set_param_ui(&B, 0u, RI_CTL_LEVI_RTIME & 0xFFu, 64u);
    levi_set_param_ui(&B, 0u, RI_CTL_LEVI_RDRYWET & 0xFFu, 127u);
    levi_trigger(&B, 0u, 60u);
    stereo(&B, la, lb, N);
    RI_ASSERT(differ(oa, la, N), "predelay shifts onset");

    /* Tone darkens; hi-damp darkens; lo-damp thins (brightens ratio).
     * Fresh sets both sides: retriggers ramp envelopes from running
     * values, so reused voices carry history. */
    levi_init_set(&A);
    rev_on(&A);
    levi_set_param_ui(&A, 0u, RI_CTL_LEVI_RTONE & 0xFFu, 127u);
    levi_set_param_ui(&A, 0u, RI_CTL_LEVI_RTIME & 0xFFu, 64u);
    levi_set_param_ui(&A, 0u, RI_CTL_LEVI_RDRYWET & 0xFFu, 127u);
    levi_trigger(&A, 0u, 60u);
    stereo(&A, oa, ob, N);
    levi_init_set(&B);
    rev_on(&B);
    levi_set_param_ui(&B, 0u, RI_CTL_LEVI_RTONE & 0xFFu, 0u);
    levi_set_param_ui(&B, 0u, RI_CTL_LEVI_RTIME & 0xFFu, 64u);
    levi_set_param_ui(&B, 0u, RI_CTL_LEVI_RDRYWET & 0xFFu, 127u);
    levi_trigger(&B, 0u, 60u);
    stereo(&B, la, lb, N);
    RI_ASSERT(bright_of(lb, 0u, N) < bright_of(ob, 0u, N), "tone darkens");
    RI_ASSERT(bright_of(la, 0u, N) < bright_of(oa, 0u, N), "tone darkens L");
    levi_init_set(&B);
    rev_on(&B);
    levi_set_param_ui(&B, 0u, RI_CTL_LEVI_RTONE & 0xFFu, 127u);
    levi_set_param_ui(&B, 0u, RI_CTL_LEVI_RHIDAMP & 0xFFu, 0u);
    levi_set_param_ui(&B, 0u, RI_CTL_LEVI_RTIME & 0xFFu, 64u);
    levi_set_param_ui(&B, 0u, RI_CTL_LEVI_RDRYWET & 0xFFu, 127u);
    levi_trigger(&B, 0u, 60u);
    stereo(&B, la, lb, N);
    RI_ASSERT(bright_of(lb, 0u, N) < bright_of(ob, 0u, N), "hi-damp darkens");
    RI_ASSERT(bright_of(la, 0u, N) < bright_of(oa, 0u, N), "hi-damp darkens L");
    levi_init_set(&B);
    rev_on(&B);
    levi_set_param_ui(&B, 0u, RI_CTL_LEVI_RTONE & 0xFFu, 127u);
    levi_set_param_ui(&B, 0u, RI_CTL_LEVI_RHIDAMP & 0xFFu, 127u);
    levi_set_param_ui(&B, 0u, RI_CTL_LEVI_RLODAMP & 0xFFu, 127u);
    levi_set_param_ui(&B, 0u, RI_CTL_LEVI_RTIME & 0xFFu, 64u);
    levi_set_param_ui(&B, 0u, RI_CTL_LEVI_RDRYWET & 0xFFu, 127u);
    levi_trigger(&B, 0u, 60u);
    stereo(&B, la, lb, N);
    RI_ASSERT(bright_of(lb, 0u, N) / (bright_of(ob, 0u, N) + 1e-20f) > 1.2f, "lo-damp thins %f/%f",
        bright_of(lb, 0u, N), bright_of(ob, 0u, N));
    RI_ASSERT(bright_of(la, 0u, N) / (bright_of(oa, 0u, N) + 1e-20f) > 1.2f, "lo-damp thins L");

    /* Freeze sustains bounded (vs unfrozen decay); both released. */
    levi_init_set(&A);
    rev_on(&A);
    levi_set_param_ui(&A, 0u, RI_CTL_LEVI_RTIME & 0xFFu, 64u);
    levi_set_param_ui(&A, 0u, RI_CTL_LEVI_RDRYWET & 0xFFu, 127u);
    levi_trigger(&A, 0u, 60u);
    stereo(&A, la, lb, 48000u);
    levi_release(&A, 0u);
    levi_set_param_ui(&A, 0u, RI_CTL_LEVI_RFREEZE & 0xFFu, 1u);
    stereo(&A, la, lb, NL2 - 48000u);
    levi_init_set(&B);
    rev_on(&B);
    levi_set_param_ui(&B, 0u, RI_CTL_LEVI_RTIME & 0xFFu, 64u);
    levi_set_param_ui(&B, 0u, RI_CTL_LEVI_RDRYWET & 0xFFu, 127u);
    levi_trigger(&B, 0u, 60u);
    stereo(&B, lc, ld, 48000u);
    levi_release(&B, 0u);
    stereo(&B, lc, ld, NL2 - 48000u);
    RI_ASSERT(energy(la, 120000u, 9600u) > energy(lc, 120000u, 9600u), "freeze sustains");
    RI_ASSERT(peak(la, NL2 - 48000u) < 8.0f && peak(lb, NL2 - 48000u) < 8.0f, "freeze bounded");
    {
        /* Held drone: the sustained tail is flat (slow decay fails). */
        float e1 = energy(la, 120000u, 9600u);
        float e2 = energy(la, 134400u, 9600u);
        RI_ASSERT(e1 > 0.0f && e2 / (e1 + 1e-20f) > 0.8f && e2 / (e1 + 1e-20f) < 1.25f,
            "freeze flat %f/%f", e2, e1);
    }

    /* DM_REVERB module: 5 params; LFO->TIME moves the tail. */
    RI_ASSERT(ri_levi_dm_nparam(RI_LEVI_DM_REVERB) == 5u, "reverb nparam");
    levi_init_set(&A);
    rev_on(&A);
    levi_set_param_ui(&A, 0u, RI_CTL_LEVI_RTIME & 0xFFu, 64u);
    levi_set_param_ui(&A, 0u, RI_CTL_LEVI_RDRYWET & 0xFFu, 127u);
    levi_trigger(&A, 0u, 60u);
    stereo(&A, oa, ob, N);
    levi_init_set(&B);
    rev_on(&B);
    levi_set_param_ui(&B, 0u, RI_CTL_LEVI_RTIME & 0xFFu, 64u);
    levi_set_param_ui(&B, 0u, RI_CTL_LEVI_RDRYWET & 0xFFu, 127u);
    levi_set_mx_ui(&B, 0u, 0u, (uint8_t)ri_levi_ms_to_ui(RI_LEVI_MS_LFO0));
    levi_set_mx_ui(&B, 0u, 1u, RI_LEVI_DM_REVERB);
    levi_set_mx_ui(&B, 0u, 2u, RI_LEVI_DR_TIME);
    levi_set_mx_ui(&B, 0u, 3u, 100u);
    levi_trigger(&B, 0u, 60u);
    stereo(&B, la, lb, N);
    RI_ASSERT(differ(oa, la, N), "dm reverb time moves tail");

    /* Chain order: delay + reverb both on differs from each alone. */
    levi_init_set(&A);
    rev_on(&A);
    levi_set_param_ui(&A, 0u, RI_CTL_LEVI_DLYTIME & 0xFFu, 84u);
    levi_set_param_ui(&A, 0u, RI_CTL_LEVI_DLYFB & 0xFFu, 64u);
    levi_set_param_ui(&A, 0u, RI_CTL_LEVI_DLYDRYWET & 0xFFu, 127u);
    levi_set_param_ui(&A, 0u, RI_CTL_LEVI_DBYPASS & 0xFFu, 1u);
    levi_set_param_ui(&A, 0u, RI_CTL_LEVI_RTIME & 0xFFu, 64u);
    levi_set_param_ui(&A, 0u, RI_CTL_LEVI_RDRYWET & 0xFFu, 127u);
    levi_trigger(&A, 0u, 60u);
    stereo(&A, oa, ob, N);
    levi_init_set(&B);
    rev_on(&B);
    levi_set_param_ui(&B, 0u, RI_CTL_LEVI_DLYTIME & 0xFFu, 84u);
    levi_set_param_ui(&B, 0u, RI_CTL_LEVI_DLYFB & 0xFFu, 64u);
    levi_set_param_ui(&B, 0u, RI_CTL_LEVI_DLYDRYWET & 0xFFu, 127u);
    levi_trigger(&B, 0u, 60u);
    stereo(&B, la, lb, N);
    RI_ASSERT(differ(oa, la, N), "chain order matters");

    /* Keys / allow-list / pages / texts. */
    RI_ASSERT(ri_auto_allowed(RI_CTL_LEVI_RTYPE), "rtype allowed");
    RI_ASSERT(ri_auto_allowed(RI_CTL_LEVI_RBYPASS), "rbypass allowed");
    RI_ASSERT(ri_auto_allowed(RI_CTL_LEVI_RFREEZE), "rfreeze allowed");
    RI_ASSERT(!ri_auto_allowed(0x0EC5u), "0x0EC5 refused (P9b)");
    {
        const struct RICtlDef *d = ri_ctlreg_find((uint16_t)((RI_SEC_LEVI << 8) | RI_SLEVI_FXREV));
        RI_ASSERT(d && d->engine_id == RI_CTL_LEVI_RBYPASS, "fxrev binds bypass");
    }
    {
        struct RISectLevi lv;
        char tx[16];
        memset(&lv, 0, sizeof lv);
        ri_slevi_init(&lv);
        RI_ASSERT(ri_slevi_set_value(&lv, RI_SLEVI_MODULE, (int)RI_SLEVI_M_REVERB) == 1, "reverb module");
        RI_ASSERT(ri_slevi_page_count(&lv) == 2u, "reverb 2 pages");
        RI_ASSERT(ri_slevi_enc_live(&lv, 0u) && !strcmp(ri_slevi_enc_name(&lv, 0u), "ON"), "slot0 on");
        RI_ASSERT(ri_slevi_ctl_idx(&lv, RI_SLEVI_ENC0) == RI_SLEVI_FXREV, "on idx");
        RI_ASSERT(ri_slevi_enc_live(&lv, 1u) && !strcmp(ri_slevi_enc_name(&lv, 1u), "TYPE"), "slot1 type");
        RI_ASSERT(ri_slevi_press(&lv, RI_SLEVI_PAGEDN) == 1, "page 2");
        RI_ASSERT(ri_slevi_enc_live(&lv, 0u) && !strcmp(ri_slevi_enc_name(&lv, 0u), "FREEZE"), "p2 freeze");
        ri_slevi_enc_text(&lv, 0u, tx, sizeof tx);
        RI_ASSERT(!strcmp(tx, "OFF"), "freeze text %s", tx);
        lv.page = 0u;
        ri_slevi_enc_text(&lv, 1u, tx, sizeof tx);
        RI_ASSERT(!strcmp(tx, "ROOM"), "type text %s", tx);
    }

    /* Null-safe entry points. */
    {
        float x = 1.0f, y = 1.0f;
        levi_fx_reverb(0, SR, 0.5f, 0.5f, &x, &y);
        RI_ASSERT(x == 0.5f && y == 0.5f, "reverb null passthrough");
        levi_fx_reverb_bind(0);
    }

    /* Finite/bounded extremes storm (all types, max everything). */
    {
        uint32_t t, bad = 0;
        for (t = 0u; t < RI_LEVI_RT_N; t++) {
            int i;
            levi_init_set(&A);
            rev_on(&A);
            levi_set_param_ui(&A, 0u, RI_CTL_LEVI_RTYPE & 0xFFu, (uint8_t)t);
            levi_set_param_ui(&A, 0u, RI_CTL_LEVI_RTIME & 0xFFu, 127u);
            levi_set_param_ui(&A, 0u, RI_CTL_LEVI_RPREDLY & 0xFFu, 127u);
            levi_set_param_ui(&A, 0u, RI_CTL_LEVI_RDRYWET & 0xFFu, 127u);
            levi_trigger(&A, 0u, (uint8_t)(20 + t * 20u));
            for (i = 0; i < 4800; i++) {
                float l, r;
                levi_voice_render_sum_stereo(&A, ob, oc, 1u, SR);
                l = ob[0];
                r = oc[0];
                if (!((l > -8.0f && l < 8.0f) && (r > -8.0f && r < 8.0f)))
                    bad = 1;
            }
        }
        RI_ASSERT(!bad, "extremes bounded");
    }

    RI_RESULT("t139_levi_reverb");
}
