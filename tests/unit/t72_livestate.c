/* t72_livestate — live state from the audio clock (§12.10 G6a): position
 * law, per-section playheads, meters, tap recording (manual p. 32–33,
 * 43–44, 147), Song-mode bar follow. */
#include <stdio.h>
#include "tests/helpers/ri_assert.h"
#include "gui/livestate.h"
#include "gui/panelui.h"
#include "gui/ctlreg.h"
#include "engine/seq/sched.h"

int main(void) {
    static struct RISectUI s1, d8, d9, p[4], tr;
    struct RIPanelUI pu;
    uint32_t i;
    /* position law: integer-exact 16ths from samples */
    RI_ASSERT(ri_live_16ths(5999u, 120u, 48000u) == 0u && ri_live_16ths(6000u, 120u, 48000u) == 1u,
        "120 bpm: 6000 samples per 16th");
    RI_ASSERT(ri_live_16ths(5625u * 7u, 128u, 48000u) == 7u && ri_live_16ths(5625u * 7u - 1u, 128u, 48000u) == 6u,
        "128 bpm: 5625 samples per 16th, floor");
    RI_ASSERT(ri_live_16ths(48000u * 3600u, 500u, 48000u) == 120000u, "an hour at 500 bpm");
    RI_ASSERT(ri_live_16ths(1000u, 0u, 48000u) == 0u && ri_live_16ths(1000u, 120u, 0u) == 0u, "bad args");
    RI_ASSERT(ri_live_step(13u, 12u) == 1u && ri_live_step(13u, 16u) == 13u && ri_live_step(5u, 0u) == 5u,
        "independent loop lengths (p. 147)");
    RI_ASSERT(ri_live_bar(4u, 47u) == 6u, "bars follow 16 sixteenths");
    /* meters: -36 dBFS .. 0 dBFS -> 0..127, clip at 1.0 */
    RI_ASSERT(ri_live_meter_level(1.0f) == 127 && ri_live_meter_level(2.0f) == 127, "0 dBFS / over = full");
    RI_ASSERT(ri_live_meter_level(0.5f) == 102, "-6 dB lands on the -7 dB rung: %d", ri_live_meter_level(0.5f));
    RI_ASSERT(ri_live_meter_level(0.0158489f) == 0 && ri_live_meter_level(0.01f) == 0 &&
        ri_live_meter_level(0.0f) == 0, "floor -36 dB");
    RI_ASSERT(ri_live_meter_level(0.1f) == 56, "-20 dB = 16 rungs above -36: %d", ri_live_meter_level(0.1f));
    RI_ASSERT(ri_live_gr_level(0.0f) == 0 && ri_live_gr_level(-10.0f) == 64 && ri_live_gr_level(-40.0f) == 127 &&
        ri_live_gr_level(3.0f) == 0, "gain reduction scale");
    /* panel live feed */
    ri_sui_init(&s1, RI_SEC_SYNTH1);
    ri_sui_init(&d8, RI_SEC_808);
    ri_sui_init(&d9, RI_SEC_909);
    for (i = 0; i < 4; i++)
        ri_sui_init(&p[i], (uint8_t)(RI_SEC_PAT_SYNTH1 + i));
    ri_sui_init(&tr, RI_SEC_TRANSPORT);
    ri_panel_init(&pu);
    pu.synth[0] = &s1; pu.drum[0] = &d8; pu.drum[1] = &d9; pu.tr = &tr;
    for (i = 0; i < 4; i++)
        pu.pat[i] = &p[i];
    pu.opts.program_synth = 1;
    ri_sui_set(&p[2], RI_SPAT_LENGTH, 12);            /* 808 pattern: 12 steps */
    RI_ASSERT(pu.playhead[2] == -1, "stopped: no playhead");
    pu.focus = RI_FOCUS_808;
    RI_ASSERT(ri_panel_key(&pu, 0x20, 0) == 0 && ri_pdrum_get(&d8.u.s808.pat, 0, RI_L808_BD) == RI_HIT_OFF,
        "tap ignored while stopped (p. 32: with playback activated)");
    RI_ASSERT(ri_panel_live(&pu, 1, 13u) == 1 && pu.playhead[2] == 1 && pu.playhead[3] == 13 && pu.playhead[0] == 13,
        "each section loops its own length");
    RI_ASSERT(ri_panel_live(&pu, 1, 13u) == 0, "same position: no change");
    /* tap A = BD at the 808 playhead (step 1); again = no change (only adds) */
    RI_ASSERT(ri_panel_key(&pu, 0x20, 0) == 1 && ri_pdrum_get(&d8.u.s808.pat, 1, RI_L808_BD) == RI_HIT_LOW, "tap BD");
    RI_ASSERT(ri_panel_key(&pu, 0x20, 0) == 0, "tapping only adds");
    RI_ASSERT(ri_panel_key(&pu, 0x0B, 0) == 1 && (d8.u.s808.pat.row.drum[1].flags & RI_DRUM_AC), "tap - = AC");
    /* 909: tap K = CH at step 13 sets low, never lowers a high hit */
    pu.focus = RI_FOCUS_909;
    ri_pdrum_set(&d9.u.s909.pat, 13, RI_L909_BD, RI_HIT_HIGH);
    RI_ASSERT(ri_panel_key(&pu, 0x27, 0) == 1 && ri_pdrum_get(&d9.u.s909.pat, 13, RI_L909_CH) == RI_HIT_LOW, "tap CH");
    RI_ASSERT(ri_panel_key(&pu, 0x20, 0) == 0 && ri_pdrum_get(&d9.u.s909.pat, 13, RI_L909_BD) == RI_HIT_HIGH,
        "a tap never lowers a high hit");
    /* held delete: Shift+A on the 808 deletes BD at every step reached while held */
    pu.focus = RI_FOCUS_808;
    for (i = 0; i < 12; i++)
        ri_pdrum_set(&d8.u.s808.pat, i, RI_L808_BD, RI_HIT_LOW);
    RI_ASSERT(ri_panel_key(&pu, 0x20, RI_QUAL_LSHIFT) == 1 && ri_pdrum_get(&d8.u.s808.pat, 1, RI_L808_BD) == RI_HIT_OFF,
        "delete at the playhead");
    ri_panel_live(&pu, 1, 14u);
    ri_panel_live(&pu, 1, 15u);
    RI_ASSERT(ri_pdrum_get(&d8.u.s808.pat, 2, RI_L808_BD) == RI_HIT_OFF &&
        ri_pdrum_get(&d8.u.s808.pat, 3, RI_L808_BD) == RI_HIT_OFF, "held delete follows the playhead");
    RI_ASSERT(ri_panel_key(&pu, 0x20 | 0x80, RI_QUAL_LSHIFT) == 0 && !pu.del_held, "release ends delete");
    ri_panel_live(&pu, 1, 16u);
    RI_ASSERT(ri_pdrum_get(&d8.u.s808.pat, 4, RI_L808_BD) == RI_HIT_LOW, "after release: kept");
    /* synth: Tab adds a note, Shift+Tab rests (p. 44) */
    pu.focus = RI_FOCUS_SYNTH1;
    s1.u.s303.pat.row.r303[0].flags = RI_STEP_REST;
    RI_ASSERT(ri_panel_key(&pu, 0x42, 0) == 1 && !(s1.u.s303.pat.row.r303[0].flags & RI_STEP_REST), "Tab = note");
    RI_ASSERT(ri_panel_key(&pu, 0x42, RI_QUAL_LSHIFT) == 1 && (s1.u.s303.pat.row.r303[0].flags & RI_STEP_REST),
        "Shift+Tab = pause");
    ri_panel_key(&pu, 0x42 | 0x80, RI_QUAL_LSHIFT);
    /* Song-mode bar follow, with loop wrap when started inside the loop */
    ri_panel_live(&pu, 0, 0u);
    ri_sui_press(&tr, RI_STR_MODE);
    ri_sui_press(&tr, RI_STR_PLAY);
    RI_ASSERT(ri_panel_live(&pu, 1, 0u) == 1 && ri_sui_value(&tr, RI_STR_BAR) == 1, "play from bar 1");
    ri_panel_live(&pu, 1, 40u);
    RI_ASSERT(ri_sui_value(&tr, RI_STR_BAR) == 3, "40 sixteenths -> bar 3: %d", ri_sui_value(&tr, RI_STR_BAR));
    ri_sui_press(&tr, RI_STR_STOP);
    ri_panel_live(&pu, 0, 0u);
    ri_sui_press(&tr, RI_STR_LOOP);
    ri_sui_set(&tr, RI_STR_LOOP_START, 3);
    ri_sui_set(&tr, RI_STR_LOOP_LEN, 2);                 /* loop bars 3-4 */
    ri_sui_press(&tr, RI_STR_PLAY);
    ri_panel_live(&pu, 1, 0u);                            /* starts at bar 3 (held) */
    ri_panel_live(&pu, 1, 16u * 5u);                      /* 5 bars later: 3,4,3,4,3 -> 4 */
    RI_ASSERT(ri_sui_value(&tr, RI_STR_BAR) == 4, "loop wrap: %d", ri_sui_value(&tr, RI_STR_BAR));
    RI_ASSERT(ri_panel_live(&pu, 0, 0u) == 1 && pu.playhead[0] == -1, "stop clears playheads");
    RI_ASSERT(ri_panel_live(0, 1, 1u) == 0, "null");
    RI_RESULT("livestate");
}
