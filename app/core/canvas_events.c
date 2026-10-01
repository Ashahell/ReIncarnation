/* canvas_events.c — backend-free canvas event bodies (portability plan T3b).
 * Mirrors gui/widgets/rsection.mcc.c MUIM_HandleEvent branch for branch.
 */
#include "app/core/canvas_events.h"

#include "gui/keymap.h"
#include "gui/knob_logic.h"

static long cev_to_n(const struct RICtlDef *d, int v) {
    int span = d->max_v - d->min_v;
    return span > 0 ? (long)(((long)(v - d->min_v) * 127 + span / 2) / span) : 0;
}

static int cev_from_n(const struct RICtlDef *d, long n) {
    int span = d->max_v - d->min_v;
    return d->min_v + (int)(((long)n * span + 63) / 127);
}

void ri_cev_init(struct RICevState *st) {
    if (!st)
        return;
    st->drag_id = RI_CEV_NODRAG;
    st->acc_dx = st->acc_dy = 0.0;
    st->last_x = st->last_y = 0;
    st->drag_n0 = 0;
    st->rep_idx = RI_CEV_NODRAG;
    st->rep_dir = 0;
    st->rep_ticks = 0u;
}

uint32_t ri_cev_tick(struct RICevState *st, struct RISectUI *ui) {
    if (!st || !ui)
        return 0u;
    if (st->rep_idx != RI_CEV_NODRAG && ++st->rep_ticks >= 4 &&
        ri_sui_step(ui, st->rep_idx, st->rep_dir))
        return RI_CEV_CHANGED;
    return 0u;
}

uint32_t ri_cev_key(struct RICevState *st, struct RISectUI *ui,
    struct RIPanelUI *panel, int key_owner, uint32_t code, uint32_t qual) {
    struct RIKeyAction a;
    (void)st;
    if (!ui || !panel)
        return 0u;
    if (!key_owner)
        return 0u;
    a = ri_key_decode(code, qual, &panel->opts, panel->focus);
    if (a.kind == RI_KA_NONE)
        return 0u;
    if (ri_panel_key(panel, code, qual))
        return RI_CEV_CHANGED | RI_CEV_EAT;
    return RI_CEV_EAT;
}

uint32_t ri_cev_button(struct RICevState *st, struct RISectUI *ui,
    struct RIPanelUI *panel, const struct RIGeoSection *geo, int zoom,
    int lx, int ly, int w, int h, int kind, uint32_t ms) {
    uint16_t id;
    uint32_t idx;
    int opt = -1;
    const struct RICtlDef *cd;
    uint32_t out = 0u;
    if (!st || !ui || !geo)
        return 0u;
    id = ri_geo_hit_opt(geo, lx, ly, zoom, &opt);
    idx = id & 0xFFu;
    if (kind == 0 && panel && lx >= 0 && ly >= 0 && lx < w && ly < h &&
        ri_panel_click(panel, ui->section))
        out |= RI_CEV_CHANGED;
    if (kind == 1) {
        st->rep_idx = RI_CEV_NODRAG;
        if (st->drag_id == RI_CEV_NODRAG)
            return out;
        st->drag_id = RI_CEV_NODRAG;
        return out | RI_CEV_EAT;
    }
    if (id == RI_CEV_NODRAG)
        return out;
    cd = ri_ctlreg_find((uint16_t)((ui->section << 8) | idx));
    if (!cd)
        return out;
    if (kind == 2) {
        if (ri_sui_reset(ui, idx))
            out |= RI_CEV_CHANGED;
        return out | RI_CEV_EAT;
    }
    if (kind != 0)
        return out;
    if (opt == RI_GEO_HIT_UP || opt == RI_GEO_HIT_DOWN) {
        st->rep_idx = (uint16_t)idx;
        st->rep_dir = (int8_t)(opt == RI_GEO_HIT_UP ? 1 : -1);
        st->rep_ticks = 0u;
        if (ri_sui_step(ui, idx, st->rep_dir))
            out |= RI_CEV_CHANGED;
    } else if (opt >= 0) {
        if (ri_sui_set(ui, idx, opt))
            out |= RI_CEV_CHANGED;
    } else if (cd->kind == RI_CK_KNOB || cd->kind == RI_CK_SELECTOR || cd->kind == RI_CK_FADER) {
        st->drag_id = id;
        st->drag_n0 = cev_to_n(cd, ri_sui_value(ui, idx));
        st->acc_dx = st->acc_dy = 0.0;
        st->last_x = lx;
        st->last_y = ly;
    } else if (ri_sui_tap(ui, idx, ms) || ri_sui_press(ui, idx)) {
        /* A tap that moved the tempo needs no ordinary press: ri_str_press
         * has no case for the key, and a tap without a clock falls through
         * to it and does nothing. */
        out |= RI_CEV_CHANGED;
    }
    return out | RI_CEV_EAT;
}

uint32_t ri_cev_move(struct RICevState *st, struct RISectUI *ui,
    int x, int y, int shift) {
    uint32_t idx;
    const struct RICtlDef *cd;
    double n;
    if (!st || !ui || st->drag_id == RI_CEV_NODRAG)
        return 0u;
    idx = st->drag_id & 0xFFu;
    cd = ri_ctlreg_find((uint16_t)((ui->section << 8) | idx));
    st->acc_dx += (double)(x - st->last_x);
    st->acc_dy += (double)(st->last_y - y);
    st->last_x = x;
    st->last_y = y;
    ri_knob_clamp_acc((double)st->drag_n0, &st->acc_dx, &st->acc_dy, shift);
    n = ri_knob_drag_to_value((double)st->drag_n0, st->acc_dx, st->acc_dy, shift);
    if (cd && ri_sui_set(ui, idx, cev_from_n(cd, (long)ri_ctl_quantize(n))))
        return RI_CEV_CHANGED | RI_CEV_EAT;
    return RI_CEV_EAT;
}
