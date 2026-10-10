/* project/smf_export.c — SMF type 1 writer. See smf_export.h.
 *
 * Nothing here is clever; that is the point. Every rule below is a place
 * where a plausible-looking implementation produces a file that opens
 * without complaint and plays nothing.
 */
#include <string.h>

#include "project/smf_export.h"

/* The writer's own sample rate is NOT assumed: deltas come from the events'
 * sample positions, and the ppq in the header is the only tempo anchor. */
#define SMF_SR 48000u

uint32_t ri_smf_vlq(uint8_t *out, uint32_t cap, uint32_t v) {
    uint8_t tmp[5];
    uint32_t n = 0u, i;
    do {
        tmp[n++] = (uint8_t)(v & 0x7Fu);
        v >>= 7;
    } while (v);
    /* Big-endian, and every byte but the LAST carries the continuation bit.
     * Getting that backwards yields a VLQ whose length says one thing and
     * whose value says another -- the classic silently-broken SMF. */
    if (n > cap)
        return 0u;
    for (i = 0u; i < n; i++)
        out[i] = (uint8_t)(tmp[n - 1u - i] | (i + 1u < n ? 0x80u : 0x00u));
    return n;
}

uint32_t ri_smf_vlq_read(const uint8_t *in, uint32_t cap, uint32_t *used) {
    uint32_t v = 0u, i = 0u;
    if (!in || !used)
        return 0u;
    *used = 0u;
    while (i < cap && i < 5u) {
        v = (v << 7) | (uint32_t)(in[i] & 0x7Fu);
        i++;
        if ((in[i - 1u] & 0x80u) == 0u) {
            *used = i;
            return v;
        }
    }
    return 0u;   /* unterminated: refuse rather than guess */
}

uint32_t ri_smf_chunk_len(const uint8_t *in, uint32_t cap) {
    if (!in || cap < 4u)
        return 0u;
    return ((uint32_t)in[0] << 24) | ((uint32_t)in[1] << 16) |
           ((uint32_t)in[2] << 8) | (uint32_t)in[3];
}

void ri_smf_song_init(struct RISmfSong *s, uint16_t ppq, uint32_t bpm_milli,
    struct RISmfTrack *tracks, uint32_t cap) {
    if (!s)
        return;
    memset(s, 0, sizeof *s);
    /* ppq 0 is refused by the delta path rather than silently dividing; the
     * header still records what the caller asked for. */
    s->ppq = ppq;
    s->bpm_milli = bpm_milli;
    s->tracks = tracks;
    s->ntracks = 0u;
    s->cap = (tracks && cap) ? cap : 0u;
}

int ri_smf_song_add(struct RISmfSong *s, const struct RISmfTrack *t) {
    if (!s || !t)
        return 1;
    if (s->ntracks >= s->cap)
        return 1;   /* no room: say so rather than drop the last track */
    s->tracks[s->ntracks++] = *t;
    return 0;
}

uint32_t ri_smf_delta_ticks(const struct RISmfTrack *t, uint16_t ppq, uint32_t i) {
    uint64_t d;
    if (!t || !t->events || i == 0u || i >= t->count)
        return 0u;
    if (ppq == 0u)
        return 0u;   /* fail closed, never divide */
    d = (t->events[i].sample > t->events[i - 1u].sample)
        ? (t->events[i].sample - t->events[i - 1u].sample) : 0u;
    d = d * (uint64_t)ppq / (uint64_t)SMF_SR;
    return (uint32_t)(d > 0xFFFFFFFFULL ? 0xFFFFFFFFULL : d);
}

uint8_t ri_smf_velocity(uint16_t flags) {
    /* OWNER REVIEW ITEM. Conventional, documented, not defended: accent is
     * louder, plain is middle. Deliberately a single function so the
     * convention is one edit and one test rather than a search. */
    return (flags & RI_SMF_ACCENT) ? (uint8_t)112u : (uint8_t)64u;
}

/* ---- writing ------------------------------------------------------- */

static uint32_t put(uint8_t *o, uint32_t cap, uint32_t at, uint32_t v) {
    return (at < cap) ? (o[at] = (uint8_t)(v & 0xFFu), at + 1u) : cap + 1u;
}
static uint32_t put32(uint8_t *o, uint32_t cap, uint32_t at, uint32_t v) {
    at = put(o, cap, at, (v >> 24) & 0xFFu);
    at = put(o, cap, at, (v >> 16) & 0xFFu);
    at = put(o, cap, at, (v >> 8) & 0xFFu);
    return put(o, cap, at, v & 0xFFu);
}

static uint32_t put16(uint8_t *o, uint32_t cap, uint32_t at, uint32_t v) {
    at = put(o, cap, at, (v >> 8) & 0xFFu);
    return put(o, cap, at, v & 0xFFu);
}
/* Meta 0x51 set-tempo, microseconds per quarter note. */
static uint32_t put_tempo(uint8_t *o, uint32_t cap, uint32_t at, uint32_t bpm_milli) {
    uint32_t usq = (bpm_milli > 0u)
        ? (uint32_t)((60000000ULL / (uint64_t)bpm_milli)) : 500000u;
    at = put(o, cap, at, 0xFFu);
    at = put(o, cap, at, 0x51u);
    at = put(o, cap, at, 0x03u);
    at = put(o, cap, at, (usq >> 16) & 0xFFu);
    at = put(o, cap, at, (usq >> 8) & 0xFFu);
    return put(o, cap, at, usq & 0xFFu);
}

static uint32_t put_text(uint8_t *o, uint32_t cap, uint32_t at, uint8_t type,
    const char *s) {
    uint32_t n = 0u;
    if (s)
        while (s[n] && n < 127u)
            n++;
    at = put(o, cap, at, 0xFFu);
    at = put(o, cap, at, type);
    at = put(o, cap, at, n);
    {
        uint32_t i;
        for (i = 0u; i < n; i++)
            at = put(o, cap, at, (uint32_t)(uint8_t)s[i]);
    }
    return at;
}

static uint32_t put_eot(uint8_t *o, uint32_t cap, uint32_t at) {
    at = put(o, cap, at, 0x00u);      /* delta 0 */
    at = put(o, cap, at, 0xFFu);
    at = put(o, cap, at, 0x2Fu);
    return put(o, cap, at, 0x00u);
}

/* One data track. `base` is the write offset; the track's own length is
 * returned separately because MTrk needs it BEFORE the bytes, and this
 * writes the bytes into a scratch first. */
static uint32_t build_track(const struct RISmfTrack *t, uint16_t ppq,
    uint8_t *scratch, uint32_t scap) {
    uint32_t at = 0u, i;
    if (!t || !t->events)
        return put_eot(scratch, scap, at);
    if (t->name)
        at = put_text(scratch, scap, at, 0x03u, t->name);
    for (i = 0u; i < t->count; i++) {
        const struct RISmfEvent *e = &t->events[i];
        uint32_t d;
        /* The delta is measured against THIS track's previous event, not
         * against a global cursor: a per-track walk is the only thing that
         * stays right when two tracks have unrelated event counts. */
        d = (i == 0u) ? 0u : ri_smf_delta_ticks(t, ppq, i);
        at += ri_smf_vlq(scratch + at, scap - at, d);
        if (at >= scap)
            return 0u;
        if (e->type == RI_SMF_EV_NOTE_ON) {
            at = put(scratch, scap, at, 0x90u | (uint32_t)(e->channel & 0x0Fu));
            at = put(scratch, scap, at, e->note & 0x7Fu);
            at = put(scratch, scap, at, ri_smf_velocity(e->flags));
        } else {
            /* Note-off as a real note-off (0x80), never note-on with
             * velocity 0: a DAW importing the latter leaves the note
             * hanging, which is the classic "stuck note" failure. */
            at = put(scratch, scap, at, 0x80u | (uint32_t)(e->channel & 0x0Fu));
            at = put(scratch, scap, at, e->note & 0x7Fu);
            at = put(scratch, scap, at, 0x40u);
        }
        if (at >= scap)
            return 0u;
    }
    return put_eot(scratch, scap, at);
}

uint32_t ri_smf_write(const struct RISmfSong *s, uint8_t *out, uint32_t cap) {
    uint32_t at = 0u, i, ntr;
    static uint8_t scratch[8192];
    if (!s || !out || cap < 14u)
        return 0u;
    ntr = s->ntracks;
    if (ntr > s->cap)
        ntr = s->cap;

    at = put(out, cap, at, 'M'); at = put(out, cap, at, 'T');
    at = put(out, cap, at, 'h'); at = put(out, cap, at, 'd');
    /* The chunk length is a FIXED 4-byte big-endian integer -- 00 00 00 06 --
     * and NOT a variable-length quantity. I "fixed" this to a VLQ on the
     * grounds that every length in the format is variable, which made the
     * header eleven bytes instead of fourteen and produced a file whose
     * MTrk magic sat where the division field belongs. VLQ is for DELTA
     * TIMES inside a track, and for nothing else. */
    at = put32(out, cap, at, 6u);
    at = put16(out, cap, at, 1u);              /* format 1 */
    at = put16(out, cap, at, ntr + 1u);        /* data tracks + the tempo track */
    at = put16(out, cap, at, s->ppq);

    /* Track 0 is the conductor: name, tempo, EOT. Type 1 requires it. */
    {
        uint32_t tl = 0u;
        tl = put_text(scratch, sizeof scratch, tl, 0x03u, "ReIncarnation");
        tl = put_tempo(scratch, sizeof scratch, tl, s->bpm_milli);
        tl = put_eot(scratch, sizeof scratch, tl);
        at = put(out, cap, at, 'M'); at = put(out, cap, at, 'T');
        at = put(out, cap, at, 'r'); at = put(out, cap, at, 'k');
        at = put32(out, cap, at, tl);
        if (at > cap || (at + tl) > cap)
            return 0u;
        memcpy(out + at, scratch, tl);
        at += tl;
    }

    for (i = 0u; i < ntr; i++) {
        uint32_t tl = build_track(&s->tracks[i], s->ppq, scratch, sizeof scratch);
        if (!tl)
            return 0u;
        at = put(out, cap, at, 'M'); at = put(out, cap, at, 'T');
        at = put(out, cap, at, 'r'); at = put(out, cap, at, 'k');
        /* The declared length must be EXACT: too long shifts every later
         * track, too short loses the tail, and neither is reported. */
        at = put32(out, cap, at, tl);
        if (at > cap || (at + tl) > cap)
            return 0u;
        memcpy(out + at, scratch, tl);
        at += tl;
        if (at > cap)
            return 0u;
    }
    return at;
}