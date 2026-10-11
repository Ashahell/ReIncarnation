/* project/song_sections.h — R8g: a song that addresses more than one section.
 *
 * Until now `tools/render --stems` could only ever write ONE stem, and the
 * reason was never the stem code: `ri_sched_emit_timed` takes a device as an
 * ARGUMENT and emits for one section, so every event in a render carried
 * device 0 and `render_stems` derived a set of exactly one. I had recorded
 * that as "needs a song format with a per-step device, an engine data-model
 * change". That was the wrong conclusion: **a section is a property of the
 * STEP LIST you hand the emitter, not of an individual step.** `RIStep` needs
 * no device field, and nothing in the engine changes. A multi-section song is
 * several step lists, each emitted for its own device, merged.
 *
 * **THE MERGE IS THE INTERESTING PART, AND A PER-DEVICE SORT IS NOT ENOUGH.**
 * `ri_sched_emit_timed` returns one device's run already sorted by the §8
 * key. Two sorted runs are not one sorted run, and the engine walks the
 * array in order -- it STOPS on an out-of-order event and counts it (t205).
 * So the merge is by the same key:
 *
 *     sample, then type, then device, then voice, then seq
 *
 * `device` being in the key is what makes this work: both sections start at
 * seq 0, so two events at the same sample with the same type are ordered by
 * their section, which is exactly what a 303 + 808 pattern produces at every
 * downbeat. A merge that sorted by sample alone would tie there, and an
 * unstable sort would resolve the tie differently between runs.
 *
 * **EACH DEVICE KEEPS ITS OWN seq SPACE.** Renumbering one device's seq into
 * another's would make two DIFFERENT events compare equal under the key, and
 * then their order is whatever the sort happened to do.
 *
 * Pure: no allocation, no IO. It produces events; the caller renders.
 */
#ifndef RI_SONGSECTIONS_H
#define RI_SONGSECTIONS_H
#include <stdint.h>
#include "engine/fx/route.h"      /* RI_ROUTE_NSECTIONS */
#include "engine/seq/clock.h"
#include "engine/seq/sched.h"

/* One section's step list. `device` must be < RI_ROUTE_NSECTIONS; a device
 * with no steps is skipped rather than being an error, because a song with a
 * section switched off is a normal thing to have. */
struct RISectionRun {
    const struct RIStep *steps;
    uint32_t nsteps;
    uint16_t device;
};

/* THE MERGE ITSELF, exposed because it is the law the whole module rests on
 * and it is not otherwise testable at a tie: two events at the same sample
 * with different types only arise from specific step patterns, and a test
 * that hunts for one is a test that breaks when the fixture changes.
 *
 * `out` must not alias `a` or `b`. It does NOT check -- an aliasing merge
 * silently loses events, and a check would cost a scan of the whole run to
 * prevent a caller's mistake. Returns how many were written, stopping at
 * `cap`. */
uint32_t ri_song_merge(struct RIEvent *out, uint32_t cap, const struct RIEvent *a,
    uint32_t na, const struct RIEvent *b, uint32_t nb);

/* The OR of `1 << device` over every run with steps. Zero if any device is
 * out of range -- a caller whose run names a section that does not exist has
 * a bug, and returning a bit for it would hand the engine a mask it cannot
 * honour. */
uint32_t ri_song_sections(const struct RISectionRun *runs, uint32_t n);

/* Emit every run and MERGE the results into one §8-ordered array.
 *
 * Returns the number of events written, or 0 for any refusal: NULL map,
 * runs, or out; a ppq of 0; a zero capacity; no runs with steps; or a device
 * past the last section.
 *
 * A `cap` smaller than the song TRUNCATES and reports what fitted. It does
 * not overwrite and does not silently drop the rest: a caller that gets 300
 * of 900 events can see the shortfall, and a caller that gets 900 of 900
 * cannot be wrong about which one it has. */
uint32_t ri_song_emit(const struct RITempoMap *map, uint32_t ppq,
    const struct RISectionRun *runs, uint32_t n,
    const struct RISchedOpts *opts, struct RIEvent *out, uint32_t cap);

/* The same, at a start tick. This exists rather than a caller emitting at a
 * tick and adding the offset to the samples afterwards, because the start
 * tick is where the scheduler's ONE rounding happens: shifting finished
 * sample positions is a second answer to the ticks-to-samples question, and
 * at some tempos the two disagree by a sample per bar. */
uint32_t ri_song_emit_at(const struct RITempoMap *map, uint32_t ppq,
    const struct RISectionRun *runs, uint32_t n,
    const struct RISchedOpts *opts, struct RIEvent *out, uint32_t cap,
    uint64_t start_tick);

#endif
