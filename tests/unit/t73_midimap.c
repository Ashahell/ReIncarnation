/* t73_midimap — Remote MIDI Control, Standard Mapping (§12.10 G7):
 * ReBirth manual p. 127–134, 144–145, Appendix C p. 195–204. Note
 * numbers below are typed from the manual's tables, independently of
 * gui/midimap.c. */
#include <stdio.h>
#include "tests/helpers/ri_assert.h"
#include "gui/midimap.h"
#include "gui/ctlreg.h"

static struct RISectUI s1, s2, d8, d9, mx[5], fx[4], pat[4], tr;
static struct RIPanelUI pu;
static struct RIMidiIn mi;

static void setup(void) {
    uint32_t i;
    ri_sui_init(&s1, RI_SEC_SYNTH1);
    ri_sui_init(&s2, RI_SEC_SYNTH2);
    ri_sui_init(&d8, RI_SEC_808);
    ri_sui_init(&d9, RI_SEC_909);
    for (i = 0; i < 5; i++) {
        ri_sui_init(&mx[i], (uint8_t)(i < 4 ? RI_SEC_MIX_SYNTH1 + i : RI_SEC_MASTER));
        if (i)
            ri_sui_bind_board(&mx[i], mx[0].u.mix.board);
    }
    for (i = 0; i < 4; i++) {
        ri_sui_init(&fx[i], (uint8_t)(RI_SEC_PCF + i));
        ri_sui_init(&pat[i], (uint8_t)(RI_SEC_PAT_SYNTH1 + i));
    }
    ri_sui_init(&tr, RI_SEC_TRANSPORT);
    ri_panel_init(&pu);
    pu.synth[0] = &s1; pu.synth[1] = &s2; pu.drum[0] = &d8; pu.drum[1] = &d9; pu.tr = &tr;
    for (i = 0; i < 5; i++)
        pu.mix[i] = &mx[i];
    for (i = 0; i < 4; i++) {
        pu.fx[i] = &fx[i];
        pu.pat[i] = &pat[i];
    }
    ri_midi_init(&mi, 0);
}

static int on(uint8_t n) { return ri_midi_msg(&mi, &pu, 0x90, n, 100); }

int main(void) {
    uint32_t i, ncc = 0;
    setup();
    /* every Appendix C controller reaches its control: 127 = max / on, 0 = min / off */
    for (i = 0; i < ri_ctlreg_count(); i++) {
        const struct RICtlDef *d = ri_ctlreg_at(i);
        struct RISectUI *u;
        uint32_t idx;
        if (!d || d->midi_cc == RI_MIDI_CC_NONE)
            continue;
        ncc++;
        u = d->section == RI_SEC_SYNTH1 ? &s1 : d->section == RI_SEC_SYNTH2 ? &s2 : d->section == RI_SEC_808 ? &d8
          : d->section == RI_SEC_909 ? &d9 : d->section == RI_SEC_MASTER ? &mx[4]
          : d->section >= RI_SEC_MIX_SYNTH1 && d->section <= RI_SEC_MIX_909 ? &mx[d->section - RI_SEC_MIX_SYNTH1]
          : d->section >= RI_SEC_PCF && d->section <= RI_SEC_COMP ? &fx[d->section - RI_SEC_PCF] : &tr;
        idx = d->reg_id & 0xFFu;
        ri_midi_msg(&mi, &pu, 0xB0, d->midi_cc, 127);
        if (d->kind == RI_CK_SWITCH)
            RI_ASSERT(ri_sui_value(u, idx) == 1, "CC %u %s/%s on", d->midi_cc, d->group, d->legend);
        else
            RI_ASSERT(ri_sui_value(u, idx) == d->max_v, "CC %u %s/%s -> max %d (got %d)", d->midi_cc, d->group,
                d->legend, d->max_v, ri_sui_value(u, idx));
        ri_midi_msg(&mi, &pu, 0xB0, d->midi_cc, 0);
        if (d->kind == RI_CK_SWITCH)
            RI_ASSERT(ri_sui_value(u, idx) == 0, "CC %u off", d->midi_cc);
        else
            RI_ASSERT(ri_sui_value(u, idx) == d->min_v, "CC %u -> min", d->midi_cc);
    }
    RI_ASSERT(ncc == 99u, "Appendix C controllers %u", ncc);
    /* CC value laws (E0): selectors split 0..127 evenly; knobs round */
    ri_midi_msg(&mi, &pu, 0xB0, 64, 63);          /* 808 Instrument Selection: 12 positions */
    RI_ASSERT(ri_sui_value(&d8, RI_S808_SELECT) == 5, "63 -> position 5 of 12");
    ri_midi_msg(&mi, &pu, 0xB0, 25, 64);          /* Synth 1 Cutoff 0..127 */
    RI_ASSERT(ri_sui_value(&s1, 2) == 64, "cutoff 64");
    /* p. 199 Various Switches (every mode) */
    RI_ASSERT(on(67) == 1 && pu.focus == RI_FOCUS_808 && on(68) == 1 && pu.focus == RI_FOCUS_909, "focus notes");
    RI_ASSERT(on(96) == 1 && pu.opts.select_patterns && on(95) == 1 && pu.opts.program_synth, "option notes");
    RI_ASSERT(on(64) == 1 && !pu.opts.select_patterns && pu.opts.program_synth, "64 swaps: program only");
    RI_ASSERT(on(64) == 1 && pu.opts.select_patterns && !pu.opts.program_synth, "64 swaps back: patterns only");
    RI_ASSERT(on(69) == 1 && ri_sui_led(&tr, RI_STR_PLAY, 0), "A3 69 Play");
    RI_ASSERT(on(70) == 1 && !ri_sui_led(&tr, RI_STR_PLAY, 0), "A#3 70 Stop (keys column, E0)");
    ri_sui_press(&tr, RI_STR_MODE);
    RI_ASSERT(on(71) == 1 && ri_sui_led(&tr, RI_STR_RECORD, 0), "B3 71 Record");
    RI_ASSERT(on(73) == 1 && ri_sui_value(&tr, RI_STR_BAR) == 2 && on(72) == 1 && ri_sui_value(&tr, RI_STR_BAR) == 1,
        "bar + / -");
    RI_ASSERT(on(74) == 1 && ri_sui_led(&fx[0], RI_SFX_ONOFF, 0) && on(77) == 1 && ri_sui_led(&fx[3], RI_SFX_ONOFF, 0),
        "PCF / Comp enable");
    RI_ASSERT(on(80) == 1 && ri_sui_led(&mx[0], RI_SMIX_PCF, 0), "G#4 80 Synth 1 PCF");
    RI_ASSERT(on(88) == 1 && ri_sui_led(&mx[2], RI_SMIX_PCF, 0) && !ri_sui_led(&mx[0], RI_SMIX_PCF, 0),
        "E5 88 808 PCF steals it");
    RI_ASSERT(on(90) == 1 && !ri_sui_led(&mx[3], RI_SMIX_ONOFF, 0), "F#5 90 909 Mix (mute)");
    RI_ASSERT(on(94) == 1 && ri_sui_led(&mx[4], RI_SMST_COMP, 0), "A#5 94 Master Comp");
    /* p. 200–201 Pattern Selection (patterns on, program off) */
    RI_ASSERT(on(19) == 1 && ri_spat_selected(&pat[0].u.pat) == 2 && pu.focus == RI_FOCUS_SYNTH1, "G-1 19 Synth 1 P3");
    RI_ASSERT(on(28) == 1 && on(32) == 1 && ri_spat_selected(&pat[1].u.pat) == 18, "Synth 2 bank C, P3 = C3");
    RI_ASSERT(on(38) == 1 && ri_sui_led(&pat[2], RI_SPAT_OFF, 0) == 0, "D1 38 808 on/off");
    RI_ASSERT(on(63) == 1 && ri_spat_selected(&pat[3].u.pat) == 7 && pu.focus == RI_FOCUS_909, "D#3 63 909 P8");
    /* p. 202–204 section switches (program on), and the E0 overlap rule */
    on(95);
    on(65);
    RI_ASSERT(on(31) == 1 && ri_sui_display(&s1, RI_S303_DISPLAY) == 2, "G0 31 Step (focus Synth 1)");
    RI_ASSERT(on(27) == 1 && ri_sui_led(&s1, RI_S303_ACCENT, 0), "D#0 27 Accent");
    RI_ASSERT(on(19) == 1 && ri_spat_selected(&pat[0].u.pat) == 2, "19 = pitch G, not pattern (focused section wins)");
    on(67);
    RI_ASSERT(on(29) == 1 && ri_sui_value(&d8, RI_S808_SELECT) == 1, "F0 29 808 instrument BD");
    RI_ASSERT(on(12) == 1 && ri_pdrum_get(&d8.u.s808.pat, 0, RI_L808_BD) != RI_HIT_OFF, "C-1 12 step 1 on");
    RI_ASSERT(on(40) == 1 && ri_spat_selected(&pat[2].u.pat) == 0 && ri_sui_value(&pat[2], RI_SPAT_BANK) == 1,
        "40 is past the 808 switches: 808 bank B (pattern table)");
    /* channel, velocity 0, running status, SysEx, realtime interleave */
    ri_sui_press(&tr, RI_STR_STOP);
    ri_midi_elapse(&mi, &pu, 1000);
    i = mi.ignored;
    RI_ASSERT(ri_midi_msg(&mi, &pu, 0x91, 69, 100) == 1 && mi.ignored == i + 1u && !ri_sui_led(&tr, RI_STR_PLAY, 0) &&
        ri_sui_value(&tr, RI_STR_MIDI) == 1, "channel 2 Play ignored; only the MIDI LED lights (p. 134, 144)");
    RI_ASSERT(ri_midi_msg(&mi, &pu, 0x90, 69, 0) == 0, "velocity 0 = note off");
    {
        static const uint8_t bytes[] = { 0xB0, 25, 10, 0xF8, 26, 20, 0xF0, 0x41, 0x10, 0xF7, 27, 30 };
        for (i = 0; i < sizeof bytes; i++)
            ri_midi_byte(&mi, &pu, bytes[i]);
        RI_ASSERT(ri_sui_value(&s1, 2) == 10 && ri_sui_value(&s1, 3) == 20 && ri_sui_value(&s1, 4) == 0,
            "running status across realtime; SysEx cancels running status");
    }
    /* LEDs: MIDI LED lights and times out; Sync red on the downbeat, green on other beats */
    ri_midi_elapse(&mi, &pu, 1000);
    RI_ASSERT(ri_sui_value(&tr, RI_STR_MIDI) == 0, "MIDI LED off after timeout");
    RI_ASSERT(on(66) == 1 && ri_sui_value(&tr, RI_STR_MIDI) == 1, "MIDI LED on a message");
    RI_ASSERT(ri_midi_elapse(&mi, &pu, 50) == 0 && ri_midi_elapse(&mi, &pu, 60) == 1, "100 ms");
    ri_midi_msg(&mi, &pu, 0xFA, 0, 0);
    ri_midi_msg(&mi, &pu, 0xF8, 0, 0);
    RI_ASSERT(ri_sui_value(&tr, RI_STR_SYNC) == 1, "downbeat red");
    for (i = 1; i < 24; i++)
        ri_midi_msg(&mi, &pu, 0xF8, 0, 0);
    RI_ASSERT(ri_sui_value(&tr, RI_STR_SYNC) == 0, "off after the first quarter of the beat");
    ri_midi_msg(&mi, &pu, 0xF8, 0, 0);
    RI_ASSERT(ri_sui_value(&tr, RI_STR_SYNC) == 2, "beat 2 green");
    for (i = 25; i < 96; i++)
        ri_midi_msg(&mi, &pu, 0xF8, 0, 0);
    ri_midi_msg(&mi, &pu, 0xF8, 0, 0);
    RI_ASSERT(ri_sui_value(&tr, RI_STR_SYNC) == 1, "next bar red");
    ri_midi_msg(&mi, &pu, 0xF0, 0, 0);
    RI_ASSERT(ri_midi_msg(0, &pu, 0x90, 60, 1) == 0 && ri_midi_msg(&mi, 0, 0x90, 60, 1) == 0, "null");
    RI_RESULT("midimap");
}
