/* art_mix.c — mixer/master background (portability plan T2). */
#include "gui/draw/art.h"

#include "gui/panelgeo.h"

void ri_art_bg_mix(struct ri_dlist *dl, const struct RIGeoSection *g, int ox, int oy, int z, int master) {
    static const char *const db[5] = { "CLIP", "-6", "-12", "-24", "-36" };
    static const int dby[5] = { 120, 158, 200, 240, 272 };
    int k;
#define PX(q) ri_geo_px((q), z)
    ri_art_panel(dl, ox, oy, ox + PX(g->w) - 1, oy + PX(g->h) - 1, ri_art_rgb(C_MIX_PANEL), 0);
    if (master) {
        ri_art_panel(dl, ox + PX(22), oy + PX(20), ox + PX(315), oy + PX(70), ri_art_rgb(C_MIX_HEAD), 0); /* moulded header strip */
        ri_art_text_c(dl, ox + PX(170), oy + PX(45), "MASTER", C_MIX_HEADTX);
        for (k = 0; k < 5; k++) {
            ri_art_text_c(dl, ox + PX(42), oy + PX(dby[k]), db[k], C_MIX_TEXT);
            ri_art_text_c(dl, ox + PX(292), oy + PX(dby[k]), db[k], C_MIX_TEXT);
        }
    } else {
        ri_art_panel(dl, ox + PX(10), oy + PX(10), ox + PX(274), oy + PX(82), ri_art_rgb(C_MIX_HEAD), 0); /* moulded header strip */
        ri_art_text_c(dl, ox + PX(142), oy + PX(45), "MIX", C_MIX_HEADTX);
        ri_art_text_c(dl, ox + PX(32), oy + PX(190), "L", C_MIX_TEXT);
        ri_art_text_c(dl, ox + PX(125), oy + PX(190), "R", C_MIX_TEXT);
        ri_art_text_c(dl, ox + PX(160), oy + PX(407), "0", C_MIX_TEXT);
        ri_art_text_c(dl, ox + PX(245), oy + PX(407), "10", C_MIX_TEXT);
    }
#undef PX
    /* slider scale (kept in bg_mix: colour depends on the caller). */
    if (master)
        for (k = 0; k < 8; k++)
            ri_art_line(dl, ox + ri_geo_px(128, z), oy + ri_geo_px(120 + 25 * k, z),
                ox + ri_geo_px(202, z), oy + ri_geo_px(120 + 25 * k, z), C_MIX_TEXT);
    else
        for (k = 0; k < 7; k++)
            ri_art_line(dl, ox + ri_geo_px(32, z), oy + ri_geo_px(262 + 28 * k, z),
                ox + ri_geo_px(118, z), oy + ri_geo_px(262 + 28 * k, z), C_MIX_TEXT);
}
