/* songsteps.h — song-to-steps converter, WBS 2.1 builder step 1.
 * Bridges parsed songs (project/rbng.h RISong, real codec Task 13) to
 * the walker input (engine/seq/sched.h RIStep). Layout contract:
 * RBSongStep.{note,flags} == RIStep.{note,flags} bit-for-bit
 * (rbng.h documents RI_RBNG_* == RI_STEP_*; pinned by t1_formats §16),
 * so conversion is a bounded copy — no interpretation. REST steps pass
 * through (the walker owns gate/slide semantics); automation/mods are
 * NOT steps (later: AUTOMATION events / mod refs).
 */
#ifndef RI_SONGSTEPS_H
#define RI_SONGSTEPS_H
#include <stdint.h>

struct RISong; /* project/rbng.h (opaque here; header-only dep) */
struct RIStep; /* engine/seq/sched.h */

/* Copy min(nsteps, cap) steps. Returns count. NULL song/out or cap 0
 * yields 0. Pure, no allocation, no IO. */
uint32_t ri_song_to_steps(const struct RISong *song, struct RIStep *out,
                          uint32_t cap);
#endif
