/* t168_gap_accounting — the GAP must be the SUM of the phases that is
 * subtracted, and the components must account for the total.
 *
 * WHY (2026-10-05). `dp_gap` is `us - (what the phases accounted for)`, and its
 * purpose is to answer "was this draw interrupted, or was a phase slow?" — the
 * one distinction the aggregate could not make. It was implemented as
 * `us - max(build, replay, blit)`.
 *
 * That is arithmetically wrong, because the phases are DISJOINT intervals: the
 * maximum is one of them, so `us - max` equals the OTHER TWO PHASES plus the
 * true residue. It booked the replay and the blit into a bucket labelled "no
 * phase", and the field it published — a "119 us gap, 36 % of every partial" —
 * was the artefact.
 *
 * The whole thing is decidable without a guest, which is why it is a unit test:
 * the log line refutes it on its own. Every mean on `RIAPP draw:` shares the same
 * denominator, so
 *
 *     part_avg == build_avg + rpl_avg + blt_avg + gap_avg
 *
 * and for the recorded quiet window 233 == 113 + 34 + 70 + gap gives gap = 16,
 * not the 121 that was logged. 121 - 16 = 105 = rpl + blt, exactly.
 *
 * This mirrors the accounting the way t152 mirrors the reason-capture
 * discipline: the real draw_frame is AROS-only, but the ARITHMETIC is portable,
 * and a rule that can be checked in five lines on the host should be.
 */
#include <stdio.h>
#include "tests/helpers/ri_assert.h"

/* The accounting rule, transcribed from gui/widgets/rsection.mcc.c. Keeping it
 * here as a literal rather than calling into the widget is deliberate: the point
 * is to pin the RULE, and a helper that both sides share could be wrong in one
 * place and consistent. */
static unsigned gap_sum(unsigned us, unsigned build, unsigned replay, unsigned blit) {
    unsigned acct = build + replay + blit;
    return acct < us ? us - acct : 0u;
}

/* What the code did, for the contrast. Never ship this. */
static unsigned gap_max(unsigned us, unsigned build, unsigned replay, unsigned blit) {
    unsigned acct = build;
    if (replay > acct) acct = replay;
    if (blit > acct) acct = blit;
    return acct < us ? us - acct : 0u;
}

int main(void) {
    struct { unsigned us, build, replay, blit, want; } cases[] = {
        /* The recorded Dell quiet window, through the buffer. */
        { 233u, 113u, 34u, 70u, 16u },
        /* The same window, direct-painted (blit removed, replay absorbs it). */
        { 238u, 117u, 105u, 0u, 16u },
        /* The stall window: components normal, total enormous. */
        { 2950u, 123u, 34u, 71u, 2722u },
        /* A blit that was interrupted, so the components do NOT read normal. */
        { 2803u, 132u, 56u, 2596u, 19u },
        /* Degenerate: components exceed the total (clock jitter). No underflow. */
        { 10u, 8u, 5u, 4u, 0u },
        { 0u, 0u, 0u, 0u, 0u },
    };
    unsigned i;

    for (i = 0u; i < sizeof cases / sizeof cases[0]; i++) {
        RI_ASSERT(gap_sum(cases[i].us, cases[i].build, cases[i].replay,
                cases[i].blit) == cases[i].want,
            "case %u: gap_sum(%u, %u+%u+%u) = %u, want %u", i, cases[i].us,
            cases[i].build, cases[i].replay, cases[i].blit,
            gap_sum(cases[i].us, cases[i].build, cases[i].replay, cases[i].blit),
            cases[i].want);
    }

    /* THE INVARIANT that would have caught it: the phases and the gap partition
     * the total, so they must add back up to it exactly. Checked over the same
     * cases, which is what makes this a partition and not a coincidence. */
    for (i = 0u; i < sizeof cases / sizeof cases[0]; i++) {
        unsigned acct = cases[i].build + cases[i].replay + cases[i].blit;
        unsigned gap = gap_sum(cases[i].us, cases[i].build, cases[i].replay,
            cases[i].blit);
        if (acct >= cases[i].us) {
            RI_ASSERT(gap == 0u, "case %u: no gap when the phases cover it", i);
            continue;
        }
        RI_ASSERT(acct + gap == cases[i].us,
            "case %u: phases %u + gap %u must equal the total %u", i, acct, gap,
            cases[i].us);
    }

    /* AND THE CONTRAST, so the failure mode is named rather than merely absent:
     * `max` books the other phases into the gap. On the quiet window it invents
     * 105 us that does not exist, and that is the whole of the reported figure. */
    RI_ASSERT(gap_max(233u, 113u, 34u, 70u) == 120u,
        "the max rule invents 120 us on the quiet window, got %u",
        gap_max(233u, 113u, 34u, 70u));
    RI_ASSERT(gap_max(233u, 113u, 34u, 70u) - gap_sum(233u, 113u, 34u, 70u)
            == 34u + 70u,
        "the difference is exactly the two phases the max threw away");

    /* Flatness was cited as evidence that the gap was not preemption. Show why
     * that argument cannot work with `max`: hold the phases constant and the gap
     * is constant, so its flatness is an identity and carries no information. */
    {
        /* Hold `us` and the LARGEST phase (the build) fixed, and move a
         * non-largest one. With `max` the gap cannot move -- which is exactly
         * why its flatness proved nothing. With `sum` it must move. */
        unsigned a = gap_max(233u, 113u, 34u, 70u);
        unsigned b = gap_max(233u, 113u, 34u, 100u);
        RI_ASSERT(a == b,
            "max-gap is blind to a non-largest phase: %u vs %u", a, b);
        RI_ASSERT(gap_sum(233u, 113u, 34u, 70u) != gap_sum(233u, 113u, 34u, 100u),
            "the sum-gap tracks a non-largest phase, which is what makes it "
            "an instrument rather than a constant");
    }

    RI_RESULT("gap_accounting");
    return 0;
}