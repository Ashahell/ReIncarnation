/* levi_matrix.h — Levi mod-matrix slot core (v2 feature 4a, owner order).
 * Own slot model (clean-room: arrangement/function from the manual, no
 * ASM content). Sources today: per-operator contours (spec: "operator
 * contours as sources") + keytrack note; LFO/macro/velocity slots are
 * reserved in the id space for their slices. Pure C, host-tested;
 * evaluation sums normalized offsets per destination (apply-site
 * scaling + clamping belongs to the render hook, slice 4b).
 */
#ifndef RI_LEVI_MATRIX_H
#define RI_LEVI_MATRIX_H
#include <stdint.h>

/* Sources (own ids; velocity + 2 macro reserved at 13..15). */
#define RI_LEVI_MS_OPENV0 0u
#define RI_LEVI_MS_OPENV1 1u
#define RI_LEVI_MS_OPENV2 2u
#define RI_LEVI_MS_OPENV3 3u
#define RI_LEVI_MS_OPENV4 4u
#define RI_LEVI_MS_OPENV5 5u
#define RI_LEVI_MS_OPENV6 6u
#define RI_LEVI_MS_OPENV7 7u
#define RI_LEVI_MS_LFO0 8u
#define RI_LEVI_MS_LFO1 9u
#define RI_LEVI_MS_LFO2 10u
#define RI_LEVI_MS_LFO3 11u
#define RI_LEVI_MS_LFO4 12u
#define RI_LEVI_MS_NOTE 16u   /* keytrack, bipolar (note-60)/60, C4 centre */
/* Fidelity P5b (manual p. 126 groups, own ids; 13..15 stay reserved). */
#define RI_LEVI_MS_ENV0 17u   /* ENV 1-5: 17..21 */
#define RI_LEVI_MS_LFOP0 22u  /* LFO 1+..5+ (unipolar): 22..26 */
#define RI_LEVI_MS_POLYAT 27u /* 27..42 read 0 until their data arrives (P6/P8/P9) */
#define RI_LEVI_MS_MONOAT 28u
#define RI_LEVI_MS_VELON 29u
#define RI_LEVI_MS_VELOFF 30u
#define RI_LEVI_MS_VMOD 31u
#define RI_LEVI_MS_VMODP 32u
#define RI_LEVI_MS_WHEEL 33u
#define RI_LEVI_MS_BEND 34u
#define RI_LEVI_MS_RBNABS 35u
#define RI_LEVI_MS_RBNABSP 36u
#define RI_LEVI_MS_RBNREL 37u
#define RI_LEVI_MS_MPEX 38u
#define RI_LEVI_MS_MPEYR 39u
#define RI_LEVI_MS_MPEYA 40u
#define RI_LEVI_MS_EXPPED 41u
#define RI_LEVI_MS_SUSPED 42u
#define RI_LEVI_MS_N 43u

/* Destinations (normalized offsets; apply scales per param). */
#define RI_LEVI_MD_CUTOFF 0u
#define RI_LEVI_MD_RESO 1u
#define RI_LEVI_MD_DRIVE 2u
#define RI_LEVI_MD_OPLEVEL 3u
#define RI_LEVI_MD_VLEVEL 4u
#define RI_LEVI_MD_MORPH 5u
#define RI_LEVI_MD_N 6u

#define RI_LEVI_MX_NSLOTS 32u

/* Destination modules (fidelity P5b, manual p. 127 groups, own ids):
 * a destination is module + parameter. */
#define RI_LEVI_DM_NONE 0u
#define RI_LEVI_DM_OSC1 1u     /* 1..8 OSC n, 9 all oscillators */
#define RI_LEVI_DM_ALLOSC 9u
#define RI_LEVI_DM_CARR 10u    /* carriers: init, env level */
#define RI_LEVI_DM_MODS 11u    /* modulators: init, env level */
#define RI_LEVI_DM_DFILT 12u
#define RI_LEVI_DM_AFILT 13u
#define RI_LEVI_DM_VCA 14u
#define RI_LEVI_DM_ENV1 15u    /* 15..19 */
#define RI_LEVI_DM_LFO1 20u    /* 20..24 */
#define RI_LEVI_DM_MTRX 25u    /* route depth 1..32 */
#define RI_LEVI_DM_MACRO 26u   /* macro 1..8 */
#define RI_LEVI_DM_ALGO 27u    /* algorithm morph */
#define RI_LEVI_DM_VOICE 28u   /* voice params (fidelity P6b) */
#define RI_LEVI_DM_DELAY 29u   /* delay params (fidelity P7a) */
#define RI_LEVI_DM_N 30u
/* DM_DELAY params. */
#define RI_LEVI_DD_TIME 0u
#define RI_LEVI_DD_FEEDBACK 1u
#define RI_LEVI_DD_WETTONE 2u
#define RI_LEVI_DD_FBTONE 3u
#define RI_LEVI_DD_DRYWET 4u
#define RI_LEVI_DD_N 5u
/* Voice params (P6b): detune, pan, analog feel, bend range, vibrato
 * amt/rate, glide toggle/time/curve, panner width. */
#define RI_LEVI_DVO_DETUNE 0u
#define RI_LEVI_DVO_PAN 1u
#define RI_LEVI_DVO_AFEEL 2u
#define RI_LEVI_DVO_BEND 3u
#define RI_LEVI_DVO_VIBAMT 4u
#define RI_LEVI_DVO_VIBRATE 5u
#define RI_LEVI_DVO_GLIDETGL 6u
#define RI_LEVI_DVO_GLTIME 7u
#define RI_LEVI_DVO_GLCURVE 8u
#define RI_LEVI_DVO_PANWIDTH 9u
#define RI_LEVI_DVO_N 10u
/* OSC params */
#define RI_LEVI_DO_INIT 0u
#define RI_LEVI_DO_ENVL 1u
#define RI_LEVI_DO_PITCH 2u
#define RI_LEVI_DO_RATIO 3u
#define RI_LEVI_DO_FINE 4u
#define RI_LEVI_DO_FB 5u
#define RI_LEVI_DO_PHASE 6u
#define RI_LEVI_DO_PAN 7u      /* inert until the stereo voice (P6) */
#define RI_LEVI_DO_WAVE 8u
#define RI_LEVI_DO_ATTACK 9u
#define RI_LEVI_DO_HOLD 10u
#define RI_LEVI_DO_DECAY 11u
#define RI_LEVI_DO_SUSTAIN 12u
#define RI_LEVI_DO_RELEASE 13u
#define RI_LEVI_DO_N 14u
/* D.FILTER: cutoff, reso, morph, drive, env1 amt, lfo1 amt, level;
 * A.FILTER: cutoff, reso, pre-drive, env2 amt, lfo2 amt; VCA: level,
 * lfo3 amt, oscs level; ENV: attack, hold, decay, sustain, release,
 * level, 3 curves; LFO: rate, level, smooth, steps. */
#define RI_LEVI_DE_ATTACK 0u
#define RI_LEVI_DE_HOLD 1u
#define RI_LEVI_DE_DECAY 2u
#define RI_LEVI_DE_SUSTAIN 3u
#define RI_LEVI_DE_RELEASE 4u
#define RI_LEVI_DE_LEVEL 5u
#define RI_LEVI_DE_ACURVE 6u
#define RI_LEVI_DE_DCURVE 7u
#define RI_LEVI_DE_RCURVE 8u
#define RI_LEVI_NMACRO 8u
#define RI_LEVI_MACRO_NR 8u

struct RILeviMxSlot {
    uint8_t src;   /* RI_LEVI_MS_* */
    uint8_t dst;   /* RI_LEVI_MD_* (v1 ids; kept in step with dmod/dpar) */
    int8_t depth;  /* -100..+100 % */
    uint8_t on;    /* gate (route switches bind here in 4b) */
    uint8_t dmod, dpar; /* destination module + parameter (P5b) */
    uint8_t pad[2];
};

struct RILeviMacroRoute {
    uint8_t dmod, dpar;
    int8_t depth;  /* -100..+100 % */
    uint8_t bval;  /* button value 0..127 */
};

struct RILeviMatrix {
    struct RILeviMxSlot slot[RI_LEVI_MX_NSLOTS];
    /* Macros (P5b, pp. 120-123): knob 0..127, button on = button value. */
    uint8_t mknob[RI_LEVI_NMACRO], mbtn[RI_LEVI_NMACRO];
    struct RILeviMacroRoute mroute[RI_LEVI_NMACRO][RI_LEVI_MACRO_NR];
};

/* One evaluated contribution: destination and signed amount (depth x
 * source, -1..1 full scale; apply sites scale per parameter). */
struct RILeviModOut {
    uint8_t dmod, dpar;
    uint8_t pad[2];
    float x;
};
#define RI_LEVI_MODOUT_MAX (RI_LEVI_MX_NSLOTS + RI_LEVI_NMACRO * RI_LEVI_MACRO_NR)

void ri_levi_matrix_init(struct RILeviMatrix *m);
/* Program one slot (depth -100..+100, enables the slot). 0 ok, 2 bad. */
int ri_levi_matrix_set(struct RILeviMatrix *m, uint32_t slot, uint32_t src,
    uint32_t dst, int depth);
/* Gate a slot without losing its program. 0 ok, 2 bad. */
int ri_levi_matrix_enable(struct RILeviMatrix *m, uint32_t slot, uint32_t on);
/* Sum normalized offsets per destination: openv[8] are the 0..1 op
 * contours, lfo5[5] the bipolar LFO values, note is MIDI (keytrack
 * law). dst_out[RI_LEVI_MD_N] zeroed first. 0 ok, 2 bad. */
int ri_levi_matrix_eval(const struct RILeviMatrix *m, const float *openv,
    const float *lfo5, uint32_t note, float *dst_out);
/* P5b: route slot to module/param (0 ok, 2 bad; dmod NONE clears). */
int ri_levi_matrix_route(struct RILeviMatrix *m, uint32_t slot, uint32_t src, uint32_t dmod, uint32_t dpar, int depth);
/* Parameters in a destination module (0 = not a module). */
uint32_t ri_levi_dm_nparam(uint32_t dmod);
/* Full evaluation (P5b): src[RI_LEVI_MS_N] source values; route depths
 * first take their Mod Matrix depth modulation, macros their macro
 * modulation. Writes up to RI_LEVI_MODOUT_MAX outputs; returns the count. */
uint32_t ri_levi_matrix_eval2(const struct RILeviMatrix *m, const float *src, struct RILeviModOut *out);
/* UI source list in the manual's group order (p. 126): 0 = none, then
 * ENV 1-5, LFO 1-5, LFO 1+-5+, OSC 1-8 ENV, keytrack, aftertouch,
 * velocity, voice mod, wheels, ribbon, MPE, pedals. Returns the source
 * id, or RI_LEVI_MS_N for none / out of range. */
#define RI_LEVI_MS_UI_N 41u
uint32_t ri_levi_ms_by_ui(uint32_t ui);
uint32_t ri_levi_ms_to_ui(uint32_t src);
/* Names (own, upper case, never NULL). */
const char *ri_levi_ms_name(uint32_t src);
const char *ri_levi_dm_name(uint32_t dmod);
const char *ri_levi_dp_name(uint32_t dmod, uint32_t dpar);

#endif
