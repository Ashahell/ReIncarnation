/* t69_secttr — Transport panel behaviour (§12.10 G3/G4), ReBirth manual
 * p. 144-146; laws from engine/seq/transport.h (§12.9a). */
#include <stdio.h>
#include "tests/helpers/ri_assert.h"
#include "gui/secttr.h"
#include "gui/ctlreg.h"

int main(void) {
    struct RISectTr s;
    int i;
    RI_ASSERT(ri_str_init(&s) == 0 && ri_str_value(&s, RI_STR_TEMPO) == 120 && ri_str_value(&s, RI_STR_BAR) == 1 &&
        ri_str_led(&s, RI_STR_MODE, 0) && !ri_str_led(&s, RI_STR_MODE, 1), "init: Pattern mode, 120 bpm, bar 1");
    /* tempo 20..500 with arrows */
    ri_str_set_value(&s, RI_STR_TEMPO, 499);
    RI_ASSERT(ri_str_step(&s, RI_STR_TEMPO, 1) == 1 && ri_str_step(&s, RI_STR_TEMPO, 1) == 0 &&
        ri_str_value(&s, RI_STR_TEMPO) == 500, "tempo stops at 500");
    ri_str_set_value(&s, RI_STR_TEMPO, 5);
    RI_ASSERT(ri_str_value(&s, RI_STR_TEMPO) == 20 && ri_str_reset(&s, RI_STR_TEMPO) == 1 &&
        ri_str_value(&s, RI_STR_TEMPO) == 120, "tempo floor 20, default 120");
    /* Pattern mode: Rew/FF, Record, Bar arrows have no function */
    RI_ASSERT(ri_str_press(&s, RI_STR_FF) == 0 && ri_str_press(&s, RI_STR_RECORD) == 0 &&
        ri_str_step(&s, RI_STR_BAR, 1) == 0 && ri_str_value(&s, RI_STR_BAR) == 1, "pattern mode ignores song controls");
    /* play / stop */
    RI_ASSERT(ri_str_press(&s, RI_STR_PLAY) == 1 && ri_str_led(&s, RI_STR_PLAY, 0), "play");
    RI_ASSERT(ri_str_press(&s, RI_STR_STOP) == 1 && !ri_str_led(&s, RI_STR_PLAY, 0), "stop");
    /* Song mode */
    RI_ASSERT(ri_str_press(&s, RI_STR_MODE) == 1 && ri_str_led(&s, RI_STR_MODE, 1), "song mode");
    RI_ASSERT(ri_str_press(&s, RI_STR_FF) == 1 && ri_str_value(&s, RI_STR_BAR) == 11, "FF: ten bars forward");
    RI_ASSERT(ri_str_step(&s, RI_STR_BAR, 1) == 1 && ri_str_value(&s, RI_STR_BAR) == 12, "bar arrow: one bar");
    RI_ASSERT(ri_str_press(&s, RI_STR_REW) == 1 && ri_str_value(&s, RI_STR_BAR) == 2, "Rew: ten bars back");
    RI_ASSERT(ri_str_press(&s, RI_STR_REW) == 1 && ri_str_value(&s, RI_STR_BAR) == 1, "Rew clamps at bar 1");
    for (i = 0; i < 120; i++)
        ri_str_press(&s, RI_STR_FF);
    RI_ASSERT(ri_str_value(&s, RI_STR_BAR) == 999, "FF clamps at bar 999");
    /* stop sequence (engine law): playing -> stop holds; next -> loop start; next -> song start */
    ri_str_set_value(&s, RI_STR_LOOP_START, 5);
    ri_str_press(&s, RI_STR_PLAY);
    ri_str_press(&s, RI_STR_STOP);
    RI_ASSERT(ri_str_value(&s, RI_STR_BAR) == 999, "stop holds position");
    ri_str_press(&s, RI_STR_STOP);
    RI_ASSERT(ri_str_value(&s, RI_STR_BAR) == 5, "second stop: loop start");
    ri_str_press(&s, RI_STR_STOP);
    RI_ASSERT(ri_str_value(&s, RI_STR_BAR) == 1, "third stop: song start");
    /* record: song mode only; back to Pattern mode drops record */
    RI_ASSERT(ri_str_press(&s, RI_STR_RECORD) == 1 && ri_str_led(&s, RI_STR_RECORD, 0) && ri_str_led(&s, RI_STR_PLAY, 0),
        "record (plays)");
    ri_str_press(&s, RI_STR_MODE);
    RI_ASSERT(!ri_str_led(&s, RI_STR_RECORD, 0) && ri_str_led(&s, RI_STR_PLAY, 0), "pattern mode: record off, still playing");
    /* loop */
    RI_ASSERT(ri_str_press(&s, RI_STR_LOOP) == 1 && ri_str_led(&s, RI_STR_LOOP, 0), "loop on");
    ri_str_set_value(&s, RI_STR_LOOP_START, 998);
    ri_str_set_value(&s, RI_STR_LOOP_LEN, 8);
    RI_ASSERT(ri_str_value(&s, RI_STR_LOOP_START) == 998 && ri_str_value(&s, RI_STR_LOOP_LEN) == 2,
        "loop clamped to the song end (engine clamp law)");
    RI_ASSERT(ri_str_step(&s, RI_STR_LOOP_LEN, -1) == 1 && ri_str_value(&s, RI_STR_LOOP_LEN) == 1, "length arrow");
    /* shuffle knob, indicators */
    RI_ASSERT(ri_str_set_value(&s, RI_STR_SHUFFLE, 200) == 1 && ri_str_value(&s, RI_STR_SHUFFLE) == 127, "shuffle clamps");
    ri_str_indicator_set(&s, RI_STR_SYNC, 1);
    ri_str_indicator_set(&s, RI_STR_MIDI, 7);
    RI_ASSERT(ri_str_led(&s, RI_STR_SYNC, 0) == 1 && ri_str_led(&s, RI_STR_MIDI, 0) == 1, "indicators");
    RI_ASSERT(ri_str_press(&s, RI_STR_MIDI) == 0 && ri_str_set_value(&s, RI_STR_BAR, 7) == 0, "read-only");
    RI_RESULT("secttr");
}
