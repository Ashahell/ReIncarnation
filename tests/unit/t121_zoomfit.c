/* t121_zoomfit — S5 auto-fit zoom (2026-09-29).
 * Pure choice from geometry + app layout: largest 2/1/0 fitting the
 * screen, fail-closed 0. Hand-computed pins (Q/2 at z0, 3Q/4 at z1):
 * - Mix page binds width everywhere: 5x142 + 166 + 24 seams + 6 gap +
 *   36 rails = 942, +12 root inner = 954 content.
 * - Dell 1366x768: z1 needs 1392+12 > 1366 (Mix width) -> 0.
 * - riqemu1 1280x1024: z1 width fails -> 0.
 * - 800x600: nothing fits -> 0 fallback.
 * - 1920x1080: z1 fits (1404x~932); z2 fails height (drums 940) -> 1.
 * - 2560x1440: z2 fits (1830x~1168) -> 2.
 * Mutants: rail width 0 (z1 fits Dell), chrome +600 (1920 drops to 0).
 */
#include <stdio.h>
#include <string.h>
#include "tests/helpers/ri_assert.h"
#include "gui/zoomfit.h"

int main(void) {
    int w = 0, h = 0;
    RI_ASSERT(ri_zoomfit_content(0, &w, &h) == 0, "content ok");
    RI_ASSERT(w == 954, "content w0 %d", w);
    RI_ASSERT(h == 634, "content h0 %d", h);
    RI_ASSERT(ri_zoomfit_content(1, &w, &h) == 0, "content ok");
    RI_ASSERT(w == 1392, "content w1 %d", w);
    RI_ASSERT(ri_zoomfit_content(2, &w, &h) == 0, "content ok");
    RI_ASSERT(w == 1830, "content w2 %d", w);
    RI_ASSERT(ri_zoomfit_content(3, &w, &h) == 2, "content bad zoom");
    RI_ASSERT(ri_zoomfit_content(0, 0, &h) == 2, "content null");
    RI_ASSERT(ri_zoom_fit(1366, 768, 24, 64) == 0, "dell fit");
    RI_ASSERT(ri_zoom_fit(1280, 1024, 24, 64) == 0, "qemu fit");
    RI_ASSERT(ri_zoom_fit(800, 600, 24, 64) == 0, "small fallback");
    RI_ASSERT(ri_zoom_fit(1920, 1080, 24, 64) == 1, "hd fit");
    RI_ASSERT(ri_zoom_fit(2560, 1440, 24, 64) == 2, "qhd fit");
    RI_ASSERT(ri_zoom_fit(1920, 1080, 600, 64) == 0, "chrome binds");
    RI_ASSERT(ri_zoom_fit(0, 768, 24, 64) == 0, "bad screen");
    RI_ASSERT(ri_zoom_fit(1366, 768, -5, 64) == 0, "bad chrome");
    /* Guard (owner breakage 2026-09-29): explicit want clamped to fit. */
    RI_ASSERT(ri_zoom_clamp(1366, 768, 32, 72, 1) == 0, "dell 1.5 clamps");
    RI_ASSERT(ri_zoom_clamp(1366, 768, 32, 72, 2) == 0, "dell 2 clamps");
    RI_ASSERT(ri_zoom_clamp(1366, 768, 32, 72, 0) == 0, "dell 1 keeps");
    RI_ASSERT(ri_zoom_clamp(1920, 1080, 32, 72, 1) == 1, "hd 1.5 keeps");
    RI_ASSERT(ri_zoom_clamp(1920, 1080, 32, 72, 2) == 1, "hd 2 clamps to fit");
    RI_ASSERT(ri_zoom_clamp(2560, 1440, 32, 72, 2) == 2, "qhd 2 keeps");
    RI_ASSERT(ri_zoom_clamp(1366, 768, 32, 72, RI_ZOOMFIT_FIT) == 0, "fit refits");
    RI_ASSERT(ri_zoom_clamp(0, 0, 32, 72, 1) == 1, "unknown screen keeps");
    RI_ASSERT(ri_zoom_clamp(1366, 768, 32, 72, 9) == 0, "bad want");
    RI_ASSERT(ri_zoom_parse("fit", 3) == RI_ZOOMFIT_FIT, "parse fit");
    RI_ASSERT(ri_zoom_parse("2", 1) == 2, "parse 2");
    RI_ASSERT(ri_zoom_parse("1\n", 2) == 1, "parse newline");
    RI_ASSERT(ri_zoom_parse("fit\n", 4) == RI_ZOOMFIT_FIT, "parse fit newline");
    RI_ASSERT(ri_zoom_parse("9", 1) == RI_ZOOMFIT_FIT, "parse bad");
    RI_ASSERT(ri_zoom_parse(0, 0) == RI_ZOOMFIT_FIT, "parse null");
    {
        char b[8];
        RI_ASSERT(ri_zoom_format(-1, b, sizeof b) == 3, "format fit");
        RI_ASSERT(!memcmp(b, "fit", 3), "format fit bytes");
        RI_ASSERT(ri_zoom_format(1, b, sizeof b) == 1 && b[0] == '1', "format 1");
        RI_ASSERT(ri_zoom_format(9, b, sizeof b) == 0, "format bad");
        RI_ASSERT(ri_zoom_format(0, 0, 0) == 0, "format null");
    }
    /* Levi canvas zoom step (fidelity plan P1, 2026-09-30): the dense
     * hardware panel takes the largest zoom >= the app zoom that still
     * fits. Dell: app 1x (Mix is too wide at 1.5x) but the Levi page fits
     * at 1.5x: 1317 + 12 + 32 <= 1366. */
    RI_ASSERT(ri_zoom_levi(0, 1366, 768, 32, 72) == 1, "dell levi 1.5x");
    RI_ASSERT(ri_zoom_levi(2, 4000, 3000, 32, 72) == 2, "levi never above 2x");
    RI_ASSERT(ri_zoom_levi(0, 4000, 3000, 32, 72) == 2, "big screen levi 2x");
    RI_ASSERT(ri_zoom_levi(0, 800, 600, 32, 72) == 0, "small screen stays");
    RI_ASSERT(ri_zoom_levi(1, 0, 0, 32, 72) == 1, "unknown screen keeps app zoom");
    RI_ASSERT(ri_zoom_levi(7, 1366, 768, 32, 72) == 0 && ri_zoom_levi(-1, 1366, 768, 32, 72) == 0, "bad zoom");
    {
        int z;
        for (z = 0; z <= 2; z++)
            RI_ASSERT(ri_zoom_levi(z, 1366, 768, 32, 72) >= z, "levi never below app zoom %d", z);
    }
    RI_RESULT("zoomfit");
}
