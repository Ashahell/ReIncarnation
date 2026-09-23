/* t29_layout — Module 2.9 first-panel geometry, TC-2.9.1 code side:
 *
 * The panel-909-geometry doc names four knob centers at 1024×768;
 * the AROS shell needs them as numbers, not prose. ri_panel909_knob
 * returns the doc rect for knob index 0..3 (tune/level/decay/
 * flamres) scaled to a zoom level via ri_zoom_factor; out-of-range
 * index or unknown zoom fails closed (all zeros). Knob radius 28 px
 * at 1x (the doc value); centers exact per the doc table.
 *
 * RED status: ri_panel909_knob_rect does not exist yet, so this
 * file fails to BUILD on the current tree (feature-absent
 * manifest, same shape as the t29_paneldefault slice).
 */
#include <stdio.h>
#include "gui/panels.h"
#include "gui/knob_logic.h"

static int fails = 0;
#define CHECK(cond, ...) do { \
    if (!(cond)) { printf("FAIL "); printf(__VA_ARGS__); printf("\n"); fails++; } \
} while (0)

int main(void) {
    struct RIPanelRect r;

    /* --- doc centers at 1x (TC-2.9.1 ±2px contract values,
     * compact measured lock: even 33 px pitch, row y=52) --- */
    r = ri_panel909_knob_rect(0, 0);
    CHECK(r.x == 30 && r.y == 52, "tune (%d,%d)", r.x, r.y);
    r = ri_panel909_knob_rect(1, 0);
    CHECK(r.x == 63 && r.y == 52, "level (%d,%d)", r.x, r.y);
    r = ri_panel909_knob_rect(2, 0);
    CHECK(r.x == 96 && r.y == 52, "decay (%d,%d)", r.x, r.y);
    r = ri_panel909_knob_rect(3, 0);
    CHECK(r.x == 129 && r.y == 52, "flamres (%d,%d)", r.x, r.y);

    /* --- radius 32 at 1x, scales with zoom --- */
    r = ri_panel909_knob_rect(0, 0);
    CHECK(r.w == 32 && r.h == 32, "diam %dx%d", r.w, r.h);
    r = ri_panel909_knob_rect(0, 2);
    CHECK(r.x == 60 && r.y == 104, "2x center (%d,%d)", r.x, r.y);
    CHECK(r.w == 64 && r.h == 64, "2x diam %dx%d", r.w, r.h);

    /* --- fail-closed edges --- */
    r = ri_panel909_knob_rect(4, 0);
    CHECK(r.x == 0 && r.y == 0 && r.w == 0 && r.h == 0, "index oob");
    r = ri_panel909_knob_rect(0, 9);
    CHECK(r.x == 0 && r.y == 0 && r.w == 0 && r.h == 0, "zoom bad");

    if (fails)
        printf("FAIL %d\n", fails);
    else
        printf("PASS t29_layout\n");
    return fails != 0;
}
