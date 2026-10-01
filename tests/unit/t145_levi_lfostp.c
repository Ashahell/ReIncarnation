/* t145_levi_lfostp — Levi P8e step-LFO editor.
 * Laws: the untouched step wave follows the analytic ramp (songs
 * bit-identical); the first write materialises the ramp (continuity);
 * RAMP restores the default; SEMI LOCK snaps to the 1/12 grid; step keys
 * 0x1300-0x130E/0x1400-0x1402 allowed, neighbours refused; LP_SEMI is
 * LFO param 14; the panel pages 2 -> 3 (SEMI LOCK, STEP EDIT gate, STEP /
 * VALUE / RAMP); null-safe; finite extremes.
 */
#include <stdio.h>
#include <string.h>
#include "tests/helpers/ri_assert.h"
#include "engine/dsp/levi.h"
#include "engine/dsp/levi_matrix.h"
#include "engine/seq/autolane.h"
#include "engine/engine.h"
#include "gui/sectlevi.h"
#include "gui/ctlreg.h"

#define SR 48000.0f
#define BS 256u

static struct RILeviSet A, B;

/* LFO 1 as a step table: one step per note, so each note reads its entry. */
static void step_lfo(struct RILeviSet *s, uint32_t nsteps) {
    levi_init_set(s);
    levi_set_lfo_ui(s, 0u, 0u, RI_LEVI_LP_WAVE, RI_LEVI_LW_STEP);
    levi_set_lfo_ui(s, 0u, 0u, RI_LEVI_LP_ONESHOT, 2u);
    levi_set_lfo_ui(s, 0u, 0u, RI_LEVI_LP_STEPS, (uint8_t)nsteps);
}

/* The next entry's value for LFO 1 (step index of the next note-on). */
static float next_step_value(struct RILeviSet *s) {
    levi_trigger(s, 0u, 60u);
    return ri_levi_lfo_step(&s->v[0].lfo[0], SR);
}

static int closef(float a, float b, float eps) {
    float d = a > b ? a - b : b - a;
    return d <= eps;
}

/* Panel: LFO module n, page p, encoder k -> key/value. */
static int pan_key(struct RISectLevi *lv, uint32_t mod, uint32_t page, uint32_t k,
    uint16_t *key, int *val) {
    ri_slevi_set_value(lv, RI_SLEVI_MODULE, (int)mod);
    lv->page = (uint8_t)page;
    return ri_slevi_ctl_key(lv, RI_SLEVI_ENC0 + k, key, val);
}

int main(void) {
    struct RIEngine e;
    struct RIEvent ev;
    uint32_t i, v, k;
    static float la[BS], ra[BS], lb[BS], rb[BS], oa[4096], ob[4096];

    /* ---- Defaults: SEMI off, every table unowned (analytic ramp). ---- */
    RI_ASSERT(RI_LEVI_LP_SEMI == 14u && RI_LEVI_LP_N == 15u, "LP_SEMI is param 14");
    {
        int lo = -1, hi = -1;
        RI_ASSERT(ri_levi_lfo_range(RI_LEVI_LP_SEMI, &lo, &hi) == 0 && lo == 0 && hi == 1 &&
            ri_levi_lfo_default(RI_LEVI_LP_SEMI) == 0, "semi lock 0..1, default off");
        RI_ASSERT(ri_levi_lfo_range(RI_LEVI_LP_N, &lo, &hi) == 2, "param 15 refused");
        levi_init_set(&A);
        for (v = 0u; v < RI_LEVI_NVOICES; v++)
            for (k = 0u; k < RI_LEVI_NLFO; k++) {
                RI_ASSERT(!A.v[v].lfo[k].sown && !A.v[v].lfo[k].semi, "v%u lfo%u clean", v, k);
                for (i = 0u; i < RI_LEVI_MAXSTEPS; i++)
                    RI_ASSERT(A.v[v].lfo[k].sval[i] == 0, "v%u lfo%u step %u zero", v, k, i);
            }
    }

    /* ---- The untouched step wave is the analytic ramp (-1, 0, +1). ---- */
    {
        float got[3];
        step_lfo(&A, 3u);
        for (i = 0u; i < 3u; i++)
            got[i] = next_step_value(&A);
        RI_ASSERT(got[0] == -1.0f && got[1] == 0.0f && got[2] == 1.0f,
            "analytic ramp %f/%f/%f", (double)got[0], (double)got[1], (double)got[2]);
        RI_ASSERT(!A.v[0].lfo[0].sown, "default table not owned");
    }

    /* ---- Audibly identical while unowned (same-age twin). ---- */
    {
        step_lfo(&A, 3u);
        step_lfo(&B, 3u);
        levi_trigger(&A, 0u, 60u);
        levi_trigger(&B, 0u, 60u);
        for (i = 0u; i < 16u; i++) {
            levi_voice_render_sum_stereo(&A, la, ra, BS, SR);
            levi_voice_render_sum_stereo(&B, lb, rb, BS, SR);
            memcpy(oa + i * BS, la, sizeof la);
            memcpy(ob + i * BS, lb, sizeof lb);
        }
        RI_ASSERT(!memcmp(oa, ob, sizeof oa), "unowned step table renders identically");
    }

    /* ---- First write materialises the ramp: the other steps stay put. ---- */
    {
        float got[3];
        step_lfo(&A, 3u);
        RI_ASSERT(levi_set_lfo_stepctl(&A, 0u, RI_LEVI_LS_STEP, 1u) == 0, "cursor 1");
        RI_ASSERT(levi_set_lfo_stepctl(&A, 0u, RI_LEVI_LS_VALUE, 96u) == 0, "value 96");
        for (i = 0u; i < 3u; i++)
            got[i] = next_step_value(&A);
        RI_ASSERT(got[0] == -1.0f && closef(got[1], 0.5079f, 0.002f) && got[2] == 1.0f,
            "continuity %f/%f/%f", (double)got[0], (double)got[1], (double)got[2]);
        RI_ASSERT(A.v[0].lfo[0].sown && A.v[3].lfo[0].sown && A.glfo[0].sown, "table owned");
        RI_ASSERT(A.lsc[0] == 1u && A.lsc[4] == 0u, "per-LFO cursor");
        /* the materialised ladder: n ramp steps, then the top held to 64
         * (step 1 is the edited one, so it carries the write) */
        RI_ASSERT(A.v[0].lfo[0].sval[0] == -63 && A.v[0].lfo[0].sval[2] == 63 &&
            A.v[0].lfo[0].sval[3] == 63 &&
            A.v[0].lfo[0].sval[RI_LEVI_MAXSTEPS - 1u] == 63,
            "ladder %d/%d/%d/%d", (int)A.v[0].lfo[0].sval[0], (int)A.v[0].lfo[0].sval[2],
            (int)A.v[0].lfo[0].sval[3], (int)A.v[0].lfo[0].sval[RI_LEVI_MAXSTEPS - 1u]);
    }

    /* ---- The panel's bottom end reads -1 (the clamp, so an edit at 0
     * still matches the analytic -1 the wave had). ---- */
    {
        float x;
        step_lfo(&A, 3u);
        levi_set_lfo_stepctl(&A, 0u, RI_LEVI_LS_STEP, 0u);
        levi_set_lfo_stepctl(&A, 0u, RI_LEVI_LS_VALUE, 0u);
        RI_ASSERT(A.v[0].lfo[0].sval[0] == -64, "sval holds val-64 (%d)",
            (int)A.v[0].lfo[0].sval[0]);
        x = next_step_value(&A);
        RI_ASSERT(x == -1.0f, "panel 0 reads -1 (%f)", (double)x);
    }

    /* ---- An owned step change reaches the audio (same-age twins). The
     * route (LFO 1 -> VCA level) is what makes the table audible: an
     * unedited pair renders bit-identically, an edit sounds on exactly
     * the notes that read the edited step. ---- */
    {
        uint32_t na = 0u, n1 = 0u, n1exp = 0u, nsame = 0u;
        step_lfo(&A, 3u);
        step_lfo(&B, 3u);
        levi_set_mx_ui(&A, 3u, 0u, 6u);                  /* source LFO 1 */
        levi_set_mx_ui(&A, 3u, 1u, RI_LEVI_DM_VCA);
        levi_set_mx_ui(&A, 3u, 2u, 0u);                  /* VCA level */
        levi_set_mx_ui(&A, 3u, 3u, 127u);
        levi_set_mx_ui(&B, 3u, 0u, 6u);
        levi_set_mx_ui(&B, 3u, 1u, RI_LEVI_DM_VCA);
        levi_set_mx_ui(&B, 3u, 2u, 0u);
        levi_set_mx_ui(&B, 3u, 3u, 127u);
        for (i = 0u; i < 8u; i++) {                      /* control: no edit */
            levi_trigger(&A, 0u, 60u);
            levi_trigger(&B, 0u, 60u);
            for (k = 0u; k < 4u; k++) {
                levi_voice_render_sum_stereo(&A, la, ra, BS, SR);
                levi_voice_render_sum_stereo(&B, lb, rb, BS, SR);
                if (memcmp(la, lb, sizeof la) != 0)
                    nsame++;
            }
        }
        RI_ASSERT(nsame == 0u && la[0] != 0.0f, "the route is live (%u/32 differ, %f)",
            nsame, (double)la[BS - 1u]);
        step_lfo(&A, 3u);
        step_lfo(&B, 3u);
        levi_set_mx_ui(&A, 3u, 0u, 6u);
        levi_set_mx_ui(&A, 3u, 1u, RI_LEVI_DM_VCA);
        levi_set_mx_ui(&A, 3u, 2u, 0u);
        levi_set_mx_ui(&A, 3u, 3u, 127u);
        levi_set_mx_ui(&B, 3u, 0u, 6u);
        levi_set_mx_ui(&B, 3u, 1u, RI_LEVI_DM_VCA);
        levi_set_mx_ui(&B, 3u, 2u, 0u);
        levi_set_mx_ui(&B, 3u, 3u, 127u);
        levi_set_lfo_stepctl(&B, 0u, RI_LEVI_LS_STEP, 1u);
        levi_set_lfo_stepctl(&B, 0u, RI_LEVI_LS_VALUE, 127u);
        for (i = 0u; i < 8u; i++) {
            levi_trigger(&A, 0u, 60u);
            levi_trigger(&B, 0u, 60u);
            for (k = 0u; k < 4u; k++) {
                uint32_t diff;
                levi_voice_render_sum_stereo(&A, la, ra, BS, SR);
                levi_voice_render_sum_stereo(&B, lb, rb, BS, SR);
                diff = memcmp(la, lb, sizeof la) != 0 ? 1u : 0u;
                if (A.v[0].lfo[0].stepk == 1u) {         /* the note that reads step 1 */
                    n1 += diff;
                    n1exp++;
                } else
                    na += !diff;                         /* every other note matches */
            }
        }
        RI_ASSERT(n1exp > 0u && n1 == n1exp && na == 32u - n1exp,
            "the edit sounds where it is read (%u/%u differ, %u/32 elsewhere)", n1, n1exp, na);
    }

    /* ---- RAMP restores the default (owned tables walk back to -1..+1). ---- */
    {
        float got[3];
        step_lfo(&A, 3u);
        levi_set_lfo_stepctl(&A, 0u, RI_LEVI_LS_STEP, 1u);
        levi_set_lfo_stepctl(&A, 0u, RI_LEVI_LS_VALUE, 100u);
        RI_ASSERT(A.v[0].lfo[0].sown, "owned before RAMP");
        RI_ASSERT(levi_set_lfo_stepctl(&A, 0u, RI_LEVI_LS_RAMP, 0u) == 0 &&
            A.v[0].lfo[0].sown, "ramp 0 leaves an owned table alone");
        RI_ASSERT(levi_set_lfo_stepctl(&A, 0u, RI_LEVI_LS_RAMP, 1u) == 0, "ramp");
        RI_ASSERT(!A.v[0].lfo[0].sown && !A.glfo[0].sown, "RAMP clears ownership");
        for (i = 0u; i < 3u; i++)
            got[i] = next_step_value(&A);
        RI_ASSERT(got[0] == -1.0f && got[1] == 0.0f && got[2] == 1.0f, "ramp %f/%f/%f",
            (double)got[0], (double)got[1], (double)got[2]);
        levi_set_lfo_stepctl(&A, 0u, RI_LEVI_LS_VALUE, 90u);
        RI_ASSERT(A.v[0].lfo[0].sown && levi_set_lfo_stepctl(&A, 0u, RI_LEVI_LS_RAMP, 0u) == 0 &&
            A.v[0].lfo[0].sown, "ramp 0 after a value write keeps the table owned");
    }

    /* ---- A shrunken table keeps the step index inside n. ---- */
    {
        float x;
        step_lfo(&A, 3u);
        RI_ASSERT(next_step_value(&A) == -1.0f && next_step_value(&A) == 0.0f, "steps 0,1");
        x = next_step_value(&A);
        RI_ASSERT(x == 1.0f && A.v[0].lfo[0].stepk == 2u, "step 2 held (%f, k=%u)",
            (double)x, A.v[0].lfo[0].stepk);
        RI_ASSERT(levi_set_lfo_ui(&A, 0u, 0u, RI_LEVI_LP_STEPS, 2u) == 0, "n = 2");
        x = ri_levi_lfo_step(&A.v[0].lfo[0], SR);       /* same step, table now shorter */
        RI_ASSERT(x == 1.0f, "index held inside n (%f)", (double)x);
    }

    /* ---- SEMI LOCK snaps writes to the 1/12 grid; unlocked writes are raw. ---- */
    {
        uint8_t seen[128];
        uint32_t ng = 0u;
        step_lfo(&A, 4u);
        levi_set_lfo_stepctl(&A, 0u, RI_LEVI_LS_STEP, 0u);
        memset(seen, 0, sizeof seen);
        for (i = 0u; i < 128u; i++) {
            float x, g;
            levi_set_lfo_stepctl(&A, 0u, RI_LEVI_LS_VALUE, (uint8_t)i);
            if (i == 70u)
                RI_ASSERT(A.v[0].lfo[0].sval[0] == 6, "semi off stores raw 70 (%d)",
                    (int)A.v[0].lfo[0].sval[0]);
            levi_set_lfo_ui(&A, 0u, 0u, RI_LEVI_LP_SEMI, 1u);
            levi_set_lfo_stepctl(&A, 0u, RI_LEVI_LS_VALUE, (uint8_t)i);
            x = (float)A.v[0].lfo[0].sval[0] / 63.0f;   /* the bipolar position */
            g = (float)(int)(x * 12.0f + (x < 0.0f ? -0.5f : 0.5f)) / 12.0f;
            /* within one storage quantum (1/63) of a grid point, centre exact */
            RI_ASSERT(closef(x, g, 1.0f / 63.0f + 1e-4f) || i == 64u,
                "semi snap %u -> %d (%f vs %f)", i, (int)A.v[0].lfo[0].sval[0],
                (double)x, (double)g);
            seen[A.v[0].lfo[0].sval[0] + 64] = 1u;
            levi_set_lfo_ui(&A, 0u, 0u, RI_LEVI_LP_SEMI, 0u);
        }
        for (i = 0u; i < 128u; i++)
            ng += seen[i] ? 1u : 0u;
        RI_ASSERT(ng == 25u, "25 semitone levels on the grid (%u)", ng);
        RI_ASSERT(levi_set_lfo_stepctl(&A, 0u, RI_LEVI_LS_VALUE, 64u) == 0 &&
            A.v[0].lfo[0].sval[0] == 0, "centre stays centre");
        /* a locked write lands where a plain write of the snapped value would */
        levi_set_lfo_ui(&A, 0u, 0u, RI_LEVI_LP_SEMI, 1u);
        levi_set_lfo_stepctl(&A, 0u, RI_LEVI_LS_VALUE, 70u);
        RI_ASSERT(A.v[0].lfo[0].sval[0] == 5, "70 snaps to 69 (%d)",
            (int)(A.v[0].lfo[0].sval[0] + 64));
        levi_set_lfo_stepctl(&A, 0u, RI_LEVI_LS_VALUE, 69u);
        RI_ASSERT(A.v[0].lfo[0].sval[0] == 5, "locked writes are idempotent (%d)",
            (int)(A.v[0].lfo[0].sval[0] + 64));
    }

    /* ---- Device-wide table: every voice copy follows. ---- */
    {
        step_lfo(&A, 3u);
        levi_set_lfo_stepctl(&A, 2u, RI_LEVI_LS_STEP, 2u);
        levi_set_lfo_stepctl(&A, 2u, RI_LEVI_LS_VALUE, 30u);
        for (v = 0u; v < RI_LEVI_NVOICES; v++)
            RI_ASSERT(A.v[v].lfo[2].sown && A.v[v].lfo[2].sval[2] == -34,
                "voice %u follows (%d)", v, (int)A.v[v].lfo[2].sval[2]);
        RI_ASSERT(A.glfo[2].sown && A.glfo[2].sval[2] == -34, "shared copy follows");
        RI_ASSERT(!A.v[0].lfo[0].sown, "other LFO untouched");
        RI_ASSERT(A.lsc[2] == 2u, "cursor per LFO");
    }

    /* ---- Steps beyond n are stored but never read. ---- */
    {
        float got[3];
        step_lfo(&A, 3u);
        levi_set_lfo_stepctl(&A, 0u, RI_LEVI_LS_STEP, 40u);
        levi_set_lfo_stepctl(&A, 0u, RI_LEVI_LS_VALUE, 0u);
        for (i = 0u; i < 3u; i++)
            got[i] = next_step_value(&A);
        RI_ASSERT(got[0] == -1.0f && got[1] == 0.0f && got[2] == 1.0f,
            "step 40 inert at n=3 (%f/%f/%f)", (double)got[0], (double)got[1], (double)got[2]);
        RI_ASSERT(A.v[0].lfo[0].sval[40] == -64, "stored anyway");
    }

    /* ---- Clamps and refusals. ---- */
    {
        step_lfo(&A, 3u);
        levi_set_lfo_stepctl(&A, 0u, RI_LEVI_LS_STEP, 200u);
        RI_ASSERT(A.lsc[0] == RI_LEVI_MAXSTEPS - 1u, "cursor clamps to 63 (%u)", A.lsc[0]);
        RI_ASSERT(levi_set_lfo_stepctl(0, 0u, RI_LEVI_LS_STEP, 1u) == 2, "NULL refused");
        RI_ASSERT(levi_set_lfo_stepctl(&A, RI_LEVI_NLFO, 0u, 1u) == 2, "lfo 5 refused");
        RI_ASSERT(levi_set_lfo_stepctl(&A, 0u, 3u, 1u) == 2, "field 3 refused");
        RI_ASSERT(levi_set_lfo_stepctl(&A, 99u, 1u, 1u) == 2, "wild lfo refused");
    }

    /* ---- Keys: block rules and LP_SEMI as LFO param 14. ---- */
    {
        RI_ASSERT(RI_LEVI_LSKEY(0u, 0u) == 0x1300u && RI_LEVI_LSKEY(0u, 1u) == 0x1301u &&
            RI_LEVI_LSKEY(3u, 2u) == 0x130Eu && RI_LEVI_LSKEY(4u, 0u) == 0x1400u &&
            RI_LEVI_LSKEY(4u, 2u) == 0x1402u, "step key layout");
        for (i = 0u; i < RI_LEVI_NLFO; i++)
            for (k = 0u; k <= 2u; k++)
                RI_ASSERT(ri_auto_allowed(RI_LEVI_LSKEY(i, k)), "step key %04x allowed",
                    RI_LEVI_LSKEY(i, k));
        RI_ASSERT(!ri_auto_allowed(0x1303u) && !ri_auto_allowed(0x1307u) && !ri_auto_allowed(0x130Fu) &&
            !ri_auto_allowed(0x13FFu) && !ri_auto_allowed(0x1403u) && !ri_auto_allowed(0x1404u) &&
            !ri_auto_allowed(0x1410u) && ri_auto_allowed(0x12EFu) &&
            ri_auto_allowed(RI_LEVI_MRKEY(7u, 3u, 3u)), "neighbours refused");
        RI_ASSERT(ri_auto_allowed(RI_LEVI_LFOKEY(0u, RI_LEVI_LP_SEMI)) &&
            ri_auto_allowed(RI_LEVI_LFOKEY(4u, RI_LEVI_LP_SEMI)) &&
            !ri_auto_allowed(RI_LEVI_LFOKEY(0u, 15u)) && !ri_auto_allowed(0x10F0u),
            "LP_SEMI keys allowed, param 15 refused");
    }

    /* ---- The engine dispatches the step block. ---- */
    {
        ri_engine_init(&e);
        memset(&ev, 0, sizeof ev);
        ev.type = RI_EV_AUTOMATION;
        ev.value = RI_LEVI_LSKEY(0u, RI_LEVI_LS_STEP);
        ev.flags = 40u;
        ri_engine_apply_event(&e, &ev);
        RI_ASSERT(e.slevi.lsc[0] == 40u, "engine cursor");
        ev.value = RI_LEVI_LSKEY(0u, RI_LEVI_LS_VALUE);
        ev.flags = 100u;
        ri_engine_apply_event(&e, &ev);
        RI_ASSERT(e.slevi.v[0].lfo[0].sval[40] == 36 && e.slevi.v[7].lfo[0].sval[40] == 36,
            "engine value every voice (%d)", (int)e.slevi.v[7].lfo[0].sval[40]);
        ev.value = RI_LEVI_LSKEY(4u, RI_LEVI_LS_STEP);
        ev.flags = 2u;
        ri_engine_apply_event(&e, &ev);
        RI_ASSERT(e.slevi.lsc[4] == 2u && e.slevi.lsc[0] == 40u, "LFO 5 block");
        ev.value = RI_LEVI_LSKEY(4u, RI_LEVI_LS_RAMP);
        ev.flags = 1u;
        ri_engine_apply_event(&e, &ev);
        RI_ASSERT(!e.slevi.v[0].lfo[4].sown && e.slevi.v[0].lfo[0].sown, "engine ramp");
    }

    /* ---- Panel: LFO pages 1..3, SEMI LOCK, STEP EDIT gate, step editor. ---- */
    {
        struct RISectLevi lv;
        uint16_t key = 0u;
        int val = -1;
        char buf[16];
        ri_slevi_init(&lv);
        ri_slevi_set_value(&lv, RI_SLEVI_MODULE, (int)RI_SLEVI_M_LFO1);
        RI_ASSERT(ri_slevi_page_count(&lv) == 3u, "LFO page count 3 (%u)",
            ri_slevi_page_count(&lv));
        lv.page = 2u;
        RI_ASSERT(strcmp(ri_slevi_page_title(&lv), "LFO 1  3/3") == 0, "step page title: %s",
            ri_slevi_page_title(&lv));
        lv.page = 0u;
        RI_ASSERT(strcmp(ri_slevi_page_title(&lv), "LFO 1  1/3") == 0, "params page title: %s",
            ri_slevi_page_title(&lv));
        /* page 2 slot 6 = SEMI LOCK (live), slot 7 = STEP EDIT gate */
        lv.page = 1u;
        RI_ASSERT(ri_slevi_enc_live(&lv, 6u) && ri_slevi_enc_live(&lv, 7u), "page 2 slots live");
        RI_ASSERT(strcmp(ri_slevi_enc_name(&lv, 6u), "SEMI LOCK") == 0 &&
            strcmp(ri_slevi_enc_name(&lv, 7u), "STEP EDIT") == 0, "page 2 names");
        RI_ASSERT(pan_key(&lv, RI_SLEVI_M_LFO1, 1u, 6u, &key, &val) == 1 &&
            key == RI_LEVI_LFOKEY(0u, RI_LEVI_LP_SEMI), "semi lock key %04x", key);
        RI_ASSERT(pan_key(&lv, RI_SLEVI_M_LFO1 + 2u, 1u, 6u, &key, &val) == 1 &&
            key == RI_LEVI_LFOKEY(2u, RI_LEVI_LP_SEMI), "semi lock is per LFO");
        ri_slevi_set_value(&lv, RI_SLEVI_MODULE, (int)RI_SLEVI_M_LFO1);
        lv.page = 1u;
        RI_ASSERT(ri_slevi_set_value(&lv, RI_SLEVI_ENC0 + 6u, 127u) == 1 &&
            lv.lfv[0][RI_LEVI_LP_SEMI] == 1u, "semi lock switch");
        ri_slevi_enc_text(&lv, 6u, buf, sizeof buf);
        RI_ASSERT(strcmp(buf, "LOCK") == 0, "semi text %s", buf);
        ri_slevi_set_value(&lv, RI_SLEVI_ENC0 + 6u, 0u);
        ri_slevi_enc_text(&lv, 6u, buf, sizeof buf);
        RI_ASSERT(strcmp(buf, "FREE") == 0, "semi text off %s", buf);
        /* the gate opens and closes the step editor page (knob or press) */
        RI_ASSERT(ri_slevi_set_value(&lv, RI_SLEVI_ENC0 + 7u, 127u) == 1 && lv.page == 2u &&
            lv.val[RI_SLEVI_LFEDIT] == 1, "STEP EDIT opens page 3 (%u)", lv.page);
        RI_ASSERT(strcmp(ri_slevi_enc_name(&lv, 0u), "STEP") == 0 &&
            strcmp(ri_slevi_enc_name(&lv, 1u), "VALUE") == 0 &&
            strcmp(ri_slevi_enc_name(&lv, 2u), "RAMP") == 0, "step editor names");
        RI_ASSERT(ri_slevi_enc_live(&lv, 0u) && ri_slevi_enc_live(&lv, 1u) &&
            ri_slevi_enc_live(&lv, 2u) && !ri_slevi_enc_live(&lv, 3u) &&
            ri_slevi_enc_live(&lv, 7u), "step editor slots");
        RI_ASSERT(ri_slevi_set_value(&lv, RI_SLEVI_ENC0 + 7u, 0u) == 1 && lv.page == 1u,
            "STEP EDIT closes page 3 (%u)", lv.page);
        lv.page = 1u;
        RI_ASSERT(ri_slevi_press(&lv, RI_SLEVI_LFEDIT) == 1 && lv.page == 2u,
            "STEP EDIT press opens page 3 (%u)", lv.page);
        RI_ASSERT(ri_slevi_press(&lv, RI_SLEVI_LFEDIT) == 1 && lv.page == 1u &&
            lv.val[RI_SLEVI_LFEDIT] == 0, "STEP EDIT press closes it (%u)", lv.page);
        /* step editor encoders: cursor, value, ramp -> LSKEY */
        ri_slevi_init(&lv);
        RI_ASSERT(pan_key(&lv, RI_SLEVI_M_LFO1 + 1u, 2u, 0u, &key, &val) == 1 &&
            key == RI_LEVI_LSKEY(1u, 0u) && val == 0, "cursor key %04x=%d", key, val);
        RI_ASSERT(pan_key(&lv, RI_SLEVI_M_LFO1 + 1u, 2u, 1u, &key, &val) == 1 &&
            key == RI_LEVI_LSKEY(1u, 1u) && val == 0, "value key %04x=%d", key, val);
        RI_ASSERT(pan_key(&lv, RI_SLEVI_M_LFO1 + 1u, 2u, 2u, &key, &val) == 1 &&
            key == RI_LEVI_LSKEY(1u, 2u) && val == 0, "ramp key %04x=%d", key, val);
        RI_ASSERT(ri_slevi_set_value(&lv, RI_SLEVI_ENC0 + 0u, 127u) == 1 &&
            lv.lfsc[1] == RI_LEVI_MAXSTEPS - 1u, "cursor knob -> 63 (%u)", lv.lfsc[1]);
        RI_ASSERT(ri_slevi_set_value(&lv, RI_SLEVI_ENC0 + 1u, 127u) == 1 &&
            lv.lfsv[1] == 127u, "value knob");
        RI_ASSERT(ri_slevi_set_value(&lv, RI_SLEVI_ENC0 + 1u, 0u) == 1 &&
            lv.lfsv[1] == 0u, "value knob at the bottom");
        RI_ASSERT(ri_slevi_set_value(&lv, RI_SLEVI_ENC0 + 2u, 127u) == 1 &&
            lv.lfsr[1] == 1u, "ramp gate");
        lv.page = 2u;
        ri_slevi_enc_text(&lv, 0u, buf, sizeof buf);
        RI_ASSERT(strcmp(buf, "64/64") == 0, "cursor text %s", buf);
        ri_slevi_enc_text(&lv, 1u, buf, sizeof buf);
        RI_ASSERT(strcmp(buf, "-100") == 0, "value text %s", buf);
        ri_slevi_enc_text(&lv, 2u, buf, sizeof buf);
        RI_ASSERT(strcmp(buf, "RAMP") == 0, "ramp text %s", buf);
        ri_slevi_set_value(&lv, RI_SLEVI_ENC0 + 2u, 0u);
        ri_slevi_enc_text(&lv, 2u, buf, sizeof buf);
        RI_ASSERT(strcmp(buf, "----") == 0, "ramp text off %s", buf);
        /* the encoder reset returns the three mirrors to their defaults */
        ri_slevi_set_value(&lv, RI_SLEVI_ENC0 + 0u, 127u);
        ri_slevi_set_value(&lv, RI_SLEVI_ENC0 + 1u, 127u);
        RI_ASSERT(ri_slevi_reset(&lv, RI_SLEVI_ENC0 + 0u) == 1 && lv.lfsc[1] == 0u &&
            ri_slevi_reset(&lv, RI_SLEVI_ENC0 + 1u) == 1 && lv.lfsv[1] == 64u,
            "reset gives cursor 0 and centre value (%u/%u)", lv.lfsc[1], lv.lfsv[1]);
        ri_slevi_enc_text(&lv, 1u, buf, sizeof buf);
        RI_ASSERT(strcmp(buf, "0") == 0, "centre text %s", buf);
        /* pages walk 1-2-3 and the mirrors survive the walk */
        lv.page = 0u;
        RI_ASSERT(ri_slevi_press(&lv, RI_SLEVI_PAGEDN) == 1 && lv.page == 1u &&
            ri_slevi_press(&lv, RI_SLEVI_PAGEDN) == 1 && lv.page == 2u &&
            ri_slevi_press(&lv, RI_SLEVI_PAGEUP) == 1 && lv.page == 1u,
            "LFO pages walk 1-2-3");
        /* the registry carries the gate */
        {
            const struct RICtlDef *d = ri_ctlreg_find((uint16_t)((RI_SEC_LEVI << 8) | RI_SLEVI_LFEDIT));
            RI_ASSERT(d && d->kind == RI_CK_SWITCH && d->engine_id == 0u && !d->automatable,
                "STEP EDIT row is UI-only");
        }
        RI_ASSERT(ri_slevi_set_value(&lv, RI_SLEVI_MODULE, (int)RI_SLEVI_M_OSC) == 1 &&
            lv.page == 0u, "module change recalls page 1");
        RI_ASSERT(ri_slevi_set_value(0, RI_SLEVI_ENC0, 1u) == 0, "NULL panel refused");
    }

    /* ---- Finite extremes: every LFO, every step, semi both ways. ---- */
    {
        uint32_t bad = 0u;
        step_lfo(&A, RI_LEVI_MAXSTEPS);
        for (i = 0u; i < 700u; i++) {
            uint32_t lf = (i * 7u) % RI_LEVI_NLFO;
            if (i % 3u == 0u)
                levi_set_lfo_ui(&A, 0u, lf, RI_LEVI_LP_SEMI, (uint8_t)(i % 2u));
            levi_set_lfo_stepctl(&A, lf, RI_LEVI_LS_STEP, (uint8_t)(i % 64u));
            levi_set_lfo_stepctl(&A, lf, RI_LEVI_LS_VALUE, (uint8_t)(i * 37u % 128u));
            if (i % 97u == 0u)
                levi_set_lfo_stepctl(&A, lf, RI_LEVI_LS_RAMP, 1u);
            if (i % 11u == 0u)
                levi_trigger(&A, 0u, (uint8_t)(36u + i % 60u));
            levi_voice_render_sum_stereo(&A, la, ra, BS, SR);
            for (k = 0u; k < BS; k++)
                if (!((la[k] > -8.0f && la[k] < 8.0f) && (ra[k] > -8.0f && ra[k] < 8.0f)))
                    bad = 1u;
        }
        RI_ASSERT(!bad, "extremes bounded");
    }

    RI_RESULT("t145_levi_lfostp");
}
