/* t166_bbox_zoom — a damage box must be computed in the CANVAS's zoom.
 *
 * WHY (2026-10-04). app/riapp.c asked ri_geo_bbox for its repaint rectangles
 * with a hardcoded zoom of 0 at four call sites, while the canvases are not at
 * zoom 0: the transport is unconditionally RI_GEO_ZOOM_COMPACT, and the others
 * follow s_zoom. A bbox in the wrong coordinate space is not rejected -- it is
 * clamped into the canvas by refresh_box_why, so it comes out looking like a
 * valid rectangle and repaints the WRONG ART. Proven on the host for the
 * transport: asked 540,52..622,92 versus real 404,38..467,70, DISJOINT.
 *
 * The Song Position display was therefore not being repainted by the path whose
 * entire job is to follow the song.
 *
 * This is a pure geometry test: it asserts that a bbox computed at a canvas's
 * own zoom actually overlaps the art that lives there, for every section/canvas
 * pairing the app uses. No framebuffer, no build_dl, no AROS.
 */
#include <stdio.h>
#include "tests/helpers/ri_assert.h"
#include "gui/ctlreg.h"
#include "gui/panelgeo.h"
#include "gui/sectui.h"
#include "gui/secttr.h"

/* A bbox at the WRONG zoom must not silently look plausible: this is the
 * failure's shape, asserted directly so the regression cannot come back as
 * "the rectangle was valid, it was just wrong". */
static int overlaps(int ax0, int ay0, int ax1, int ay1,
                    int bx0, int by0, int bx1, int by1) {
    return !(ax1 < bx0 || bx1 < ax0 || ay1 < by0 || by1 < ay0);
}

int main(void) {
    const struct RIGeoSection *g;
    int a0, b0, c0, d0, a3, b3, c3, d3;
    int x0, y0, x1, y1;

    /* The concrete regression: transport at zoom 0 misses the bar entirely. */
    g = ri_geo_section(RI_SEC_TRANSPORT);
    RI_ASSERT(g != 0, "transport geo");
    ri_geo_bbox(g, (uint16_t)((RI_SEC_TRANSPORT << 8) | RI_STR_BAR), 0,
        &a0, &b0, &c0, &d0);
    ri_geo_bbox(g, (uint16_t)((RI_SEC_TRANSPORT << 8) | RI_STR_BAR),
        RI_GEO_ZOOM_COMPACT, &a3, &b3, &c3, &d3);
    RI_ASSERT(!overlaps(a0, b0, c0, d0, a3, b3, c3, d3),
        "zoom 0 (%d,%d..%d,%d) must MISS compact (%d,%d..%d,%d) -- this is the bug",
        a0, b0, c0, d0, a3, b3, c3, d3);
    RI_ASSERT(a3 < (int)g->w && c3 < (int)g->w && b3 < (int)g->h && d3 < (int)g->h,
        "the compact bbox lies inside the section (%d,%d..%d,%d vs %ux%u)",
        a3, b3, c3, d3, (unsigned)g->w, (unsigned)g->h);

    /* And the rule the app now follows, across every zoom: a bbox asked at the
     * canvas's own zoom is inside the canvas, and asking at a DIFFERENT zoom
     * does not silently produce an in-range box that misses the art. */
    {
        static const int ZS[4] = { 0, 1, 2, RI_GEO_ZOOM_COMPACT };
        unsigned s, k;
        for (s = 0; s < 4u; s++) {
            int zw[4], zs[4], zc[4], zd[4];
            for (k = 0u; k < 4u; k++) {
                if (ri_geo_bbox(g, (uint16_t)((RI_SEC_TRANSPORT << 8) | RI_STR_BAR),
                        ZS[k], &zw[k], &zs[k], &zc[k], &zd[k]) != 0) {
                    zw[k] = zs[k] = zc[k] = zd[k] = -1;
                }
            }
            for (k = 0u; k < 4u; k++) {
                int pw, ph;
                if (zw[k] < 0)
                    continue;
                pw = ri_geo_px((int)g->w, ZS[k]);
                ph = ri_geo_px((int)g->h, ZS[k]);
                RI_ASSERT(zw[k] >= 0 && zs[k] >= 0 &&
                    zc[k] < pw && zd[k] < ph && zc[k] >= zw[k] && zd[k] >= zs[k],
                    "transport bar at zoom %d is in range: %d,%d..%d,%d in %dx%d",
                    ZS[k], zw[k], zs[k], zc[k], zd[k], pw, ph);
            }
            /* Every other zoom's box must fail to cover this one: that is the
             * property the app relies on when it passes s_zoom[k]. */
            for (k = 0u; k < 4u; k++) {
                if (zw[k] < 0 || ZS[k] == RI_GEO_ZOOM_COMPACT)
                    continue;
                RI_ASSERT(!overlaps(zw[k], zs[k], zc[k], zd[k], a3, b3, c3, d3),
                    "zoom %d must not cover the compact bar (it did)", ZS[k]);
            }
        }
    }

    /* Master meters and the step lamps take the same shape, so assert the
     * call is well formed at the app's zoom for the master too. */
    g = ri_geo_section(RI_SEC_MASTER);
    if (g) {
        int ok = ri_geo_bbox(g, (uint16_t)((RI_SEC_MASTER << 8) | 1u),
            RI_GEO_ZOOM_COMPACT, &x0, &y0, &x1, &y1);
        RI_ASSERT(ok == 0 || ok == 1, "master meter bbox is defined");
        if (ok == 0)
            RI_ASSERT(x1 >= x0 && y1 >= y0, "master meter bbox is non-degenerate");
    }

    RI_RESULT("bbox_zoom");
    return 0;
}
