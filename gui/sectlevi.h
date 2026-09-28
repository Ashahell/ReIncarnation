/* gui/sectlevi.h — Levi section front-panel behaviour (owner 2026-09-28).
 * Pure C, host-tested. 909-style lane select (SELECTOR) + 16 step
 * buttons (toggle the selected lane, middle C on enable) with 303-style
 * edit step + piano keyboard for pitch (13 chromatic keys C..C+).
 * Values index = registry index within the section (reg_id & 0xFF).
 */
#ifndef RI_SECTLEVI_H
#define RI_SECTLEVI_H
#include <stdint.h>
#include "engine/seq/pattern.h"
#include "engine/dsp/levi.h"

#define RI_SLEVI_NCTL 68u
#define RI_SLEVI_SELECT 4u      /* Lane Selection: 0..5 */
#define RI_SLEVI_MODE 2u          /* FM/PM toggle */
#define RI_SLEVI_ALGO 37u         /* Algorithm select 0..7 */
#define RI_SLEVI_ALGOB 38u        /* Morph target 0..7 */
#define RI_SLEVI_MORPH 39u        /* Morph position 0..100 */
#define RI_SLEVI_OPSEL 40u        /* Op selection 0..7 (UI-only) */
#define RI_SLEVI_OPMODE 41u       /* Op Mode 0..6 (packed op*16+mode) */
#define RI_SLEVI_FTYPE 42u        /* Filter type 0..3 */
#define RI_SLEVI_DRIVE 43u        /* Drive 0..127 */
#define RI_SLEVI_CUTOFF2 44u      /* Analog cutoff */
#define RI_SLEVI_RESO2 45u        /* Analog reso */
#define RI_SLEVI_ATTACK 46u       /* Attack time */
#define RI_SLEVI_DECAY 47u        /* Decay time */
#define RI_SLEVI_SUSTAIN 48u      /* Sustain level */
#define RI_SLEVI_RELEASE 49u      /* Release time */
#define RI_SLEVI_LOOP 50u /* Envelope loop */
#define RI_SLEVI_ALGODISP 51u   /* Central algorithm readout 1..8 */
#define RI_SLEVI_ARPON 52u        /* Arp on (UI-only, binds later) */
#define RI_SLEVI_ARPRATE 53u      /* Arp rate (UI-only) */
#define RI_SLEVI_SEQON 54u        /* Seq on (UI-only) */
#define RI_SLEVI_SEQLEN 55u       /* Seq length 1..16 (UI-only) */
#define RI_SLEVI_ROUTE0 56u       /* Matrix routes 0..7 (UI-only) */
#define RI_SLEVI_FXPRE 64u        /* PreFX (UI-only) */
#define RI_SLEVI_FXDLY 65u        /* Delay (UI-only) */
#define RI_SLEVI_FXREV 66u        /* Reverb (UI-only) */
#define RI_SLEVI_FXPOST 67u       /* PostFX (UI-only) */
#define RI_SLEVI_STEP 5u     /* Step: edit_step + 1 (wraps) */
#define RI_SLEVI_BACK 6u     /* Back: edit_step - 1 (wraps) */
#define RI_SLEVI_DISPLAY 7u     /* EDIT STEP readout */
#define RI_SLEVI_STEP0 8u       /* 16 step buttons: 8..23 */
#define RI_SLEVI_KEY0 24u       /* 13 pitch keys: 24..36 = C..C+ */
#define RI_SLEVI_KEYS 13u
#define RI_SLEVI_MIDDLE_C 60u

struct RISectLevi {
    uint8_t section;           /* RI_SEC_LEVI */
    uint8_t sel;               /* selected lane 0..5 */
    uint8_t edit_step;         /* 0..15 */
    uint8_t opsel;             /* selected operator 0..7 */
    int16_t val[RI_SLEVI_NCTL];
    uint8_t opmode[RI_LEVI_NOPS];      /* panel truth per op (engine follows) */
    struct RIPattern pat;      /* chord kind, class Levi */
};

int ri_slevi_init(struct RISectLevi *s);
int ri_slevi_press(struct RISectLevi *s, uint32_t idx);
int ri_slevi_set_value(struct RISectLevi *s, uint32_t idx, int v);
int ri_slevi_reset(struct RISectLevi *s, uint32_t idx);
/* Step LED: selected lane sounding; key LED: pitch match at
 * (edit step, selected lane). */
int ri_slevi_led(const struct RISectLevi *s, uint32_t idx);
/* EDIT STEP 1..16. */
int ri_slevi_display(const struct RISectLevi *s);
/* Central algorithm readout 1..8 (panel truth); 0 on NULL. */
int ri_slevi_algo_display(const struct RISectLevi *s);

#endif
