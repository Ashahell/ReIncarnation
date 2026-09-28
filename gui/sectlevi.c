/* sectlevi.c — Levi section behaviour bodies (owner 2026-09-28). */
#include "gui/sectlevi.h"
#include "gui/ctlreg.h"
#include "engine/dsp/levi.h"

static const struct RICtlDef *def(const struct RISectLevi *s, uint32_t idx) {
    (void)s;
    return ri_ctlreg_find((uint16_t)((RI_SEC_LEVI << 8) | idx));
}

int ri_slevi_init(struct RISectLevi *s) {
    uint32_t i;
    if (!s)
        return 2;
    s->section = RI_SEC_LEVI;
    s->sel = 0u;
    s->edit_step = 0u;
    s->opsel = 0u;
    for (i = 0u; i < RI_LEVI_NOPS; i++)
        s->opmode[i] = RI_LEVI_FM;
    for (i = 0u; i < RI_SLEVI_NCTL; i++) {
        const struct RICtlDef *d = def(s, i);
        s->val[i] = d ? d->def_v : 0;
    }
    s->val[RI_SLEVI_OPMODE] = 0; /* packed op*16+mode, op 0 FM */
    ri_pattern_init(&s->pat, RI_PATTERN_KIND_LEVI, 0u);
    return 0;
}

int ri_slevi_press(struct RISectLevi *s, uint32_t idx) {
    uint32_t e;
    if (!s || idx >= RI_SLEVI_NCTL)
        return 0;
    if (idx == RI_SLEVI_STEP) {
        s->edit_step = (uint8_t)((s->edit_step + 1u) % RI_PATTERN_STEPS);
        return 1;
    }
    if (idx == RI_SLEVI_MODE) {
        s->val[RI_SLEVI_MODE] = (int16_t)(s->val[RI_SLEVI_MODE] ? 0 : 1);
        return 1;
    }
    if (idx == RI_SLEVI_BACK) {
        s->edit_step = (uint8_t)((s->edit_step + RI_PATTERN_STEPS - 1u) % RI_PATTERN_STEPS);
        return 1;
    }
    if (idx >= RI_SLEVI_STEP0 && idx < RI_SLEVI_STEP0 + RI_PATTERN_STEPS) {
        uint32_t st = idx - RI_SLEVI_STEP0;
        if (ri_levi_on(&s->pat, st, s->sel))
            return ri_levi_set(&s->pat, st, s->sel,
                (uint8_t)ri_levi_get(&s->pat, st, s->sel), 0) == 0;
        return ri_levi_set(&s->pat, st, s->sel, RI_SLEVI_MIDDLE_C, 1) == 0;
    }
    if (idx >= RI_SLEVI_KEY0 && idx < RI_SLEVI_KEY0 + RI_SLEVI_KEYS) {
        e = s->edit_step % RI_PATTERN_STEPS;
        return ri_levi_set(&s->pat, e, s->sel,
            (uint8_t)(RI_SLEVI_MIDDLE_C + (idx - RI_SLEVI_KEY0)), 1) == 0;
    }
    return 0;
}

int ri_slevi_set_value(struct RISectLevi *s, uint32_t idx, int v) {
    const struct RICtlDef *d = s && idx < RI_SLEVI_NCTL ? def(s, idx) : 0;
    if (!d)
        return 0;
    if (idx == RI_SLEVI_SELECT) {
        uint8_t sel = v < 0 ? 0u : v > 5 ? 5u : (uint8_t)v;
        if (s->sel == sel)
            return 0;
        s->sel = sel;
        return 1;
    }
    if (idx == RI_SLEVI_ALGO || idx == RI_SLEVI_ALGOB || idx == RI_SLEVI_FTYPE) {
        /* Plain selectors (selector idiom, like Lane). FTYPE clamps 0..3. */
        int hi = idx == RI_SLEVI_FTYPE ? 3 : 7;
        int w = v < 0 ? 0 : v > hi ? hi : v;
        if (s->val[idx] == w)
            return 0;
        s->val[idx] = (int16_t)w;
        return 1;
    }
    if (idx == RI_SLEVI_OPSEL) {
        uint8_t o = v < 0 ? 0u : v > 7 ? 7u : (uint8_t)v;
        int packed = (int)o * 16 + s->opmode[o];
        if (s->opsel == o && s->val[RI_SLEVI_OPMODE] == packed)
            return 0;
        s->opsel = o;
        s->val[RI_SLEVI_OPMODE] = (int16_t)packed;
        return 1;
    }
    if (idx == RI_SLEVI_OPMODE) {
        /* Shared knob: panel truth per op, packed op*16+mode on the wire. */
        int m = v < 0 ? 0 : v > 6 ? 6 : v;
        int packed = (int)s->opsel * 16 + m;
        if (s->val[idx] == packed && s->opmode[s->opsel] == (uint8_t)m)
            return 0;
        s->opmode[s->opsel] = (uint8_t)m;
        s->val[idx] = (int16_t)packed;
        return 1;
    }
    if (d->kind != RI_CK_KNOB && d->kind != RI_CK_SWITCH)
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

int ri_slevi_reset(struct RISectLevi *s, uint32_t idx) {
    const struct RICtlDef *d = s && idx < RI_SLEVI_NCTL ? def(s, idx) : 0;
    return d ? ri_slevi_set_value(s, idx, d->def_v) : 0;
}

int ri_slevi_led(const struct RISectLevi *s, uint32_t idx) {
    uint32_t e;
    if (!s || idx >= RI_SLEVI_NCTL)
        return 0;
    e = s->edit_step % RI_PATTERN_STEPS;
    if (idx >= RI_SLEVI_STEP0 && idx < RI_SLEVI_STEP0 + RI_PATTERN_STEPS)
        return ri_levi_on(&s->pat, idx - RI_SLEVI_STEP0, s->sel);
    if (idx >= RI_SLEVI_KEY0 && idx < RI_SLEVI_KEY0 + RI_SLEVI_KEYS)
        return ri_levi_get(&s->pat, e, s->sel) ==
            (uint32_t)(RI_SLEVI_MIDDLE_C + (idx - RI_SLEVI_KEY0));
    return 0;
}

int ri_slevi_display(const struct RISectLevi *s) {
    if (!s)
        return 0;
    return (int)(s->edit_step % RI_PATTERN_STEPS) + 1;
}
