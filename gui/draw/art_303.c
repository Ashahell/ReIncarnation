/* art_303.c — 303 background (portability plan T2). */
#include "gui/draw/art.h"

#include "gui/panelgeo.h"
#include "gui/sect303.h"

void ri_art_bg_303(struct ri_dlist *dl, const struct RIGeoSection *g, int ox, int oy, int z) {
    static const int black[13] = { 0, 1, 0, 1, 0, 0, 1, 0, 1, 0, 1, 0, 0 };
    uint32_t i, j;
#define PX(q) ri_geo_px((q), z)
    ri_art_rect(dl, ox, oy, ox + PX(g->w) - 1, oy + PX(g->h) - 1, C_PANEL);
    ri_art_line(dl, ox, oy + PX(185), ox + PX(g->w) - 1, oy + PX(185), C_PANEL_DK);
    /* keyboard block: an 8 Q black rim on every side (low C starts at 184 Q) */
    ri_art_rect(dl, ox + PX(176), oy + PX(200), ox + PX(858), oy + PX(435), C_BLACK);
    for (i = 0; i < 13; i++)
        for (j = 0; j < g->nitems; j++)
            if ((g->items[j].reg_id & 0xFFu) == RI_S303_KEY0 + i && g->items[j].shape == RI_GEO_RECT && !black[i])
                ri_art_rect(dl, ox + PX(g->items[j].cx - 38), oy + PX(208), ox + PX(g->items[j].cx + 38),
                    oy + PX(428), C_WHITEKEY);
    for (i = 0; i < 13; i++)
        for (j = 0; j < g->nitems; j++)
            if ((g->items[j].reg_id & 0xFFu) == RI_S303_KEY0 + i && g->items[j].shape == RI_GEO_RECT && black[i])
                ri_art_rect(dl, ox + PX(g->items[j].cx - 28), oy + PX(208), ox + PX(g->items[j].cx + 28),
                    oy + PX(320), C_BLACK);
    ri_art_rect(dl, ox + PX(880), oy + PX(290), ox + PX(1270), oy + PX(320), C_BLACK);
#undef PX
}
