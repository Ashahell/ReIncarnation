/* t138_levi_delay — Levi P7a device delay + FX framework.
 * Laws: bypassed/we-defaults bit-identical to clean; dry-only identical;
 * echoes arrive late; feedback bounded at max, single echo at 0; tones
 * darken; 4 types differ; pingpong crosses (single echo, hard-panned);
 * BPM stored inert (P8 clock); DM_DELAY routes; keys/pages; finite.
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

static struct RILeviSet A, B;
static float oa[N], ob[N], oc[N];
static float la[NL], lb[NL];

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

static void delay_on(struct RILeviSet *s) {
    /* Panel ON (FXDLY row 65, 1 = on). */
    levi_set_param_ui(s, 0u, RI_CTL_LEVI_DBYPASS & 0xFFu, 1u);
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
    delay_on(&B);
    levi_set_param_ui(&B, 0u, RI_CTL_LEVI_DLYDRYWET & 0xFFu, 0u);
    levi_trigger(&B, 0u, 60u);
    stereo(&B, la, lb, N);
    RI_ASSERT(!differ(oa, la, N), "dry-only identical");

    /* Echoes arrive late: 250 ms delay, note released early, tail lives. */
    levi_init_set(&B);
    delay_on(&B);
    levi_set_param_ui(&B, 0u, RI_CTL_LEVI_DLYTIME & 0xFFu, 84u); /* ~250 ms */
    levi_set_param_ui(&B, 0u, RI_CTL_LEVI_DLYFB & 0xFFu, 64u);
    levi_set_param_ui(&B, 0u, RI_CTL_LEVI_DLYDRYWET & 0xFFu, 127u);
    levi_trigger(&B, 0u, 60u);
    stereo(&B, la, lb, 2400u);
    levi_release(&B, 0u);
    stereo(&B, la + 2400u, lb + 2400u, NL - 2400u);
    RI_ASSERT(energy(la, 48000u, 9600u) > 0.0f, "echo tail lives");

    /* Max feedback bounded over 5 s (no runaway at any corner). */
    levi_init_set(&B);
    delay_on(&B);
    levi_set_param_ui(&B, 0u, RI_CTL_LEVI_DLYFB & 0xFFu, 127u);
    levi_set_param_ui(&B, 0u, RI_CTL_LEVI_DLYDRYWET & 0xFFu, 127u);
    levi_trigger(&B, 0u, 60u);
    stereo(&B, la, lb, NL);
    RI_ASSERT(peak(la, NL) < 8.0f && peak(lb, NL) < 8.0f, "feedback bounded %f",
        peak(la, NL) > peak(lb, NL) ? peak(la, NL) : peak(lb, NL));

    /* Zero feedback: one echo, then (near) silence. */
    levi_init_set(&B);
    delay_on(&B);
    levi_set_param_ui(&B, 0u, RI_CTL_LEVI_DLYTIME & 0xFFu, 84u);
    levi_set_param_ui(&B, 0u, RI_CTL_LEVI_DLYFB & 0xFFu, 0u);
    levi_set_param_ui(&B, 0u, RI_CTL_LEVI_DLYDRYWET & 0xFFu, 127u);
    levi_trigger(&B, 0u, 60u);
    stereo(&B, la, lb, 2400u);
    levi_release(&B, 0u);
    stereo(&B, la + 2400u, lb + 2400u, NL - 2400u);
    RI_ASSERT(peak(la + 96000u, NL - 96000u) < 1e-6f, "single echo then silent");

    /* Wet tone darkens (full wet, mid feedback). */
    levi_init_set(&A);
    delay_on(&A);
    levi_set_param_ui(&A, 0u, RI_CTL_LEVI_DLYWTONE & 0xFFu, 127u);
    levi_set_param_ui(&A, 0u, RI_CTL_LEVI_DLYFB & 0xFFu, 64u);
    levi_set_param_ui(&A, 0u, RI_CTL_LEVI_DLYDRYWET & 0xFFu, 127u);
    levi_trigger(&A, 0u, 60u);
    stereo(&A, oa, ob, N);
    levi_init_set(&B);
    delay_on(&B);
    levi_set_param_ui(&B, 0u, RI_CTL_LEVI_DLYWTONE & 0xFFu, 0u);
    levi_set_param_ui(&B, 0u, RI_CTL_LEVI_DLYFB & 0xFFu, 64u);
    levi_set_param_ui(&B, 0u, RI_CTL_LEVI_DLYDRYWET & 0xFFu, 127u);
    levi_trigger(&B, 0u, 60u);
    stereo(&B, la, lb, N);
    RI_ASSERT(bright_of(lb, 0u, N) < bright_of(ob, 0u, N), "wet tone darkens R");
    RI_ASSERT(bright_of(la, 0u, N) < bright_of(oa, 0u, N), "wet tone darkens L");

    /* Loop tone darkens the recirculating tail. */
    levi_init_set(&A);
    delay_on(&A);
    levi_set_param_ui(&A, 0u, RI_CTL_LEVI_DLYFBTONE & 0xFFu, 127u);
    levi_set_param_ui(&A, 0u, RI_CTL_LEVI_DLYFB & 0xFFu, 64u);
    levi_set_param_ui(&A, 0u, RI_CTL_LEVI_DLYDRYWET & 0xFFu, 127u);
    levi_trigger(&A, 0u, 60u);
    stereo(&A, oa, ob, N);
    levi_init_set(&B);
    delay_on(&B);
    levi_set_param_ui(&B, 0u, RI_CTL_LEVI_DLYFBTONE & 0xFFu, 0u);
    levi_set_param_ui(&B, 0u, RI_CTL_LEVI_DLYFB & 0xFFu, 64u);
    levi_set_param_ui(&B, 0u, RI_CTL_LEVI_DLYDRYWET & 0xFFu, 127u);
    levi_trigger(&B, 0u, 60u);
    stereo(&B, la, lb, N);
    RI_ASSERT(bright_of(lb, 0u, N) < bright_of(ob, 0u, N), "loop tone darkens");

    /* Matrix feedback through the depth stays bounded (clamp holds).
     * Base at max so the LFO would push past 0.95 without the clamp. */
    levi_init_set(&B);
    delay_on(&B);
    levi_set_param_ui(&B, 0u, RI_CTL_LEVI_DLYFB & 0xFFu, 127u);
    levi_set_param_ui(&B, 0u, RI_CTL_LEVI_DLYDRYWET & 0xFFu, 127u);
    levi_set_mx_ui(&B, 0u, 0u, (uint8_t)ri_levi_ms_to_ui(RI_LEVI_MS_LFO0));
    levi_set_mx_ui(&B, 0u, 1u, RI_LEVI_DM_DELAY);
    levi_set_mx_ui(&B, 0u, 2u, RI_LEVI_DD_FEEDBACK);
    levi_set_mx_ui(&B, 0u, 3u, 100u);
    levi_trigger(&B, 0u, 60u);
    stereo(&B, la, lb, NL);
    RI_ASSERT(peak(la, NL) < 8.0f && peak(lb, NL) < 8.0f, "matrix feedback bounded");

    /* Type clamps to PINGPONG (3) at the engine door (hard-left so
     * clean and pingpong differ). */
    levi_init_set(&A);
    delay_on(&A);
    levi_set_param_ui(&A, 0u, RI_CTL_LEVI_DLYTYPE & 0xFFu, 200u);
    levi_set_param_ui(&A, 0u, RI_CTL_LEVI_DLYFB & 0xFFu, 64u);
    levi_set_param_ui(&A, 0u, RI_CTL_LEVI_DLYDRYWET & 0xFFu, 127u);
    levi_set_param_ui(&A, 0u, RI_CTL_LEVI_VPAN & 0xFFu, 0u);
    levi_trigger(&A, 0u, 60u);
    stereo(&A, oa, ob, N);
    levi_init_set(&B);
    delay_on(&B);
    levi_set_param_ui(&B, 0u, RI_CTL_LEVI_DLYTYPE & 0xFFu, 3u);
    levi_set_param_ui(&B, 0u, RI_CTL_LEVI_DLYFB & 0xFFu, 64u);
    levi_set_param_ui(&B, 0u, RI_CTL_LEVI_DLYDRYWET & 0xFFu, 127u);
    levi_set_param_ui(&B, 0u, RI_CTL_LEVI_VPAN & 0xFFu, 0u);
    levi_trigger(&B, 0u, 60u);
    stereo(&B, la, lb, N);
    RI_ASSERT(!differ(oa, la, N) && !differ(ob, lb, N), "dtype clamps");
    /* Three mono topologies differ (same program; pingpong needs
     * asymmetry — covered below, it equals clean on dual-mono). */
    {
        float p0[N];
        uint32_t t, first = 1u;
        for (t = 0u; t < 3u; t++) {
            levi_init_set(&B);
            delay_on(&B);
            levi_set_param_ui(&B, 0u, RI_CTL_LEVI_DLYTYPE & 0xFFu, (uint8_t)t);
            levi_set_param_ui(&B, 0u, RI_CTL_LEVI_DLYFB & 0xFFu, 64u);
            levi_set_param_ui(&B, 0u, RI_CTL_LEVI_DLYDRYWET & 0xFFu, 127u);
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

    /* Pingpong crosses: hard-left input sounds the right channel
     * (clean keeps it exactly silent). */
    levi_init_set(&A);
    delay_on(&A);
    levi_set_param_ui(&A, 0u, RI_CTL_LEVI_DLYTYPE & 0xFFu, RI_LEVI_DT_PINGPONG);
    levi_set_param_ui(&A, 0u, RI_CTL_LEVI_DLYTIME & 0xFFu, 40u);
    levi_set_param_ui(&A, 0u, RI_CTL_LEVI_DLYFB & 0xFFu, 64u);
    levi_set_param_ui(&A, 0u, RI_CTL_LEVI_DLYDRYWET & 0xFFu, 127u);
    levi_set_param_ui(&A, 0u, RI_CTL_LEVI_VPAN & 0xFFu, 0u); /* hard left */
    levi_trigger(&A, 0u, 60u);
    stereo(&A, oa, ob, N);
    RI_ASSERT(peak(ob, N) > 0.0f, "pingpong crosses to R");
    levi_init_set(&B);
    delay_on(&B);
    levi_set_param_ui(&B, 0u, RI_CTL_LEVI_DLYTYPE & 0xFFu, RI_LEVI_DT_CLEAN);
    levi_set_param_ui(&B, 0u, RI_CTL_LEVI_DLYTIME & 0xFFu, 40u);
    levi_set_param_ui(&B, 0u, RI_CTL_LEVI_DLYFB & 0xFFu, 64u);
    levi_set_param_ui(&B, 0u, RI_CTL_LEVI_DLYDRYWET & 0xFFu, 127u);
    levi_set_param_ui(&B, 0u, RI_CTL_LEVI_VPAN & 0xFFu, 0u);
    levi_trigger(&B, 0u, 60u);
    stereo(&B, la, lb, N);
    RI_ASSERT(peak(lb, N) == 0.0f, "clean R silent");

    /* BPM flag stored, inert without the P8 clock. */
    levi_init_set(&A);
    delay_on(&A);
    levi_set_param_ui(&A, 0u, RI_CTL_LEVI_DLYTIME & 0xFFu, 84u);
    levi_trigger(&A, 0u, 60u);
    stereo(&A, oa, ob, N);
    levi_init_set(&B);
    delay_on(&B);
    levi_set_param_ui(&B, 0u, RI_CTL_LEVI_DLYTIME & 0xFFu, 84u);
    levi_set_param_ui(&B, 0u, RI_CTL_LEVI_DLYBPM & 0xFFu, 1u);
    levi_trigger(&B, 0u, 60u);
    stereo(&B, la, lb, N);
    RI_ASSERT(!differ(oa, la, N), "bpm stored inert");

    /* DM_DELAY module: 5 params; LFO->TIME route moves the echo. */
    RI_ASSERT(ri_levi_dm_nparam(RI_LEVI_DM_DELAY) == 5u, "delay nparam");
    levi_init_set(&A);
    delay_on(&A);
    levi_set_param_ui(&A, 0u, RI_CTL_LEVI_DLYTIME & 0xFFu, 40u);
    levi_trigger(&A, 0u, 60u);
    stereo(&A, oa, ob, N);
    levi_init_set(&B);
    delay_on(&B);
    levi_set_param_ui(&B, 0u, RI_CTL_LEVI_DLYTIME & 0xFFu, 40u);
    levi_set_mx_ui(&B, 0u, 0u, (uint8_t)ri_levi_ms_to_ui(RI_LEVI_MS_LFO0));
    levi_set_mx_ui(&B, 0u, 1u, RI_LEVI_DM_DELAY);
    levi_set_mx_ui(&B, 0u, 2u, RI_LEVI_DD_TIME);
    levi_set_mx_ui(&B, 0u, 3u, 100u);
    levi_trigger(&B, 0u, 60u);
    stereo(&B, la, lb, N);
    RI_ASSERT(differ(oa, la, N), "dm delay time moves echo");

    /* Keys / allow-list / pages. */
    RI_ASSERT(ri_auto_allowed(RI_CTL_LEVI_DLYTYPE), "dtype allowed");
    RI_ASSERT(ri_auto_allowed(RI_CTL_LEVI_DBYPASS), "dbypass allowed");
    RI_ASSERT(ri_auto_allowed(RI_CTL_LEVI_DLYBPM), "dbpm allowed");
    RI_ASSERT(!ri_auto_allowed(0x0E87u), "0x0E87 refused (P7b)");
    {
        const struct RICtlDef *d = ri_ctlreg_find((uint16_t)((RI_SEC_LEVI << 8) | RI_SLEVI_FXDLY));
        RI_ASSERT(d && d->engine_id == RI_CTL_LEVI_DBYPASS, "fxdly binds bypass");
    }
    {
        struct RISectLevi lv;
        char tx[16];
        memset(&lv, 0, sizeof lv);
        ri_slevi_init(&lv);
        RI_ASSERT(ri_slevi_set_value(&lv, RI_SLEVI_MODULE, (int)RI_SLEVI_M_DELAY) == 1, "delay module");
        RI_ASSERT(ri_slevi_enc_live(&lv, 0u) && !strcmp(ri_slevi_enc_name(&lv, 0u), "ON"), "slot0 on");
        RI_ASSERT(ri_slevi_ctl_idx(&lv, RI_SLEVI_ENC0) == RI_SLEVI_FXDLY, "on idx");
        RI_ASSERT(ri_slevi_enc_live(&lv, 1u) && !strcmp(ri_slevi_enc_name(&lv, 1u), "TYPE"), "slot1 type");
        RI_ASSERT(ri_slevi_page_reaches(RI_SLEVI_DLYTIME), "page reaches time");
        ri_slevi_enc_text(&lv, 1u, tx, sizeof tx);
        RI_ASSERT(!strcmp(tx, "CLEAN"), "type text %s", tx);
        RI_ASSERT(ri_slevi_set_value(&lv, RI_SLEVI_ENC0 + 1u, 127) == 1, "type max");
        ri_slevi_enc_text(&lv, 1u, tx, sizeof tx);
        RI_ASSERT(!strcmp(tx, "PINGPONG"), "type text %s", tx);
    }

    /* Utility laws: time map range, fail-closed NULLs. */
    {
        float t0 = ri_levi_delay_time(0u), t1 = ri_levi_delay_time(127u);
        RI_ASSERT(t0 > 0.0009f && t0 < 0.0011f, "time min %f", t0);
        RI_ASSERT(t1 > 1.9f && t1 <= 2.0f, "time max %f", t1);
        RI_ASSERT(ri_levi_delay_time(200u) <= 2.0f, "time clamp");
    }
    {
        float x = 1.0f, y = 1.0f;
        levi_fx_delay(0, SR, 0.5f, 0.5f, &x, &y);
        RI_ASSERT(x == 0.5f && y == 0.5f, "delay null passthrough");
    }

    /* Finite/bounded extremes storm (stereo, all types). */
    {
        uint32_t t, bad = 0;
        for (t = 0u; t < RI_LEVI_DT_N; t++) {
            int i;
            levi_init_set(&A);
            delay_on(&A);
            levi_set_param_ui(&A, 0u, RI_CTL_LEVI_DLYTYPE & 0xFFu, (uint8_t)t);
            levi_set_param_ui(&A, 0u, RI_CTL_LEVI_DLYTIME & 0xFFu, (uint8_t)(t * 40u));
            levi_set_param_ui(&A, 0u, RI_CTL_LEVI_DLYFB & 0xFFu, 127u);
            levi_set_param_ui(&A, 0u, RI_CTL_LEVI_DLYDRYWET & 0xFFu, 127u);
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

    RI_RESULT("t138_levi_delay");
}
