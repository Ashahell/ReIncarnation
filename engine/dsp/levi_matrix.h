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
#define RI_LEVI_MS_NOTE 16u   /* keytrack, bipolar (note-60)/60 */
#define RI_LEVI_MS_N 17u      /* ids 13..15 reserved, 16 = note */

/* Destinations (normalized offsets; apply scales per param). */
#define RI_LEVI_MD_CUTOFF 0u
#define RI_LEVI_MD_RESO 1u
#define RI_LEVI_MD_DRIVE 2u
#define RI_LEVI_MD_OPLEVEL 3u
#define RI_LEVI_MD_VLEVEL 4u
#define RI_LEVI_MD_MORPH 5u
#define RI_LEVI_MD_N 6u

#define RI_LEVI_MX_NSLOTS 32u

struct RILeviMxSlot {
    uint8_t src;   /* RI_LEVI_MS_* */
    uint8_t dst;   /* RI_LEVI_MD_* */
    int8_t depth;  /* -100..+100 % */
    uint8_t on;    /* gate (route switches bind here in 4b) */
};

struct RILeviMatrix {
    struct RILeviMxSlot slot[RI_LEVI_MX_NSLOTS];
};

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

#endif
