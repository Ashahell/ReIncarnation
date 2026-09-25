/* gui/sectfx.c — PCF / Delay / Dist / Comp front-panel behaviour (§12.10 G3/G4). */
#include "gui/sectfx.h"
#include "gui/ctlreg.h"

static int isfx(uint32_t section) {
    return section >= RI_SEC_PCF && section <= RI_SEC_COMP;
}

static const struct RICtlDef *def(const struct RISectFx *s, uint32_t idx) {
    return s && isfx(s->section) && idx < RI_SFX_NCTL ? ri_ctlreg_find((uint16_t)((s->section << 8) | idx)) : 0;
}

int ri_sfx_init(struct RISectFx *s, uint8_t section) {
    uint32_t i;
    if (!s || !isfx(section))
        return 2;
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
    return d && d->kind == RI_CK_SWITCH && s->val[idx] != 0;
}

void ri_sfx_meter_set(struct RISectFx *s, uint32_t idx, int level) {
    const struct RICtlDef *d = def(s, idx);
    if (d && d->kind == RI_CK_METER)
        s->val[idx] = (int16_t)(level < 0 ? 0 : level > 127 ? 127 : level);
}
