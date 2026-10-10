/* project/smf_export.h — Standard MIDI File type 1 writer (R7).
 * Pure C, host-tested, no allocation, no IO, no float.
 *
 * WHY A WRITER AND NOT A CONVERTER. The engine already knows what a pattern
 * plays: `ri_sched_emit_sorted()` produces the events the internal synth
 * receives, with accent, slide, flam and device already resolved. An
 * exporter built on THAT has one authority for "what notes does this
 * pattern produce" -- if the SMF were built by re-deriving notes from the
 * pattern a second way, the file and the instrument would disagree
 * whenever the two derivations drifted, and nothing would say so.
 *
 * THE MAPPING IS AN OWNER REVIEW ITEM (interop spec R7). Accent to
 * velocity, slide to legato and flam to a second hit are conventions, not
 * facts. They are therefore isolated in `ri_smf_velocity()` and its
 * neighbours rather than scattered through the writer, and t193 pins them
 * only as deterministic and in range -- never as a chosen convention. The
 * defaults here are the conventional ones and are recorded, not defended.
 *
 * WHY THE ENCODING IS THE WHOLE JOB. An SMF has no schema and no error
 * reporting: a track chunk that overstates its length produces a file that
 * opens cleanly and plays nothing. Every law in t193 is one of those silent
 * failures.
 *
 * ONE THING TO GET RIGHT BEFORE ANYTHING ELSE, because I got it wrong:
 * **chunk lengths are FIXED 4-byte big-endian integers, not variable-length
 * quantities.** Only DELTA TIMES inside a track are VLQ. Writing the header
 * length as a VLQ yields an eleven-byte header whose MTrk magic lands where
 * the division field belongs -- a file that is not an SMF at all.
 */
#ifndef RI_SMF_EXPORT_H
#define RI_SMF_EXPORT_H
#include <stdint.h>
#include "engine/seq/sched.h"

/* Event types this writer consumes. Deliberately its own small set rather
 * than the engine's RI_EV_*: the SMF has a fixed vocabulary and mapping onto
 * it is a decision that belongs in one place. */
#define RI_SMF_EV_NOTE_ON  0u
#define RI_SMF_EV_NOTE_OFF 1u

/* Step flags this writer reads, matching engine/seq/sched.h. */
#define RI_SMF_ACCENT 0x02u
#define RI_SMF_SLIDE  0x01u
#define RI_SMF_FLAM   0x08u

/* One track's worth of already-sorted events. `events` is borrowed, not
 * owned, and must outlive the write. */
struct RISmfEvent {
    uint64_t sample;    /* master-clock sample position, absolute */
    uint32_t type;      /* RI_SMF_EV_* */
    uint16_t channel;   /* 0..15 */
    uint16_t note;      /* 0..127 */
    uint16_t flags;     /* RI_SMF_ACCENT / _SLIDE / _FLAM */
};

struct RISmfTrack {
    const struct RISmfEvent *events;
    uint32_t count;
    uint8_t channel;        /* 0..15 */
    const char *name;       /* borrowed, for the track name meta event */
};

struct RISmfSong {
    uint16_t ppq;           /* ticks per quarter note */
    uint32_t bpm_milli;     /* 120000 = 120.000 BPM */
    uint32_t ntracks;
    uint32_t cap;
    struct RISmfTrack *tracks;  /* borrowed array, owned by the caller */
};

/* The tracks array and its capacity are handed over HERE rather than
 * discovered by add(): an init that leaves the array NULL and the capacity
 * 0 makes every add() fail, and the first caller that forgets to check gets
 * a NULL dereference inside the writer instead of a refusal at the edge. */
void ri_smf_song_init(struct RISmfSong *s, uint16_t ppq, uint32_t bpm_milli,
    struct RISmfTrack *tracks, uint32_t cap);
/* Refuses (returns non-zero) rather than growing past `cap`: a writer that
 * silently drops the last track produces a file that looks complete. */
int ri_smf_song_add(struct RISmfSong *s, const struct RISmfTrack *t);

/* Variable-length quantity. Returns bytes written, or 0 if `cap` is too
 * small -- a truncated VLQ is a DIFFERENT NUMBER, which is worse than no
 * number at all. */
uint32_t ri_smf_vlq(uint8_t *out, uint32_t cap, uint32_t v);
/* Read one VLQ (delta times only). `used` receives the byte count. */
uint32_t ri_smf_vlq_read(const uint8_t *in, uint32_t cap, uint32_t *used);
/* A chunk length: a fixed 4-byte big-endian integer. Provided so a reader
 * walks the file the same way the writer measured it -- and so t193 can
 * prove the two agree. */
uint32_t ri_smf_chunk_len(const uint8_t *in, uint32_t cap);

/* The DELTA in ticks from event i-1 to event i, within ONE track. SMF delta
 * times are relative; a writer that emits absolute times produces a file
 * whose placement is wrong by a running offset that no reader reports. It
 * takes the track, not the song, because the walk is per-track: two tracks
 * with unrelated event counts have no common cursor to be wrong about. */
uint32_t ri_smf_delta_ticks(const struct RISmfTrack *t, uint16_t ppq, uint32_t i);

/* The mapping, isolated. Owner review item; deterministic and in 1..127. */
uint8_t ri_smf_velocity(uint16_t flags);

/* Write the whole file. Returns bytes written, or 0 on any refusal (NULL,
 * or a buffer too small). Never writes a partial file header. */
uint32_t ri_smf_write(const struct RISmfSong *s, uint8_t *out, uint32_t cap);

/* ---------------------------------------------------------------------
 * IMPORT (R7b). Reads a file someone else wrote, which is the whole
 * difficulty: most of what arrives is not what the writer intended.
 * ------------------------------------------------------------------- */

/* What the importer could not represent. Every field is a COUNT, never a
 * flag, because the caller's job is to tell the user which of six things
 * went wrong, and a flag set can only say that one of them did. */
struct RISmfImport {
    uint16_t ppq;            /* as read from the header */
    uint32_t steps;          /* 16th steps written */
    uint32_t stuck_notes;    /* note-ons still held at end-of-track */
    uint32_t orphan_off;     /* note-offs with no matching note-on */
    uint32_t bad_note;       /* note numbers clamped into 0..127 */
    uint32_t past_end;       /* events beyond the caller's cap */
    uint32_t tempo_changes;  /* read and REPORTED, never applied */
    uint32_t tracks;         /* data tracks seen */
    uint8_t  channels;       /* distinct channels carrying notes */
};

/* Quantise to 16ths of a step array. Returns the steps written, or 0 for
 * any refusal (NULL, bad magic, ppq 0, a chunk length that runs past the
 * buffer). A malformed file is refused rather than parsed: an importer
 * that trusts a declared length walks off its own allocation.
 *
 * `ppq_in` is the caller's fallback for a header that declares 0. */
uint32_t ri_smf_read(const uint8_t *buf, uint32_t len, struct RIStep *out,
    uint32_t cap, uint16_t ppq_in, struct RISmfImport *rep);

#endif