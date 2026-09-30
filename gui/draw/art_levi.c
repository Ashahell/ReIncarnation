/* art_levi.c — Levi voice canvas: hardware top panel (fidelity plan P1,
 * owner 2026-09-30: "look as close to the hardware as possible", house
 * font in a contrasting colour).
 *
 * Arrangement and function follow the Owner's Manual top panel; every
 * pixel is our own (clean-room): graphite panel, black-bordered section
 * boxes with black title bands and teal titles, black button caps with
 * teal legends, aluminium knobs, encoders with a white LED-dot ring, a
 * dark LCD page, a 2-digit algorithm readout, the ribbon and a keybed.
 * No ASM logo, wordmark or trade mark: the mark slot reads "LEVI".
 * Hardware controls whose engine lands in a later phase are drawn dim
 * and are not in the geometry, so they cannot be hit.
 * Positions: generated from the reference measurements (see RI_GEO_LEVI
 * in gui/panelgeo.c), xQ = (x-75)*0.6245, yQ = (y-48)*0.6245.
 */
#include "gui/draw/art.h"

#include <string.h>
#include "engine/dsp/kernels.h"
#include "gui/ctlreg.h"
#include "gui/draw/font_legend.h"
#include "gui/knob_logic.h"
#include "gui/panelgeo.h"
#include "gui/sectlevi.h"
#include "gui/sectui.h"

struct LeviBox { int x0, y0, x1, y1, tx; const char *title; };
struct LeviKnob { int x, y, style; };      /* 0 plain, 1 ring, 2 bipolar, 3 big encoder, 4 bipolar legend only */
struct LeviText { int x, y; const char *full, *brief; int col, maxw; };
struct LeviCap { int x, y, w; const char *l1, *l2; int col; };

/* Generated from the reference measurements (see RI_GEO_LEVI). */
static const struct LeviBox BOX[] = {
    /* CV / Gate block dropped (owner 2026-09-30: no jacks in a soft synth);
     * the arp & seq block takes the column. */
    { 189, 20, 475, 295, 332, "ARPEGGIATOR & SEQUENCER CONTROL" },
    { 485, 20, 662, 295, 574, "MAIN SYSTEMS" },
    { 671, 20, 1004, 295, 838, "MASTER CONTROL" },
    { 1015, 20, 1744, 106, 1380, "" },
    { 1015, 117, 1091, 296, 1053, "ALGORITHM" },
    { 1102, 117, 1744, 296, 1423, "MODULE SELECT" },
};
static const struct LeviKnob DKNOB[] = {
    { 62, 137, 0 },
    { 126, 136, 1 },
    { 274, 164, 0 },
    { 332, 164, 0 },
    { 390, 164, 0 },
    { 447, 164, 0 },
    { 1463, 64, 4 },       /* ENV 1 / ENV 2: live knobs since P5, legend only here */
    { 1706, 64, 4 },
    { 606, 134, 3 },
};
static const struct LeviText KLABEL[] = {
    { 1108, 91, "ATTACK", "ATK", 1, 54 },
    { 1164, 91, "DECAY", "DEC", 1, 54 },
    { 1220, 91, "RELEASE", "REL", 1, 54 },
    { 1296, 91, "CUTOFF", "CUT", 1, 54 },
    { 1351, 91, "RESONANCE", "RES", 1, 54 },
    { 1539, 91, "CUTOFF", "CUT", 1, 54 },
    { 1595, 91, "RESONANCE", "RES", 1, 54 },
    { 1650, 91, "PRE-DRIVE", "DRV", 1, 54 },
    { 62, 169, "MASTER", "VOL", 1, 54 },
    { 126, 172, "BALANCE", "BAL", 1, 54 },
    { 274, 189, "MODE", "MODE", 1, 54 },
    { 332, 189, "OCTAVE", "OCT", 1, 54 },
    { 390, 189, "GATE", "GATE", 1, 54 },
    { 447, 189, "ENTROPY", "ENTR", 1, 54 },
    { 1054, 91, "ENV LEVEL", "ENV", 1, 54 },
    { 1407, 91, "DRIVE / MORPH", "DRV", 1, 54 },
    { 1463, 91, "ENV 1", "ENV1", 1, 54 },
    { 1706, 91, "ENV 2", "ENV2", 1, 54 },
};
static const struct LeviCap DCAP[] = {
    { 62, 208, 29, "SINGLE", "", 1 },
    { 126, 208, 29, "MULTI", "", 1 },
    { 62, 247, 29, "LOWER", "", 0 },
    { 126, 247, 29, "UPPER", "", 0 },
    { 62, 290, 29, "GLIDE", "", 1 },
    { 127, 290, 29, "RIBBON", "", 0 },
    { 52, 320, 29, "DOWN", "", 1 },
    { 94, 320, 29, "UP", "", 1 },
    { 136, 320, 29, "CHORD", "", 1 },
    { 262, 218, 29, "ARP", "LATCH", 1 },
    { 308, 218, 29, "TRK", "PARAMS", 0 },
    { 355, 218, 29, "TRK 1", "EDIT", 0 },
    { 401, 218, 29, "TRK 2", "EDIT", 0 },
    { 447, 218, 29, "TRK", "EDIT", 0 },
    { 216, 264, 29, "TAP", "TEMPO", 1 },
    { 262, 264, 29, "SEQ", "", 2 },
    { 355, 264, 29, "TRK 1", "ON", 2 },
    { 401, 264, 29, "TRK 2", "ON", 1 },
    { 447, 264, 29, "TRK", "ON", 1 },
    { 520, 51, 29, "MIDI", "", 0 },
    { 520, 88, 29, "SYSTEM", "", 0 },
    { 520, 124, 29, "SAVE", "", 0 },
    { 520, 190, 29, "INIT", "", 1 },
    { 520, 227, 29, "RANDOM", "", 1 },
    { 520, 264, 29, "SHIFT", "", 1 },
    { 584, 51, 29, "BROWSE", "", 0 },
    { 629, 51, 29, "FAVORITE", "", 0 },
    { 584, 220, 29, "<", "", 1 },
    { 629, 220, 29, ">", "", 1 },
    { 606, 264, 74, "HOME", "", 1 },
    { 968, 61, 29, "EXIT", "", 1 },
};
static const struct LeviText DTEXT[] = {
    { 126, 224, "EDIT", "EDIT", 0, 44 },
    { 94, 247, "BOTH", "BOTH", 1, 44 },
    { 73, 340, "OCTAVE", "OCT", 1, 44 },
    { 136, 337, "EDIT", "EDIT", 0, 44 },
    { 216, 237, "EDIT", "EDIT", 0, 44 },
    { 262, 237, "SUSTAIN", "SUS", 0, 44 },
    { 308, 237, "SETTINGS", "SET", 0, 44 },
    { 216, 280, "METRONOME", "METRO", 0, 44 },
    { 262, 280, "STEP RECORD", "STEP", 0, 44 },
    { 401, 245, "SEQ REC + TRK = ARM", "REC+TRK=ARM", 1, 150 },
    { 328, 296, "RIBBON SEQ STEP MODE", "RIBBON STEPS", 1, 150 },
    { 584, 236, "-10", "-10", 0, 44 },
    { 629, 236, "+10", "+10", 0, 44 },
    { 606, 280, "PANIC", "PANIC", 0, 44 },
    { 968, 185, "PAGE", "PAGE", 1, 44 },
    { 1137, 27, "OSC ENV LEVEL & BIAS", "OSC ENV & BIAS", 0, 300 },
    { 1380, 27, "DIGITAL FILTER", "DIGITAL FILT", 0, 300 },
    { 1624, 27, "ANALOG FILTER", "ANALOG FILT", 0, 300 },
    { 1254, 160, "OSCILLATOR GROUP EDIT", "OSC GROUP EDIT", 1, 300 },
    { 1300, 242, "INDIVIDUAL OSCILLATOR SETTINGS", "OSC SETTINGS", 1, 300 },
    { 1300, 280, "PAGE RECALL", "RECALL", 0, 100 },
};
#define LEVI_RIBBON_X0 190
#define LEVI_RIBBON_Y0 305
#define LEVI_RIBBON_X1 1427
#define LEVI_RIBBON_Y1 335
/* big encoder (main systems) */
#define LEVI_BIGENC_X 606
#define LEVI_BIGENC_Y 134
#define LEVI_BIGENC_R 31
#define LEVI_MARK_X 1622
#define LEVI_MARK_Y 240
#define LEVI_GROUP_X0 1112
#define LEVI_GROUP_Y0 148
#define LEVI_GROUP_X1 1396
#define LEVI_GROUP_Y1 223
#define LEVI_DIV1_X 1258
#define LEVI_DIV2_X 1502
#define LEVI_DIV_Y0 39
#define LEVI_DIV_Y1 98
#define LEVI_CHAIN_Y_ENV 156
#define LEVI_CHAIN_Y_MID 182
#define LEVI_CHAIN_Y_LFO 209
#define LEVI_CHAIN_X0 1396
#define LEVI_CHAIN_X1 1730
#define LEVI_MSDIV_X 556
#define LEVI_MSDIV_Y0 39
#define LEVI_MSDIV_Y1 282
#define LEVI_PAGEBOX_X 968
#define LEVI_PAGEBOX_Y 162
#define LEVI_PAGEBOX_W 37
#define LEVI_PAGEBOX_H 64
#define LEVI_RECALL_Y 275

/* Keybed band (Q): wheels, STEP EDIT block, keys over x 440..1738. */
#define LEVI_KEY_X0 440
#define LEVI_KEY_W 59
#define LEVI_KEY_N 22
#define LEVI_KEY_Y0 355
#define LEVI_KEY_Y1 550
#define LEVI_BKEY_Y1 470

static int levi_face(const struct ri_dlist *dl, int face) {
    return face ? face : (dl ? dl->cur_face : 0);
}

static int levi_text_w(const struct ri_dlist *dl, const char *s, int face) {
    const struct ri_face *f = ri_face_by_id(levi_face(dl, face));
    return f ? ri_face_width(f, s) : (int)strlen(s) * 6;
}

/* Centred legend, any length (the art helpers stop at 23 characters);
 * face 0 = the section face for this zoom. Strings are upper case. */
static void levi_text(struct ri_dlist *dl, int cx, int cy, const char *t, int col, int face) {
    ri_draw_text_face(dl, cx, cy, 1u, ri_art_rgb(col), levi_face(dl, face), t);
}

/* Best fit for maxw px: the full legend in the requested face, then in
 * small print, then the brief form (requested face, then small print).
 * face 0 = the section face for this zoom. */
static void levi_label(struct ri_dlist *dl, int cx, int cy, const char *full, const char *brief,
    int maxw, int col, int face) {
    int f = levi_face(dl, face);
    const char *t = full;
    if (levi_text_w(dl, full, f) > maxw) {
        if (levi_text_w(dl, full, RI_FACE_S) <= maxw || !brief || !brief[0])
            f = RI_FACE_S;
        else {
            t = brief;
            if (levi_text_w(dl, brief, f) > maxw)
                f = RI_FACE_S;
        }
    }
    levi_text(dl, cx, cy, t, col, f);
}

/* Short cap words (hardware legends at our font size). */
static const char *levi_brief(const char *w) {
    static const struct { const char *w, *b; } T[] = {
        { "SINGLE", "SNGL" }, { "LOWER", "LOW" }, { "UPPER", "UPP" }, { "GLIDE", "GLD" },
        { "RIBBON", "RBN" }, { "CHORD", "CHD" }, { "LATCH", "LTCH" }, { "PARAMS", "PRM" },
        { "TEMPO", "TMP" }, { "SYSTEM", "SYS" }, { "RANDOM", "RND" }, { "SHIFT", "SHFT" },
        { "BROWSE", "BRWS" }, { "FAVORITE", "FAV" }, { "MACRO", "MAC" }, { "ASSIGN", "ASGN" },
        { "MATRIX", "MTX" }, { "VOICE", "VOIC" }, { "PITCH", "PTCH" }, { "FEEDBK", "FDBK" },
        { "LEVEL", "LVL" }, { "DELAY", "DLY" }, { "ATTACK", "ATK" }, { "DECAY", "DEC" },
        { "SUSTAIN", "SUS" }, { "RELEASE", "REL" }, { "DIGITAL", "DIG" }, { "ANALOG", "ANA" },
        { "FILTER", "FLT" }, { "REVERB", "REV" }, { "POST-FX", "POST" }, { "PRE-FX", "PRE" },
        { "EDIT", "ED" }, { "SEQ", "SEQ" }, { "TRK 1", "T1" }, { "TRK 2", "T2" },
    };
    uint32_t i;
    for (i = 0u; i < sizeof(T) / sizeof(T[0]); i++)
        if (!strcmp(T[i].w, w))
            return T[i].b;
    return w;
}

/* Black cap with a thin rim; legend lines in col, small-print face.
 * lit = backlit (teal rim, lighter face, white legend). */
static void levi_cap(struct ri_dlist *dl, int cx, int cy, int hw, int hh, const char *l1, const char *l2,
    int col, int lit) {
    uint32_t face = ri_art_rgb(C_LEVI_CAP);
    int maxw = 2 * hw - 2, lh;
    ri_art_rect(dl, cx - hw, cy - hh, cx + hw, cy + hh, lit ? C_LEVI_TEAL : C_LEVI_EDGE);
    ri_draw_rect(dl, cx - hw + 1, cy - hh + 1, cx + hw - 1, cy + hh - 1, lit ? ri_art_shade(face, 14) : face);
    ri_draw_line(dl, cx - hw + 2, cy - hh + 1, cx + hw - 2, cy - hh + 1, ri_art_shade(face, lit ? 30 : 18));
    if (lit)
        col = C_LEVI_LABEL;
    if (!l1 || !l1[0])
        return;
    if (l2 && l2[0]) {
        lh = hh / 2 > 1 ? hh / 2 : 1;
        levi_label(dl, cx, cy - lh + 1, l1, levi_brief(l1), maxw, col, RI_FACE_S);
        levi_label(dl, cx, cy + lh - 1, l2, levi_brief(l2), maxw, col, RI_FACE_S);
    } else {
        levi_label(dl, cx, cy, l1, levi_brief(l1), maxw, col, dl->cur_face > RI_FACE_S ? RI_FACE_M : RI_FACE_S);
    }
}

/* Seven-segment digit (white segments, dark unlit ghosts) in a box. */
static void levi_seg7(struct ri_dlist *dl, int x0, int y0, int x1, int y1, int digit) {
    /* 0-9, then 10 = C (custom algorithm), 11 = blank */
    static const uint8_t SEG[12] = { 0x3F, 0x06, 0x5B, 0x4F, 0x66, 0x6D, 0x7D, 0x07, 0x7F, 0x6F, 0x39, 0x00 };
    int w = x1 - x0, h = y1 - y0, t = w / 6 > 1 ? w / 6 : 1, m = (y0 + y1) / 2, k;
    uint8_t on = SEG[digit >= 0 && digit < 12 ? digit : digit % 10];
    for (k = 0; k < 7; k++) {
        uint32_t c = (on >> k) & 1u ? ri_art_rgb(C_LEVI_LABEL) : ri_art_shade(ri_art_rgb(C_LEVI_CAP), 10);
        switch (k) {
        case 0: ri_draw_rect(dl, x0 + t, y0, x1 - t, y0 + t - 1, c); break;
        case 1: ri_draw_rect(dl, x1 - t + 1, y0 + t, x1, m - 1, c); break;
        case 2: ri_draw_rect(dl, x1 - t + 1, m + 1, x1, y1 - t, c); break;
        case 3: ri_draw_rect(dl, x0 + t, y1 - t + 1, x1 - t, y1, c); break;
        case 4: ri_draw_rect(dl, x0, m + 1, x0 + t - 1, y1 - t, c); break;
        case 5: ri_draw_rect(dl, x0, y0 + t, x0 + t - 1, m - 1, c); break;
        default: ri_draw_rect(dl, x0 + t, m - t / 2, x1 - t, m - t / 2 + t - 1, c); break;
        }
    }
    (void)h;
}

static void levi_polar(int cx, int cy, float deg, float r, int *x, int *y) {
    float a = (deg - 90.0f) * 0.0174533f;
    *x = cx + (int)(r * ri_sin(a + 1.5707963f));
    *y = cy + (int)(r * ri_sin(a));
}

/* Encoder: black collar with a ring of LED dots (lit up to value), an
 * aluminium knob inside. dots < 0 draws the ring dark (dead slot). */
static void levi_encoder(struct ri_dlist *dl, int cx, int cy, int r, int value, int live, uint32_t pan) {
    int k, n = 15, lit = live ? (value * n + 63) / 127 : 0, dr = r / 12 > 1 ? r / 12 : 1;
    ri_art_disc_grad(dl, cx, cy, r + r / 3, ri_art_rgb(C_LEVI_CAP), ri_art_rgb(C_LEVI_CAP));
    for (k = 0; k < n; k++) {
        int x, y;
        levi_polar(cx, cy, -135.0f + 270.0f * (float)k / (float)(n - 1), (float)(r + r / 6), &x, &y);
        ri_art_disc_grad(dl, x, y, dr, ri_art_rgb(k < lit ? C_LEVI_LABEL : C_LEVI_EDGE),
            ri_art_rgb(k < lit ? C_LEVI_LABEL : C_LEVI_EDGE));
    }
    ri_art_knob(dl, cx, cy, 2 * r * 4 / 5, 2 * r * 4 / 5, live ? C_LEVI_SILVER : C_LEVI_DIM, C_BLACK, 0,
        -135.0f + 270.0f * (float)value / 127.0f, pan);
}

static void levi_box(struct ri_dlist *dl, int ox, int oy, int z, const struct LeviBox *b) {
#define PX(q) ri_geo_px((q), z)
    int x0 = ox + PX(b->x0), y0 = oy + PX(b->y0), x1 = ox + PX(b->x1), y1 = oy + PX(b->y1);
    int band = PX(14) > 3 ? PX(14) : 3, e = PX(2) > 1 ? PX(2) : 1;
    ri_art_rect(dl, x0, y0, x1, y1, C_LEVI_CAP);                     /* black frame */
    ri_art_rect(dl, x0 + e, y0 + (b->title[0] ? band : e), x1 - e, y1 - e, C_LEVI_PANEL);
    if (b->title[0])
        levi_text(dl, ox + PX(b->tx), y0 + band / 2 + 1, b->title, C_LEVI_TEAL, 0);
#undef PX
}

void ri_art_bg_levi(struct ri_dlist *dl, const struct RIGeoSection *g, int ox, int oy, int z) {
    uint32_t i;
    int k;
    uint32_t pan = ri_art_rgb(C_LEVI_PANEL);
#define PX(q) ri_geo_px((q), z)
    ri_art_rect(dl, ox, oy, ox + PX(g->w) - 1, oy + PX(g->h) - 1, C_LEVI_PANEL);
    for (i = 0u; i < sizeof(BOX) / sizeof(BOX[0]); i++)
        levi_box(dl, ox, oy, z, &BOX[i]);
    /* Dashed dividers: filter groups, main systems. */
    for (k = PX(LEVI_DIV_Y0); k < PX(LEVI_DIV_Y1); k += PX(6) > 2 ? PX(6) : 2) {
        ri_art_rect(dl, ox + PX(LEVI_DIV1_X), oy + k, ox + PX(LEVI_DIV1_X), oy + k + PX(2), C_LEVI_EDGE);
        ri_art_rect(dl, ox + PX(LEVI_DIV2_X), oy + k, ox + PX(LEVI_DIV2_X), oy + k + PX(2), C_LEVI_EDGE);
    }
    for (k = PX(LEVI_MSDIV_Y0); k < PX(LEVI_MSDIV_Y1); k += PX(6) > 2 ? PX(6) : 2)
        ri_art_rect(dl, ox + PX(LEVI_MSDIV_X), oy + k, ox + PX(LEVI_MSDIV_X), oy + k + PX(2), C_LEVI_EDGE);
    /* Oscillator group edit well + module chain rules (teal). */
    ri_art_rect(dl, ox + PX(LEVI_GROUP_X0), oy + PX(LEVI_GROUP_Y0), ox + PX(LEVI_GROUP_X1),
        oy + PX(LEVI_GROUP_Y1), C_LEVI_BOX);
    ri_art_rect(dl, ox + PX(LEVI_GROUP_X0 + 12), oy + PX(LEVI_GROUP_Y0 + 11), ox + PX(LEVI_GROUP_X0 + 72),
        oy + PX(LEVI_GROUP_Y0 + 12), C_LEVI_TEAL);
    ri_art_rect(dl, ox + PX(LEVI_GROUP_X1 - 72), oy + PX(LEVI_GROUP_Y0 + 11), ox + PX(LEVI_GROUP_X1 - 12),
        oy + PX(LEVI_GROUP_Y0 + 12), C_LEVI_TEAL);
    ri_art_rect(dl, ox + PX(LEVI_CHAIN_X0), oy + PX(LEVI_CHAIN_Y_MID), ox + PX(LEVI_CHAIN_X1),
        oy + PX(LEVI_CHAIN_Y_MID), C_LEVI_EDGE);
    ri_art_rect(dl, ox + PX(1470), oy + PX(LEVI_CHAIN_Y_ENV - 1), ox + PX(1672), oy + PX(LEVI_CHAIN_Y_ENV),
        C_LEVI_TEAL);
    ri_art_rect(dl, ox + PX(1470), oy + PX(LEVI_CHAIN_Y_LFO), ox + PX(1672), oy + PX(LEVI_CHAIN_Y_LFO + 1),
        C_LEVI_TEAL);
    ri_art_rect(dl, ox + PX(1137), oy + PX(LEVI_RECALL_Y + 4), ox + PX(1255), oy + PX(LEVI_RECALL_Y + 4), C_LEVI_TEAL);
    ri_art_rect(dl, ox + PX(1345), oy + PX(LEVI_RECALL_Y + 4), ox + PX(1462), oy + PX(LEVI_RECALL_Y + 4), C_LEVI_TEAL);
    /* Dim (later-phase) knobs, the main systems encoder. */
    for (i = 0u; i < sizeof(DKNOB) / sizeof(DKNOB[0]); i++) {
        int cx = ox + PX(DKNOB[i].x), cy = oy + PX(DKNOB[i].y);
        if (DKNOB[i].style == 3) {
            int r = PX(LEVI_BIGENC_R);
            ri_art_disc_grad(dl, cx, cy, r + PX(4), ri_art_rgb(C_LEVI_TEAL), ri_art_shade(ri_art_rgb(C_LEVI_TEAL), -25));
            ri_art_knob(dl, cx, cy, 2 * r, 2 * r, C_LEVI_DIM, C_LEVI_DIM, 0, 0.0f, pan);
        } else if (DKNOB[i].style == 1) {
            levi_encoder(dl, cx, cy, PX(15), 0, 0, pan);
        } else {
            if (DKNOB[i].style == 2 || DKNOB[i].style == 4) {  /* bipolar legend: 0 above, -/+ at the ends */
                levi_text(dl, cx, cy - PX(26), "0", C_LEVI_LABEL, RI_FACE_S);
                levi_text(dl, cx - PX(22), cy + PX(14), "-", C_LEVI_LABEL, RI_FACE_S);
                levi_text(dl, cx + PX(22), cy + PX(14), "+", C_LEVI_LABEL, RI_FACE_S);
            }
            if (DKNOB[i].style != 4)
                ri_art_knob(dl, cx, cy, PX(30), PX(38), C_LEVI_DIM, C_LEVI_EDGE, 0, 0.0f, pan);
        }
    }
    for (i = 0u; i < sizeof(KLABEL) / sizeof(KLABEL[0]); i++)
        levi_label(dl, ox + PX(KLABEL[i].x), oy + PX(KLABEL[i].y), KLABEL[i].full, KLABEL[i].brief,
            PX(KLABEL[i].maxw), C_LEVI_LABEL, 0);
    for (i = 0u; i < sizeof(DCAP) / sizeof(DCAP[0]); i++)
        levi_cap(dl, ox + PX(DCAP[i].x), oy + PX(DCAP[i].y), PX(DCAP[i].w) / 2, PX(10), DCAP[i].l1, DCAP[i].l2,
            DCAP[i].col == 0 ? C_LEVI_TEALDIM : DCAP[i].col == 2 ? C_LEVI_REDDIM : C_LEVI_LABELDIM, 0);
    for (i = 0u; i < sizeof(DTEXT) / sizeof(DTEXT[0]); i++)
        levi_label(dl, ox + PX(DTEXT[i].x), oy + PX(DTEXT[i].y), DTEXT[i].full, DTEXT[i].brief,
            PX(DTEXT[i].maxw), DTEXT[i].col ? C_LEVI_LABEL : C_LEVI_TEAL, DTEXT[i].maxw <= 44 ? RI_FACE_S : 0);
    /* PAGE up/down well; own mark (no maker logo or product wordmark). */
    ri_art_rect(dl, ox + PX(LEVI_PAGEBOX_X - LEVI_PAGEBOX_W / 2), oy + PX(LEVI_PAGEBOX_Y - LEVI_PAGEBOX_H / 2),
        ox + PX(LEVI_PAGEBOX_X + LEVI_PAGEBOX_W / 2), oy + PX(LEVI_PAGEBOX_Y + LEVI_PAGEBOX_H / 2), C_LEVI_CAP);
    levi_text(dl, ox + PX(LEVI_MARK_X), oy + PX(LEVI_MARK_Y), "LEVI", C_LEVI_TEAL, RI_FACE_L);
    levi_text(dl, ox + PX(LEVI_MARK_X), oy + PX(LEVI_MARK_Y + 22), "8 OPERATORS  2 FILTERS", C_LEVI_LABEL, 0);
    /* Ribbon strip. */
    ri_art_rect(dl, ox + PX(LEVI_RIBBON_X0), oy + PX(LEVI_RIBBON_Y0), ox + PX(LEVI_RIBBON_X1),
        oy + PX(LEVI_RIBBON_Y1), C_LEVI_CAP);
    ri_art_rect(dl, ox + PX(LEVI_RIBBON_X0 + 2), oy + PX(LEVI_RIBBON_Y0 + 2), ox + PX(LEVI_RIBBON_X1 - 2),
        oy + PX(LEVI_RIBBON_Y1 - 2), C_LEVI_BOX);
    /* Keybed band: cheek block, pitch/mod wheels, STEP EDIT block (our
     * own addition for the chord steps), then the keys. */
    ri_art_rect(dl, ox, oy + PX(346), ox + PX(g->w) - 1, oy + PX(g->h) - 1, C_LEVI_CAP);
    for (k = 0; k < 2; k++) {
        int wx = 50 + 50 * k;
        ri_art_rect(dl, ox + PX(wx - 14), oy + PX(380), ox + PX(wx + 14), oy + PX(530), C_LEVI_BOX);
        for (i = 0u; i < 9u; i++)
            ri_art_rect(dl, ox + PX(wx - 11), oy + PX(386 + 16 * (int)i), ox + PX(wx + 11),
                oy + PX(386 + 16 * (int)i + 6), k == 0 && i == 4u ? C_LEVI_TEAL : C_LEVI_EDGE);
    }
    levi_text(dl, ox + PX(50), oy + PX(542), "PITCH", C_LEVI_LABEL, RI_FACE_S);
    levi_text(dl, ox + PX(100), oy + PX(542), "MOD", C_LEVI_LABEL, RI_FACE_S);
    ri_art_rect(dl, ox + PX(160), oy + PX(356), ox + PX(420), oy + PX(548), C_LEVI_PANEL);
    levi_text(dl, ox + PX(290), oy + PX(372), "STEP EDIT", C_LEVI_TEAL, 0);
    levi_text(dl, ox + PX(250), oy + PX(388), "LANE", C_LEVI_LABEL, RI_FACE_S);
    levi_text(dl, ox + PX(368), oy + PX(462), "STEP", C_LEVI_LABEL, RI_FACE_S);
    for (k = 0; k < LEVI_KEY_N; k++) {
        int x0 = LEVI_KEY_X0 + LEVI_KEY_W * k;
        int active = k >= 7 && k <= 14;
        uint32_t wk = ri_art_rgb(C_WHITEKEY);
        if (active)
            continue;                                  /* live keys draw as controls */
        ri_draw_rect(dl, ox + PX(x0 + 1), oy + PX(LEVI_KEY_Y0), ox + PX(x0 + LEVI_KEY_W - 2), oy + PX(LEVI_KEY_Y1),
            wk);
        ri_draw_rect(dl, ox + PX(x0 + 1), oy + PX(LEVI_KEY_Y1 - 8), ox + PX(x0 + LEVI_KEY_W - 2),
            oy + PX(LEVI_KEY_Y1), ri_art_shade(wk, -36));
    }
    {
        static const int bk[7] = { 1, 1, 0, 1, 1, 1, 0 };   /* black key after white key n (per octave) */
        for (k = 0; k < LEVI_KEY_N - 1; k++) {
            int x = LEVI_KEY_X0 + LEVI_KEY_W * (k + 1);
            if (!bk[k % 7] || (k >= 7 && k <= 13))
                continue;
            ri_art_rect(dl, ox + PX(x - 18), oy + PX(LEVI_KEY_Y0), ox + PX(x + 18), oy + PX(LEVI_BKEY_Y1), C_BLACK);
            ri_draw_rect(dl, ox + PX(x - 14), oy + PX(LEVI_BKEY_Y1 - 12), ox + PX(x + 14), oy + PX(LEVI_BKEY_Y1 - 4),
                ri_art_shade(ri_art_rgb(C_BLACK), 22));
        }
    }
#undef PX
}

/* Module-select cap legends by module id. */
static void levi_mod_label(uint32_t m, const char **l1, const char **l2) {
    static const char *const G[12] = { "MODE", "WAVE", "PITCH", "FINE", "FEEDBK", "LEVEL",
        "DELAY", "ATTACK", "HOLD", "DECAY", "SUSTAIN", "RELEASE" };
    static const char *const C[7][2] = { { "DIGITAL", "FILTER" }, { "ANALOG", "FILTER" }, { "VCA", "" },
        { "PRE-FX", "" }, { "DELAY", "" }, { "REVERB", "" }, { "POST-FX", "" } };
    static const char *const N[5] = { "1", "2", "3", "4", "5" };
    *l2 = "";
    if (m >= RI_SLEVI_M_GMODE && m <= RI_SLEVI_M_GRELEASE)
        *l1 = G[m - RI_SLEVI_M_GMODE];
    else if (m >= RI_SLEVI_M_ENV1 && m < RI_SLEVI_M_ENV1 + 5u) {
        *l1 = "ENV";
        *l2 = N[m - RI_SLEVI_M_ENV1];
    } else if (m >= RI_SLEVI_M_LFO1 && m < RI_SLEVI_M_LFO1 + 5u) {
        *l1 = "LFO";
        *l2 = N[m - RI_SLEVI_M_LFO1];
    } else if (m >= RI_SLEVI_M_DFILT && m <= RI_SLEVI_M_POSTFX) {
        *l1 = C[m - RI_SLEVI_M_DFILT][0];
        *l2 = C[m - RI_SLEVI_M_DFILT][1];
    } else if (m == RI_SLEVI_M_ALGO) {
        *l1 = "ALGO";
        *l2 = "EDIT";
    } else if (m == RI_SLEVI_M_ARP)
        *l1 = "ARP";
    else if (m == RI_SLEVI_M_SEQ)
        *l1 = "SEQ";
    else if (m == RI_SLEVI_M_MATRIX) {
        *l1 = "MOD";
        *l2 = "MATRIX";
    } else if (m == RI_SLEVI_M_VOICE)
        *l1 = "VOICE";
    else if (m == RI_SLEVI_M_MACRO) {
        *l1 = "MACRO";
        *l2 = "ASSIGN";
    }
    else
        *l1 = "";
}

/* The page on the LCD: title, then 4 slots over 4 slots (the encoders
 * above and below the display), name over value. */
static void levi_page(struct ri_dlist *dl, const struct RISectUI *ui, int x0, int y0, int x1, int y1, int z) {
    const struct RISectLevi *s = &ui->u.slevi;
    int w = x1 - x0, h = y1 - y0, k, cw = w / 4;
    char buf[16];
    (void)z;
    ri_art_rect(dl, x0 - 2, y0 - 2, x1 + 2, y1 + 2, C_LEVI_CAP);
    ri_art_rect(dl, x0, y0, x1, y1, C_LEVI_LCD);
    levi_text(dl, x0 + w / 2, y0 + h * 12 / 100, ri_slevi_page_title(s), C_LEVI_TEAL, 0);
    ri_draw_rect(dl, x0 + 4, y0 + h * 22 / 100, x1 - 4, y0 + h * 22 / 100, ri_art_rgb(C_LEVI_EDGE));
    for (k = 0; k < 8; k++) {
        int cx = x0 + cw * (k % 4) + cw / 2;
        int cy = y0 + (k < 4 ? h * 38 / 100 : h * 72 / 100);
        int live = ri_slevi_enc_live(s, (uint32_t)k);
        const char *n = ri_slevi_enc_name(s, (uint32_t)k);
        levi_label(dl, cx, cy, n, levi_brief(n), cw - 2, live ? C_LEVI_TEAL : C_LEVI_DIM, 0);
        if (live) {
            ri_slevi_enc_text(s, (uint32_t)k, buf, (uint32_t)sizeof buf);
            levi_text(dl, cx, cy + h * 14 / 100, buf, C_LEVI_LABEL, 0);
        }
    }
}

void ri_art_levi_item(struct ri_dlist *dl, const struct RIGeoItem *it, const struct RICtlDef *d,
    const struct RISectUI *ui, int cx, int cy, int hw, int hh, int z) {
    static const int OSCCOL[8] = { C_LEVI_TEAL, C_LEVI_O2, C_LEVI_O3, C_LEVI_O4, C_LEVI_O5, C_LEVI_O6,
        C_LEVI_O7, C_LEVI_O8 };
    uint32_t idx = it->reg_id & 0xFFu, pan = ri_art_rgb(C_LEVI_PANEL);
    const struct RISectLevi *s = &ui->u.slevi;
    int v = ri_sui_value(ui, idx);
#define PX(q) ri_geo_px((q), z)
    if (idx >= RI_SLEVI_ENC0 && idx < RI_SLEVI_ENC0 + RI_SLEVI_NENC) {
        /* collar (r * 4/3) stays inside the item box: damage boxes (S3) */
        levi_encoder(dl, cx, cy, hw * 3 / 4, v, ri_slevi_enc_live(s, idx - RI_SLEVI_ENC0), pan);
        return;
    }
    if (idx == RI_SLEVI_PAGE) {
        levi_page(dl, ui, cx - hw, cy - hh, cx + hw, cy + hh, z);
        return;
    }
    if (idx == RI_SLEVI_ALGODISP) {                 /* 2-digit readout, white segments */
        int n = ri_sui_display(ui, idx), dw = (2 * hw - PX(14)) / 2;
        ri_art_rect(dl, cx - hw, cy - hh, cx + hw, cy + hh, C_LEVI_CAP);
        levi_seg7(dl, cx - hw + PX(5), cy - hh + PX(6), cx - hw + PX(5) + dw - PX(2), cy + hh - PX(6),
            n ? n / 10 : 10);                        /* 0 = custom: "C " */
        levi_seg7(dl, cx + PX(2), cy - hh + PX(6), cx + PX(2) + dw - PX(2), cy + hh - PX(6), n ? n % 10 : 11);
        return;
    }
    if (idx == RI_SLEVI_DISPLAY) {                  /* STEP EDIT readout */
        ri_art_rect(dl, cx - hw, cy - hh, cx + hw, cy + hh, C_LEVI_CAP);
        ri_art_led_digits(dl, cx - hw + PX(3), cy - hh + PX(3), cx + hw - PX(3), cy + hh - PX(3),
            ri_sui_display(ui, idx), 2);
        return;
    }
    if (idx >= RI_SLEVI_STEP0 && idx < RI_SLEVI_STEP0 + 16u) {   /* ribbon step segment */
        uint32_t st = idx - RI_SLEVI_STEP0;
        int on = ri_sui_led(ui, idx, 0), edit = (uint32_t)(ri_sui_display(ui, RI_SLEVI_DISPLAY) - 1) == st;
        ri_art_rect(dl, cx - hw, cy - hh, cx + hw, cy + hh, edit ? C_LEVI_LABEL : C_LEVI_EDGE);
        ri_draw_rect(dl, cx - hw + 1, cy - hh + 1, cx + hw - 1, cy + hh - 1,
            on ? ri_art_rgb(C_LEVI_TEAL) : ri_art_rgb(C_LEVI_BOX));
        if (on)
            ri_draw_rect(dl, cx - hw + 1, cy - hh + 1, cx + hw - 1, cy - hh + 2, ri_art_shade(ri_art_rgb(C_LEVI_TEAL), 45));
        {
            char n[3];
            n[0] = (char)(st >= 9u ? '1' : (char)('1' + st));
            n[1] = (char)(st >= 9u ? (char)('0' + st - 9u) : 0);
            n[2] = 0;
            levi_text(dl, cx, cy, n, on ? C_LEVI_CAP : C_LEVI_DIM, RI_FACE_S);
        }
        return;
    }
    if (idx >= RI_SLEVI_KEY0 && idx < RI_SLEVI_KEY0 + RI_SLEVI_KEYS) {       /* keybed key */
        int lit = ri_sui_led(ui, idx, 0);
        int black = hh > PX(50);
        if (!black) {                      /* white: hit band is the lower part; paint full height */
            cy = (PX(LEVI_KEY_Y0) + PX(LEVI_KEY_Y1)) / 2 + (cy - PX(510));
            hh = (PX(LEVI_KEY_Y1) - PX(LEVI_KEY_Y0)) / 2;
        }
        uint32_t c = ri_art_rgb(black ? C_BLACK : C_WHITEKEY);
        ri_draw_rect(dl, cx - hw, cy - hh, cx + hw, cy + hh, black ? c : ri_art_shade(c, -10));
        ri_draw_rect(dl, cx - hw + 1, cy - hh, cx + hw - 1, cy + hh - PX(8), c);
        ri_draw_rect(dl, cx - hw + 1, cy + hh - PX(8), cx + hw - 1, cy + hh, ri_art_shade(c, black ? 18 : -30));
        if (lit)
            ri_draw_rect(dl, cx - hw + PX(4), cy + hh - PX(26), cx + hw - PX(4), cy + hh - PX(14),
                ri_art_rgb(C_LEVI_TEAL));
        return;
    }
    if (it->shape == RI_GEO_KNOB) {                 /* aluminium knob (algorithm: encoder) */
        if (idx == RI_SLEVI_ALGO) {
            levi_encoder(dl, cx, cy, hw * 3 / 4, v * 127 / 63, 1, pan);   /* 64 algorithms (P3) */
            return;
        }
        ri_art_knob(dl, cx, cy, PX(it->w), PX(it->h), C_LEVI_SILVER, C_BLACK, 0,
            (float)ri_knob_pointer_mdeg((int)((v - d->min_v) * 127 / (d->max_v > d->min_v ? d->max_v - d->min_v : 1)))
                / 1000.0f, pan);
        return;
    }
    if (it->shape == RI_GEO_OPTION) {
        const char *l1 = "", *l2 = "";
        char n[2];
        int lit, col = C_LEVI_TEAL;
        if (idx == RI_SLEVI_OPSEL) {                 /* OSC 1..8, per-osc colours */
            n[0] = (char)('1' + it->opt);
            n[1] = 0;
            lit = ri_sui_value(ui, RI_SLEVI_MODULE) == (int)RI_SLEVI_M_OSC && v == it->opt;
            levi_cap(dl, cx, cy, hw, hh, "OSC", n, OSCCOL[it->opt & 7], lit);
            return;
        }
        if (idx == RI_SLEVI_SELECT) {                /* STEP EDIT lane */
            n[0] = (char)('1' + it->opt);
            n[1] = 0;
            levi_cap(dl, cx, cy, hw, hh, n, "", C_LEVI_TEAL, v == it->opt);
            return;
        }
        levi_mod_label((uint32_t)it->opt, &l1, &l2);
        lit = v == it->opt;
        levi_cap(dl, cx, cy, hw, hh, l1, l2, col, lit);
        return;
    }
    if (it->shape == RI_GEO_RECT) {                  /* live caps */
        const char *l1 = idx == RI_SLEVI_ARPON ? "ARP" : idx == RI_SLEVI_SEQON ? "SEQ" :
            idx == RI_SLEVI_STEP ? "STEP" : idx == RI_SLEVI_BACK ? "BACK" :
            idx == RI_SLEVI_PAGEUP ? "^" : idx == RI_SLEVI_PAGEDN ? "v" : "";
        const char *l2 = idx == RI_SLEVI_ARPON ? "ON" : idx == RI_SLEVI_SEQON ? ">" : "";
        levi_cap(dl, cx, cy, hw, hh, l1, l2, idx == RI_SLEVI_SEQON ? C_LEVI_LABEL : C_LEVI_TEAL,
            (idx == RI_SLEVI_ARPON || idx == RI_SLEVI_SEQON) && v != 0);
        return;
    }
#undef PX
}
