/* levi_audit.c — advisor wiring audit of every Leviasynth control (2026-10-08).
 *
 * NOT a gated test: a reference harness for the levi-midi work. It walks
 *   (a) every registry row of RI_SEC_LEVI with a lane key, and
 *   (b) every live encoder slot on every MODULE page (ri_slevi_* page model),
 * turns each to an alternate value and reports:
 *   STATE   the engine state did not change at all (not delivered)  -> bug
 *   SILENT  state changed but a 2-note render is bit-identical, in the
 *           default context AND in every context below -> needs an ear
 *           test or a richer context; not proof of a bug
 *   DEFAULT the panel's own default, sent to a fresh engine, changes the
 *           engine state -> the panel shows a value the engine is not at
 *           (RIAPP's startup burst skips the Levi section).
 *
 * Build (after scripts/ri_build_host.sh all):
 *   gcc -std=c99 -O2 -I. -pthread -o /tmp/levi_audit \
 *     docs/evidence/levi-wiring/2026-10-08-advisor-audit/levi_audit.c \
 *     <every .o file in /tmp/ri/build> -lm -lpng
 *   /tmp/levi_audit            (defaults + state + default-context audio, ~1 min)
 *   /tmp/levi_audit ctx        (adds the context sweep, ~7 min)
 */
#include <stdio.h>
#include <string.h>
#include <stddef.h>
#include <math.h>
#include "gui/ctlreg.h"
#include "gui/sectlevi.h"
#include "engine/engine.h"
#include "engine/seq/sched.h"
#include "engine/dsp/levi.h"
#include "engine/dsp/levi_fx.h"

#define SR 48000.0f
#define NON 48000u
#define NOFF 24000u
#define NT (NON + NOFF)

static struct RIEngine E, EA, EB;
static float L[NT], R_[NT], BL[NT], BR[NT];
static int with_ctx;

/* Contexts applied before the control under test (and before the baseline). */
#define NCTX 9
static const uint16_t CTX[NCTX][12][2] = {
    { { 0 } },
    /* FX blocks on, types set, wet */
    { { 0x0E7D, 1 }, { 0x0E86, 1 }, { 0x0E8C, 1 }, { 0x0E92, 1 }, { 0x0E87, 3 }, { 0x0E8D, 3 },
      { 0x0E7B, 64 }, { 0x0E84, 64 }, { 0x0E8B, 64 }, { 0x0E91, 64 } },
    { { 0x0E11, 1 } },                                   /* arp on */
    { { 0x0E08, 3 } },                                   /* legacy filter type 3 */
    { { 0x0E2B, 1 }, { 0x0E2D, 5 }, { 0x0E2E, 20 }, { 0x0E34, 64 } }, /* morph mode */
    { { 0x0E58, 1 } },                                   /* polyphony mode 1 */
    { { 0x0F0A, 1 }, { 0x0F2A, 1 }, { 0x0F4A, 1 }, { 0x0F6A, 1 },     /* every op direct out */
      { 0x0F8A, 1 }, { 0x0FAA, 1 }, { 0x0FCA, 1 }, { 0x0FEA, 1 } },
    { { 0x0EC6, 1 } },                                   /* performance mode 1 */
    { { 0x0E58, 1 }, { 0x0E65, 64 }, { 0x0E66, 100 } }, /* mono + glide */
};

static void ev_apply(struct RIEngine *e, uint16_t key, int val) {
    struct RIEvent ev;
    memset(&ev, 0, sizeof ev);
    ev.type = RI_EV_AUTOMATION;
    ev.value = key;
    ev.flags = (uint16_t)(val & 127);
    ri_engine_apply_event(e, &ev);
}

static void fresh(struct RIEngine *e) {
    memset(e, 0, sizeof *e);
    ri_engine_init(e);
    ri_engine_defaults(e);
}

/* RILeviFx holds two RIReverb cores with per-instance line pointers: compare
 * around them (same trick as t116). */
static int engines_equal(const struct RIEngine *a, const struct RIEngine *b) {
    size_t r0 = offsetof(struct RIEngine, slevi) + offsetof(struct RILeviSet, fx) +
        offsetof(struct RILeviFx, rvl);
    size_t r1 = r0 + 2u * sizeof(struct RIReverb);
    if (memcmp(a, b, r0))
        return 0;
    return !memcmp((const unsigned char *)a + r1, (const unsigned char *)b + r1, sizeof *a - r1);
}

static int state_moves(uint16_t key, int val) {
    fresh(&EA);
    fresh(&EB);
    ev_apply(&EA, key, val);
    return !engines_equal(&EA, &EB);
}

static void render(int ctx, uint16_t key, int val, int have, float *l, float *r) {
    int c;
    fresh(&E);
    ri_engine_load(&E, 0, 0u, (uint64_t)NT, RI_ENGINE_SLEVI);
    for (c = 0; c < 12 && CTX[ctx][c][0]; c++)
        ev_apply(&E, CTX[ctx][c][0], CTX[ctx][c][1]);
    if (have)
        ev_apply(&E, key, val);
    levi_note_on(&E.slevi, 60u);
    levi_note_on(&E.slevi, 64u);
    ri_engine_render(&E, l, r, NON, SR);
    levi_note_off(&E.slevi, 60u);
    levi_note_off(&E.slevi, 64u);
    ri_engine_render(&E, l + NON, r + NON, NOFF, SR);
}

static int differs(void) {
    uint32_t i;
    for (i = 0; i < NT; i++)
        if (L[i] != BL[i] || R_[i] != BR[i])
            return 1;
    return 0;
}

/* 0 audible in the default context, else first audible context, -1 never. */
static int audible(uint16_t key, int val) {
    int c, last = with_ctx ? NCTX : 1;
    for (c = 0; c < last; c++) {
        render(c, 0, 0, 0, BL, BR);
        render(c, key, val, 1, L, R_);
        if (differs())
            return c;
    }
    return -1;
}

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

int main(int argc, char **argv) {
    uint32_t n, m, pg, k;
    uint32_t rows = 0, rstate = 0, rsil = 0, rdef = 0;
    uint32_t encs = 0, enokey = 0, enomove = 0, estate = 0, esil = 0, edef = 0;
    with_ctx = argc > 1 && !strcmp(argv[1], "ctx");

    /* (a) registry rows */
    for (n = 0; n < ri_ctlreg_count(); n++) {
        const struct RICtlDef *d = ri_ctlreg_at(n);
        uint16_t key;
        int v;
        if (!d || d->section != RI_SEC_LEVI)
            continue;
        key = ri_ctlreg_auto_id(d);
        if (!key)
            continue;
        rows++;
        if (state_moves(key, d->def_v)) {
            rdef++;
            printf("DEFAULT row %3u %-12s %-14s def %d key %04x\n", d->reg_id & 0xFFu, d->group,
                d->legend, d->def_v, key);
        }
        v = d->def_v == d->max_v ? d->min_v : d->max_v;
        if (!state_moves(key, v)) {
            rstate++;
            printf("STATE   row %3u %-12s %-14s key %04x=%d\n", d->reg_id & 0xFFu, d->group, d->legend,
                key, v);
        } else if (audible(key, v) < 0) {
            rsil++;
            printf("SILENT  row %3u %-12s %-14s key %04x=%d\n", d->reg_id & 0xFFu, d->group, d->legend,
                key, v);
        }
    }

    /* (b) encoder slots on every module page */
    for (m = 0; m < RI_SLEVI_NMOD; m++) {
        struct RISectLevi p;
        uint32_t np;
        open_page(&p, m, 0);
        np = ri_slevi_page_count(&p);
        for (pg = 0; pg < np; pg++) {
            for (k = 0; k < 8u; k++) {
                struct RISectLevi q;
                char title[64], nm[64];
                uint16_t key = 0;
                int kv = 0, cur, alt, j;
                open_page(&q, m, pg);
                if (!ri_slevi_enc_live(&q, k))
                    continue;
                encs++;
                snprintf(title, sizeof title, "%s", ri_slevi_page_title(&q));
                snprintf(nm, sizeof nm, "%s", ri_slevi_enc_name(&q, k));
                /* the panel's own default for this target */
                if (resolve(&q, RI_SLEVI_ENC0 + k, &key, &kv) && state_moves(key, kv)) {
                    edef++;
                    printf("DEFAULT enc %-22s slot %u %-14s key %04x=%d\n", title, k + 1, nm, key, kv);
                }
                cur = ri_slevi_value(&q, RI_SLEVI_ENC0 + k);
                alt = cur >= 64 ? 0 : 127;
                if (!ri_slevi_set_value(&q, RI_SLEVI_ENC0 + k, alt)) {
                    static const int tries[3] = { 64, 32, 96 };
                    for (j = 0; j < 3; j++)
                        if (ri_slevi_set_value(&q, RI_SLEVI_ENC0 + k, tries[j]))
                            break;
                    if (j == 3) {
                        enomove++;
                        printf("NOMOVE  enc %-22s slot %u %-14s\n", title, k + 1, nm);
                        continue;
                    }
                }
                if (!resolve(&q, RI_SLEVI_ENC0 + k, &key, &kv)) {
                    enokey++;
                    printf("NOKEY   enc %-22s slot %u %-14s\n", title, k + 1, nm);
                    continue;
                }
                if (!state_moves(key, kv)) {
                    estate++;
                    printf("STATE   enc %-22s slot %u %-14s key %04x=%d\n", title, k + 1, nm, key, kv);
                } else if (audible(key, kv) < 0) {
                    esil++;
                    printf("SILENT  enc %-22s slot %u %-14s key %04x=%d\n", title, k + 1, nm, key, kv);
                }
            }
        }
    }
    printf("rows: %u keyed, %u default-mismatch, %u state-dead, %u silent%s\n", rows, rdef, rstate, rsil,
        with_ctx ? " (all contexts)" : " (default context)");
    printf("encoders: %u live slots, %u default-mismatch, %u nomove, %u nokey, %u state-dead, %u silent%s\n",
        encs, edef, enomove, enokey, estate, esil, with_ctx ? " (all contexts)" : " (default context)");
    return 0;
}
