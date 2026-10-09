/* gui/miditrans.h — MIDI clock to transport (M3 interop, R1 applied).
 * Pure C, host-tested. Applies follower intents to the panel transport
 * (the engine follows through the existing sync path — no second
 * setter route) and computes the followed session tempo with a phase
 * servo so long-term drift is zero. The caller owns the tempo knob
 * read-only law (secttr tempo_lock) and the TAP disable from the
 * locked flag here. Only acts when the sync source is MIDI (M3 is
 * opt-in; Internal behavior is untouched).
 */
#ifndef RI_MIDITRANS_H
#define RI_MIDITRANS_H
#include <stdint.h>
#include "midi_io/midi_follow.h"

struct RISectUI;
struct RIFollowIntent;
struct RIFollowSync;

/* Phase servo (E0, ledgered): session tempo trims toward the clock by
 * this many BPM per engine tick of phase error, bounded by the max. At
 * 0.01 a one-bar jump recovers in ~2 min; normal-op trims stay micro. */
#define RI_MTRANS_TRIM_BPM_PER_TICK 0.01f
#define RI_MTRANS_TRIM_MAX_BPM 2.0f
/* Lock deadline (E0, ledgered): the take counts as locked within this
 * many beats of steady clock (the follower needs just over one). */
#define RI_MTRANS_LOCK_BEATS 2u

struct RIMidiTrans {
    struct RIFollowSync sync; /* M1: source/following/held tempo */
    uint8_t playing;          /* take running per applied intents */
    uint8_t pad[3];
    int64_t expected;         /* engine ticks due at the next clock */
    int16_t lat_ms;           /* E0 latency offset (M2 setting) */
    int16_t pad2;
};

void midi_trans_init(struct RIMidiTrans *t);
void midi_trans_set_lat(struct RIMidiTrans *t, int16_t ms);
void midi_trans_set_source(struct RIMidiTrans *t, uint32_t source);
/* One follower intent onto the panel transport (cursor owned by the
 * caller): START plays from song start (+latency), CONTINUE plays
 * keeping the cursor, STOP stops (tempo held per R1), SEEK locates
 * (stopped only, per the M1 law). Ignored unless sync source is MIDI. */
void midi_trans_apply(struct RIMidiTrans *t, const struct RIFollowIntent *it,
    struct RISectUI *tru, uint64_t *cursor);
/* Per block: nf8 clocks observed, follower lock + BPM, current panel
 * cursor. Returns 0 while internal (knob owns tempo), else the
 * effective session tempo (measured + phase trim). Updates the sync
 * latch, so knob_locked/tempo display follow it. */
float midi_trans_tempo(struct RIMidiTrans *t, struct RISectUI *tru,
    uint32_t nf8, uint32_t locked, float bpm, uint64_t cursor);
/* 1 when the panel tempo control must show measured + ignore edits. */
int midi_trans_tempo_locked(const struct RIMidiTrans *t);
const struct RIFollowSync *midi_trans_sync(const struct RIMidiTrans *t);
#endif
