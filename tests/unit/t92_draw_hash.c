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

static struct ri_dcmd T_BACK[8192];
static char T_SPOOL[32768];
static struct RIMixBoard T_BOARD;

static const uint8_t T_SECS[] = {
    RI_SEC_SYNTH1, RI_SEC_SYNTH2, RI_SEC_808, RI_SEC_909,
    RI_SEC_MIX_SYNTH1, RI_SEC_MIX_SYNTH2, RI_SEC_MIX_808, RI_SEC_MIX_909, RI_SEC_MASTER,
    RI_SEC_PCF, RI_SEC_DELAY, RI_SEC_DIST, RI_SEC_COMP,
    RI_SEC_TRANSPORT, RI_SEC_PAT_SYNTH1, RI_SEC_PAT_SYNTH2, RI_SEC_PAT_808, RI_SEC_PAT_909
};


/* Pinned hashes (first GREEN run): any drawing change is deliberate. */
static const struct { uint8_t sec, z; uint32_t h; } T_PIN[] = {
    { 0u, 0u, 0xbbec181du },
    { 0u, 1u, 0x325393dbu },
    { 0u, 2u, 0x527ea532u },
    { 0u, 3u, 0x5afe0508u },
    { 1u, 0u, 0xbbec181du },
    { 1u, 1u, 0x325393dbu },
    { 1u, 2u, 0x527ea532u },
    { 1u, 3u, 0x5afe0508u },
    { 2u, 0u, 0xa09e073cu },
    { 2u, 1u, 0xd88bd2cbu },
    { 2u, 2u, 0x3ace0365u },
    { 2u, 3u, 0x793e4e5du },
    { 3u, 0u, 0x28956d4eu },
    { 3u, 1u, 0xb360a453u },
    { 3u, 2u, 0xf9af02f9u },
    { 3u, 3u, 0xc700dbd5u },
    { 4u, 0u, 0x9e77619eu },
    { 4u, 1u, 0xb387b378u },
    { 4u, 2u, 0xd725f16du },
    { 4u, 3u, 0x6dc4e7a4u },
    { 5u, 0u, 0x9e77619eu },
    { 5u, 1u, 0xb387b378u },
    { 5u, 2u, 0xd725f16du },
    { 5u, 3u, 0x6dc4e7a4u },
    { 6u, 0u, 0x9e77619eu },
    { 6u, 1u, 0xb387b378u },
    { 6u, 2u, 0xd725f16du },
    { 6u, 3u, 0x6dc4e7a4u },
    { 7u, 0u, 0x9e77619eu },
    { 7u, 1u, 0xb387b378u },
    { 7u, 2u, 0xd725f16du },
    { 7u, 3u, 0x6dc4e7a4u },
    { 8u, 0u, 0xdd18b312u },
    { 8u, 1u, 0xd2aae2c9u },
    { 8u, 2u, 0x07180b6du },
    { 8u, 3u, 0x1a54c655u },
    { 9u, 0u, 0x54ffddfbu },
    { 9u, 1u, 0xe7a9ec00u },
    { 9u, 2u, 0xba4b2e72u },
    { 9u, 3u, 0x3a57d834u },
    { 10u, 0u, 0x13f9f90au },
    { 10u, 1u, 0x666532bfu },
    { 10u, 2u, 0x5673b186u },
    { 10u, 3u, 0x7b24532fu },
    { 11u, 0u, 0xa2904703u },
    { 11u, 1u, 0x70943de0u },
    { 11u, 2u, 0xd69c759bu },
    { 11u, 3u, 0x7ce36ac7u },
    { 12u, 0u, 0xe250e607u },
    { 12u, 1u, 0x31befd1cu },
    { 12u, 2u, 0x2043ada5u },
    { 12u, 3u, 0x0f620ca1u },
    { 13u, 0u, 0x857fb23du },
    { 13u, 1u, 0x0e7ea3cbu },
    { 13u, 2u, 0x4899e30fu },
    { 13u, 3u, 0x7a36e7a1u },
    { 14u, 0u, 0x8fd852bbu },
    { 14u, 1u, 0x7f139ae0u },
    { 14u, 2u, 0x6f397638u },
    { 14u, 3u, 0x34fe0657u },
    { 15u, 0u, 0xf7cae7f6u },
    { 15u, 1u, 0x8ffbb3a9u },
    { 15u, 2u, 0x65c85889u },
    { 15u, 3u, 0xab356e92u },
    { 16u, 0u, 0x63a5bb74u },
    { 16u, 1u, 0x1d472bd7u },
    { 16u, 2u, 0x4b9d85d9u },
    { 16u, 3u, 0x3abbe17cu },
    { 17u, 0u, 0x9c346d5cu },
    { 17u, 1u, 0x2c0a0fffu },
    { 17u, 2u, 0xf1212241u },
    { 17u, 3u, 0x8e33cdb4u },
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
            ri_dlist_init(&dl, T_BACK, 8192u, T_SPOOL, sizeof T_SPOOL);
            ri_draw_section(&dl, &ui, T_SECS[s], (int)z, 0, 0, 0, 0, 0);
            RI_ASSERT(dl.n > 0u, "sec %u z%u empty", T_SECS[s], z);
            RI_ASSERT(dl.n < dl.cap, "sec %u z%u overflow", T_SECS[s], z);
            RI_ASSERT(dl.spn < dl.spcap, "sec %u z%u spool", T_SECS[s], z);
            h = ri_dlist_hash(&dl);
            RI_ASSERT(h == pin_lookup(T_SECS[s], z), "pin sec=%u z=%u got %08x", T_SECS[s], z, h);
            /* Title-bar device tag (owner 2026-09-27): each pattern block
             * names the device it affects, from the wiring table. */
            if (T_SECS[s] >= RI_SEC_PAT_SYNTH1 && T_SECS[s] <= RI_SEC_PAT_909 && z == 0u) {
                static const char *const want[4] = {
                    "PATTERN 303A", "PATTERN 303B", "PATTERN 808", "PATTERN 909"
                };
                uint32_t k, found = 0u;
                const char *tag = want[T_SECS[s] - RI_SEC_PAT_SYNTH1];
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
