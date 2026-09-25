/* gui/keymap.h — computer-keyboard map (§12.10 G5). Pure C, host-tested.
 * ReBirth 2.0.1 Owner's Manual Appendix E (p. 221–225) + p. 18–22, 30–33,
 * 43–44. Keys are PHYSICAL positions ("The keys are on absolute positions
 * on the keyboard, not on certain character positions", p. 224), so the
 * map is keyed by Amiga raw key codes, which are positional by design.
 * PC column of Appendix E (AROS runs on PC hardware): Fast Forward =
 * keypad 4, Rewind = keypad 5; menu shortcuts take [Ctrl] as in ReBirth
 * for Windows, and also Right-Amiga (the AROS menu-shortcut key).
 * Decode only: applying an action belongs to gui/panelui.h.
 */
#ifndef RI_KEYMAP_H
#define RI_KEYMAP_H
#include <stdint.h>

/* Amiga raw key codes used here (positional; key-up = code | 0x80). */
#define RI_RAW_MINUS 0x0Bu     /* the key right of 0 on the top row */
#define RI_RAW_KP0 0x0Fu
#define RI_RAW_KP1 0x1Du
#define RI_RAW_KP2 0x1Eu
#define RI_RAW_KP4 0x2Du
#define RI_RAW_KP5 0x2Eu
#define RI_RAW_KP7 0x3Du
#define RI_RAW_KP8 0x3Eu
#define RI_RAW_SPACE 0x40u
#define RI_RAW_BACKSPACE 0x41u
#define RI_RAW_TAB 0x42u
#define RI_RAW_KPENTER 0x43u
#define RI_RAW_RETURN 0x44u
#define RI_RAW_KPMINUS 0x4Au
#define RI_RAW_UP 0x4Cu
#define RI_RAW_DOWN 0x4Du
#define RI_RAW_KPSTAR 0x5Du
#define RI_RAW_KPPLUS 0x5Eu
#define RI_RAW_UPFLAG 0x80u

/* Qualifier bits (devices/inputevent.h values, repeated so the decoder
 * stays pure C and host-testable). */
#define RI_QUAL_LSHIFT 0x0001u
#define RI_QUAL_RSHIFT 0x0002u
#define RI_QUAL_CONTROL 0x0008u
#define RI_QUAL_LALT 0x0010u
#define RI_QUAL_RALT 0x0020u
#define RI_QUAL_RCOMMAND 0x0080u
#define RI_QUAL_REPEAT 0x0200u

/* Focus: the three (2.0: four) main sections, window order (p. 22). */
#define RI_FOCUS_SYNTH1 0u
#define RI_FOCUS_SYNTH2 1u
#define RI_FOCUS_808 2u
#define RI_FOCUS_909 3u
#define RI_FOCUS_COUNT 4u

enum RIKeyKind {
    RI_KA_NONE = 0,
    RI_KA_FOCUS,        /* arg: -1 up, +1 down (p. 22) */
    RI_KA_PATTERN,      /* section = focus index, arg = pattern 0..7 (p. 224) */
    RI_KA_TRANSPORT,    /* arg = RI_KT_* (p. 224) */
    RI_KA_SYNTH,        /* arg = RI_S303_* index to press on the focused synth (p. 225) */
    RI_KA_TAP,          /* section = focus; synth: arg 0 (Tab); drums: arg = instrument 0..11 */
    RI_KA_TAP_DELETE,   /* Shift+Tab (synth) / Shift+key (drums) held; key-up ends it */
    RI_KA_TAP_END,      /* key-up of a delete-tap */
    RI_KA_MENU          /* arg = RI_KM_* (p. 222–223) */
};

/* Transport commands (keypad, p. 224). */
enum { RI_KT_STOP = 0, RI_KT_PLAY, RI_KT_STOPPLAY, RI_KT_RECORD, RI_KT_FF, RI_KT_REW,
       RI_KT_LOOP_START, RI_KT_LOOP_END, RI_KT_NEXT_BAR, RI_KT_PREV_BAR,
       RI_KT_TEMPO_UP, RI_KT_TEMPO_DOWN, RI_KT_COUNT };

/* Menu shortcuts (p. 222–223). */
enum { RI_KM_NEW = 0, RI_KM_OPEN, RI_KM_CLOSE, RI_KM_SAVE, RI_KM_INFO, RI_KM_QUIT,
       RI_KM_CUT, RI_KM_COPY, RI_KM_PASTE, RI_KM_TOUCHED_LOOP, RI_KM_TOUCHED_SONG,
       RI_KM_SHIFT_LEFT, RI_KM_SHIFT_RIGHT, RI_KM_RANDOMIZE, RI_KM_ALTER, RI_KM_ALTER_ACCENTS,
       RI_KM_SELECT_MOD, RI_KM_PROGRAM_SYNTH, RI_KM_SELECT_PATTERNS, RI_KM_COUNT };

struct RIKeyAction {
    uint8_t kind;       /* RI_KA_* */
    uint8_t section;    /* focus index for PATTERN / SYNTH / TAP */
    int16_t arg;
};

/* Options-menu state that gates the typewriter keys (p. 18–20, 43). */
struct RIKeyOpts {
    uint8_t select_patterns;   /* "Select Patterns from Keyboard" */
    uint8_t program_synth;     /* "Program Synth from Keyboard" (also drum tap) */
};

/* Decode one raw key event. focus = RI_FOCUS_* of the section that has
 * the focus. Precedence (E0, ledger docs/evidence/gui/keyboard.md):
 * menu shortcut > keypad transport > focus arrows > programming keys of
 * the focused section (when program_synth is on) > pattern keys (when
 * select_patterns is on). Unmapped keys and key-ups decode to NONE,
 * except the key-up that ends a delete-tap. */
struct RIKeyAction ri_key_decode(uint32_t raw, uint32_t qual, const struct RIKeyOpts *opts,
    uint32_t focus);
#endif
