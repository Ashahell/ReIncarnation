/* t171_tr_layout — the transport plate keeps its own furniture apart
 * (owner Dell 2026-10-05: TAP sat on the button slot and covered the
 * PATTERN legend; the song name ran off the panel's left edge and over the
 * SHUFFLE legend). At every zoom: TAP's box clears every other control's
 * box and the button slot, and the plate draws no song name at all (it has
 * no strip that fits a text line at every zoom; RIAPP puts the name in the
 * window title). Every plate text stays inside the panel.
 */
#include <stdio.h>
#include <string.h>
#include "tests/helpers/ri_assert.h"
#include "gui/sectui.h"
#include "gui/panelgeo.h"
#include "gui/secttr.h"
#include "gui/ctlreg.h"
#include "gui/draw/canvas.h"
#include "gui/draw/art.h"
#include "platform/host/raster.h"

static struct ri_dcmd back[16384];
static char spool[65536];

static int overlap(int a0, int b0, int a1, int b1, int c0, int d0, int c1, int d1) {
    return !(a1 < c0 || c1 < a0 || b1 < d0 || d1 < b0);
}

static void check_zoom(int z, const char *song) {
    const struct RIGeoSection *g = ri_geo_section(RI_SEC_TRANSPORT);
    struct RISectUI ui;
    struct ri_dlist dl;
    struct ri_text_metrics tm;
    int sx0 = ri_geo_px(384, z), sy0 = ri_geo_px(94, z);
    int sx1 = ri_geo_px(1028, z), sy1 = ri_geo_px(180, z);
    int w = ri_geo_px(g->w, z), h = ri_geo_px(g->h, z);
    int tx0 = 0, ty0 = 0, tx1 = -1, ty1 = -1, have_tap = 0, found = 0;
    uint32_t i, k;
    const uint16_t tap = (uint16_t)((RI_SEC_TRANSPORT << 8) | RI_STR_TAP);
    for (i = 0u; i < g->nitems; i++)
        if (g->items[i].reg_id == tap &&
            ri_geo_item_box(&g->items[i], z, &tx0, &ty0, &tx1, &ty1) == 0)
            have_tap = 1;
    RI_ASSERT(have_tap, "z=%d: TAP has a box", z);
    RI_ASSERT(!overlap(tx0, ty0, tx1, ty1, sx0, sy0, sx1, sy1), "z=%d: TAP clears the button slot", z);
    for (i = 0u; i < g->nitems; i++) {
        int a0, b0, a1, b1;
        if (g->items[i].reg_id == tap || ri_geo_item_box(&g->items[i], z, &a0, &b0, &a1, &b1) != 0)
            continue;
        RI_ASSERT(!overlap(tx0, ty0, tx1, ty1, a0, b0, a1, b1),
            "z=%d: TAP overlaps item %u (reg %04x)", z, i, g->items[i].reg_id);
    }
    ri_art_tr_set_song(song);
    ri_sui_init(&ui, RI_SEC_TRANSPORT);
    ri_dlist_init(&dl, back, 16384u, spool, sizeof spool);
    tm.width = ri_raster_text_width;
    tm.height = 7;
    tm.baseline = 5;
    tm.ctx = 0;
    ri_draw_section(&dl, &ui, RI_SEC_TRANSPORT, z, 0, 0, &tm, 0, 0);
    for (k = 0u; k < dl.n; k++) {
        const struct ri_dcmd *c = &dl.cmd[k];
        int a0, b0, a1, b1;
        if (c->op != RI_D_TEXT || !c->text)
            continue;
        RI_ASSERT(strstr(c->text, "zombie") == NULL && strstr(c->text, "extremely") == NULL &&
            strstr(c->text, "...") == NULL, "z=%d: the plate draws the song name ('%s')", z, c->text);
        if (ri_dcmd_bbox(c, &a0, &b0, &a1, &b1) != 0)
            continue;
        found++;
        RI_ASSERT(a0 >= 0 && b0 >= 0 && a1 < w && b1 < h,
            "z=%d: text '%s' inside the panel (%d,%d)-(%d,%d) in %dx%d", z, c->text, a0, b0, a1, b1, w, h);
        RI_ASSERT(strcmp(c->text, "TAP") == 0 || !overlap(a0, b0, a1, b1, tx0, ty0, tx1, ty1), "z=%d: TAP (%d,%d)-(%d,%d) covers text %s (%d,%d)-(%d,%d)", z, tx0, ty0, tx1, ty1, c->text, a0, b0, a1, b1);
    }
    RI_ASSERT(found > 0, "z=%d: the plate has legends to check", z);
}

int main(void) {
    static const char *names[2] = {
        "zombie-nation.rbng",
        "an-extremely-long-song-file-name-that-must-be-shortened.rbng" };
    int z, n;
    for (n = 0; n < 2; n++)
        for (z = 0; z <= 3; z++)
            check_zoom(z, names[n]);
    ri_art_tr_set_song(NULL);
    RI_RESULT("tr_layout");
}
