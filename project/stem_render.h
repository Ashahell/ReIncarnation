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

/* THE FILE NAME FOR A STEM: "<index>-<name>.wav".
 *
 * SECTIONS AND FX SHARE ONE NUMBERING, AND THAT IS THE POINT. The sections
 * keep their own indices (1..5, by section, so a gap says which sections the
 * song did not address) and the FX buses continue after them, so a file list
 * sorts into strip order then effect order with nothing interleaved. R8f.
 *
 * OWNER DECISION 2026-10-10: A DIRECTORY PER SONG, so the files inside it do
 * not repeat the song name -- the directory already carries it.
 *
 * The index comes FIRST so a DAW's file list sorts into strip order, and the
 * two-digit form means a gap is visible at a glance: stems are numbered by
 * SECTION, not by running count, so a song that renders 303A and the 808 is
 * 01-303a and 03-808.wav. The gap is information, not an error -- it says
 * which sections the song did not address.
 *
 * Returns bytes written excluding the NUL, or 0 for any refusal: a NULL or
 * too-small buffer, a section above RI_ROUTE_NSECTIONS, or a section with no
 * name. An unnamed section is refused rather than numbered blankly, because
 * "02-.wav" in a folder the user is about to trust is worse than no file. */
uint32_t ri_stemr_name(char *out, uint32_t cap, uint32_t index,
    uint32_t section);

/* THE FX BUSES CONTINUE THE SECTION NUMBERING: `06-fx-send.wav` and
 * `07-fx-return.wav`. They are NOT sections and get no section index of their
 * own, because the tap's first two arrays are indexed by section and
 * `stem_render` names files from that array -- anything else in that index
 * space would be a file named after the wrong machine.
 *
 * `index` is the CALLER's to pass (6 and 7), rather than a function of the
 * bus, so that adding an FX bus later does not renumber the ones before it. */
uint32_t ri_stemr_fx_name(char *out, uint32_t cap, uint32_t index,
    uint32_t fx);

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
    /* SECTION -> SET INDEX. The set appends stems in section order, so its
     * index is a RUNNING COUNT, not a section number: a song addressing
     * sections 0, 2 and 4 produces set entries 0, 1 and 2. Asking
     * `stem_set_write` for section 2 writes the 808's bytes under the 808's
     * name -- correct by luck when every section is present, and silently
     * wrong the moment one is missing. The mapping lives HERE because section
     * identity is this module's, not the caller's and not the WAV writer's. */
    uint32_t set_of[RI_ROUTE_NSECTIONS];
    float *outs[RI_ROUTE_NSECTIONS]; /* caller interleaved buffers */
    /* R8f: the two FX buses, and where their stems landed in the SET. Same
     * trap as `set_of`: the set appends, so a set index is a running count
     * and NOT a section number. Kept beside `set_of` rather than in a
     * parallel array because it is the same question. */
    float *fx[RI_STEM_FX_N];
    /* THE FX STEMS' OWN INTERLEAVE BUFFER, THE CALLER'S. It is NOT a
     * section's, and borrowing one is a real bug rather than a saving: the
     * set stores POINTERS, so writing an FX stem through the 303A's
     * interleave buffer overwrites the 303A's audio before it is written
     * out. My first version looked slot 0's buffer up and the 303A stem came
     * out silent -- the same aliasing family as R8g's merge, one layer up.
     * Two buses need 2 * frames * 2 floats of their own. */
    float *fx_out;
    uint32_t fx_set_of[RI_STEM_FX_N];
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

/* The set index holding SECTION's stem, or `RI_STEMR_NO_STEM` if that
 * section produced none. Call it after `ri_stemr_finish`. */
#define RI_STEMR_NO_STEM 0xFFFFFFFFu
uint32_t ri_stemr_set_index(const struct RIStemr *r, uint32_t section);

/* Tell the bridge where an FX bus's mono buffer is. Must be called after
 * `ri_stemr_begin` and before `ri_stemr_bind`, which zeroes them. A NULL
 * means "not asked for" and is the state every existing caller is in. */
void ri_stemr_fx_bind(struct RIStemr *r, uint32_t fx, float *buf);

/* Where the FX stems interleave. MUST be the caller's own buffer of
 * `frames * 2` floats, never a section slot's. NULL means "no FX stems" and
 * is what every existing caller passes. */
void ri_stemr_fx_out(struct RIStemr *r, float *buf);

/* The set index holding FX bus `fx`'s stem, or `RI_STEMR_NO_STEM`. */
uint32_t ri_stemr_fx_set_index(const struct RIStemr *r, uint32_t fx);

#endif