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

/* 808 section: Owner's Manual p. 148 figure (368 x 117 px = 1472 x 468 Q).
 * Twelve columns AC BD SD LT MT HT RS CP CB CY OH CH at 87.6 Q pitch: red
 * LEVEL row, white parameter knobs, sound switches (alternate legends LC MC
 * HC CL MA above them), instrument legends, 16 steps at 64.3 Q pitch in
 * TR-808 colour groups, and the large Instrument Selection knob with its
 * 12-position ring (AC -> CH clockwise, 28.2 deg apart, from 208 deg). */
#define S8(i) (uint16_t)((RI_SEC_808 << 8) | (i))
static const struct RIGeoItem RI_GEO_808[] = {
    { S8(0), RI_GEO_KNOB, 0, 92, 62, 40, 56 },
    { S8(0), RI_GEO_LEGEND, 0, 92, 18, 0, 0 },
    { S8(1), RI_GEO_KNOB, 0, 180, 62, 40, 56 },
    { S8(1), RI_GEO_LEGEND, 0, 180, 18, 0, 0 },
    { S8(4), RI_GEO_KNOB, 0, 267, 62, 40, 56 },
    { S8(4), RI_GEO_LEGEND, 0, 267, 18, 0, 0 },
    { S8(7), RI_GEO_KNOB, 0, 355, 62, 40, 56 },
    { S8(7), RI_GEO_LEGEND, 0, 355, 18, 0, 0 },
    { S8(10), RI_GEO_KNOB, 0, 442, 62, 40, 56 },
    { S8(10), RI_GEO_LEGEND, 0, 442, 18, 0, 0 },
    { S8(13), RI_GEO_KNOB, 0, 530, 62, 40, 56 },
    { S8(13), RI_GEO_LEGEND, 0, 530, 18, 0, 0 },
    { S8(16), RI_GEO_KNOB, 0, 617, 62, 40, 56 },
    { S8(16), RI_GEO_LEGEND, 0, 617, 18, 0, 0 },
    { S8(18), RI_GEO_KNOB, 0, 705, 62, 40, 56 },
    { S8(18), RI_GEO_LEGEND, 0, 705, 18, 0, 0 },
    { S8(20), RI_GEO_KNOB, 0, 793, 62, 40, 56 },
    { S8(20), RI_GEO_LEGEND, 0, 793, 18, 0, 0 },
    { S8(21), RI_GEO_KNOB, 0, 880, 62, 40, 56 },
    { S8(21), RI_GEO_LEGEND, 0, 880, 18, 0, 0 },
    { S8(24), RI_GEO_KNOB, 0, 968, 62, 40, 56 },
    { S8(24), RI_GEO_LEGEND, 0, 968, 18, 0, 0 },
    { S8(26), RI_GEO_KNOB, 0, 1056, 62, 40, 56 },
    { S8(26), RI_GEO_LEGEND, 0, 1056, 18, 0, 0 },
    { S8(2), RI_GEO_KNOB, 0, 180, 148, 36, 44 },
    { S8(2), RI_GEO_LEGEND, 0, 180, 108, 0, 0 },
    { S8(5), RI_GEO_KNOB, 0, 267, 148, 36, 44 },
    { S8(5), RI_GEO_LEGEND, 0, 267, 108, 0, 0 },
    { S8(8), RI_GEO_KNOB, 0, 355, 148, 36, 44 },
    { S8(8), RI_GEO_LEGEND, 0, 355, 108, 0, 0 },
    { S8(11), RI_GEO_KNOB, 0, 442, 148, 36, 44 },
    { S8(11), RI_GEO_LEGEND, 0, 442, 108, 0, 0 },
    { S8(14), RI_GEO_KNOB, 0, 530, 148, 36, 44 },
    { S8(14), RI_GEO_LEGEND, 0, 530, 108, 0, 0 },
    { S8(22), RI_GEO_KNOB, 0, 880, 148, 36, 44 },
    { S8(22), RI_GEO_LEGEND, 0, 880, 108, 0, 0 },
    { S8(3), RI_GEO_KNOB, 0, 180, 238, 36, 44 },
    { S8(3), RI_GEO_LEGEND, 0, 180, 205, 0, 0 },
    { S8(6), RI_GEO_KNOB, 0, 267, 238, 36, 44 },
    { S8(6), RI_GEO_LEGEND, 0, 267, 205, 0, 0 },
    { S8(23), RI_GEO_KNOB, 0, 880, 238, 36, 44 },
    { S8(23), RI_GEO_LEGEND, 0, 880, 205, 0, 0 },
    { S8(25), RI_GEO_KNOB, 0, 968, 238, 36, 44 },
    { S8(25), RI_GEO_LEGEND, 0, 968, 205, 0, 0 },
    { S8(9), RI_GEO_RECT, 0, 355, 258, 38, 40 },
    { S8(9), RI_GEO_LEGEND, 0, 355, 205, 0, 0 },
    { S8(12), RI_GEO_RECT, 0, 442, 258, 38, 40 },
    { S8(12), RI_GEO_LEGEND, 0, 442, 205, 0, 0 },
    { S8(15), RI_GEO_RECT, 0, 530, 258, 38, 40 },
    { S8(15), RI_GEO_LEGEND, 0, 530, 205, 0, 0 },
    { S8(17), RI_GEO_RECT, 0, 617, 258, 38, 40 },
    { S8(17), RI_GEO_LEGEND, 0, 617, 205, 0, 0 },
    { S8(19), RI_GEO_RECT, 0, 705, 258, 38, 40 },
    { S8(19), RI_GEO_LEGEND, 0, 705, 205, 0, 0 },
    { S8(27), RI_GEO_OPTION, 0, 92, 312, 70, 32 },
    { S8(27), RI_GEO_OPTION, 1, 180, 312, 70, 32 },
    { S8(27), RI_GEO_OPTION, 2, 267, 312, 70, 32 },
    { S8(27), RI_GEO_OPTION, 3, 355, 312, 70, 32 },
    { S8(27), RI_GEO_OPTION, 4, 442, 312, 70, 32 },
    { S8(27), RI_GEO_OPTION, 5, 530, 312, 70, 32 },
    { S8(27), RI_GEO_OPTION, 6, 617, 312, 70, 32 },
    { S8(27), RI_GEO_OPTION, 7, 705, 312, 70, 32 },
    { S8(27), RI_GEO_OPTION, 8, 793, 312, 70, 32 },
    { S8(27), RI_GEO_OPTION, 9, 880, 312, 70, 32 },
    { S8(27), RI_GEO_OPTION, 10, 968, 312, 70, 32 },
    { S8(27), RI_GEO_OPTION, 11, 1056, 312, 70, 32 },
    { S8(27), RI_GEO_KNOB, 0, 1295, 205, 104, 110 },
    { S8(27), RI_GEO_OPTION, 0, 1248, 293, 44, 24 },
    { S8(27), RI_GEO_OPTION, 1, 1212, 261, 44, 24 },
    { S8(27), RI_GEO_OPTION, 2, 1195, 215, 44, 24 },
    { S8(27), RI_GEO_OPTION, 3, 1203, 167, 44, 24 },
    { S8(27), RI_GEO_OPTION, 4, 1232, 128, 44, 24 },
    { S8(27), RI_GEO_OPTION, 5, 1276, 107, 44, 24 },
    { S8(27), RI_GEO_OPTION, 6, 1325, 109, 44, 24 },
    { S8(27), RI_GEO_OPTION, 7, 1366, 135, 44, 24 },
    { S8(27), RI_GEO_OPTION, 8, 1391, 177, 44, 24 },
    { S8(27), RI_GEO_OPTION, 9, 1393, 225, 44, 24 },
    { S8(27), RI_GEO_OPTION, 10, 1372, 269, 44, 24 },
    { S8(27), RI_GEO_OPTION, 11, 1332, 298, 44, 24 },
    { S8(28), RI_GEO_RECT, 0, 92, 400, 50, 76 },
    { S8(29), RI_GEO_RECT, 0, 156, 400, 50, 76 },
    { S8(30), RI_GEO_RECT, 0, 221, 400, 50, 76 },
    { S8(31), RI_GEO_RECT, 0, 285, 400, 50, 76 },
    { S8(32), RI_GEO_RECT, 0, 349, 400, 50, 76 },
    { S8(33), RI_GEO_RECT, 0, 413, 400, 50, 76 },
    { S8(34), RI_GEO_RECT, 0, 478, 400, 50, 76 },
    { S8(35), RI_GEO_RECT, 0, 542, 400, 50, 76 },
    { S8(36), RI_GEO_RECT, 0, 606, 400, 50, 76 },
    { S8(37), RI_GEO_RECT, 0, 670, 400, 50, 76 },
    { S8(38), RI_GEO_RECT, 0, 735, 400, 50, 76 },
    { S8(39), RI_GEO_RECT, 0, 799, 400, 50, 76 },
    { S8(40), RI_GEO_RECT, 0, 863, 400, 50, 76 },
    { S8(41), RI_GEO_RECT, 0, 927, 400, 50, 76 },
    { S8(42), RI_GEO_RECT, 0, 992, 400, 50, 76 },
    { S8(43), RI_GEO_RECT, 0, 1056, 400, 50, 76 },
};

static const struct RIGeoSection RI_GEO_SECTIONS[] = {
    { RI_SEC_SYNTH1, 0, 1464, 460, RI_GEO_303,
      (uint32_t)(sizeof(RI_GEO_303) / sizeof(RI_GEO_303[0])) },
    { RI_SEC_808, 0, 1472, 468, RI_GEO_808,
      (uint32_t)(sizeof(RI_GEO_808) / sizeof(RI_GEO_808[0])) },
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

uint16_t ri_geo_hit_opt(const struct RIGeoSection *s, int x, int y, int zoom, int *opt) {
    uint32_t i;
    if (opt)
        *opt = -1;
    if (!s || !zoom_num(zoom))
        return 0xFFFFu;
    for (i = 0; i < s->nitems; i++) {
        const struct RIGeoItem *it = &s->items[i];
        int cx = ri_geo_px(it->cx, zoom), cy = ri_geo_px(it->cy, zoom);
        if (it->shape == RI_GEO_KNOB) {
            long r = ri_geo_px(it->h, zoom) / 2, dx = x - cx, dy = y - cy;
            if (dx * dx + dy * dy <= r * r)
                return it->reg_id;
        } else if (it->shape == RI_GEO_RECT || it->shape == RI_GEO_OPTION) {
            int hw = ri_geo_px(it->w, zoom) / 2, hh = ri_geo_px(it->h, zoom) / 2;
            if (x >= cx - hw && x <= cx + hw && y >= cy - hh && y <= cy + hh) {
                if (opt && it->shape == RI_GEO_OPTION)
                    *opt = it->opt;
                return it->reg_id;
            }
        }
    }
    return 0xFFFFu;
}

uint16_t ri_geo_hit(const struct RIGeoSection *s, int x, int y, int zoom) {
    return ri_geo_hit_opt(s, x, y, zoom, 0);
}
