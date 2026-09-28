/* art_tr.c — transport background + anchored text (portability plan T2). */
#include "gui/draw/art.h"

#include <string.h>
#include "gui/draw/font_legend.h"
#include "gui/panelgeo.h"

void ri_art_text_at(struct ri_dlist *dl, int x, int cy, const char *t, int col, int align,
    const struct ri_text_metrics *tm) {
    int w = 0;
    if (!t)
        return;
    if (dl && dl->cur_face) {
        /* Face metrics (S2): identical on host and AROS by construction. */
        const struct ri_face *f = ri_face_by_id(dl->cur_face);
        if (f)
            w = ri_face_width(f, t);
    } else if (tm && tm->width)
        w = tm->width(tm->ctx, t);
    /* Without metrics the backend centres at x (documented fallback;
     * AROS always passes metrics, keeping the old TextLength behaviour). */
    ri_draw_text(dl, align < 0 ? x - w / 2 : align > 0 ? x + w / 2 : x, cy, 1u,
        ri_art_rgb(col), t);
}

void ri_art_bg_tr(struct ri_dlist *dl, const struct RIGeoSection *g, int ox, int oy, int z,
    const struct ri_text_metrics *tm) {
#define PX(q) ri_geo_px((q), z)
    ri_art_panel(dl, ox, oy, ox + PX(g->w) - 1, oy + PX(g->h) - 1, ri_art_rgb(C_TR_PANEL), 0);
    ri_art_text_c(dl, ox + PX(25), oy + PX(150), "0", C_MIX_TEXT);
    ri_art_text_c(dl, ox + PX(132), oy + PX(150), "10", C_MIX_TEXT);
    ri_art_text_c(dl, ox + PX(242), oy + PX(40), "SYNC", C_MIX_TEXT);
    ri_art_text_c(dl, ox + PX(372), oy + PX(40), "MIDI", C_MIX_TEXT);
    ri_art_text_at(dl, ox + PX(642), oy + PX(45), "PATTERN", C_MIX_TEXT, -1, tm);
    ri_art_text_at(dl, ox + PX(740), oy + PX(45), "SONG MODE", C_MIX_TEXT, 1, tm);
    ri_art_rect(dl, ox + PX(384), oy + PX(94), ox + PX(1028), oy + PX(180), C_MIX_SLOT);
    ri_art_line(dl, ox + PX(1330), oy + PX(38), ox + PX(1400), oy + PX(38), C_MIX_TEXT);
    ri_art_text_c(dl, ox + PX(1444), oy + PX(38), "LOOP", C_MIX_TEXT);
    ri_art_line(dl, ox + PX(1488), oy + PX(38), ox + PX(1560), oy + PX(38), C_MIX_TEXT);
#undef PX
}
