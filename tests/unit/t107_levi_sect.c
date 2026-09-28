/* t107_levi_sect — Levi section front-panel behaviour (owner 2026-09-28).
 * 909-style lane select + 16 step toggles, 303-style edit step + piano
 * keyboard for pitch. Newly-on steps take middle C; keys retune the
 * (edit step, selected lane); STEP/BACK walk the edit step.
 */
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "tests/helpers/ri_assert.h"
#include "gui/sectlevi.h"
#include "gui/ctlreg.h"
#include "engine/seq/pattern.h"

int main(void) {
    struct RISectLevi s;
    RI_ASSERT(ri_slevi_init(&s) == 0, "init rc");
    RI_ASSERT(ri_slevi_init(0) == 2, "init null");
    RI_ASSERT(s.section == RI_SEC_LEVI, "section");
    RI_ASSERT(s.sel == 0u && s.edit_step == 0u, "defaults");
    RI_ASSERT(s.pat.kind == RI_PATTERN_KIND_LEVI, "pat kind");
    /* Lane select via set (selector idiom). */
    RI_ASSERT(ri_slevi_set_value(&s, RI_SLEVI_SELECT, 3) == 1, "select");
    RI_ASSERT(s.sel == 3u, "sel 3");
    RI_ASSERT(ri_slevi_set_value(&s, RI_SLEVI_SELECT, 9) == 1, "select clamp");
    RI_ASSERT(s.sel == 5u, "sel max");
    RI_ASSERT(ri_slevi_set_value(&s, RI_SLEVI_SELECT, -4) == 1, "select floor");
    RI_ASSERT(s.sel == 0u, "sel min");
    /* Step buttons toggle the selected lane (middle C on enable). */
    RI_ASSERT(ri_slevi_press(&s, RI_SLEVI_STEP0 + 4) == 1, "step on");
    RI_ASSERT(ri_levi_on(&s.pat, 4u, 0u) == 1, "lane on");
    RI_ASSERT(ri_levi_get(&s.pat, 4u, 0u) == 60u, "middle C");
    RI_ASSERT(ri_slevi_press(&s, RI_SLEVI_STEP0 + 4) == 1, "step off");
    RI_ASSERT(ri_levi_on(&s.pat, 4u, 0u) == 0, "lane off");
    /* Piano keys retune (edit step, selected lane). */
    RI_ASSERT(ri_slevi_press(&s, RI_SLEVI_STEP0 + 4) == 1, "step on");
    RI_ASSERT(ri_slevi_press(&s, RI_SLEVI_KEY0 + 7) == 1, "key G");
    RI_ASSERT(ri_levi_get(&s.pat, 0u, 0u) == 67u, "pitched");
    RI_ASSERT(ri_slevi_led(&s, RI_SLEVI_KEY0 + 7) == 1, "key led");
    RI_ASSERT(ri_slevi_led(&s, RI_SLEVI_KEY0 + 6) == 0, "key dark");
    RI_ASSERT(ri_slevi_led(&s, RI_SLEVI_STEP0 + 4) == 1, "step led");
    /* STEP/BACK walk the edit step; DISPLAY reads it. */
    RI_ASSERT(ri_slevi_press(&s, RI_SLEVI_STEP) == 1, "step fwd");
    RI_ASSERT(ri_slevi_display(&s) == 2, "display 2");
    RI_ASSERT(ri_slevi_press(&s, RI_SLEVI_BACK) == 1, "step back");
    RI_ASSERT(ri_slevi_display(&s) == 1, "display 1");
    RI_ASSERT(ri_slevi_press(&s, RI_SLEVI_BACK) == 1, "wrap back");
    RI_ASSERT(ri_slevi_display(&s) == 16, "display 16");
    /* Mode switch toggles FM/PM. */
    RI_ASSERT(ri_slevi_press(&s, RI_SLEVI_MODE) == 1, "mode on");
    RI_ASSERT(s.val[RI_SLEVI_MODE] == 1, "mode PM");
    RI_ASSERT(ri_slevi_press(&s, RI_SLEVI_MODE) == 1, "mode off");
    RI_ASSERT(s.val[RI_SLEVI_MODE] == 0, "mode FM");
    RI_ASSERT(ri_slevi_set_value(&s, 0u, 200) == 1, "knob clamp");
    RI_ASSERT(s.val[0] == 127, "knob max");
    RI_ASSERT(ri_slevi_set_value(&s, 0u, -5) == 1, "knob floor");
    RI_ASSERT(s.val[0] == 0, "knob min");
    RI_ASSERT(ri_slevi_set_value(&s, 0u, -5) == 0, "knob same");
    /* Fail-closed. */
    RI_ASSERT(ri_slevi_press(0, RI_SLEVI_STEP0) == 0, "press null");
    RI_ASSERT(ri_slevi_press(&s, RI_SLEVI_NCTL) == 0, "press bad");
    RI_ASSERT(ri_slevi_set_value(0, 0u, 1) == 0, "set null");
    RI_ASSERT(ri_slevi_led(0, 0u) == 0, "led null");
    RI_ASSERT(ri_slevi_display(0) == 0, "display null");
    RI_RESULT("levisect");
}
