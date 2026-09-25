/* gui/sect808.c — 808 section front-panel behaviour (§12.10 G3/G4). */
#include "gui/sect808.h"
#include "gui/ctlreg.h"

static const struct RICtlDef *def(uint32_t idx) {
    return ri_ctlreg_find((uint16_t)((RI_SEC_808 << 8) | idx));
}

int ri_s808_init(struct RISect808 *s) {
    uint32_t i;
    if (!s)
        return 2;
    s->section = RI_SEC_808;
    s->pad[0] = s->pad[1] = s->pad[2] = 0;
    for (i = 0; i < RI_S808_NCTL; i++) {
        const struct RICtlDef *d = def(i);
        s->val[i] = d ? d->def_v : 0;
    }
    ri_pattern_init(&s->pat, RI_PATTERN_KIND_DRUM, RI_DRUM_CLASS_808);
    return 0;
}

int ri_s808_lane(const struct RISect808 *s) {
    int sel = s ? s->val[RI_S808_SELECT] : 0;
    return sel <= 0 ? -1 : sel - 1;
}

int ri_s808_press(struct RISect808 *s, uint32_t idx) {
    const struct RICtlDef *d;
    if (!s || idx >= RI_S808_NCTL)
        return 0;
    d = def(idx);
    if (!d)
        return 0;
    if (d->kind == RI_CK_STEP) {
        uint32_t step = idx - RI_S808_STEP0;
        int lane = ri_s808_lane(s);
        if (lane < 0)
            return ri_pdrum_set_ac(&s->pat, step, !(s->pat.row.drum[step].flags & RI_DRUM_AC)) == 0;
        ri_pdrum_click(&s->pat, step, (uint32_t)lane, 0); /* 808: off <-> on (p. 28) */
        return 1;
    }
    if (d->kind == RI_CK_SWITCH) {
        s->val[idx] = (int16_t)(s->val[idx] ? 0 : 1);
        return 1;
    }
    return 0;
}

int ri_s808_set_value(struct RISect808 *s, uint32_t idx, int v) {
    const struct RICtlDef *d = s && idx < RI_S808_NCTL ? def(idx) : 0;
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

int ri_s808_reset(struct RISect808 *s, uint32_t idx) {
    const struct RICtlDef *d = s && idx < RI_S808_NCTL ? def(idx) : 0;
    return d ? ri_s808_set_value(s, idx, d->def_v) : 0;
}

int ri_s808_led(const struct RISect808 *s, uint32_t idx) {
    const struct RICtlDef *d = s && idx < RI_S808_NCTL ? def(idx) : 0;
    if (!d)
        return 0;
    if (d->kind == RI_CK_STEP) {
        uint32_t step = idx - RI_S808_STEP0;
        int lane = ri_s808_lane(s);
        if (lane < 0)
            return (s->pat.row.drum[step].flags & RI_DRUM_AC) != 0;
        return ri_pdrum_get(&s->pat, step, (uint32_t)lane) != RI_HIT_OFF;
    }
    if (d->kind == RI_CK_SWITCH)
        return s->val[idx] != 0;
    return 0;
}
