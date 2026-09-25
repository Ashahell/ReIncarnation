/* t61_panelgeo — section geometry (§12.10 G2, 303 slice).
 * Invariants: every Synth-1 registry control has exactly one value item
 * (knob or rect) and nothing else does; value items sit inside the section
 * and never overlap; decorations belong to a real control; knob order and
 * pitch follow the manual figure (Tune..Accent left to right, uniform
 * pitch); keyboard keys are in pitch order left to right with black keys
 * above white keys; zoom scaling and hit-testing round-trip.
 */
#include <stdio.h>
#include <string.h>
#include "tests/helpers/ri_assert.h"
#include "gui/panelgeo.h"
#include "gui/ctlreg.h"

static int is_value(const struct RIGeoItem *it) {
    return it->shape == RI_GEO_KNOB || it->shape == RI_GEO_RECT;
}

static void box(const struct RIGeoItem *it, int *x0, int *y0, int *x1, int *y1) {
    int w = it->shape == RI_GEO_KNOB ? it->h : it->w;
    int h = it->shape == RI_GEO_KNOB ? it->h : it->h;
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

int main(void) {
    const struct RIGeoSection *s = ri_geo_section(RI_SEC_SYNTH1);
    uint32_t i, j, nsec, nval = 0;
    RI_ASSERT(s != 0, "303 section geometry missing");
    if (!s)
        RI_RESULT("panelgeo");
    RI_ASSERT(s->w == 1464 && s->h == 460, "303 section is the p. 153 figure (366x115 px)");
    RI_ASSERT(ri_geo_px(s->w, 0) == 732 && ri_geo_px(s->h, 0) == 230, "1x = 732x230 px");
    RI_ASSERT(ri_geo_px(s->w, 2) == 1464, "2x doubles");
    RI_ASSERT(ri_geo_px(s->w, 1) == 1098, "1.5x");
    RI_ASSERT(ri_geo_px(100, 7) == 0, "unknown zoom -> 0");

    /* 1:1 registry <-> value items */
    nsec = ri_ctlreg_section_count(RI_SEC_SYNTH1, 0);
    for (i = 0; i < s->nitems; i++) {
        const struct RIGeoItem *it = &s->items[i];
        const struct RICtlDef *d = ri_ctlreg_find(it->reg_id);
        RI_ASSERT(d && d->section == RI_SEC_SYNTH1, "item %u owns unknown control %04x", i, it->reg_id);
        if (!is_value(it))
            continue;
        nval++;
        for (j = i + 1; j < s->nitems; j++)
            if (is_value(&s->items[j]))
                RI_ASSERT(s->items[j].reg_id != it->reg_id, "control %04x has two value items", it->reg_id);
        if (d)
            RI_ASSERT((it->shape == RI_GEO_KNOB) == (d->kind == RI_CK_KNOB),
                "%s: knob shape iff knob kind", d->legend);
    }
    RI_ASSERT(nval == nsec, "value items %u, Synth 1 registry controls %u", nval, nsec);

    /* inside the section, no overlaps between value items */
    for (i = 0; i < s->nitems; i++) {
        int a0, b0, a1, b1;
        if (!is_value(&s->items[i]))
            continue;
        box(&s->items[i], &a0, &b0, &a1, &b1);
        RI_ASSERT(a0 >= 0 && b0 >= 0 && a1 <= s->w && b1 <= s->h, "item %04x outside section",
            s->items[i].reg_id);
        for (j = i + 1; j < s->nitems; j++) {
            int c0, d0, c1, d1;
            if (!is_value(&s->items[j]))
                continue;
            box(&s->items[j], &c0, &d0, &c1, &d1);
            RI_ASSERT(a1 <= c0 || c1 <= a0 || b1 <= d0 || d1 <= b0, "items %04x and %04x overlap",
                s->items[i].reg_id, s->items[j].reg_id);
        }
    }

    /* knob row: Tune..Accent left to right, one row, uniform pitch (±1 Q) */
    {
        const struct RIGeoItem *k[6];
        for (i = 0; i < 6; i++) {
            k[i] = value_item(s, (uint16_t)((RI_SEC_SYNTH1 << 8) | (i + 1)));
            RI_ASSERT(k[i] && k[i]->shape == RI_GEO_KNOB, "knob %u missing", i + 1);
        }
        for (i = 1; i < 6; i++)
            if (k[i] && k[i - 1]) {
                int d = k[i]->cx - k[i - 1]->cx;
                RI_ASSERT(d >= 130 && d <= 133, "knob pitch %d Q (figure: 131.5)", d);
                RI_ASSERT(k[i]->cy == k[0]->cy, "knobs on one row");
            }
        RI_ASSERT(value_item(s, (uint16_t)(RI_SEC_SYNTH1 << 8))->cx < k[0]->cx, "Waveform left of Tune");
    }

    /* keyboard: 13 keys in pitch order; black keys (C# D# F# G# A#) above whites */
    {
        static const int black[13] = { 0, 1, 0, 1, 0, 0, 1, 0, 1, 0, 1, 0, 0 };
        int prev = -1;
        for (i = 0; i < 13; i++) {
            const struct RIGeoItem *it = value_item(s, (uint16_t)((RI_SEC_SYNTH1 << 8) | (7 + i)));
            RI_ASSERT(it != 0, "key %u missing", i);
            if (!it)
                continue;
            RI_ASSERT(it->cx > prev, "key %u out of pitch order", i);
            prev = it->cx;
            RI_ASSERT(black[i] ? it->cy < 330 : it->cy > 330, "key %u row wrong (black above white)", i);
        }
    }

    /* hit test round-trip at every zoom for every value item */
    for (j = 0; j < 3; j++)
        for (i = 0; i < s->nitems; i++) {
            const struct RIGeoItem *it = &s->items[i];
            if (!is_value(it))
                continue;
            RI_ASSERT(ri_geo_hit(s, ri_geo_px(it->cx, (int)j), ri_geo_px(it->cy, (int)j), (int)j) == it->reg_id,
                "hit(centre of %04x, zoom %u)", it->reg_id, j);
        }
    RI_ASSERT(ri_geo_hit(s, 1, 1, 0) == 0xFFFFu, "corner hits nothing");
    RI_RESULT("panelgeo");
}
