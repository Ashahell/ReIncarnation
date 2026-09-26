/* t85_pal_keys — portability T3: RI_KEY_* positional tables.
 * AROS/host identity; Windows Set-1 + SDL maps cover every Appendix-E
 * code the keymap uses (completeness), with spot-checked positions.
 */
#include <stdio.h>
#include <stdint.h>
#include "tests/helpers/ri_assert.h"
#include "platform/pal/ri_pal_input.h"

/* Every raw code gui/keymap.c dispatches on (union of MENU, TRANSPORT,
 * PITCH, SYNTH, PATROW ranges, drum taps, arrows, Tab). */
static const uint8_t T85_USED[] = {
    0x00,
    0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
    0x09, 0x0A, 0x0B, 0x0C,
    0x0F,
    0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17,
    0x18, 0x19, 0x1A, 0x1B, 0x1D, 0x1E,
    0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27,
    0x28, 0x29, 0x2A, 0x2D, 0x2E,
    0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38,
    0x39, 0x3A, 0x3D, 0x3E,
    0x40, 0x41, 0x42, 0x43, 0x44, 0x45,
    0x4A, 0x4C, 0x4D, 0x4E, 0x4F,
    0x5D, 0x5E
};

int main(void) {
    uint32_t i;
    /* AROS/host identity across the whole used set. */
    for (i = 0u; i < sizeof(T85_USED); i++)
        RI_ASSERT(ri_pal_key_map(T85_USED[i]) == T85_USED[i], "identity 0x%02x", T85_USED[i]);
    /* Spot positions: physical keys, all three backends. */
    RI_ASSERT(ri_key_from_win32(0x0Fu) == RI_KEY_TAB, "win32 tab");
    RI_ASSERT(ri_key_from_win32(0x1Cu) == RI_KEY_RETURN, "win32 return");
    RI_ASSERT(ri_key_from_win32(0x39u) == RI_KEY_SPACE, "win32 space");
    RI_ASSERT(ri_key_from_win32(0x148u) == RI_KEY_UP, "win32 up");
    RI_ASSERT(ri_key_from_win32(0x150u) == RI_KEY_DOWN, "win32 down");
    RI_ASSERT(ri_key_from_sdl(43u) == RI_KEY_TAB, "sdl tab");
    RI_ASSERT(ri_key_from_sdl(40u) == RI_KEY_RETURN, "sdl return");
    RI_ASSERT(ri_key_from_sdl(44u) == RI_KEY_SPACE, "sdl space");
    RI_ASSERT(ri_key_from_sdl(82u) == RI_KEY_UP, "sdl up");
    RI_ASSERT(ri_key_from_sdl(81u) == RI_KEY_DOWN, "sdl down");
    /* Completeness: every used code reachable from Win32 + SDL. */
    for (i = 0u; i < sizeof(T85_USED); i++) {
        uint32_t w, s, found_w = 0u, found_s = 0u;
        for (w = 0u; w < 0x200u && !found_w; w++)
            if (ri_key_from_win32((uint16_t)w) == T85_USED[i])
                found_w = 1u;
        for (s = 0u; s < 282u && !found_s; s++)
            if (ri_key_from_sdl((uint16_t)s) == T85_USED[i])
                found_s = 1u;
        RI_ASSERT(found_w, "win32 covers 0x%02x", T85_USED[i]);
        RI_ASSERT(found_s, "sdl covers 0x%02x", T85_USED[i]);
    }
    /* Aliases pin the raw values (keymap unchanged). */
    RI_ASSERT(RI_KEY_TAB == 0x42u, "alias tab");
    RI_ASSERT(RI_KEY_SPACE == 0x40u, "alias space");
    RI_ASSERT(RI_KEY_MINUS == 0x0Bu, "alias minus");
    RI_RESULT("pal_keys");
}
