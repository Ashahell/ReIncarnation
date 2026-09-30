/* t134_levi_voice — Levi P6a voice allocator + 9 poly modes + legato/reset.
 * Laws: rotate/reassign/mono(last/low/high)/unison/unison-poly steal policy;
 * legato retunes without env restart, reset forces restart; direct
 * lane==voice path stays bit-identical; keys/pages/allow-list; finite.
 */
#include <stdio.h>
#include <string.h>
#include "tests/helpers/ri_assert.h"
#include "engine/dsp/levi.h"
#include "engine/seq/autolane.h"
#include "gui/sectlevi.h"
#include "gui/ctlreg.h"

#define SR 48000.0f

static int active_notes(struct RILeviSet *s, int v) {
    return s->v[v].active ? 1 : 0;
}
static int active_count(struct RILeviSet *s) {
    int n = 0, v;
    for (v = 0; v < (int)RI_LEVI_NVOICES; v++)
        n += active_notes(s, v);
    return n;
}

int main(void) {
    struct RILeviSet s, t;
    int v, n;

    /* Defaults. */
    levi_init_set(&s);
    RI_ASSERT(levi_alloc_mode(&s) == 0u, "default rotate");
    RI_ASSERT(s.udensity == 8u && s.ulimit == RI_LEVI_NVOICES, "default density 8 limit 8 (P6d)");
    RI_ASSERT(levi_set_param_ui(&s, 0u, RI_CTL_LEVI_POLYMODE & 0xFFu, 0u) == 0, "polymode set");
    RI_ASSERT(levi_set_param_ui(&s, 0u, RI_CTL_LEVI_POLYMODE & 0xFFu, 8u) == 0, "polymode max");
    RI_ASSERT(levi_set_param_ui(&s, 0u, RI_CTL_LEVI_POLYMODE & 0xFFu, 9u) == 0, "polymode clamp");
    RI_ASSERT(levi_set_alloc_ui(&s, 9u) == 2, "alloc refuse");

    /* Direct path bit-identical at defaults (new allocator changes nothing). */
    levi_init_set(&s);
    levi_init_set(&t);
    levi_trigger(&s, 0u, 60u);
    levi_trigger(&t, 0u, 60u);
    {
        float a = levi_voice_render(&s.v[0], 0, SR);
        float b = levi_voice_render(&t.v[0], 0, SR);
        RI_ASSERT(a == b, "direct bit-identical");
    }

    /* Rotate: 8 notes fill 0..7, 9th steals voice 0. */
    levi_init_set(&s);
    levi_set_alloc_ui(&s, 0u);
    for (v = 0; v < 8; v++) {
        n = levi_note_on(&s, (uint8_t)(60 + v));
        RI_ASSERT(n == 1, "rotate one %d", v);
    }
    RI_ASSERT(active_count(&s) == 8, "rotate full");
    RI_ASSERT(s.v[0].note == 60, "rotate order");
    n = levi_note_on(&s, 70u);
    RI_ASSERT(n == 1, "rotate steal one");
    RI_ASSERT(s.v[0].note == 70, "rotate steal oldest");
    n = levi_note_on(&s, 71u);
    RI_ASSERT(n == 1 && s.v[1].note == 71, "rotate steal advances");

    /* Reassign: first free, steal oldest without rotation. */
    levi_init_set(&s);
    levi_set_alloc_ui(&s, 1u);
    levi_note_on(&s, 60u);
    levi_note_on(&s, 64u);
    levi_note_off(&s, 60u);
    n = levi_note_on(&s, 67u);
    RI_ASSERT(n == 1 && s.v[0].note == 67, "reassign first free");

    /* Mono last/low/high priority on voice 0. */
    levi_init_set(&s);
    levi_set_alloc_ui(&s, 2u);
    levi_note_on(&s, 60u);
    levi_note_on(&s, 64u);
    levi_note_on(&s, 67u);
    RI_ASSERT(active_count(&s) == 1 && s.v[0].note == 67, "mono last");
    levi_note_off(&s, 67u);
    RI_ASSERT(s.v[0].note == 64, "mono fallback last");
    levi_init_set(&s);
    levi_set_alloc_ui(&s, 3u);
    levi_note_on(&s, 64u);
    levi_note_on(&s, 60u);
    levi_note_on(&s, 67u);
    RI_ASSERT(s.v[0].note == 60, "mono low");
    levi_init_set(&s);
    levi_set_alloc_ui(&s, 4u);
    levi_note_on(&s, 60u);
    levi_note_on(&s, 64u);
    RI_ASSERT(s.v[0].note == 64, "mono high");

    /* Unison: one note -> all limit voices same pitch. */
    levi_init_set(&s);
    levi_set_alloc_ui(&s, 5u);
    n = levi_note_on(&s, 60u);
    RI_ASSERT(n == 8, "unison all %d", n);
    for (v = 0; v < 8; v++)
        RI_ASSERT(s.v[v].active && s.v[v].note == 60, "unison same %d", v);
    /* Unison legato: voice 0 retunes, others full-retrigger. */
    levi_set_op_ui(&s, 0u, 0u, RI_LEVI_OP_LEGATO, 1u);
    {
        int i;
        for (i = 0; i < 200; i++)
            levi_voice_render(&s.v[0], 0, SR);
        {
            float b0 = s.v[0].st[0][0].env.value;
            levi_note_on(&s, 64u);
            RI_ASSERT(s.v[0].st[0][0].env.value >= b0 - 0.001f, "unison legato v0");
            RI_ASSERT(s.v[1].st[0][0].env.stage == RI_LEVI_SEG_D ||
                s.v[1].st[0][0].env.stage == RI_LEVI_SEG_A, "unison retrig v1");
        }
    }

    /* UnisonPoly density 2 limit 4: 2 notes use 4 voices; 3rd steals oldest. */
    levi_init_set(&s);
    levi_set_alloc_ui(&s, 8u);
    levi_set_param_ui(&s, 0u, RI_CTL_LEVI_UDENSITY & 0xFFu, 16u); /* 2 stacked */
    levi_set_param_ui(&s, 0u, RI_CTL_LEVI_ULIMIT & 0xFFu, 48u);   /* 4 voices (P6d) */
    levi_note_on(&s, 60u);
    RI_ASSERT(active_count(&s) == 2, "upoly density");
    levi_note_on(&s, 64u);
    RI_ASSERT(active_count(&s) == 4, "upoly two notes");
    levi_note_on(&s, 67u);
    RI_ASSERT(active_count(&s) == 4, "upoly steal keeps limit");
    /* Density above limit clamps to the limit (return counts applied). */
    levi_init_set(&s);
    levi_set_alloc_ui(&s, 8u);
    levi_set_param_ui(&s, 0u, RI_CTL_LEVI_UDENSITY & 0xFFu, 127u); /* 8 stacked */
    levi_set_param_ui(&s, 0u, RI_CTL_LEVI_ULIMIT & 0xFFu, 22u);    /* 2 voices */
    n = levi_note_on(&s, 60u);
    RI_ASSERT(n == 2, "upoly clamp %d", n);
    levi_set_param_ui(&s, 0u, RI_CTL_LEVI_ULIMIT & 0xFFu, 127u);
    RI_ASSERT(s.ulimit == RI_LEVI_NVOICES, "limit clamp 8 (P6d)");

    /* Legato retune vs reset restart (mono, op legato flag). */
    levi_init_set(&s);
    levi_set_alloc_ui(&s, 2u);
    levi_set_op_ui(&s, 0u, 0u, RI_LEVI_OP_LEGATO, 1u);
    levi_set_op_ui(&s, 0u, 0u, RI_LEVI_OP_COARSE, 0u); /* ratio 0.25 */
    levi_set_op_ui(&s, 1u, 0u, RI_LEVI_OP_COARSE, 0u);
    levi_note_on(&s, 60u);
    {
        float e0 = s.v[0].st[0][0].env.value;
        int i;
        for (i = 0; i < 100; i++)
            levi_voice_render(&s.v[0], 0, SR);
        (void)e0;
        float before = s.v[0].st[0][0].env.value;
        float bf = s.v[0].st[0][0].freq;
        levi_note_on(&s, 64u);
        float after = s.v[0].st[0][0].env.value;
        RI_ASSERT(s.v[0].note == 64, "legato retune note");
        RI_ASSERT(after >= before - 0.001f, "legato no restart %f %f", before, after);
        RI_ASSERT(s.v[0].st[0][0].freq != bf, "legato retuned %f", s.v[0].st[0][0].freq);
        /* Retune follows the trigger pitch law (ratio kept). */
        levi_trigger(&s, 1u, 64u);
        RI_ASSERT(s.v[0].st[0][0].freq == s.v[1].st[0][0].freq, "retune == trigger pitch");
    }
    levi_init_set(&s);
    levi_set_alloc_ui(&s, 2u);
    levi_set_op_ui(&s, 0u, 0u, RI_LEVI_OP_LEGATO, 1u);
    levi_set_op_ui(&s, 0u, 0u, RI_LEVI_OP_RESET, 1u);
    levi_note_on(&s, 60u);
    {
        int i;
        for (i = 0; i < 100; i++)
            levi_voice_render(&s.v[0], 0, SR);
        levi_note_on(&s, 64u);
        RI_ASSERT(s.v[0].st[0][0].env.stage == RI_LEVI_SEG_D ||
            s.v[0].st[0][0].env.stage == RI_LEVI_SEG_A, "reset restarts");
    }

    /* Keys / allow-list / pages. */
    RI_ASSERT(ri_auto_allowed(RI_CTL_LEVI_POLYMODE), "polymode allowed");
    RI_ASSERT(ri_auto_allowed(RI_CTL_LEVI_UDENSITY), "density allowed");
    RI_ASSERT(ri_auto_allowed(RI_CTL_LEVI_ULIMIT), "ulimit allowed");
    {
        const struct RICtlDef *d = ri_ctlreg_find((uint16_t)((RI_SEC_LEVI << 8) | 129u));
        RI_ASSERT(d && d->engine_id == RI_CTL_LEVI_POLYMODE, "reg polymode");
    }
    {
        struct RISectLevi lv;
        memset(&lv, 0, sizeof lv);
        ri_slevi_init(&lv);
        RI_ASSERT(ri_slevi_set_value(&lv, RI_SLEVI_MODULE, (int)RI_SLEVI_M_VOICE) == 1, "voice module");
        RI_ASSERT(ri_slevi_page_count(&lv) >= 1u, "voice pages");
        RI_ASSERT(ri_slevi_enc_live(&lv, 0u), "voice slot0 live");
        RI_ASSERT(!strcmp(ri_slevi_enc_name(&lv, 0u), "POLYPHONY"), "slot0 name");
        RI_ASSERT(ri_slevi_ctl_idx(&lv, RI_SLEVI_ENC0) == RI_SLEVI_POLYMODE, "slot0 idx");
        RI_ASSERT(ri_slevi_page_reaches(RI_SLEVI_POLYMODE), "page reaches polymode");
    }

    /* Finite/bounded storm across modes. */
    {
        uint32_t m;
        for (m = 0u; m < 9u; m++) {
            int i, bad = 0;
            levi_init_set(&s);
            levi_set_alloc_ui(&s, m);
            for (i = 0; i < 40; i++)
                levi_note_on(&s, (uint8_t)(30 + (i * 7) % 90));
            for (i = 0; i < 4800; i++) {
                float x = levi_voice_render(&s.v[i % RI_LEVI_NVOICES], 0, SR);
                if (!(x > -8.0f && x < 8.0f))
                    bad = 1;
            }
            RI_ASSERT(!bad, "mode %u bounded", m);
        }
    }
    RI_ASSERT(levi_note_on(0, 60u) == -1, "null fail-closed");
    RI_ASSERT(levi_note_on(&s, 200u) == -1, "note range");

    RI_RESULT("t134_levi_voice");
}
