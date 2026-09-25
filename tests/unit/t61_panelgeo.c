/* t61_panelgeo — section geometry (§12.10 G2: 303 + 808).
 * Generic invariants for every laid-out section: each registry control of
 * the section has exactly one value item (knob or rect) and nothing else
 * does; value items sit inside the section and never overlap each other or
 * an option; options only belong to SELECTOR controls and cover every
 * selector value; decorations belong to real controls; zoom + hit-test
 * round-trip (options report their value). Section-specific checks follow
 * the manual figures (p. 153 synth, p. 148 808).
 */
#include <stdio.h>
#include <string.h>
#include "tests/helpers/ri_assert.h"
#include "gui/panelgeo.h"
#include "gui/ctlreg.h"

static int is_value(const struct RIGeoItem *it) {
    return it->shape == RI_GEO_KNOB || it->shape == RI_GEO_RECT;
}
static int is_hit(const struct RIGeoItem *it) {
    return is_value(it) || it->shape == RI_GEO_OPTION || it->shape == RI_GEO_STEPPER;
}

static int has_steppers(const struct RIGeoSection *s, uint16_t id) {
    uint32_t i, up = 0, dn = 0;
    for (i = 0; i < s->nitems; i++)
        if (s->items[i].reg_id == id && s->items[i].shape == RI_GEO_STEPPER) {
            up += s->items[i].opt == 1;
            dn += s->items[i].opt == 0;
        }
    return up == 1 && dn == 1;
}

static void box(const struct RIGeoItem *it, int *x0, int *y0, int *x1, int *y1) {
    int w = it->shape == RI_GEO_KNOB ? it->h : it->w;
    int h = it->h;
    *x0 = it->cx - w / 2; *x1 = it->cx + w / 2;
    *y0 = it->cy - h / 2; *y1 = it->cy + h / 2;
}

static const struct RIGeoItem *value_item(const struct RIGeoSection *s, uint16_t id) {
    uint32_t i;
    for (i = 0; i < s->nitems; i++)
        if (s->items[i].reg_id == id && is_value(&s->items[i]))
            return &s->items[i];
    return 0;
}

static void check_section(uint32_t sec) {
    const struct RIGeoSection *s = ri_geo_section(sec);
    uint32_t i, j, z, nval = 0, nsec;
    RI_ASSERT(s != 0, "%s geometry missing", ri_ctlreg_section_name(sec));
    if (!s)
        return;
    nsec = ri_ctlreg_section_count(sec, 0);
    for (i = 0; i < s->nitems; i++) {
        const struct RIGeoItem *it = &s->items[i];
        const struct RICtlDef *d = ri_ctlreg_find(it->reg_id);
        RI_ASSERT(d && d->section == sec, "%s item %u owns unknown control %04x",
            ri_ctlreg_section_name(sec), i, it->reg_id);
        if (!d)
            continue;
        if (it->shape == RI_GEO_STEPPER)
            RI_ASSERT(d->kind == RI_CK_SELECTOR && it->opt <= 1, "stepper on non-selector %s", d->legend);
        if (it->shape == RI_GEO_OPTION) {
            RI_ASSERT(d->kind == RI_CK_SELECTOR, "option on non-selector %s", d->legend);
            RI_ASSERT(it->opt >= d->min_v && it->opt <= d->max_v, "option %u out of range", it->opt);
        }
        if (!is_value(it))
            continue;
        nval++;
        for (j = i + 1; j < s->nitems; j++)
            if (is_value(&s->items[j]))
                RI_ASSERT(s->items[j].reg_id != it->reg_id, "control %04x has two value items", it->reg_id);
        /* a selector is a rotary knob, or a value display with up/down arrows (p. 18) */
        RI_ASSERT((it->shape == RI_GEO_KNOB) == (d->kind == RI_CK_KNOB ||
            (d->kind == RI_CK_SELECTOR && !has_steppers(s, d->reg_id))),
            "%s/%s: knob shape iff knob/selector kind", d->group, d->legend);
    }
    /* a SELECTOR may have no value item when options reach every value
     * (the 909 selects instruments by legend only, p. 151) */
    for (i = 0; i < nsec; i++) {
        const struct RICtlDef *d = ri_ctlreg_find((uint16_t)((sec << 8) | i));
        if (d && d->kind == RI_CK_SELECTOR && !value_item(s, d->reg_id))
            nsec--;
    }
    RI_ASSERT(nval == nsec, "%s value items %u, registry controls %u", ri_ctlreg_section_name(sec), nval, nsec);
    /* every selector value reachable by an option */
    for (i = 0; i < s->nitems; i++) {
        const struct RICtlDef *d = ri_ctlreg_find(s->items[i].reg_id);
        int v;
        if (!d || d->kind != RI_CK_SELECTOR || (!is_value(&s->items[i]) && s->items[i].shape != RI_GEO_OPTION) ||
            has_steppers(s, d->reg_id))
            continue;
        for (v = d->min_v; v <= d->max_v; v++) {
            int found = 0;
            for (j = 0; j < s->nitems; j++)
                if (s->items[j].shape == RI_GEO_OPTION && s->items[j].reg_id == d->reg_id && s->items[j].opt == v)
                    found = 1;
            RI_ASSERT(found, "%s value %d has no option", d->legend, v);
        }
    }
    /* bounds + no overlap among hit items */
    for (i = 0; i < s->nitems; i++) {
        int a0, b0, a1, b1;
        if (!is_hit(&s->items[i]))
            continue;
        box(&s->items[i], &a0, &b0, &a1, &b1);
        RI_ASSERT(a0 >= 0 && b0 >= 0 && a1 <= s->w && b1 <= s->h, "item %04x/%u outside section",
            s->items[i].reg_id, s->items[i].opt);
        for (j = i + 1; j < s->nitems; j++) {
            int c0, d0, c1, d1;
            if (!is_hit(&s->items[j]))
                continue;
            box(&s->items[j], &c0, &d0, &c1, &d1);
            RI_ASSERT(a1 <= c0 || c1 <= a0 || b1 <= d0 || d1 <= b0, "hit items %04x/%u and %04x/%u overlap",
                s->items[i].reg_id, s->items[i].opt, s->items[j].reg_id, s->items[j].opt);
        }
    }
    /* hit round-trip at every zoom */
    for (z = 0; z < 3; z++)
        for (i = 0; i < s->nitems; i++) {
            const struct RIGeoItem *it = &s->items[i];
            int opt = -2;
            uint16_t h;
            if (!is_hit(it))
                continue;
            h = ri_geo_hit_opt(s, ri_geo_px(it->cx, (int)z), ri_geo_px(it->cy, (int)z), (int)z, &opt);
            RI_ASSERT(h == it->reg_id, "hit(centre of %04x, zoom %u) = %04x", it->reg_id, z, h);
            RI_ASSERT(it->shape == RI_GEO_OPTION ? opt == it->opt
                : it->shape == RI_GEO_STEPPER ? opt == (it->opt ? RI_GEO_HIT_UP : RI_GEO_HIT_DOWN) : opt == -1,
                "option value of %04x", it->reg_id);
        }
    RI_ASSERT(ri_geo_hit(s, 1, 1, 0) == 0xFFFFu, "corner hits nothing");
}

#define ID(sec, i) (uint16_t)(((sec) << 8) | (i))

int main(void) {
    const struct RIGeoSection *s;
    uint32_t i;
    RI_ASSERT(ri_geo_px(1464, 0) == 732 && ri_geo_px(460, 0) == 230, "1x = 732x230 px");
    RI_ASSERT(ri_geo_px(1464, 2) == 1464 && ri_geo_px(1464, 1) == 1098, "2x / 1.5x");
    RI_ASSERT(ri_geo_px(100, 7) == 0, "unknown zoom -> 0");

    /* ---- 303 (p. 153) ---- */
    check_section(RI_SEC_SYNTH1);
    s = ri_geo_section(RI_SEC_SYNTH1);
    if (s) {
        const struct RIGeoItem *k[6];
        static const int black[13] = { 0, 1, 0, 1, 0, 0, 1, 0, 1, 0, 1, 0, 0 };
        int prev = -1;
        RI_ASSERT(s->w == 1464 && s->h == 460, "303 section = p. 153 figure");
        for (i = 0; i < 6; i++)
            k[i] = value_item(s, ID(RI_SEC_SYNTH1, i + 1));
        for (i = 1; i < 6; i++)
            if (k[i] && k[i - 1])
                RI_ASSERT(k[i]->cx - k[i - 1]->cx >= 130 && k[i]->cx - k[i - 1]->cx <= 133 &&
                    k[i]->cy == k[0]->cy, "303 knob pitch/row");
        for (i = 0; i < 13; i++) {
            const struct RIGeoItem *it = value_item(s, ID(RI_SEC_SYNTH1, 7 + i));
            if (!it)
                continue;
            RI_ASSERT(it->cx > prev, "key %u out of pitch order", i);
            prev = it->cx;
            RI_ASSERT(black[i] ? it->cy < 330 : it->cy > 330, "key %u row", i);
        }
    }

    /* ---- 808 (p. 148) ---- */
    check_section(RI_SEC_808);
    s = ri_geo_section(RI_SEC_808);
    if (s) {
        static const int lvl[12] = { 0, 1, 4, 7, 10, 13, 16, 18, 20, 21, 24, 26 }; /* AC..CH Level */
        int prev = -1;
        RI_ASSERT(s->w == 1472 && s->h == 468, "808 section = p. 148 figure");
        for (i = 0; i < 12; i++) {                 /* LEVEL row: 12 columns left to right, one row */
            const struct RIGeoItem *it = value_item(s, ID(RI_SEC_808, lvl[i]));
            RI_ASSERT(it && it->shape == RI_GEO_KNOB, "808 level %u", i);
            if (!it)
                continue;
            RI_ASSERT(it->cx > prev && it->cy == 62, "808 level column %u order/row", i);
            prev = it->cx;
        }
        prev = -1;
        for (i = 0; i < 16; i++) {                 /* 16 steps, uniform pitch, one row */
            const struct RIGeoItem *it = value_item(s, ID(RI_SEC_808, 28 + i));
            RI_ASSERT(it != 0, "step %u", i + 1);
            if (!it)
                continue;
            if (prev >= 0)
                RI_ASSERT(it->cx - prev >= 63 && it->cx - prev <= 66, "step pitch %d", it->cx - prev);
            prev = it->cx;
        }
        /* each parameter knob sits in its instrument's column (under its Level) */
        {
            static const int par[][2] = { { 2, 1 }, { 3, 1 }, { 5, 4 }, { 6, 4 }, { 8, 7 }, { 11, 10 },
                { 14, 13 }, { 22, 21 }, { 23, 21 }, { 25, 24 }, { 9, 7 }, { 12, 10 }, { 15, 13 },
                { 17, 16 }, { 19, 18 } };
            for (i = 0; i < sizeof(par) / sizeof(par[0]); i++) {
                const struct RIGeoItem *p = value_item(s, ID(RI_SEC_808, par[i][0]));
                const struct RIGeoItem *l = value_item(s, ID(RI_SEC_808, par[i][1]));
                RI_ASSERT(p && l && p->cx == l->cx && p->cy > l->cy, "808 control %d not under its Level",
                    par[i][0]);
            }
        }
    }
    /* ---- 909 (p. 151) ---- */
    check_section(RI_SEC_909);
    s = ri_geo_section(RI_SEC_909);
    if (s) {
        int prev = -1;
        RI_ASSERT(s->w == 1460 && s->h == 468, "909 section = p. 151 figure");
        for (i = 0; i < 16; i++) {
            const struct RIGeoItem *it = value_item(s, ID(RI_SEC_909, 30 + i));
            RI_ASSERT(it != 0, "909 step %u", i + 1);
            if (!it)
                continue;
            if (prev >= 0)
                RI_ASSERT(it->cx - prev == 84, "909 step pitch %d", it->cx - prev);
            prev = it->cx;
        }
        /* TR-909 grouping: each instrument's knobs sit over its own steps */
        {
            static const int own[][2] = { { 1, 1 }, { 2, 0 }, { 5, 3 }, { 6, 2 }, { 9, 5 }, { 13, 6 },
                { 15, 9 }, { 19, 10 }, { 20, 11 }, { 21, 12 }, { 18, 13 }, { 22, 13 }, { 23, 14 }, { 26, 15 } };
            for (i = 0; i < sizeof(own) / sizeof(own[0]); i++) {
                const struct RIGeoItem *k = value_item(s, ID(RI_SEC_909, own[i][0]));
                const struct RIGeoItem *st = value_item(s, ID(RI_SEC_909, 30 + own[i][1]));
                RI_ASSERT(k && st && k->cx == st->cx && k->cy < st->cy, "909 control %d not over step %d",
                    own[i][0], own[i][1] + 1);
            }
        }
    }
    /* ---- mixers (p. 157) + master (p. 23) ---- */
    for (i = RI_SEC_MIX_SYNTH1; i <= RI_SEC_MIX_909; i++) {
        const struct RIGeoSection *m = ri_geo_section(i), *m0 = ri_geo_section(RI_SEC_MIX_SYNTH1);
        uint32_t k;
        check_section(i);
        RI_ASSERT(m && m->w == 284 && m->h == 464, "mixer = p. 157 figure");
        if (!m || !m0)
            continue;
        RI_ASSERT(m->nitems == m0->nitems, "mixers share one layout");
        for (k = 0; k < m->nitems && k < m0->nitems; k++)
            RI_ASSERT((m->items[k].reg_id & 0xFF) == (m0->items[k].reg_id & 0xFF) && m->items[k].cx == m0->items[k].cx &&
                m->items[k].cy == m0->items[k].cy && m->items[k].shape == m0->items[k].shape, "mixer item %u", k);
        {   /* fader left of the switches, Pan above the fader, Delay below the switches */
            const struct RIGeoItem *fad = value_item(m, ID(i, 2)), *pan = value_item(m, ID(i, 3));
            const struct RIGeoItem *dly = value_item(m, ID(i, 4)), *cmp = value_item(m, ID(i, 7));
            RI_ASSERT(fad && pan && dly && cmp && pan->cy < fad->cy && fad->cx < cmp->cx && dly->cy > cmp->cy &&
                fad->h > 3 * fad->w, "mixer arrangement");
        }
    }
    check_section(RI_SEC_MASTER);
    s = ri_geo_section(RI_SEC_MASTER);
    if (s) {
        const struct RIGeoItem *l = value_item(s, ID(RI_SEC_MASTER, 1)), *f = value_item(s, ID(RI_SEC_MASTER, 0));
        const struct RIGeoItem *r = value_item(s, ID(RI_SEC_MASTER, 2));
        RI_ASSERT(s->w == 332 && s->h == 392, "master = p. 23 figure");
        RI_ASSERT(l && f && r && l->cx < f->cx && f->cx < r->cx && l->h == r->h, "meters either side of the fader");
    }
    /* ---- FX units (p. 159, 161, 163, 164) ---- */
    for (i = RI_SEC_PCF; i <= RI_SEC_COMP; i++) {
        const struct RIGeoItem *on, *mt;
        check_section(i);
        s = ri_geo_section(i);
        if (!s)
            continue;
        on = value_item(s, ID(i, 0));
        mt = value_item(s, ID(i, 1));
        RI_ASSERT(on && mt && on->cx < mt->cx && on->cy == mt->cy && on->cy < 50, "%s header: on/off left, meter right",
            ri_ctlreg_section_name(i));
    }
    s = ri_geo_section(RI_SEC_PCF);
    if (s) {
        uint32_t k;
        RI_ASSERT(s->w == 332 && s->h == 424 && has_steppers(s, ID(RI_SEC_PCF, 2)), "PCF = p. 159, pattern arrows");
        for (k = 4; k < 8; k++) {       /* four sliders in one row, Freq Q Amt Dec */
            const struct RIGeoItem *f = value_item(s, ID(RI_SEC_PCF, k));
            const struct RIGeoItem *g0 = value_item(s, ID(RI_SEC_PCF, k - 1));
            RI_ASSERT(f && f->shape == RI_GEO_RECT && f->h > 2 * f->w, "PCF slider %u", k);
            if (k > 4 && f && g0)
                RI_ASSERT(f->cx > g0->cx && f->cy == g0->cy, "PCF slider order %u", k);
        }
    }
    s = ri_geo_section(RI_SEC_DELAY);
    RI_ASSERT(s && s->w == 332 && s->h == 376 && has_steppers(s, ID(RI_SEC_DELAY, 2)), "Delay = p. 161, steps arrows");
    s = ri_geo_section(RI_SEC_DIST);
    RI_ASSERT(s && s->w == 336 && s->h == 264, "Dist = p. 163");
    s = ri_geo_section(RI_SEC_COMP);
    RI_ASSERT(s && s->w == 332 && s->h == 376, "Comp = p. 164");
    RI_RESULT("panelgeo");
}
