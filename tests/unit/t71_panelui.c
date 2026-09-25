/* t71_panelui — front panel focus + keyboard dispatch (§12.10 G5),
 * ReBirth manual p. 20, 22, 224–225. */
#include <stdio.h>
#include "tests/helpers/ri_assert.h"
#include "gui/panelui.h"
#include "gui/ctlreg.h"

int main(void) {
    static struct RISectUI s1, s2, d8, d9, p[4], tr;
    struct RIPanelUI pu;
    uint32_t i;
    ri_sui_init(&s1, RI_SEC_SYNTH1);
    ri_sui_init(&s2, RI_SEC_SYNTH2);
    ri_sui_init(&d8, RI_SEC_808);
    ri_sui_init(&d9, RI_SEC_909);
    for (i = 0; i < 4; i++)
        ri_sui_init(&p[i], (uint8_t)(RI_SEC_PAT_SYNTH1 + i));
    ri_sui_init(&tr, RI_SEC_TRANSPORT);
    ri_panel_init(&pu);
    pu.synth[0] = &s1; pu.synth[1] = &s2; pu.drum[0] = &d8; pu.drum[1] = &d9; pu.tr = &tr;
    for (i = 0; i < 4; i++)
        pu.pat[i] = &p[i];
    RI_ASSERT(pu.focus == RI_FOCUS_SYNTH1 && !pu.opts.select_patterns && !pu.opts.program_synth, "init");
    /* focus membership: section, its mixer, its pattern section */
    RI_ASSERT(ri_panel_focus_of(RI_SEC_MIX_808) == RI_FOCUS_808 && ri_panel_focus_of(RI_SEC_PAT_909) == RI_FOCUS_909 &&
        ri_panel_focus_of(RI_SEC_TRANSPORT) == -1 && ri_panel_focus_of(RI_SEC_PCF) == -1, "focus membership");
    /* click in a section moves the focus (p. 22) */
    RI_ASSERT(ri_panel_click(&pu, RI_SEC_909) == 1 && pu.focus == RI_FOCUS_909, "click focus");
    RI_ASSERT(ri_panel_click(&pu, RI_SEC_DELAY) == 0 && pu.focus == RI_FOCUS_909, "FX click keeps focus");
    /* arrows stop at the ends */
    RI_ASSERT(ri_panel_key(&pu, RI_RAW_DOWN, 0) == 0 && pu.focus == RI_FOCUS_909, "down at bottom");
    RI_ASSERT(ri_panel_key(&pu, RI_RAW_UP, 0) == 1 && pu.focus == RI_FOCUS_808, "up");
    /* Ctrl+G turns on pattern keys; W = Synth 2 pattern 2 in its current bank; focus follows (p. 22) */
    RI_ASSERT(ri_panel_key(&pu, 0x24, RI_QUAL_CONTROL) == 1 && pu.opts.select_patterns, "ctrl-g");
    ri_sui_set(&p[1], RI_SPAT_BANK, 2);       /* bank C armed on Synth 2 */
    RI_ASSERT(ri_panel_key(&pu, 0x11, 0) == 1 && ri_spat_selected(&p[1].u.pat) == 17 && pu.focus == RI_FOCUS_SYNTH2,
        "W -> Synth 2 C2, focus Synth 2");
    RI_ASSERT(ri_panel_key(&pu, 0x05, 0) == 1 && ri_spat_selected(&p[0].u.pat) == 4 && pu.focus == RI_FOCUS_SYNTH1,
        "5 -> Synth 1 A5");
    /* Ctrl+F: program synth; keys go to the FOCUSED synth only */
    RI_ASSERT(ri_panel_key(&pu, 0x23, RI_QUAL_CONTROL) == 1 && pu.opts.program_synth, "ctrl-f");
    RI_ASSERT(ri_panel_key(&pu, RI_RAW_RETURN, 0) == 1 && ri_sui_display(&s1, RI_S303_DISPLAY) == 2 &&
        ri_sui_display(&s2, RI_S303_DISPLAY) == 1, "Return = Step on Synth 1 only");
    RI_ASSERT(ri_panel_key(&pu, RI_RAW_BACKSPACE, 0) == 1 && ri_sui_display(&s1, RI_S303_DISPLAY) == 1, "Backspace = Back");
    RI_ASSERT(ri_panel_key(&pu, 0x19, 0) == 1 && ri_sui_led(&s1, RI_S303_ACCENT, 0) == 1, "P = Accent");
    /* keypad transport */
    RI_ASSERT(ri_panel_key(&pu, RI_RAW_KPENTER, 0) == 1 && ri_sui_led(&tr, RI_STR_PLAY, 0), "Enter plays");
    RI_ASSERT(ri_panel_key(&pu, RI_RAW_SPACE, 0) == 1 && !ri_sui_led(&tr, RI_STR_PLAY, 0), "space stops");
    RI_ASSERT(ri_panel_key(&pu, RI_RAW_SPACE, 0) == 1 && ri_sui_led(&tr, RI_STR_PLAY, 0), "space plays");
    RI_ASSERT(ri_panel_key(&pu, RI_RAW_KPPLUS, 0) == 1 && ri_sui_value(&tr, RI_STR_TEMPO) == 121, "+ tempo");
    RI_ASSERT(ri_panel_key(&pu, RI_RAW_KPMINUS, 0) == 1 && ri_sui_value(&tr, RI_STR_TEMPO) == 120, "- tempo");
    RI_ASSERT(ri_panel_key(&pu, RI_RAW_KP8, 0) == 0, "bar keys inert in Pattern mode");
    ri_sui_press(&tr, RI_STR_MODE);             /* Song mode */
    ri_sui_set(&tr, RI_STR_LOOP_START, 9);
    ri_sui_set(&tr, RI_STR_LOOP_LEN, 4);
    RI_ASSERT(ri_panel_key(&pu, RI_RAW_KP8, 0) == 1 && ri_sui_value(&tr, RI_STR_BAR) == 2, "8 next bar");
    RI_ASSERT(ri_panel_key(&pu, RI_RAW_KP2, 0) == 1 && ri_sui_value(&tr, RI_STR_BAR) == 13, "2 loop end");
    RI_ASSERT(ri_panel_key(&pu, RI_RAW_KP1, 0) == 1 && ri_sui_value(&tr, RI_STR_BAR) == 9, "1 loop start");
    RI_ASSERT(ri_panel_key(&pu, RI_RAW_KP4, 0) == 1 && ri_sui_value(&tr, RI_STR_BAR) == 19, "4 fast forward");
    RI_ASSERT(ri_panel_key(&pu, RI_RAW_KPSTAR, 0) == 1 && ri_sui_led(&tr, RI_STR_RECORD, 0), "* record");
    RI_ASSERT(ri_panel_key(&pu, RI_RAW_KP0, 0) == 1 && !ri_sui_led(&tr, RI_STR_PLAY, 0), "0 stop");
    /* taps and other menus are decoded, not applied (G6 / G8) */
    pu.focus = RI_FOCUS_808;
    RI_ASSERT(ri_panel_key(&pu, 0x21, 0) == 0 && pu.last.kind == RI_KA_TAP && pu.last.arg == 2, "tap SD decoded");
    RI_ASSERT(ri_panel_key(&pu, 0x13, RI_QUAL_CONTROL) == 0 && pu.last.arg == RI_KM_RANDOMIZE, "ctrl-r decoded");
    /* missing section pointers are safe */
    pu.synth[0] = 0;
    pu.focus = RI_FOCUS_SYNTH1;
    RI_ASSERT(ri_panel_key(&pu, RI_RAW_RETURN, 0) == 0, "no synth: no crash, no change");
    RI_ASSERT(ri_panel_key(0, RI_RAW_UP, 0) == 0, "null panel");
    RI_RESULT("panelui");
}
