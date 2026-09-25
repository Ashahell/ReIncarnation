/* gui/sectui.h — one front-panel behaviour interface for every laid-out
 * section (§12.10 G4). Pure C, host-tested: the AROS canvas (RSection)
 * talks only to this, never to a section's own module. Index = registry
 * index within the section (reg_id & 0xFF).
 */
#ifndef RI_SECTUI_H
#define RI_SECTUI_H
#include <stdint.h>
#include "gui/sect303.h"
#include "gui/sect808.h"
#include "gui/sect909.h"

struct RISectUI {
    uint8_t section;  /* RI_SEC_* */
    uint8_t pad[3];
    union {
        struct RISect303 s303;
        struct RISect808 s808;
        struct RISect909 s909;
    } u;
};

int ri_sui_init(struct RISectUI *s, uint8_t section); /* 0 ok, 2 not laid out */
int ri_sui_press(struct RISectUI *s, uint32_t idx);
int ri_sui_set(struct RISectUI *s, uint32_t idx, int v);
int ri_sui_reset(struct RISectUI *s, uint32_t idx);
int ri_sui_value(const struct RISectUI *s, uint32_t idx);
int ri_sui_led(const struct RISectUI *s, uint32_t idx, uint32_t which);
/* numeric readout of a DISPLAY control (303 EDIT STEP 1..16); 0 otherwise */
int ri_sui_display(const struct RISectUI *s, uint32_t idx);
#endif
