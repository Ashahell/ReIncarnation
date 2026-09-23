/* t29_knobart — Module 2.9 knob artwork geometry (TC-2.9.2 dial):
 *
 * Pointer angle contract for the custom knob renderer (replaces
 * MUIC_Knob stock visuals): value 0..127 maps to -135000..+135000
 * millidegrees (270-degree sweep, 0 = straight up), clamped;
 * linearity step 2125/2126 mdeg per unit. Exact integer contract
 * (no fp, no trig in the engine — endpoint rendering is the AROS
 * draw routine's job, verified visually on device).
 *
 * RED status: ri_knob_pointer_mdeg does not exist yet, so this
 * file fails to BUILD on the current tree (feature-absent
 * manifest, same shape as every t2x slice).
 */
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "gui/knob_logic.h"
#include "gui/knob_art.h"

static int fails = 0;
#define CHECK(cond, ...) do { \
    if (!(cond)) { printf("FAIL "); printf(__VA_ARGS__); printf("\n"); fails++; } \
} while (0)

int main(void) {
    int v, prev;

    /* --- sweep ends exact --- */
    CHECK(ri_knob_pointer_mdeg(0) == -135000, "min %d",
        ri_knob_pointer_mdeg(0));
    CHECK(ri_knob_pointer_mdeg(127) == 135000, "max %d",
        ri_knob_pointer_mdeg(127));
    CHECK(ri_knob_pointer_mdeg(64) == 1063, "mid %d",
        ri_knob_pointer_mdeg(64));

    /* --- linearity: unit steps of 2125/2126 mdeg, monotone --- */
    prev = ri_knob_pointer_mdeg(0);
    for (v = 1; v <= 127; v++) {
        int cur = ri_knob_pointer_mdeg(v);
        int step = cur - prev;
        CHECK(step == 2125 || step == 2126, "v=%d step %d", v, step);
        CHECK(cur >= prev, "v=%d not monotone", v);
        prev = cur;
    }

    /* --- fail-closed clamps --- */
    CHECK(ri_knob_pointer_mdeg(-5) == -135000, "neg clamp %d",
        ri_knob_pointer_mdeg(-5));
    CHECK(ri_knob_pointer_mdeg(200) == 135000, "over clamp %d",
        ri_knob_pointer_mdeg(200));

    /* --- v2 art (measured TR-09): 80px frames, tick ring,
     * shadow, short thick pointer, no hub --- */
    {
        static unsigned char G1[80 * 80 * 4], G2[80 * 80 * 4];
        uint32_t i;
        CHECK(RI_KNOB_PX == 80u, "frame size %u", RI_KNOB_PX);
        ri_knob_render_frame(G1, 64);
        /* corners transparent (outside disc + shadow reach) */
        CHECK(G1[3] == 0 && G1[(79 * 80 + 79) * 4 + 3] == 0,
            "corners opaque");
        CHECK(G1[(0 * 80 + 79) * 4 + 3] == 0, "corner30 opaque");
        /* face center exact (warm gradient, no hub) */
        CHECK(G1[(40 * 80 + 40) * 4 + 0] == 63 &&
            G1[(40 * 80 + 40) * 4 + 1] == 55 &&
            G1[(40 * 80 + 40) * 4 + 2] == 40 &&
            G1[(40 * 80 + 40) * 4 + 3] == 255, "face center");
        /* pointer orange up at value 64 (r19 -> y=21) */
        CHECK(G1[(21 * 80 + 40) * 4 + 0] == 227 &&
            G1[(21 * 80 + 40) * 4 + 1] == 124 &&
            G1[(21 * 80 + 40) * 4 + 2] == 59, "pointer");
        /* tick at top (r29-35 -> y 5..11): dark tick color */
        CHECK(G1[(8 * 80 + 40) * 4 + 3] == 255, "tick alpha");
        CHECK(G1[(8 * 80 + 40) * 4 + 0] < 80, "tick bright %u",
            G1[(8 * 80 + 40) * 4 + 0]);
        /* bottom gap: no tick below center (y 72 -> transparent) */
        CHECK(G1[(72 * 80 + 40) * 4 + 3] == 0, "gap filled?");
        /* shadow SE of disc (offset silhouette, partial alpha):
         * (55,62) is outside the body but inside the +2/+3
         * shifted silhouette */
        CHECK(G1[(62 * 80 + 55) * 4 + 3] == 64, "shadow alpha %u",
            G1[(62 * 80 + 55) * 4 + 3]);
        /* determinism + value sensitivity */
        ri_knob_render_frame(G2, 64);
        CHECK(memcmp(G1, G2, sizeof G1) == 0, "remix nondet");
        ri_knob_render_frame(G2, 0);
        {
            uint32_t ndiff = 0;
            for (i = 0; i < 80u * 80u; i++) {
                if (G1[i * 4] != G2[i * 4] || G1[i * 4 + 1] != G2[i * 4 + 1] ||
                    G1[i * 4 + 2] != G2[i * 4 + 2])
                    ndiff++;
            }
            CHECK(ndiff > 50u, "frames too close (%u)", ndiff);
        }
    }

    if (fails)
        printf("FAIL %d\n", fails);
    else
        printf("PASS t29_knobart\n");
    return fails != 0;
}
