/* art_fx.c — FX background (portability plan T2). */
#include "gui/draw/art.h"

#include "gui/ctlreg.h"
#include "gui/panelgeo.h"
#include "gui/sectfx.h"

void ri_art_bg_fx(struct ri_dlist *dl, const struct RIGeoSection *g, int ox, int oy, int z, uint8_t sec) {
    static const char *const title[4] = { "PCF", "DELAY", "DIST", "COMP" };
    int k, x1 = g->w - 17;
#define PX(q) ri_geo_px((q), z)
    ri_art_rect(dl, ox, oy, ox + PX(g->w) - 1, oy + PX(g->h) - 1, C_FX_PANEL);
    ri_art_rect(dl, ox + PX(17), oy + PX(17), ox + PX(x1), oy + PX(70), C_FX_HEAD);
    ri_art_text_c(dl, ox + PX(g->w / 2), oy + PX(43), title[sec - RI_SEC_PCF], C_MIX_TEXT);
    if (sec == RI_SEC_PCF) {
        ri_art_text_c(dl, ox + PX(268), oy + PX(105), "BP", C_MIX_TEXT);
        ri_art_text_c(dl, ox + PX(268), oy + PX(145), "LP", C_MIX_TEXT);
        for (k = 0; k < 6; k++)                         /* slider scale */
            ri_art_line(dl, ox + PX(18), oy + PX(234 + 26 * k), ox + PX(314), oy + PX(234 + 26 * k), C_MIX_TEXT);
    } else if (sec == RI_SEC_DELAY) {
        ri_art_text_c(dl, ox + PX(262), oy + PX(92), "3", C_MIX_TEXT);  /* 8th-note triplet */
        ri_art_note_glyph(dl, ox + PX(258), oy + PX(122), 1, z);
        ri_art_note_glyph(dl, ox + PX(258), oy + PX(160), 2, z);        /* 16th note */
        ri_art_text_c(dl, ox + PX(40), oy + PX(315), "L", C_MIX_TEXT);
        ri_art_text_c(dl, ox + PX(133), oy + PX(315), "R", C_MIX_TEXT);
        ri_art_text_c(dl, ox + PX(205), oy + PX(315), "0", C_MIX_TEXT);
        ri_art_text_c(dl, ox + PX(292), oy + PX(315), "10", C_MIX_TEXT);
    } else if (sec == RI_SEC_DIST) {
        for (k = 0; k < 2; k++) {
            ri_art_text_c(dl, ox + PX(k ? 208 : 46), oy + PX(202), "0", C_MIX_TEXT);
            ri_art_text_c(dl, ox + PX(k ? 294 : 132), oy + PX(202), "10", C_MIX_TEXT);
        }
    } else {
        for (k = 0; k < 9; k++)                         /* reduction scale, 0 in the middle */
            ri_art_line(dl, ox + PX(48 + 28 * k), oy + PX(136), ox + PX(48 + 28 * k), oy + PX(142), C_MIX_TEXT);
        ri_art_text_c(dl, ox + PX(160), oy + PX(156), "0", C_MIX_TEXT);
        for (k = 0; k < 2; k++) {
            ri_art_text_c(dl, ox + PX(k ? 206 : 46), oy + PX(312), "0", C_MIX_TEXT);
            ri_art_text_c(dl, ox + PX(k ? 292 : 132), oy + PX(312), "10", C_MIX_TEXT);
        }
    }
#undef PX
}
