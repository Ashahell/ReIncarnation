/* t62_sect303 — 303 section step-entry workflow (§12.10 G3/G4).
 * Every assertion cites the ReBirth 2.0.1 Owner's Manual rule it pins.
 */
#include <stdio.h>
#include "tests/helpers/ri_assert.h"
#include "gui/sect303.h"
#include "gui/ctlreg.h"
#include "engine/seq/sched.h"

int main(void) {
    struct RISect303 s;
    int i;
    RI_ASSERT(ri_s303_init(&s, RI_SEC_808) == 2, "only synth sections");
    RI_ASSERT(ri_s303_init(&s, RI_SEC_SYNTH1) == 0, "init");
    RI_ASSERT(ri_s303_display(&s) == 1, "starts at step 1");
    RI_ASSERT(s.val[RI_S303_TUNE] == 64 && s.val[2] == 96, "registry defaults applied");
    RI_ASSERT(ri_s303_led(&s, RI_S303_NOTEPAUSE, 1) == 1 && ri_s303_led(&s, RI_S303_NOTEPAUSE, 0) == 0,
        "empty pattern shows Pause (Clear state, p. 52)");
    RI_ASSERT(ri_s303_led(&s, RI_S303_KEY0, 0) == 1, "low C lit on an empty step");

    /* Step wraps last -> first, Back wraps first -> last (p. 41, 154) */
    RI_ASSERT(ri_s303_press(&s, RI_S303_BACK) == 1 && ri_s303_display(&s) == 16, "Back at 1 -> 16");
    RI_ASSERT(ri_s303_press(&s, RI_S303_STEP) == 1 && ri_s303_display(&s) == 1, "Step at 16 -> 1");
    ri_pattern_set_length(&s.pat, 5);
    for (i = 0; i < 5; i++)
        ri_s303_press(&s, RI_S303_STEP);
    RI_ASSERT(ri_s303_display(&s) == 1, "wrap honours pattern length 5");
    ri_s303_press(&s, RI_S303_BACK);
    RI_ASSERT(ri_s303_display(&s) == 5, "Back wraps to the last step of the pattern");
    ri_pattern_set_length(&s.pat, 16);

    /* pitch key: key only, Note/Pause untouched, no advance outside Pitch Mode */
    ri_s303_init(&s, RI_SEC_SYNTH1);
    ri_s303_press(&s, RI_S303_KEY0 + 4); /* E */
    RI_ASSERT(s.pat.row.r303[0].key == 4, "key set");
    RI_ASSERT((s.pat.row.r303[0].flags & RI_STEP_REST) != 0, "Note/Pause untouched by a pitch key (p. 40-41)");
    RI_ASSERT(ri_s303_display(&s) == 1, "no advance without Pitch Mode");
    RI_ASSERT(ri_s303_led(&s, RI_S303_KEY0 + 4, 0) && !ri_s303_led(&s, RI_S303_KEY0, 0), "key LED follows");

    /* Pitch Mode auto-advance (p. 42) */
    ri_s303_press(&s, RI_S303_PITCHMODE);
    RI_ASSERT(ri_s303_led(&s, RI_S303_PITCHMODE, 0), "Pitch Mode LED");
    ri_s303_press(&s, RI_S303_KEY0 + 7); /* G on step 1 */
    RI_ASSERT(ri_s303_display(&s) == 2 && s.pat.row.r303[0].key == 7, "pitch then advance");
    ri_s303_press(&s, RI_S303_KEY0 + 12); /* high C on step 2 */
    RI_ASSERT(ri_s303_display(&s) == 3 && s.pat.row.r303[1].key == 12, "second pitch, advance");

    /* "Click on the Pitch mode button twice" -> step one (p. 40) */
    ri_s303_press(&s, RI_S303_PITCHMODE);
    ri_s303_press(&s, RI_S303_PITCHMODE);
    RI_ASSERT(ri_s303_display(&s) == 1 && s.pitch_mode == 1, "double click -> step 1, mode on");
    /* but not when something happened in between */
    ri_s303_press(&s, RI_S303_STEP);
    ri_s303_press(&s, RI_S303_PITCHMODE);  /* off */
    ri_s303_press(&s, RI_S303_STEP);       /* intervening action */
    ri_s303_press(&s, RI_S303_PITCHMODE);  /* on */
    RI_ASSERT(ri_s303_display(&s) == 3, "no reset after an intervening action");

    /* toggles on the edit step; Up+Down allowed (p. 154) */
    ri_s303_init(&s, RI_SEC_SYNTH2);
    ri_s303_press(&s, RI_S303_UP);
    ri_s303_press(&s, RI_S303_DOWN);
    ri_s303_press(&s, RI_S303_ACCENT);
    ri_s303_press(&s, RI_S303_SLIDE);
    ri_s303_press(&s, RI_S303_NOTEPAUSE);
    RI_ASSERT(s.pat.row.r303[0].flags == (RI_STEP_UP | RI_STEP_DOWN | RI_STEP_ACCENT | RI_STEP_SLIDE),
        "flags toggled, REST cleared -> note (flags %02x)", s.pat.row.r303[0].flags);
    RI_ASSERT(ri_s303_led(&s, RI_S303_UP, 0) && ri_s303_led(&s, RI_S303_DOWN, 0) &&
        ri_s303_led(&s, RI_S303_NOTEPAUSE, 0) && !ri_s303_led(&s, RI_S303_NOTEPAUSE, 1), "LEDs");
    ri_s303_press(&s, RI_S303_STEP);
    RI_ASSERT(!ri_s303_led(&s, RI_S303_ACCENT, 0), "LEDs show the edit step only");

    /* Clear (p. 52, 153) */
    ri_s303_press(&s, RI_S303_CLEAR);
    RI_ASSERT(s.pat.row.r303[0].flags == RI_STEP_REST && s.pat.row.r303[0].key == 0, "Clear -> Pause, low C");

    /* knobs clamp to the registry range; Tune spans +/-12 semitones */
    RI_ASSERT(ri_s303_set_value(&s, RI_S303_TUNE, 127) == 1 && s.val[RI_S303_TUNE] == 76, "Tune clamps at +12");
    RI_ASSERT(ri_s303_set_value(&s, RI_S303_TUNE, 0) == 1 && s.val[RI_S303_TUNE] == 52, "Tune clamps at -12");
    RI_ASSERT(ri_s303_reset(&s, RI_S303_TUNE) == 1 && s.val[RI_S303_TUNE] == 64, "right-click default");
    RI_ASSERT(ri_s303_set_value(&s, RI_S303_STEP, 1) == 0, "buttons are not value controls");
    RI_ASSERT(ri_s303_press(&s, RI_S303_WAVE) == 1 && s.val[RI_S303_WAVE] == 1, "waveform switch toggles");
    RI_RESULT("sect303");
}
