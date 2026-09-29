/* t112_partial_redraw — S3 dirty-rect parity (2026-09-28).
 * For every section (z0) and every scalar value control: render F0,
 * drive the control min->max, render F1, replay only the F1 commands
 * hitting ri_geo_bbox(id) onto F0, and require the whole frame to equal
 * F1. Proves each damage box covers everything its control touches.
 * (Step keys share the same box path; momentary buttons and cross-id
 * selectors repaint whole sections on-device and stay out of this test.)
 */
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "tests/helpers/ri_assert.h"
#include "gui/panelgeo.h"
#include "gui/ctlreg.h"
#include "gui/sectui.h"
#include "gui/sectmix.h"
#include "gui/draw/art.h"
#include "gui/draw/canvas.h"
#include "platform/host/raster.h"

static struct ri_dcmd B1[24576];
static char SP1[32768];
static uint32_t F0[2048u * 1024u], F1[2048u * 1024u], P1[2048u * 1024u];
static struct RIMixBoard BOARD;

static int is_scalar(uint32_t kind) {
    return kind == RI_CK_KNOB || kind == RI_CK_FADER || kind == RI_CK_SWITCH ||
        kind == RI_CK_SELECTOR || kind == RI_CK_DISPLAY;
}

static int has_damage_item(const struct RIGeoSection *g, uint32_t lo) {
    uint32_t i;
    for (i = 0u; i < g->nitems; i++) {
        uint8_t sh = g->items[i].shape;
        if ((g->items[i].reg_id & 0xFFu) == lo &&
            (sh == RI_GEO_KNOB || sh == RI_GEO_RECT || sh == RI_GEO_OPTION ||
                sh == RI_GEO_STEPPER || sh == RI_GEO_LED))
            return 1;
    }
    return 0;
}

static uint32_t render_sec(uint8_t sec, struct RISectUI *ui, uint32_t *px,
    struct ri_dlist *dl) {
    struct ri_raster r;
    struct ri_text_metrics tm;
    const struct RIGeoSection *g;
    uint32_t w, h;
    g = ri_geo_section(sec == RI_SEC_SYNTH2 ? RI_SEC_SYNTH1 : sec);
    if (!g)
        return 0u;
    w = (uint32_t)ri_geo_px((int)g->w, 0);
    h = (uint32_t)ri_geo_px((int)g->h, 0);
    if (w == 0u || h == 0u || (uint64_t)w * h > 2048u * 1024u)
        return 0u;
    tm.width = ri_raster_text_width;
    tm.height = 7;
    tm.baseline = 5;
    tm.ctx = 0;
    ri_draw_section(dl, ui, sec, 0, 0, 0, &tm, 0, 0);
    if (dl->n == 0u || dl->n >= dl->cap)
        return 0u;
    ri_raster_init(&r, px, w, h);
    ri_raster_clear(&r, 0x000000u);
    ri_raster_replay(&r, dl, 0);
    return w * 65536u + h;
}

int main(void) {
    struct RISectUI ui;
    struct ri_dlist dl1;
    uint32_t sec, ntested = 0u, nwide = 0u, testedsec[RI_SEC_COUNT] = { 0 };
    ri_smix_init(&BOARD);
    for (sec = 0u; sec < RI_SEC_COUNT; sec++) {
        const struct RIGeoSection *g =
            ri_geo_section(sec == RI_SEC_SYNTH2 ? RI_SEC_SYNTH1 : sec);
        uint32_t nctl, k;
        if (!g)
            continue;
        if (ri_sui_init(&ui, (uint8_t)sec) != 0)
            continue;
        if (ri_smix_strip(sec) >= 0 || sec == RI_SEC_MASTER)
            ri_sui_bind_board(&ui, &BOARD);
        nctl = ri_ctlreg_count();
        for (k = 0u; k < nctl; k++) {
            const struct RICtlDef *d = ri_ctlreg_at(k);
            uint32_t idx, wh;
            int bx0, by0, bx1, by1;
            if (!d || d->section != sec || !is_scalar(d->kind))
                continue;
            idx = d->reg_id & 0xFFu;
            if (!has_damage_item(g, idx))
                continue;
            if (d->min_v == d->max_v)
                continue;
            if (ri_geo_wide(d->reg_id)) {
                /* Wide controls repaint whole sections on-device (bank
                 * swaps content, Algo swaps the voice UI, Loop Start drags
                 * Len); parity covers the damage path only. */
                nwide++;
                continue;
            }
            /* F0 at min, F1 at max; the readback gate skips controls
             * whose value never lands (unsupported here, not silent). */
            ri_sui_set(&ui, idx, d->min_v);
            ri_dlist_init(&dl1, B1, 24576u, SP1, sizeof SP1);
            if (!render_sec((uint8_t)sec, &ui, F0, &dl1))
                continue;
            ri_sui_set(&ui, idx, d->max_v);
            if (ri_sui_value(&ui, idx) != d->max_v)
                continue;
            ri_dlist_init(&dl1, B1, 24576u, SP1, sizeof SP1);
            wh = render_sec((uint8_t)sec, &ui, F1, &dl1);
            if (!wh)
                continue;
            RI_ASSERT(ri_geo_bbox(g, d->reg_id, 0, &bx0, &by0, &bx1, &by1) == 0,
                "bbox sec=%u idx=%u", sec, idx);
            /* P1 = F0 + the F1 list replayed through the damage box. */
            memcpy(P1, F0, sizeof F0);
            {
                struct ri_raster r;
                const struct RIGeoSection *gg = g;
                uint32_t w = (uint32_t)ri_geo_px((int)gg->w, 0);
                uint32_t h = (uint32_t)ri_geo_px((int)gg->h, 0);
                ri_raster_init(&r, P1, w, h);
                ri_raster_replay_box(&r, &dl1, 0, bx0, by0, bx1, by1);
                RI_ASSERT(memcmp(F1, P1, (size_t)w * h * 4u) == 0,
                    "partial sec=%u idx=%u box=%d,%d..%d,%d", sec, idx,
                    bx0, by0, bx1, by1);
            }
            ntested++;
            testedsec[sec]++;
        }
    }
    RI_ASSERT(ntested >= 100u, "tested %u controls", ntested);
    /* 35 today: selectors + Loop Start, plus the Levi page UI (fidelity P1,
     * 2026-09-30): 8 master-control encoders, the algorithm encoder and
     * the page-linked panel knobs repaint the LCD page and encoder rings.
     * Trips on silent widening. */
    RI_ASSERT(nwide <= 38u && nwide >= 7u, "wide controls %u", nwide);
    for (sec = 0u; sec < RI_SEC_COUNT; sec++) {
        const struct RIGeoSection *g =
            ri_geo_section(sec == RI_SEC_SYNTH2 ? RI_SEC_SYNTH1 : sec);
        uint32_t k, has = 0u;
        uint32_t nctl;
        if (!g)
            continue;
        nctl = ri_ctlreg_count();
        for (k = 0u; k < nctl; k++) {
            const struct RICtlDef *d = ri_ctlreg_at(k);
            if (d && d->section == sec && is_scalar(d->kind) &&
                has_damage_item(g, d->reg_id & 0xFFu) && d->min_v != d->max_v &&
                !ri_geo_wide(d->reg_id))
                has = 1u;
        }
        if (has)
            RI_ASSERT(testedsec[sec] > 0u, "sec %u untested", sec);
    }
    RI_RESULT("partial_redraw");
}
