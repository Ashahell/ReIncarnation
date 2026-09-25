/* gui/sect808.h — 808 section front-panel behaviour (§12.10 G3/G4).
 * Pure C, host-tested. ReBirth 2.0.1 Owner's Manual p. 28-35, 148-150:
 * the Instrument Selection (large knob or a click on an instrument legend)
 * chooses which row the 16 step buttons show and edit; AC is the accent
 * row; five switches pick the alternate sound of a slot (LT/LC, MT/MC,
 * HT/HC, RS/CL, CP/MA) — panel state, the pattern keeps the slot.
 * Values index = registry index within the section (reg_id & 0xFF).
 */
#ifndef RI_SECT808_H
#define RI_SECT808_H
#include <stdint.h>
#include "engine/seq/pattern.h"

#define RI_S808_NCTL 44u
#define RI_S808_SELECT 27u   /* Instrument Selection: 0 = AC, 1..11 = BD..CH */
#define RI_S808_STEP0 28u    /* 16 step buttons: 28..43 */

struct RISect808 {
    uint8_t section;         /* RI_SEC_808 */
    uint8_t pad[3];
    int16_t val[RI_S808_NCTL];
    struct RIPattern pat;    /* drum kind, class 808 */
};

int ri_s808_init(struct RISect808 *s);
/* Click on a step button or switch. Returns 1 when state changed. */
int ri_s808_press(struct RISect808 *s, uint32_t idx);
/* Set a value control (knob / switch / selector), clamped. 1 = changed. */
int ri_s808_set_value(struct RISect808 *s, uint32_t idx, int v);
int ri_s808_reset(struct RISect808 *s, uint32_t idx);
/* Lit state: step buttons show the selected instrument's row (AC row when
 * AC is selected); switches show the alternate sound. */
int ri_s808_led(const struct RISect808 *s, uint32_t idx);
/* Lane edited by the step buttons, or -1 when AC is selected. */
int ri_s808_lane(const struct RISect808 *s);
#endif
