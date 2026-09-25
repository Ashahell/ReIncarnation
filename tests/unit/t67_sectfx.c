/* t67_sectfx — PCF / Delay / Dist / Comp behaviour (§12.10 G3/G4),
 * ReBirth manual p. 18, 159-164. */
#include <stdio.h>
#include "tests/helpers/ri_assert.h"
#include "gui/sectfx.h"
#include "gui/ctlreg.h"

int main(void) {
    struct RISectFx s;
    uint32_t sec;
    int i;
    RI_ASSERT(ri_sfx_init(&s, RI_SEC_909) == 2 && ri_sfx_init(&s, RI_SEC_MASTER) == 2, "only FX sections");
    RI_ASSERT(ri_sfx_init(0, RI_SEC_PCF) == 2, "null");
    for (sec = RI_SEC_PCF; sec <= RI_SEC_COMP; sec++) {
        RI_ASSERT(ri_sfx_init(&s, (uint8_t)sec) == 0, "init %u", sec);
        RI_ASSERT(ri_sfx_led(&s, RI_SFX_ONOFF) == 0, "unit %u starts bypassed", sec);
        RI_ASSERT(ri_sfx_press(&s, RI_SFX_ONOFF) == 1 && ri_sfx_led(&s, RI_SFX_ONOFF) == 1, "on/off %u", sec);
        RI_ASSERT(ri_sfx_press(&s, RI_SFX_METER) == 0 && ri_sfx_set_value(&s, RI_SFX_METER, 9) == 0, "meter read-only");
        ri_sfx_meter_set(&s, RI_SFX_METER, 200);
        RI_ASSERT(s.val[RI_SFX_METER] == 127, "meter feed clamps");
    }
    /* PCF (p. 159-160) */
    ri_sfx_init(&s, RI_SEC_PCF);
    RI_ASSERT(s.val[RI_SFX_PCF_PATTERN] == 0 && ri_sfx_step(&s, RI_SFX_PCF_PATTERN, -1) == 0, "pattern stops at 0");
    for (i = 0; i < 60; i++)
        ri_sfx_step(&s, RI_SFX_PCF_PATTERN, 1);
    RI_ASSERT(s.val[RI_SFX_PCF_PATTERN] == 53, "pattern stops at 53");
    RI_ASSERT(ri_sfx_step(&s, RI_SFX_PCF_PATTERN, -1) == 1 && s.val[RI_SFX_PCF_PATTERN] == 52, "arrow down");
    RI_ASSERT(ri_sfx_step(&s, 4, 1) == 0, "sliders have no arrows");
    RI_ASSERT(ri_sfx_press(&s, RI_SFX_PCF_MODE) == 1 && s.val[RI_SFX_PCF_MODE] == 1, "mode LP -> BP");
    RI_ASSERT(ri_sfx_set_value(&s, 4, 300) == 1 && s.val[4] == 127, "Freq slider clamps");
    RI_ASSERT(ri_sfx_reset(&s, 6) == 0 && s.val[6] == 0, "Amt defaults to 0 (no pattern effect)");
    RI_ASSERT(ri_ctlreg_find((uint16_t)((RI_SEC_PCF << 8) | 4))->kind == RI_CK_FADER, "PCF Freq is a slider (p. 159)");
    /* Delay (p. 161-162) */
    ri_sfx_init(&s, RI_SEC_DELAY);
    RI_ASSERT(s.val[RI_SFX_DLY_STEPS] == 3 && s.val[RI_SFX_DLY_TRIPLET] == 0, "delay defaults");
    RI_ASSERT(ri_sfx_step(&s, RI_SFX_DLY_STEPS, 1) == 1 && s.val[RI_SFX_DLY_STEPS] == 4, "steps up");
    ri_sfx_set_value(&s, RI_SFX_DLY_STEPS, 1);
    RI_ASSERT(ri_sfx_step(&s, RI_SFX_DLY_STEPS, -1) == 0 && s.val[RI_SFX_DLY_STEPS] == 1, "steps stop at 1");
    RI_ASSERT(ri_sfx_press(&s, RI_SFX_DLY_TRIPLET) == 1 && ri_sfx_led(&s, RI_SFX_DLY_TRIPLET), "triplet switch");
    RI_ASSERT(ri_sfx_set_value(&s, 5, 127) == 1 && s.val[5] == 127, "F.Back full");
    RI_ASSERT(ri_sfx_press(&s, 4) == 0, "knobs are not buttons");
    /* Comp (p. 164) */
    ri_sfx_init(&s, RI_SEC_COMP);
    ri_sfx_meter_set(&s, RI_SFX_COMP_GR, 60);
    RI_ASSERT(s.val[RI_SFX_COMP_GR] == 60, "level reduction meter");
    ri_sfx_meter_set(&s, 2, 99);
    RI_ASSERT(s.val[2] == 64, "meter feed ignores knobs");
    RI_RESULT("sectfx");
}
