/* riseq.h — master clock + event-sequencer object (WBS 2.1, TC-2.1.1).
 * WBS interface ADAPTED: RiSeqCreate is RiSeqInit here — Phase-0a bans
 * heap allocation in engine/ (render-path hygiene), so the caller owns
 * the struct (static/stack) and init never allocates.
 * RiSeqAdvanceFrames is executor-defined (the render side must advance
 * the clock; no other mutator exists).
 * Rate: the seq runs at locked RI_AUDIO_SR (48000); device-rate
 * conversion lives at the backend (later task). The owned tempo map is
 * the bring-up default (single segment, 120 BPM); tempo loading arrives
 * with snapshots (TC-2.1.3). ao is carried opaquely for future backend
 * binding and may be NULL on host/CI (engine defaults apply).
 * Hard rules (WBS): no alloc/lock in render path; deterministic.
 */
#ifndef RI_RISEQ_H
#define RI_RISEQ_H
#include <stdint.h>
#include "engine/seq/clock.h"
#include "engine/seq/sched.h"

struct AudioObject; /* audio_io/audio.h (opaque here; no link dependency) */

/* Immutable event list + loop region (WBS 2.1). Storage is caller-owned
 * (no allocation, Phase-0a); LoadSnapshot stores the pointer (one
 * pointer store — safe to call from the GUI side between buffers).
 * loop_end <= loop_start disables looping (passthrough). */
struct RISeqSnapshot {
    const struct RIEvent *events;
    uint32_t n_events;
    uint64_t loop_start; /* inclusive, samples */
    uint64_t loop_end;   /* exclusive, samples */
    uint64_t length;     /* song length, samples */
};

struct RISeq {
    struct AudioObject *ao;
    uint32_t ppq;
    uint64_t samples;
    struct RISegment seg;
    struct RITempoMap map;
    const struct RISeqSnapshot *snap;    /* active (render side) */
    const struct RISeqSnapshot *pending; /* staged (GUI side) */
};

void RiSeqInit(struct RISeq *s, struct AudioObject *ao, uint32_t ppq);
void RiSeqAdvanceFrames(struct RISeq *s, uint32_t frames); /* render-side */
uint64_t RiSeqMasterClock(const struct RISeq *s);          /* samples */
void RiSeqLoadSnapshot(struct RISeq *s, const struct RISeqSnapshot *snap);
void RiSeqRequestSnapshot(struct RISeq *s, const struct RISeqSnapshot *snap);
const struct RISeqSnapshot *RiSeqBeginBuffer(struct RISeq *s);
/* Staged swap (TC-2.1.3): Request stages (GUI side, pointer store);
 * BeginBuffer applies staged->active at the next buffer start (render
 * side) and returns the active snapshot (NULL if none ever set).
 * Protocol contract: Request only between buffers; single-writer per
 * side — the store/load pair is lock-free by construction. */
/* Loop cursor (XX): maps MasterClock into [loop_start, loop_end);
 * *iteration counts completed wraps (0-based pass index); pre-loop and
 * no/degenerate snapshot return the raw clock with iteration 0. */
uint64_t RiSeqLoopPos(const struct RISeq *s, uint64_t *iteration);

/* Per-buffer event window (XX, playback side): copy events with
 * s0 <= sample < s1 into out (input order preserved — caller keeps
 * lists sorted; walker output is sorted by contract), up to cap.
 * Returns count. NULL/empty/degenerate (s1 <= s0) yields 0. Pure. */
uint32_t ri_events_in_window(const struct RIEvent *ev, uint32_t n,
    uint64_t s0, uint64_t s1, struct RIEvent *out, uint32_t cap);
#endif
