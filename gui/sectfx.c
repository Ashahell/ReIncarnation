/* gui/sectfx.c — PCF / Delay / Dist / Comp front-panel behaviour (§12.10 G3/G4). */
#include "gui/sectfx.h"
#include "gui/ctlreg.h"

static int isfx(uint32_t section) {
    return section >= RI_SEC_PCF && section <= RI_SEC_COMP;
}

static const struct RICtlDef *def(const struct RISectFx *s, uint32_t idx) {
    return s && isfx(s->section) && idx < RI_SFX_NCTL ? ri_ctlreg_find((uint16_t)((s->section << 8) | idx)) : 0;
}

/* Canvas order is PCF, DELAY, DIST, COMP (RI_SEC_PCF..RI_SEC_COMP). */
static uint32_t fx_slot(const struct RISectFx *s) {
    return (uint32_t)(s->section - RI_SEC_PCF);
}

/* DERIVED, not stored. A displaced canvas's val[0] is unreachable from
 * the canvas that displaced it (four separate RISectFx -- panelui.h fx[4]
 * -- and none holds a pointer to a sibling), so storing the selection
 * per canvas would leave the old lamp lit: a switch that looks live and
 * is not, the exact defect class this ruling exists to remove. */
static int fx_on(const struct RISectFx *s) {
    return s && s->rack && s->rack->live == fx_slot(s) ? 1 : 0;
}

int ri_sfx_init(struct RISectFx *s, uint8_t section) {
    uint32_t i;
    if (!s || !isfx(section))
        return 2;
    s->rack = 0; /* shared rack is bound separately (ri_sui_bind_fx_rack) */
    s->section = section;
    s->pad[0] = s->pad[1] = s->pad[2] = 0;
    for (i = 0; i < RI_SFX_NCTL; i++) {
        const struct RICtlDef *d = def(s, i);
        s->val[i] = d ? d->def_v : 0;
    }
    return 0;
}

int ri_sfx_press(struct RISectFx *s, uint32_t idx) {
    const struct RICtlDef *d = def(s, idx);
    if (!d || d->kind != RI_CK_SWITCH)
        return 0;
    if (idx == RI_SFX_ONOFF) {
        /* Claim the rack, displacing whoever held it. OFF is deliberately
         * NOT symmetric: it clears only itself and leaves the rack empty,
         * so switching a module off never silently brings the displaced one
         * back. With no rack bound this reports "handled" and changes
         * nothing -- fail closed, so an unlaid-out canvas cannot steal the
         * selection (t222). */
        if (!s->rack)
            return 1;
        if (fx_on(s))
            s->rack->live = RI_SFX_LIVE_NONE;
        else
            s->rack->live = (uint8_t)fx_slot(s);
        return 1;
    }
    s->val[idx] = (int16_t)(s->val[idx] ? 0 : 1);
    return 1;
}

int ri_sfx_set_value(struct RISectFx *s, uint32_t idx, int v) {
    const struct RICtlDef *d = def(s, idx);
    if (!d || (d->kind != RI_CK_KNOB && d->kind != RI_CK_FADER && d->kind != RI_CK_SELECTOR))
        return 0;
    if (v < d->min_v)
        v = d->min_v;
    if (v > d->max_v)
        v = d->max_v;
    if (s->val[idx] == v)
        return 0;
    s->val[idx] = (int16_t)v;
    return 1;
}

int ri_sfx_reset(struct RISectFx *s, uint32_t idx) {
    const struct RICtlDef *d = def(s, idx);
    return d ? ri_sfx_set_value(s, idx, d->def_v) : 0;
}

int ri_sfx_step(struct RISectFx *s, uint32_t idx, int dir) {
    const struct RICtlDef *d = def(s, idx);
    if (!d || d->kind != RI_CK_SELECTOR || !dir)
        return 0;
    return ri_sfx_set_value(s, idx, s->val[idx] + (dir > 0 ? 1 : -1));
}

int ri_sfx_led(const struct RISectFx *s, uint32_t idx) {
    const struct RICtlDef *d = def(s, idx);
    if (!d || d->kind != RI_CK_SWITCH)
        return 0;
    if (idx == RI_SFX_ONOFF)
        return fx_on(s);
    return s->val[idx] != 0;
}

void ri_sfx_meter_set(struct RISectFx *s, uint32_t idx, int level) {
    const struct RICtlDef *d = def(s, idx);
    if (d && d->kind == RI_CK_METER)
        s->val[idx] = (int16_t)(level < 0 ? 0 : level > 127 ? 127 : level);
}

/* --- shared rack: owner ruling 2026-10-14, "as intuitive as possible ...
 * If the user turns on an FX panel that conflicts with another, that other
 * one should be turned off and the newly activated FX panel should become
 * active." --- */

int ri_sfx_rack_init(struct RIFxRack *r) {
    if (!r)
        return 2;
    r->live = RI_SFX_LIVE_NONE;
    return 0;
}

int ri_sfx_bind_rack(struct RISectFx *s, struct RIFxRack *r) {
    if (!s || !r)
        return 2;
    s->rack = r;
    return 0;
}

/* DERIVED, not stored. A displaced canvas's val[0] is unreachable from the
 * canvas that displaced it (they are four separate RISectFx -- panelui.h
 * fx[4] -- and none holds a pointer to a sibling), so storing the selection
 * per canvas would leave the old lamp lit: a switch that looks live and is
 * not, which is the exact defect class this ruling exists to remove. */
int ri_sfx_value(const struct RISectFx *s, uint32_t idx) {
    if (!s)
        return 0;
    if (idx == RI_SFX_ONOFF)
        return fx_on(s);
    return idx < RI_SFX_NCTL ? s->val[idx] : 0;
}
