/* t232_midi_push — a ReBirth CC must reach the ENGINE, not just the lamp.
 *
 * THE PUREEST INSTANCE of "looks live but isn't": for a G7 Control Change the
 * **lamp moves and the engine never hears it**.
 *
 * Two MIDI devices, two paths, and only one of them reaches the engine:
 *
 *   G7 (Appendix C)  `cc()` in midimap.c -> ri_sui_press / ri_sui_set
 *                    PANEL ONLY. The model changes, the lamp redraws...
 *   M4 (Leviasynth)  midi_drain -> ri_ctl_send / ri_ctl_send2
 *                    ENGINE ONLY, deliberately isolated from the G7 map (t183)
 *
 * The G7 change is then never pushed, because `sync_values()` -- the only
 * per-frame pusher -- gates on **per-canvas** `MUIA_RSection_Changes`, and a
 * MIDI message never goes through the canvas event handler. `s_panel.changes`
 * is read in exactly one place (`midi_drain`), and all it does there is redraw:
 *
 *   if (s_panel.changes != ch0)
 *       for (i = 0u; i < 15u; i++) MUI_Redraw(s_canvas[i], MADF_DRAWOBJECT);
 *
 * So the redraw happens, the push does not. That is why no test caught it:
 * t73 exercises the map against the PANEL and never looks at the engine, and
 * the shell half is not host-testable.
 *
 * WHAT THIS TEST PINS is the portable half -- the map must REPORT which control
 * it touched, so the shell can push exactly that one. Reporting a single
 * control per message is exact, not approximate: one `ri_midi_msg` call handles
 * one message and can change at most one control.
 */
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "tests/helpers/ri_assert.h"
#include "gui/ctlreg.h"
#include "gui/midimap.h"
#include "gui/sectfx.h"
#include "gui/sectmix.h"
#include "gui/sectui.h"
#include "gui/sect303.h"
#include "gui/sectpat.h"
#include "gui/keymap.h"
#include "gui/sect808.h"

/* The transport slot is the only one note_various needs for CC 69..73; a
 * zeroed RISectUI is enough for the note tables under test, and using a real
 * one keeps the fixture free of a malloc dependency. */
/* Real objects: RIPanelUI holds POINTERS, so zeroing it and calling
 * ri_sui_init() on them segfaults. The mixer also needs ONE shared board,
 * exactly as riapp.c binds it, because insert routing spans strips. */
static struct RIPanelUI panel;
static struct RIMidiIn m;
static struct RISectUI g_fx[4], g_mix[6], g_tr, g_synth[2], g_drum[2], g_pat[RI_FOCUS_COUNT];
static struct RIMixBoard g_board;
static struct RIFxRack g_rack;

static void fresh(void) {
    unsigned i;
    memset(&panel, 0, sizeof panel);
    ri_smix_init(&g_board);
    ri_sfx_rack_init(&g_rack);
    for (i = 0; i < 4; i++) {
        ri_sui_init(&g_fx[i], (uint8_t)(RI_SEC_PCF + i));
        /* the shared FX rack, as riapp.c binds it -- without it the enable is
         * fail-closed and a MIDI note changes nothing at all */
        ri_sui_bind_fx_rack(&g_fx[i], &g_rack);
        panel.fx[i] = &g_fx[i];
    }
    /* ui_of() indexes mix[] as sec - RI_SEC_MIX_SYNTH1 (0..4, master last)
     * with LEVI at 5 -- the same wiring riapp.c does. Wiring only some of
     * them makes ui_of() return NULL and the control silently unreachable,
     * which is exactly how the census miscounted 74 CCs on the first run. */
    for (i = 0; i < 6; i++) {
        uint8_t sec = i == 5u ? (uint8_t)RI_SEC_MIX_LEVI
            : (uint8_t)(i == 4u ? RI_SEC_MASTER : (uint32_t)RI_SEC_MIX_SYNTH1 + i);
        ri_sui_init(&g_mix[i], sec);
        ri_sui_bind_board(&g_mix[i], &g_board);
        panel.mix[i] = &g_mix[i];
    }
    ri_sui_init(&g_synth[0], RI_SEC_SYNTH1); ri_sui_init(&g_synth[1], RI_SEC_SYNTH2);
    panel.synth[0] = &g_synth[0]; panel.synth[1] = &g_synth[1];
    ri_sui_init(&g_drum[0], RI_SEC_808); ri_sui_init(&g_drum[1], RI_SEC_909);
    panel.drum[0] = &g_drum[0]; panel.drum[1] = &g_drum[1];
    for (i = 0; i < RI_FOCUS_COUNT; i++) {
        ri_sui_init(&g_pat[i], (uint8_t)(RI_SEC_PAT_SYNTH1 + i));
        panel.pat[i] = &g_pat[i];
    }
    ri_sui_init(&g_tr, RI_SEC_TRANSPORT);
    panel.tr = &g_tr;
    ri_midi_init(&m, 0u);
}

int main(void) {
    /* ---- a CC that moves a KNOB must report its registry id and value ---- */
    fresh();
    /* The Levi's Level fader is CC 109 (RI_MIXER Level). Pick a CC the map
     * owns and whose control is a knob, so the set path is exercised rather
     * than the switch path. */
    {
        const struct RICtlDef *d = ri_ctlreg_by_cc(109u);
        RI_ASSERT(d != 0, "CC 109 is in the map");
        ri_midi_msg(&m, &panel, 0xB0u, 109u, 127u);
        RI_ASSERT(m.touched == 1u && m.touch_reg == (d ? d->reg_id : 0u),
            "CC 109 reports its registry id %04x (got %04x)", d ? d->reg_id : 0u, m.touch_reg);
        RI_ASSERT(m.touch_val == (d ? (int16_t)d->max_v : (int16_t)-1),
            "CC 109 reports the value it set (%d, want %d)", (int)m.touch_val,
            d ? (int)d->max_v : -1);
    }

    /* ---- the FX enables (Note On 74..77) ---- */
    fresh();
    ri_midi_msg(&m, &panel, 0x90u, 74u, 127u);
    RI_ASSERT(m.touched == 1u &&
        m.touch_reg == ((uint16_t)((RI_SEC_PCF << 8) | RI_SFX_ONOFF)),
        "note 74 reports the PCF enable %04x (got %04x)",
        (uint16_t)((RI_SEC_PCF << 8) | RI_SFX_ONOFF), m.touch_reg);
    RI_ASSERT(ri_sfx_led(&panel.fx[0]->u.fx, RI_SFX_ONOFF) == 1, "and the lamp did move");

    /* A message that changes NOTHING must report nothing, or the shell would
     * push a stale value every time an unrelated message arrives. */
    fresh();
    /* A CC is the honest no-op: the same controller value twice. (Note 74 is
     * a PRESS -- a toggle -- so repeating it genuinely changes the state
     * again, which is why the first version of this assertion was wrong.) */
    ri_midi_msg(&m, &panel, 0xB0u, 109u, 127u);
    RI_ASSERT(m.touched == 1u, "the first CC reports something");
    ri_midi_msg(&m, &panel, 0xB0u, 109u, 127u);
    RI_ASSERT(m.touched == 0u,
        "re-sending the same value is a no-op and reports nothing (reg %04x)",
        m.touch_reg);

    /* ---- a mixer insert via Note On 80..93 ---- */
    fresh();
    ri_midi_msg(&m, &panel, 0x90u, 80u, 127u);
    RI_ASSERT(m.touched == 1u &&
        m.touch_reg == ((uint16_t)((RI_SEC_MIX_SYNTH1 << 8) | RI_SMIX_PCF)),
        "note 80 reports the Synth1 PCF insert %04x (got %04x)",
        (uint16_t)((RI_SEC_MIX_SYNTH1 << 8) | RI_SMIX_PCF), m.touch_reg);

    /* ---- unrelated traffic must never claim to have touched a control ----
     * Notes on an unmapped pitch, other channels, realtime and SysEx. */
    fresh();
    ri_midi_msg(&m, &panel, 0x90u, 60u, 127u);   /* a pitch, not a switch */
    RI_ASSERT(m.touched == 0u, "an unmapped note reports nothing");
    ri_midi_msg(&m, &panel, 0xB1u, 109u, 127u);  /* wrong channel */
    RI_ASSERT(m.touched == 0u, "another channel reports nothing");
    ri_midi_msg(&m, &panel, 0xF8u, 0u, 0u);      /* clock */
    RI_ASSERT(m.touched == 0u, "a clock reports nothing");
    ri_midi_msg(&m, &panel, 0xF0u, 0x7Eu, 0x7F); /* SysEx */
    RI_ASSERT(m.touched == 0u, "SysEx reports nothing");

    /* ---- every CC in the map must be pushable, not just the ones named ----
     * The gap was invisible because a handful of controls happened to be
     * exercised. If any mapped CC cannot report, the shell cannot push it. */
    {
        unsigned cc_no;
        int pushed = 0, silent = 0;
        for (cc_no = 0; cc_no < 128u; cc_no++) {
            if (!ri_ctlreg_by_cc((uint8_t)cc_no))
                continue;
            fresh();
            ri_midi_msg(&m, &panel, 0xB0u, (uint8_t)cc_no, 127u);
            if (m.touched) pushed++;
            else {
                const struct RICtlDef *dd = ri_ctlreg_by_cc((uint8_t)cc_no);
                printf("  NO REPORT cc=%u %04x sec=%u idx=%u kind=%u bind=%u\n",
                    cc_no, dd ? dd->reg_id : 0u, dd ? dd->section : 0u,
                    dd ? (dd->reg_id & 0xFFu) : 0u, dd ? dd->kind : 0u,
                    dd ? dd->bind : 0u);
                silent++;
            }
        }
        RI_ASSERT(silent == 0, "%d mapped CC(s) change a control but report "
            "nothing to push", silent);
        RI_ASSERT(pushed > 40, "mapped CCs that report: %d", pushed);
    }

    /* ================= the note path: VERIFIED, and there is no gap =========
     * The P-RB2 commit ended by saying note_pattern's controls have "no engine
     * target, so there is nothing to push -- an ASSUMPTION ... should be
     * verified". Verified. And the assumption was wrong in the direction of
     * OVER-CLAIMING: there is nothing wrong there at all.
     *
     * Every control the Note-On path can touch is panel-only, so a G7 note
     * legitimately pushes nothing: there is no engine target to push TO. The
     * CC path is the one that carries targets, and that is the path P-RB2
     * fixed.
     *
     * THE CENSUS IS DRIVEN BY THE MAP, not by hand-written index ranges. Two
     * earlier versions tried ranges and both were wrong: the first scanned
     * whole sections and flagged 29 rows that were knobs the note path never
     * touches, the second used RI_S808_STEP0 + 16 and swept the 808's
     * *instrument* rows (HT/Level, CY/Tune) because they share the index band.
     * A census whose ranges are written by hand reports whatever those ranges
     * happen to contain. Driving every note in both modes with every focus and
     * inspecting what the map ACTUALLY reports cannot be wrong that way.
     *
     * The invariant is now ENFORCED rather than left in a comment: if any row
     * the note path reaches ever GAINS a bind, this fails and the map must be
     * taught to report it. A property nobody checks decays into a comment
     * nobody reads, which is how the P-RB2 caveat became a claim at all. */
    {
        unsigned nn, f, mode;
        int seen = 0, panel_only_reported = 0, leak = 0;
        for (f = 0; f < RI_FOCUS_COUNT; f++)
            for (mode = 0; mode < 2u; mode++)
                for (nn = 12u; nn <= 94u; nn++) {
                    const struct RICtlDef *d;
                    fresh();
                    panel.focus = (uint8_t)f;
                    panel.opts.select_patterns = (uint8_t)(mode ? 1 : 0);
                    panel.opts.program_synth = (uint8_t)(mode ? 0 : 1);
                    ri_midi_msg(&m, &panel, 0x90u, (uint8_t)nn, 127u);
                    if (!m.touched)
                        continue;
                    seen++;
                    d = ri_ctlreg_find(m.touch_reg);
                    if (!d)
                        continue;
                    /* Split by FAMILY, because "reported with a target" and
                     * "not reported" are different things and conflating them
                     * was the bug in the previous two versions of this census.
                     * The mixer and FX families DO carry targets and DO report
                     * -- asserted individually above and below. The families
                     * here (synth / drum / pattern) are supposed to be
                     * panel-only, so anything they report must have no target;
                     * if one ever gains a bind, that is the moment the map has
                     * to learn to push it, and this is where it gets noticed. */
                    if (d->section == RI_SEC_SYNTH1 || d->section == RI_SEC_SYNTH2
                        || d->section == RI_SEC_808 || d->section == RI_SEC_909
                        || (d->section >= RI_SEC_PAT_SYNTH1
                            && d->section <= RI_SEC_PAT_LEVI)) {
                        panel_only_reported++;
                        if (ri_ctlreg_auto_id(d) != 0u) {
                            leak++;
                            printf("  NOTE-PATH LEAK %04x %s/%s bind=%u\n",
                                d->reg_id, d->group, d->legend, d->bind);
                        }
                    }
                }
        RI_ASSERT(seen > 20, "the note-path sweep actually exercised controls (%d)", seen);
        RI_ASSERT(panel_only_reported > 0,
            "and reached the panel-only families it is reasoning about (%d)",
            panel_only_reported);
        RI_ASSERT(leak == 0,
            "%d panel-only control(s) now HAVE an engine target -- the map must "
            "be taught to report them, or their lamps move and their sounds do "
            "not", leak);
    }

    /* And the two named rows the caveat was about, so the reader does not have
     * to take a census on trust. */
    {
        const struct RICtlDef *bank = ri_ctlreg_find(
            (uint16_t)((RI_SEC_PAT_SYNTH1 << 8) | RI_SPAT_BANK));
        const struct RICtlDef *accent = ri_ctlreg_find(
            (uint16_t)((RI_SEC_SYNTH1 << 8) | RI_S303_ACCENT));
        RI_ASSERT(bank && ri_ctlreg_auto_id(bank) == 0u,
            "PAT Bank has no engine target -- pushing nothing is correct");
        RI_ASSERT(accent && ri_ctlreg_auto_id(accent) == 0u,
            "303A Step/Accent has no engine target either -- also correct");
    }

    RI_RESULT("midi_push");
    return 0;
}
