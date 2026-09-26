/* ri_pal_input.h — normalised input events (portability plan T3, §3.4).
 * C99, includes only <stdint.h> (plan §2 gate).
 * RI_KEY_* is positional, matching Amiga raw codes (keymap unchanged);
 * each backend maps native scancodes via one table (AROS identity,
 * Windows set-1, SDL). Key-up on AROS = code|0x80 (handled backend-side).
 */
#ifndef RI_PAL_INPUT_H
#define RI_PAL_INPUT_H
#include <stdint.h>

enum ri_ev_kind {
    RI_IN_MOUSE_DOWN = 1,
    RI_IN_MOUSE_UP = 2,
    RI_IN_MOUSE_MOVE = 3,
    RI_IN_WHEEL = 4,
    RI_IN_KEY_DOWN = 5,
    RI_IN_KEY_UP = 6,
    RI_IN_TICK = 7
};

struct ri_input {
    uint8_t kind;    /* enum ri_ev_kind */
    uint8_t button;  /* mouse button (0 left, 1 right, 2 middle) */
    uint16_t key;    /* RI_KEY_* positional code */
    uint16_t qual;   /* qualifier mask (backend-defined bits) */
    int16_t x, y;    /* pointer position (canvas coords) */
    int16_t dx, dy;  /* motion delta / wheel delta */
};

/* Backend maps a native scancode to RI_KEY_*. Returns 0 if unmapped. */
uint16_t ri_pal_key_map(uint16_t native);

/* RI_KEY_* — positional codes matching Amiga raw codes (keymap unchanged;
 * key-up = code|0x80 backend-side). Aliases pin the Appendix-E set. */
#define RI_KEY_GRAVE 0x00u
#define RI_KEY_1 0x01u
#define RI_KEY_2 0x02u
#define RI_KEY_3 0x03u
#define RI_KEY_4 0x04u
#define RI_KEY_5 0x05u
#define RI_KEY_6 0x06u
#define RI_KEY_7 0x07u
#define RI_KEY_8 0x08u
#define RI_KEY_9 0x09u
#define RI_KEY_0 0x0Au
#define RI_KEY_MINUS 0x0Bu
#define RI_KEY_EQUAL 0x0Cu
#define RI_KEY_BACKSLASH 0x0Du
#define RI_KEY_KP0 0x0Fu
#define RI_KEY_Q 0x10u
#define RI_KEY_W 0x11u
#define RI_KEY_E 0x12u
#define RI_KEY_R 0x13u
#define RI_KEY_T 0x14u
#define RI_KEY_Y 0x15u
#define RI_KEY_U 0x16u
#define RI_KEY_I 0x17u
#define RI_KEY_O 0x18u
#define RI_KEY_P 0x19u
#define RI_KEY_LBRACKET 0x1Au
#define RI_KEY_RBRACKET 0x1Bu
#define RI_KEY_KP1 0x1Du
#define RI_KEY_KP2 0x1Eu
#define RI_KEY_A 0x20u
#define RI_KEY_S 0x21u
#define RI_KEY_D 0x22u
#define RI_KEY_F 0x23u
#define RI_KEY_G 0x24u
#define RI_KEY_H 0x25u
#define RI_KEY_J 0x26u
#define RI_KEY_K 0x27u
#define RI_KEY_L 0x28u
#define RI_KEY_SEMICOLON 0x29u
#define RI_KEY_QUOTE 0x2Au
#define RI_KEY_KP4 0x2Du
#define RI_KEY_KP5 0x2Eu
#define RI_KEY_Z 0x31u
#define RI_KEY_X 0x32u
#define RI_KEY_C 0x33u
#define RI_KEY_V 0x34u
#define RI_KEY_B 0x35u
#define RI_KEY_N 0x36u
#define RI_KEY_M 0x37u
#define RI_KEY_COMMA 0x38u
#define RI_KEY_PERIOD 0x39u
#define RI_KEY_SLASH 0x3Au
#define RI_KEY_KP7 0x3Du
#define RI_KEY_KP8 0x3Eu
#define RI_KEY_SPACE 0x40u
#define RI_KEY_BACKSPACE 0x41u
#define RI_KEY_TAB 0x42u
#define RI_KEY_KPENTER 0x43u
#define RI_KEY_RETURN 0x44u
#define RI_KEY_ESC 0x45u
#define RI_KEY_KPMINUS 0x4Au
#define RI_KEY_UP 0x4Cu
#define RI_KEY_DOWN 0x4Du
#define RI_KEY_LEFT 0x4Fu
#define RI_KEY_RIGHT 0x4Eu
#define RI_KEY_KPSTAR 0x5Du
#define RI_KEY_KPPLUS 0x5Eu
#define RI_KEY_UPFLAG 0x80u

/* Pure positional maps for non-AROS backends (one table each; host-tested).
 * Windows Set-1 make codes (extended 0xE0-prefixed as 0x100|code);
 * SDL scancodes (SDL_scancode.h). Return 0 when unmapped. */
uint16_t ri_key_from_win32(uint16_t set1);
uint16_t ri_key_from_sdl(uint16_t sdlsc);

#endif
