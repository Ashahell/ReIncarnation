/* art_303.c — 303 background (portability plan T2). */
#include "gui/draw/art.h"

#include "gui/panelgeo.h"
#include "gui/sect303.h"

void ri_art_bg_303(struct ri_dlist *dl, const struct RIGeoSection *g, int ox, int oy, int z) {
    static const int black[13] = { 0, 1, 0, 1, 0, 0, 1, 0, 1, 0, 1, 0, 0 };
    uint32_t i, j;
#define PX(q) ri_geo_px((q), z)
    ri_art_panel(dl, ox, oy, ox + PX(g->w) - 1, oy + PX(g->h) - 1, ri_art_rgb(C_PANEL), 1); /* brushed aluminium */
    {   /* corner screws (the plate is bolted to the case) */
        int sr = PX(8), m = PX(14);
        uint32_t pc = ri_art_rgb(C_PANEL);
        ri_art_screw(dl, ox + m, oy + m, sr, pc);
        ri_art_screw(dl, ox + PX(g->w) - 1 - m, oy + m, sr, pc);
        ri_art_screw(dl, ox + m, oy + PX(g->h) - 1 - m, sr, pc);
        ri_art_screw(dl, ox + PX(g->w) - 1 - m, oy + PX(g->h) - 1 - m, sr, pc);
    }
    ri_art_line(dl, ox, oy + PX(185), ox + PX(g->w) - 1, oy + PX(185), C_PANEL_DK);
    /* keyboard block: an 8 Q black rim on every side (low C starts at 184 Q) */
    ri_art_rect(dl, ox + PX(176), oy + PX(200), ox + PX(858), oy + PX(435), C_BLACK);
    for (i = 0; i < 13; i++)
        for (j = 0; j < g->nitems; j++)
            if ((g->items[j].reg_id & 0xFFu) == RI_S303_KEY0 + i && g->items[j].shape == RI_GEO_RECT && !black[i])
            {
                int kx0 = ox + PX(g->items[j].cx - 38), kx1 = ox + PX(g->items[j].cx + 38);
                int ky0 = oy + PX(208), ky1 = oy + PX(428), y, st = PX(8) > 0 ? PX(8) : 1;
                uint32_t wk = ri_art_rgb(C_WHITEKEY);
                for (y = ky0; y <= ky1; y += st)      /* ivory: faint grade, darker toward the lip */
                    ri_draw_rect(dl, kx0, y, kx1, y + st - 1 > ky1 ? ky1 : y + st - 1,
                        ri_art_mix(wk, ri_art_shade(wk, -12), (y - ky0) * 256 / (ky1 - ky0 + 1)));
                ri_draw_line(dl, kx0, ky1, kx1, ky1, ri_art_shade(wk, -40));
                ri_draw_line(dl, kx1, ky0, kx1, ky1, ri_art_shade(wk, -22));
            }
    for (i = 0; i < 13; i++)
        for (j = 0; j < g->nitems; j++)
            if ((g->items[j].reg_id & 0xFFu) == RI_S303_KEY0 + i && g->items[j].shape == RI_GEO_RECT && black[i])
            {
                int kx0 = ox + PX(g->items[j].cx - 28), kx1 = ox + PX(g->items[j].cx + 28);
                int ky0 = oy + PX(208), ky1 = oy + PX(320), y, st = PX(6) > 0 ? PX(6) : 1;
                for (y = ky0; y <= ky1; y += st)      /* ebony: glossy top, deep bottom */
                    ri_draw_rect(dl, kx0, y, kx1, y + st - 1 > ky1 ? ky1 : y + st - 1,
                        ri_art_mix(0x3A3A3Au, 0x0C0C0Cu, (y - ky0) * 256 / (ky1 - ky0 + 1)));
                ri_draw_line(dl, kx0 + 1, ky0 + 1, kx0 + 1, ky1 - 1, 0x4A4A4Au);
            }
    ri_art_rect(dl, ox + PX(880), oy + PX(290), ox + PX(1270), oy + PX(320), C_BLACK);
#undef PX
}
