/* gui/sect909.c — 909 section front-panel behaviour (§12.10 G3/G4). */
#include "gui/sect909.h"
#include "gui/ctlreg.h"

static const struct RICtlDef *def(uint32_t idx) {
    return ri_ctlreg_find((uint16_t)((RI_SEC_909 << 8) | idx));
}

int ri_s909_init(struct RISect909 *s) {
    uint32_t i;
    if (!s)
        return 2;
    s->section = RI_SEC_909;
    s->pad[0] = s->pad[1] = s->pad[2] = 0;
    for (i = 0; i < RI_S909_NCTL; i++) {
        const struct RICtlDef *d = def(i);
        s->val[i] = d ? d->def_v : 0;
    }
    ri_pattern_init(&s->pat, RI_PATTERN_KIND_DRUM, RI_DRUM_CLASS_909);
    return 0;
}

int ri_s909_lane(const struct RISect909 *s) {
    int sel = s ? s->val[RI_S909_SELECT] : 0;
    return sel <= 0 ? -1 : sel - 1;
}

int ri_s909_press(struct RISect909 *s, uint32_t idx) {
    const struct RICtlDef *d = s && idx < RI_S909_NCTL ? def(idx) : 0;
    if (!d)
        return 0;
    if (d->kind == RI_CK_STEP) {
        uint32_t step = idx - RI_S909_STEP0;
        int lane = ri_s909_lane(s);
        if (lane < 0)
            return ri_pdrum_set_ac(&s->pat, step, !(s->pat.row.drum[step].flags & RI_DRUM_AC)) == 0;
        ri_pdrum_click(&s->pat, step, (uint32_t)lane, s->val[RI_S909_FLAMBTN] != 0);
        return 1;
    }
    if (d->kind == RI_CK_SWITCH) {
        s->val[idx] = (int16_t)(s->val[idx] ? 0 : 1);
        return 1;
    }
    return 0;
}

int ri_s909_set_value(struct RISect909 *s, uint32_t idx, int v) {
    const struct RICtlDef *d = s && idx < RI_S909_NCTL ? def(idx) : 0;
    if (!d || (d->kind != RI_CK_KNOB && d->kind != RI_CK_SWITCH && d->kind != RI_CK_SELECTOR))
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

int ri_s909_reset(struct RISect909 *s, uint32_t idx) {
    const struct RICtlDef *d = s && idx < RI_S909_NCTL ? def(idx) : 0;
    return d ? ri_s909_set_value(s, idx, d->def_v) : 0;
}

int ri_s909_led(const struct RISect909 *s, uint32_t idx) {
    const struct RICtlDef *d = s && idx < RI_S909_NCTL ? def(idx) : 0;
    if (!d)
        return 0;
    if (d->kind == RI_CK_STEP) {
        uint32_t step = idx - RI_S909_STEP0;
        int lane = ri_s909_lane(s);
        if (lane < 0)
            return (s->pat.row.drum[step].flags & RI_DRUM_AC) != 0;
        return (int)ri_pdrum_get(&s->pat, step, (uint32_t)lane);
    }
    if (d->kind == RI_CK_SWITCH)
        return s->val[idx] != 0;
    return 0;
}
