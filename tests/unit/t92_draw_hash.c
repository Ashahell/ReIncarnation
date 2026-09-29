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
    { 4u, 0u, 0x57ddab19u },
    { 4u, 1u, 0x64f34b03u },
    { 4u, 2u, 0xd034b7d4u },
    { 4u, 3u, 0x7ee16e6du },
    { 5u, 0u, 0x84714234u },
    { 5u, 1u, 0x0226447eu },
    { 5u, 2u, 0x2e14a881u },
    { 5u, 3u, 0x58353a2cu },
    { 6u, 0u, 0x874d529au },
    { 6u, 1u, 0x7469edd0u },
    { 6u, 2u, 0x3a714453u },
    { 6u, 3u, 0x09ba69eau },
    { 7u, 0u, 0xeb4ef872u },
    { 7u, 1u, 0xb3c77e48u },
    { 7u, 2u, 0xdd50447bu },
    { 7u, 3u, 0xcfe464a2u },
    { 8u, 0u, 0xf0eb2b26u },
    { 8u, 1u, 0x9e3eabe0u },
    { 8u, 2u, 0xbd60126fu },
    { 8u, 3u, 0xbdb2950du },
    { 9u, 0u, 0xd527c0fdu },
    { 9u, 1u, 0x24cae6f6u },
    { 9u, 2u, 0xc03fd817u },
    { 9u, 3u, 0x4e222296u },
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
    { 13u, 0u, 0x9d2edb52u },
    { 13u, 1u, 0x699cb048u },
    { 13u, 2u, 0x876d6f55u },
    { 13u, 3u, 0x5a79732eu },
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
    { 18u, 0u, 0x96ff0ddeu },
    { 18u, 1u, 0x8526927cu },
    { 18u, 2u, 0x3ab20c12u },
    { 18u, 3u, 0x178d525au },
    { 19u, 0u, 0x7323b7e8u },
    { 19u, 1u, 0xaeb0bb65u },
    { 19u, 2u, 0x07c41ed9u },
    { 19u, 3u, 0x32830106u },
    { 20u, 0u, 0x753de8c3u },
    { 20u, 1u, 0x57053cc5u },
    { 20u, 2u, 0x6ed317aeu },
    { 20u, 3u, 0xbd0e05afu },
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
