/* t92_draw_hash — portability T2: display-list hash pins.
 * Every laid-out section × zoom 0..3 (procedural, no skin/panel) builds a
 * non-empty list inside the static backing; hashes are pinned below —
 * any drawing change is deliberate.
 */
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "tests/helpers/ri_assert.h"
#include "gui/draw/art.h"
#include "gui/ctlreg.h"
#include "gui/sectui.h"
#include "gui/sectmix.h"

static struct ri_dcmd T_BACK[24576];
static char T_SPOOL[32768];
static struct RIMixBoard T_BOARD;

static const uint8_t T_SECS[] = {
    RI_SEC_SYNTH1, RI_SEC_SYNTH2, RI_SEC_808, RI_SEC_909, RI_SEC_LEVI,
    RI_SEC_MIX_SYNTH1, RI_SEC_MIX_SYNTH2, RI_SEC_MIX_808, RI_SEC_MIX_909, RI_SEC_MASTER,
    RI_SEC_PCF, RI_SEC_DELAY, RI_SEC_DIST, RI_SEC_COMP,
    RI_SEC_TRANSPORT, RI_SEC_PAT_SYNTH1, RI_SEC_PAT_SYNTH2, RI_SEC_PAT_808, RI_SEC_PAT_909,
    RI_SEC_PAT_LEVI, RI_SEC_MIX_LEVI
};

/* Pattern-section device tag: PAT ids are not contiguous (PAT_LEVI=19). */
static const char *pat_tag(uint8_t sec) {
    switch (sec) {
    case RI_SEC_PAT_SYNTH1: return "PATTERN 303A";
    case RI_SEC_PAT_SYNTH2: return "PATTERN 303B";
    case RI_SEC_PAT_808: return "PATTERN 808";
    case RI_SEC_PAT_909: return "PATTERN 909";
    case RI_SEC_PAT_LEVI: return "PATTERN LEVI";
    default: return 0;
    }
}


/* Pinned hashes (first GREEN run): any drawing change is deliberate. */
static const struct { uint8_t sec, z; uint32_t h; } T_PIN[] = {
    { 0u, 0u, 0x7dd3a016u },
    { 0u, 1u, 0x846d70ddu },
    { 0u, 2u, 0x53cbc11fu },
    { 0u, 3u, 0x9978f5cfu },
    { 1u, 0u, 0x7dd3a016u },
    { 1u, 1u, 0x846d70ddu },
    { 1u, 2u, 0x53cbc11fu },
    { 1u, 3u, 0x9978f5cfu },
    { 2u, 0u, 0xc8611d5eu },
    { 2u, 1u, 0x5f7b2f25u },
    { 2u, 2u, 0x2ff4d1abu },
    { 2u, 3u, 0x5f1b5d78u },
    { 3u, 0u, 0xda618659u },
    { 3u, 1u, 0x92453ca5u },
    { 3u, 2u, 0x4f102b7fu },
    { 3u, 3u, 0x92320177u },
    { 4u, 0u, 0x577a5ee1u },
    { 4u, 1u, 0x2055a167u },
    { 4u, 2u, 0x13a44d59u },
    { 4u, 3u, 0xcb00ab75u },
    { 5u, 0u, 0xde57a6dcu },
    { 5u, 1u, 0x09aa5ffau },
    { 5u, 2u, 0x63ad8f8cu },
    { 5u, 3u, 0x9f636824u },
    { 6u, 0u, 0x4bfbbfa2u },
    { 6u, 1u, 0x7b9de564u },
    { 6u, 2u, 0x34f917f6u },
    { 6u, 3u, 0xbf355972u },
    { 7u, 0u, 0xa03e871au },
    { 7u, 1u, 0xd06f64fcu },
    { 7u, 2u, 0xe07b09beu },
    { 7u, 3u, 0x7d7dd2bau },
    { 8u, 0u, 0x25bd61eeu },
    { 8u, 1u, 0x8ff1afa4u },
    { 8u, 2u, 0xefbbc27fu },
    { 8u, 3u, 0xe65b9e75u },
    { 9u, 0u, 0x50d5afd5u },
    { 9u, 1u, 0xd628bfdau },
    { 9u, 2u, 0x30a0cb2bu },
    { 9u, 3u, 0x2c97c73eu },
    { 10u, 0u, 0x96dcca65u },
    { 10u, 1u, 0x2f5529d5u },
    { 10u, 2u, 0xf8a2b152u },
    { 10u, 3u, 0x9e586081u },
    { 11u, 0u, 0x76938c72u },
    { 11u, 1u, 0xbbe4078du },
    { 11u, 2u, 0x03601c95u },
    { 11u, 3u, 0x7b231db2u },
    { 12u, 0u, 0xe3f8b298u },
    { 12u, 1u, 0x2d3e7a3fu },
    { 12u, 2u, 0xb3d4341bu },
    { 12u, 3u, 0x68592477u },
    /* Transport, re-pinned 2026-10-10 for R4's clock-out lamp: the only
     * section whose pixels moved, which is the check working. Six OTHER
     * sections moved first and that was a real bug, not a re-pin -- see
     * the RI_CTLREG_SEC_LO/HI note in gui/ctlreg.c, where the bounds were
     * hand-written literals that a new row silently invalidated. */
    { 13u, 0u, 0xcb839d35u },
    { 13u, 1u, 0x1ca1986au },
    { 13u, 2u, 0x4474cc29u },
    { 13u, 3u, 0x2caa2f17u },
    { 14u, 0u, 0x939d9d41u },
    { 14u, 1u, 0x2991749cu },
    { 14u, 2u, 0x3e6bd744u },
    { 14u, 3u, 0xe95ea013u },
    { 15u, 0u, 0xf4bb77c8u },
    { 15u, 1u, 0xcd4847cdu },
    { 15u, 2u, 0xf78c7a79u },
    { 15u, 3u, 0xdf84da32u },
    { 16u, 0u, 0x8490a310u },
    { 16u, 1u, 0xd3dd2895u },
    { 16u, 2u, 0xd8578429u },
    { 16u, 3u, 0x29cf6650u },
    { 17u, 0u, 0xa3fef008u },
    { 17u, 1u, 0x51f13f5du },
    { 17u, 2u, 0x27e69141u },
    { 17u, 3u, 0x045e0188u },
    /* sec 18 = LEVI, re-pinned 2026-10-13: BALANCE and the four arpeggiator
     * knobs (MODE/OCTAVE/GATE/ENTROPY) moved out of art_levi.c's dim DKNOB
     * table into RI_GEO_LEVI, so they are drawn from ri_sui_value() by
     * ri_art_levi_item -- bright C_LEVI_SILVER at their real value instead of
     * a hardcoded 0.0f. That is a deliberate drawing change and it is the
     * only one: every other section's pin is untouched at all four zooms,
     * which is what confirms the delta is confined to the Levi panel. */
    /* sec 18 = LEVI, re-pinned 2026-10-23 for P-KEY2: the keybed's paint rule
     * (ri_levi_key_paint_q) now paints each key's OWN hit box instead of
     * stretching whites over the whole band, and ri_art_bg_levi lays the white
     * bed (355..470) for the live slots so the keys keep their full height.
     * That is a deliberate drawing change, and it is the only one: every
     * OTHER section's pin is untouched at all four zooms, which is what
     * confirms the delta is confined to the Levi panel. */
    { 18u, 0u, 0xf95e0ef2u},
    { 18u, 1u, 0x965d4934u},
    { 18u, 2u, 0x7ba7cb22u},
    { 18u, 3u, 0xb3701436u},
    { 19u, 0u, 0x7323b7e8u },
    { 19u, 1u, 0xaeb0bb65u },
    { 19u, 2u, 0x07c41ed9u },
    { 19u, 3u, 0x32830106u },
    { 20u, 0u, 0xcd4734cbu },
    { 20u, 1u, 0xc6e25d49u },
    { 20u, 2u, 0x52a0c26bu },
    { 20u, 3u, 0x21f26307u },
};

static uint32_t pin_lookup(uint8_t sec, uint8_t z) {
    uint32_t i;
    for (i = 0u; i < sizeof(T_PIN) / sizeof(T_PIN[0]); i++)
        if (T_PIN[i].sec == sec && T_PIN[i].z == (uint8_t)z)
            return T_PIN[i].h;
    return 0u;
}

int main(void) {
    uint32_t s, z;
    ri_smix_init(&T_BOARD);
    for (s = 0u; s < sizeof(T_SECS); s++) {
        struct RISectUI ui;
        if (ri_sui_init(&ui, T_SECS[s]) != 0)
            continue; /* not laid out here: nothing to pin */
        if (ri_smix_strip(T_SECS[s]) >= 0 || T_SECS[s] == RI_SEC_MASTER)
            ri_sui_bind_board(&ui, &T_BOARD);
        for (z = 0u; z < 4u; z++) {
            struct ri_dlist dl;
            uint32_t h;
            ri_dlist_init(&dl, T_BACK, 24576u, T_SPOOL, sizeof T_SPOOL);
            ri_draw_section(&dl, &ui, T_SECS[s], (int)z, 0, 0, 0, 0, 0);
            RI_ASSERT(dl.n > 0u, "sec %u z%u empty", T_SECS[s], z);
            RI_ASSERT(dl.n < dl.cap, "sec %u z%u overflow", T_SECS[s], z);
            RI_ASSERT(dl.spn < dl.spcap, "sec %u z%u spool", T_SECS[s], z);
            h = ri_dlist_hash(&dl);
            RI_ASSERT(h == pin_lookup(T_SECS[s], z), "pin sec=%u z=%u got %08x", T_SECS[s], z, h);
            /* Title-bar device tag (owner 2026-09-27): each pattern block
             * names the device it affects, from the wiring table. */
            if (pat_tag(T_SECS[s]) && z == 0u) {
                uint32_t k, found = 0u;
                const char *tag = pat_tag(T_SECS[s]);
                for (k = 0u; k < dl.n; k++) {
                    if (dl.cmd[k].op == RI_D_TEXT && dl.cmd[k].text &&
                        !strcmp(dl.cmd[k].text, tag)) {
                        found = 1u;
                        break;
                    }
                }
                RI_ASSERT(found, "tag %s sec=%u", tag, T_SECS[s]);
            }
        }
    }
    RI_RESULT("draw_hash");
}
