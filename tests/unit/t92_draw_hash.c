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
    RI_SEC_SYNTH1, RI_SEC_SYNTH2, RI_SEC_808, RI_SEC_909,
    RI_SEC_MIX_SYNTH1, RI_SEC_MIX_SYNTH2, RI_SEC_MIX_808, RI_SEC_MIX_909, RI_SEC_MASTER,
    RI_SEC_PCF, RI_SEC_DELAY, RI_SEC_DIST, RI_SEC_COMP,
    RI_SEC_TRANSPORT, RI_SEC_PAT_SYNTH1, RI_SEC_PAT_SYNTH2, RI_SEC_PAT_808, RI_SEC_PAT_909
};


/* Pinned hashes (first GREEN run): any drawing change is deliberate. */
static const struct { uint8_t sec, z; uint32_t h; } T_PIN[] = {
    { 0u, 0u, 0xfbb0028fu },
    { 0u, 1u, 0x3ecd1623u },
    { 0u, 2u, 0x7d5a0d32u },
    { 0u, 3u, 0x51c105e8u },
    { 1u, 0u, 0xfbb0028fu },
    { 1u, 1u, 0x3ecd1623u },
    { 1u, 2u, 0x7d5a0d32u },
    { 1u, 3u, 0x51c105e8u },
    { 2u, 0u, 0xca11875du },
    { 2u, 1u, 0x41f079cbu },
    { 2u, 2u, 0x524232fau },
    { 2u, 3u, 0xc08c0753u },
    { 3u, 0u, 0xa26a4174u },
    { 3u, 1u, 0x668aae6fu },
    { 3u, 2u, 0x28f9c35eu },
    { 3u, 3u, 0xca318f84u },
    { 4u, 0u, 0x3008d710u },
    { 4u, 1u, 0x505d3812u },
    { 4u, 2u, 0xc8559586u },
    { 4u, 3u, 0xa68cb014u },
    { 5u, 0u, 0x3008d710u },
    { 5u, 1u, 0x505d3812u },
    { 5u, 2u, 0xc8559586u },
    { 5u, 3u, 0xa68cb014u },
    { 6u, 0u, 0x3008d710u },
    { 6u, 1u, 0x505d3812u },
    { 6u, 2u, 0xc8559586u },
    { 6u, 3u, 0xa68cb014u },
    { 7u, 0u, 0x3008d710u },
    { 7u, 1u, 0x505d3812u },
    { 7u, 2u, 0xc8559586u },
    { 7u, 3u, 0xa68cb014u },
    { 8u, 0u, 0xd8ac2b29u },
    { 8u, 1u, 0x47c87343u },
    { 8u, 2u, 0x60a81cadu },
    { 8u, 3u, 0x8100d722u },
    { 9u, 0u, 0xb81647acu },
    { 9u, 1u, 0x6a05c910u },
    { 9u, 2u, 0x9bd38628u },
    { 9u, 3u, 0x2637579bu },
    { 10u, 0u, 0xbc04cdbeu },
    { 10u, 1u, 0x1a970823u },
    { 10u, 2u, 0x5d0073b7u },
    { 10u, 3u, 0xb4d0531eu },
    { 11u, 0u, 0x7578c9bbu },
    { 11u, 1u, 0x5453cb3bu },
    { 11u, 2u, 0xfb305556u },
    { 11u, 3u, 0x01a1b58fu },
    { 12u, 0u, 0xa6d80cb2u },
    { 12u, 1u, 0xbf847a3fu },
    { 12u, 2u, 0xf457e455u },
    { 12u, 3u, 0xbc66064du },
    { 13u, 0u, 0xfb09738eu },
    { 13u, 1u, 0x22c00e1au },
    { 13u, 2u, 0x2e10e1ecu },
    { 13u, 3u, 0x9a2b7b86u },
    { 14u, 0u, 0xde464043u },
    { 14u, 1u, 0x474156dcu },
    { 14u, 2u, 0x22b06e9eu },
    { 14u, 3u, 0xe69393e9u },
    { 15u, 0u, 0x12612cc6u },
    { 15u, 1u, 0xf1b84b69u },
    { 15u, 2u, 0xdf2deae3u },
    { 15u, 3u, 0x74cd7c1cu },
    { 16u, 0u, 0x9b05b388u },
    { 16u, 1u, 0x31d56411u },
    { 16u, 2u, 0x3b154bedu },
    { 16u, 3u, 0x40e57ea4u },
    { 17u, 0u, 0xaa278f70u },
    { 17u, 1u, 0x2d0ec499u },
    { 17u, 2u, 0x76c9beb5u },
    { 17u, 3u, 0x073ab36cu },
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
