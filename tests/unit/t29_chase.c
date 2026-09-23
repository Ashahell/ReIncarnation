/* t29_chase — Module 2.9 partial, TC-2.9.3 (step chase composition):
 *
 *   - Full 16-step bar at 174 BPM: beat positions 0..15 map to steps
 *     0..15, position 16 wraps to 0 (no step skipped, none repeated).
 *   - Step duration 60000/(174*4) = 86.2 ms exceeds the 33 ms LED
 *     frame, so a per-step LED update is feasible every step.
 *   - Toggle is an involution on {0,1} (click-click restores).
 *   - LED lag bound composition: lag 0 and lag 33 ms pass, anything
 *     above fails (mirrors t1_knob §5 edges at the composition level).
 *
 * Green pin on frozen code (property pin over landed Task 12
 * behaviour); the RED driver for the slice is t29_paneldefault
 * (ri_panel_default_ctl does not exist yet).
 */
#include <stdio.h>
#include <math.h>
#include "gui/knob_logic.h"

static int fails = 0;
#define CHECK(cond, ...) do { \
    if (!(cond)) { printf("FAIL "); printf(__VA_ARGS__); printf("\n"); fails++; } \
} while (0)

int main(void) {
    int k;
    double s16 = ri_step16_ms(174.0);

    /* --- full bar: positions 0..15 -> steps 0..15, 16 wraps to 0 --- */
    for (k = 0; k < 16; k++)
        CHECK(ri_chase_step((double)k, 16) == k, "bar step %d", k);
    CHECK(ri_chase_step(16.0, 16) == 0, "bar wrap");

    /* --- 174 BPM step exceeds the LED frame (per-step update feasible) --- */
    CHECK(fabs(s16 - 15000.0 / 174.0) < 1e-9, "174 step %f", s16);
    CHECK(s16 > RI_LED_FRAME_MS, "step %.4g ms inside frame", s16);

    /* --- toggle involution on {0,1} --- */
    CHECK(ri_step_toggle(ri_step_toggle(0)) == 0, "toggle 0 round-trip");
    CHECK(ri_step_toggle(ri_step_toggle(1)) == 1, "toggle 1 round-trip");

    /* --- lag bound composition --- */
    CHECK(ri_led_lag_ok(0.0) == 1, "lag 0");
    CHECK(ri_led_lag_ok(33.0) == 1, "lag edge");
    CHECK(ri_led_lag_ok(33.1) == 0, "lag over");

    if (fails)
        printf("FAIL %d\n", fails);
    else
        printf("PASS t29_chase\n");
    return fails != 0;
}
