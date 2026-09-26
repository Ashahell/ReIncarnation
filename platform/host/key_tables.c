/* key_tables.c — positional key maps (portability plan T3).
 * Pure C99, host-testable. AROS/host backends use the identity map;
 * Windows Set-1 and SDL scancodes map to RI_KEY_* by physical position.
 * Sources: IBM Set-1 make codes (Microsoft "Scan Code Set 1" docs);
 * SDL_scancode.h (USB HID usage IDs). Extended Set-1 (0xE0 prefix) is
 * passed as 0x100|code.
 */
#include "platform/pal/ri_pal_input.h"

uint16_t ri_pal_key_map(uint16_t native) {
    return native; /* AROS/host identity: native codes are positional */
}

uint16_t ri_key_from_win32(uint16_t set1) {
    switch (set1) {
    case 0x29u: return RI_KEY_GRAVE;
    case 0x02u: return RI_KEY_1;
    case 0x03u: return RI_KEY_2;
    case 0x04u: return RI_KEY_3;
    case 0x05u: return RI_KEY_4;
    case 0x06u: return RI_KEY_5;
    case 0x07u: return RI_KEY_6;
    case 0x08u: return RI_KEY_7;
    case 0x09u: return RI_KEY_8;
    case 0x0Au: return RI_KEY_9;
    case 0x0Bu: return RI_KEY_0;
    case 0x0Cu: return RI_KEY_MINUS;
    case 0x0Du: return RI_KEY_EQUAL;
    case 0x52u: return RI_KEY_KP0;
    case 0x10u: return RI_KEY_Q;
    case 0x11u: return RI_KEY_W;
    case 0x12u: return RI_KEY_E;
    case 0x13u: return RI_KEY_R;
    case 0x14u: return RI_KEY_T;
    case 0x15u: return RI_KEY_Y;
    case 0x16u: return RI_KEY_U;
    case 0x17u: return RI_KEY_I;
    case 0x18u: return RI_KEY_O;
    case 0x19u: return RI_KEY_P;
    case 0x1Au: return RI_KEY_LBRACKET;
    case 0x1Bu: return RI_KEY_RBRACKET;
    case 0x4Fu: return RI_KEY_KP1;
    case 0x50u: return RI_KEY_KP2;
    case 0x1Eu: return RI_KEY_A;
    case 0x1Fu: return RI_KEY_S;
    case 0x20u: return RI_KEY_D;
    case 0x21u: return RI_KEY_F;
    case 0x22u: return RI_KEY_G;
    case 0x23u: return RI_KEY_H;
    case 0x24u: return RI_KEY_J;
    case 0x25u: return RI_KEY_K;
    case 0x26u: return RI_KEY_L;
    case 0x27u: return RI_KEY_SEMICOLON;
    case 0x28u: return RI_KEY_QUOTE;
    case 0x4Bu: return RI_KEY_KP4;
    case 0x4Cu: return RI_KEY_KP5;
    case 0x2Cu: return RI_KEY_Z;
    case 0x2Du: return RI_KEY_X;
    case 0x2Eu: return RI_KEY_C;
    case 0x2Fu: return RI_KEY_V;
    case 0x30u: return RI_KEY_B;
    case 0x31u: return RI_KEY_N;
    case 0x32u: return RI_KEY_M;
    case 0x33u: return RI_KEY_COMMA;
    case 0x34u: return RI_KEY_PERIOD;
    case 0x35u: return RI_KEY_SLASH;
    case 0x47u: return RI_KEY_KP7;
    case 0x48u: return RI_KEY_KP8;
    case 0x39u: return RI_KEY_SPACE;
    case 0x0Eu: return RI_KEY_BACKSPACE;
    case 0x0Fu: return RI_KEY_TAB;
    case 0x11Cu: return RI_KEY_KPENTER;
    case 0x1Cu: return RI_KEY_RETURN;
    case 0x01u: return RI_KEY_ESC;
    case 0x4Au: return RI_KEY_KPMINUS;
    case 0x148u: return RI_KEY_UP;
    case 0x150u: return RI_KEY_DOWN;
    case 0x14Du: return RI_KEY_RIGHT;
    case 0x14Bu: return RI_KEY_LEFT;
    case 0x37u: return RI_KEY_KPSTAR;
    case 0x4Eu: return RI_KEY_KPPLUS;
    default: return 0u;
    }
}

uint16_t ri_key_from_sdl(uint16_t sdlsc) {
    switch (sdlsc) {
    case 53u: return RI_KEY_GRAVE;
    case 30u: return RI_KEY_1;
    case 31u: return RI_KEY_2;
    case 32u: return RI_KEY_3;
    case 33u: return RI_KEY_4;
    case 34u: return RI_KEY_5;
    case 35u: return RI_KEY_6;
    case 36u: return RI_KEY_7;
    case 37u: return RI_KEY_8;
    case 38u: return RI_KEY_9;
    case 39u: return RI_KEY_0;
    case 45u: return RI_KEY_MINUS;
    case 46u: return RI_KEY_EQUAL;
    case 98u: return RI_KEY_KP0;
    case 20u: return RI_KEY_Q;
    case 26u: return RI_KEY_W;
    case 8u: return RI_KEY_E;
    case 21u: return RI_KEY_R;
    case 23u: return RI_KEY_T;
    case 28u: return RI_KEY_Y;
    case 24u: return RI_KEY_U;
    case 12u: return RI_KEY_I;
    case 18u: return RI_KEY_O;
    case 19u: return RI_KEY_P;
    case 47u: return RI_KEY_LBRACKET;
    case 48u: return RI_KEY_RBRACKET;
    case 89u: return RI_KEY_KP1;
    case 90u: return RI_KEY_KP2;
    case 4u: return RI_KEY_A;
    case 22u: return RI_KEY_S;
    case 7u: return RI_KEY_D;
    case 9u: return RI_KEY_F;
    case 10u: return RI_KEY_G;
    case 11u: return RI_KEY_H;
    case 13u: return RI_KEY_J;
    case 14u: return RI_KEY_K;
    case 15u: return RI_KEY_L;
    case 51u: return RI_KEY_SEMICOLON;
    case 52u: return RI_KEY_QUOTE;
    case 92u: return RI_KEY_KP4;
    case 93u: return RI_KEY_KP5;
    case 29u: return RI_KEY_Z;
    case 27u: return RI_KEY_X;
    case 6u: return RI_KEY_C;
    case 25u: return RI_KEY_V;
    case 5u: return RI_KEY_B;
    case 17u: return RI_KEY_N;
    case 16u: return RI_KEY_M;
    case 54u: return RI_KEY_COMMA;
    case 55u: return RI_KEY_PERIOD;
    case 56u: return RI_KEY_SLASH;
    case 95u: return RI_KEY_KP7;
    case 96u: return RI_KEY_KP8;
    case 44u: return RI_KEY_SPACE;
    case 42u: return RI_KEY_BACKSPACE;
    case 43u: return RI_KEY_TAB;
    case 88u: return RI_KEY_KPENTER;
    case 40u: return RI_KEY_RETURN;
    case 41u: return RI_KEY_ESC;
    case 86u: return RI_KEY_KPMINUS;
    case 82u: return RI_KEY_UP;
    case 81u: return RI_KEY_DOWN;
    case 79u: return RI_KEY_RIGHT;
    case 80u: return RI_KEY_LEFT;
    case 85u: return RI_KEY_KPSTAR;
    case 87u: return RI_KEY_KPPLUS;
    default: return 0u;
    }
}
