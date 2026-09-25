/* pattern.h — per-instance pattern model (spec 2026-09-25 §1). */
#ifndef RI_PATTERN_H
#define RI_PATTERN_H
#include <stdint.h>

#define RI_PATTERN_STEPS           16u   /* storage rows, always 16 (p. 53-54) */
#define RI_PATTERN_BANKS            4u   /* A-D (p. 147) */
#define RI_PATTERN_PER_BANK         8u
#define RI_PATTERN_BANK_PATTERNS   32u
#define RI_PATTERN_KIND_303         0u
#define RI_PATTERN_KIND_DRUM        1u
#define RI_PATTERN_PAYLOAD_VERSION  1u

/* 303 (spec §1.1) */
#define RI_303_KEYS        13u   /* low C .. high C */
#define RI_303_BASE_NOTE   36u   /* E0: docs/evidence/sequencer/303-base-note.md */
#define RI_303_SEMI_MIN   (-12)  /* Down + key 0 */
#define RI_303_SEMI_MAX     24   /* Up + key 12 */

/* Drum (spec §1.2) */
#define RI_DRUM_CLASS_808  0u
#define RI_DRUM_CLASS_909  1u
#define RI_DRUM_LANES     16u
#define RI_DRUM_CLASSIC_LANES 11u
#define RI_DRUM_LANE_MASK_CLASSIC 0x07FFu
#define RI_DRUM_AC        0x01u  /* RIDrumRow.flags: global accent row */

#include "engine/seq/sched.h"
#include "engine/seq/clock.h"
#include "engine/dsp/rb808.h"
#include "engine/dsp/rb909.h"

/* Canonical lane order = ReBirth tap-record / panel order (p. 32) */
enum { RI_L808_BD, RI_L808_SD, RI_L808_LT, RI_L808_MT, RI_L808_HT, RI_L808_RS,
       RI_L808_CP, RI_L808_CB, RI_L808_CY, RI_L808_OH, RI_L808_CH };
enum { RI_L909_BD, RI_L909_SD, RI_L909_LT, RI_L909_MT, RI_L909_HT, RI_L909_RS,
       RI_L909_CP, RI_L909_CH, RI_L909_OH, RI_L909_CC, RI_L909_RC };

struct RI303Row  { uint8_t key; uint8_t flags; };           /* RI_STEP_* */
struct RIDrumRow { uint16_t on, high, flam; uint8_t flags; uint8_t pad; };

struct RIPattern {
    uint8_t kind, length, payload_ver, drum_class;
    union { struct RI303Row r303[RI_PATTERN_STEPS];
            struct RIDrumRow drum[RI_PATTERN_STEPS]; } row;
};

struct RIPatternBank {
    uint8_t instance, kind, drum_class, pad;
    struct RIPattern pat[RI_PATTERN_BANK_PATTERNS];
};

/* Lane -> engine voice id (hosting uses these; 808 lane == rb808 slot). */
extern const uint8_t RI_LANE_TO_RB808_SLOT[RI_DRUM_CLASSIC_LANES];
extern const uint8_t RI_LANE_TO_RB909_VOICE[RI_DRUM_CLASSIC_LANES];

void ri_pattern_init(struct RIPattern *p, uint8_t kind, uint8_t drum_class);
void ri_bank_init(struct RIPatternBank *b, uint8_t instance, uint8_t kind,
                  uint8_t drum_class);
int  ri_pattern_set_length(struct RIPattern *p, uint32_t len);  /* clamps 1..16, rc 0 */
int  ri_pattern_valid(const struct RIPattern *p);               /* 0 ok, 2 bad */
/* 303 step setters: rc 2 on wrong kind / step >= 16 / key > 12 */
int  ri_p303_set(struct RIPattern *p, uint32_t step, uint8_t key, uint8_t flags);
/* Drum: set one lane state; state = RI_HIT_OFF/LOW/HIGH/FLAM */
#define RI_HIT_OFF  0u
#define RI_HIT_LOW  1u
#define RI_HIT_HIGH 2u
#define RI_HIT_FLAM 3u
int  ri_pdrum_set(struct RIPattern *p, uint32_t step, uint32_t lane, uint32_t state);
uint32_t ri_pdrum_get(const struct RIPattern *p, uint32_t step, uint32_t lane);
int  ri_pdrum_set_ac(struct RIPattern *p, uint32_t step, int on);
/* ReBirth step-button click (p. 30): 909 off->low->high->off, flam mode
 * off<->flam; 808 off<->on. Returns the new state. */
uint32_t ri_pdrum_click(struct RIPattern *p, uint32_t step, uint32_t lane, int flam_mode);
/* Task 3: normalized octave (+1 Up, -1 Down, 0 both/neither); semitone
 * relative to RI_303_BASE_NOTE; MIDI note; fold into [-12, 24];
 * canonical encode (never Up+Down). */
int ri_p303_octave(uint8_t flags);
int ri_p303_semi(const struct RI303Row *r);
uint8_t ri_p303_note(const struct RI303Row *r);
int ri_p303_fold(int semi, int *folded);
void ri_p303_encode(int semi, uint8_t *key, uint8_t *octflags);
/* Task 4: ReBirth Edit menu ops (p. 51-54, spec §3). All return 0 ok,
 * 2 refused with no state change. */
int ri_pattern_clear(struct RIPattern *p);
int ri_bank_copy(const struct RIPatternBank *src, uint32_t sslot,
    struct RIPatternBank *dst, uint32_t dslot); /* same kind+class */
int ri_bank_cut(struct RIPatternBank *b, uint32_t slot,
    struct RIPattern *clip);
int ri_bank_paste(struct RIPatternBank *b, uint32_t slot,
    const struct RIPattern *clip);
int ri_pattern_shift(struct RIPattern *p, int dir); /* -1 left, +1 right */
int ri_pdrum_shift_lane(struct RIPattern *p, uint32_t lane, int dir);
int ri_p303_transpose(struct RIPattern *p, int semis, uint32_t *nfolded);
#define RI_RND_PATTERN  0u
#define RI_RND_PITCHES  1u
#define RI_RND_ACCENTS  2u  /* "Accents etc.": Note/Pause, Accent, Slide, Octave */
int ri_p303_random(struct RIPattern *p, uint32_t what, uint32_t seed);
int ri_p303_alter(struct RIPattern *p, uint32_t what, uint32_t seed);
int ri_pdrum_random_lane(struct RIPattern *p, uint32_t lane, uint32_t seed);
int ri_pdrum_alter_lane(struct RIPattern *p, uint32_t lane, uint32_t seed);
/* Pattern -> events (Tasks 6-7). 303 rows expand through the shared
 * timed-emit loop (cyclic seam via carry); drum rows emit per-lane
 * one-shots. Hosting contract (engine slice): NOTE_ON + ACCENT flag ->
 * rb909_trigger(accent=1); RI_EV_FLAM -> rb909_arm_flam(width). The
 * accent == 2 overload must never come back. */
uint32_t ri_pattern303_to_steps(const struct RIPattern *p, int cyclic,
    struct RIStep out[RI_PATTERN_STEPS]);
uint32_t ri_sched_emit_pattern(const struct RIPattern *p, uint16_t device,
    const struct RITempoMap *map, uint64_t start_tick, uint32_t ppq,
    const struct RISchedOpts *opts,
    const struct RISchedCarry *carry_in, struct RISchedCarry *carry_out,
    struct RIEvent *out, uint32_t cap);
#endif
