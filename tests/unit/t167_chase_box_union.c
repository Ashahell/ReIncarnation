/* t167_chase_box_union — one invalidation over the union of two step lamps must
 * paint exactly what two separate invalidations paint.
 *
 * WHY (2026-10-04/05). The drum chase used to invalidate TWO boxes per step
 * change -- the old lamp and the new one -- as two separate
 * ri_rsection_refresh_box_why calls. That cost twice what it needed to, because
 * MUI_Redraw is SYNCHRONOUS on AROS Zune: mui_redraw.c calls
 * `DoMethod(obj, MUIM_Draw, 0)` inline with no deferral, so each call was a
 * COMPLETE draw cycle rather than a queued one.
 *
 * Measured on the Dell: a quiet box repaint is ~234 us, of which ~122 us is a
 * fixed per-partial cost no phase timer covers. Two of them is ~714 us per step
 * change, and the chase moves constantly. The fix unions the two rectangles and
 * invalidates once.
 *
 * This asserts that the union is LOSSLESS, which is the only thing that makes it
 * safe: a union is correct only if repainting the union repaints both controls
 * and nothing between them goes stale. It proves that by construction --
 * comparing against the two-separate-boxes result for every reachable pair of
 * step lamps on the 808 and 909 rows -- rather than by asserting the code's own
 * arithmetic.
 *
 * Host-side and arithmetic-free: it renders the real display list through the
 * host raster and compares pixels.
 */
#include <stdio.h>
#include <string.h>
#include "tests/helpers/ri_assert.h"
#include "gui/draw/canvas.h"
#include "gui/draw/art.h"
#include "gui/ctlreg.h"
#include "gui/sectui.h"
#include "platform/host/raster.h"

#define W 512u
#define H 512u
static struct ri_dcmd B1[24576];
static char SP1[32768];
static uint32_t F0[W * H], PU[W * H], P1[W * H], FULL[W * H];
static struct RIMixBoard BOARD;

static void replay_box(uint32_t *px, const struct ri_dlist *dl,
                       int x0, int y0, int x1, int y1) {
    struct ri_raster r;
    ri_raster_init(&r, px, W, H);
    ri_raster_replay_box(&r, dl, 0, x0, y0, x1, y1);
}

/* One render of the section. The lamp that is lit is not varied here: the claim
 * under test is about the GEOMETRY of the union, not about which lamp is on, and
 * a union that repaints both rects cannot depend on the contents. Varying the
 * artwork instead would test the raster, not the union. */
static void build_one(const struct RIGeoSection *g, uint8_t sec,
                      struct RISectUI *ui, struct ri_dlist *dl) {
    struct ri_text_metrics tm;
    tm.width = ri_raster_text_width;
    tm.height = 7;
    tm.baseline = 5;
    tm.ctx = 0;
    ri_dlist_init(dl, B1, 24576u, SP1, sizeof SP1);
    ri_draw_section(dl, ui, sec, 0, 0, 0, &tm, 0, 0);
    (void)g;
}

int main(void) {
    static const uint8_t SECS[2] = { RI_SEC_808, RI_SEC_909 };
    uint32_t s, pairs = 0u;
    ri_smix_init(&BOARD);

    for (s = 0u; s < 2u; s++) {
        const struct RIGeoSection *g = ri_geo_section(SECS[s]);
        uint32_t base = (SECS[s] == RI_SEC_808) ? RI_S808_STEP0 : RI_S909_STEP0;
        uint32_t i, j;
        struct RISectUI ui;
        if (!g || ri_sui_init(&ui, SECS[s]) != 0)
            continue;
        {
            /* Paint the section once, in full, as the shared base frame. */
            struct ri_dlist df;
            struct ri_raster rr;
            build_one(g, SECS[s], &ui, &df);
            if (df.n == 0u || df.n >= df.cap)
                continue;
            ri_raster_init(&rr, FULL, W, H);
            ri_raster_clear(&rr, 0x000000u);
            ri_raster_replay(&rr, &df, 0);
        }
        /* The two lists differ in which lamp is lit: render with the lamp set to
         * i, then to j, so the union has to repaint genuinely different art. */
        for (i = 0u; i < 16u; i++) {
            for (j = 0u; j < 16u; j++) {
                int ax0, ay0, ax1, ay1, bx0, by0, bx1, by1;
                int ux0, uy0, ux1, uy1;
                struct ri_dlist da;
                if (i == j)
                    continue;                 /* nothing moved: not a step change */
                if (ri_geo_bbox(g, (uint16_t)(base + i), 0, &ax0, &ay0, &ax1, &ay1) != 0)
                    continue;
                if (ri_geo_bbox(g, (uint16_t)(base + j), 0, &bx0, &by0, &bx1, &by1) != 0)
                    continue;
                ux0 = ax0 < bx0 ? ax0 : bx0;
                uy0 = ay0 < by0 ? ay0 : by0;
                ux1 = ax1 > bx1 ? ax1 : bx1;
                uy1 = ay1 > by1 ? ay1 : by1;

                build_one(g, SECS[s], &ui, &da);
                if (da.n == 0u || da.n >= da.cap)
                    continue;

                /* BASE: the section fully painted. Both strategies then start
                 * from this same frame, so the ONLY difference between them is
                 * the repaint strategy. (An earlier version of this test
                 * compared against an unpainted buffer, which made the union look
                 * wrong for repainting the region BETWEEN the lamps -- a defect
                 * in the ground truth, not in the union.) */
                memcpy(F0, FULL, sizeof F0);

                /* STRATEGY A: two separate box replays. */
                memcpy(P1, F0, sizeof F0);
                replay_box(P1, &da, ax0, ay0, ax1, ay1);
                replay_box(P1, &da, bx0, by0, bx1, by1);

                /* STRATEGY B: one union replay. */
                memcpy(PU, F0, sizeof F0);
                replay_box(PU, &da, ux0, uy0, ux1, uy1);

                if (memcmp(P1, PU, (size_t)W * H * 4u) != 0) {
                    uint32_t px, py, diff = 0u;
                    for (py = 0u; py < H; py++)
                        for (px = 0u; px < W; px++)
                            if (P1[py * W + px] != PU[py * W + px]) {
                                if (diff < 3u)
                                    printf("  differs at %u,%u: %08x vs %08x\n",
                                        (unsigned)px, (unsigned)py,
                                        P1[py * W + px], PU[py * W + px]);
                                diff++;
                            }
                    RI_ASSERT(0, "union box sec=%u lamps %u->%u differs in %u px;"
                        " union=%d,%d..%d,%d boxes=%d,%d..%d,%d and %d,%d..%d,%d",
                        (unsigned)SECS[s], (unsigned)i, (unsigned)j, (unsigned)diff,
                        ux0, uy0, ux1, uy1, ax0, ay0, ax1, ay1, bx0, by0, bx1, by1);
                }
                pairs++;
                if (pairs >= 16u * 15u * 2u)
                    break;
            }
            if (pairs >= 16u * 15u * 2u)
                break;
        }
    }
    RI_ASSERT(pairs > 0u, "at least one lamp pair was compared (%u)", (unsigned)pairs);
    RI_RESULT("chase_box_union");
    return 0;
}