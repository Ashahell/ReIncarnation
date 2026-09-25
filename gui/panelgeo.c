/* gui/panelgeo.c — section panel geometry (§12.10 G2).
 * 303 section measured from the Owner's Manual p. 153 figure (366 x 115 px)
 * at 4x with a 10-px grid; values in Q (quarter figure px). Knob pitch is
 * uniform 131.5 Q (32.9 px) Tune..Accent, the TB-303 control order. The
 * other sections land with their own slices (ri_geo_section returns NULL).
 * Ledger: docs/evidence/gui/panel-geometry.md.
 */
#include "gui/panelgeo.h"
#include "gui/ctlreg.h"

#define S1(i) (uint16_t)((RI_SEC_SYNTH1 << 8) | (i))
#define KNOB(i, x) { S1(i), RI_GEO_KNOB, 0, (x), 110, 84, 120 }, \
                   { S1(i), RI_GEO_LEGEND, 0, (x), 20, 0, 0 }
#define WKEY(i, x) { S1(i), RI_GEO_RECT, 0, (x), 385, 34, 60 }, \
                   { S1(i), RI_GEO_LED, 0, (x), 338, 12, 12 }
#define BKEY(i, x) { S1(i), RI_GEO_RECT, 0, (x), 275, 30, 50 }, \
                   { S1(i), RI_GEO_LED, 0, (x), 238, 12, 12 }
#define SBTN(i, x) { S1(i), RI_GEO_RECT, 0, (x), 385, 34, 60 }, \
                   { S1(i), RI_GEO_LED, 0, (x), 338, 12, 12 }, \
                   { S1(i), RI_GEO_LEGEND, 0, (x), 305, 0, 0 }

static const struct RIGeoItem RI_GEO_303[] = {
    /* top row */
    { S1(0), RI_GEO_RECT, 0, 135, 62, 180, 36 },     /* Waveform switch */
    { S1(0), RI_GEO_LEGEND, 0, 132, 20, 0, 0 },
    { S1(0), RI_GEO_DIVIDER, 0, 262, 30, 0, 135 },
    KNOB(1, 352), KNOB(2, 484), KNOB(3, 615), KNOB(4, 747), KNOB(5, 878), KNOB(6, 1010),
    { S1(6), RI_GEO_DIVIDER, 0, 1099, 30, 0, 130 },
    { S1(29), RI_GEO_RECT, 0, 1356, 130, 73, 65 },   /* EDIT STEP display */
    { S1(29), RI_GEO_LEGEND, 0, 1356, 62, 0, 0 },
    /* left block */
    { S1(27), RI_GEO_RECT, 0, 109, 311, 53, 22 },    /* Pitch Mode */
    { S1(27), RI_GEO_LED, 0, 108, 278, 12, 12 },
    { S1(27), RI_GEO_LEGEND, 0, 110, 240, 0, 0 },
    { S1(28), RI_GEO_RECT, 0, 109, 402, 62, 28 },    /* Clear */
    { S1(28), RI_GEO_LEGEND, 0, 110, 365, 0, 0 },
    /* keyboard: C C# D D# E F F# G G# A A# B C (registry 7..19) */
    WKEY(7, 222), BKEY(8, 267), WKEY(9, 307), BKEY(10, 349), WKEY(11, 392),
    WKEY(12, 476), BKEY(13, 518), WKEY(14, 560), BKEY(15, 602), WKEY(16, 645),
    BKEY(17, 687), WKEY(18, 730), WKEY(19, 812),
    /* Note/Pause toggle with its two state LEDs */
    { S1(24), RI_GEO_RECT, 0, 1212, 252, 60, 27 },
    { S1(24), RI_GEO_LED, 0, 996, 250, 12, 12 },
    { S1(24), RI_GEO_LED, 0, 1152, 250, 12, 12 },
    /* Down Up Accent Slide */
    SBTN(20, 909), SBTN(21, 1011), SBTN(22, 1111), SBTN(23, 1211),
    /* Back / Step */
    { S1(25), RI_GEO_RECT, 0, 1345, 267, 58, 30 },
    { S1(25), RI_GEO_LEGEND, 0, 1344, 225, 0, 0 },
    { S1(26), RI_GEO_RECT, 0, 1349, 392, 86, 53 },
    { S1(26), RI_GEO_LEGEND, 0, 1349, 343, 0, 0 },
};

static const struct RIGeoSection RI_GEO_SECTIONS[] = {
    { RI_SEC_SYNTH1, 0, 1464, 460, RI_GEO_303,
      (uint32_t)(sizeof(RI_GEO_303) / sizeof(RI_GEO_303[0])) },
};

const struct RIGeoSection *ri_geo_section(uint32_t section) {
    uint32_t i;
    for (i = 0; i < sizeof(RI_GEO_SECTIONS) / sizeof(RI_GEO_SECTIONS[0]); i++)
        if (RI_GEO_SECTIONS[i].section == section)
            return &RI_GEO_SECTIONS[i];
    return 0;
}

/* zoom factor as a fraction: 1x = 2/2, 1.5x = 3/2, 2x = 4/2 */
static int zoom_num(int zoom) {
    return zoom == 0 ? 2 : zoom == 1 ? 3 : zoom == 2 ? 4 : 0;
}

int ri_geo_px(int q, int zoom) {
    long num = (long)q * RI_GEO_BASE_SCALE_NUM * zoom_num(zoom);
    long den = 4L * RI_GEO_BASE_SCALE_DEN * 2L;
    if (!zoom_num(zoom))
        return 0;
    return (int)(num >= 0 ? (num + den / 2) / den : -((-num + den / 2) / den));
}

uint16_t ri_geo_hit(const struct RIGeoSection *s, int x, int y, int zoom) {
    uint32_t i;
    if (!s || !zoom_num(zoom))
        return 0xFFFFu;
    for (i = 0; i < s->nitems; i++) {
        const struct RIGeoItem *it = &s->items[i];
        int cx = ri_geo_px(it->cx, zoom), cy = ri_geo_px(it->cy, zoom);
        if (it->shape == RI_GEO_KNOB) {
            long r = ri_geo_px(it->h, zoom) / 2, dx = x - cx, dy = y - cy;
            if (dx * dx + dy * dy <= r * r)
                return it->reg_id;
        } else if (it->shape == RI_GEO_RECT) {
            int hw = ri_geo_px(it->w, zoom) / 2, hh = ri_geo_px(it->h, zoom) / 2;
            if (x >= cx - hw && x <= cx + hw && y >= cy - hh && y <= cy + hh)
                return it->reg_id;
        }
    }
    return 0xFFFFu;
}
