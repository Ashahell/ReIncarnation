/* art_levi.c — Levi voice canvas background (owner 2026-09-28).
 * Hardware-family front panel (photo verdict): near-black own panel,
 * amber section title bars with steel divider rules (909-bar idiom),
 * 303-position piano keyboard block, 909-style step number plates.
 * Option numerals render inside their boxes via art_section (909-plate
 * idiom). Control states (LEDs, knob faces, step fills) render
 * generically from the geometry + UI state. Own words only: no ASM
 * marks, no Leviasynth wordmark, house font path.
 */
#include "gui/draw/art.h"

#include "gui/panelgeo.h"
#include "gui/sectlevi.h"

static const int black[13] = { 0, 1, 0, 1, 0, 0, 1, 0, 1, 0, 1, 0, 0 };

/* Top-band section title bars: hardware block order left to right. */
static const struct { int x0, x1; const char *t; } LEVI_BAR[] = {
    { 40, 350, "MODULE" },
    { 360, 640, "OSC" },
    { 660, 990, "ALGORITHM" },
    { 1010, 1220, "DIGITAL FILTER" },
};

void ri_art_bg_levi(struct ri_dlist *dl, const struct RIGeoSection *g, int ox, int oy, int z) {
    uint32_t i, j;
    char n[3];
#define PX(q) ri_geo_px((q), z)
    ri_art_rect(dl, ox, oy, ox + PX(g->w) - 1, oy + PX(g->h) - 1, C_LEVI_PANEL);
    /* Section title bars (909-bar idiom, hardware block order). */
    {
        static const struct { int x0, x1; const char *t; } bars[] = {
            { 40, 350, "MODULE" }, { 360, 640, "OSC" }, { 660, 990, "ALGORITHM" },
            { 1010, 1220, "DIGITAL FILTER" },
        };
        uint32_t b;
        for (b = 0u; b < sizeof(bars) / sizeof(bars[0]); b++) {
            ri_art_panel(dl, ox + PX(bars[b].x0), oy + PX(26), ox + PX(bars[b].x1), oy + PX(56),
                ri_art_rgb(C_LEVI_RULE), 0);
            ri_art_text_c(dl, ox + PX((bars[b].x0 + bars[b].x1) / 2), oy + PX(41), bars[b].t,
                C_LEVI_HEAD);
        }
        ri_art_text_c(dl, ox + PX(1430), oy + PX(41), "LEVI", C_LEVI_HEAD);
        ri_art_line(dl, ox + PX(350), oy + PX(56), ox + PX(350), oy + PX(190), C_LEVI_RULE);
        ri_art_line(dl, ox + PX(650), oy + PX(56), ox + PX(650), oy + PX(190), C_LEVI_RULE);
        ri_art_line(dl, ox + PX(1000), oy + PX(56), ox + PX(1000), oy + PX(190), C_LEVI_RULE);
    }
    for (i = 0; i < sizeof(LEVI_BAR) / sizeof(LEVI_BAR[0]); i++) {
        int x0 = LEVI_BAR[i].x0, x1 = LEVI_BAR[i].x1;
        ri_art_panel(dl, ox + PX(x0), oy + PX(26), ox + PX(x1), oy + PX(56), ri_art_rgb(C_LEVI_RULE), 0);
        ri_art_text_c(dl, ox + PX((x0 + x1) / 2), oy + PX(41), LEVI_BAR[i].t, C_LEVI_HEAD);
    }
    ri_art_text_c(dl, ox + PX(1430), oy + PX(41), "LEVI", C_LEVI_HEAD);
    ri_art_line(dl, ox + PX(350), oy + PX(56), ox + PX(350), oy + PX(190), C_LEVI_RULE);
    ri_art_line(dl, ox + PX(650), oy + PX(56), ox + PX(650), oy + PX(190), C_LEVI_RULE);
    ri_art_line(dl, ox + PX(1000), oy + PX(56), ox + PX(1000), oy + PX(190), C_LEVI_RULE);
    /* Piano keyboard section (owner 2026-09-29 photo verdict): labeled
     * KEYBOARD block, black row above the white row (geometry rows:
     * black cy=335 h=60, white cy=380 h=90). */
    ri_art_panel(dl, ox + PX(40), oy + PX(272), ox + PX(880), oy + PX(298),
        ri_art_rgb(C_LEVI_RULE), 0);
    ri_art_text_c(dl, ox + PX(460), oy + PX(285), "KEYBOARD", C_LEVI_HEAD);
    ri_art_rect(dl, ox + PX(40), oy + PX(300), ox + PX(880), oy + PX(435), C_BLACK);
    for (i = 0; i < 13; i++)
        for (j = 0; j < g->nitems; j++)
            if ((g->items[j].reg_id & 0xFFu) == RI_SLEVI_KEY0 + i && g->items[j].shape == RI_GEO_RECT && !black[i])
                ri_art_rect(dl, ox + PX(g->items[j].cx - 38), oy + PX(335), ox + PX(g->items[j].cx + 38),
                    oy + PX(425), C_WHITEKEY);
    for (i = 0; i < 13; i++)
        for (j = 0; j < g->nitems; j++)
            if ((g->items[j].reg_id & 0xFFu) == RI_SLEVI_KEY0 + i && g->items[j].shape == RI_GEO_RECT && black[i])
                ri_art_rect(dl, ox + PX(g->items[j].cx - 28), oy + PX(305), ox + PX(g->items[j].cx + 28),
                    oy + PX(365), C_BLACK);
    /* Step number plates (909 idiom, Levi step rows y=200/300). */
    for (i = 0; i < 16; i++) {
        int cx = 900 + 70 * (int)(i % 8u);
        int cy = (i < 8u) ? 200 : 300;
        n[0] = (char)(i >= 9 ? '1' : '0' + (i + 1));
        n[1] = (char)(i >= 9 ? '0' + (i + 1 - 10) : 0);
        n[2] = 0;
        ri_art_rect(dl, ox + PX(cx - 28), oy + PX(cy + 38), ox + PX(cx + 28), oy + PX(cy + 62), C_BLACK);
        ri_art_text_c(dl, ox + PX(cx), oy + PX(cy + 50), n, C_CREAM);
    }
    /* Bottom voice-strip block titles (hardware order). */
    ri_art_text_c(dl, ox + PX(1095), oy + PX(452), "ANALOG FILTER", C_LEVI_HEAD);
    ri_art_text_c(dl, ox + PX(1300), oy + PX(452), "ENVELOPE", C_LEVI_HEAD);
    ri_art_line(dl, ox + PX(1165), oy + PX(460), ox + PX(1165), oy + PX(540), C_LEVI_RULE);
    ri_art_text_c(dl, ox + PX(160), oy + PX(452), "ARP", C_LEVI_HEAD);
    ri_art_text_c(dl, ox + PX(380), oy + PX(452), "SEQ", C_LEVI_HEAD);
    ri_art_text_c(dl, ox + PX(660), oy + PX(452), "MATRIX", C_LEVI_HEAD);
    ri_art_text_c(dl, ox + PX(950), oy + PX(452), "FX", C_LEVI_HEAD);
    ri_art_line(dl, ox + PX(270), oy + PX(460), ox + PX(270), oy + PX(540), C_LEVI_RULE);
    ri_art_line(dl, ox + PX(470), oy + PX(460), ox + PX(470), oy + PX(540), C_LEVI_RULE);
#undef PX
}
