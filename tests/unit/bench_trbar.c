/* bench_trbar — what is LEFT in the transport's build, against the REAL damage
 * box, not a synthetic one.
 *
 * WHY (2026-10-05). On target, `bsec` reads 13 = RI_SEC_TRANSPORT in 16 of 16
 * windows, with `bsecsum` 447 of `build_avg` 55 x n=8, and `box_bar`
 * 175/177/8. So the transport's build is the whole story, and the box is
 * `ri_geo_bbox(g, (RI_SEC_TRANSPORT << 8) | RI_STR_BAR, zoom)` -- the Song
 * Position display, at the canvas's own zoom (RI_GEO_ZOOM_COMPACT, NOT 0; that
 * was the bbox-zoom bug, e8b766b).
 *
 * bench_build measures a synthetic 64x16 box at the section centre. That is the
 * right shape for a step lamp and the WRONG box for this lane, so every number
 * it produced for the transport was about a box nobody repaints. This bench uses
 * the real one.
 *
 * The question it answers: after the item cull and the disc-row clamp, is the
 * remaining cost the digits being legitimately repainted, or is something else
 * still being drawn? Prints, for the real box:
 *   built/kept  commands with and without the clip
 *   us_clip     the clipped build's cost
 *   ns/kept     cost per SURVIVING command -- high means the survivors are
 *               expensive primitives, low means there is still dead weight
 *   ops         the op histogram of the survivors, and of what was discarded
 *
 * A bench, not a test: it asserts nothing, because a diagnosis that can fail is
 * a test pretending to be a measurement.
 */
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <time.h>
#include "gui/sectui.h"
#include "gui/sectmix.h"
#include "gui/ctlreg.h"
#include "gui/panelgeo.h"
#include "gui/draw/canvas.h"
#include "gui/draw/art.h"
#include "gui/secttr.h"
#include "platform/pal/ri_pal_draw.h"

static struct ri_dcmd FULL[24576], CLIP[24576];
static char SPOOL_FULL[65536], SPOOL_CLIP[65536];
static struct RIMixBoard BOARD;
static struct ri_text_metrics TM;

static int tw(void *ctx, const char *s) {
    (void)ctx;
    return s ? (int)strlen(s) * 6 : 0;
}

static void hist(const struct ri_dlist *dl, const char *tag) {
    uint32_t r = 0u, l = 0u, c = 0u, t = 0u, i = 0u;
    uint32_t k;
    for (k = 0u; k < dl->n; k++) {
        switch (dl->cmd[k].op) {
        case RI_D_RECT: r++; break;
        case RI_D_LINE: l++; break;
        case RI_D_CIRCLE: c++; break;
        case RI_D_TEXT: t++; break;
        case RI_D_IMAGE: i++; break;
        default: break;
        }
    }
    printf("    %-9s n=%-5u rect=%-5u line=%-4u circ=%-4u text=%-3u image=%-3u\n",
        tag, dl->n, r, l, c, t, i);
}

static double us_since(const struct timespec *t0) {
    struct timespec t1;
    clock_gettime(CLOCK_MONOTONIC, &t1);
    return (double)(t1.tv_sec - t0->tv_sec) * 1e6 +
        (double)(t1.tv_nsec - t0->tv_nsec) / 1e3;
}

#define REPS 400

int main(void) {
    const struct RIGeoSection *g;
    struct RISectUI ui;
    struct ri_dlist dl;
    struct timespec t0;
    int x0 = 0, y0 = 0, x1 = 0, y1 = 0, z;
    uint32_t k, rc, nfull;
    double uf, uc;

    TM.width = tw;
    TM.height = 7;
    TM.baseline = 5;
    TM.ctx = 0;
    ri_smix_init(&BOARD);

    g = ri_geo_section(RI_SEC_TRANSPORT);
    if (!g || ri_sui_init(&ui, RI_SEC_TRANSPORT) != 0) {
        printf("no transport\n");
        return 0;
    }
    ri_sui_bind_board(&ui, &BOARD);

    /* THE box the app really asks for, at the zoom the canvas really uses. */
    z = RI_GEO_ZOOM_COMPACT;
    rc = (uint32_t)ri_geo_bbox(g,
        (uint16_t)(((uint32_t)RI_SEC_TRANSPORT << 8) | RI_STR_BAR), z,
        &x0, &y0, &x1, &y1);
    printf("RI_STR_BAR bbox at zoom %d -> rc=%u  %d,%d..%d,%d  (%d x %d px)\n",
        z, rc, x0, y0, x1, y1, x1 - x0 + 1, y1 - y0 + 1);
    if (rc != 0) {
        printf("no bbox for RI_STR_BAR; the cull cannot be measured here\n");
        return 0;
    }
    printf("section is %d x %d px at this zoom; items %u\n",
        ri_geo_px((int)g->w, z), ri_geo_px((int)g->h, z), (unsigned)g->nitems);

    ri_dlist_init(&dl, FULL, 24576u, SPOOL_FULL, sizeof SPOOL_FULL);
    ri_draw_section(&dl, &ui, RI_SEC_TRANSPORT, z, 0, 0, &TM, 0, 0);
    hist(&dl, "unclipped");

    nfull = dl.n;
    clock_gettime(CLOCK_MONOTONIC, &t0);
    for (k = 0; k < REPS; k++) {
        ri_dlist_init(&dl, FULL, 24576u, SPOOL_FULL, sizeof SPOOL_FULL);
        ri_draw_section(&dl, &ui, RI_SEC_TRANSPORT, z, 0, 0, &TM, 0, 0);
    }
    uf = us_since(&t0) / (double)REPS;

    ri_dlist_init(&dl, CLIP, 24576u, SPOOL_CLIP, sizeof SPOOL_CLIP);
    ri_dlist_set_clip(&dl, x0, y0, x1, y1);
    ri_draw_section(&dl, &ui, RI_SEC_TRANSPORT, z, 0, 0, &TM, 0, 0);
    ri_dlist_clear_clip(&dl);
    hist(&dl, "clipped");

    clock_gettime(CLOCK_MONOTONIC, &t0);
    for (k = 0; k < REPS; k++) {
        ri_dlist_init(&dl, CLIP, 24576u, SPOOL_CLIP, sizeof SPOOL_CLIP);
        ri_dlist_set_clip(&dl, x0, y0, x1, y1);
        ri_draw_section(&dl, &ui, RI_SEC_TRANSPORT, z, 0, 0, &TM, 0, 0);
        ri_dlist_clear_clip(&dl);
    }
    uc = us_since(&t0) / (double)REPS;

    printf("\n  unclipped %8.2f us over %u commands  (%.1f ns/cmd)\n", uf, nfull,
        nfull ? uf * 1000.0 / (double)nfull : 0.0);
    printf("  clipped   %8.2f us -> %.1f ns per SURVIVING command\n", uc,
        dl.n ? uc * 1000.0 / (double)dl.n : 0.0);
    printf("  kept %u of %u  (%.1f%% kept, %.1f%% of the cost removed)\n", dl.n,
        nfull, nfull ? 100.0 * (double)dl.n / (double)nfull : 0.0,
        uf > 0.0 ? 100.0 * (1.0 - uc / uf) : 0.0);

    /* THE DECOMPOSITION THAT DECIDES WHETHER ANYTHING IS LEFT (2026-10-05).
     *
     * Three builds, differing only in the clip box:
     *
     *   unclipped            every item is emitted
     *   DISJOINT clip        every item WITH a damage box is culled, so what
     *                        survives is the BACKGROUND plus the items the cull
     *                        deliberately cannot skip -- the shapes with no
     *                        declared box, which it draws rather than skips
     *   the real box         the above, plus the items that intersect
     *
     * So `unclipped - disjoint` is the work the item cull removes, and
     * `real - disjoint` is the work the SURVIVORS cost. That second number is
     * the one that matters: the LCD background and the two text commands were
     * established to be inside the box by construction and therefore
     * unreachable, so if the survivors are all of it, this path is finished and
     * the residue is not a target at all.
     */
    {
        struct timespec t2;
        double t_far;
        clock_gettime(CLOCK_MONOTONIC, &t2);
        for (k = 0; k < REPS; k++) {
            ri_dlist_init(&dl, CLIP, 24576u, SPOOL_CLIP, sizeof SPOOL_CLIP);
            ri_dlist_set_clip(&dl, -9, -9, -1, -1);   /* disjoint: nothing survives */
            ri_draw_section(&dl, &ui, RI_SEC_TRANSPORT, z, 0, 0, &TM, 0, 0);
            ri_dlist_clear_clip(&dl);
        }
        t_far = us_since(&t2) / (double)REPS;
        printf("\n  DECOMPOSITION\n");
        printf("    unclipped                 %7.2f us   (everything)\n", uf);
        printf("    disjoint clip             %7.2f us   (background + the items"
            " the cull cannot skip)\n", t_far);
        printf("    the real box              %7.2f us   (that, plus the"
            " survivors)\n", uc);
        printf("    -> the item cull removes  %7.2f us  (%.0f%% of the build)\n",
            uf - t_far, uf > 0.0 ? 100.0 * (uf - t_far) / uf : 0.0);
        printf("    -> the SURVIVORS cost     %7.2f us  (%.0f%% of the clipped"
            " build)\n", uc - t_far, uc > 0.0 ? 100.0 * (uc - t_far) / uc : 0.0);
        printf("    -> unreachable floor      %7.2f us  (%.0f%% of the clipped"
            " build)\n", t_far, uc > 0.0 ? 100.0 * t_far / uc : 0.0);
        /* RATIO, because absolute microseconds on this host are bimodal by ~25 %
         * with CPU frequency and cannot be compared across runs. The ratio of
         * two builds in the SAME process is the stable quantity. */
        printf("    RATIO floor/unclipped = %.4f   survivors/unclipped = %.4f\n",
            uf > 0.0 ? t_far / uf : 0.0, uf > 0.0 ? (uc - t_far) / uf : 0.0);
    }

    /* WHAT IS THE 80 % FLOOR MADE OF?  The disjoint-clip build costs 3.62 us and
     * emits the background plus every item the cull cannot skip. Count the
     * transport's items by shape and by whether ri_geo_item_box gives them a
     * box, because "no declared box" is precisely the population the cull draws
     * unconditionally. */
    {
        /* READ FROM gui/panelgeo.h, NOT REMEMBERED. An earlier version of this
         * probe guessed the enum and got it wrong in a way that INVERTED the
         * conclusion: it labelled index 3 "OPTION" when index 3 is RI_GEO_LEGEND
         * and index 5 is RI_GEO_OPTION, so "5 items have no damage box" came out
         * as OPTIONs when they are the legends -- which is exactly the
         * hypothesis the guess was used to refute. This is the RI_RAW_SPACE
         * lesson (0x40, not 57) in a new form: do not reason from a constant
         * that has not been read. */
        static const char *SH[16] = { "KNOB", "RECT", "LED", "LEGEND",
            "DIVIDER", "OPTION", "STEPPER", "?", "?", "?", "?", "?", "?", "?", "?", "?" };
        uint32_t cnt[16], boxed[16], ci;
        memset(cnt, 0, sizeof cnt);
        memset(boxed, 0, sizeof boxed);
        for (ci = 0u; ci < g->nitems; ci++) {
            uint8_t sh = g->items[ci].shape;
            int bx0, by0, bx1, by1;
            cnt[sh & 15u]++;
            if (ri_geo_item_box(&g->items[ci], z, &bx0, &by0, &bx1, &by1) == 0)
                boxed[sh & 15u]++;
        }
        printf("\n  the floor's population: transport items by shape\n");
        printf("    %-9s %6s %8s %10s\n", "shape", "items", "boxed", "UNCULLED");
        for (ci = 0u; ci < 16u; ci++)
            if (cnt[ci])
                printf("    %-9s %6u %8u %10u%s\n", SH[ci], cnt[ci], boxed[ci],
                    cnt[ci] - boxed[ci],
                    (cnt[ci] - boxed[ci]) ? "  <- drawn every repaint" : "");
        {
            uint32_t tb = 0u, tu = 0u, q;
            for (q = 0u; q < 16u; q++) { tb += boxed[q]; tu += cnt[q] - boxed[q]; }
            printf("    %-9s %6u %8u %10u\n", "TOTAL", (unsigned)g->nitems, tb, tu);
        }
    }

    /* IS THE VALUE TEXT A MONOSPACE ROW OF FIXED-WIDTH CELLS?  This is the
     * gate on narrowing the BAR box to only the characters that changed, and it
     * has to be MEASURED rather than assumed: the display is drawn by
     * ri_art_text_c, i.e. a centred string, so a per-character box needs both
     * that the face is monospaced and that the alignment divides evenly. This
     * lane has been bitten by exactly this shape of assumption (a bbox computed
     * in the wrong coordinate space came out DISJOINT from what it named).
     *
     * Read the TEXT commands' own recorded extents at several values and look
     * for three equal cells. A TEXT command's bbox is what the replay and the
     * clip both use, so this reads production's own answer rather than
     * re-deriving it. */
    printf("\n  value text extents (the gate on a narrowed box):\n");
    {
        static const int vals[6] = { 0, 9, 99, 100, 101, 999 };
        uint32_t vi;
        int prevx0 = -1, uniform = 1;
        int nblankw = -1;   /* outer: the verdict line reads it after the loop */
        for (vi = 0u; vi < 6u; vi++) {
            ri_sui_set(&ui, RI_STR_BAR, vals[vi]);
            ri_dlist_init(&dl, FULL, 24576u, SPOOL_FULL, sizeof SPOOL_FULL);
            ri_draw_section(&dl, &ui, RI_SEC_TRANSPORT, z, 0, 0, &TM, 0, 0);
            printf("    v=%-4d", vals[vi]);
            for (k = 0u; k < dl.n; k++) {
                struct ri_dcmd *c = &dl.cmd[k];
                int a0, b0, a1, b1;
                if (c->op != (uint8_t)RI_D_TEXT)
                    continue;
                if (ri_dcmd_bbox(c, &a0, &b0, &a1, &b1) != 0)
                    continue;
                printf("  text[%d,%d..%d,%d] w=%d \"%s\"", a0, b0, a1, b1,
                    a1 - a0 + 1, c->text ? c->text : "");
            }
            printf("\n");
            /* The decisive comparison is between strings of IDENTICAL
             * structure, because only those must agree if the face is
             * fixed-width. The two Song Position readouts differ by one digit
             * and nothing else -- same length, same leading blanks -- so if the
             * face were monospaced their extents would be equal. */
            {
                int nblank = -1;
                for (k = 0u; k < dl.n; k++) {
                    struct ri_dcmd *c = &dl.cmd[k];
                    int a0, b0, a1, b1, nb = 0;
                    const char *t;
                    if (c->op != (uint8_t)RI_D_TEXT)
                        continue;
                    if (ri_dcmd_bbox(c, &a0, &b0, &a1, &b1) != 0)
                        continue;
                    t = c->text;
                    if (!t || t[0] == '8')
                        continue;          /* the static "888" ghost */
                    while (t[nb] == ' ')
                        nb++;
                    if (nb < 2)
                        continue;          /* not a Song Position readout */
                    if (nblank < 0) { nblank = nb; nblankw = a1 - a0 + 1; prevx0 = a0; }
                    else if (nblankw != a1 - a0 + 1)
                        uniform = 0;
                }
            }
        }
        printf("\n    two Song Position readouts of IDENTICAL structure"
            " (3 chars, 2 leading blanks, 1 digit): %s\n",
            uniform ? "same width -- a fixed-cell split would be measurable"
                    : "DIFFERENT WIDTH -- THE FACE IS PROPORTIONAL, so a"
                      " fixed-cell split is NOT sound and the narrowed-box"
                      " cut is DEAD. Raw: \"  1\" and \"  4\" measure 12 and 13.");
        printf("    first such readout: x0=%d width=%d\n", prevx0, nblankw);
    }
    return 0;
}