/* gui/ctlreg.h — ReBirth 2.0.1 panel control registry (§12.10 G1).
 * Pure C (no MUI): the single source for every front-panel control — its
 * section, instrument group, ReBirth legend, widget kind, range, default,
 * Standard MIDI Mapping controller number (Owner's Manual Appendix C,
 * p. 195-198), Song-automation flag (p. 72-73) and engine binding.
 * A control the engine cannot honour yet is RI_BIND_NONE: the GUI shows it
 * disabled, it never becomes a silent no-op (review §10 item 2).
 * Plan: docs/superpowers/plans/2026-09-25-gui-parity-plan.md (G1).
 */
#ifndef RI_CTLREG_H
#define RI_CTLREG_H
#include <stdint.h>

/* Sections (registry id = section << 8 | index). */
#define RI_SEC_SYNTH1 0u
#define RI_SEC_SYNTH2 1u
#define RI_SEC_808 2u
#define RI_SEC_909 3u
#define RI_SEC_MIX_SYNTH1 4u
#define RI_SEC_MIX_SYNTH2 5u
#define RI_SEC_MIX_808 6u
#define RI_SEC_MIX_909 7u
#define RI_SEC_MASTER 8u
#define RI_SEC_PCF 9u
#define RI_SEC_DELAY 10u
#define RI_SEC_DIST 11u
#define RI_SEC_COMP 12u
#define RI_SEC_TRANSPORT 13u
#define RI_SEC_PAT_SYNTH1 14u
#define RI_SEC_PAT_SYNTH2 15u
#define RI_SEC_PAT_808 16u
#define RI_SEC_PAT_909 17u
#define RI_SEC_COUNT 18u

/* Widget kinds. Only KNOB/FADER/SWITCH/SELECTOR are value controls that
 * can carry a MIDI controller or Song automation. */
#define RI_CK_KNOB 0u
#define RI_CK_FADER 1u
#define RI_CK_SWITCH 2u   /* n-state latching switch / on-off button */
#define RI_CK_BUTTON 3u   /* momentary */
#define RI_CK_LED 4u
#define RI_CK_STEP 5u     /* one of the 16 step buttons (value = step state) */
#define RI_CK_SELECTOR 6u /* n-position rotary / radio group */
#define RI_CK_DISPLAY 7u  /* numeric readout (+ arrow buttons) */
#define RI_CK_METER 8u

/* Engine bindings. */
#define RI_BIND_NONE 0u    /* not honoured by the engine yet: render disabled */
#define RI_BIND_303 1u     /* engine_id = 0x030x / 0x031x (rb303_set_param path) */
#define RI_BIND_808V 2u    /* rb808_set_param(voice, engine_id) */
#define RI_BIND_808ALL 3u  /* rb808_set_param on every 808 voice */
#define RI_BIND_909V 4u    /* rb909_set_param(voice, engine_id) */
#define RI_BIND_909HAT 5u  /* rb909_set_hat_level */
#define RI_BIND_FX 6u      /* ri_engine_fx_set(engine_id) */
#define RI_BIND_PAN 7u     /* ri_engine_set_pan(voice = route section) */
#define RI_BIND_SEND 8u    /* ri_engine_set_send(voice = route section) */
#define RI_BIND_INSERT 9u  /* ri_engine_assign_insert(engine_id = unit, voice = owner) */
#define RI_BIND_TEMPO 10u  /* ri_engine_set_tempo */

#define RI_MIDI_CC_NONE 0xFFu

struct RICtlDef {
    uint16_t reg_id;      /* section << 8 | index, stable */
    uint8_t section;      /* RI_SEC_* */
    uint8_t kind;         /* RI_CK_* */
    const char *group;    /* instrument/row legend ("BD", "Mixer", ...) or "" */
    const char *legend;   /* ReBirth control legend */
    int16_t min_v, max_v, def_v;
    uint8_t midi_cc;      /* Appendix C controller, RI_MIDI_CC_NONE if none */
    uint8_t automatable;  /* 1 = recordable in Song mode */
    uint8_t bind;         /* RI_BIND_* */
    uint8_t pad;
    uint16_t engine_id;   /* control / FX id / insert unit, per bind */
    uint16_t voice;       /* engine voice / route section / insert owner */
};

uint32_t ri_ctlreg_count(void);
const struct RICtlDef *ri_ctlreg_at(uint32_t i);           /* NULL out of range */
const struct RICtlDef *ri_ctlreg_find(uint16_t reg_id);    /* NULL if unknown */
const struct RICtlDef *ri_ctlreg_by_cc(uint8_t cc);        /* NULL if unmapped */
const char *ri_ctlreg_section_name(uint32_t section);      /* "Synth 1", ... */
/* Per-section totals: controls, and how many are engine-bound. */
uint32_t ri_ctlreg_section_count(uint32_t section, uint32_t *bound);
#endif
