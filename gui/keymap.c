/* gui/keymap.c — computer-keyboard map (§12.10 G5), Appendix E. */
#include "gui/keymap.h"
#include "gui/sect303.h"

static struct RIKeyAction act(uint8_t kind, uint32_t section, int arg) {
    struct RIKeyAction a;
    a.kind = kind;
    a.section = (uint8_t)section;
    a.arg = (int16_t)arg;
    return a;
}

/* Menu shortcuts, p. 222–223 (letter key -> command). */
static const struct { uint8_t raw, cmd; } RI_KEY_MENU[] = {
    { 0x36, RI_KM_NEW }, { 0x18, RI_KM_OPEN }, { 0x11, RI_KM_CLOSE }, { 0x21, RI_KM_SAVE },
    { 0x17, RI_KM_INFO }, { 0x10, RI_KM_QUIT }, { 0x32, RI_KM_CUT }, { 0x33, RI_KM_COPY },
    { 0x34, RI_KM_PASTE }, { 0x28, RI_KM_TOUCHED_LOOP }, { 0x14, RI_KM_TOUCHED_SONG },
    { 0x26, RI_KM_SHIFT_LEFT }, { 0x27, RI_KM_SHIFT_RIGHT }, { 0x13, RI_KM_RANDOMIZE },
    { 0x15, RI_KM_ALTER }, { 0x16, RI_KM_ALTER_ACCENTS }, { 0x37, RI_KM_SELECT_MOD },
    { 0x23, RI_KM_PROGRAM_SYNTH }, { 0x24, RI_KM_SELECT_PATTERNS }
};

/* Keypad transport, p. 224 (PC column). */
static const struct { uint8_t raw, cmd; } RI_KEY_TRANSPORT[] = {
    { RI_RAW_KP0, RI_KT_STOP }, { RI_RAW_KPENTER, RI_KT_PLAY }, { RI_RAW_SPACE, RI_KT_STOPPLAY },
    { RI_RAW_KPSTAR, RI_KT_RECORD }, { RI_RAW_KP4, RI_KT_FF }, { RI_RAW_KP5, RI_KT_REW },
    { RI_RAW_KP1, RI_KT_LOOP_START }, { RI_RAW_KP2, RI_KT_LOOP_END }, { RI_RAW_KP8, RI_KT_NEXT_BAR },
    { RI_RAW_KP7, RI_KT_PREV_BAR }, { RI_RAW_KPPLUS, RI_KT_TEMPO_UP },
    { RI_RAW_KPMINUS, RI_KT_TEMPO_DOWN }
};

/* Synth programming, p. 225: pitch keys C C# D D# E F F# G G# A A# B C. */
static const uint8_t RI_KEY_PITCH[13] = {
    0x33, 0x23, 0x34, 0x24, 0x35, 0x36, 0x26, 0x37, 0x27, 0x38, 0x28, 0x39, 0x3A
};
static const struct { uint8_t raw, idx; } RI_KEY_SYNTH[] = {
    { RI_RAW_RETURN, RI_S303_STEP }, { RI_RAW_BACKSPACE, RI_S303_BACK },
    { RI_RAW_MINUS, RI_S303_NOTEPAUSE }, { 0x19, RI_S303_ACCENT }, { 0x1A, RI_S303_SLIDE },
    { 0x29, RI_S303_DOWN }, { 0x2A, RI_S303_UP }
};

/* Pattern-key rows, p. 20 / 224: first raw code of keys 1..8 per section. */
static const uint8_t RI_KEY_PATROW[RI_FOCUS_COUNT] = { 0x01, 0x10, 0x20, 0x31 };

/* Drum tap, p. 32 / 226: '-' = AC, A S D F G H J K L ; ' = instruments
 * 1..11 in Instrument Selection order (808: BD SD LT MT HT RS CP CB CY OH
 * CH; 909: BD SD LT MT HT RS CP CH OH CC RC — same index per key). */
static int drum_tap_instrument(uint32_t raw) {
    if (raw == RI_RAW_MINUS)
        return 0;
    if (raw >= 0x20u && raw <= 0x2Au)
        return (int)(raw - 0x20u) + 1;
    return -1;
}

struct RIKeyAction ri_key_decode(uint32_t raw, uint32_t qual, const struct RIKeyOpts *opts,
    uint32_t focus) {
    uint32_t i, code = raw & 0x7Fu;
    int up = (raw & RI_RAW_UPFLAG) != 0, shift = (qual & (RI_QUAL_LSHIFT | RI_QUAL_RSHIFT)) != 0;
    int program = opts && opts->program_synth, patterns = opts && opts->select_patterns;
    int is_synth = focus == RI_FOCUS_SYNTH1 || focus == RI_FOCUS_SYNTH2;
    if (focus >= RI_FOCUS_COUNT)
        return act(RI_KA_NONE, 0, 0);
    if (up) {   /* only a held delete-tap cares about release; never a menu chord's */
        if (program && !(qual & (RI_QUAL_CONTROL | RI_QUAL_RCOMMAND)) && ((is_synth && code == RI_RAW_TAB) || (!is_synth && drum_tap_instrument(code) >= 0)))
            return act(RI_KA_TAP_END, focus, is_synth ? 0 : drum_tap_instrument(code));
        return act(RI_KA_NONE, 0, 0);
    }
    if (qual & (RI_QUAL_CONTROL | RI_QUAL_RCOMMAND)) {
        for (i = 0; i < sizeof(RI_KEY_MENU) / sizeof(RI_KEY_MENU[0]); i++)
            if (RI_KEY_MENU[i].raw == code)
                return act(RI_KA_MENU, 0, RI_KEY_MENU[i].cmd);
        return act(RI_KA_NONE, 0, 0);
    }
    for (i = 0; i < sizeof(RI_KEY_TRANSPORT) / sizeof(RI_KEY_TRANSPORT[0]); i++)
        if (RI_KEY_TRANSPORT[i].raw == code)
            return act(RI_KA_TRANSPORT, 0, RI_KEY_TRANSPORT[i].cmd);
    if (code == RI_RAW_UP || code == RI_RAW_DOWN)
        return act(RI_KA_FOCUS, 0, code == RI_RAW_UP ? -1 : 1);
    if (program) {
        if (is_synth) {
            if (code == RI_RAW_TAB)
                return act(shift ? RI_KA_TAP_DELETE : RI_KA_TAP, focus, 0);
            for (i = 0; i < 13u; i++)
                if (RI_KEY_PITCH[i] == code)
                    return act(RI_KA_SYNTH, focus, (int)(RI_S303_KEY0 + i));
            for (i = 0; i < sizeof(RI_KEY_SYNTH) / sizeof(RI_KEY_SYNTH[0]); i++)
                if (RI_KEY_SYNTH[i].raw == code)
                    return act(RI_KA_SYNTH, focus, RI_KEY_SYNTH[i].idx);
        } else if (drum_tap_instrument(code) >= 0) {
            return act(shift ? RI_KA_TAP_DELETE : RI_KA_TAP, focus, drum_tap_instrument(code));
        }
    }
    if (patterns)
        for (i = 0; i < RI_FOCUS_COUNT; i++)
            if (code >= RI_KEY_PATROW[i] && code < RI_KEY_PATROW[i] + 8u)
                return act(RI_KA_PATTERN, i, (int)(code - RI_KEY_PATROW[i]));
    return act(RI_KA_NONE, 0, 0);
}
