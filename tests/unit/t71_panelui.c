/* t71_panelui — front panel focus + keyboard dispatch (§12.10 G5),
 * ReBirth manual p. 20, 22, 224–225. */
#include <stdio.h>
#include <string.h>
#include "tests/helpers/ri_assert.h"
#include "gui/panelui.h"
#include "gui/ctlreg.h"
#include "gui/secttr.h"
#include "gui/sectpat.h"
#include "engine/seq/pattern.h"

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
    /* S7: Ctrl+M cycles the focused section's skin; Shift+Ctrl+M the panel. */
    {
        static const char *const inst[3] = { "Classic", "808-RI", "Template" };
        pu.skin_installed = inst;
        pu.skin_n = 3u;
        pu.focus = RI_FOCUS_808;
        RI_ASSERT(ri_panel_key(&pu, 0x37, RI_QUAL_CONTROL) == 1, "ctrl-m applies");
        RI_ASSERT(!strcmp(ri_skinassign_get(&pu.skin_assign, RI_SEC_808), "808-RI"), "808 skinned");
        RI_ASSERT(!strcmp(ri_skinassign_get(&pu.skin_assign, RI_SEC_SYNTH1), ""), "303 untouched");
        RI_ASSERT(!strcmp(pu.skin_current, "808-RI"), "current mirrors focus");
        RI_ASSERT(ri_panel_key(&pu, 0x37, RI_QUAL_CONTROL | RI_QUAL_LSHIFT) == 1, "shift-ctrl-m applies");
        RI_ASSERT(!strcmp(ri_skinassign_get(&pu.skin_assign, RI_SEC_SYNTH1), "Template"), "panel follows");
        RI_ASSERT(!strcmp(ri_skinassign_get(&pu.skin_assign, RI_SEC_MASTER), "Template"), "master follows");
        RI_ASSERT(!strcmp(pu.skin_current, "Template"), "current mirrors all");
    }
    /* G6b repaint policy (owner Dell 2026-10-01: 18 full repaints a
     * second while playing, 1-2 xruns each once the audio task was
     * yielded below the UI). The live feed says WHY it changed, and only
     * the canvases that can show that change are stale. */
    {
        uint32_t m, s, steps_reg = 0u, tap_all = 0u, wrong = 0u;
        /* stopped: the feed is quiet */
        RI_ASSERT(ri_panel_live(&pu, 0, 0u) == 0u, "stopped feed is quiet");
        /* the playing edge, and the first step landing */
        m = (uint32_t)ri_panel_live(&pu, 1, 0u);
        RI_ASSERT((m & RI_PANEL_CH_PLAYING) != 0u, "playing edge bit (%u)", m);
        RI_ASSERT((m & RI_PANEL_CH_PLAYHEAD) != 0u, "first step bit (%u)", m);
        m = (uint32_t)ri_panel_live(&pu, 1, 0u);
        RI_ASSERT(m == 0u, "a still playhead is silent (%u)", m);
        /* a step moving is a PLAYHEAD change and nothing else */
        m = (uint32_t)ri_panel_live(&pu, 1, 1u);
        RI_ASSERT(m == RI_PANEL_CH_PLAYHEAD, "step move is PLAYHEAD only (%u)", m);
        {
            /* the proof harness re-polls on this counter (app/sectproof.c
             * reads s_panel.changes), so it moves with a change only */
            uint32_t c0;
            ri_panel_live(&pu, 1, 0u);            /* the step moves back to 0 */
            c0 = pu.changes;
            ri_panel_live(&pu, 1, 0u);            /* nothing changed */
            RI_ASSERT(pu.changes == c0, "a silent tick does not count (%u)", pu.changes - c0);
            ri_panel_live(&pu, 1, 1u);            /* the step moved again */
            RI_ASSERT(pu.changes == c0 + 1u, "a change counts once (%u)", pu.changes - c0);
        }
        /* Song mode: the Song Position display following is its own bit.
         * A bar boundary every 16 sixteenths and a step every 1, so a
         * second bar boundary lands with the step where it already was. */
        ri_sui_set(&tr, RI_STR_MODE, 1);
        ri_sui_press(&tr, RI_STR_PLAY);
        tr.u.tr.song_bars = 64u;
        pu.play_start_ticks = 0u;
        tr.u.tr.cursor = 0u;
        {
            int bar0 = ri_sui_value(&tr, RI_STR_BAR);
            m = (uint32_t)ri_panel_live(&pu, 1, 16u);
            RI_ASSERT((m & RI_PANEL_CH_FOLLOW) != 0u, "the bar display follow bit (%u)", m);
            RI_ASSERT(ri_sui_value(&tr, RI_STR_BAR) == bar0 + 1, "the display moved a bar (%d)",
                ri_sui_value(&tr, RI_STR_BAR));
            m = (uint32_t)ri_panel_live(&pu, 1, 32u);
            RI_ASSERT(m == RI_PANEL_CH_FOLLOW, "a bar boundary with the step still is FOLLOW alone (%u)", m);
            m = (uint32_t)ri_panel_live(&pu, 1, 33u);
            RI_ASSERT(m == RI_PANEL_CH_PLAYHEAD, "a step inside the bar is PLAYHEAD only (%u)", m);
        }
        /* A held delete-tap edits the row it lands on, and that is a
         * change of its own; a tap with nothing to delete is not. */
        tr.u.tr.song_mode = 0;   /* pattern mode: the bar display does not follow */
        ri_pdrum_set_ac(&d8.u.s808.pat, 1u, 1);
        pu.del_held = 1u;
        pu.del_focus = RI_FOCUS_808;
        pu.del_arg = 0;
        m = (uint32_t)ri_panel_live(&pu, 1, 48u);      /* playhead to step 0, no AC there */
        RI_ASSERT(m == RI_PANEL_CH_PLAYHEAD, "a tap with nothing to delete is not a change (%u)", m);
        m = (uint32_t)ri_panel_live(&pu, 1, 49u);      /* onto step 1: the tap clears it */
        RI_ASSERT(m == (RI_PANEL_CH_PLAYHEAD | RI_PANEL_CH_TAP),
            "a delete-tap that edits a step is its own bit (%u)", m);
        RI_ASSERT(ri_pdrum_get(&d8.u.s808.pat, 1u, 0u) == RI_HIT_OFF, "step 1 cleared");
        m = (uint32_t)ri_panel_live(&pu, 1, 64u);      /* step 0 again: nothing to delete */
        RI_ASSERT(m == RI_PANEL_CH_PLAYHEAD, "a tap on a cleared step changes nothing (%u)", m);
        m = (uint32_t)ri_panel_live(&pu, 1, 65u);      /* back onto step 1, now empty */
        RI_ASSERT(m == RI_PANEL_CH_PLAYHEAD, "a second tap on step 1 changes nothing (%u)", m);
        pu.del_held = 0u;
        /* The policy: what each change makes stale on each section. */
        for (s = 0; s < RI_SEC_COUNT; s++) {
            uint32_t k;
            for (k = 0; k < ri_ctlreg_count(); k++) {
                const struct RICtlDef *dd = ri_ctlreg_at(k);
                if (dd && dd->section == s && dd->kind == RI_CK_STEP)
                    steps_reg |= 1u << s;
            }
        }
        RI_ASSERT(steps_reg == ((1u << RI_SEC_808) | (1u << RI_SEC_909) | (1u << RI_SEC_LEVI)),
            "step rows live in the 808, 909 and Levi sections (%u)", steps_reg);
        for (s = 0; s < RI_SEC_COUNT; s++) {
            int want = ((steps_reg >> s) & 1u) ? RI_STALE_ALL : RI_STALE_NONE;
            if (ri_panel_live_stale(RI_PANEL_CH_TAP, s) != want)
                tap_all++;
            if (ri_panel_live_stale(0u, s) != RI_STALE_NONE)
                wrong++;
            if (ri_panel_live_stale(RI_PANEL_CH_PLAYING, s) != RI_STALE_NONE)
                wrong++;
            if (ri_panel_live_stale(0x40u, s) != RI_STALE_NONE)
                wrong++;
            if (ri_panel_live_stale(RI_PANEL_CH_PLAYHEAD, s) !=
                ((s == RI_SEC_808 || s == RI_SEC_909) ? RI_STALE_STEPS : RI_STALE_NONE))
                wrong++;
            if (ri_panel_live_stale(RI_PANEL_CH_FOLLOW, s) !=
                (s == RI_SEC_TRANSPORT ? RI_STALE_BAR : RI_STALE_NONE))
                wrong++;
            /* both live reasons at once: each canvas answers for its own */
            if (ri_panel_live_stale(RI_PANEL_CH_PLAYHEAD | RI_PANEL_CH_FOLLOW, s) !=
                (s == RI_SEC_TRANSPORT ? RI_STALE_BAR :
                 (s == RI_SEC_808 || s == RI_SEC_909) ? RI_STALE_STEPS : RI_STALE_NONE))
                wrong++;
        }
        RI_ASSERT(tap_all == 0u, "a tap only stales the step rows (%u sections)", tap_all);
        RI_ASSERT(wrong == 0u, "the policy answers wrong for %u section/change pairs", wrong);
    }
    RI_RESULT("panelui");
}
