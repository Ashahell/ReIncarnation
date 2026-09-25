/* t68_sectpat — Pattern section behaviour (§12.10 G3/G4), ReBirth manual p. 147. */
#include <stdio.h>
#include "tests/helpers/ri_assert.h"
#include "gui/sectpat.h"
#include "gui/ctlreg.h"

int main(void) {
    struct RISectPat s;
    uint32_t sec;
    int i;
    RI_ASSERT(ri_spat_init(&s, RI_SEC_909) == 2 && ri_spat_init(0, RI_SEC_PAT_808) == 2, "only pattern sections");
    for (sec = RI_SEC_PAT_SYNTH1; sec <= RI_SEC_PAT_909; sec++)
        RI_ASSERT(ri_spat_init(&s, (uint8_t)sec) == 0 && ri_spat_selected(&s) == 0 && ri_spat_led(&s, RI_SPAT_OFF),
            "init %u: A1, section on", sec);
    /* same bank: one click selects */
    RI_ASSERT(ri_spat_set_value(&s, RI_SPAT_PATTERN, 5) == 1 && ri_spat_selected(&s) == 5, "A6");
    /* bank click only arms: "No Pattern gets selected until you click one of the Pattern buttons" */
    RI_ASSERT(ri_spat_set_value(&s, RI_SPAT_BANK, 2) == 1 && ri_spat_selected(&s) == 5, "bank C armed, still A6");
    RI_ASSERT(ri_spat_value(&s, RI_SPAT_BANK) == 2 && ri_spat_value(&s, RI_SPAT_PATTERN) == -1,
        "C lit, no pattern lit (A6 is not in bank C)");
    RI_ASSERT(ri_spat_set_value(&s, RI_SPAT_PATTERN, 0) == 1 && ri_spat_selected(&s) == 16 &&
        ri_spat_value(&s, RI_SPAT_PATTERN) == 0, "C1 selected");
    RI_ASSERT(ri_spat_set_value(&s, RI_SPAT_PATTERN, 0) == 0, "reselect is no change");
    RI_ASSERT(ri_spat_set_value(&s, RI_SPAT_BANK, 4) == 0 && ri_spat_set_value(&s, RI_SPAT_PATTERN, 8) == 0,
        "out of range refused");
    /* per-pattern length, 1..16 in 16ths */
    RI_ASSERT(ri_spat_value(&s, RI_SPAT_LENGTH) == 16 && ri_spat_step(&s, RI_SPAT_LENGTH, 1) == 0, "16 max");
    for (i = 0; i < 20; i++)
        ri_spat_step(&s, RI_SPAT_LENGTH, -1);
    RI_ASSERT(ri_spat_value(&s, RI_SPAT_LENGTH) == 1, "stops at 1");
    ri_spat_set_value(&s, RI_SPAT_LENGTH, 12);
    ri_spat_set_value(&s, RI_SPAT_BANK, 0);
    ri_spat_set_value(&s, RI_SPAT_PATTERN, 5);
    RI_ASSERT(ri_spat_value(&s, RI_SPAT_LENGTH) == 16, "A6 keeps its own length");
    ri_spat_set_value(&s, RI_SPAT_BANK, 2);
    ri_spat_set_value(&s, RI_SPAT_PATTERN, 0);
    RI_ASSERT(ri_spat_value(&s, RI_SPAT_LENGTH) == 12 && ri_spat_reset(&s, RI_SPAT_LENGTH) == 1 &&
        ri_spat_value(&s, RI_SPAT_LENGTH) == 16, "C1 length 12, default 16");
    /* switches */
    RI_ASSERT(ri_spat_press(&s, RI_SPAT_OFF) == 1 && !ri_spat_led(&s, RI_SPAT_OFF), "section off: lamp out");
    RI_ASSERT(ri_spat_press(&s, RI_SPAT_SHUFFLE) == 1 && ri_spat_led(&s, RI_SPAT_SHUFFLE), "shuffle on");
    RI_ASSERT(ri_spat_press(&s, RI_SPAT_BANK) == 0 && ri_spat_step(&s, RI_SPAT_BANK, 1) == 0, "no arrows on banks");
    RI_RESULT("sectpat");
}
