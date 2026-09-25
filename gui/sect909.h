/* gui/sect909.h — 909 section front-panel behaviour (§12.10 G3/G4).
 * Pure C, host-tested. ReBirth 2.0.1 Owner's Manual p. 29-31, 151-152:
 * instruments are selected by clicking their legend (AC = accent row);
 * a step click cycles off -> low -> high -> off; with the Flam button
 * active a click toggles off <-> flam (flam width = the global Flam knob).
 * Values index = registry index within the section (reg_id & 0xFF).
 */
#ifndef RI_SECT909_H
#define RI_SECT909_H
#include <stdint.h>
#include "engine/seq/pattern.h"

#define RI_S909_NCTL 46u
#define RI_S909_FLAM 27u       /* Flam knob (width) */
#define RI_S909_SELECT 28u     /* Instrument Selection: 0 = AC, 1..11 = BD..RC */
#define RI_S909_FLAMBTN 29u    /* Flam button (entry mode) */
#define RI_S909_STEP0 30u      /* 16 step buttons: 30..45 */

struct RISect909 {
    uint8_t section;           /* RI_SEC_909 */
    uint8_t pad[3];
    int16_t val[RI_S909_NCTL];
    struct RIPattern pat;      /* drum kind, class 909 */
};

int ri_s909_init(struct RISect909 *s);
int ri_s909_press(struct RISect909 *s, uint32_t idx);
int ri_s909_set_value(struct RISect909 *s, uint32_t idx, int v);
int ri_s909_reset(struct RISect909 *s, uint32_t idx);
/* Step: RI_HIT_OFF/LOW/HIGH/FLAM of the selected instrument (AC row: 0/1);
 * Flam button: 1 when active; else 0. */
int ri_s909_led(const struct RISect909 *s, uint32_t idx);
int ri_s909_lane(const struct RISect909 *s); /* -1 = AC */
#endif
