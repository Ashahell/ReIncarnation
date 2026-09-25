/* t63_sect808 — 808 section behaviour (§12.10 G3/G4), ReBirth manual p. 28-35, 148-150. */
#include <stdio.h>
#include <string.h>
#include "tests/helpers/ri_assert.h"
#include "gui/sect808.h"
#include "gui/ctlreg.h"

int main(void) {
    struct RISect808 s;
    uint32_t i;
    RI_ASSERT(ri_s808_init(&s) == 0, "init");
    RI_ASSERT(s.val[RI_S808_SELECT] == 1 && ri_s808_lane(&s) == RI_L808_BD, "BD selected by default");
    RI_ASSERT(s.val[1] == 100 && s.val[0] == 64, "registry defaults");

    /* four-on-the-floor on BD: steps 1,5,9,13 */
    for (i = 0; i < 16; i += 4)
        RI_ASSERT(ri_s808_press(&s, RI_S808_STEP0 + i) == 1, "BD step %u", i + 1);
    RI_ASSERT(ri_pdrum_get(&s.pat, 4, RI_L808_BD) == RI_HIT_LOW && ri_pdrum_get(&s.pat, 5, RI_L808_BD) == RI_HIT_OFF,
        "808 hit = on (no levels on the 808)");
    RI_ASSERT(ri_s808_led(&s, RI_S808_STEP0 + 8) && !ri_s808_led(&s, RI_S808_STEP0 + 9), "step LEDs show BD row");

    /* legend click / big knob -> CH; step buttons now show and edit CH only (p. 28) */
    RI_ASSERT(ri_s808_set_value(&s, RI_S808_SELECT, 11) == 1 && ri_s808_lane(&s) == RI_L808_CH, "select CH");
    RI_ASSERT(!ri_s808_led(&s, RI_S808_STEP0), "CH row empty");
    ri_s808_press(&s, RI_S808_STEP0 + 2);
    RI_ASSERT(ri_pdrum_get(&s.pat, 2, RI_L808_CH) == RI_HIT_LOW && ri_pdrum_get(&s.pat, 2, RI_L808_BD) == RI_HIT_OFF,
        "edit goes to CH only");
    ri_s808_press(&s, RI_S808_STEP0 + 2);
    RI_ASSERT(ri_pdrum_get(&s.pat, 2, RI_L808_CH) == RI_HIT_OFF, "second click turns it off");

    /* AC row (p. 35): selector position 0, flags not lanes */
    ri_s808_set_value(&s, RI_S808_SELECT, 0);
    RI_ASSERT(ri_s808_lane(&s) == -1, "AC selected");
    ri_s808_press(&s, RI_S808_STEP0 + 4);
    RI_ASSERT((s.pat.row.drum[4].flags & RI_DRUM_AC) && ri_s808_led(&s, RI_S808_STEP0 + 4), "AC on step 5");
    RI_ASSERT(ri_pdrum_get(&s.pat, 4, RI_L808_BD) == RI_HIT_LOW, "AC leaves the BD hit alone");
    RI_ASSERT(ri_pattern_valid(&s.pat) == 0, "pattern stays valid");

    /* sound switches are panel state (p. 149): pattern unchanged */
    {
        struct RIPattern before = s.pat;
        RI_ASSERT(ri_s808_press(&s, 9) == 1 && ri_s808_led(&s, 9), "LT/LC switch -> LC");
        RI_ASSERT(!memcmp(&before, &s.pat, sizeof before), "switch does not touch the pattern");
    }
    /* selector clamps to 12 positions; knobs reset to defaults */
    RI_ASSERT(ri_s808_set_value(&s, RI_S808_SELECT, 40) == 1 && s.val[RI_S808_SELECT] == 11, "selector clamps");
    ri_s808_set_value(&s, 1, 3);
    RI_ASSERT(ri_s808_reset(&s, 1) == 1 && s.val[1] == 100, "right-click default");
    RI_ASSERT(ri_s808_press(&s, 1) == 0, "knobs are not buttons");
    RI_RESULT("sect808");
}
