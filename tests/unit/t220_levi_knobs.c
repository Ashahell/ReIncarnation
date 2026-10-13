/* t220_levi_knobs — five Levi knobs drawn dim at a hardcoded 0.0f with no
 * geometry row, so they were neither clickable nor animated.
 *
 * Owner ear check 2026-10-13: "the left column's MASTER and BALANCE knobs ...
 * the arpeggiator's MODE, OCTAVE, GATE and ENTROPY knobs (existing controls
 * 186, 184, 185 and 189). All six are drawn in the same 'dim, value 0, not
 * clickable' table as the Main Systems knob."
 *
 * The defect was never the engine. Every control named here already carries
 * bind=LEVI with a real RI_CTL_LEVI_* engine id, so they were deliverable;
 * what was missing was a panelgeo row to hit-test and a draw that read the
 * real value instead of a literal 0.0f. Both live in portable code:
 * gui/panelgeo.c is in the portable core and gui/draw is confined to it.
 *
 * MASTER is deliberately NOT in this list. It has no engine control anywhere
 * in the tree — the "MASTER CONTROL" ribbon (idx 69..76) is a different,
 * also-unbuilt thing — and picking a target for it would be exactly the
 * "pinning an invention" the owner ruled out for the Patch knob. It stays
 * dim until the owner says what it drives.
 *
 * What is pinned: each control is hit-testable, and the hit round-trips to
 * its own reg id at every zoom. That is the "does nothing" half of the
 * report in one property; the "does not move" half is the existing
 * ri_art_levi_item path, which reads ri_sui_value() and only runs for a
 * control that has geometry.
 */
#include <stdio.h>
#include <stdint.h>
#include "tests/helpers/ri_assert.h"
#include "gui/panelgeo.h"
#include "gui/ctlreg.h"
#include "gui/sectui.h"

#define L(id) ((uint16_t)((uint16_t)(RI_SEC_LEVI << 8) | (uint16_t)(id)))

int main(void) {
    const struct RIGeoSection *g = ri_geo_section(RI_SEC_LEVI);
    static const struct { uint16_t reg; uint32_t idx; const char *what; } WANT[5] = {
        { L(225), 225u, "BALANCE -> RI_CTL_LEVI_PFBAL" },
        { L(186), 186u, "arp MODE" },
        { L(184), 184u, "arp OCTAVE" },
        { L(185), 185u, "arp GATE" },
        { L(189), 189u, "arp ENTROPY" },
    };
    int z, k;
    uint32_t i;

    RI_ASSERT(g != 0, "Levi section laid out");

    /* Each named control exists in the registry and is deliverable. If this
     * fails the wiring is wrong at the source, not at the geometry. */
    for (k = 0; k < 5; k++) {
        const struct RICtlDef *d = ri_ctlreg_find(WANT[k].reg);
        RI_ASSERT(d != 0, "%s exists in the registry", WANT[k].what);
        if (!d)
            continue;
        RI_ASSERT(d->automatable, "%s must be automatable", WANT[k].what);
        RI_ASSERT(d->bind == RI_BIND_LEVI, "%s must bind to the Levi", WANT[k].what);
        RI_ASSERT(ri_ctlreg_auto_id(d) != 0u, "%s must have a delivery key", WANT[k].what);
    }

    /* Each is laid out with a hit box, and the hit round-trips at every
     * zoom -- the "does nothing" half of the report. */
    for (k = 0; k < 5; k++) {
        int x0, y0, x1, y1;
        RI_ASSERT(ri_geo_bbox(g, WANT[k].reg, 0, &x0, &y0, &x1, &y1) == 0,
            "%s has a geometry box", WANT[k].what);
    }
    for (z = 0; z < 4; z++) {
        for (k = 0; k < 5; k++) {
            const struct RIGeoItem *it;
            int opt = -2;
            uint16_t h;
            int n, found = 0;
            for (i = 0u; i < g->nitems; i++) {
                it = &g->items[i];
                if (it->reg_id == WANT[k].reg) { found = 1; break; }
            }
            RI_ASSERT(found, "%s present in items (zoom %d)", WANT[k].what, z);
            if (!found)
                continue;
            h = ri_geo_hit_opt(g, ri_geo_px(it->cx, z), ri_geo_px(it->cy, z), z, &opt);
            RI_ASSERT(h == WANT[k].reg, "hit at centre of %s = %04x, want %04x",
                WANT[k].what, h, WANT[k].reg);
            n = it->shape;
            /* Only KNOB and RECT carry a control. LED, LEGEND, DIVIDER,
             * OPTION and STEPPER are decoration or belong to another
             * control -- a row in any of those is a box that does nothing. */
            RI_ASSERT(n == RI_GEO_KNOB || n == RI_GEO_RECT,
                "%s has a control shape (%u)", WANT[k].what, n);
        }
    }

    /* MASTER must NOT have quietly acquired a box: it has no engine target,
     * so laying it out would produce a knob that animates and does nothing
     * -- a worse failure than one that is visibly inert. */
    {
        int a0, b0, c0, d0;
        RI_ASSERT(ri_geo_bbox(g, L(2), 0, &a0, &b0, &c0, &d0) != 0,
            "MASTER (no engine target) must stay unlaid-out");
    }

    /* The "does not move" half, in the form the draw actually reads.
     * ri_art_levi_item passes ri_sui_value(ui, idx) straight to the knob
     * pointer, so if the panel model holds the control at its registry
     * default and that value MOVES when set, the knob animates. Asserting
     * only the geometry would have left the hardcoded 0.0f unpinned --
     * which is literally the bug. */
    {
        struct RISectUI ui;
        RI_ASSERT(ri_sui_init(&ui, RI_SEC_LEVI) == 0, "levi panel init");
        for (k = 0; k < 5; k++) {
            const struct RICtlDef *d = ri_ctlreg_find(WANT[k].reg);
            int lo, hi, mid;
            if (!d)
                continue;
            lo = ri_sui_value(&ui, WANT[k].idx);
            RI_ASSERT(lo == d->def_v, "%s sits at its registry default (%d vs %d)",
                WANT[k].what, lo, d->def_v);
            hi = d->max_v > d->def_v ? d->max_v : d->min_v;
            ri_sui_set(&ui, WANT[k].idx, hi);
            mid = ri_sui_value(&ui, WANT[k].idx);
            RI_ASSERT(mid == hi, "%s value MOVES (%d -> %d)", WANT[k].what, lo, mid);
        }
    }

    RI_RESULT("leviknobs");
}