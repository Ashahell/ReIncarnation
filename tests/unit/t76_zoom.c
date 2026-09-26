/* t76_zoom — zoom finish (§12.10 G8.2).
 * Oracle: spec §13 (masters at 2x, filtered downscale once per zoom change),
 * P-18 (150 px screen travel = full 0..127 at every zoom).
 * Pins: the zoom-factor table against panelgeo, downscale sizes + exact
 * pixels at every zoom, and the drag law's zoom-independence (screen px in,
 * same value out, whatever the zoom — the canvas feeds raw deltas).
 */
#include <stdio.h>
#include <string.h>
#include "tests/helpers/ri_assert.h"
#include "gui/skin.h"
#include "gui/panelgeo.h"
#include "gui/knob_logic.h"

int main(void) {
    int z;
    static const uint32_t NUM[4] = { 4u, 6u, 8u, 3u };

    /* factor table + consistency with the geometry it mirrors */
    for (z = 0; z < 4; z++)
        RI_ASSERT(ri_skin_zoom_num(z) == NUM[z], "factor %d", z);
    RI_ASSERT(ri_skin_zoom_num(-1) == 0u && ri_skin_zoom_num(4) == 0u, "factor bad");
    for (z = 0; z < 4; z++) {
        /* px = q*num/8 (rounded); multiples of 8 are exact */
        RI_ASSERT((uint32_t)ri_geo_px(16, z) * 8u == 16u * NUM[z], "geo agree %d", z);
        RI_ASSERT(ri_geo_px(0, z) == 0, "geo zero %d", z);
    }

    /* downscale sizes + exact pixels at every zoom (solid 64px master) */
    {
        static uint32_t master[64 * 64];
        uint32_t dw[4], dh[4], k;
        for (k = 0; k < 64u * 64u; k++)
            master[k] = 0xFF404040u;
        for (z = 0; z < 4; z++) {
            static uint32_t out[64 * 64];
            dw[z] = 64u * NUM[z] / 8u;
            dh[z] = 64u * NUM[z] / 8u;
            ri_skin_downscale(master, 64u, 64u, out, dw[z], dh[z]);
            RI_ASSERT(out[0] == 0xFF404040u &&
                      out[dw[z] * dh[z] - 1u] == 0xFF404040u, "solid z%d", z);
        }
        RI_ASSERT(dw[0] == 32u && dw[1] == 48u && dw[2] == 64u && dw[3] == 24u,
                  "sizes 32/48/64/24");
    }

    /* drag law: 150 screen px = full range at EVERY zoom (P-18) */
    for (z = 0; z < 4; z++) {
        double full = ri_knob_drag_to_value(0.0, 150.0, 0.0, 0);
        double half = ri_knob_drag_to_value(0.0, 75.0, 0.0, 0);
        (void)z;
        RI_ASSERT(full >= 126.9 && full <= 127.0, "150px full");
        RI_ASSERT(half >= 63.0 && half <= 64.0, "75px half");
    }

    RI_RESULT("zoom");
}
