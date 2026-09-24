/* t29_paneldefault — Module 2.9 partial (first panel), TC-2.9.2
 * right-click + TC-2.9.1 panel inventory:
 *
 *   - TC-2.9.2: right-click resets the control to its default. The
 *     MCC shell needs a host-tested accessor: ri_panel_default_ctl
 *     returns the control's def_value, or -1 fail-closed when the
 *     panel is NULL or carries no such control (the shell ignores
 *     -1). Pins the full 909 default table (first panel) + spot
 *     defaults on 303A/mixer/transport + fail-closed edges.
 *   - TC-2.9.1 (inventory half): the first panel (909, index 3)
 *     exposes exactly tune/level/decay/flamres at 0x0900..0x0903 —
 *     the code side of the panel-909-geometry doc contract (pixel
 *     centers live in the doc + MCC shells, not host-testable).
 *
 * RED status: ri_panel_default_ctl does not exist yet, so this file
 * fails to BUILD on the current tree (feature-absent manifest, same
 * shape as the t23/t24 slices that pinned new panel/params surfaces).
 */
#include <stdio.h>
#include "gui/panels.h"

static int fails = 0;
#define CHECK(cond, ...) do { \
    if (!(cond)) { printf("FAIL "); printf(__VA_ARGS__); printf("\n"); fails++; } \
} while (0)

int main(void) {
    const struct RIPanelDesc *p909 = ri_panel_get(3);
    char buf[8];

    /* --- value readout formatting (proof-vehicle text row): 0..127
     * as decimal, out-of-range fail-closed to "---". --- */
    ri_ctl_format_value(buf, 0);
    CHECK(buf[0] == '0' && buf[1] == '\0', "fmt 0 [%s]", buf);
    ri_ctl_format_value(buf, 64);
    CHECK(buf[0] == '6' && buf[1] == '4' && buf[2] == '\0', "fmt 64 [%s]",
        buf);
    ri_ctl_format_value(buf, 100);
    CHECK(buf[0] == '1' && buf[1] == '0' && buf[2] == '0' &&
        buf[3] == '\0', "fmt 100 [%s]", buf);
    ri_ctl_format_value(buf, 127);
    CHECK(buf[0] == '1' && buf[1] == '2' && buf[2] == '7' &&
        buf[3] == '\0', "fmt 127 [%s]", buf);
    ri_ctl_format_value(buf, -1);
    CHECK(buf[0] == '-' && buf[1] == '-' && buf[2] == '-' &&
        buf[3] == '\0', "fmt neg [%s]", buf);
    ri_ctl_format_value(buf, 200);
    CHECK(buf[0] == '-' && buf[3] == '\0', "fmt over [%s]", buf);

    /* --- undo-commit counter formatting (arbitrary counts, not just
     * 0..127 ctl values): decimal into buf (needs 6 bytes), large
     * values saturate at 65535, no negatives in-range. --- */
    {
        char cbuf[8];
        ri_ctl_format_count(cbuf, 0);
        CHECK(cbuf[0] == '0' && cbuf[1] == '\0', "cnt 0 [%s]", cbuf);
        ri_ctl_format_count(cbuf, 7);
        CHECK(cbuf[0] == '7' && cbuf[1] == '\0', "cnt 7 [%s]", cbuf);
        ri_ctl_format_count(cbuf, 12345);
        CHECK(cbuf[0] == '1' && cbuf[4] == '5' && cbuf[5] == '\0',
            "cnt 12345 [%s]", cbuf);
        ri_ctl_format_count(cbuf, 999999);
        CHECK(cbuf[4] == '5' && cbuf[5] == '\0', "cnt sat [%s]", cbuf);
    }

    /* --- first-panel inventory: 909 exposes exactly 4 controls --- */
    CHECK(p909 != 0, "909 panel missing");
    if (p909) {
        CHECK(p909->nctls == 4u, "909 nctls %u", p909->nctls);
        CHECK(ri_panel_find_ctl(p909, 0x0900u) != 0, "tune id missing");
        CHECK(ri_panel_find_ctl(p909, 0x0901u) != 0, "level id missing");
        CHECK(ri_panel_find_ctl(p909, 0x0902u) != 0, "decay id missing");
        CHECK(ri_panel_find_ctl(p909, 0x0903u) != 0, "flamres id missing");
    }

    /* --- right-click defaults: full 909 table --- */
    CHECK(ri_panel_default_ctl(p909, 0x0900u) == 64, "tune def %d",
        ri_panel_default_ctl(p909, 0x0900u));
    CHECK(ri_panel_default_ctl(p909, 0x0901u) == 100, "level def %d",
        ri_panel_default_ctl(p909, 0x0901u));
    CHECK(ri_panel_default_ctl(p909, 0x0902u) == 64, "decay def %d",
        ri_panel_default_ctl(p909, 0x0902u));
    CHECK(ri_panel_default_ctl(p909, 0x0903u) == 64, "flamres def %d",
        ri_panel_default_ctl(p909, 0x0903u));

    /* --- spot defaults elsewhere (neutral playing positions) --- */
    CHECK(ri_panel_default_ctl(ri_panel_get(0), 0x0300u) == 96,
        "cutoff def");
    CHECK(ri_panel_default_ctl(ri_panel_get(4), 0x0B00u) == 127,
        "bus1 def");
    CHECK(ri_panel_default_ctl(ri_panel_get(5), 0x0B00u + 10u) == 120,
        "tempo def");

    /* --- fail-closed edges: unknown id, NULL panel --- */
    CHECK(ri_panel_default_ctl(p909, 0xDEADu) == -1, "unknown id not -1");
    CHECK(ri_panel_default_ctl(0, 0x0900u) == -1, "null panel not -1");

    if (fails)
        printf("FAIL %d\n", fails);
    else
        printf("PASS t29_paneldefault\n");
    return fails != 0;
}
