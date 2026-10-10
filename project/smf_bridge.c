/* project/smf_bridge.c — R8f. See smf_bridge.h for the laws. */
#include <string.h>
#include "engine/fx/route.h"   /* RI_ROUTE_NSECTIONS */
#include "project/smf_bridge.h"

/* The two enums are NOT the same numbers, which is the whole reason this
 * function exists. A cast would turn every note-on into a note-off and
 * produce a file of releases with no attacks. */
static int smf_type(uint16_t ev_type, uint32_t *out) {
    switch (ev_type) {
    case RI_EV_NOTE_ON:
        *out = RI_SMF_EV_NOTE_ON;
        return 1;
    case RI_EV_NOTE_OFF:
        *out = RI_SMF_EV_NOTE_OFF;
        return 1;
    default:
        return 0;   /* including NOTE_CONTINUE: a slide cannot cross */
    }
}

uint32_t ri_smf_bridge(struct RISmfTrack *t, struct RISmfEvent *out,
    uint32_t cap, const struct RIEvent *ev, uint32_t nev, uint8_t channel) {
    uint32_t i, n = 0u, slides = 0u;
    if (!t || !out || !ev || cap == 0u || nev == 0u)
        return 0u;
    memset(t, 0, sizeof *t);
    for (i = 0u; i < nev; i++) {
        uint32_t type;
        if (ev[i].type == RI_EV_NOTE_CONTINUE) {
            /* DROPPED AND COUNTED. Both available fakes are audible: a
             * note-off/note-on pair is the re-attack R6a exists to refuse,
             * and a bare pitch bend is a different note on a track that is
             * supposed to be this pattern. */
            slides++;
            continue;
        }
        if (!smf_type(ev[i].type, &type))
            continue;
        if (n >= cap)
            break;      /* truncate, never overwrite */
        out[n].sample = ev[i].sample;
        out[n].type = type;
        out[n].channel = (uint16_t)(channel & 0x0Fu);
        out[n].note = (uint16_t)(ev[i].value & 0x7Fu);
        out[n].flags = (uint16_t)((ev[i].flags & RI_EVFLAG_ACCENT) ?
            RI_SMF_ACCENT : 0u);
        n++;
    }
    if (n == 0u) {
        /* Nothing crossed: a track of nothing is not a track. */
        t->slides = slides;
        return 0u;
    }
    t->events = out;
    t->count = n;
    t->channel = (uint8_t)(channel & 0x0Fu);
    t->name = 0;
    t->slides = slides;
    return n;
}

uint32_t ri_smf_bridge_song(struct RISmfSong *s, struct RISmfTrack *tracks,
    struct RISmfEvent *out, uint32_t outcap, const struct RIEvent *ev,
    uint32_t nev, uint16_t ppq, uint32_t bpm_milli) {
    uint32_t dev, ntr = 0u, used = 0u;
    if (!tracks || !out || !ev || outcap == 0u || nev == 0u)
        return 0u;
    /* THROUGH song_init, NOT A memset. ppq and tempo are the caller's, and a
     * song left with ppq 0 collapses every event onto tick 0 -- a file that
     * happens to be right only when it holds one event, and looks right
     * while it is wrong. */
    ri_smf_song_init(s, ppq, bpm_milli, tracks, RI_ROUTE_NSECTIONS);
    for (dev = 0u; dev < RI_ROUTE_NSECTIONS; dev++) {
        struct RISmfTrack *t = &tracks[ntr];
        uint32_t got;
        /* Split the source by DEVICE first, so each track gets its own
         * contiguous slice of the output array and the bridge never has to
         * compact or reorder anything. */
        {
            static struct RIEvent sub[1024];
            uint32_t i, n = 0u;
            if (nev > 1024u)
                return 0u;
            for (i = 0u; i < nev; i++)
                if (ev[i].device == dev && n < 1024u)
                    sub[n++] = ev[i];
            if (n == 0u)
                continue;      /* no events for this device: no track */
            got = ri_smf_bridge(t, out + used, outcap - used, sub, n,
                (uint8_t)dev);
            used += got;
            if (got == 0u)
                continue;      /* only slides: not a track */
        }
        if (ri_smf_song_add(s, t) != 0)
            break;
        ntr++;
    }
    return ntr;
}
