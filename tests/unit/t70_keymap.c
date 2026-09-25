/* t70_keymap — computer-keyboard map (§12.10 G5), ReBirth manual
 * Appendix E p. 221–226 + p. 20, 22, 32, 43. Expected keys are typed from
 * the manual's tables as US-layout raw positions, independently of the
 * decoder's tables. */
#include <stdio.h>
#include "tests/helpers/ri_assert.h"
#include "gui/keymap.h"
#include "gui/sect303.h"

static struct RIKeyAction k(uint32_t raw, uint32_t qual, int pat, int prog, uint32_t focus) {
    struct RIKeyOpts o;
    o.select_patterns = (uint8_t)pat;
    o.program_synth = (uint8_t)prog;
    return ri_key_decode(raw, qual, &o, focus);
}

int main(void) {
    struct RIKeyAction a;
    uint32_t i, f;
    /* p. 224 pattern rows, US positions: 1-8 / Q W E R T Y U I / A S D F G H J K / Z X C V B N M , */
    static const uint8_t row[4][8] = {
        { 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08 },
        { 0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17 },
        { 0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27 },
        { 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38 } };
    for (f = 0; f < 4; f++)
        for (i = 0; i < 8; i++) {
            a = k(row[f][i], 0, 1, 0, RI_FOCUS_SYNTH1);
            RI_ASSERT(a.kind == RI_KA_PATTERN && a.section == f && a.arg == (int)i, "pattern key %u/%u", f, i);
            a = k(row[f][i], 0, 0, 0, RI_FOCUS_SYNTH1);
            RI_ASSERT(a.kind == RI_KA_NONE, "pattern keys need the Options toggle (p. 20)");
        }
    RI_ASSERT(k(0x09, 0, 1, 0, 0).kind == RI_KA_NONE && k(0x18, 0, 1, 0, 0).kind == RI_KA_NONE,
        "9 / O are not pattern keys");
    /* p. 224 transport keypad (PC column) */
    {
        static const uint8_t raw[12] = { 0x0F, 0x43, 0x40, 0x5D, 0x2D, 0x2E, 0x1D, 0x1E, 0x3E, 0x3D, 0x5E, 0x4A };
        static const int cmd[12] = { RI_KT_STOP, RI_KT_PLAY, RI_KT_STOPPLAY, RI_KT_RECORD, RI_KT_FF,
            RI_KT_REW, RI_KT_LOOP_START, RI_KT_LOOP_END, RI_KT_NEXT_BAR, RI_KT_PREV_BAR,
            RI_KT_TEMPO_UP, RI_KT_TEMPO_DOWN };
        for (i = 0; i < 12; i++) {
            a = k(raw[i], 0, 0, 0, RI_FOCUS_808);
            RI_ASSERT(a.kind == RI_KA_TRANSPORT && a.arg == cmd[i], "transport %u", i);
        }
    }
    /* p. 22 focus arrows */
    RI_ASSERT(k(0x4C, 0, 0, 0, 2).kind == RI_KA_FOCUS && k(0x4C, 0, 0, 0, 2).arg == -1, "up");
    RI_ASSERT(k(0x4D, 0, 0, 0, 2).arg == 1, "down");
    /* p. 225 synth programming: C C# D D# E F F# G G# A A# B C = C F V G B N J M K , L . / */
    {
        static const uint8_t pitch[13] = { 0x33, 0x23, 0x34, 0x24, 0x35, 0x36, 0x26, 0x37, 0x27, 0x38, 0x28, 0x39, 0x3A };
        for (i = 0; i < 13; i++) {
            a = k(pitch[i], 0, 0, 1, RI_FOCUS_SYNTH2);
            RI_ASSERT(a.kind == RI_KA_SYNTH && a.section == RI_FOCUS_SYNTH2 && a.arg == (int)(RI_S303_KEY0 + i),
                "pitch %u", i);
        }
        RI_ASSERT(k(0x44, 0, 0, 1, 0).arg == RI_S303_STEP && k(0x41, 0, 0, 1, 0).arg == RI_S303_BACK, "step/back");
        RI_ASSERT(k(0x0B, 0, 0, 1, 0).arg == RI_S303_NOTEPAUSE && k(0x19, 0, 0, 1, 0).arg == RI_S303_ACCENT &&
            k(0x1A, 0, 0, 1, 0).arg == RI_S303_SLIDE, "note/pause, accent (P), slide ([)");
        RI_ASSERT(k(0x29, 0, 0, 1, 0).arg == RI_S303_DOWN && k(0x2A, 0, 0, 1, 0).arg == RI_S303_UP,
            "octave down (;) / up (')");
        RI_ASSERT(k(0x33, 0, 0, 0, 0).kind == RI_KA_NONE, "synth keys need the Options toggle (p. 43)");
        a = k(0x42, 0, 0, 1, RI_FOCUS_SYNTH1);
        RI_ASSERT(a.kind == RI_KA_TAP && a.section == RI_FOCUS_SYNTH1, "Tab taps a synth note (p. 44)");
        RI_ASSERT(k(0x42, RI_QUAL_LSHIFT, 0, 1, 0).kind == RI_KA_TAP_DELETE, "Shift+Tab deletes (p. 44)");
        RI_ASSERT(k(0x42 | 0x80, RI_QUAL_LSHIFT, 0, 1, 0).kind == RI_KA_TAP_END, "release ends delete");
    }
    /* p. 32 / 226 drum tap: - A S D F G H J K L ; ' -> AC, then instruments 1..11 */
    {
        static const uint8_t tap[12] = { 0x0B, 0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28, 0x29, 0x2A };
        for (f = RI_FOCUS_808; f <= RI_FOCUS_909; f++)
            for (i = 0; i < 12; i++) {
                a = k(tap[i], 0, 1, 1, f);
                RI_ASSERT(a.kind == RI_KA_TAP && a.section == f && a.arg == (int)i, "tap %u/%u", f, i);
            }
        RI_ASSERT(k(0x20, RI_QUAL_RSHIFT, 0, 1, RI_FOCUS_909).kind == RI_KA_TAP_DELETE, "Shift deletes drum notes");
        /* found on riqemu1: the key-up of Ctrl+F (F = MT tap key) decoded TAP_END */
        RI_ASSERT(k(0x23 | 0x80, RI_QUAL_CONTROL, 1, 1, RI_FOCUS_808).kind == RI_KA_NONE, "menu chord release");
    }
    /* Precedence (E0): the focused section's programming keys beat pattern keys. */
    RI_ASSERT(k(0x33, 0, 1, 1, RI_FOCUS_SYNTH1).kind == RI_KA_SYNTH, "C: synth pitch over 909 pattern 3");
    RI_ASSERT(k(0x33, 0, 1, 1, RI_FOCUS_808).kind == RI_KA_PATTERN, "C with drum focus: 909 pattern 3");
    RI_ASSERT(k(0x21, 0, 1, 1, RI_FOCUS_808).kind == RI_KA_TAP, "S with 808 focus: tap SD");
    RI_ASSERT(k(0x21, 0, 1, 0, RI_FOCUS_808).kind == RI_KA_PATTERN, "S without programming: 808 pattern 2");
    /* p. 222–223 menu shortcuts: [Ctrl] (PC) or Right-Amiga */
    {
        static const uint8_t raw[19] = { 0x36, 0x18, 0x11, 0x21, 0x17, 0x10, 0x32, 0x33, 0x34, 0x28, 0x14,
            0x26, 0x27, 0x13, 0x15, 0x16, 0x37, 0x23, 0x24 };
        for (i = 0; i < 19; i++) {
            a = k(raw[i], RI_QUAL_CONTROL, 1, 1, 0);
            RI_ASSERT(a.kind == RI_KA_MENU && a.arg == (int)i, "menu %u", i);
            RI_ASSERT(k(raw[i], RI_QUAL_RCOMMAND, 0, 0, 0).arg == (int)i, "amiga menu %u", i);
        }
        RI_ASSERT(k(0x01, RI_QUAL_CONTROL, 1, 1, 0).kind == RI_KA_NONE, "ctrl-1 is nothing");
    }
    RI_ASSERT(k(0x10 | 0x80, 0, 1, 1, 0).kind == RI_KA_NONE, "key-up of a pattern key");
    RI_ASSERT(k(0x10, 0, 1, 1, 4).kind == RI_KA_NONE && ri_key_decode(0x10, 0, 0, 0).kind == RI_KA_NONE,
        "bad focus / no options");
    RI_RESULT("keymap");
}
