/* art_pat.c — pattern background + focus bar (portability plan T2). */
#include "gui/draw/art.h"

#include "gui/panelgeo.h"
#include "gui/panelui.h"

void ri_art_bg_pat(struct ri_dlist *dl, const struct RIGeoSection *g, int ox, int oy, int z) {
#define PX(q) ri_geo_px((q), z)
    ri_art_rect(dl, ox, oy, ox + PX(g->w) - 1, oy + PX(g->h) - 1, C_FX_PANEL);
    ri_art_rect(dl, ox + PX(20), oy + PX(17), ox + PX(262), oy + PX(72), C_PAT_HEAD);
    ri_art_text_c(dl, ox + PX(158), oy + PX(45), "PATTERN", C_MIX_TEXT);
    ri_art_rect(dl, ox + PX(18), oy + PX(92), ox + PX(262), oy + PX(212), C_MIX_SLOT);
    ri_art_rect(dl, ox + PX(18), oy + PX(258), ox + PX(262), oy + PX(318), C_MIX_SLOT);
#undef PX
}

void ri_art_focus_bar(struct ri_dlist *dl, uint8_t section,
    const struct RIPanelUI *panel, int ox, int oy, int z) {
    struct RIGeoItem fb;
    int f, cx, cy, hw, hh;
    if (!panel)
        return;
    f = ri_panel_focus_of(section);
    if (f < 0 || ri_geo_focus_bar(section, &fb) != 0)
        return;
    cx = ox + ri_geo_px(fb.cx, z);
    cy = oy + ri_geo_px(fb.cy, z);
    hw = ri_geo_px(fb.w, z) / 2;
    hh = ri_geo_px(fb.h, z) / 2;
    /* orange when this section has the focus (p. 22), a dark groove otherwise */
    ri_art_rect(dl, cx - hw, cy - hh, cx + hw, cy + hh,
        panel->focus == (uint8_t)f ? C_909_ORANGE : C_MIX_SLOT);
}
