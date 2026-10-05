/* t170_ctlreg_index — an exhaustive guard on `ri_ctlreg_find`, and a record of
 * one replacement that was attempted and is NOT known to be sound.
 *
 * WHY (2026-10-05). The linear scan is 45 % of the section's disjoint-clip build
 * (ratio 0.4448-0.4527, three runs in one process) and the transport's 31 items
 * resolve at a mean depth of 224 of 485 -- 6949 comparisons per repaint. So it is
 * worth attacking, and a global binary search looks free.
 *
 *   IT IS NOT, and this test is why that was established rather than shipped: the
 *   ids do not ascend across the whole table, so a binary search over it returns
 *   NULL for ids that exist. The check below is exhaustive over all 65536 possible
 *   reg_id values precisely because both this failure and the one below are
 *   invisible to a sampled test.
 *
 *   A PER-SECTION RANGE was then tried, on the premise that the table is grouped
 *   by section with each section ascending within its run. That premise is TRUE of
 *   contiguity -- every section's entries are adjacent -- but the sections are not
 *   in numeric order (RI_SEC_MIX_LEVI sits at indices 182..189, before
 *   RI_SEC_PAT_LEVI at 480..484). Installed and checked, it FAILED: `ri_ctlreg_find`
 *   returned NULL for ids the linear scan resolves, from reg_id 4833 upward. That
 *   is the LEVI range, so the failure is inside a section's run rather than at a
 *   boundary, which means the per-section ids do not ascend there either. **The
 *   change was reverted and the reason is not fully established.**
 *
 *   The replacement that does not depend on any ordering assumption is a
 *   DIRECT-MAPPED index: reg_id is (section << 8) | idx, so a table indexed by
 *   both needs no sort at all. That is real work with run-time state and a first-call
 *   cost, and it is not started here.
 *
 * So this test pins three things: the global binary search is refuted; every entry
 * is reachable through the shipped lookup by its own id; and `ri_ctlreg_find`
 * matches a linear scan, written out in full below, on all 65536 ids.
 *
 * The linear scan is deliberately not shared with production: a helper both sides
 * called could be wrong in one place and consistent.
 */
#include <stdio.h>
#include <stdint.h>
#include "tests/helpers/ri_assert.h"
#include "gui/ctlreg.h"

/* The obvious implementation, spelled out. Deliberately not shared with
 * production: a helper both sides call could be wrong in one place and
 * consistent. */
static const struct RICtlDef *linear(uint16_t reg_id) {
    uint32_t i;
    for (i = 0; i < ri_ctlreg_count(); i++) {
        const struct RICtlDef *d = ri_ctlreg_at(i);
        if (d && d->reg_id == reg_id)
            return d;
    }
    return 0;
}

int main(void) {
    uint32_t n = ri_ctlreg_count(), sec, i;
    uint32_t nonempty = 0, matched = 0, looked_up = 0;

    RI_ASSERT(n > 0u, "the registry is empty, so nothing can be checked");

    /* (1) EVERY ordering-based replacement is refuted by the table itself, and
     * the two failures are different in kind, so both are checked.
     *
     *   (a) A GLOBAL binary search: the ids do not ascend across the table.
     *
     *   (b) A BINARY SEARCH INSIDE A PER-SECTION RANGE: the ids do not ascend
     *       WITHIN a section either. This is the one that looked safest and was
     *       installed anyway -- bounds as literals, no mutable state, 6
     *       comparisons instead of 224 -- and it returned NULL for every LEVI
     *       control from reg_id 4822 upward.
     *
     * The cause is LEVI's run being ordered BY SUB-PANEL, each sub-panel
     * ascending in reg_id but the sub-panels themselves not in idx order: the
     * table goes ... 4835 at index 445, then 4822 at index 446. So the table is
     * grouped by section and then by sub-panel, and NEITHER grouping ascends in
     * reg_id. Any search that assumes otherwise is wrong here.
     *
     * ⚠ AND THE MEASUREMENT THAT TOLD ME OTHERWISE WAS MY OWN BUG, which is why
     * this is pinned rather than left as prose. The probe that "proved" every
     * section ascends stored `prev = i` -- the INDEX -- instead of
     * `prev = d->reg_id`, so it compared a reg_id against an index and reported
     * `ascending: yes` for all 21 sections. It was wrong, it was believed, and an
     * implementation shipped on the strength of it and had to be reverted. **This
     * is the second invented-or-mishandled constant in this lane in two days**
     * (the geometry-shape enum, which reversed a published conclusion), and the
     * failure mode is identical: an instrument that reports a property the code
     * does not have. So the check below computes ascendingness from reg_id, and
     * names the inversions rather than only counting them. */
    {
        uint32_t glob = 0, within = 0;
        const struct RICtlDef *sample_lo = 0, *sample_hi = 0;
        for (i = 1; i < n; i++) {
            const struct RICtlDef *a = ri_ctlreg_at(i - 1);
            const struct RICtlDef *b = ri_ctlreg_at(i);
            if (!a || !b)
                continue;
            if (b->reg_id < a->reg_id)
                glob++;
            if ((uint32_t)(a->reg_id >> 8) == (uint32_t)(b->reg_id >> 8) &&
                b->reg_id < a->reg_id) {
                if (!within) {
                    sample_lo = a;
                    sample_hi = b;
                }
                within++;
            }
        }
        RI_ASSERT(glob > 0u, "the ids ascend across the whole table, so a GLOBAL "
            "binary search may be viable after all -- re-check it");
        RI_ASSERT(within > 0u, "the ids ascend WITHIN every section, so a "
            "PER-SECTION RANGE search may be viable after all -- re-check it "
            "(it was installed on exactly that false belief and returned NULL for "
            "every LEVI control from reg_id 4822 upward)");
        RI_ASSERT(sample_lo && sample_hi && sample_hi->reg_id < sample_lo->reg_id,
            "the within-section inversion is gone; the named pair should be "
            "replaced with whatever supersedes it");
    }

    /* (2) The shipped lookup, checked per section. */
    for (sec = 0; sec < RI_SEC_COUNT; sec++) {
        uint32_t lo = 0xFFFFFFFFu, hi = 0u, cnt = 0u;
        for (i = 0; i < n; i++) {
            const struct RICtlDef *d = ri_ctlreg_at(i);
            if (!d || (uint32_t)(d->reg_id >> 8) != sec)
                continue;
            if (cnt == 0u)
                lo = i;
            hi = i;
            cnt++;
        }
        if (cnt == 0u) {
            /* A section with no entries must read as empty, not as some other
             * section's range. */
            RI_ASSERT(ri_ctlreg_find((uint16_t)(sec << 8)) == 0,
                "section %u has no entries but lookup of %u<<8 succeeded", sec, sec);
            continue;
        }
        nonempty++;

        /* Every entry of this section lies inside the claimed range... */
        for (i = lo; i <= hi; i++) {
            const struct RICtlDef *d = ri_ctlreg_at(i);
            RI_ASSERT(d != 0, "section %u: NULL at index %u inside its range",
                sec, i);
            RI_ASSERT((uint32_t)(d->reg_id >> 8) == sec,
                "section %u: index %u holds section %u", sec, i,
                (unsigned)(d->reg_id >> 8));
        }
        /* Every entry of the section is reachable through the shipped lookup. */
        for (i = 0; i < n; i++) {
            const struct RICtlDef *d = ri_ctlreg_at(i);
            if (!d || (uint32_t)(d->reg_id >> 8) != sec)
                continue;
            RI_ASSERT(ri_ctlreg_find(d->reg_id) == d,
                "section %u: entry %u (reg_id %u) is not returned by its own id",
                sec, i, d->reg_id);
        }
    }

    /* (3) Exhaustive equivalence over all 65536 possible reg_ids. */
    for (i = 0; i < 65536u; i++) {
        uint16_t rid = (uint16_t)i;
        const struct RICtlDef *got = ri_ctlreg_find(rid);
        const struct RICtlDef *want = linear(rid);
        looked_up++;
        if (got != want) {
            RI_ASSERT(0, "reg_id %u: binary search gave %s, linear scan gave %s",
                rid, got ? "an entry" : "NULL", want ? "an entry" : "NULL");
        }
        if (got) {
            RI_ASSERT(got->reg_id == rid, "reg_id %u returned an entry for %u",
                rid, got->reg_id);
            matched++;
        }
    }

    /* Volume, so "returns NULL for everything" cannot pass. */
    RI_ASSERT(nonempty >= 18u, "only %u sections have entries", nonempty);
    RI_ASSERT(matched > 0u && matched <= n,
        "%u ids resolved, table has %u entries", matched, n);
    RI_ASSERT(looked_up == 65536u, "only %u ids looked up", looked_up);

    RI_RESULT("ctlreg_index");
    return 0;
}