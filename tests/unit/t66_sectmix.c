/* t66_sectmix — mixers + master behaviour (§12.10 G3/G4), ReBirth manual
 * p. 23-24, 56-57, 59-70, 157-158. */
#include <stdio.h>
#include "tests/helpers/ri_assert.h"
#include "gui/sectmix.h"
#include "gui/ctlreg.h"

int main(void) {
    static struct RIMixBoard b;
    uint32_t sec;
    ri_smix_init(&b);
    RI_ASSERT(ri_smix_strip(RI_SEC_MIX_SYNTH1) == 0 && ri_smix_strip(RI_SEC_MIX_909) == 3 &&
        ri_smix_strip(RI_SEC_MASTER) == 4 && ri_smix_strip(RI_SEC_909) == -1, "strips");
    for (sec = RI_SEC_MIX_SYNTH1; sec <= RI_SEC_MIX_909; sec++) {
        RI_ASSERT(ri_smix_led(&b, sec, RI_SMIX_ONOFF) == 1, "mixer %u starts on (sounding)", sec);
        RI_ASSERT(ri_smix_value(&b, sec, RI_SMIX_LEVEL) == 100 && ri_smix_value(&b, sec, RI_SMIX_PAN) == 64 &&
            ri_smix_value(&b, sec, RI_SMIX_DELAY) == 0, "mixer %u defaults", sec);
        RI_ASSERT(!ri_smix_led(&b, sec, RI_SMIX_DIST) && !ri_smix_led(&b, sec, RI_SMIX_PCF) &&
            !ri_smix_led(&b, sec, RI_SMIX_COMP), "inserts start off");
    }
    RI_ASSERT(ri_smix_value(&b, RI_SEC_MASTER, RI_SMST_LEVEL) == 100, "master level default");
    /* mute (p. 56) */
    RI_ASSERT(ri_smix_press(&b, RI_SEC_MIX_808, RI_SMIX_ONOFF) == 1 && !ri_smix_led(&b, RI_SEC_MIX_808, RI_SMIX_ONOFF),
        "mute");
    RI_ASSERT(ri_smix_press(&b, RI_SEC_MIX_808, RI_SMIX_ONOFF) == 1 && ri_smix_led(&b, RI_SEC_MIX_808, RI_SMIX_ONOFF),
        "unmute");
    /* PCF: one section at a time; switching on elsewhere steals it (p. 62 step 16) */
    ri_smix_press(&b, RI_SEC_MIX_SYNTH1, RI_SMIX_PCF);
    RI_ASSERT(ri_smix_led(&b, RI_SEC_MIX_SYNTH1, RI_SMIX_PCF), "PCF on synth 1");
    ri_smix_press(&b, RI_SEC_MIX_808, RI_SMIX_PCF);
    RI_ASSERT(ri_smix_led(&b, RI_SEC_MIX_808, RI_SMIX_PCF) && !ri_smix_led(&b, RI_SEC_MIX_SYNTH1, RI_SMIX_PCF),
        "PCF stolen: synth LED goes out");
    RI_ASSERT(ri_route_owner(&b.route, RI_ROUTE_PCF) == 2, "route owner = 808 strip");
    ri_smix_press(&b, RI_SEC_MIX_808, RI_SMIX_PCF);
    RI_ASSERT(ri_route_owner(&b.route, RI_ROUTE_PCF) == RI_ROUTE_NONE, "PCF released");
    /* Comp: one section or the master (p. 67) */
    ri_smix_press(&b, RI_SEC_MIX_909, RI_SMIX_COMP);
    ri_smix_press(&b, RI_SEC_MASTER, RI_SMST_COMP);
    RI_ASSERT(ri_smix_led(&b, RI_SEC_MASTER, RI_SMST_COMP) && !ri_smix_led(&b, RI_SEC_MIX_909, RI_SMIX_COMP) &&
        ri_route_owner(&b.route, RI_ROUTE_COMP) == RI_ROUTE_MASTER, "master takes the comp");
    ri_smix_press(&b, RI_SEC_MIX_SYNTH2, RI_SMIX_COMP);
    RI_ASSERT(!ri_smix_led(&b, RI_SEC_MASTER, RI_SMST_COMP) && ri_smix_value(&b, RI_SEC_MIX_SYNTH2, RI_SMIX_COMP) == 1,
        "section takes it back");
    /* independent units */
    ri_smix_press(&b, RI_SEC_MIX_SYNTH2, RI_SMIX_DIST);
    RI_ASSERT(ri_smix_led(&b, RI_SEC_MIX_SYNTH2, RI_SMIX_DIST) && ri_smix_led(&b, RI_SEC_MIX_SYNTH2, RI_SMIX_COMP),
        "dist and comp on one strip");
    /* fader / knobs: clamp, default, per strip */
    RI_ASSERT(ri_smix_set_value(&b, RI_SEC_MIX_SYNTH1, RI_SMIX_LEVEL, 300) == 1 &&
        ri_smix_value(&b, RI_SEC_MIX_SYNTH1, RI_SMIX_LEVEL) == 127, "fader clamps");
    RI_ASSERT(ri_smix_value(&b, RI_SEC_MIX_SYNTH2, RI_SMIX_LEVEL) == 100, "other strip untouched");
    RI_ASSERT(ri_smix_reset(&b, RI_SEC_MIX_SYNTH1, RI_SMIX_LEVEL) == 1 &&
        ri_smix_value(&b, RI_SEC_MIX_SYNTH1, RI_SMIX_LEVEL) == 100, "right-click default");
    RI_ASSERT(ri_smix_set_value(&b, RI_SEC_MIX_909, RI_SMIX_PAN, -5) == 1 &&
        ri_smix_value(&b, RI_SEC_MIX_909, RI_SMIX_PAN) == 0, "pan hard left");
    RI_ASSERT(ri_smix_set_value(&b, RI_SEC_MASTER, RI_SMST_LEVEL, 80) == 1 &&
        ri_smix_value(&b, RI_SEC_MASTER, RI_SMST_LEVEL) == 80, "master fader");
    RI_ASSERT(ri_smix_set_value(&b, RI_SEC_MIX_909, RI_SMIX_DIST, 1) == 0, "switches are not set by value");
    RI_ASSERT(ri_smix_press(&b, RI_SEC_MIX_909, RI_SMIX_PAN) == 0, "knobs are not buttons");
    /* meters: display only, fed by the engine */
    ri_smix_meter_set(&b, RI_SEC_MIX_808, 0, 90);
    ri_smix_meter_set(&b, RI_SEC_MASTER, 1, 300);
    RI_ASSERT(ri_smix_value(&b, RI_SEC_MIX_808, RI_SMIX_METER) == 90 &&
        ri_smix_value(&b, RI_SEC_MASTER, RI_SMST_METER_R) == 127 && ri_smix_value(&b, RI_SEC_MASTER, RI_SMST_METER_L) == 0,
        "meters");
    RI_ASSERT(ri_smix_press(&b, RI_SEC_MIX_808, RI_SMIX_METER) == 0 &&
        ri_smix_set_value(&b, RI_SEC_MIX_808, RI_SMIX_METER, 5) == 0, "meters are read-only");
    /* non-mixer sections refused */
    RI_ASSERT(ri_smix_press(&b, RI_SEC_909, 0) == 0 && ri_smix_value(&b, RI_SEC_PCF, 0) == 0, "foreign section");
    RI_ASSERT(ri_smix_press(0, RI_SEC_MIX_808, RI_SMIX_ONOFF) == 0, "null board");
    RI_RESULT("sectmix");
}
