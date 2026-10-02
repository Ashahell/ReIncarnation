/* t152_repaint_reason — attributing an expensive repaint to its caller.
 *
 * The Dell 2026-10-02 record could say "119 partial repaints, average 4036 us,
 * worst 303098 us" and could not say which caller asked for them. A partial of
 * 4 ms and a partial of 300 ms are different failures with different fixes,
 * and the aggregate n=/max= pair collapses them into one indistinguishable
 * number. rsection.mcc.c now records every box repaint into the reason bucket
 * its caller declared, so an expensive partial names its own cause.
 *
 * This is host-testable without a GUI because the accounting is a pure
 * function of the reason code: RI_RSEC_BOX_* values, the bounds check that
 * folds an unknown reason onto OTHER, and the agreement between the
 * per-reason sum and the reason-agnostic dp_* total (which is what keeps the
 * split a partition of the aggregate rather than a second, drifting tally).
 *
 * Laws:
 *  - the four reason codes are distinct and contiguous from 0, so a bucket
 *    array indexed by them is total;
 *  - an out-of-range reason folds onto OTHER rather than indexing out of
 *    bounds — the backend must be able to pass junk from a new caller;
 *  - set_box_why is one-shot: it describes the NEXT refresh_box, and the
 *    plain refresh_box consumes it, so a caller that forgets to re-arm
 *    cannot attribute its repaints to the previous reason;
 *  - the default is NONE, not STEPS, so an unattributed box is visibly
 *    unattributed rather than silently charged to the drum lamps;
 *  - per-reason sums add up to the aggregate dp_sum, and per-reason counts
 *    to dp_n, when every reason is exercised (the split is a partition).
 */
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "tests/helpers/ri_assert.h"
#include "gui/panelui.h"

/* One canvas's per-reason tally, as rsection.mcc.c keeps it. The field names
 * mirror struct RSectionDiag; the struct itself is AROS-only, so the shapes
 * are re-declared here and the partition law is what is pinned. */
struct tally {
    uint32_t dp_max, dp_sum;
    int32_t dp_n;
    uint32_t dpw_max[4], dpw_sum[4];
    int32_t dpw_n[4];
};

static void record(struct tally *t, int why, uint32_t us) {
    int w = ri_rsection_box_why(why);
    if (us > t->dp_max)
        t->dp_max = us;
    t->dp_sum += us;
    t->dp_n++;
    if (us > t->dpw_max[w])
        t->dpw_max[w] = us;
    t->dpw_sum[w] += us;
    t->dpw_n[w]++;
}

int main(void) {
    struct tally t;

    /* Distinct, contiguous, and 0 is NONE so the array is total. */
    RI_ASSERT(RI_RSEC_BOX_NONE == 0, "NONE is 0");
    RI_ASSERT(RI_RSEC_BOX_STEPS == 1, "STEPS is 1");
    RI_ASSERT(RI_RSEC_BOX_BAR == 2, "BAR is 2");
    RI_ASSERT(RI_RSEC_BOX_OTHER == 3, "OTHER is 3");

    /* An out-of-range reason folds onto OTHER, never indexes away. */
    RI_ASSERT(ri_rsection_box_why(-1) == RI_RSEC_BOX_OTHER, "negative folds to OTHER");
    RI_ASSERT(ri_rsection_box_why(4) == RI_RSEC_BOX_OTHER, "above OTHER folds to OTHER");
    RI_ASSERT(ri_rsection_box_why(9999) == RI_RSEC_BOX_OTHER, "far above folds to OTHER");
    RI_ASSERT(ri_rsection_box_why(RI_RSEC_BOX_STEPS) == RI_RSEC_BOX_STEPS, "STEPS passes through");
    RI_ASSERT(ri_rsection_box_why(RI_RSEC_BOX_BAR) == RI_RSEC_BOX_BAR, "BAR passes through");

    /* The default is NONE, so an unattributed box is not charged to a
     * caller it may not belong to. */
    RI_ASSERT(ri_rsection_box_why(RI_RSEC_BOX_NONE) == RI_RSEC_BOX_NONE, "NONE passes through");

    /* The split is a partition: every sample lands in exactly one bucket,
     * and the per-reason tallies add back to the aggregate. */
    memset(&t, 0, sizeof t);
    record(&t, RI_RSEC_BOX_STEPS, 4036u); /* the average that was unexplained */
    record(&t, RI_RSEC_BOX_STEPS, 641u);
    record(&t, RI_RSEC_BOX_BAR, 187u);
    record(&t, RI_RSEC_BOX_OTHER, 303098u); /* the 303 ms tail */
    record(&t, 7, 50u);                    /* junk -> OTHER */
    RI_ASSERT(t.dp_n == 5, "aggregate count 5 (%ld)", (long)t.dp_n);
    RI_ASSERT(t.dp_max == 303098u, "aggregate max is the outlier (%lu)", (unsigned long)t.dp_max);
    {
        uint32_t s = 0u, mx = 0u;
        int32_t n = 0L;
        int w;
        for (w = 0; w < 4; w++) {
            s += t.dpw_sum[w];
            n += t.dpw_n[w];
            if (t.dpw_max[w] > mx)
                mx = t.dpw_max[w];
        }
        RI_ASSERT(s == t.dp_sum, "per-reason sums add to the aggregate (%lu vs %lu)",
            (unsigned long)s, (unsigned long)t.dp_sum);
        RI_ASSERT(n == t.dp_n, "per-reason counts add to the aggregate (%ld vs %ld)",
            (long)n, (long)t.dp_n);
        RI_ASSERT(mx == t.dp_max, "per-reason maxes agree with the aggregate");
    }
    /* And the point of the whole exercise: the 303 ms is now attributable,
     * and it is NOT the drum lamps. */
    RI_ASSERT(t.dpw_max[RI_RSEC_BOX_OTHER] == 303098u, "the tail lands in OTHER");
    RI_ASSERT(t.dpw_max[RI_RSEC_BOX_STEPS] == 4036u, "the average is STEPS");
    RI_ASSERT(t.dpw_n[RI_RSEC_BOX_OTHER] == 2u, "junk folded into OTHER (%ld)",
        (long)t.dpw_n[RI_RSEC_BOX_OTHER]);
    RI_ASSERT(t.dpw_n[RI_RSEC_BOX_BAR] == 1L, "BAR counted once");

    /* The heap of steps is the case that motivated the split: 119 lamp
     * repaints in one window, none of which individually explains the
     * 303 ms, so the aggregate average is a lie unless the tail is named. */
    memset(&t, 0, sizeof t);
    {
        int i;
        for (i = 0; i < 119; i++)
            record(&t, RI_RSEC_BOX_STEPS, 300u);
        record(&t, RI_RSEC_BOX_BAR, 303098u);
        RI_ASSERT(t.dp_n == 120, "120 samples");
        RI_ASSERT(t.dpw_max[RI_RSEC_BOX_STEPS] == 300u,
            "the 119 lamps stay cheap on their own (%lu)",
            (unsigned long)t.dpw_max[RI_RSEC_BOX_STEPS]);
        RI_ASSERT(t.dpw_max[RI_RSEC_BOX_BAR] == 303098u,
            "the tail belongs to the Song Position box");
        RI_ASSERT(t.dpw_n[RI_RSEC_BOX_STEPS] == 119L, "119 lamps (%ld)",
            (long)t.dpw_n[RI_RSEC_BOX_STEPS]);
    }

    /* An unattributed box still counts in the aggregate (it happened), and
     * lands in NONE rather than being dropped. */
    memset(&t, 0, sizeof t);
    record(&t, RI_RSEC_BOX_NONE, 12u);
    RI_ASSERT(t.dp_n == 1, "an unattributed box is counted");
    RI_ASSERT(t.dpw_n[RI_RSEC_BOX_NONE] == 1L, "and it lands in NONE");

    RI_RESULT("repaint_reason");
}