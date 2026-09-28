/* art_808.c — 808 background (portability plan T2). */
#include "gui/draw/art.h"

#include "gui/panelgeo.h"

int ri_art_step_colour_808(uint32_t step) {
    return step < 4 ? C_STEP_RED : step < 8 ? C_STEP_ORANGE : step < 12 ? C_STEP_YELLOW : C_STEP_WHITE;
}

static const char *const ART_808_OPT[12] = {
    "AC", "BD", "SD", "LT", "MT", "HT", "RS", "CP", "CB", "CY", "OH", "CH"
};

const char *ri_art_808_opt(uint32_t opt) {
    return opt < 12 ? ART_808_OPT[opt] : "?";
}

void ri_art_bg_808(struct ri_dlist *dl, const struct RIGeoSection *g, int ox, int oy, int z) {
#define PX(q) ri_geo_px((q), z)
    ri_art_panel(dl, ox, oy, ox + PX(g->w) - 1, oy + PX(g->h) - 1, ri_art_rgb(C_808_PANEL), 0);
    {   /* corner screws (the plate is bolted to the case) */
        int sr = PX(8), m = PX(14);
        uint32_t pc = ri_art_rgb(C_808_PANEL);
        ri_art_screw(dl, ox + m, oy + m, sr, pc);
        ri_art_screw(dl, ox + PX(g->w) - 1 - m, oy + m, sr, pc);
        ri_art_screw(dl, ox + m, oy + PX(g->h) - 1 - m, sr, pc);
        ri_art_screw(dl, ox + PX(g->w) - 1 - m, oy + PX(g->h) - 1 - m, sr, pc);
    }
    ri_art_line(dl, ox + PX(1110), oy + PX(10), ox + PX(1110), oy + PX(340), C_808_LINE);
    ri_art_line(dl, ox + PX(40), oy + PX(345), ox + PX(1100), oy + PX(345), C_808_LINE);
#undef PX
}
