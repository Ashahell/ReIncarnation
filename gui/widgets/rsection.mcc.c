/*
 * rsection.mcc.c — RSection: every laid-out ReBirth section as one canvas
 * (§12.10 G4).
 *
 * AROS-ONLY. Subclasses MUIC_Area. What exists and where comes from the
 * registry + measured geometry; what a click does comes from gui/sectui.h;
 * this file only renders and routes the pointer. Section looks are
 * clean-room approximations of the ReBirth panels (manual figures p. 148,
 * p. 153): light synth panel with black keyboard block; dark 808 panel with
 * red LEVEL knobs, white parameter knobs, cream instrument legends and the
 * TR-808 step colours (1-4 red, 5-8 orange, 9-12 yellow, 13-16 white).
 * Knobs/selectors: press + drag (up/right increase, P-18 law in the
 * control's own range), [Shift] fine, right-click = default. Buttons,
 * steps, switches and instrument legends act on press.
 * Lessons kept: pens per screen in Setup (palette screens ignore RGB pens);
 * event handler added in Setup on _win(obj) (never _window); paint on every
 * Draw (Zune does not flag the initial show-time Draw).
 * Must NEVER enter the host build (audit gates widgets/).
 */

#ifndef __AROS__
#error "rsection.mcc.c is AROS-only: Zune custom class, never in the host build"
#endif

#include <exec/types.h>
#include <intuition/classes.h>
#include <intuition/classusr.h>
#include <intuition/intuition.h>
#include <graphics/rastport.h>
#include <graphics/view.h>
#include <devices/inputevent.h>
#include <utility/tagitem.h>
#include <libraries/mui.h>
#include <proto/exec.h>
#include <proto/intuition.h>
#include <proto/graphics.h>
#include <proto/muimaster.h>
#include <proto/utility.h>
#include <clib/alib_protos.h>
#include <string.h>
#include "gui/ctlreg.h"
#include "gui/panelgeo.h"
#include "gui/sectui.h"
#include "gui/knob_logic.h"
#include "engine/dsp/kernels.h"
#include "gui/widgets/rsection.h"

#define TAG_SECTION (TAG_USER + 0x52534381u)
#define TAG_ZOOM (TAG_USER + 0x52534382u)

enum { C_PANEL, C_PANEL_DK, C_BLACK, C_WHITEKEY, C_BTN, C_BTN_HI, C_BTN_LO,
       C_LED_ON, C_LED_OFF, C_KNOB, C_KNOB_RIM, C_TICK, C_TEXT, C_TEXT_INV,
       C_SEG_BG, C_SEG, C_DISABLED,
       C_808_PANEL, C_808_LINE, C_KNOB_RED, C_KNOB_WHITE, C_CREAM, C_CREAM_LIT,
       C_STEP_RED, C_STEP_ORANGE, C_STEP_YELLOW, C_STEP_WHITE, C_LAMP_OFF,
       C_909_PANEL, C_909_BAR, C_909_ORANGE, C_909_KNOB, C_LAMP_LOW, C_LAMP_FLAM, C_909_STEP,
       C_MIX_PANEL, C_MIX_HEAD, C_MIX_HEADTX, C_MIX_SLOT, C_MIX_GREEN, C_MIX_GREEN_OFF, C_MIX_KNOB, C_MIX_TEXT,
       C_FX_PANEL, C_FX_HEAD, C_SEG_DIM,
       C_NCOL };
static const ULONG RI_RSECT_RGB[C_NCOL] = {
    0xD6D6CEu, 0x8C8C84u, 0x141414u, 0xF4F4F0u, 0xB4B4AEu, 0xF0F0EAu, 0x5A5A56u,
    0xFF2A1Au, 0x5A1410u, 0xC8C8C4u, 0x3C3C3Cu, 0x2A2A2Au, 0x1E1E1Eu, 0xF0F0F0u,
    0x280808u, 0xFF3020u, 0x9C9C96u,
    0x3A362Eu, 0x6A6458u, 0xC41E1Eu, 0xE6E6E0u, 0xE8E0C8u, 0xFFF6D0u,
    0xC82020u, 0xE07418u, 0xE6D21Eu, 0xE4E4DCu, 0x2A2620u,
    0xDCDCD4u, 0x2E2E2Cu, 0xE8761Eu, 0x4A4A48u, 0xE8901Eu, 0x30D040u, 0xC4C4BCu,
    0x5E6A72u, 0x8E8A3Au, 0xF2EAB8u, 0x1A1E22u, 0x38E040u, 0x1E4A22u, 0x9AA0A6u, 0xE8ECEEu,
    0x6E7276u, 0x2E3A7Au, 0x4A1210u
};
static LONG s_pens[C_NCOL];

/* 909 header bars (TR-909 panel: dark bar, orange instrument name over its
 * step group) and instrument-select legend texts (ReBirth p. 151 boxes) */
static const struct { int x0, x1; const char *t; } RI_909_BAR[] = {
    { 22, 98, "AC" }, { 108, 272, "BASS DRUM" }, { 276, 440, "SNARE DRUM" }, { 444, 608, "LOW TOM" },
    { 612, 776, "MID TOM" }, { 780, 944, "HI TOM" }, { 948, 1028, "RIM" }, { 1032, 1112, "CLAP" },
    { 1116, 1280, "HI HAT" }, { 1284, 1448, "CYMBAL" }
};
static const char *const RI_909_OPT[12] = {
    "AC", "BASS DRUM", "SNARE DRUM", "LOW TOM", "MID TOM", "HI TOM", "RS", "CP", "CH", "OH", "CC", "RC"
};
static const char *const RI_808_OPT[12] = {
    "AC", "BD", "SD", "LT", "MT", "HT", "RS", "CP", "CB", "CY", "OH", "CH"
};

struct RSectionData {
    struct RISectUI ui;
    LONG zoom;
    LONG changes;
    uint16_t drag_id;
    LONG drag_n0;
    double acc_dx, acc_dy;
    WORD last_x, last_y;
    BOOL shown;
    uint16_t rep_idx;      /* held value-display arrow, 0xFFFF none */
    int8_t rep_dir;
    uint8_t rep_ticks;
    struct MUI_EventHandlerNode ehn;
    struct RSectionDiag diag;
};

static void pen(struct RastPort *rp, ULONG col) {
    SetAPen(rp, (ULONG)s_pens[col < C_NCOL ? col : C_BLACK]);
}

static void pens_obtain(Object *obj) {
    struct ColorMap *cm = _screen(obj)->ViewPort.ColorMap;
    ULONG i;
    for (i = 0; i < C_NCOL; i++) {
        ULONG c = RI_RSECT_RGB[i];
        s_pens[i] = ObtainBestPen(cm, (c >> 16 & 0xFF) * 0x01010101u,
            (c >> 8 & 0xFF) * 0x01010101u, (c & 0xFF) * 0x01010101u,
            OBP_Precision, PRECISION_EXACT, TAG_DONE);
    }
}

static void pens_release(Object *obj) {
    struct ColorMap *cm = _screen(obj)->ViewPort.ColorMap;
    ULONG i;
    for (i = 0; i < C_NCOL; i++)
        if (s_pens[i] >= 0)
            ReleasePen(cm, (ULONG)s_pens[i]);
}

static void fill_rect(struct RastPort *rp, int x0, int y0, int x1, int y1, ULONG col) {
    if (x1 < x0 || y1 < y0)
        return;
    pen(rp, col);
    RectFill(rp, x0, y0, x1, y1);
}

static void fill_circle(struct RastPort *rp, int cx, int cy, int r, ULONG col) {
    int dy;
    pen(rp, col);
    for (dy = -r; dy <= r; dy++) {
        int dx = r;
        while (dx > 0 && dx * dx + dy * dy > r * r)
            dx--;
        RectFill(rp, cx - dx, cy + dy, cx + dx, cy + dy);
    }
}

static void line(struct RastPort *rp, int x0, int y0, int x1, int y1, ULONG col) {
    pen(rp, col);
    Move(rp, x0, y0);
    Draw(rp, x1, y1);
}

static void text_c(struct RastPort *rp, int cx, int cy, const char *s0, ULONG col) {
    char s[24]; /* panel legends are upper case, as on the ReBirth panels */
    int n = 0, w;
    while (s0[n] && n < 23) {
        s[n] = (s0[n] >= 'a' && s0[n] <= 'z') ? (char)(s0[n] - 32) : s0[n];
        n++;
    }
    s[n] = 0;
    pen(rp, col);
    SetDrMd(rp, JAM1);
    w = TextLength(rp, (STRPTR)s, n);
    Move(rp, cx - w / 2, cy + rp->TxBaseline / 2);
    Text(rp, (STRPTR)s, n);
}

static void bevel(struct RastPort *rp, int x0, int y0, int x1, int y1, ULONG face) {
    fill_rect(rp, x0, y0, x1, y1, face);
    line(rp, x0, y0, x1, y0, C_BTN_HI);
    line(rp, x0, y0, x0, y1, C_BTN_HI);
    line(rp, x0, y1, x1, y1, C_BTN_LO);
    line(rp, x1, y0, x1, y1, C_BTN_LO);
}

static LONG to_n(const struct RICtlDef *d, int v) {
    int span = d->max_v - d->min_v;
    return span > 0 ? (LONG)(((long)(v - d->min_v) * 127 + span / 2) / span) : 0;
}
static int from_n(const struct RICtlDef *d, LONG n) {
    int span = d->max_v - d->min_v;
    return d->min_v + (int)(((long)n * span + 63) / 127);
}

static void polar(int cx, int cy, float deg, float r, int *x, int *y) {
    float a = (deg - 90.0f) * 0.0174533f; /* 0 deg = up, clockwise */
    *x = cx + (int)(ri_sin(a + 1.5707963f) * r);
    *y = cy + (int)(ri_sin(a) * r);
}

static void draw_knob(struct RastPort *rp, int cx, int cy, int body, int ring, ULONG face,
    ULONG ptr, BOOL ticks, float deg) {
    int k, rb = body / 2, rr = ring / 2, x0, y0, x1, y1;
    if (ticks)
        for (k = 0; k <= 10; k++) {
            polar(cx, cy, (float)(-135 + 27 * k), (float)(rb + 2), &x0, &y0);
            polar(cx, cy, (float)(-135 + 27 * k), (float)rr, &x1, &y1);
            line(rp, x0, y0, x1, y1, C_TICK);
        }
    fill_circle(rp, cx, cy, rb, C_KNOB_RIM);
    fill_circle(rp, cx, cy, rb - 2, face);
    polar(cx, cy, deg, (float)(rb / 3), &x0, &y0);
    polar(cx, cy, deg, (float)(rb - 3), &x1, &y1);
    line(rp, x0, y0, x1, y1, ptr);
}

/* ---------------------------------------------------------------- 303 */
static void bg_303(struct RastPort *rp, const struct RIGeoSection *g, int ox, int oy, int z) {
    static const int black[13] = { 0, 1, 0, 1, 0, 0, 1, 0, 1, 0, 1, 0, 0 };
    uint32_t i, j;
#define PX(q) ri_geo_px((q), z)
    fill_rect(rp, ox, oy, ox + PX(g->w) - 1, oy + PX(g->h) - 1, C_PANEL);
    line(rp, ox, oy + PX(185), ox + PX(g->w) - 1, oy + PX(185), C_PANEL_DK);
    fill_rect(rp, ox + PX(185), oy + PX(200), ox + PX(855), oy + PX(435), C_BLACK);
    for (i = 0; i < 13; i++)
        for (j = 0; j < g->nitems; j++)
            if ((g->items[j].reg_id & 0xFFu) == RI_S303_KEY0 + i && g->items[j].shape == RI_GEO_RECT && !black[i])
                fill_rect(rp, ox + PX(g->items[j].cx - 38), oy + PX(208), ox + PX(g->items[j].cx + 38),
                    oy + PX(428), C_WHITEKEY);
    for (i = 0; i < 13; i++)
        for (j = 0; j < g->nitems; j++)
            if ((g->items[j].reg_id & 0xFFu) == RI_S303_KEY0 + i && g->items[j].shape == RI_GEO_RECT && black[i])
                fill_rect(rp, ox + PX(g->items[j].cx - 28), oy + PX(208), ox + PX(g->items[j].cx + 28),
                    oy + PX(320), C_BLACK);
    fill_rect(rp, ox + PX(860), oy + PX(290), ox + PX(1250), oy + PX(320), C_BLACK);
#undef PX
}

/* ---------------------------------------------------------------- 808 */
static ULONG step_colour_808(uint32_t step) {
    return step < 4 ? C_STEP_RED : step < 8 ? C_STEP_ORANGE : step < 12 ? C_STEP_YELLOW : C_STEP_WHITE;
}

static void bg_808(struct RastPort *rp, const struct RIGeoSection *g, int ox, int oy, int z) {
#define PX(q) ri_geo_px((q), z)
    fill_rect(rp, ox, oy, ox + PX(g->w) - 1, oy + PX(g->h) - 1, C_808_PANEL);
    line(rp, ox + PX(1110), oy + PX(10), ox + PX(1110), oy + PX(340), C_808_LINE);
    line(rp, ox + PX(40), oy + PX(345), ox + PX(1100), oy + PX(345), C_808_LINE);
#undef PX
}

/* ---------------------------------------------------------------- 909 */
static void bg_909(struct RastPort *rp, const struct RIGeoSection *g, int ox, int oy, int z) {
    uint32_t i;
    char n[3];
#define PX(q) ri_geo_px((q), z)
    fill_rect(rp, ox, oy, ox + PX(g->w) - 1, oy + PX(g->h) - 1, C_909_PANEL);
    for (i = 0; i < sizeof(RI_909_BAR) / sizeof(RI_909_BAR[0]); i++) {
        int x0 = RI_909_BAR[i].x0, x1 = RI_909_BAR[i].x1;
        fill_rect(rp, ox + PX(x0), oy + PX(26), ox + PX(x1), oy + PX(56), C_909_BAR);
        text_c(rp, ox + PX((x0 + x1) / 2), oy + PX(41), RI_909_BAR[i].t, C_909_ORANGE);
    }
    for (i = 0; i <= 8; i++)                   /* group dividers, as on the TR-909 panel */
        line(rp, ox + PX(106 + 168 * (int)i), oy + PX(56), ox + PX(106 + 168 * (int)i), oy + PX(296), C_909_BAR);
    line(rp, ox + PX(1030), oy + PX(56), ox + PX(1030), oy + PX(296), C_909_BAR);
    line(rp, ox + PX(20), oy + PX(296), ox + PX(1448), oy + PX(296), C_909_BAR);
    for (i = 0; i < 16; i++) {                 /* step numbers under the keys (p. 151) */
        n[0] = (char)(i >= 9 ? '1' : '0' + (i + 1));
        n[1] = (char)(i >= 9 ? '0' + (i + 1 - 10) : 0);
        n[2] = 0;
        fill_rect(rp, ox + PX(148 + 84 * (int)i - 40), oy + PX(418), ox + PX(148 + 84 * (int)i + 40), oy + PX(446),
            C_909_BAR);
        text_c(rp, ox + PX(148 + 84 * (int)i), oy + PX(432), n, C_CREAM);
    }
#undef PX
}

/* TR-909 key: dark frame, off-white cap, lamp window at the top */
static void key_909(struct RastPort *rp, int x0, int y0, int x1, int y1, int z, ULONG lamp) {
    int f = ri_geo_px(6, z), lw = (x1 - x0) / 4;
    fill_rect(rp, x0, y0, x1, y1, C_909_BAR);
    bevel(rp, x0 + f, y0 + f, x1 - f, y1 - f, C_WHITEKEY);
    fill_rect(rp, (x0 + x1) / 2 - lw, y0 + f + ri_geo_px(8, z), (x0 + x1) / 2 + lw, y0 + f + ri_geo_px(16, z), lamp);
}

static const char *legend_909(const char *leg) {
    return !strcmp(leg, "Attack") ? "ATT" : !strcmp(leg, "Decay") ? "DEC" : !strcmp(leg, "Snappy") ? "SNAP" : leg;
}

/* ------------------------------------------------------ mixers/master */
/* ReBirth mixer (p. 157) and master (p. 23): blue-grey panel, olive title
 * bar, green on/off lamp, segment meters, fader with scale, small rockers
 * with red LEDs. */
static void bg_mix(struct RastPort *rp, const struct RIGeoSection *g, int ox, int oy, int z, int master) {
    static const char *const db[5] = { "CLIP", "-6", "-12", "-24", "-36" };
    static const int dby[5] = { 120, 158, 200, 240, 272 };
    int k;
#define PX(q) ri_geo_px((q), z)
    fill_rect(rp, ox, oy, ox + PX(g->w) - 1, oy + PX(g->h) - 1, C_MIX_PANEL);
    if (master) {
        fill_rect(rp, ox + PX(22), oy + PX(20), ox + PX(315), oy + PX(70), C_MIX_HEAD);
        text_c(rp, ox + PX(170), oy + PX(45), "MASTER", C_MIX_HEADTX);
        for (k = 0; k < 5; k++) {
            text_c(rp, ox + PX(42), oy + PX(dby[k]), db[k], C_MIX_TEXT);
            text_c(rp, ox + PX(292), oy + PX(dby[k]), db[k], C_MIX_TEXT);
        }
        for (k = 0; k < 8; k++)                    /* fader scale */
            line(rp, ox + PX(128), oy + PX(120 + 25 * k), ox + PX(202), oy + PX(120 + 25 * k), C_MIX_TEXT);
    } else {
        fill_rect(rp, ox + PX(10), oy + PX(10), ox + PX(274), oy + PX(82), C_MIX_HEAD);
        text_c(rp, ox + PX(142), oy + PX(45), "MIX", C_MIX_HEADTX);
        text_c(rp, ox + PX(32), oy + PX(190), "L", C_MIX_TEXT);
        text_c(rp, ox + PX(125), oy + PX(190), "R", C_MIX_TEXT);
        text_c(rp, ox + PX(160), oy + PX(407), "0", C_MIX_TEXT);
        text_c(rp, ox + PX(245), oy + PX(407), "10", C_MIX_TEXT);
        for (k = 0; k < 7; k++)                    /* fader scale */
            line(rp, ox + PX(32), oy + PX(262 + 28 * k), ox + PX(118), oy + PX(262 + 28 * k), C_MIX_TEXT);
    }
#undef PX
}

/* vertical segment meter, level 0..127; master: clip lamp on top */
static void meter(struct RastPort *rp, int x0, int y0, int x1, int y1, int level, int nseg, int master) {
    int k, h = (y1 - y0 - 2) / nseg;
    fill_rect(rp, x0, y0, x1, y1, C_MIX_SLOT);
    for (k = 0; k < nseg; k++) {                   /* k = 0 bottom */
        int lit = level * nseg > k * 127 && level > 0;
        int top = k == nseg - 1, warn = k >= nseg - (master ? 3 : 2);
        ULONG on = top ? C_LED_ON : warn ? C_STEP_YELLOW : C_MIX_GREEN;
        ULONG off = top ? C_LED_OFF : warn ? C_LAMP_OFF : C_MIX_GREEN_OFF;
        int yb = y1 - 1 - k * h;
        fill_rect(rp, x0 + 2, yb - h + 2, x1 - 2, yb, lit ? on : off);
    }
}

/* ReBirth fader: dark slot, black cap with a light index line */
static void fader(struct RastPort *rp, int cx, int y0, int y1, int capw, int caph, int n) {
    int travel = (y1 - y0) - caph, cy = y0 + caph / 2 + (int)((long)travel * (127 - n) / 127);
    fill_rect(rp, cx - 1, y0, cx + 1, y1, C_BLACK);
    bevel(rp, cx - capw / 2, cy - caph / 2, cx + capw / 2, cy + caph / 2, C_MIX_SLOT);
    line(rp, cx - capw / 2 + 2, cy, cx + capw / 2 - 2, cy, C_MIX_TEXT);
}

/* ------------------------------------------------------------ FX units */
/* ReBirth FX units (p. 159-164): grey panel, navy title bar with the same
 * on/off lamp + input meter header as the mixers, red LED digits with
 * arrow buttons, vertical mode levers, sliders (PCF), knobs with 0/10 or
 * L/R marks, a horizontal level-reduction LED row (Comp). */
static void note_glyph(struct RastPort *rp, int x, int y, int flags, int z) {
    int k;
    fill_circle(rp, x, y, ri_geo_px(6, z), C_MIX_TEXT);
    line(rp, x + ri_geo_px(6, z), y, x + ri_geo_px(6, z), y - ri_geo_px(28, z), C_MIX_TEXT);
    for (k = 0; k < flags; k++)
        line(rp, x + ri_geo_px(6, z), y - ri_geo_px(28 - 8 * k, z), x + ri_geo_px(16, z),
            y - ri_geo_px(20 - 8 * k, z), C_MIX_TEXT);
}

static void bg_fx(struct RastPort *rp, const struct RIGeoSection *g, int ox, int oy, int z, uint8_t sec) {
    static const char *const title[4] = { "PCF", "DELAY", "DIST", "COMP" };
    int k, x1 = g->w - 17;
#define PX(q) ri_geo_px((q), z)
    fill_rect(rp, ox, oy, ox + PX(g->w) - 1, oy + PX(g->h) - 1, C_FX_PANEL);
    fill_rect(rp, ox + PX(17), oy + PX(17), ox + PX(x1), oy + PX(70), C_FX_HEAD);
    text_c(rp, ox + PX(g->w / 2), oy + PX(43), title[sec - RI_SEC_PCF], C_MIX_TEXT);
    if (sec == RI_SEC_PCF) {
        text_c(rp, ox + PX(268), oy + PX(105), "BP", C_MIX_TEXT);
        text_c(rp, ox + PX(268), oy + PX(145), "LP", C_MIX_TEXT);
        for (k = 0; k < 6; k++)                         /* slider scale */
            line(rp, ox + PX(18), oy + PX(234 + 26 * k), ox + PX(314), oy + PX(234 + 26 * k), C_MIX_TEXT);
    } else if (sec == RI_SEC_DELAY) {
        text_c(rp, ox + PX(262), oy + PX(92), "3", C_MIX_TEXT);  /* 8th-note triplet */
        note_glyph(rp, ox + PX(258), oy + PX(122), 1, z);
        note_glyph(rp, ox + PX(258), oy + PX(160), 2, z);        /* 16th note */
        text_c(rp, ox + PX(40), oy + PX(315), "L", C_MIX_TEXT);
        text_c(rp, ox + PX(133), oy + PX(315), "R", C_MIX_TEXT);
        text_c(rp, ox + PX(205), oy + PX(315), "0", C_MIX_TEXT);
        text_c(rp, ox + PX(292), oy + PX(315), "10", C_MIX_TEXT);
    } else if (sec == RI_SEC_DIST) {
        for (k = 0; k < 2; k++) {
            text_c(rp, ox + PX(k ? 208 : 46), oy + PX(202), "0", C_MIX_TEXT);
            text_c(rp, ox + PX(k ? 294 : 132), oy + PX(202), "10", C_MIX_TEXT);
        }
    } else {
        for (k = 0; k < 9; k++)                         /* reduction scale, 0 in the middle */
            line(rp, ox + PX(48 + 28 * k), oy + PX(136), ox + PX(48 + 28 * k), oy + PX(142), C_MIX_TEXT);
        text_c(rp, ox + PX(160), oy + PX(156), "0", C_MIX_TEXT);
        for (k = 0; k < 2; k++) {
            text_c(rp, ox + PX(k ? 206 : 46), oy + PX(312), "0", C_MIX_TEXT);
            text_c(rp, ox + PX(k ? 292 : 132), oy + PX(312), "10", C_MIX_TEXT);
        }
    }
#undef PX
}

/* two red LED digits (unlit "8" behind, as on the ReBirth displays) */
static void led_digits(struct RastPort *rp, int x0, int y0, int x1, int y1, int v) {
    char b[3];
    int cx = (x0 + x1) / 2, cy = (y0 + y1) / 2;
    fill_rect(rp, x0, y0, x1, y1, C_SEG_BG);
    text_c(rp, cx, cy, "88", C_SEG_DIM);
    b[0] = (char)(v >= 10 ? '0' + v / 10 % 10 : ' ');
    b[1] = (char)('0' + v % 10);
    b[2] = 0;
    text_c(rp, cx, cy, b, C_SEG);
}

static void arrow_btn(struct RastPort *rp, int x0, int y0, int x1, int y1, int up) {
    int cx = (x0 + x1) / 2, cy = (y0 + y1) / 2, h = (y1 - y0) / 4, k;
    bevel(rp, x0, y0, x1, y1, C_WHITEKEY);
    for (k = 0; k <= h; k++)                       /* filled triangle */
        line(rp, cx - k, up ? cy - h / 2 + k : cy + h / 2 - k, cx + k, up ? cy - h / 2 + k : cy + h / 2 - k, C_BLACK);
}

/* Comp level reduction: 9 LEDs, "0" in the middle, reduction lights leftward */
static void gr_row(struct RastPort *rp, int x0, int y0, int x1, int y1, int v) {
    int k, n = v > 0 ? 1 + (v * 4 + 126) / 127 : 0, w = (x1 - x0) / 9;
    for (k = 0; k < 9; k++) {
        int lit = k <= 4 && 4 - k < n;
        ULONG on = k <= 2 ? C_MIX_GREEN : C_STEP_YELLOW, off = k <= 2 ? C_MIX_GREEN_OFF : k <= 4 ? C_LAMP_OFF : C_LED_OFF;
        fill_circle(rp, x0 + w * k + w / 2, (y0 + y1) / 2, (y1 - y0) / 2 - 1, lit ? on : off);
    }
}

static const char *legend_fx(const char *leg) {
    return !strcmp(leg, "Decay") ? "DEC" : !strcmp(leg, "Threshold") ? "THRES" : leg;
}

/* ------------------------------------------------------------ generic */
static void draw_section(Object *obj, struct RSectionData *dd) {
    struct RastPort *rp = _rp(obj);
    uint8_t sec = dd->ui.section;
    const struct RIGeoSection *g = ri_geo_section(sec == RI_SEC_SYNTH2 ? RI_SEC_SYNTH1 : sec);
    int ox = _mleft(obj), oy = _mtop(obj), z = (int)dd->zoom, is808 = sec == RI_SEC_808;
    int is909 = sec == RI_SEC_909, ismix = ri_smix_strip(sec) >= 0, isfx = sec >= RI_SEC_PCF && sec <= RI_SEC_COMP;
    ULONG txt = is808 ? C_CREAM : ismix || isfx ? C_MIX_TEXT : C_TEXT;
    uint32_t i;
    char buf[4];
#define PX(q) ri_geo_px((q), z)
    if (!g)
        return;
    if (is808)
        bg_808(rp, g, ox, oy, z);
    else if (is909)
        bg_909(rp, g, ox, oy, z);
    else if (ismix)
        bg_mix(rp, g, ox, oy, z, sec == RI_SEC_MASTER);
    else if (isfx)
        bg_fx(rp, g, ox, oy, z, sec);
    else
        bg_303(rp, g, ox, oy, z);
    for (i = 0; i < g->nitems; i++) {
        const struct RIGeoItem *it = &g->items[i];
        uint32_t idx = it->reg_id & 0xFFu;
        const struct RICtlDef *d = ri_ctlreg_find((uint16_t)((sec << 8) | idx));
        int cx = ox + PX(it->cx), cy = oy + PX(it->cy);
        int hw = PX(it->w) / 2, hh = PX(it->h) / 2;
        int v;
        if (!d)
            continue;
        v = ri_sui_value(&dd->ui, idx);
        switch (it->shape) {
        case RI_GEO_KNOB:
            if (d->kind == RI_CK_SELECTOR) {             /* 808 instrument selector */
                draw_knob(rp, cx, cy, PX(it->w), PX(it->h), C_BLACK, C_LED_ON, FALSE,
                    208.0f + 28.2f * (float)v);
            } else {
                ULONG face = d->bind == RI_BIND_NONE ? C_DISABLED
                    : is909 ? C_909_KNOB : ismix || isfx ? C_MIX_KNOB
                    : !is808 ? C_KNOB : !strcmp(d->legend, "Level") ? C_KNOB_RED : C_KNOB_WHITE;
                draw_knob(rp, cx, cy, PX(it->w), PX(it->h), face, is909 ? C_909_ORANGE : C_BLACK, !is808,
                    (float)ri_knob_pointer_mdeg((int)to_n(d, v)) / 1000.0f);
            }
            break;
        case RI_GEO_RECT:
            if (isfx && d->kind == RI_CK_METER && idx == RI_SFX_COMP_GR) {
                gr_row(rp, cx - hw, cy - hh, cx + hw, cy + hh, v);
            } else if (isfx && d->kind == RI_CK_SELECTOR) {
                led_digits(rp, cx - hw, cy - hh, cx + hw, cy + hh, v);
            } else if (isfx && d->kind == RI_CK_SWITCH && idx != RI_SFX_ONOFF) {  /* vertical lever */
                fill_rect(rp, cx - hw, cy - hh, cx + hw, cy + hh, C_MIX_SLOT);
                bevel(rp, cx - hw + 2, v ? cy - hh + 2 : cy + 1, cx + hw - 2, v ? cy - 1 : cy + hh - 2, C_BTN);
            } else if (ismix || isfx) {
                if (d->kind == RI_CK_METER) {
                    meter(rp, cx - hw, cy - hh, cx + hw, cy + hh, v, sec == RI_SEC_MASTER ? 12 : isfx ? 3 : 4,
                        sec == RI_SEC_MASTER);
                } else if (d->kind == RI_CK_FADER) {
                    fader(rp, cx, cy - hh, cy + hh, 2 * hw, PX(it->w) * 3 / 5, (int)to_n(d, v));
                } else if (idx == 0 && sec != RI_SEC_MASTER) {   /* mute / bypass lamp button */
                    bevel(rp, cx - hw, cy - hh, cx + hw, cy + hh, C_MIX_SLOT);
                    fill_rect(rp, cx - hw + 3, cy - hh + 3, cx + hw - 3, cy + hh - 3,
                        ri_sui_led(&dd->ui, idx, 0) ? C_MIX_GREEN : C_MIX_GREEN_OFF);
                } else {                                                    /* insert rocker */
                    fill_rect(rp, cx - hw, cy - hh, cx + hw, cy + hh, C_MIX_SLOT);
                    bevel(rp, cx - hw + 2, cy - hh + 2, cx + hw - 2, cy + hh - 2,
                        ri_sui_led(&dd->ui, idx, 0) ? C_WHITEKEY : C_BTN);
                }
            } else if (d->kind == RI_CK_DISPLAY) {
                int n = ri_sui_display(&dd->ui, idx);
                fill_rect(rp, cx - hw, cy - hh, cx + hw, cy + hh, C_SEG_BG);
                buf[0] = (char)('0' + n / 10);
                buf[1] = (char)('0' + n % 10);
                buf[2] = 0;
                text_c(rp, cx, cy, buf, C_SEG);
            } else if (d->kind == RI_CK_STEP && is808) {
                uint32_t st = idx - RI_S808_STEP0;
                bevel(rp, cx - hw, cy - hh, cx + hw, cy + hh, step_colour_808(st));
                fill_rect(rp, cx - hw / 3, cy - hh + 3, cx + hw / 3, cy - hh + 3 + PX(10),
                    ri_sui_led(&dd->ui, idx, 0) ? C_LED_ON : C_LAMP_OFF);
            } else if (d->kind == RI_CK_STEP && is909) {
                int st = ri_sui_led(&dd->ui, idx, 0);
                ULONG lamp = st == 1 ? (ri_sui_value(&dd->ui, RI_S909_SELECT) == 0 ? C_LED_ON : C_LAMP_LOW)
                    : st == 2 ? C_LED_ON : st == 3 ? C_LAMP_FLAM : C_LAMP_OFF;
                key_909(rp, cx - hw, cy - hh, cx + hw, cy + hh, z, lamp);
            } else if (d->kind == RI_CK_SWITCH && is909) {     /* Flam button */
                key_909(rp, cx - hw, cy - hh, cx + hw, cy + hh, z, v ? C_LED_ON : C_LAMP_OFF);
            } else if (d->kind == RI_CK_SWITCH && is808) {     /* sound switch: slot + lever */
                fill_rect(rp, cx - hw, cy - hh, cx + hw, cy + hh, C_BLACK);
                if (v)
                    fill_rect(rp, cx - hw + 2, cy, cx + hw - 2, cy + hh - 2, C_BTN);
                else
                    fill_rect(rp, cx - hw + 2, cy - hh + 2, cx + hw - 2, cy, C_BTN);
            } else if (sec != RI_SEC_808 && idx == RI_S303_WAVE) {
                fill_rect(rp, cx - hw, cy - hh, cx + hw, cy + hh, C_BLACK);
                if (v)
                    bevel(rp, cx + 2, cy - hh + 2, cx + hw - 2, cy + hh - 2, C_BTN);
                else
                    bevel(rp, cx - hw + 2, cy - hh + 2, cx - 2, cy + hh - 2, C_BTN);
                text_c(rp, v ? cx + hw / 2 : cx - hw / 2, cy, v ? "SQR" : "SAW", C_TEXT);
            } else {
                bevel(rp, cx - hw, cy - hh, cx + hw, cy + hh, C_BTN);
            }
            break;
        case RI_GEO_OPTION: {
            BOOL lit = v == it->opt;
            if (is909) {                                  /* outlined legend box, LED left */
                const char *t = it->opt < 12 ? RI_909_OPT[it->opt] : "?";
                if (TextLength(rp, (STRPTR)t, (ULONG)strlen(t)) + PX(40) > 2 * hw)
                    t = it->opt == 1 ? "BASS" : it->opt == 2 ? "SNARE" : t; /* bar above names the drum */
                fill_rect(rp, cx - hw, cy - hh, cx + hw, cy + hh, C_909_BAR);
                fill_rect(rp, cx - hw + 1, cy - hh + 1, cx + hw - 1, cy + hh - 1, C_909_PANEL);
                fill_circle(rp, cx - hw + PX(12), cy, PX(5), lit ? C_LED_ON : C_LED_OFF);
                text_c(rp, cx + PX(8), cy, t, C_BLACK);
                break;
            }
            fill_rect(rp, cx - hw, cy - hh, cx + hw, cy + hh, lit ? C_CREAM_LIT : C_CREAM);
            if (lit)
                line(rp, cx - hw, cy + hh, cx + hw, cy + hh, C_LED_ON);
            {
                text_c(rp, cx, cy, it->opt < 12 ? RI_808_OPT[it->opt] : "?", C_BLACK);
            }
            break;
        }
        case RI_GEO_LED: {
            uint32_t which = 0, j;
            for (j = 0; j < i; j++)
                if (g->items[j].reg_id == it->reg_id && g->items[j].shape == RI_GEO_LED)
                    which++;
            fill_circle(rp, cx, cy, PX(it->w) / 2 + 1, ri_sui_led(&dd->ui, idx, which) ? C_LED_ON : C_LED_OFF);
            break;
        }
        case RI_GEO_STEPPER:
            arrow_btn(rp, cx - hw, cy - hh, cx + hw, cy + hh, it->opt != 0);
            break;
        case RI_GEO_LEGEND: {
            ULONG col = (!is808 && idx >= RI_S303_DOWN && idx <= RI_S303_SLIDE) ? C_TEXT_INV : txt;
            const char *s = d->legend;
            if (!is808 && idx == RI_S303_DISPLAY)
                s = "EDIT STEP";
            else if (is808 && d->kind == RI_CK_SWITCH) /* the alternate sound's legend */
                s = idx == 9 ? "LC" : idx == 12 ? "MC" : idx == 15 ? "HC" : idx == 17 ? "CL" : "MA";
            else if (is909)
                s = legend_909(s);
            else if (isfx)
                s = legend_fx(s);
            text_c(rp, cx, cy, s, col);
            break;
        }
        case RI_GEO_DIVIDER:
            line(rp, cx, cy, cx, cy + PX(it->h), is808 ? C_808_LINE : C_BLACK);
            break;
        default:
            break;
        }
    }
#undef PX
}

static void changed(Object *obj, struct RSectionData *d) {
    d->changes++;
    SetAttrs(obj, MUIA_RSection_Changes, d->changes, TAG_DONE);
    MUI_Redraw(obj, MADF_DRAWOBJECT);
}

static const struct RIGeoSection *geo(const struct RSectionData *d) {
    return ri_geo_section(d->ui.section == RI_SEC_SYNTH2 ? RI_SEC_SYNTH1 : d->ui.section);
}

BOOPSI_DISPATCHER_PROTO(IPTR, rsection_dispatcher, Class *, Object *, Msg);

BOOPSI_DISPATCHER(IPTR, rsection_dispatcher, cl, obj, msg) {
    struct RSectionData *d;
    switch (msg->MethodID) {
    case OM_NEW: {
        struct opSet *s = (struct opSet *)msg;
        Object *o = (Object *)DoSuperMethodA(cl, obj, msg);
        if (!o)
            return (IPTR)NULL;
        d = (struct RSectionData *)INST_DATA(cl, o);
        memset(&d->diag, 0, sizeof d->diag);
        d->diag.last_x = d->diag.last_y = -1;
        d->diag.last_hit = 0xFFFFu;
        if (ri_sui_init(&d->ui, (uint8_t)GetTagData(TAG_SECTION, RI_SEC_SYNTH1, s->ops_AttrList)) != 0) {
            CoerceMethod(cl, o, OM_DISPOSE);
            return (IPTR)NULL;
        }
        d->zoom = (LONG)GetTagData(TAG_ZOOM, 0, s->ops_AttrList);
        d->changes = 0;
        d->drag_id = 0xFFFFu;
        d->rep_idx = 0xFFFFu;
        d->shown = FALSE;
        return (IPTR)o;
    }
    case OM_GET: {
        struct opGet *gm = (struct opGet *)msg;
        d = (struct RSectionData *)INST_DATA(cl, obj);
        if (gm->opg_AttrID == MUIA_RSection_Changes) {
            *gm->opg_Storage = (IPTR)d->changes;
            return (IPTR)1;
        }
        if (gm->opg_AttrID == MUIA_RSection_State) {
            *gm->opg_Storage = (IPTR)&d->ui;
            return (IPTR)1;
        }
        if (gm->opg_AttrID == MUIA_RSection_Diag) {
            *gm->opg_Storage = (IPTR)&d->diag;
            return (IPTR)1;
        }
        return DoSuperMethodA(cl, obj, msg);
    }
    case MUIM_AskMinMax: {
        struct MUIP_AskMinMax *m = (struct MUIP_AskMinMax *)msg;
        IPTR rc = DoSuperMethodA(cl, obj, msg);
        const struct RIGeoSection *g;
        d = (struct RSectionData *)INST_DATA(cl, obj);
        g = geo(d);
        m->MinMaxInfo->MinWidth += ri_geo_px(g->w, (int)d->zoom);
        m->MinMaxInfo->MinHeight += ri_geo_px(g->h, (int)d->zoom);
        m->MinMaxInfo->DefWidth = m->MinMaxInfo->MaxWidth = m->MinMaxInfo->MinWidth;
        m->MinMaxInfo->DefHeight = m->MinMaxInfo->MaxHeight = m->MinMaxInfo->MinHeight;
        return rc;
    }
    case MUIM_Setup: {
        IPTR rc = DoSuperMethodA(cl, obj, msg);
        if (rc) {
            d = (struct RSectionData *)INST_DATA(cl, obj);
            pens_obtain(obj);
            d->ehn.ehn_Priority = 0;
            d->ehn.ehn_Flags = 0;
            d->ehn.ehn_Object = obj;
            d->ehn.ehn_Class = cl;
            d->ehn.ehn_Events = IDCMP_MOUSEBUTTONS | IDCMP_MOUSEMOVE | IDCMP_INTUITICKS;
            DoMethod(_win(obj), MUIM_Window_AddEventHandler, &d->ehn); /* _win, never _window */
            d->diag.setups++;
        }
        return rc;
    }
    case MUIM_Cleanup:
        d = (struct RSectionData *)INST_DATA(cl, obj);
        DoMethod(_win(obj), MUIM_Window_RemEventHandler, &d->ehn);
        pens_release(obj);
        return DoSuperMethodA(cl, obj, msg);
    case MUIM_Show: {
        IPTR rc = DoSuperMethodA(cl, obj, msg);
        d = (struct RSectionData *)INST_DATA(cl, obj);
        d->shown = TRUE;
        d->diag.shows++;
        return rc;
    }
    case MUIM_Hide:
        d = (struct RSectionData *)INST_DATA(cl, obj);
        d->shown = FALSE;
        d->drag_id = 0xFFFFu;
        d->rep_idx = 0xFFFFu;
        return DoSuperMethodA(cl, obj, msg);
    case MUIM_Draw:
        DoSuperMethodA(cl, obj, msg);
        draw_section(obj, (struct RSectionData *)INST_DATA(cl, obj));
        return (IPTR)0;
    case MUIM_HandleEvent: {
        struct MUIP_HandleEvent *m = (struct MUIP_HandleEvent *)msg;
        struct IntuiMessage *im = m->imsg;
        d = (struct RSectionData *)INST_DATA(cl, obj);
        if (!im || !d->shown)
            return (IPTR)0;
        if (im->Class == IDCMP_INTUITICKS) {   /* held arrow repeats (p. 18) */
            if (d->rep_idx != 0xFFFFu && ++d->rep_ticks >= 4 && ri_sui_step(&d->ui, d->rep_idx, d->rep_dir))
                changed(obj, d);
            return (IPTR)0;
        }
        d->diag.events++;
        if (im->Class == IDCMP_MOUSEBUTTONS) {
            int lx = im->MouseX - _mleft(obj), ly = im->MouseY - _mtop(obj), opt = -1;
            uint16_t id = ri_geo_hit_opt(geo(d), lx, ly, (int)d->zoom, &opt);
            uint32_t idx = id & 0xFFu;
            const struct RICtlDef *cd;
            d->diag.buttons++;
            d->diag.last_x = lx;
            d->diag.last_y = ly;
            d->diag.last_hit = id;
            if (im->Code == SELECTUP) {
                d->rep_idx = 0xFFFFu;
                if (d->drag_id == 0xFFFFu)
                    return (IPTR)0;
                d->drag_id = 0xFFFFu;
                return (IPTR)MUI_EventHandlerRC_Eat;
            }
            if (id == 0xFFFFu)
                return (IPTR)0;
            cd = ri_ctlreg_find((uint16_t)((d->ui.section << 8) | idx));
            if (!cd)
                return (IPTR)0;
            if (im->Code == MENUDOWN) {
                if (ri_sui_reset(&d->ui, idx))
                    changed(obj, d);
                return (IPTR)MUI_EventHandlerRC_Eat;
            }
            if (im->Code != SELECTDOWN)
                return (IPTR)0;
            if (opt == RI_GEO_HIT_UP || opt == RI_GEO_HIT_DOWN) {   /* value-display arrow */
                d->rep_idx = (uint16_t)idx;
                d->rep_dir = (int8_t)(opt == RI_GEO_HIT_UP ? 1 : -1);
                d->rep_ticks = 0;
                if (ri_sui_step(&d->ui, idx, d->rep_dir))
                    changed(obj, d);
            } else if (opt >= 0) {                            /* instrument legend */
                if (ri_sui_set(&d->ui, idx, opt))
                    changed(obj, d);
            } else if (cd->kind == RI_CK_KNOB || cd->kind == RI_CK_SELECTOR || cd->kind == RI_CK_FADER) {
                d->drag_id = id;
                d->drag_n0 = to_n(cd, ri_sui_value(&d->ui, idx));
                d->acc_dx = d->acc_dy = 0.0;
                d->last_x = im->MouseX;
                d->last_y = im->MouseY;
            } else if (ri_sui_press(&d->ui, idx)) {
                changed(obj, d);
            }
            return (IPTR)MUI_EventHandlerRC_Eat;
        }
        if (im->Class == IDCMP_MOUSEMOVE && d->drag_id != 0xFFFFu) {
            uint32_t idx = d->drag_id & 0xFFu;
            const struct RICtlDef *cd = ri_ctlreg_find((uint16_t)((d->ui.section << 8) | idx));
            int fine = (im->Qualifier & (IEQUALIFIER_LSHIFT | IEQUALIFIER_RSHIFT)) != 0;
            double n;
            d->acc_dx += (double)(im->MouseX - d->last_x);
            d->acc_dy += (double)(d->last_y - im->MouseY);
            d->last_x = im->MouseX;
            d->last_y = im->MouseY;
            ri_knob_clamp_acc((double)d->drag_n0, &d->acc_dx, &d->acc_dy, fine);
            n = ri_knob_drag_to_value((double)d->drag_n0, d->acc_dx, d->acc_dy, fine);
            if (cd && ri_sui_set(&d->ui, idx, from_n(cd, (LONG)ri_ctl_quantize(n))))
                changed(obj, d);
            return (IPTR)MUI_EventHandlerRC_Eat;
        }
        return (IPTR)0;
    }
    default:
        return DoSuperMethodA(cl, obj, msg);
    }
}
BOOPSI_DISPATCHER_END

static struct MUI_CustomClass *s_rsection_class = NULL;

struct MUI_CustomClass *ri_rsection_class(void) {
    if (!s_rsection_class)
        s_rsection_class = MUI_CreateCustomClass(NULL, MUIC_Area, NULL,
            sizeof(struct RSectionData), (APTR)rsection_dispatcher);
    return s_rsection_class;
}

void ri_rsection_dispose_class(void) {
    if (s_rsection_class) {
        MUI_DeleteCustomClass(s_rsection_class);
        s_rsection_class = NULL;
    }
}

APTR ri_rsection_create(ULONG section, LONG zoom) {
    struct MUI_CustomClass *mcc = ri_rsection_class();
    if (!mcc)
        return NULL;
    return (APTR)NewObject(mcc->mcc_Class, NULL,
        TAG_SECTION, section,
        TAG_ZOOM, zoom,
        MUIA_FillArea, FALSE,
        TAG_DONE);
}

void ri_rsection_refresh(APTR obj) {
    if (obj)
        MUI_Redraw((Object *)obj, MADF_DRAWOBJECT);
}
