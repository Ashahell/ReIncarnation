/* t180_levi_laws — W4: audit residue becomes gated law.
 * Fast (no 7-minute sweep): every keyed Levi control and every live
 * encoder slot moves engine state for at least one value in its range,
 * and ~10 SILENT controls get the context that makes each audible (or a
 * measured by-design explanation). The W2 adoption law is t178.
 *
 * A. Step-editor init determinism (bug: ri_slevi_init left lfsc/lfsv/lfsr
 *    uninitialized, so the panel read stack garbage — the harness STEP
 *    values differed across identical runs).
 * B. Delivery law: rows take the audit's alternate value; slots take the
 *    audit's alternate-or-tries values. Seeded retries cover controls
 *    that need context (MPOS needs slots, CLEAR needs a note, RAMP needs
 *    an owned table, PARAM needs a destination). NOKEY slots must be the
 *    UI-only STEP EDIT gate (t145 pins the press).
 * C. Audibility spot contexts (two-note renders, baseline vs variant).
 */
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stddef.h>
#include "tests/helpers/ri_assert.h"
#include "gui/ctlreg.h"
#include "gui/sectlevi.h"
#include "engine/engine.h"
#include "engine/seq/ctlplane.h"
#include "engine/dsp/levi.h"
#include "engine/dsp/levi_fx.h"
#include "engine/dsp/levi_matrix.h"

static int engines_equal(const struct RIEngine *a, const struct RIEngine *b) {
    size_t r0 = offsetof(struct RIEngine, slevi) + offsetof(struct RILeviSet, fx) +
        offsetof(struct RILeviFx, rvl);
    size_t r1 = r0 + 2u * sizeof(struct RIReverb);
    if (memcmp(a, b, r0))
        return 0;
    return !memcmp((const unsigned char *)a + r1, (const unsigned char *)b + r1, sizeof *a - r1);
}

static void fresh_engine(struct RIEngine *e) {
    memset(e, 0, sizeof *e);
    ri_engine_init(e);
    ri_engine_defaults(e);
}

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

static int moves(uint16_t key, int val) {
    static struct RIEngine a, b;
    fresh_engine(&a);
    memcpy(&b, &a, sizeof b);
    apply(&b, key, val);
    return !engines_equal(&a, &b);
}

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

/* Two-note render; returns 1 when variant differs from baseline. */
#define RNON 24000u
#define RNOFF 12000u
#define RNT (RNON + RNOFF)
static float s_bl[RNT], s_br[RNT], s_l[RNT], s_r[RNT];

static void render2(struct RIEngine *e, int vel, float *l, float *r) {
    if (vel < 0) {
        levi_note_on(&e->slevi, 60u);
        levi_note_on(&e->slevi, 64u);
    } else {
        levi_note_vel(&e->slevi, 60u, (uint8_t)vel);
        levi_note_vel(&e->slevi, 64u, (uint8_t)vel);
    }
    ri_engine_render(e, l, r, RNON, 48000.0f);
    levi_note_off(&e->slevi, 60u);
    levi_note_off(&e->slevi, 64u);
    ri_engine_render(e, l + RNON, r + RNON, RNOFF, 48000.0f);
}

static void render_legato(struct RIEngine *e, float *l, float *r) {
    levi_note_on(&e->slevi, 60u);
    ri_engine_render(e, l, r, RNON / 2u, 48000.0f);
    levi_note_on(&e->slevi, 64u);   /* no release: legato into 64 */
    ri_engine_render(e, l + RNON / 2u, r + RNON / 2u, RNON / 2u, 48000.0f);
    levi_note_off(&e->slevi, 64u);
    ri_engine_render(e, l + RNON, r + RNON, RNOFF, 48000.0f);
}

static int differs(void) {
    uint32_t i;
    for (i = 0u; i < RNT; i++)
        if (s_l[i] != s_bl[i] || s_r[i] != s_br[i])
            return 1;
    return 0;
}

static void fresh_loaded(struct RIEngine *e) {
    fresh_engine(e);
    ri_engine_load(e, 0, 0u, (uint64_t)RNT, RI_ENGINE_SLEVI);
}

int main(void) {
    static struct RISectLevi p, q;
    uint32_t n, m, pg, k, u;
    uint32_t nstate = 0u;
    /* A. Step-editor storage starts deterministic (t145 pins fresh
     * VALUE at 0; RESET still uses the mod-val default 64). */
    memset(&p, 0xA5, sizeof p);
    ri_slevi_init(&p);
    for (u = 0u; u < RI_LEVI_NLFO; u++)
        RI_ASSERT(p.lfsc[u] == 0u && p.lfsv[u] == 0u && p.lfsr[u] == 0u,
            "lfo%u step init %u/%u/%u", u, p.lfsc[u], p.lfsv[u], p.lfsr[u]);
    /* B. Rows: the audit's alternate value moves fresh state. */
    for (n = 0u; n < ri_ctlreg_count(); n++) {
        const struct RICtlDef *d = ri_ctlreg_at(n);
        uint16_t key;
        int v, ok;
        if (!d || d->section != RI_SEC_LEVI)
            continue;
        key = ri_ctlreg_auto_id(d);
        if (!key)
            continue;
        v = d->def_v == d->max_v ? d->min_v : d->max_v;
        ok = moves(key, v);
        if (!ok && key == (RI_CTL_LEVI_SEQCLEAR & 0xFFFFu)) {
            /* Clearing an empty sequence is a correct no-op; seed one. */
            static struct RIEngine e1, e2;
            fresh_engine(&e1);
            e1.slevi.seq_t1[0].note[0] = 60u;
            memcpy(&e2, &e1, sizeof e2);
            apply(&e2, key, 1);
            ok = !engines_equal(&e1, &e2);
        }
        if (!ok) {
            nstate++;
            printf("STATE row %3u %-12s key %04x\n", d->reg_id & 0xFFu, d->legend, key);
        }
    }
    /* B. Slots: the audit's alternates move fresh state. A fresh panel
     * per slot (audit parity): slot sets must not leak into later slots. */
    for (m = 0u; m < RI_SLEVI_NMOD; m++) {
        uint32_t np0;
        open_page(&q, m, 0u);
        np0 = ri_slevi_page_count(&q);
        for (pg = 0u; pg < np0; pg++) {
            for (k = 0u; k < 8u; k++) {
                uint16_t key = 0u;
                int kv = 0, cur, alt, j, ok = 0;
                static const int tries[3] = { 64, 32, 96 };
                open_page(&q, m, pg);
                if (!ri_slevi_enc_live(&q, k))
                    continue;
                cur = ri_slevi_value(&q, RI_SLEVI_ENC0 + k);
                alt = cur >= 64 ? 0 : 127;
                if (ri_slevi_set_value(&q, RI_SLEVI_ENC0 + k, alt))
                    ok = 1;
                else {
                    for (j = 0; j < 3 && !ok; j++)
                        ok = ri_slevi_set_value(&q, RI_SLEVI_ENC0 + k, tries[j]);
                }
                if (!ok) {
                    /* NOMOVE: only PARAM slots (their param list is empty
                     * with no destination chosen; the proofs below show
                     * they move once one is set). */
                    RI_ASSERT(!strcmp(ri_slevi_enc_name(&q, k), "PARAM"),
                        "nomove not a param %u/%u/%u", m, pg, k);
                    continue;
                }
                if (!resolve(&q, RI_SLEVI_ENC0 + k, &key, &kv)) {
                    /* NOKEY: only the UI-only STEP EDIT gate (t145 pins
                     * the press that pages to the step editor). */
                    RI_ASSERT(ri_slevi_ctl_idx(&q, RI_SLEVI_ENC0 + k) == RI_SLEVI_LFEDIT,
                        "nokey not the gate %u/%u/%u", m, pg, k);
                    continue;
                }
                ok = moves(key, kv);
                if (!ok && (key & 3u) == 2u && ((key & 0xFF00u) == 0x1300u || key == 0x1402u)) {
                    /* RAMP reasserts the default analytic ramp (identity);
                     * own a step first, then restoring moves. */
                    static struct RIEngine e1, e2;
                    fresh_engine(&e1);
                    apply(&e1, (uint16_t)(key - 1u), 0);
                    memcpy(&e2, &e1, sizeof e2);
                    apply(&e2, key, 1);
                    ok = !engines_equal(&e1, &e2);
                }
                if (!ok && key == (RI_CTL_LEVI_SEQCLEAR & 0xFFFFu)) {
                    static struct RIEngine e1, e2;
                    fresh_engine(&e1);
                    e1.slevi.seq_t1[0].note[0] = 60u;
                    memcpy(&e2, &e1, sizeof e2);
                    apply(&e2, key, 1);
                    ok = !engines_equal(&e1, &e2);
                }
                if (!ok) {
                    nstate++;
                    printf("STATE enc m%u pg%u slot%u key %04x=%d\n", m, pg, k + 1u, key, kv);
                }
            }
        }
    }
    RI_ASSERT(nstate == 0u, "%u state-dead controls", nstate);
    /* PARAM slots move once a destination is chosen (matrix + macro). */
    {
        static struct RISectLevi mp;
        static struct RIEngine e1, e2;
        uint16_t key;
        int kv;
        open_page(&mp, RI_SLEVI_M_MATRIX, 0u);
        RI_ASSERT(ri_slevi_set_value(&mp, RI_SLEVI_ENC0 + 1u, 127) == 1, "mx module sets");
        RI_ASSERT(ri_slevi_set_value(&mp, RI_SLEVI_ENC0 + 2u, 127) == 1, "mx param moves with dest");
        RI_ASSERT(resolve(&mp, RI_SLEVI_ENC0 + 2u, &key, &kv) == 1, "mx param resolves");
        fresh_engine(&e1);
        apply(&e1, (uint16_t)(RI_LEVI_MXKEY(0u, 0u) & 0xFFFFu), 24);
        apply(&e1, (uint16_t)(RI_LEVI_MXKEY(0u, 1u) & 0xFFFFu), 34);
        memcpy(&e2, &e1, sizeof e2);
        apply(&e2, key, kv);
        RI_ASSERT(!engines_equal(&e1, &e2), "mx param moves engine");
        open_page(&mp, RI_SLEVI_M_MACRO, 2u);
        RI_ASSERT(ri_slevi_set_value(&mp, RI_SLEVI_ENC0 + 0u, 127) == 1, "mr module sets");
        RI_ASSERT(ri_slevi_set_value(&mp, RI_SLEVI_ENC0 + 1u, 127) == 1, "mr param moves with dest");
        RI_ASSERT(resolve(&mp, RI_SLEVI_ENC0 + 1u, &key, &kv) == 1, "mr param resolves");
        fresh_engine(&e1);
        apply(&e1, (uint16_t)(RI_LEVI_MRKEY(0u, 0u, 0u) & 0xFFFFu), 34);
        memcpy(&e2, &e1, sizeof e2);
        apply(&e2, key, kv);
        RI_ASSERT(!engines_equal(&e1, &e2), "mr param moves engine");
    }
    /* C1. Velocity amount: needs velocity input (notes at vel 64). */
    {
        static struct RIEngine e;
        fresh_loaded(&e);
        render2(&e, 64, s_bl, s_br);
        fresh_loaded(&e);
        apply(&e, RI_CTL_LEVI_DVEL, 127);
        render2(&e, 64, s_l, s_r);
        RI_ASSERT(differs(), "dvel moves vel-64 notes");
    }
    /* C2. Vibrato rate: needs depth. */
    {
        static struct RIEngine e;
        fresh_loaded(&e);
        apply(&e, RI_CTL_LEVI_VVIBAMT, 127);
        apply(&e, RI_CTL_LEVI_VVIBRATE, 0);
        render2(&e, -1, s_bl, s_br);
        fresh_loaded(&e);
        apply(&e, RI_CTL_LEVI_VVIBAMT, 127);
        apply(&e, RI_CTL_LEVI_VVIBRATE, 127);
        render2(&e, -1, s_l, s_r);
        RI_ASSERT(differs(), "vibrate moves with depth");
    }
    /* C3. Glide time: needs mono legato. */
    {
        static struct RIEngine e;
        fresh_loaded(&e);
        apply(&e, RI_CTL_LEVI_POLYMODE, RI_LEVI_POLY_MONO);
        apply(&e, RI_CTL_LEVI_VGLIDE, 1);
        apply(&e, RI_CTL_LEVI_VGLTIME, 0);
        render_legato(&e, s_bl, s_br);
        fresh_loaded(&e);
        apply(&e, RI_CTL_LEVI_POLYMODE, RI_LEVI_POLY_MONO);
        apply(&e, RI_CTL_LEVI_VGLIDE, 1);
        apply(&e, RI_CTL_LEVI_VGLTIME, 127);
        render_legato(&e, s_l, s_r);
        RI_ASSERT(differs(), "glide moves legato mono");
    }
    /* C4. Bend range: needs bend input. */
    {
        static struct RIEngine e;
        fresh_loaded(&e);
        levi_bend(&e.slevi, 2.0f);
        apply(&e, RI_CTL_LEVI_VBENDRNG, 0);
        render2(&e, -1, s_bl, s_br);
        fresh_loaded(&e);
        levi_bend(&e.slevi, 2.0f);
        apply(&e, RI_CTL_LEVI_VBENDRNG, 11);
        render2(&e, -1, s_l, s_r);
        RI_ASSERT(differs(), "bend range moves bent notes");
    }
    /* C5. Matrix route: NOTE keytrack into DFILT cutoff with depth. */
    {
        static struct RIEngine e;
        uint32_t src_ui = ri_levi_ms_to_ui(RI_LEVI_MS_NOTE);
        RI_ASSERT(src_ui != 0u, "note has a ui source");
        fresh_loaded(&e);
        apply(&e, (uint16_t)(RI_LEVI_MXKEY(0u, 0u) & 0xFFFFu), (int)src_ui);
        apply(&e, (uint16_t)(RI_LEVI_MXKEY(0u, 1u) & 0xFFFFu), RI_LEVI_DM_DFILT);
        apply(&e, (uint16_t)(RI_LEVI_MXKEY(0u, 2u) & 0xFFFFu), 0u);
        apply(&e, (uint16_t)(RI_LEVI_MXKEY(0u, 3u) & 0xFFFFu), 64);
        render2(&e, -1, s_bl, s_br);
        fresh_loaded(&e);
        apply(&e, (uint16_t)(RI_LEVI_MXKEY(0u, 0u) & 0xFFFFu), (int)src_ui);
        apply(&e, (uint16_t)(RI_LEVI_MXKEY(0u, 1u) & 0xFFFFu), RI_LEVI_DM_DFILT);
        apply(&e, (uint16_t)(RI_LEVI_MXKEY(0u, 2u) & 0xFFFFu), 0u);
        apply(&e, (uint16_t)(RI_LEVI_MXKEY(0u, 3u) & 0xFFFFu), 127);
        render2(&e, -1, s_l, s_r);
        RI_ASSERT(differs(), "keytrack->cutoff route is audible");
    }
    /* C6. Delay type: needs time passing signal. */
    {
        static struct RIEngine e;
        fresh_loaded(&e);
        apply(&e, RI_CTL_LEVI_DBYPASS, 1);
        apply(&e, RI_CTL_LEVI_DLYTIME, 64);
        apply(&e, RI_CTL_LEVI_DLYDRYWET, 96);
        apply(&e, RI_CTL_LEVI_DLYTYPE, 0);
        render2(&e, -1, s_bl, s_br);
        fresh_loaded(&e);
        apply(&e, RI_CTL_LEVI_DBYPASS, 1);
        apply(&e, RI_CTL_LEVI_DLYTIME, 64);
        apply(&e, RI_CTL_LEVI_DLYDRYWET, 96);
        apply(&e, RI_CTL_LEVI_DLYTYPE, 2);
        render2(&e, -1, s_l, s_r);
        RI_ASSERT(differs(), "delay type moves wet signal");
    }
    /* C7. Per-op param on an idle op is silent by design; the same path
     * on a live op is audible (proves delivery, bounds the silence). */
    {
        static struct RIEngine e;
        fresh_loaded(&e);
        RI_ASSERT(e.slevi.v[0].live[4] == 0u, "op 5 idle in DUO (got %u)", e.slevi.v[0].live[4]);
        render2(&e, -1, s_bl, s_br);
        fresh_loaded(&e);
        apply(&e, (uint16_t)(RI_LEVI_OPKEY(4u, RI_LEVI_OP_COARSE) & 0xFFFFu), 100);
        render2(&e, -1, s_l, s_r);
        RI_ASSERT(!differs(), "idle-op param silent");
        fresh_loaded(&e);
        render2(&e, -1, s_bl, s_br);
        fresh_loaded(&e);
        apply(&e, (uint16_t)(RI_LEVI_OPKEY(0u, RI_LEVI_OP_COARSE) & 0xFFFFu), 100);
        render2(&e, -1, s_l, s_r);
        RI_ASSERT(differs(), "live-op param audible");
    }
    /* C8. Analog Cutoff at 127 (~14.5 kHz) vs the 12 kHz start: measured
     * identical on two dark DUO notes, so the SILENT line is expected. */
    {
        static struct RIEngine e;
        fresh_loaded(&e);
        render2(&e, -1, s_bl, s_br);
        fresh_loaded(&e);
        apply(&e, (uint16_t)(RI_CTL_LEVI_CUTOFF2 & 0xFFFFu), 127);
        render2(&e, -1, s_l, s_r);
        RI_ASSERT(!differs(), "analog cutoff 127 identical here");
    }
    /* C9. Width on the default patch: measured identical (mono content);
     * needs a wide patch — ear test on the Dell. */
    {
        static struct RIEngine e;
        fresh_loaded(&e);
        render2(&e, -1, s_bl, s_br);
        fresh_loaded(&e);
        apply(&e, RI_CTL_LEVI_VWIDTH, 127);
        render2(&e, -1, s_l, s_r);
        RI_ASSERT(!differs(), "width identical on mono patch");
    }
    /* C10. Reverb tone extremes with wet signal. */
    {
        static struct RIEngine e;
        fresh_loaded(&e);
        apply(&e, RI_CTL_LEVI_RBYPASS, 1);
        apply(&e, RI_CTL_LEVI_RDRYWET, 96);
        apply(&e, (uint16_t)(RI_CTL_LEVI_RTONE & 0xFFFFu), 0);
        render2(&e, -1, s_bl, s_br);
        fresh_loaded(&e);
        apply(&e, RI_CTL_LEVI_RBYPASS, 1);
        apply(&e, (uint16_t)(RI_CTL_LEVI_RTONE & 0xFFFFu), 127);
        render2(&e, -1, s_l, s_r);
        RI_ASSERT(differs(), "reverb tone moves wet signal");
    }
    RI_RESULT("levilaws");
}
