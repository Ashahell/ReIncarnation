/* canvas_events.c — backend-free canvas event bodies (portability plan T3b).
 * Mirrors gui/widgets/rsection.mcc.c MUIM_HandleEvent branch for branch.
 */
#include "app/core/canvas_events.h"

#include "gui/keymap.h"
#include "gui/knob_logic.h"
#include "gui/sectlevi.h"

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
    st->key_id = RI_CEV_NODRAG;
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
    /* THE RELEASE IS NOT AN ACTION, BUT IT IS STILL AN EVENT.
     *
     * ri_panel_key() owns the press-count latches -- a held space or Stop is
     * one press, and that key's own release re-arms it (t241, t242) -- so it
     * must see key-ups. Decoding first and returning early on RI_KA_NONE,
     * which is exactly what a key-up decodes to (gui/keymap.c), skipped it:
     * on the real app the latch never cleared and each press-count key
     * worked ONCE per launch. The host tests called ri_panel_key() one level
     * below this door and could not see it; the lane could -- a second space
     * press produced no TR line at all
     * (docs/evidence/midi/transport-storm/ev-ptr2.log). */
    if (ri_panel_key(panel, code, qual))
        return RI_CEV_CHANGED | RI_CEV_EAT;
    a = ri_key_decode(code, qual, &panel->opts, panel->focus);
    if (a.kind == RI_KA_NONE)
        return 0u;
    return RI_CEV_EAT;
}

/* Keybed keys latch until release so the shell can send note-on now and
 * note-off on SELECTUP. Every button that is a keybed key does this -- the
 * bed is 37 keys at 24..36 and 228..251, so the test is the key MAP, not an
 * id range; every other button is momentary, and latching one would strand
 * its indicator lit. */
static void levi_key_latch(struct RICevState *st, uint32_t section, uint16_t id) {
    uint32_t idx;
    if (!st)
        return;
    idx = id & 0xFFu;
    if (section == RI_SEC_LEVI && ri_levi_key_n((uint16_t)idx) >= 0)
        st->key_id = id;
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
        /* SELECTUP ends whatever SELECTDOWN started: an arrow repeat, a
         * drag, and a held key. The key case is the reason this branch
         * clears state it did not set -- without it a note-off has nowhere
         * to go and the voice hangs (owner 2026-10-13). */
        int held = (st->key_id != RI_CEV_NODRAG) || (st->drag_id != RI_CEV_NODRAG);
        st->rep_idx = RI_CEV_NODRAG;
        st->key_id = RI_CEV_NODRAG;
        st->drag_id = RI_CEV_NODRAG;
        return held ? (out | RI_CEV_EAT) : out;
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
    } else if (ri_sui_tap(ui, idx, ms) || (levi_key_latch(st, ui->section, id), ri_sui_press(ui, idx))) {
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
