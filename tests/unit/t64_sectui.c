/* t64_sectui — section behaviour dispatch (§12.10 G4): every laid-out
 * section initialises, routes press/set/reset/value/led to its own module,
 * and sections without a layout refuse cleanly. */
#include <stdio.h>
#include "tests/helpers/ri_assert.h"
#include "gui/sectui.h"
#include "gui/ctlreg.h"
#include "gui/panelgeo.h"

int main(void) {
    struct RISectUI a, b;
    uint32_t sec;
    for (sec = 0; sec < RI_SEC_COUNT; sec++) {
        int rc = ri_sui_init(&a, (uint8_t)sec);
        int laid = ri_geo_section(sec) != 0 || sec == RI_SEC_SYNTH2; /* Synth 2 reuses the Synth 1 layout */
        RI_ASSERT((rc == 0) == laid, "%s: init %d vs layout %d", ri_ctlreg_section_name(sec), rc, laid);
    }
    ri_sui_init(&a, RI_SEC_SYNTH1);
    RI_ASSERT(ri_sui_press(&a, RI_S303_STEP) == 1 && ri_sui_display(&a, RI_S303_DISPLAY) == 2, "303 route");
    RI_ASSERT(ri_sui_set(&a, 2, 10) == 1 && ri_sui_value(&a, 2) == 10, "303 set/value");
    RI_ASSERT(ri_sui_reset(&a, 2) == 1 && ri_sui_value(&a, 2) == 96, "303 reset");
    ri_sui_init(&b, RI_SEC_808);
    RI_ASSERT(ri_sui_press(&b, RI_S808_STEP0) == 1 && ri_sui_led(&b, RI_S808_STEP0, 0) == 1, "808 route");
    RI_ASSERT(ri_sui_display(&b, RI_S808_STEP0) == 0, "808 has no numeric display");
    ri_sui_init(&b, RI_SEC_909);
    RI_ASSERT(ri_sui_press(&b, RI_S909_STEP0) == 1 && ri_sui_led(&b, RI_S909_STEP0, 0) == RI_HIT_LOW, "909 route");
    RI_ASSERT(ri_sui_init(&b, RI_SEC_MASTER) == 2 && ri_sui_press(&b, 0) == 0, "master not laid out yet");
    RI_RESULT("sectui");
}
