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

    /* --- frame renderer (64x64 RGBA, 2x of locked 32px) --- */
    {
        static unsigned char F1[64 * 64 * 4], F2[64 * 64 * 4];
        uint32_t i;
        /* table integrity: cardinals + norm (catches a pasting slip) */
        CHECK(RI_SIN_Q15[0] == 0 && RI_SIN_Q15[90] == 32767 &&
            RI_SIN_Q15[180] == 0 && RI_SIN_Q15[270] == -32767,
            "sin cardinals");
        for (i = 0; i < 360; i += 37) {
            int32_t s = RI_SIN_Q15[i], c = RI_SIN_Q15[(i + 90) % 360];
            int64_t n = (int64_t)s * s + (int64_t)c * c;
            int64_t want = (int64_t)32767 * 32767;
            int64_t err = n > want ? n - want : want - n;
            CHECK(err < 100000, "sin norm %u", i);
        }
        ri_knob_render_frame(F1, 64);
        /* corners transparent (outside disc) */
        CHECK(F1[3] == 0 && F1[(63 * 64 + 63) * 4 + 3] == 0,
            "corners opaque");
        /* hub exact at center */
        CHECK(F1[(32 * 64 + 32) * 4 + 0] == 16 &&
            F1[(32 * 64 + 32) * 4 + 3] == 255, "hub");
        /* rim ring exact */
        CHECK(F1[(56 * 64 + 32) * 4 + 0] == 13 &&
            F1[(56 * 64 + 32) * 4 + 3] == 255, "rim");
        /* pointer orange up at value 64 (mid) */
        CHECK(F1[(10 * 64 + 32) * 4 + 0] == 224 &&
            F1[(10 * 64 + 32) * 4 + 1] == 123 &&
            F1[(10 * 64 + 32) * 4 + 2] == 46, "pointer top");
        CHECK(F1[(19 * 64 + 32) * 4 + 0] == 224, "pointer mid");
        /* low pin at r=7 (r=6 sits inside the hub-ring zone and is
         * correctly ring-colored — pointer paints first, hub last) */
        CHECK(F1[(25 * 64 + 32) * 4 + 0] == 224, "pointer low");
        /* off-pointer body pixel: gradient range, opaque, not orange */
        CHECK(F1[(19 * 64 + 40) * 4 + 3] == 255, "body alpha");
        CHECK(F1[(19 * 64 + 40) * 4 + 0] != 224 ||
            F1[(19 * 64 + 40) * 4 + 1] != 123, "body is pointer?");
        /* determinism + value sensitivity */
        ri_knob_render_frame(F2, 64);
        CHECK(memcmp(F1, F2, sizeof F1) == 0, "remix nondet");
        ri_knob_render_frame(F2, 0);
        {
            uint32_t ndiff = 0;
            for (i = 0; i < 64u * 64u; i++) {
                if (F1[i * 4] != F2[i * 4] || F1[i * 4 + 1] != F2[i * 4 + 1] ||
                    F1[i * 4 + 2] != F2[i * 4 + 2])
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
