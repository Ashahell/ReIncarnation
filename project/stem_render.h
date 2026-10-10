/* project/stem_render.h — R8d: the bridge from the engine's tap to the
 * stem set. Pure C, host-tested, no allocation, no IO.
 *
 * R8a built the WAV writer (`stem_wav`), R8c built the engine's note tap
 * (`RIStemTap`, t204). What is between them is this module, and it is small
 * on purpose: it hands the tap a whole render's worth of caller buffers,
 * zeroes them, and on completion interleaves each strip and gives it to
 * `RIStemSet`.
 *
 * NOTHING IS ALLOCATED AND NOTHING IS OWNED. The caller supplies every
 * buffer and keeps them alive: `RIStemSet` stores the POINTER, so an
 * interleaved buffer that dies before `stem_set_write` is a dangling read in
 * a file the user is about to trust.
 *
 * THE LAWS, which are all about a render that does not go to the end:
 *
 *  - **THE BUFFERS ARE ZEROED BY `ri_stemr_bind`, ALWAYS.** The tap writes
 *    only the samples the engine actually renders. A render that stops short
 *    — out-of-order events (t205), a song shorter than the buffer — leaves
 *    the tail holding whatever the caller had there, and in a stem file that
 *    is not silence, it is the last song's audio. Zeroing is the module's job
 *    precisely because the caller will not remember, and "it worked in the
 *    test" is how that survives.
 *  - **A SHORT OR OVER-LONG RENDER IS REFUSED, NOT PADDED.** `ri_stemr_finish`
 *    takes what the engine actually rendered and refuses anything that is
 *    not the frame count asked for, counting it. Handing the set a
 *    full-length stem anyway would put silence at the end of a file the
 *    caller believes is complete.
 *  - **ONE STEM PER RENDERED SECTION, IN SECTION ORDER.** The section mask
 *    is read off the engine at `ri_stemr_bind`, so a slot offered for a
 *    section the song has switched off does NOT become a stem — a file of
 *    silence labelled as an instrument. Stem N is then the same section on
 *    every run.
 *  - **INTERLEAVED, NOT TWO MONO FILES.** `stem_wav_write` reads
 *    `samples[i * channels + c]`, so a stereo strip has to be interleaved and
 *    this is the only place that copy happens.
 */
#ifndef RI_STEMRENDER_H
#define RI_STEMRENDER_H
#include <stdint.h>
#include "engine/engine.h"
#include "project/stem_wav.h"

/* One mixer section's buffers. All three are caller-owned and must outlive
 * the `stem_set_write` calls. Each holds at least `frames` samples, and
 * `out` at least `frames * 2` because it is interleaved. */
struct RIStemrSlot {
    float *l;    /* mono left scratch, filled by the engine's tap */
    float *r;    /* mono right scratch */
    float *out;  /* interleaved stereo output, read by stem_set_add */
};

struct RIStemr {
    struct RIStemSet set;      /* the stems, ready for stem_set_write */
    struct RIStemTap tap;      /* what gets handed to the engine */
    uint32_t frames;           /* what was asked for */
    uint32_t rate;
    uint32_t depth;
    uint8_t nstems;            /* slots offered at begin */
    uint8_t bound;             /* 1 once ri_stemr_bind has zeroed and attached */
    uint8_t finished;          /* 1 once finish has succeeded */
    uint8_t pad;
    uint32_t sections;         /* the engine's mask at bind time */
    float *outs[RI_ROUTE_NSECTIONS]; /* caller interleaved buffers */
    uint32_t short_count;      /* renders refused for not matching `frames` */
};

/* Set up. `nstems` is 1..RI_ROUTE_NSECTIONS and `slots[nstems]` must be
 * filled. Returns 0, or 1 for any refusal: NULL, zero sections, more
 * sections than there are, zero frames, a zero rate, an unsupported bit
 * depth, or a slot missing any of its three buffers.
 *
 * Refuses rather than clamping a bit depth, because the caller's file format
 * is not this module's to choose. */
int ri_stemr_begin(struct RIStemr *r, uint32_t nstems, uint32_t frames,
    uint32_t rate, uint32_t depth, uint32_t bpm_milli,
    const struct RIStemrSlot *slots);

/* Zero every buffer and attach the tap to `e`. Call it BEFORE the first
 * `ri_engine_render`. Safe to call again (it re-zeros and re-attaches). */
void ri_stemr_bind(struct RIStemr *r, struct RIEngine *e);

/* Interleave each strip and hand it to the set. `rendered` is what the
 * engine returned. Returns 0 only when `rendered == frames` and the set is
 * then ready for `stem_set_write`; anything else is refused and counted in
 * `short_count`, with nothing added to the set. */
uint32_t ri_stemr_finish(struct RIStemr *r, uint32_t rendered);

#endif