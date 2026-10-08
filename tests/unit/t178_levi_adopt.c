/* t178_levi_adopt — W2: the Leviasynth adopts its panel at startup.
 * Bug 1: RIAPP's startup burst never sent the Levi section, so the panel
 * showed values the engine was not running until first touch (audit:
 * 19 registry DEFAULT + 201 encoder DEFAULT lines). ri_panel_levi_adopt
 * (gui/panelctl.c, the same bridge mapping live knob turns use) sends
 * every Levi panel value through the control plane. This test builds the
 * startup the way app/riapp.c does (fresh panel + fresh engine + burst +
 * drain), runs the adoption, then replays the audit's DEFAULT check for
 * every Levi key against the adopted state: sending the panel's current
 * value must change nothing.
 *
 * Method: the adopted state is snapshotted once; each key is applied to
 * a COPY (sequential applies would poison later checks: preset/algo
 * bundle keys rewrite sibling state, so check N must not see check N-1).
 *
 * Exempt keys (re-sending the panel value legitimately moves state):
 * - 0x0e03 (legacy RATIO) and 0x0e0c..0x0e0f (legacy voice ADSR): v1
 *   fan-out rows with no MODULE-page slot (page_reaches == 0). They write
 *   per-op/per-bank engine fields that the visible per-op controls also
 *   own with different values (probe: re-send moves adopted state). The
 *   per-op truth wins at startup; a front-panel touch fans out, which is
 *   the live "set all" semantic, not a startup jump. 0x0e0e passes today
 *   (uniform sustain defaults) and is exempt as the same class.
 * - 0x0e88/0x0e8e (Pre/Post-FX PRESET): bundle selects. Reloading the
 *   bundle restores the factory params over the panel's param knobs, by
 *   design (a preset touch is supposed to retune the params). Adoption
 *   sends bundles before params so every PARAM knob is jump-free; the
 *   preset register itself matches (probed: preset 0 throughout). A live
 *   turn of an unchanged preset value never sends, so re-send identity
 *   cannot hold and is not required.
 * RED on HEAD (no adoption): the 19 + 201 mismatches reappear.
 */
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stddef.h>
#include "tests/helpers/ri_assert.h"
#include "gui/ctlreg.h"
#include "gui/panelctl.h"
#include "gui/sectlevi.h"
#include "engine/engine.h"
#include "engine/seq/ctlplane.h"
#include "engine/seq/autolane.h"
#include "engine/dsp/levi.h"
#include "engine/dsp/levi_fx.h"

/* Whole-engine compare that sees around the reverb line pointers
 * (same trick as t116: per-instance views of the set's backing). */
static int engines_equal(const struct RIEngine *a, const struct RIEngine *b) {
    size_t r0 = offsetof(struct RIEngine, slevi) + offsetof(struct RILeviSet, fx) +
        offsetof(struct RILeviFx, rvl);
    size_t r1 = r0 + 2u * sizeof(struct RIReverb);
    if (memcmp(a, b, r0))
        return 0;
    return !memcmp((const unsigned char *)a + r1, (const unsigned char *)b + r1, sizeof *a - r1);
}

static struct RIControlPlane s_pl;
static struct RIEngine s_eng;
static struct RIEvent s_ev[RI_CTL_CAP];
static uint32_t s_seq;
static uint64_t s_sample;

static uint32_t test_drain(void *ctx) {
    uint32_t nd, q;
    (void)ctx;
    nd = ri_ctl_drain(&s_pl, s_ev, RI_CTL_CAP, s_sample, &s_seq);
    for (q = 0u; q < nd; q++)
        ri_engine_apply_event(&s_eng, &s_ev[q]);
    s_sample += 64u;
    return nd;
}

static void fresh_engine(struct RIEngine *e) {
    memset(e, 0, sizeof *e);
    ri_engine_init(e);
    ri_engine_defaults(e);
}

/* Bridge-clamped apply, the way ri_panel_ctl_send(_key) would deliver it. */
static void apply(struct RIEngine *e, uint16_t key, int val) {
    struct RIEvent ev;
    if (val < 0)
        val = 0;
    if (val > 127)
        val = 127;
    memset(&ev, 0, sizeof ev);
    ev.type = RI_EV_AUTOMATION;
    ev.value = key;
    ev.flags = (uint16_t)val;
    ri_engine_apply_event(e, &ev);
}

/* Keys exempt from re-send identity, with the reason above. */
static int exempt(uint16_t key) {
    return key == 0x0e03u || (key >= 0x0e0cu && key <= 0x0e0fu) ||
        key == 0x0e88u || key == 0x0e8eu;
}

/* Audit-style resolve of an encoder slot to its lane key + panel value. */
static int resolve(const struct RISectLevi *p, uint32_t idx, uint16_t *key, int *kv) {
    const struct RICtlDef *d;
    uint32_t t;
    if (ri_slevi_ctl_key(p, idx, key, kv) == 1)
        return 1;
    t = ri_slevi_ctl_idx(p, idx);
    d = ri_ctlreg_find((uint16_t)((RI_SEC_LEVI << 8) | (t & 0xFFu)));
    if (!d)
        return 0;
    *key = ri_ctlreg_auto_id(d);
    *kv = p->val[t & 0xFFu];
    return *key != 0u;
}

static void open_page(struct RISectLevi *q, uint32_t m, uint32_t pg) {
    uint32_t i;
    ri_slevi_init(q);
    ri_slevi_set_value(q, RI_SLEVI_MODULE, (int)m);
    for (i = 0u; i < pg; i++)
        ri_slevi_press(q, RI_SLEVI_PAGEDN);
}

int main(void) {
    static struct RISectLevi p, q;
    static struct RIEngine adopted, w;
    uint32_t n, m, pg, k, o;
    uint32_t sent, nrows = 0u, nslots = 0u, nmiss = 0u, nexempt = 0u;
    /* The startup the way app/riapp.c builds it: fresh panel, fresh
     * engine, empty plane, adoption burst, drain. */
    ri_slevi_init(&p);
    fresh_engine(&s_eng);
    ri_ctl_init(&s_pl);
    s_seq = 0u;
    s_sample = 0u;
    sent = ri_panel_levi_adopt(&p, &s_pl, 0, test_drain);
    RI_ASSERT(ri_ctl_pending(&s_pl) == 0u, "plane drained");
    RI_ASSERT(s_pl.dropped == 0u, "no overflow drops");
    RI_ASSERT(s_pl.refused == 0u, "no refused Levi keys");
    RI_ASSERT(sent > 0u, "adoption sent %u", sent);
    /* Navigation restored to the startup view. */
    RI_ASSERT(p.val[RI_SLEVI_MODULE] == (int16_t)RI_SLEVI_M_OSC && p.page == 0u &&
        p.opsel == 0u, "nav restored");
    memcpy(&adopted, &s_eng, sizeof adopted);
    /* (a) every keyed registry row: panel value changes nothing. */
    for (n = 0u; n < ri_ctlreg_count(); n++) {
        const struct RICtlDef *d = ri_ctlreg_at(n);
        uint16_t key;
        int v;
        if (!d || d->section != RI_SEC_LEVI)
            continue;
        key = ri_ctlreg_auto_id(d);
        if (!key)
            continue;
        nrows++;
        if (exempt(key)) {
            nexempt++;
            continue;
        }
        v = ri_slevi_value(&p, d->reg_id & 0xFFu);
        memcpy(&w, &adopted, sizeof w);
        apply(&w, key, v);
        if (!engines_equal(&adopted, &w)) {
            nmiss++;
            printf("ADOPT-MISS row %3u %-12s key %04x=%d\n", d->reg_id & 0xFFu, d->legend, key, v);
        }
    }
    /* (b) every live encoder slot on every module page (all 8
     * oscillators for the OSC module, as the adoption walks). */
    for (m = 0u; m < RI_SLEVI_NMOD; m++) {
        for (o = 0u; o < (m == RI_SLEVI_M_OSC ? RI_LEVI_NOPS : 1u); o++) {
            uint32_t np;
            open_page(&q, m, 0u);
            if (m == RI_SLEVI_M_OSC && q.opsel != (uint8_t)o)
                ri_slevi_set_value(&q, RI_SLEVI_OPSEL, (int)o);
            np = ri_slevi_page_count(&q);
            for (pg = 0u; pg < np; pg++) {
                if (pg > 0u)
                    ri_slevi_press(&q, RI_SLEVI_PAGEDN);
                for (k = 0u; k < 8u; k++) {
                    uint16_t key = 0u;
                    int kv = 0;
                    if (!ri_slevi_enc_live(&q, k))
                        continue;
                    if (!resolve(&q, RI_SLEVI_ENC0 + k, &key, &kv))
                        continue;
                    nslots++;
                    if (exempt(key)) {
                        nexempt++;
                        continue;
                    }
                    memcpy(&w, &adopted, sizeof w);
                    apply(&w, key, kv);
                    if (!engines_equal(&adopted, &w)) {
                        nmiss++;
                        printf("ADOPT-MISS enc m%u op%u pg%u slot%u key %04x=%d\n",
                            m, o, pg, k + 1u, key, kv);
                    }
                }
            }
        }
    }
    RI_ASSERT(nrows == 180u, "keyed Levi rows changed (%u)", nrows);
    RI_ASSERT(nslots > 900u, "enumerated %u page slots", nslots);
    RI_ASSERT(sent == nrows + nslots, "adoption covers every key (%u sent, %u rows + %u slots)",
        sent, nrows, nslots);
    RI_ASSERT(nexempt == 9u, "exempt set changed (%u)", nexempt);
    RI_ASSERT(nmiss == 0u, "adoption misses %u keys", nmiss);
    RI_RESULT("leviadopt");
}
