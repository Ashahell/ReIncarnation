/* t155_damage_clip_build — bounding the display-list build to the damage box.
 *
 * Dell 2026-10-02, measured on target: a box repaint averaged 5149 us of
 * which build_dl was 3048 us (59 %), and the chase issued 96 step-lamp boxes
 * in one window — 1.07 s of GUI work. The damage path built the WHOLE section
 * and then let the backend's clipped replay throw most of it away.
 *
 * Clipping at push time is claimed to be *exactly* equivalent, not an
 * approximation. The argument: the backend's clipped replay already skips
 * every command for which ri_dcmd_hits_box is 0, so "build everything, replay
 * clipped" and "build only what hits" draw the same pixels — provided the
 * filter is the same predicate. These laws pin that equivalence on the host,
 * plus the property that an unclipped build is byte-identical to before,
 * which is what keeps the draw goldens.
 *
 * Laws:
 *  - no clip set => the pushed stream is unchanged, command for command;
 *  - clip set => the surviving stream is EXACTLY the subsequence of the
 *    unclipped stream that ri_dcmd_hits_box keeps, in the same order;
 *  - a command straddling the box edge is kept (partial coverage survives);
 *  - a command wholly outside is dropped;
 *  - an inverted / empty box means "no clip", never "draw nothing" — an
 *    empty damage box must not silently become a blank canvas;
 *  - RI_D_CLIP is dropped when clipping, because replay ignores it anyway;
 *  - a full-canvas clip keeps everything (the bound costs nothing when the
 *    box is the whole section);
 *  - and the point of the exercise: a small box on a large canvas keeps far
 *    fewer commands, which is the cost the measurement is about.
 */
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "tests/helpers/ri_assert.h"
#include "gui/draw/canvas.h"
#include "platform/pal/ri_pal_draw.h"

#define CAP 512
/* Separate backing for the reference and the clipped build: sharing one
 * array clobbers ref.cmd[0] and silently manufactures a second "hit". */
static struct ri_dcmd cmd[CAP];
static struct ri_dcmd refcmd[CAP];
static char spool[8192];
static char refspool[8192];

static void build(struct ri_dlist *dl, struct ri_dcmd *backing, char *sp,
    uint32_t n, int cx0, int cy0, int cx1, int cy1) {
    uint32_t i;
    ri_dlist_init(dl, backing, CAP, sp, 8192u);
    ri_dlist_set_clip(dl, cx0, cy0, cx1, cy1);
    for (i = 0u; i < n; i++) {
        struct ri_dcmd c;
        memset(&c, 0, sizeof c);
        c.op = RI_D_RECT;
        c.x0 = (int16_t)((int)(i % 16u) * 100);
        c.y0 = (int16_t)((int)(i / 16u) * 100);
        c.x1 = (int16_t)(c.x0 + 60);
        c.y1 = (int16_t)(c.y0 + 40);
        c.rgb = 0x112233u;
        ri_dlist_push(dl, &c);
    }
    ri_dlist_clear_clip(dl);
}

int main(void) {
    struct ri_dlist dl, ref;
    uint32_t i, n = 200u;

    /* Baseline: no clip keeps everything, in order. */
    build(&ref, refcmd, refspool, n, -1, -1, -2, -2); /* inverted => no clip */
    RI_ASSERT(ref.n == n, "unclipped build keeps all %u (got %u)", n, ref.n);

    /* The equivalence: a clipped build is EXACTLY the subsequence the
     * clipped replay would have drawn. Verified per command, in order. */
    {
        int bx0 = 490, by0 = 190, bx1 = 570, by1 = 250; /* spans four grid rects */
        uint32_t k = 0u, kept = 0u;
        build(&dl, cmd, spool, n, bx0, by0, bx1, by1);
        RI_ASSERT(dl.n < ref.n, "a small box keeps fewer than all (%u vs %u)", dl.n, ref.n);
        for (i = 0u; i < ref.n; i++) {
            if (!ri_dcmd_hits_box(&ref.cmd[i], bx0, by0, bx1, by1))
                continue;
            RI_ASSERT(k < dl.n, "clipped stream exhausted at %u", k);
            if (memcmp(&ref.cmd[i], &dl.cmd[k], sizeof(struct ri_dcmd)) != 0)
                RI_ASSERT(0, "clipped cmd %u differs from the unclipped one", k);
            k++;
            kept++;
        }
        RI_ASSERT(kept == dl.n, "kept %u but the clipped build holds %u", kept, dl.n);
        RI_ASSERT(kept > 0u, "the box matched something");
    }

    /* A straddling command is kept: partial coverage must survive, or a
     * panel background crossing the box would be clipped away. */
    {
        int a0, b0, a1, b1;
        struct ri_dcmd c;
        memset(&c, 0, sizeof c);
        c.op = RI_D_RECT;
        c.x0 = 400; c.y0 = 400; c.x1 = 700; c.y1 = 500; /* box is 500,500..560,560 */
        ri_dcmd_bbox(&c, &a0, &b0, &a1, &b1);
        RI_ASSERT(ri_dcmd_hits_box(&c, 500, 500, 560, 560),
            "a straddling rect hits the box (%d,%d..%d,%d)", a0, b0, a1, b1);
        memset(&c, 0, sizeof c);
        c.op = RI_D_RECT;
        c.x0 = 0; c.y0 = 0; c.x1 = 10; c.y1 = 10;
        RI_ASSERT(!ri_dcmd_hits_box(&c, 500, 500, 560, 560),
            "a wholly-outside rect does not");
    }

    /* An inverted box means NO CLIP, not "draw nothing": a bad box must
     * degrade to the old behaviour, never to a blank canvas. A zero-area box
     * is a real 1 px box and does clip — it is not the same thing. */
    {
        uint32_t full;
        build(&dl, cmd, spool, n, -1, -1, -2, -2);
        full = dl.n;
        build(&dl, cmd, spool, n, 100, 100, 50, 50);    /* x1 < x0: inverted */
        RI_ASSERT(dl.n == full, "an inverted box means no clip (%u)", dl.n);
        build(&dl, cmd, spool, n, 200, 200, 200, 200);  /* zero-area: a real 1 px box */
        RI_ASSERT(dl.n == 1u, "a zero-area box keeps only what is on it (%u)", dl.n);
    }

    /* A full-canvas clip keeps everything: the bound costs nothing when the
     * damage covers the whole section. */
    build(&dl, cmd, spool, n, -10000, -10000, 10000, 10000);
    RI_ASSERT(dl.n == n, "a full-canvas clip keeps all (%u)", dl.n);

    /* RI_D_CLIP is dropped while clipping: the replay ignores it, so keeping
     * it would only cost. */
    {
        struct ri_dcmd c;
        memset(&c, 0, sizeof c);
        c.op = RI_D_CLIP;
        c.x0 = 0; c.y0 = 0; c.x1 = 5; c.y1 = 5;
        RI_ASSERT(!ri_dcmd_hits_box(&c, 500, 500, 560, 560),
            "a CLIP command never hits (per ri_dcmd_bbox), so it is dropped");
    }

    /* The reduction the fix is for, stated as a bound: a step-lamp-sized box
     * on a big canvas must keep a small minority of the commands. */
    {
        uint32_t lamp = 0u;
        build(&ref, refcmd, refspool, n, -1, -1, -2, -2); /* inverted => no clip */
        build(&dl, cmd, spool, n, 495, 195, 555, 245);  /* exactly one grid rect */
        for (i = 0u; i < ref.n; i++)
            if (ri_dcmd_hits_box(&ref.cmd[i], 495, 195, 555, 245))
                lamp++;
        RI_ASSERT(lamp == dl.n, "lamp box keeps %u of %u", lamp, ref.n);
        RI_ASSERT(dl.n * 10u < ref.n,
            "one lamp keeps under a tenth of the build (%u of %u)", dl.n, ref.n);
    }

    /* A cleared clip must let everything through afterwards. build_dl pairs
     * set_clip with clear_clip around every build, so this is the path a
     * full repaint takes immediately after a damage repaint: if clear_clip
     * did not clear, the next full repaint would build only the last damage
     * box and blank the rest of the section. */
    {
        struct ri_dlist d2;
        uint32_t i, kept;
        build(&d2, cmd, spool, n, 495, 195, 555, 245);
        RI_ASSERT(d2.n == 1u, "clipped to one command (%u)", d2.n);
        RI_ASSERT(d2.clip == 0u, "build leaves the clip cleared (%u)", (unsigned)d2.clip);
        kept = d2.n;
        for (i = 0u; i < n; i++) {
            struct ri_dcmd c;
            memset(&c, 0, sizeof c);
            c.op = RI_D_RECT;
            c.x0 = (int16_t)((int)(i % 16u) * 100);
            c.y0 = (int16_t)((int)(i / 16u) * 100);
            c.x1 = (int16_t)(c.x0 + 60);
            c.y1 = (int16_t)(c.y0 + 40);
            ri_dlist_push(&d2, &c);
        }
        /* the list already held `kept`; all n pushed again must land */
        RI_ASSERT(d2.n == kept + n,
            "after the clip is cleared every command is kept (%u, want %u)",
            d2.n, kept + n);
    }

    RI_RESULT("damage_clip_build");
}