/* gui/panelui.c — front panel focus + keyboard dispatch (§12.10 G5). */
#include "gui/panelui.h"
#include "gui/ctlreg.h"

void ri_panel_init(struct RIPanelUI *p) {
    uint32_t i;
    if (!p)
        return;
    p->focus = RI_FOCUS_SYNTH1;
    p->pad[0] = p->pad[1] = p->pad[2] = 0;
    p->opts.select_patterns = 0;
    p->opts.program_synth = 0;
    p->synth[0] = p->synth[1] = p->drum[0] = p->drum[1] = p->tr = 0;
    for (i = 0; i < RI_FOCUS_COUNT; i++)
        p->pat[i] = 0;
    p->last.kind = RI_KA_NONE;
    p->last.section = 0;
    p->last.arg = 0;
    p->changes = 0;
    p->last_raw = p->last_qual = 0;
}

int ri_panel_focus_of(uint32_t section) {
    switch (section) {
    case RI_SEC_SYNTH1: case RI_SEC_MIX_SYNTH1: case RI_SEC_PAT_SYNTH1: return RI_FOCUS_SYNTH1;
    case RI_SEC_SYNTH2: case RI_SEC_MIX_SYNTH2: case RI_SEC_PAT_SYNTH2: return RI_FOCUS_SYNTH2;
    case RI_SEC_808: case RI_SEC_MIX_808: case RI_SEC_PAT_808: return RI_FOCUS_808;
    case RI_SEC_909: case RI_SEC_MIX_909: case RI_SEC_PAT_909: return RI_FOCUS_909;
    default: return -1;
    }
}

static int set_focus(struct RIPanelUI *p, int f) {
    if (f < 0 || f >= (int)RI_FOCUS_COUNT || f == p->focus)
        return 0;
    p->focus = (uint8_t)f;
    p->changes++;
    return 1;
}

int ri_panel_click(struct RIPanelUI *p, uint32_t section) {
    return p ? set_focus(p, ri_panel_focus_of(section)) : 0;
}

int ri_panel_pattern_selected(struct RIPanelUI *p, uint32_t section) {
    return ri_panel_click(p, section);
}

static int transport(struct RIPanelUI *p, int cmd) {
    struct RISectUI *t = p->tr;
    if (!t)
        return 0;
    switch (cmd) {
    case RI_KT_STOP: return ri_sui_press(t, RI_STR_STOP);
    case RI_KT_PLAY: return ri_sui_press(t, RI_STR_PLAY);
    case RI_KT_STOPPLAY:   /* space: Stop while running, Play while stopped */
        return ri_sui_press(t, ri_sui_led(t, RI_STR_PLAY, 0) ? RI_STR_STOP : RI_STR_PLAY);
    case RI_KT_RECORD: return ri_sui_press(t, RI_STR_RECORD);
    case RI_KT_FF: return ri_sui_press(t, RI_STR_FF);
    case RI_KT_REW: return ri_sui_press(t, RI_STR_REW);
    case RI_KT_LOOP_START: return ri_str_goto_loop(&t->u.tr, 0);
    case RI_KT_LOOP_END: return ri_str_goto_loop(&t->u.tr, 1);
    case RI_KT_NEXT_BAR: return ri_sui_step(t, RI_STR_BAR, 1);
    case RI_KT_PREV_BAR: return ri_sui_step(t, RI_STR_BAR, -1);
    case RI_KT_TEMPO_UP: return ri_sui_step(t, RI_STR_TEMPO, 1);
    case RI_KT_TEMPO_DOWN: return ri_sui_step(t, RI_STR_TEMPO, -1);
    default: return 0;
    }
}

int ri_panel_key(struct RIPanelUI *p, uint32_t raw, uint32_t qual) {
    struct RIKeyAction a;
    int ch = 0;
    if (!p)
        return 0;
    p->last_raw = (uint16_t)raw;
    p->last_qual = (uint16_t)qual;
    a = ri_key_decode(raw, qual, &p->opts, p->focus);
    if (a.kind != RI_KA_NONE)
        p->last = a;
    switch (a.kind) {
    case RI_KA_FOCUS:   /* p. 22: up/down arrows; stops at the first/last section */
        ch = set_focus(p, (int)p->focus + a.arg);
        break;
    case RI_KA_PATTERN:  /* p. 20/224: pattern 1-8 of the current bank; focus follows */
        if (p->pat[a.section])
            ch = ri_sui_set(p->pat[a.section], RI_SPAT_PATTERN, a.arg);
        ch |= set_focus(p, a.section);
        break;
    case RI_KA_TRANSPORT:
        ch = transport(p, a.arg);
        break;
    case RI_KA_SYNTH:   /* p. 225: the focused synth's step-entry keys */
        if (p->synth[a.section])
            ch = ri_sui_press(p->synth[a.section], (uint32_t)a.arg);
        break;
    case RI_KA_MENU:
        if (a.arg == RI_KM_PROGRAM_SYNTH) {
            p->opts.program_synth = (uint8_t)!p->opts.program_synth;
            ch = 1;
        } else if (a.arg == RI_KM_SELECT_PATTERNS) {
            p->opts.select_patterns = (uint8_t)!p->opts.select_patterns;
            ch = 1;
        }
        break;
    default:
        break;
    }
    if (ch)
        p->changes++;
    return ch;
}
