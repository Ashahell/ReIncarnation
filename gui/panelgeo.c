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

/* 909 section: Owner's Manual p. 151 figure (365 x 117 px = 1460 x 468 Q).
 * Steps at 84 Q pitch; instruments grouped over their step columns as on
 * the TR-909 (BD 1-2, SD 3-4, LT 5-6, MT 7-8, HT 9-10, RS 11, CP 12, CH 13,
 * OH 14, CC 15, RC 16); knob rows at y 140 / 238; instrument-select legend
 * boxes (options) at y 322; AC Level, Flam knob and Flam button on the left.
 * No selector knob: the 909 selects instruments by clicking legends (p. 151). */
#define S9(i) (uint16_t)((RI_SEC_909 << 8) | (i))
static const struct RIGeoItem RI_GEO_909[] = {
    { S9(0), RI_GEO_KNOB, 0, 60, 140, 44, 52 },
    { S9(0), RI_GEO_LEGEND, 0, 60, 96, 0, 0 },
    { S9(2), RI_GEO_KNOB, 0, 148, 140, 44, 52 },
    { S9(2), RI_GEO_LEGEND, 0, 148, 96, 0, 0 },
    { S9(1), RI_GEO_KNOB, 0, 232, 140, 44, 52 },
    { S9(1), RI_GEO_LEGEND, 0, 232, 96, 0, 0 },
    { S9(3), RI_GEO_KNOB, 0, 148, 238, 44, 52 },
    { S9(3), RI_GEO_LEGEND, 0, 148, 196, 0, 0 },
    { S9(4), RI_GEO_KNOB, 0, 232, 238, 44, 52 },
    { S9(4), RI_GEO_LEGEND, 0, 232, 196, 0, 0 },
    { S9(6), RI_GEO_KNOB, 0, 316, 140, 44, 52 },
    { S9(6), RI_GEO_LEGEND, 0, 316, 96, 0, 0 },
    { S9(5), RI_GEO_KNOB, 0, 400, 140, 44, 52 },
    { S9(5), RI_GEO_LEGEND, 0, 400, 96, 0, 0 },
    { S9(7), RI_GEO_KNOB, 0, 316, 238, 44, 52 },
    { S9(7), RI_GEO_LEGEND, 0, 316, 196, 0, 0 },
    { S9(8), RI_GEO_KNOB, 0, 400, 238, 44, 52 },
    { S9(8), RI_GEO_LEGEND, 0, 400, 196, 0, 0 },
    { S9(10), RI_GEO_KNOB, 0, 484, 140, 44, 52 },
    { S9(10), RI_GEO_LEGEND, 0, 484, 96, 0, 0 },
    { S9(9), RI_GEO_KNOB, 0, 568, 140, 44, 52 },
    { S9(9), RI_GEO_LEGEND, 0, 568, 96, 0, 0 },
    { S9(11), RI_GEO_KNOB, 0, 484, 238, 44, 52 },
    { S9(11), RI_GEO_LEGEND, 0, 484, 196, 0, 0 },
    { S9(13), RI_GEO_KNOB, 0, 652, 140, 44, 52 },
    { S9(13), RI_GEO_LEGEND, 0, 652, 96, 0, 0 },
    { S9(12), RI_GEO_KNOB, 0, 736, 140, 44, 52 },
    { S9(12), RI_GEO_LEGEND, 0, 736, 96, 0, 0 },
    { S9(14), RI_GEO_KNOB, 0, 652, 238, 44, 52 },
    { S9(14), RI_GEO_LEGEND, 0, 652, 196, 0, 0 },
    { S9(16), RI_GEO_KNOB, 0, 820, 140, 44, 52 },
    { S9(16), RI_GEO_LEGEND, 0, 820, 96, 0, 0 },
    { S9(15), RI_GEO_KNOB, 0, 904, 140, 44, 52 },
    { S9(15), RI_GEO_LEGEND, 0, 904, 96, 0, 0 },
    { S9(17), RI_GEO_KNOB, 0, 820, 238, 44, 52 },
    { S9(17), RI_GEO_LEGEND, 0, 820, 196, 0, 0 },
    { S9(19), RI_GEO_KNOB, 0, 988, 140, 44, 52 },
    { S9(19), RI_GEO_LEGEND, 0, 988, 96, 0, 0 },
    { S9(20), RI_GEO_KNOB, 0, 1072, 140, 44, 52 },
    { S9(20), RI_GEO_LEGEND, 0, 1072, 96, 0, 0 },
    { S9(21), RI_GEO_KNOB, 0, 1156, 238, 44, 52 },
    { S9(21), RI_GEO_LEGEND, 0, 1156, 196, 0, 0 },
    { S9(18), RI_GEO_KNOB, 0, 1240, 140, 44, 52 },
    { S9(18), RI_GEO_LEGEND, 0, 1240, 96, 0, 0 },
    { S9(22), RI_GEO_KNOB, 0, 1240, 238, 44, 52 },
    { S9(22), RI_GEO_LEGEND, 0, 1240, 196, 0, 0 },
    { S9(23), RI_GEO_KNOB, 0, 1324, 140, 44, 52 },
    { S9(23), RI_GEO_LEGEND, 0, 1324, 96, 0, 0 },
    { S9(24), RI_GEO_KNOB, 0, 1324, 238, 44, 52 },
    { S9(24), RI_GEO_LEGEND, 0, 1324, 196, 0, 0 },
    { S9(25), RI_GEO_KNOB, 0, 1408, 140, 44, 52 },
    { S9(25), RI_GEO_LEGEND, 0, 1408, 96, 0, 0 },
    { S9(26), RI_GEO_KNOB, 0, 1408, 238, 44, 52 },
    { S9(26), RI_GEO_LEGEND, 0, 1408, 196, 0, 0 },
    { S9(27), RI_GEO_KNOB, 0, 60, 335, 44, 52 },
    { S9(27), RI_GEO_LEGEND, 0, 60, 292, 0, 0 },
    { S9(29), RI_GEO_RECT, 0, 60, 405, 40, 40 },
    { S9(29), RI_GEO_LED, 0, 60, 380, 10, 10 },
    { S9(28), RI_GEO_OPTION, 0, 60, 222, 60, 26 },
    { S9(28), RI_GEO_OPTION, 1, 190, 322, 150, 26 },
    { S9(28), RI_GEO_OPTION, 2, 358, 322, 150, 26 },
    { S9(28), RI_GEO_OPTION, 3, 526, 322, 150, 26 },
    { S9(28), RI_GEO_OPTION, 4, 694, 322, 150, 26 },
    { S9(28), RI_GEO_OPTION, 5, 862, 322, 150, 26 },
    { S9(28), RI_GEO_OPTION, 6, 988, 322, 70, 26 },
    { S9(28), RI_GEO_OPTION, 7, 1072, 322, 70, 26 },
    { S9(28), RI_GEO_OPTION, 8, 1156, 322, 70, 26 },
    { S9(28), RI_GEO_OPTION, 9, 1240, 322, 70, 26 },
    { S9(28), RI_GEO_OPTION, 10, 1324, 322, 70, 26 },
    { S9(28), RI_GEO_OPTION, 11, 1408, 322, 70, 26 },
    { S9(30), RI_GEO_RECT, 0, 148, 378, 62, 62 },
    { S9(31), RI_GEO_RECT, 0, 232, 378, 62, 62 },
    { S9(32), RI_GEO_RECT, 0, 316, 378, 62, 62 },
    { S9(33), RI_GEO_RECT, 0, 400, 378, 62, 62 },
    { S9(34), RI_GEO_RECT, 0, 484, 378, 62, 62 },
    { S9(35), RI_GEO_RECT, 0, 568, 378, 62, 62 },
    { S9(36), RI_GEO_RECT, 0, 652, 378, 62, 62 },
    { S9(37), RI_GEO_RECT, 0, 736, 378, 62, 62 },
    { S9(38), RI_GEO_RECT, 0, 820, 378, 62, 62 },
    { S9(39), RI_GEO_RECT, 0, 904, 378, 62, 62 },
    { S9(40), RI_GEO_RECT, 0, 988, 378, 62, 62 },
    { S9(41), RI_GEO_RECT, 0, 1072, 378, 62, 62 },
    { S9(42), RI_GEO_RECT, 0, 1156, 378, 62, 62 },
    { S9(43), RI_GEO_RECT, 0, 1240, 378, 62, 62 },
    { S9(44), RI_GEO_RECT, 0, 1324, 378, 62, 62 },
    { S9(45), RI_GEO_RECT, 0, 1408, 378, 62, 62 },
};

/* Section mixer: Owner's Manual p. 157 figure (71 x 116 px = 284 x 464 Q),
 * identical for Synth 1/2, 808 and 909. Header: on/off (mute) button left,
 * "MIX", output meter right; Pan knob top-left, Dist/PCF/Comp switches with
 * LEDs down the right, volume fader bottom-left, Delay knob bottom-right. */
#define MX(sec, i) (uint16_t)(((sec) << 8) | (i))
#define RI_GEO_MIXER(sec) \
    { MX(sec, 0), RI_GEO_RECT, 0, 49, 46, 32, 32 }, \
    { MX(sec, 1), RI_GEO_RECT, 0, 235, 45, 24, 44 }, \
    { MX(sec, 3), RI_GEO_KNOB, 0, 80, 132, 45, 70 }, \
    { MX(sec, 3), RI_GEO_LEGEND, 0, 80, 215, 0, 0 }, \
    { MX(sec, 5), RI_GEO_RECT, 0, 200, 96, 45, 22 }, \
    { MX(sec, 5), RI_GEO_LED, 0, 245, 98, 8, 8 }, \
    { MX(sec, 5), RI_GEO_LEGEND, 0, 200, 131, 0, 0 }, \
    { MX(sec, 6), RI_GEO_RECT, 0, 200, 168, 45, 22 }, \
    { MX(sec, 6), RI_GEO_LED, 0, 245, 170, 8, 8 }, \
    { MX(sec, 6), RI_GEO_LEGEND, 0, 200, 205, 0, 0 }, \
    { MX(sec, 7), RI_GEO_RECT, 0, 200, 241, 45, 22 }, \
    { MX(sec, 7), RI_GEO_LED, 0, 245, 242, 8, 8 }, \
    { MX(sec, 7), RI_GEO_LEGEND, 0, 200, 272, 0, 0 }, \
    { MX(sec, 2), RI_GEO_RECT, 0, 75, 345, 60, 200 }, \
    { MX(sec, 4), RI_GEO_KNOB, 0, 205, 352, 45, 70 }, \
    { MX(sec, 4), RI_GEO_LEGEND, 0, 205, 432, 0, 0 }
static const struct RIGeoItem RI_GEO_MIX1[] = { RI_GEO_MIXER(RI_SEC_MIX_SYNTH1) };
static const struct RIGeoItem RI_GEO_MIX2[] = { RI_GEO_MIXER(RI_SEC_MIX_SYNTH2) };
static const struct RIGeoItem RI_GEO_MIX8[] = { RI_GEO_MIXER(RI_SEC_MIX_808) };
static const struct RIGeoItem RI_GEO_MIX9[] = { RI_GEO_MIXER(RI_SEC_MIX_909) };

/* Master: p. 23 figure (83 x 98 px = 332 x 392 Q). "MASTER" header, L/R
 * meters (clip lamp on top) either side of the level fader, Comp switch
 * with LED at the bottom. */
static const struct RIGeoItem RI_GEO_MASTER[] = {
    { MX(RI_SEC_MASTER, 1), RI_GEO_RECT, 0, 98, 209, 38, 192 },
    { MX(RI_SEC_MASTER, 0), RI_GEO_RECT, 0, 165, 208, 58, 200 },
    { MX(RI_SEC_MASTER, 2), RI_GEO_RECT, 0, 238, 209, 38, 192 },
    { MX(RI_SEC_MASTER, 3), RI_GEO_RECT, 0, 202, 345, 45, 22 },
    { MX(RI_SEC_MASTER, 3), RI_GEO_LED, 0, 245, 345, 8, 8 },
    { MX(RI_SEC_MASTER, 3), RI_GEO_LEGEND, 0, 125, 350, 0, 0 },
};

/* FX units: Owner's Manual figures p. 159 (PCF 83 x 106 px = 332 x 424 Q),
 * p. 161 (Delay 83 x 94 px = 332 x 376 Q), p. 163 (Dist 84 x 66 px =
 * 336 x 264 Q), p. 164 (Comp 83 x 94 px = 332 x 376 Q). Common header:
 * on/off lamp left, title, input meter right. Value displays carry
 * up/down arrow buttons to their right. */
#define FX_HEAD(sec) \
    { MX(sec, 0), RI_GEO_RECT, 0, 50, 45, 30, 30 }, \
    { MX(sec, 1), RI_GEO_RECT, 0, 288, 45, 20, 45 }
static const struct RIGeoItem RI_GEO_PCF[] = {
    FX_HEAD(RI_SEC_PCF),
    { MX(RI_SEC_PCF, 2), RI_GEO_RECT, 0, 79, 122, 78, 65 },
    { MX(RI_SEC_PCF, 2), RI_GEO_STEPPER, 1, 141, 105, 38, 30 },
    { MX(RI_SEC_PCF, 2), RI_GEO_STEPPER, 0, 141, 139, 38, 30 },
    { MX(RI_SEC_PCF, 2), RI_GEO_LEGEND, 0, 100, 176, 0, 0 },
    { MX(RI_SEC_PCF, 3), RI_GEO_RECT, 0, 219, 124, 28, 62 },
    { MX(RI_SEC_PCF, 3), RI_GEO_LEGEND, 0, 255, 176, 0, 0 },
    { MX(RI_SEC_PCF, 4), RI_GEO_RECT, 0, 50, 294, 58, 158 },
    { MX(RI_SEC_PCF, 4), RI_GEO_LEGEND, 0, 50, 400, 0, 0 },
    { MX(RI_SEC_PCF, 5), RI_GEO_RECT, 0, 126, 294, 58, 158 },
    { MX(RI_SEC_PCF, 5), RI_GEO_LEGEND, 0, 126, 400, 0, 0 },
    { MX(RI_SEC_PCF, 6), RI_GEO_RECT, 0, 202, 294, 58, 158 },
    { MX(RI_SEC_PCF, 6), RI_GEO_LEGEND, 0, 202, 400, 0, 0 },
    { MX(RI_SEC_PCF, 7), RI_GEO_RECT, 0, 278, 294, 58, 158 },
    { MX(RI_SEC_PCF, 7), RI_GEO_LEGEND, 0, 278, 400, 0, 0 },
};
static const struct RIGeoItem RI_GEO_DELAY[] = {
    FX_HEAD(RI_SEC_DELAY),
    { MX(RI_SEC_DELAY, 2), RI_GEO_RECT, 0, 76, 122, 72, 65 },
    { MX(RI_SEC_DELAY, 2), RI_GEO_STEPPER, 1, 141, 105, 38, 30 },
    { MX(RI_SEC_DELAY, 2), RI_GEO_STEPPER, 0, 141, 139, 38, 30 },
    { MX(RI_SEC_DELAY, 2), RI_GEO_LEGEND, 0, 82, 178, 0, 0 },
    { MX(RI_SEC_DELAY, 3), RI_GEO_RECT, 0, 219, 122, 28, 65 },
    { MX(RI_SEC_DELAY, 4), RI_GEO_KNOB, 0, 88, 270, 48, 80 },
    { MX(RI_SEC_DELAY, 4), RI_GEO_LEGEND, 0, 88, 348, 0, 0 },
    { MX(RI_SEC_DELAY, 5), RI_GEO_KNOB, 0, 248, 270, 48, 80 },
    { MX(RI_SEC_DELAY, 5), RI_GEO_LEGEND, 0, 250, 348, 0, 0 },
};
static const struct RIGeoItem RI_GEO_DIST[] = {
    { MX(RI_SEC_DIST, 0), RI_GEO_RECT, 0, 50, 42, 30, 30 },
    { MX(RI_SEC_DIST, 1), RI_GEO_RECT, 0, 288, 42, 20, 45 },
    { MX(RI_SEC_DIST, 2), RI_GEO_KNOB, 0, 88, 150, 48, 80 },
    { MX(RI_SEC_DIST, 2), RI_GEO_LEGEND, 0, 88, 230, 0, 0 },
    { MX(RI_SEC_DIST, 3), RI_GEO_KNOB, 0, 250, 150, 48, 80 },
    { MX(RI_SEC_DIST, 3), RI_GEO_LEGEND, 0, 250, 230, 0, 0 },
};
static const struct RIGeoItem RI_GEO_COMP[] = {
    FX_HEAD(RI_SEC_COMP),
    { MX(RI_SEC_COMP, 4), RI_GEO_RECT, 0, 160, 119, 250, 20 },
    { MX(RI_SEC_COMP, 2), RI_GEO_KNOB, 0, 88, 265, 48, 80 },
    { MX(RI_SEC_COMP, 2), RI_GEO_LEGEND, 0, 88, 342, 0, 0 },
    { MX(RI_SEC_COMP, 3), RI_GEO_KNOB, 0, 248, 265, 48, 80 },
    { MX(RI_SEC_COMP, 3), RI_GEO_LEGEND, 0, 248, 342, 0, 0 },
};

/* Pattern section: Owner's Manual p. 147 figure (71 x 116 px = 284 x 464 Q),
 * identical for all four sections. Header: on/off lamp + "PATTERN";
 * Pattern buttons 1-4 / 5-8, "BANK", Bank buttons A-D, Shuffle button and
 * the Steps display with arrows. */
#define RI_GEO_PATSEC(sec) \
    { MX(sec, 0), RI_GEO_RECT, 0, 42, 45, 30, 30 }, \
    { MX(sec, 2), RI_GEO_OPTION, 0, 50, 122, 55, 55 }, \
    { MX(sec, 2), RI_GEO_OPTION, 1, 110, 122, 55, 55 }, \
    { MX(sec, 2), RI_GEO_OPTION, 2, 170, 122, 55, 55 }, \
    { MX(sec, 2), RI_GEO_OPTION, 3, 230, 122, 55, 55 }, \
    { MX(sec, 2), RI_GEO_OPTION, 4, 50, 182, 55, 55 }, \
    { MX(sec, 2), RI_GEO_OPTION, 5, 110, 182, 55, 55 }, \
    { MX(sec, 2), RI_GEO_OPTION, 6, 170, 182, 55, 55 }, \
    { MX(sec, 2), RI_GEO_OPTION, 7, 230, 182, 55, 55 }, \
    { MX(sec, 1), RI_GEO_LEGEND, 0, 60, 240, 0, 0 }, \
    { MX(sec, 1), RI_GEO_OPTION, 0, 50, 288, 55, 55 }, \
    { MX(sec, 1), RI_GEO_OPTION, 1, 110, 288, 55, 55 }, \
    { MX(sec, 1), RI_GEO_OPTION, 2, 170, 288, 55, 55 }, \
    { MX(sec, 1), RI_GEO_OPTION, 3, 230, 288, 55, 55 }, \
    { MX(sec, 4), RI_GEO_RECT, 0, 50, 408, 55, 55 }, \
    { MX(sec, 4), RI_GEO_LEGEND, 0, 70, 360, 0, 0 }, \
    { MX(sec, 3), RI_GEO_RECT, 0, 180, 411, 70, 58 }, \
    { MX(sec, 3), RI_GEO_STEPPER, 1, 244, 395, 32, 28 }, \
    { MX(sec, 3), RI_GEO_STEPPER, 0, 244, 430, 32, 28 }, \
    { MX(sec, 3), RI_GEO_LEGEND, 0, 188, 360, 0, 0 }
static const struct RIGeoItem RI_GEO_PAT1[] = { RI_GEO_PATSEC(RI_SEC_PAT_SYNTH1) };
static const struct RIGeoItem RI_GEO_PAT2[] = { RI_GEO_PATSEC(RI_SEC_PAT_SYNTH2) };
static const struct RIGeoItem RI_GEO_PAT8[] = { RI_GEO_PATSEC(RI_SEC_PAT_808) };
static const struct RIGeoItem RI_GEO_PAT9[] = { RI_GEO_PATSEC(RI_SEC_PAT_909) };

/* Transport panel: p. 144 figure (421 x 52 px = 1684 x 208 Q). Left to
 * right: Shuffle knob; Sync + MIDI LEDs over the Tempo display; the
 * Pattern/Song lever between its two LEDs; Play Stop Rewind FastForward
 * Record; Bar display; Loop lever + LED; Loop Start and Length displays. */
#define TR(i) MX(RI_SEC_TRANSPORT, i)
static const struct RIGeoItem RI_GEO_TRANSPORT[] = {
    { TR(2), RI_GEO_KNOB, 0, 80, 95, 50, 90 },
    { TR(2), RI_GEO_LEGEND, 0, 80, 178, 0, 0 },
    { TR(13), RI_GEO_RECT, 0, 188, 40, 12, 12 },
    { TR(12), RI_GEO_RECT, 0, 318, 40, 12, 12 },
    { TR(1), RI_GEO_RECT, 0, 232, 142, 115, 65 },
    { TR(1), RI_GEO_STEPPER, 1, 312, 127, 32, 28 },
    { TR(1), RI_GEO_STEPPER, 0, 312, 158, 32, 28 },
    { TR(1), RI_GEO_LEGEND, 0, 230, 88, 0, 0 },
    { TR(0), RI_GEO_LED, 0, 655, 45, 10, 10 },
    { TR(0), RI_GEO_RECT, 0, 690, 45, 24, 40 },
    { TR(0), RI_GEO_LED, 0, 725, 45, 10, 10 },
    { TR(4), RI_GEO_RECT, 0, 450, 137, 120, 76 },
    { TR(5), RI_GEO_RECT, 0, 578, 137, 120, 76 },
    { TR(6), RI_GEO_RECT, 0, 706, 137, 120, 76 },
    { TR(7), RI_GEO_RECT, 0, 834, 137, 120, 76 },
    { TR(8), RI_GEO_RECT, 0, 962, 137, 120, 76 },
    { TR(3), RI_GEO_RECT, 0, 1139, 143, 110, 70 },
    { TR(3), RI_GEO_STEPPER, 1, 1222, 127, 32, 28 },
    { TR(3), RI_GEO_STEPPER, 0, 1222, 158, 32, 28 },
    { TR(3), RI_GEO_LEGEND, 0, 1134, 88, 0, 0 },
    { TR(9), RI_GEO_RECT, 0, 1274, 45, 24, 40 },
    { TR(9), RI_GEO_LED, 0, 1300, 45, 10, 10 },
    { TR(10), RI_GEO_RECT, 0, 1369, 143, 110, 70 },
    { TR(10), RI_GEO_STEPPER, 1, 1452, 127, 32, 28 },
    { TR(10), RI_GEO_STEPPER, 0, 1452, 158, 32, 28 },
    { TR(10), RI_GEO_LEGEND, 0, 1364, 88, 0, 0 },
    { TR(11), RI_GEO_RECT, 0, 1544, 143, 110, 70 },
    { TR(11), RI_GEO_STEPPER, 1, 1629, 127, 32, 28 },
    { TR(11), RI_GEO_STEPPER, 0, 1629, 158, 32, 28 },
    { TR(11), RI_GEO_LEGEND, 0, 1544, 88, 0, 0 },
};

static const struct RIGeoSection RI_GEO_SECTIONS[] = {
    { RI_SEC_SYNTH1, 0, 1464, 460, RI_GEO_303,
      (uint32_t)(sizeof(RI_GEO_303) / sizeof(RI_GEO_303[0])) },
    { RI_SEC_808, 0, 1472, 468, RI_GEO_808,
      (uint32_t)(sizeof(RI_GEO_808) / sizeof(RI_GEO_808[0])) },
    { RI_SEC_909, 0, 1460, 468, RI_GEO_909,
      (uint32_t)(sizeof(RI_GEO_909) / sizeof(RI_GEO_909[0])) },
    { RI_SEC_MIX_SYNTH1, 0, 284, 464, RI_GEO_MIX1, (uint32_t)(sizeof(RI_GEO_MIX1) / sizeof(RI_GEO_MIX1[0])) },
    { RI_SEC_MIX_SYNTH2, 0, 284, 464, RI_GEO_MIX2, (uint32_t)(sizeof(RI_GEO_MIX2) / sizeof(RI_GEO_MIX2[0])) },
    { RI_SEC_MIX_808, 0, 284, 464, RI_GEO_MIX8, (uint32_t)(sizeof(RI_GEO_MIX8) / sizeof(RI_GEO_MIX8[0])) },
    { RI_SEC_MIX_909, 0, 284, 464, RI_GEO_MIX9, (uint32_t)(sizeof(RI_GEO_MIX9) / sizeof(RI_GEO_MIX9[0])) },
    { RI_SEC_MASTER, 0, 332, 392, RI_GEO_MASTER, (uint32_t)(sizeof(RI_GEO_MASTER) / sizeof(RI_GEO_MASTER[0])) },
    { RI_SEC_PCF, 0, 332, 424, RI_GEO_PCF, (uint32_t)(sizeof(RI_GEO_PCF) / sizeof(RI_GEO_PCF[0])) },
    { RI_SEC_DELAY, 0, 332, 376, RI_GEO_DELAY, (uint32_t)(sizeof(RI_GEO_DELAY) / sizeof(RI_GEO_DELAY[0])) },
    { RI_SEC_DIST, 0, 336, 264, RI_GEO_DIST, (uint32_t)(sizeof(RI_GEO_DIST) / sizeof(RI_GEO_DIST[0])) },
    { RI_SEC_COMP, 0, 332, 376, RI_GEO_COMP, (uint32_t)(sizeof(RI_GEO_COMP) / sizeof(RI_GEO_COMP[0])) },
    { RI_SEC_TRANSPORT, 0, 1684, 208, RI_GEO_TRANSPORT,
      (uint32_t)(sizeof(RI_GEO_TRANSPORT) / sizeof(RI_GEO_TRANSPORT[0])) },
    { RI_SEC_PAT_SYNTH1, 0, 284, 464, RI_GEO_PAT1, (uint32_t)(sizeof(RI_GEO_PAT1) / sizeof(RI_GEO_PAT1[0])) },
    { RI_SEC_PAT_SYNTH2, 0, 284, 464, RI_GEO_PAT2, (uint32_t)(sizeof(RI_GEO_PAT2) / sizeof(RI_GEO_PAT2[0])) },
    { RI_SEC_PAT_808, 0, 284, 464, RI_GEO_PAT8, (uint32_t)(sizeof(RI_GEO_PAT8) / sizeof(RI_GEO_PAT8[0])) },
    { RI_SEC_PAT_909, 0, 284, 464, RI_GEO_PAT9, (uint32_t)(sizeof(RI_GEO_PAT9) / sizeof(RI_GEO_PAT9[0])) },
};

const struct RIGeoSection *ri_geo_section(uint32_t section) {
    uint32_t i;
    for (i = 0; i < sizeof(RI_GEO_SECTIONS) / sizeof(RI_GEO_SECTIONS[0]); i++)
        if (RI_GEO_SECTIONS[i].section == section)
            return &RI_GEO_SECTIONS[i];
    return 0;
}

/* zoom factor as a fraction: 1x = 2/2, 1.5x = 3/2, 2x = 4/2 */
/* zoom factor in quarters: 1x = 4, 1.5x = 6, 2x = 8, compact 0.75x = 3 */
static int zoom_num(int zoom) {
    return zoom == 0 ? 4 : zoom == 1 ? 6 : zoom == 2 ? 8 : zoom == RI_GEO_ZOOM_COMPACT ? 3 : 0;
}

int ri_geo_px(int q, int zoom) {
    long num = (long)q * RI_GEO_BASE_SCALE_NUM * zoom_num(zoom);
    long den = 4L * RI_GEO_BASE_SCALE_DEN * 4L;
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
        } else if (it->shape == RI_GEO_RECT || it->shape == RI_GEO_OPTION || it->shape == RI_GEO_STEPPER) {
            int hw = ri_geo_px(it->w, zoom) / 2, hh = ri_geo_px(it->h, zoom) / 2;
            if (x >= cx - hw && x <= cx + hw && y >= cy - hh && y <= cy + hh) {
                if (opt && it->shape == RI_GEO_OPTION)
                    *opt = it->opt;
                else if (opt && it->shape == RI_GEO_STEPPER)
                    *opt = it->opt ? RI_GEO_HIT_UP : RI_GEO_HIT_DOWN;
                return it->reg_id;
            }
        }
    }
    return 0xFFFFu;
}

uint16_t ri_geo_hit(const struct RIGeoSection *s, int x, int y, int zoom) {
    return ri_geo_hit_opt(s, x, y, zoom, 0);
}

/* E0 position: the p. 147 figure ends at the Pattern selectors; the p. 22
 * figure (not measured) shows the bar just right of them. Placed in the
 * right margin of the section, full height of the selector column. */
int ri_geo_focus_bar(uint32_t section, struct RIGeoItem *out) {
    if (!out || section < RI_SEC_PAT_SYNTH1 || section > RI_SEC_PAT_909)
        return 2;
    out->reg_id = 0;
    out->shape = RI_GEO_RECT;
    out->opt = 0;
    out->cx = 272;
    out->cy = 232;
    out->w = 10;
    out->h = 420;
    return 0;
}
