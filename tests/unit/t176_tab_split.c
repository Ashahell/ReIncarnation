/* t176_tab_split — the full-draw split partitions the switch (tab-switch B0).
 *
 * WHY. Every tab switch re-renders every canvas on the new page from
 * scratch, and the question B0 answers is whether that re-render is ours
 * (display-list build + replay, removable by a replay skip) or MUI's and
 * the blit's (not removable). The TAB probe therefore carries, per switch:
 *   fn = full draws, fb = build us, fr = replay us, fl = blit us,
 *   mui = page_us - (fb + fr + fl).
 * The three spans abut exactly (one shared EClock sample ends the build and
 * starts the replay, another ends the replay and starts the blit), so they
 * partition the draw the way dp_build/dp_replay/dp_blit partition a partial
 * (t168's rule, one level up).
 *
 * The widget is AROS-only, so like t168 this pins the RULE by transcription:
 * the four parts partition page_us, mui never goes negative (disjoint spans
 * by construction — a negative mui means the instrument overlaps and every
 * number from it is suspect), and zero draws means zero sums.
 */
#include <stdio.h>
#include "tests/helpers/ri_assert.h"

/* The partition rule, transcribed from gui/widgets/rsection.mcc.c (full-draw
 * tail) and app/riapp.c (tab_switch). Keeping it here as a literal rather
 * than calling into the widget is deliberate: the point is to pin the RULE,
 * and a helper both sides share could be wrong in one place and consistent. */
static long tab_mui(long page_us, unsigned fb, unsigned fr, unsigned fl) {
    return page_us - (long)(fb + fr + fl);
}

/* What the probe must never do: forget the blit in mui. Never ship this. */
static long tab_mui_noblit(long page_us, unsigned fb, unsigned fr) {
    (void)fr;
    return page_us - (long)fb - (long)fr;
}

int main(void) {
    /* A recorded-shape switch: page 21780 us, six full draws. */
    struct { long page; unsigned fb, fr, fl, fn; long want_mui; } cases[] = {
        { 21780L, 9000u, 6000u, 1500u, 6u, 5280L },
        { 29897L, 12000u, 9000u, 2000u, 8u, 6897L },
        { 37545L, 15000u, 11000u, 2500u, 9u, 9045L },
        /* Degenerate: diag off, everything zero, mui == page. */
        { 21780L, 0u, 0u, 0u, 0u, 21780L },
        { 0L, 0u, 0u, 0u, 0u, 0L },
    };
    unsigned i;

    for (i = 0u; i < sizeof cases / sizeof cases[0]; i++) {
        long mui = tab_mui(cases[i].page, cases[i].fb, cases[i].fr,
            cases[i].fl);
        RI_ASSERT(mui == cases[i].want_mui, "case %u: mui %ld, want %ld", i,
            mui, cases[i].want_mui);
        /* THE PARTITION: the parts and mui add back to the total exactly. */
        RI_ASSERT((long)(cases[i].fb + cases[i].fr + cases[i].fl) + mui ==
                cases[i].page,
            "case %u: parts + mui must equal page_us", i);
        /* mui never negative on disjoint spans. */
        RI_ASSERT(mui >= 0L, "case %u: negative mui %ld means overlap", i,
            mui);
    }

    /* Zero draws means zero sums: the accumulation discipline (sums and fn
     * advance together, per canvas, per draw). */
    RI_ASSERT(tab_mui(20000L, 0u, 0u, 0u) == 20000L,
        "no draws: mui must be the whole page");

    /* Shared samples make the three spans abut with no residue: with sample
     * ticks t0 < tA < tB < tC, fb + fr + fl == total exactly (no rounding
     * bucket, unlike wall-clock differencing). */
    {
        unsigned t0 = 1000u, tA = 9000u, tB = 15000u, tC = 16500u;
        unsigned fb = tA - t0, fr = tB - tA, fl = tC - tB;
        RI_ASSERT(fb + fr + fl == tC - t0,
            "shared samples must partition exactly");
    }

    /* AND THE CONTRAST, so the failure mode is named: forgetting the blit
     * books it into mui, inflating MUI's share by exactly fl. */
    {
        long right = tab_mui(21780L, 900u * 10u, 600u * 10u, 150u * 10u);
        long wrong = tab_mui_noblit(21780L, 900u * 10u, 600u * 10u);
        RI_ASSERT(wrong - right == 1500L,
            "the forgotten blit lands in mui: %ld vs %ld", wrong, right);
    }

    RI_RESULT("tab_split");
    return 0;
}
