/* t65_sect909 — 909 section behaviour (§12.10 G3/G4), ReBirth manual p. 29-31, 151-152. */
#include <stdio.h>
#include <string.h>
#include "tests/helpers/ri_assert.h"
#include "gui/sect909.h"
#include "gui/ctlreg.h"

int main(void) {
    struct RISect909 s;
    RI_ASSERT(ri_s909_init(&s) == 0, "init");
    RI_ASSERT(ri_s909_lane(&s) == RI_L909_BD, "BD selected by default");
    /* off -> low -> high -> off (p. 30) */
    ri_s909_press(&s, RI_S909_STEP0);
    RI_ASSERT(ri_s909_led(&s, RI_S909_STEP0) == RI_HIT_LOW, "first click: low");
    ri_s909_press(&s, RI_S909_STEP0);
    RI_ASSERT(ri_s909_led(&s, RI_S909_STEP0) == RI_HIT_HIGH, "second click: high");
    ri_s909_press(&s, RI_S909_STEP0);
    RI_ASSERT(ri_s909_led(&s, RI_S909_STEP0) == RI_HIT_OFF, "third click: off");
    /* Flam button: click toggles off <-> flam (p. 30) */
    RI_ASSERT(ri_s909_press(&s, RI_S909_FLAMBTN) == 1 && ri_s909_led(&s, RI_S909_FLAMBTN), "flam mode on");
    ri_s909_press(&s, RI_S909_STEP0 + 3);
    RI_ASSERT(ri_s909_led(&s, RI_S909_STEP0 + 3) == RI_HIT_FLAM, "flam hit");
    ri_s909_press(&s, RI_S909_STEP0 + 3);
    RI_ASSERT(ri_s909_led(&s, RI_S909_STEP0 + 3) == RI_HIT_OFF, "flam click again: off");
    ri_s909_press(&s, RI_S909_FLAMBTN);
    /* legend select: CH (lane 7), edit goes there only */
    RI_ASSERT(ri_s909_set_value(&s, RI_S909_SELECT, 8) == 1 && ri_s909_lane(&s) == RI_L909_CH, "select CH");
    ri_s909_press(&s, RI_S909_STEP0 + 2);
    RI_ASSERT(ri_pdrum_get(&s.pat, 2, RI_L909_CH) == RI_HIT_LOW && ri_pdrum_get(&s.pat, 2, RI_L909_BD) == RI_HIT_OFF,
        "edit goes to CH");
    /* AC row */
    ri_s909_set_value(&s, RI_S909_SELECT, 0);
    ri_s909_press(&s, RI_S909_STEP0 + 4);
    RI_ASSERT((s.pat.row.drum[4].flags & RI_DRUM_AC) && ri_s909_led(&s, RI_S909_STEP0 + 4) == 1, "AC row");
    RI_ASSERT(ri_pattern_valid(&s.pat) == 0, "valid pattern");
    /* knobs */
    RI_ASSERT(s.val[RI_S909_FLAM] == 64 && ri_s909_set_value(&s, RI_S909_FLAM, 200) == 1 && s.val[RI_S909_FLAM] == 127,
        "flam knob clamps");
    RI_ASSERT(ri_s909_reset(&s, RI_S909_FLAM) == 1 && s.val[RI_S909_FLAM] == 64, "right-click default");
    RI_ASSERT(ri_s909_press(&s, 1) == 0, "knobs are not buttons");
    RI_RESULT("sect909");
}
