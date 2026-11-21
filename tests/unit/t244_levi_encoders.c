/* t244_levi_encoders — every live Leviasynth page encoder reaches the engine
 * (census door D2, which t243 reports as NOTMEASURED).
 *
 * Walks every MODULE page of the Levi section model (ri_slevi_*), turns each
 * live encoder slot to another value, resolves it exactly as RIAPP's
 * sync_values does (explicit per-op key, else the registry row of the slot's
 * target) and applies it to a fresh engine. Every slot must move the engine,
 * or be one of three exceptions, each proven rather than listed:
 *   STEP EDIT  (RI_SLEVI_LFEDIT) pages to the LFO step editor: panel-only,
 *              so it must resolve to NO key;
 *   RAMP       (LFO step editor) is an action, not a state: on a fresh
 *              engine it is a no-op by design, so it must instead clear a
 *              step edit made first;
 *   CLEAR      (sequencer) is an action too: it must empty a step that
 *              was filled first (a fresh sequencer is already empty);
 *   PARAM      of a matrix / macro route has no choices while the route's
 *              MODULE is NONE (0 params) or ALGO (1 param): it must then
 *              move, resolve and move the engine once the route is pointed
 *              at OSC 1.
 * Anything else fails and is named. The live-slot count is pinned, so a
 * page that loses its encoders is a failure too.
 *
 * Not measured here: whether a delivered change is AUDIBLE (the 2026-10-08
 * advisor harness, docs/evidence/levi-wiring/, renders that; silence in a
 * two-note render is not proof of a bug).
 */
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stddef.h>
#include "tests/helpers/ri_assert.h"
#include "gui/ctlreg.h"
#include "gui/sectlevi.h"
#include "engine/engine.h"
#include "engine/dsp/levi.h"
#include "engine/dsp/levi_fx.h"
#include "engine/dsp/levi_matrix.h"

#define T244_LIVE_SLOTS 893u /* 2026-11-21; change only with the page model */

static struct RIEngine EA, EB;

static void fresh(struct RIEngine *e) {
    memset(e, 0, sizeof *e);
    ri_engine_init(e);
    ri_engine_defaults(e);
}

static void ev_apply(struct RIEngine *e, uint16_t key, int val) {
    struct RIEvent ev;
    memset(&ev, 0, sizeof ev);
    ev.type = RI_EV_AUTOMATION;
    ev.value = key;
    ev.flags = (uint16_t)(val & 127);
    ri_engine_apply_event(e, &ev);
}

/* Same equality as t116/t243 (skips the Levi reverb line pointers). */
static int engines_equal(const struct RIEngine *a, const struct RIEngine *b) {
    size_t r0 = offsetof(struct RIEngine, slevi) +
        offsetof(struct RILeviSet, fx) + offsetof(struct RILeviFx, rvl);
    size_t r1 = r0 + 2u * sizeof(struct RIReverb);
    const struct RILeviFx *fa = &a->slevi.fx, *fb = &b->slevi.fx;
    if (memcmp(a, b, r0))
        return 0;
    if (memcmp((const unsigned char *)a + r1, (const unsigned char *)b + r1, sizeof *a - r1))
        return 0;
    return fa->rvl.fb == fb->rvl.fb && fa->rvr.fb == fb->rvr.fb &&
        fa->rvl.cap == fb->rvl.cap && fa->rvr.cap == fb->rvr.cap &&
        !memcmp(fa->rvl.len, fb->rvl.len, sizeof fa->rvl.len) &&
        !memcmp(fa->rvr.len, fb->rvr.len, sizeof fa->rvr.len);
}

static int state_moves(uint16_t key, int val) {
    fresh(&EA);
    fresh(&EB);
    ev_apply(&EA, key, val);
    return !engines_equal(&EA, &EB);
}

/* The key + value RIAPP sends for a slot (app/riapp.c sync_values). */
static int resolve(const struct RISectLevi *p, uint32_t idx, uint16_t *key, int *kv) {
    const struct RICtlDef *d;
    uint32_t t;
    if (ri_slevi_ctl_key(p, idx, key, kv) == 1)
        return 1;
    t = ri_slevi_ctl_idx(p, idx);
    d = ri_ctlreg_find((uint16_t)((RI_SEC_LEVI << 8) | t));
    if (!d)
        return 0;
    *key = ri_ctlreg_auto_id(d);
    *kv = p->val[t];
    return *key != 0u;
}

static void open_page(struct RISectLevi *q, uint32_t m, uint32_t pg) {
    uint32_t i;
    ri_slevi_init(q);
    ri_slevi_set_value(q, RI_SLEVI_MODULE, (int)m);
    for (i = 0; i < pg; i++)
        ri_slevi_press(q, RI_SLEVI_PAGEDN);
}

/* Turn a slot off its current value; 1 when the model moved. */
static int turn(struct RISectLevi *q, uint32_t k) {
    static const int tries[4] = { 127, 0, 64, 96 };
    int cur = ri_slevi_value(q, RI_SLEVI_ENC0 + k), j;
    if (ri_slevi_set_value(q, RI_SLEVI_ENC0 + k, cur >= 64 ? 0 : 127))
        return 1;
    for (j = 0; j < 4; j++)
        if (ri_slevi_set_value(q, RI_SLEVI_ENC0 + k, tries[j]))
            return 1;
    return 0;
}

/* PARAM exception: the route's MODULE (the slot before PARAM) has <= 1
 * param; point it at OSC 1 and the PARAM slot must work end to end. */
static int param_exception(struct RISectLevi *q, uint32_t k, const char *title) {
    uint16_t mk = 0, key = 0;
    int mkv = -1, kv = 0, v;
    if (k == 0u || resolve(q, RI_SLEVI_ENC0 + k - 1u, &mk, &mkv) == 0)
        return 0;
    if (ri_levi_dm_nparam((uint32_t)mkv) > 1u)
        return 0; /* a module with choices: a PARAM that cannot move is a bug */
    for (v = 0; v < 128; v++) {
        ri_slevi_set_value(q, RI_SLEVI_ENC0 + k - 1u, v);
        if (resolve(q, RI_SLEVI_ENC0 + k - 1u, &mk, &mkv) && mkv == 1)
            break;
    }
    if (mkv != 1 || !turn(q, k) || !resolve(q, RI_SLEVI_ENC0 + k, &key, &kv)) {
        printf("PARAM   %-22s slot %u: dead with the route on OSC 1\n", title, k + 1u);
        return 0;
    }
    fresh(&EA);
    fresh(&EB);
    ev_apply(&EA, mk, mkv);
    ev_apply(&EB, mk, mkv);
    ev_apply(&EA, key, kv);
    return !engines_equal(&EA, &EB);
}

/* RAMP exception: a step edit moves the engine and RAMP takes it back. */
static int ramp_exception(uint16_t key) {
    uint16_t base = (uint16_t)(key & ~3u);
    uint32_t lf = (key >> 8) == 0x14u ? 4u : (key >> 2) & 3u;
    if ((key & 3u) != RI_LEVI_LS_RAMP)
        return 0;
    fresh(&EA);
    fresh(&EB);
    ev_apply(&EA, (uint16_t)(base | RI_LEVI_LS_STEP), 3);
    ev_apply(&EA, (uint16_t)(base | RI_LEVI_LS_VALUE), 127);
    if (engines_equal(&EA, &EB) || !EA.slevi.glfo[lf].sown)
        return 0;
    ev_apply(&EA, key, 1);
    return EA.slevi.glfo[lf].sown == 0u && EA.slevi.v[0].lfo[lf].sown == 0u;
}

/* CLEAR exception: a filled step and macro lane are emptied. */
static int clear_exception(uint16_t key) {
    if (key != RI_CTL_LEVI_SEQCLEAR)
        return 0;
    fresh(&EA);
    EA.slevi.seq_t1[0].note[0] = 60u;
    EA.slevi.seq_macro[0][0] = 99u;
    ev_apply(&EA, key, 1);
    return EA.slevi.seq_t1[0].note[0] == 255u && EA.slevi.seq_macro[0][0] == 0u;
}

int main(void) {
    uint32_t m, pg, k, np;
    uint32_t live = 0, ok = 0, x_step = 0, x_ramp = 0, x_param = 0, x_clear = 0, bad = 0;
    for (m = 0; m < RI_SLEVI_NMOD; m++) {
        struct RISectLevi p;
        open_page(&p, m, 0);
        np = ri_slevi_page_count(&p);
        for (pg = 0; pg < np; pg++) {
            for (k = 0; k < 8u; k++) {
                struct RISectLevi q;
                char title[64];
                uint16_t key = 0;
                int kv = 0;
                open_page(&q, m, pg);
                if (!ri_slevi_enc_live(&q, k))
                    continue;
                live++;
                snprintf(title, sizeof title, "%s", ri_slevi_page_title(&q));
                if (ri_slevi_ctl_idx(&q, RI_SLEVI_ENC0 + k) == RI_SLEVI_LFEDIT) {
                    if (!resolve(&q, RI_SLEVI_ENC0 + k, &key, &kv)) {
                        x_step++;
                        continue;
                    }
                    printf("KEYED   %-22s slot %u: STEP EDIT reaches the engine (%04x)\n",
                        title, k + 1u, key);
                    bad++;
                    continue;
                }
                if (!turn(&q, k)) {
                    if (param_exception(&q, k, title)) {
                        x_param++;
                        continue;
                    }
                    printf("NOMOVE  %-22s slot %u %s\n", title, k + 1u, ri_slevi_enc_name(&q, k));
                    bad++;
                    continue;
                }
                if (!resolve(&q, RI_SLEVI_ENC0 + k, &key, &kv)) {
                    printf("NOKEY   %-22s slot %u %s\n", title, k + 1u, ri_slevi_enc_name(&q, k));
                    bad++;
                    continue;
                }
                if (state_moves(key, kv)) {
                    ok++;
                    continue;
                }
                if (ramp_exception(key)) {
                    x_ramp++;
                    continue;
                }
                if (clear_exception(key)) {
                    x_clear++;
                    continue;
                }
                printf("DEAD    %-22s slot %u %s key %04x=%d\n", title, k + 1u,
                    ri_slevi_enc_name(&q, k), key, kv);
                bad++;
            }
        }
    }
    printf("live %u: ok %u, step-edit %u, ramp %u, clear %u, param %u, bad %u\n", live, ok,
        x_step, x_ramp, x_clear, x_param, bad);
    RI_ASSERT(live == T244_LIVE_SLOTS, "live encoder slots %u, pinned %u", live, T244_LIVE_SLOTS);
    RI_ASSERT(bad == 0u, "%u encoder slots do not reach the engine", bad);
    RI_ASSERT(x_step == 10u, "step-edit gates %u (5 LFOs x 2 pages)", x_step);
    RI_ASSERT(x_ramp == 5u, "RAMP actions %u (one per LFO)", x_ramp);
    RI_ASSERT(x_clear == 1u, "sequencer CLEAR actions %u", x_clear);
    RI_ASSERT(ok > 0u && x_param > 0u, "the walk reached real and route slots");

    /* Controls: the checks can fail. */
    RI_ASSERT(!state_moves(0x0EB7u, 0), "control: a no-op write reads dead");
    RI_ASSERT(!ramp_exception(0x1301u), "control: a VALUE key is not a RAMP");
    RI_ASSERT(!clear_exception(0x0EB6u), "control: another key is not CLEAR");
    RI_RESULT("levi_encoders");
}
