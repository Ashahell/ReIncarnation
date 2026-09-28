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
    /* Algo block: selects clamp, op knob packs op*16+mode, opsel
     * re-points the packed display at the newly selected op. */
    RI_ASSERT(ri_slevi_set_value(&s, RI_SLEVI_ALGO, 5) == 1, "algo");
    RI_ASSERT(s.val[RI_SLEVI_ALGO] == 5, "algo stored");
    RI_ASSERT(ri_slevi_set_value(&s, RI_SLEVI_ALGO, 5) == 0, "algo same");
    RI_ASSERT(ri_slevi_set_value(&s, RI_SLEVI_ALGO, 99) == 1, "algo clamp");
    RI_ASSERT(s.val[RI_SLEVI_ALGO] == 7, "algo max");
    RI_ASSERT(ri_slevi_set_value(&s, RI_SLEVI_ALGOB, 3) == 1, "algob");
    RI_ASSERT(ri_slevi_set_value(&s, RI_SLEVI_MORPH, 50) == 1, "morph");
    RI_ASSERT(s.val[RI_SLEVI_MORPH] == 50, "morph stored");
    RI_ASSERT(ri_slevi_set_value(&s, RI_SLEVI_OPSEL, 3) == 1, "opsel");
    RI_ASSERT(s.opsel == 3u && s.val[RI_SLEVI_OPMODE] == 48, "opsel points packed");
    RI_ASSERT(ri_slevi_set_value(&s, RI_SLEVI_OPMODE, 2) == 1, "opmode");
    RI_ASSERT(s.opmode[3] == 2u && s.val[RI_SLEVI_OPMODE] == 50, "packed 3*16+2");
    RI_ASSERT(ri_slevi_set_value(&s, RI_SLEVI_OPMODE, 2) == 0, "opmode same");
    RI_ASSERT(ri_slevi_set_value(&s, RI_SLEVI_OPSEL, 0) == 1, "opsel back");
    RI_ASSERT(s.val[RI_SLEVI_OPMODE] == 0, "packed follows selection");
    RI_ASSERT(ri_slevi_set_value(&s, RI_SLEVI_OPMODE, 99) == 1, "opmode clamp");
    RI_ASSERT(s.opmode[0] == 6u && s.val[RI_SLEVI_OPMODE] == 6, "opmode max");
    RI_ASSERT(ri_slevi_reset(&s, RI_SLEVI_OPMODE) == 1, "opmode reset");
    RI_ASSERT(s.opmode[0] == 0u, "opmode default");
    /* Filter block: type clamps, drive clamps through the knob law. */
    RI_ASSERT(ri_slevi_set_value(&s, RI_SLEVI_FTYPE, 2) == 1, "ftype");
    RI_ASSERT(s.val[RI_SLEVI_FTYPE] == 2, "ftype stored");
    RI_ASSERT(ri_slevi_set_value(&s, RI_SLEVI_FTYPE, 9) == 1, "ftype clamp");
    RI_ASSERT(s.val[RI_SLEVI_FTYPE] == 3, "ftype max");
    RI_ASSERT(ri_slevi_set_value(&s, RI_SLEVI_DRIVE, 200) == 1, "drive clamp");
    RI_ASSERT(s.val[RI_SLEVI_DRIVE] == 127, "drive max");
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
