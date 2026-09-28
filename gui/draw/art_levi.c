/* art_levi.c — Levi voice canvas background (owner 2026-09-28).
 * Dark slate panel, 303-position piano keyboard block, 909-style step
 * number plates, lane option numerals. Control states (LEDs, knob
 * faces, step fills) render generically from the geometry + UI state.
 */
#include "gui/draw/art.h"

#include "gui/panelgeo.h"
#include "gui/sectlevi.h"

static const int black[13] = { 0, 1, 0, 1, 0, 0, 1, 0, 1, 0, 1, 0, 0 };

void ri_art_bg_levi(struct ri_dlist *dl, const struct RIGeoSection *g, int ox, int oy, int z) {
    uint32_t i, j;
    char n[3];
    static const char *const lanes[6] = { "1", "2", "3", "4", "5", "6" };
#define PX(q) ri_geo_px((q), z)
    ri_art_rect(dl, ox, oy, ox + PX(g->w) - 1, oy + PX(g->h) - 1, C_LEVI_PANEL);
    /* Piano keyboard block (303 block geometry, Levi registry ids). */
    ri_art_rect(dl, ox + PX(176), oy + PX(200), ox + PX(858), oy + PX(435), C_BLACK);
    for (i = 0; i < 13; i++)
        for (j = 0; j < g->nitems; j++)
            if ((g->items[j].reg_id & 0xFFu) == RI_SLEVI_KEY0 + i && g->items[j].shape == RI_GEO_RECT && !black[i])
                ri_art_rect(dl, ox + PX(g->items[j].cx - 38), oy + PX(208), ox + PX(g->items[j].cx + 38),
                    oy + PX(428), C_WHITEKEY);
    for (i = 0; i < 13; i++)
        for (j = 0; j < g->nitems; j++)
            if ((g->items[j].reg_id & 0xFFu) == RI_SLEVI_KEY0 + i && g->items[j].shape == RI_GEO_RECT && black[i])
                ri_art_rect(dl, ox + PX(g->items[j].cx - 28), oy + PX(208), ox + PX(g->items[j].cx + 28),
                    oy + PX(320), C_BLACK);
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
    /* Lane option numerals above the select row. */
    for (i = 0; i < 6; i++)
        ri_art_text_c(dl, ox + PX(480 + 70 * (int)i), oy + PX(60), lanes[i], C_CREAM);
    /* Algo block: value numerals near each option row (lane idiom).
     * Rows sit right of the step plates; labels are geometry legends. */
    for (i = 0; i < 8; i++) {
        n[0] = (char)('0' + i);
        n[1] = 0;
        ri_art_text_c(dl, ox + PX(950 + 62 * (int)i), oy + PX(123), n, C_CREAM);
        ri_art_text_c(dl, ox + PX(950 + 62 * (int)i), oy + PX(394), n, C_CREAM);
        ri_art_text_c(dl, ox + PX(950 + 62 * (int)i), oy + PX(434), n, C_CREAM);
    }
    for (i = 0; i < 7; i++) {
        n[0] = (char)('0' + i);
        n[1] = 0;
        ri_art_text_c(dl, ox + PX(450 + 62 * (int)i), oy + PX(162), n, C_CREAM);
    }
    for (i = 0; i < 4; i++) {
        n[0] = (char)('0' + i);
        n[1] = 0;
        ri_art_text_c(dl, ox + PX(450 + 62 * (int)i), oy + PX(188), n, C_CREAM);
    }
#undef PX
}
