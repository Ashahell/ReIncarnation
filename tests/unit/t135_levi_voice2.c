/* t135_levi_voice2 — Levi P6b voice params (mono) + VoiceMod + DM_VOICE.
 * Laws: defaults inert (bit-identical direct path); detune spreads voices;
 * analog feel drifts; random phase deterministic per voice; vibrato rate/
 * amt/delay; glide time/curve/mode; bend range inert at src 0; pan/width
 * stored but mono-ignored (stereo P6c); VoiceMod per-voice deterministic;
 * DM_VOICE module routes; keys/pages/allow-list; finite/bounded extremes.
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

static void render_sum(struct RILeviSet *s, float *o, uint32_t n) {
    levi_voice_render_sum(s, o, n, SR);
}

static int differ(const float *a, const float *b, uint32_t n) {
    uint32_t i;
    for (i = 0u; i < n; i++)
        if (a[i] != b[i])
            return 1;
    return 0;
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

static struct RILeviSet A, B;
static float oa[N], ob[N];

int main(void) {
    int v;

    /* Defaults inert: full P6b-zero set renders bit-identical to clean. */
    levi_init_set(&A);
    levi_init_set(&B);
    levi_trigger(&A, 0u, 60u);
    levi_trigger(&B, 0u, 60u);
    render_sum(&A, oa, N);
    render_sum(&B, ob, N);
    RI_ASSERT(!differ(oa, ob, N), "defaults bit-identical");

    /* Detune: 0 = voices identical; full = spread apart. */
    levi_init_set(&A);
    levi_trigger(&A, 0u, 60u);
    render_sum(&A, oa, N);
    levi_init_set(&B);
    levi_trigger(&B, 1u, 60u);
    render_sum(&B, ob, N);
    RI_ASSERT(!differ(oa, ob, N), "detune 0 equal");
    levi_init_set(&A);
    levi_set_param_ui(&A, 0u, RI_CTL_LEVI_VDETUNE & 0xFFu, 127u);
    levi_trigger(&A, 0u, 60u);
    render_sum(&A, oa, N);
    levi_init_set(&B);
    levi_set_param_ui(&B, 1u, RI_CTL_LEVI_VDETUNE & 0xFFu, 127u);
    levi_trigger(&B, 1u, 60u);
    render_sum(&B, ob, N);
    RI_ASSERT(differ(oa, ob, N), "detune spreads");
    RI_ASSERT(levi_set_param_ui(&B, 0u, RI_CTL_LEVI_VDETUNE & 0xFFu, 200u) == 0, "detune clamp ok");

    /* Analog feel: 0 inert; full drifts (renders differ, stay bounded). */
    levi_init_set(&A);
    levi_trigger(&A, 0u, 60u);
    render_sum(&A, oa, N);
    levi_init_set(&B);
    levi_set_param_ui(&B, 0u, RI_CTL_LEVI_VAFEEL & 0xFFu, 127u);
    levi_trigger(&B, 0u, 60u);
    render_sum(&B, ob, N);
    RI_ASSERT(differ(oa, ob, N), "afeel moves sound");
    RI_ASSERT(peak(ob, N) < 8.0f, "afeel bounded %f", peak(ob, N));

    /* Random phase: deterministic per voice (retrigger identical, voices differ). */
    levi_init_set(&A);
    levi_set_param_ui(&A, 0u, RI_CTL_LEVI_VRNDPH & 0xFFu, 127u);
    levi_set_param_ui(&A, 1u, RI_CTL_LEVI_VRNDPH & 0xFFu, 127u);
    levi_trigger(&A, 0u, 60u);
    render_sum(&A, oa, N);
    levi_trigger(&A, 0u, 60u);
    render_sum(&A, ob, N);
    RI_ASSERT(!differ(oa, ob, N), "rndphase deterministic");
    levi_init_set(&B);
    levi_set_param_ui(&B, 1u, RI_CTL_LEVI_VRNDPH & 0xFFu, 127u);
    levi_trigger(&B, 1u, 60u);
    render_sum(&B, ob, N);
    RI_ASSERT(differ(oa, ob, N), "rndphase per-voice");

    /* Vibrato: amt 0 inert; amt moves; full delay keeps attack clean. */
    levi_init_set(&A);
    levi_trigger(&A, 0u, 60u);
    render_sum(&A, oa, N);
    levi_init_set(&B);
    levi_set_param_ui(&B, 0u, RI_CTL_LEVI_VVIBAMT & 0xFFu, 127u);
    levi_set_param_ui(&B, 0u, RI_CTL_LEVI_VVIBRATE & 0xFFu, 64u);
    levi_trigger(&B, 0u, 60u);
    render_sum(&B, ob, N);
    RI_ASSERT(differ(oa, ob, N), "vibrato moves");
    levi_init_set(&B);
    levi_set_param_ui(&B, 0u, RI_CTL_LEVI_VVIBAMT & 0xFFu, 127u);
    levi_set_param_ui(&B, 0u, RI_CTL_LEVI_VVIBDLY & 0xFFu, 127u);
    levi_trigger(&B, 0u, 60u);
    render_sum(&B, ob, N);
    {
        /* Delay ramps the depth in (not a gate): early deviation stays
         * small and the depth still arrives later. */
        float early = 0.0f, late = 0.0f, pk = peak(oa, N);
        for (v = 0; v < 240; v++) {
            float d = oa[v] - ob[v];
            if (d < 0.0f)
                d = -d;
            if (d > early)
                early = d;
        }
        for (v = 2400; v < (int)N; v++) {
            float d = oa[v] - ob[v];
            if (d < 0.0f)
                d = -d;
            if (d > late)
                late = d;
        }
        RI_ASSERT(early < pk * 0.10f + 1e-6f, "vib delay keeps attack %f", early);
        RI_ASSERT(late > early, "vib delay still arrives");
    }

    /* Glide: off = instant; on = slides (differs from off). */
    levi_init_set(&A);
    levi_trigger(&A, 0u, 60u);
    render_sum(&A, oa, N);
    levi_trigger(&A, 0u, 64u);
    render_sum(&A, oa, N);
    levi_init_set(&B);
    levi_set_param_ui(&B, 0u, RI_CTL_LEVI_VGLIDE & 0xFFu, 1u);
    levi_set_param_ui(&B, 0u, RI_CTL_LEVI_VGLTIME & 0xFFu, 127u);
    levi_trigger(&B, 0u, 60u);
    render_sum(&B, ob, N);
    levi_trigger(&B, 0u, 64u);
    render_sum(&B, ob, N);
    RI_ASSERT(differ(oa, ob, N), "glide slides");
    /* Glide time 0 lands instantly (== glide off, bit for bit). */
    levi_init_set(&B);
    levi_set_param_ui(&B, 0u, RI_CTL_LEVI_VGLIDE & 0xFFu, 1u);
    levi_set_param_ui(&B, 0u, RI_CTL_LEVI_VGLTIME & 0xFFu, 0u);
    levi_trigger(&B, 0u, 60u);
    render_sum(&B, ob, N);
    levi_trigger(&B, 0u, 64u);
    render_sum(&B, ob, N);
    RI_ASSERT(!differ(oa, ob, N), "glide time 0 instant");
    /* A short glide runs to completion (progress state reaches 1). */
    {
        int i;
        levi_init_set(&A);
        levi_set_param_ui(&A, 0u, RI_CTL_LEVI_VGLIDE & 0xFFu, 1u);
        levi_set_param_ui(&A, 0u, RI_CTL_LEVI_VGLTIME & 0xFFu, 8u); /* ~20 ms */
        levi_trigger(&A, 0u, 60u);
        render_sum(&A, oa, N);
        RI_ASSERT(A.v[0].glt >= 1.0f, "fresh voice no glide");
        levi_trigger(&A, 0u, 64u);
        RI_ASSERT(A.v[0].glt == 0.0f, "glide starts");
        for (i = 0; i < 10; i++)
            render_sum(&A, oa, N);   /* 1 s >> 20 ms */
        RI_ASSERT(A.v[0].glt >= 1.0f, "glide completes");
    }
    /* Glissando steps the same slide (differs from smooth glide). */
    {
        static float oc[N];
        uint32_t i;
        levi_init_set(&B);
        levi_set_param_ui(&B, 0u, RI_CTL_LEVI_VGLIDE & 0xFFu, 1u);
        levi_set_param_ui(&B, 0u, RI_CTL_LEVI_VGLTIME & 0xFFu, 127u);
        levi_trigger(&B, 0u, 60u);
        render_sum(&B, ob, N);
        levi_trigger(&B, 0u, 64u);
        render_sum(&B, ob, N);
        for (i = 0u; i < N; i++)
            oc[i] = ob[i];
        levi_init_set(&B);
        levi_set_param_ui(&B, 0u, RI_CTL_LEVI_VGLIDE & 0xFFu, 127u);
        levi_set_param_ui(&B, 0u, RI_CTL_LEVI_VGLTIME & 0xFFu, 127u);
        levi_trigger(&B, 0u, 60u);
        render_sum(&B, ob, N);
        levi_trigger(&B, 0u, 64u);
        render_sum(&B, ob, N);
        RI_ASSERT(differ(oc, ob, N), "gliss steps");
    }

    /* Bend range inert at src 0 (P9 delivers bend); key clamps. */
    levi_init_set(&A);
    levi_trigger(&A, 0u, 60u);
    render_sum(&A, oa, N);
    levi_init_set(&B);
    levi_set_param_ui(&B, 0u, RI_CTL_LEVI_VBENDRNG & 0xFFu, 127u);
    levi_trigger(&B, 0u, 60u);
    render_sum(&B, ob, N);
    RI_ASSERT(!differ(oa, ob, N), "bendrange inert at src 0");

    /* Pan/width/mode stored, mono-ignored (stereo P6c flips this law). */
    levi_init_set(&A);
    levi_trigger(&A, 0u, 60u);
    render_sum(&A, oa, N);
    levi_init_set(&B);
    levi_set_param_ui(&B, 0u, RI_CTL_LEVI_VPAN & 0xFFu, 127u);
    levi_set_param_ui(&B, 0u, RI_CTL_LEVI_VWIDTH & 0xFFu, 0u);
    levi_set_param_ui(&B, 0u, RI_CTL_LEVI_VPANMODE & 0xFFu, 1u);
    levi_trigger(&B, 0u, 60u);
    render_sum(&B, ob, N);
    RI_ASSERT(!differ(oa, ob, N), "pan mono-ignored");

    /* VoiceMod: per-voice deterministic (VMOD->cutoff route).
     * Isolated sets: render_sum mixes every active voice. */
    levi_init_set(&A);
    levi_set_param_ui(&A, 0u, RI_LEVI_CUTOFF, 40u);   /* headroom below the 18 kHz clamp */
    levi_set_mx_ui(&A, 0u, 0u, (uint8_t)ri_levi_ms_to_ui(RI_LEVI_MS_VMOD));
    levi_set_mx_ui(&A, 0u, 1u, RI_LEVI_DM_DFILT);
    levi_set_mx_ui(&A, 0u, 2u, 0u);
    levi_set_mx_ui(&A, 0u, 3u, 100u);
    levi_trigger(&A, 0u, 60u);
    render_sum(&A, oa, N);
    levi_trigger(&A, 0u, 60u);
    render_sum(&A, ob, N);
    RI_ASSERT(!differ(oa, ob, N), "vmod deterministic");
    levi_init_set(&B);
    levi_set_param_ui(&B, 1u, RI_LEVI_CUTOFF, 40u);
    levi_set_mx_ui(&B, 0u, 0u, (uint8_t)ri_levi_ms_to_ui(RI_LEVI_MS_VMOD));
    levi_set_mx_ui(&B, 0u, 1u, RI_LEVI_DM_DFILT);
    levi_set_mx_ui(&B, 0u, 2u, 0u);
    levi_set_mx_ui(&B, 0u, 3u, 100u);
    levi_trigger(&B, 1u, 60u);
    render_sum(&B, ob, N);
    RI_ASSERT(differ(oa, ob, N), "vmod per-voice");
    /* VMOD+ ordinal path is live too (deterministic, per-voice). */
    levi_init_set(&A);
    levi_set_param_ui(&A, 0u, RI_LEVI_CUTOFF, 40u);
    levi_set_mx_ui(&A, 0u, 0u, (uint8_t)ri_levi_ms_to_ui(RI_LEVI_MS_VMODP));
    levi_set_mx_ui(&A, 0u, 1u, RI_LEVI_DM_DFILT);
    levi_set_mx_ui(&A, 0u, 2u, 0u);
    levi_set_mx_ui(&A, 0u, 3u, 100u);
    levi_trigger(&A, 0u, 60u);
    render_sum(&A, oa, N);
    levi_init_set(&B);
    levi_set_param_ui(&B, 5u, RI_LEVI_CUTOFF, 40u);
    levi_set_mx_ui(&B, 0u, 0u, (uint8_t)ri_levi_ms_to_ui(RI_LEVI_MS_VMODP));
    levi_set_mx_ui(&B, 0u, 1u, RI_LEVI_DM_DFILT);
    levi_set_mx_ui(&B, 0u, 2u, 0u);
    levi_set_mx_ui(&B, 0u, 3u, 100u);
    levi_trigger(&B, 5u, 60u);
    render_sum(&B, ob, N);
    RI_ASSERT(differ(oa, ob, N), "vmod+ per-voice");

    /* DM_VOICE module: 10 params; LFO->detune route moves pitch. */
    RI_ASSERT(ri_levi_dm_nparam(RI_LEVI_DM_VOICE) == 10u, "voice nparam");
    levi_init_set(&A);
    levi_trigger(&A, 0u, 60u);
    render_sum(&A, oa, N);
    levi_init_set(&B);
    levi_set_mx_ui(&B, 0u, 0u, (uint8_t)ri_levi_ms_to_ui(RI_LEVI_MS_LFO0));
    levi_set_mx_ui(&B, 0u, 1u, RI_LEVI_DM_VOICE);
    levi_set_mx_ui(&B, 0u, 2u, 0u);
    levi_set_mx_ui(&B, 0u, 3u, 100u);
    levi_trigger(&B, 0u, 60u);
    render_sum(&B, ob, N);
    RI_ASSERT(differ(oa, ob, N), "dm voice detune route");

    /* Keys / allow-list / pages. */
    RI_ASSERT(ri_auto_allowed(RI_CTL_LEVI_VDETUNE), "detune allowed");
    RI_ASSERT(ri_auto_allowed(RI_CTL_LEVI_VGLCURVE), "glcurve allowed");
    RI_ASSERT(!ri_auto_allowed(0x0E68u), "0x0E68 refused");
    {
        const struct RICtlDef *d = ri_ctlreg_find((uint16_t)((RI_SEC_LEVI << 8) | 132u));
        RI_ASSERT(d && d->engine_id == RI_CTL_LEVI_VDETUNE, "reg detune");
    }
    {
        struct RISectLevi lv;
        memset(&lv, 0, sizeof lv);
        ri_slevi_init(&lv);
        RI_ASSERT(ri_slevi_set_value(&lv, RI_SLEVI_MODULE, (int)RI_SLEVI_M_VOICE) == 1, "voice module");
        RI_ASSERT(ri_slevi_page_count(&lv) == 2u, "voice 2 pages");
        RI_ASSERT(ri_slevi_enc_live(&lv, 3u) && !strcmp(ri_slevi_enc_name(&lv, 3u), "DETUNE"), "slot3 detune");
        RI_ASSERT(ri_slevi_ctl_idx(&lv, RI_SLEVI_ENC0 + 3u) == RI_SLEVI_VDETUNE, "detune idx");
        RI_ASSERT(ri_slevi_press(&lv, RI_SLEVI_PAGEDN) == 1, "page 2");
        RI_ASSERT(ri_slevi_enc_live(&lv, 0u) && !strcmp(ri_slevi_enc_name(&lv, 0u), "PAN MODE"), "p2 slot0");
        RI_ASSERT(ri_slevi_ctl_idx(&lv, RI_SLEVI_ENC0) == RI_SLEVI_VPANMODE, "panmode idx");
    }

    /* Finite/bounded extremes storm. */
    {
        uint8_t vals[] = { 0u, 127u };
        uint32_t a, b, bad = 0;
        for (a = 0u; a < 2u; a++)
            for (b = 0u; b < 2u; b++) {
                int i;
                levi_init_set(&A);
                levi_set_param_ui(&A, 0u, RI_CTL_LEVI_VDETUNE & 0xFFu, vals[a]);
                levi_set_param_ui(&A, 0u, RI_CTL_LEVI_VAFEEL & 0xFFu, vals[b]);
                levi_set_param_ui(&A, 0u, RI_CTL_LEVI_VVIBAMT & 0xFFu, vals[a]);
                levi_set_param_ui(&A, 0u, RI_CTL_LEVI_VGLTIME & 0xFFu, vals[b]);
                levi_trigger(&A, 0u, (uint8_t)(20 + a * 80 + b));
                for (i = 0; i < 4800; i++) {
                    float x = levi_voice_render(&A.v[0], 0, SR);
                    if (!(x > -8.0f && x < 8.0f))
                        bad = 1;
                }
            }
        RI_ASSERT(!bad, "extremes bounded");
    }
    RI_ASSERT(peak(oa, N) > 0.0f, "sounding");

    RI_RESULT("t135_levi_voice2");
}
