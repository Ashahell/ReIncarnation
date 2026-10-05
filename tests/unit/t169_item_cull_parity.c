/* t169_item_cull_parity — the item cull must not change a single pixel.
 *
 * WHY (2026-10-05). With dp_gap corrected to subtract the SUM of its phases
 * (t168), a quiet box repaint on the Dell is 233 us = 112 build + 33 replay +
 * 69 blit + 18 unattributed, so the build is the largest real term. The damage
 * clip (bb1c385) drops commands at PUSH time, so it saved the replay but not
 * the drawing. Measured host-side (tests/unit/bench_build.c), the per-item loop
 * is 70-93 % of a build -- 808 emits 3431 of 3605 commands from 95 items, 909
 * 4603 of 4967 from 87 -- and a 64x16 damage box keeps 2 %.
 *
 * art_section.c now skips an item whose own ri_geo_item_box is disjoint from
 * the clip. THE CLAIM IS AN EQUIVALENCE, and an equivalence is exactly the
 * kind of claim that is wrong in one direction only: culling too little is
 * merely slow, culling too much puts a hole on screen. So this proves the
 * dangerous direction and nothing else.
 *
 * THE PROOF. For every section, and for every item in it, build the section
 * with the clip set to that item's OWN box, then compare the command stream
 * against the unclipped build filtered by ri_dcmd_hits_box -- which is the
 * production predicate both backends use to skip a command. If the two are
 * identical command for command, then skipping every item outside the clip
 * keeps exactly what drawing them would have kept. Doing it per item rather
 * than with one box matters: a single box would pass while one item's box is
 * wrong, because the surviving commands would come from its neighbours.
 *
 * WHAT THIS DOES NOT PROVE, STATED PLAINLY. It does not prove the cull is
 * PRESENT. With the cull removed, every assertion here still passes, because
 * the push-time clip produces the same list -- that is the point of the
 * equivalence. Presence is a performance property and bench_build measures it
 * (clipped build 490 -> 156 us across the 21 sections, 3.1x). The division is
 * deliberate: this file is the safety proof, the bench is the win, and a test
 * that claimed to be both would be claiming the cull by asserting it happened.
 *
 * The goldens (t92/t93) are the second witness and they must not move: they
 * render with no clip set, and with no clip the cull is not even reached.
 */
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "tests/helpers/ri_assert.h"
#include "gui/sectui.h"
#include "gui/sectmix.h"
#include "gui/ctlreg.h"
#include "gui/panelgeo.h"
#include "gui/draw/canvas.h"
#include "gui/draw/art.h"

static struct ri_dcmd FULL[24576], CULLED[24576];
static char SPOOL_FULL[65536], SPOOL_CULLED[65536];
static struct RIMixBoard BOARD;

static struct ri_text_metrics TM;

static int tw(void *ctx, const char *s) {
    (void)ctx;
    return s ? (int)strlen(s) * 6 : 0;
}

/* Build the whole section with no clip: the reference stream. */
static void build_full(uint8_t sec, struct RISectUI *ui, struct ri_dlist *dl,
    int ox, int oy) {
    ri_dlist_init(dl, FULL, 24576u, SPOOL_FULL, sizeof SPOOL_FULL);
    ri_draw_section(dl, ui, sec, 0, ox, oy, &TM, 0, 0);
}

/* Build it again with the clip set -- this is the path the cull changes. */
static void build_clipped(uint8_t sec, struct RISectUI *ui, struct ri_dlist *dl,
    int x0, int y0, int x1, int y1, int ox, int oy) {
    ri_dlist_init(dl, CULLED, 24576u, SPOOL_CULLED, sizeof SPOOL_CULLED);
    ri_dlist_set_clip(dl, x0, y0, x1, y1);
    ri_draw_section(dl, ui, sec, 0, ox, oy, &TM, 0, 0);
    ri_dlist_clear_clip(dl);
}

static int same_cmd(const struct ri_dcmd *a, const struct ri_dcmd *b) {
    if (a->op != b->op || a->x0 != b->x0 || a->y0 != b->y0 || a->x1 != b->x1 ||
        a->y1 != b->y1 || a->rgb != b->rgb || a->img != b->img ||
        a->frame != b->frame || a->align != b->align || a->pad[0] != b->pad[0])
        return 0;
    if ((a->text == NULL) != (b->text == NULL))
        return 0;
    if (a->text && strcmp(a->text, b->text) != 0)
        return 0;
    return 1;
}

/* The reference semantics: what the push-time clip produces, derived from the
 * unclipped build. ri_dcmd_hits_box is the production predicate, not a copy. */
static uint32_t expect_count(const struct ri_dlist *dl, int x0, int y0, int x1, int y1) {
    uint32_t i, n = 0u;
    for (i = 0u; i < dl->n; i++)
        if (ri_dcmd_hits_box(&dl->cmd[i], x0, y0, x1, y1))
            n++;
    return n;
}

static void expect_cmd(const struct ri_dlist *dl, int x0, int y0, int x1, int y1,
    uint32_t k, struct ri_dcmd *out) {
    uint32_t i, j = 0u;
    for (i = 0u; i < dl->n; i++) {
        if (!ri_dcmd_hits_box(&dl->cmd[i], x0, y0, x1, y1))
            continue;
        if (j++ == k) {
            *out = dl->cmd[i];
            return;
        }
    }
}

int main(void) {
    struct RISectUI ui;
    struct ri_dlist dlF, dlC;
    uint32_t sec, nsec = 0u, nitem = 0u, ncand = 0u, nkept = 0u;
    uint32_t nnonempty = 0u, nout_empty = 0u, nwhole = 0u, noff = 0u;

    TM.width = tw;
    TM.height = 7;
    TM.baseline = 5;
    TM.ctx = 0;
    ri_smix_init(&BOARD);

    for (sec = 0u; sec < RI_SEC_COUNT; sec++) {
        const struct RIGeoSection *g =
            ri_geo_section(sec == RI_SEC_SYNTH2 ? RI_SEC_SYNTH1 : sec);
        uint32_t i;
        if (!g)
            continue;
        if (ri_sui_init(&ui, (uint8_t)sec) != 0)
            continue;
        if (ri_smix_strip(sec) >= 0 || sec == RI_SEC_MASTER)
            ri_sui_bind_board(&ui, &BOARD);
        nsec++;

        /* (1) PER ITEM. Clip == this item's own box. Everything the item and
         * its neighbours can paint into that box must survive identically. */
        for (i = 0u; i < g->nitems; i++) {
            int bx0, by0, bx1, by1;
            struct ri_dcmd want;
            uint32_t k, nexp;
            nitem++;
            if (ri_geo_item_box(&g->items[i], 0, &bx0, &by0, &bx1, &by1) != 0)
                continue;   /* no damage box: drawn, never culled */
            ncand++;
            build_full((uint8_t)sec, &ui, &dlF, 0, 0);
            nexp = expect_count(&dlF, bx0, by0, bx1, by1);
            build_clipped((uint8_t)sec, &ui, &dlC, bx0, by0, bx1, by1, 0, 0);
            if (nexp == 0u)
                continue;
            nnonempty++;
            /* A box that keeps NOTHING would pass vacuously, so the count is
             * pinned from the reference before the streams are compared. */
            RI_ASSERT(dlC.n == nexp,
                "sec=%u item=%u box=%d,%d..%d,%d: culled %u commands, "
                "the clip alone would keep %u", sec, i, bx0, by0, bx1, by1,
                dlC.n, nexp);
            for (k = 0u; k < nexp; k++) {
                expect_cmd(&dlF, bx0, by0, bx1, by1, k, &want);
                RI_ASSERT(same_cmd(&dlC.cmd[k], &want),
                    "sec=%u item=%u cmd %u differs", sec, i, k);
            }
            nkept += nexp;
        }

        /* (2) AN ARBITRARY BOX, including one that matches nothing and one
         * that covers the whole section. The first catches a cull that drops
         * something the clip would have kept anywhere; the second is the
         * no-op case, where a cull must not drop anything at all. */
        {
            int w = ri_geo_px((int)g->w, 0), h = ri_geo_px((int)g->h, 0);
            int OFFS[4][4] = {
                { 0, 0, 0, 0 },        /* degenerate: an empty box paints nothing */
                { -9, -9, -1, -1 },    /* wholly outside the section */
                { 1, 1, 33, 17 },     /* a small interior box, offset origin */
                { -4, -4, w + 8, h + 8 }, /* the whole section, over-sized */
            };
            int oi;
            for (oi = 0; oi < 4; oi++) {
                uint32_t nexp;
                int x0 = OFFS[oi][0], y0 = OFFS[oi][1];
                int x1 = OFFS[oi][2], y1 = OFFS[oi][3];
                build_full((uint8_t)sec, &ui, &dlF, 0, 0);
                nexp = expect_count(&dlF, x0, y0, x1, y1);
                build_clipped((uint8_t)sec, &ui, &dlC, x0, y0, x1, y1, 0, 0);
                /* The off-section box must keep far less than the whole
                 * section, which is what makes it a control on the cull rather
                 * than just another small box. It is NOT required to keep
                 * nothing: PAT-909, PAT-LEVI and MIX-LEVI each emit two
                 * commands at negative coordinates (the pattern-section art
                 * reaches left of the origin), and ri_dcmd_hits_box is right to
                 * report them. Assuming otherwise would have been an assertion
                 * about my own imagination of the art. */
                if (oi == 1) {
                    RI_ASSERT(nexp * 20u < dlF.n,
                        "sec=%u off-section box kept %u of %u", sec, nexp,
                        dlF.n);
                    if (nexp * 20u < dlF.n)
                        nout_empty++;
                }
                /* And the over-sized box must keep the whole section, so the
                 * cull cannot be quietly deleting items it should have drawn. */
                if (oi == 3) {
                    RI_ASSERT(nexp == dlF.n, "sec=%u whole-section box kept %u of %u",
                        sec, nexp, dlF.n);
                    nwhole++;
                }
                RI_ASSERT(dlC.n == nexp,
                    "sec=%u box=%d,%d..%d,%d: culled %u, expected %u", sec,
                    x0, y0, x1, y1, dlC.n, nexp);
                {
                    uint32_t k;
                    for (k = 0u; k < nexp; k++) {
                        struct ri_dcmd want;
                        expect_cmd(&dlF, x0, y0, x1, y1, k, &want);
                        RI_ASSERT(same_cmd(&dlC.cmd[k], &want),
                            "sec=%u box=%d,%d..%d,%d cmd %u differs", sec,
                            x0, y0, x1, y1, k);
                    }
                }
            }
        }

        /* (3) A NON-ZERO ORIGIN WITH A CLIP. Dropping `ox`/`oy` from the cull's
         * test survived every other case, because production only ever sets a
         * clip on the partial path, which builds at ox = oy = 0 -- so the offset
         * arithmetic was unexercised and therefore untested. This arms it: the
         * box is expressed in the same space as the commands, which is the only
         * convention ri_dcmd_hits_box uses, so a cull that ignored the offset
         * would drop items that really do paint inside the box. */
        {
            static const int OX = 37, OY = 23;
            int w = ri_geo_px((int)g->w, 0), h = ri_geo_px((int)g->h, 0);
            int OB[3][4] = {
                { 1, 1, 40, 20 },            /* a small box inside the origin */
                { 200, 100, 260, 150 },      /* elsewhere in the same section */
                { -10, -10, w + 20, h + 20 }, /* everything */
            };
            int oi;
            for (oi = 0; oi < 3; oi++) {
                int x0 = OB[oi][0] + OX, y0 = OB[oi][1] + OY;
                int x1 = OB[oi][2] + OX, y1 = OB[oi][3] + OY;
                uint32_t nexp, k;
                build_full((uint8_t)sec, &ui, &dlF, OX, OY);
                nexp = expect_count(&dlF, x0, y0, x1, y1);
                build_clipped((uint8_t)sec, &ui, &dlC, x0, y0, x1, y1, OX, OY);
                RI_ASSERT(dlC.n == nexp,
                    "sec=%u offset box=%d,%d..%d,%d: culled %u, expected %u",
                    sec, x0, y0, x1, y1, dlC.n, nexp);
                for (k = 0u; k < nexp; k++) {
                    struct ri_dcmd want;
                    expect_cmd(&dlF, x0, y0, x1, y1, k, &want);
                    RI_ASSERT(same_cmd(&dlC.cmd[k], &want),
                        "sec=%u offset box=%d,%d..%d,%d cmd %u differs", sec,
                        x0, y0, x1, y1, k);
                }
                if (oi == 0)
                    noff++;
            }
        }
    }

    RI_ASSERT(nsec >= 18u, "only %u sections", nsec);
    RI_ASSERT(nitem >= 600u, "only %u items", nitem);
    RI_ASSERT(ncand >= 500u, "only %u items carry a damage box", ncand);
    RI_ASSERT(nnonempty >= 200u, "only %u per-item boxes kept anything",
        nnonempty);
    /* The degenerate/off-section boxes must exist, or case (2) is not testing
     * the empty path -- a cull that over-draws an empty box is still a bug. */
    RI_ASSERT(nout_empty >= 18u, "the off-section box was empty in only %u sections",
        nout_empty);
    RI_ASSERT(nwhole >= 18u, "the whole-section box was checked in only %u sections",
        nwhole);
    RI_ASSERT(noff >= 18u, "the offset arm ran in only %u sections", noff);
    /* Volume: the parity above is only worth something if it compared a real
     * number of commands, so the count is pinned rather than left implied. */
    RI_ASSERT(nkept >= 5000u, "only %u commands compared in the per-item pass",
        nkept);
    RI_RESULT("item_cull_parity");
    return 0;
}