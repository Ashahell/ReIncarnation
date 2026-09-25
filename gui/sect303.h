/* gui/sect303.h — 303 section front-panel behaviour (§12.10 G3/G4).
 * Pure C, host-tested. Owns the values of the 30 registry controls of one
 * synth section and the step-entry workflow of the ReBirth 2.0.1 manual
 * (p. 40-42, 153-156): Step/Back wrap, Pitch Mode auto-advance, "click
 * Pitch Mode twice -> step 1", Up/Down/Accent/Slide/Note-Pause toggles on
 * the edit step, pitch keys set the key only (Note/Pause is separate),
 * Clear. Values index = registry index within the section (reg_id & 0xFF).
 */
#ifndef RI_SECT303_H
#define RI_SECT303_H
#include <stdint.h>
#include "engine/seq/pattern.h"

#define RI_S303_NCTL 30u
/* registry indices within a synth section (gui/ctlreg.c order) */
#define RI_S303_WAVE 0u
#define RI_S303_TUNE 1u
#define RI_S303_ACCENT_KNOB 6u
#define RI_S303_KEY0 7u     /* 13 pitch keys: 7..19 = low C .. high C */
#define RI_S303_DOWN 20u
#define RI_S303_UP 21u
#define RI_S303_ACCENT 22u
#define RI_S303_SLIDE 23u
#define RI_S303_NOTEPAUSE 24u
#define RI_S303_BACK 25u
#define RI_S303_STEP 26u
#define RI_S303_PITCHMODE 27u
#define RI_S303_CLEAR 28u
#define RI_S303_DISPLAY 29u

struct RISect303 {
    uint8_t section;        /* RI_SEC_SYNTH1 / RI_SEC_SYNTH2 */
    uint8_t edit_step;      /* 0..15 */
    uint8_t pitch_mode;     /* 0/1 */
    uint8_t pm_rearm;       /* Pitch Mode turned off with nothing else since */
    int16_t val[RI_S303_NCTL];
    struct RIPattern pat;
};

int ri_s303_init(struct RISect303 *s, uint8_t section);      /* 0 ok, 2 bad section */
/* Click on a button / switch / key. Returns 1 when any state changed. */
int ri_s303_press(struct RISect303 *s, uint32_t idx);
/* Set a value control (knob / switch), clamped to its registry range.
 * Returns 1 when the value changed. */
int ri_s303_set_value(struct RISect303 *s, uint32_t idx, int v);
int ri_s303_reset(struct RISect303 *s, uint32_t idx);         /* right-click default */
/* LED state of a control (which = 0; Note/Pause: 0 = note LED, 1 = pause LED). */
int ri_s303_led(const struct RISect303 *s, uint32_t idx, uint32_t which);
int ri_s303_display(const struct RISect303 *s);               /* EDIT STEP 1..16 */
#endif
