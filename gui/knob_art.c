/* gui/knob_art.c — procedural 909-style knob frames (Module 2.9 art).
 * No AROS/MUI includes, no libm, no allocation: host + AROS safe.
 * Integer-only rendering (Q15 sine table, squared-distance tests).
 * Geometry measured from reference photos (see knob_art.h); the
 * PIL exploration recipe is the visual guide, this file is canonical.
 */
#include "gui/knob_art.h"
#include "gui/knob_logic.h"

const int16_t RI_SIN_Q15[360] = {
         0,    572,   1144,   1715,   2286,   2856,   3425,   3993,   4560,   5126,   5690,   6252,
      6813,   7371,   7927,   8481,   9032,   9580,  10126,  10668,  11207,  11743,  12275,  12803,
     13328,  13848,  14364,  14876,  15383,  15886,  16383,  16876,  17364,  17846,  18323,  18794,
     19260,  19720,  20173,  20621,  21062,  21497,  21925,  22347,  22762,  23170,  23571,  23964,
     24351,  24730,  25101,  25465,  25821,  26169,  26509,  26841,  27165,  27481,  27788,  28087,
     28377,  28659,  28932,  29196,  29451,  29697,  29934,  30162,  30381,  30591,  30791,  30982,
     31163,  31335,  31498,  31650,  31794,  31927,  32051,  32165,  32269,  32364,  32448,  32523,
     32587,  32642,  32687,  32722,  32747,  32762,  32767,  32762,  32747,  32722,  32687,  32642,
     32587,  32523,  32448,  32364,  32269,  32165,  32051,  31927,  31794,  31650,  31498,  31335,
     31163,  30982,  30791,  30591,  30381,  30162,  29934,  29697,  29451,  29196,  28932,  28659,
     28377,  28087,  27788,  27481,  27165,  26841,  26509,  26169,  25821,  25465,  25101,  24730,
     24351,  23964,  23571,  23170,  22762,  22347,  21925,  21497,  21062,  20621,  20173,  19720,
     19260,  18794,  18323,  17846,  17364,  16876,  16383,  15886,  15383,  14876,  14364,  13848,
     13328,  12803,  12275,  11743,  11207,  10668,  10126,   9580,   9032,   8481,   7927,   7371,
      6813,   6252,   5690,   5126,   4560,   3993,   3425,   2856,   2286,   1715,   1144,    572,
         0,   -572,  -1144,  -1715,  -2286,  -2856,  -3425,  -3993,  -4560,  -5126,  -5690,  -6252,
     -6813,  -7371,  -7927,  -8481,  -9032,  -9580, -10126, -10668, -11207, -11743, -12275, -12803,
    -13328, -13848, -14364, -14876, -15383, -15886, -16384, -16876, -17364, -17846, -18323, -18794,
    -19260, -19720, -20173, -20621, -21062, -21497, -21925, -22347, -22762, -23170, -23571, -23964,
    -24351, -24730, -25101, -25465, -25821, -26169, -26509, -26841, -27165, -27481, -27788, -28087,
    -28377, -28659, -28932, -29196, -29451, -29697, -29934, -30162, -30381, -30591, -30791, -30982,
    -31163, -31335, -31498, -31650, -31794, -31927, -32051, -32165, -32269, -32364, -32448, -32523,
    -32587, -32642, -32687, -32722, -32747, -32762, -32767, -32762, -32747, -32722, -32687, -32642,
    -32587, -32523, -32448, -32364, -32269, -32165, -32051, -31927, -31794, -31650, -31498, -31335,
    -31163, -30982, -30791, -30591, -30381, -30162, -29934, -29697, -29451, -29196, -28932, -28659,
    -28377, -28087, -27788, -27481, -27165, -27481, -26509, -26169, -25821, -25465, -25101, -24730,
    -24351, -23964, -23571, -23170, -22762, -22347, -21925, -21497, -21062, -20621, -20173, -19720,
    -19260, -18794, -18323, -17846, -17364, -16876, -16384, -15886, -15383, -14876, -14364, -13848,
    -13328, -12803, -12275, -11743, -11207, -10668, -10126,  -9580,  -9032,  -8481,  -7927,  -7371,
     -6813,  -6252,  -5690,  -5126,  -4560,  -3993,  -3425,  -2856,  -2286,  -1715,  -1144,   -572
};

/* Sign-correct round-half-away of p/32767. */
static int32_t qround(int32_t p) {
    if (p >= 0)
        return (p + 16383) / 32767;
    return -((-p + 16383) / 32767);
}

static void put(unsigned char *rgba, int x, int y, uint32_t c, int a) {
    unsigned char *p = rgba + ((uint32_t)y * 80u + (uint32_t)x) * 4u;
    p[0] = (unsigned char)((c >> 16) & 0xffu);
    p[1] = (unsigned char)((c >> 8) & 0xffu);
    p[2] = (unsigned char)(c & 0xffu);
    p[3] = (unsigned char)a;
}

/* Body radius 26 at center 39.5 (doubled coords below); tick ring
 * r 29..35; shadow offset (+2,+3). All thresholds x4 (squared
 * doubled coords): R26 -> 2704, feather edge 24.5 -> 2401,
 * tick band (29,35] -> (3364, 4900]. */
#define RI_C2 79 /* 2 * 39.5: doubled coords of the frame center.
 * ODD is correct (39.5 has no even double); 78 shifts every
 * boundary half a pixel and mismatches the literal-40 geometry. */
#define RI_R2 2704
#define RI_FEATHER2 2401
#define RI_TICK_LO2 3364
#define RI_TICK_HI2 4900
#define RI_SH_DX 4
#define RI_SH_DY 6

void ri_knob_render_frame(unsigned char *rgba, int value) {
    int deg, x, y, t, ch;
    int32_t sx, cx;
    int top[3], bot[3];
    if (!rgba)
        return;
    if (value < 0)
        value = 0;
    if (value > 127)
        value = 127;
    deg = ri_knob_pointer_mdeg(value) / 1000;
    deg = (deg + 360) % 360;
    sx = RI_SIN_Q15[deg];
    cx = RI_SIN_Q15[(deg + 90) % 360];
    top[0] = (RI_KNOB_BODY_TOP >> 16) & 0xff;
    top[1] = (RI_KNOB_BODY_TOP >> 8) & 0xff;
    top[2] = RI_KNOB_BODY_TOP & 0xff;
    bot[0] = (RI_KNOB_BODY_BOT >> 16) & 0xff;
    bot[1] = (RI_KNOB_BODY_BOT >> 8) & 0xff;
    bot[2] = RI_KNOB_BODY_BOT & 0xff;
    for (y = 0; y < 80; y++) {
        for (x = 0; x < 80; x++) {
            int32_t dx = 2 * x - RI_C2;
            int32_t dy = 2 * y - RI_C2;
            int32_t d2 = dx * dx + dy * dy;
            int32_t sdx = dx - RI_SH_DX;
            int32_t sdy = dy - RI_SH_DY;
            int32_t sh2 = sdx * sdx + sdy * sdy;
            unsigned char *p = rgba + ((uint32_t)y * 80u + (uint32_t)x) * 4u;
            if (d2 > RI_R2) {
                if (sh2 <= RI_R2) {
                    p[0] = p[1] = p[2] = 0;
                    p[3] = RI_KNOB_SHADOW_ALPHA;
                } else {
                    p[0] = p[1] = p[2] = p[3] = 0;
                }
                continue;
            }
            for (ch = 0; ch < 3; ch++) {
                int c = top[ch] + (bot[ch] - top[ch]) * y / 79;
                p[ch] = (unsigned char)c;
            }
            p[3] = 255;
            if (d2 > RI_FEATHER2) {
                put(rgba, x, y, RI_KNOB_RIM, 255);
            } else if (d2 > 1849 && dy < 0 && 4 * dy * dy > d2) {
                for (ch = 0; ch < 3; ch++) {
                    int c = (int)p[ch] + 45;
                    if (c > 255)
                        c = 255;
                    p[ch] = (unsigned char)c;
                }
            } else {
                if (dx < 0 && dy < 0) {
                    int add = (1400 * (2704 - (int)d2) / 2704) / 100;
                    for (ch = 0; ch < 3; ch++) {
                        int c = (int)p[ch] + add;
                        if (c > 255)
                            c = 255;
                        p[ch] = (unsigned char)c;
                    }
                }
            }
        }
    }
    /* Ticks: 11 dashes at -135..+135 step 27 (bottom gap), r 29..35. */
    for (t = -135; t <= 135; t += 27) {
        int32_t td = (t + 360) % 360;
        int32_t tsx = RI_SIN_Q15[td];
        int32_t tcx = RI_SIN_Q15[(td + 90) % 360];
        int r, w;
        for (r = 29; r <= 35; r++) {
            int32_t bx = 40 + qround((int32_t)r * tsx);
            int32_t by = 40 - qround((int32_t)r * tcx);
            for (w = -1; w <= 1; w++) {
                int32_t px = bx + qround((int32_t)w * -tcx);
                int32_t py = by + qround((int32_t)w * tsx);
                if (px < 0 || px > 79 || py < 0 || py > 79)
                    continue;
                put(rgba, (int)px, (int)py, RI_KNOB_TICK, 255);
            }
        }
    }
    /* Pointer: r 15..24, 5 px wide, over ticks/body (never hub:
     * there is none anymore). */
    {
        int r, w;
        for (r = 15; r <= 24; r++) {
            for (w = -2; w <= 2; w++) {
                int32_t px = -cx, py = sx;
                int32_t X = 40 + qround((int32_t)r * sx + (int32_t)w * px);
                int32_t Y = 40 - qround((int32_t)r * cx + (int32_t)w * py);
                if (X < 0 || X > 79 || Y < 0 || Y > 79)
                    continue;
                put(rgba, (int)X, (int)Y, RI_KNOB_POINTER, 255);
            }
        }
    }
}
