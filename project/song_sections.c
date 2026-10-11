/* project/song_sections.c — R8g. See song_sections.h for the laws. */
#include <string.h>
#include "project/song_sections.h"

uint32_t ri_song_sections(const struct RISectionRun *runs, uint32_t n) {
    uint32_t mask = 0u, i;
    if (!runs || n == 0u)
        return 0u;
    for (i = 0u; i < n; i++) {
        if (!runs[i].steps || runs[i].nsteps == 0u)
            continue;         /* a section that is switched off */
        /* REFUSED, NOT MASKED IN. Returning a bit for a section that does
         * not exist would hand the engine a mask it cannot honour, and the
         * caller would find out as silence. */
        if (runs[i].device >= RI_ROUTE_NSECTIONS)
            return 0u;
        mask |= 1u << runs[i].device;
    }
    return mask;
}

/* Merge two §8-ordered runs into one §8-ordered run. THE WHOLE §8 KEY, NOT
 * JUST THE SAMPLE: sample, type, device, voice, seq. Ordering by sample alone
 * leaves every tie to chance, and the ties are not rare -- they are every
 * downbeat where two sections play together. */
uint32_t ri_song_merge(struct RIEvent *out, uint32_t cap, const struct RIEvent *a,
    uint32_t na, const struct RIEvent *b, uint32_t nb) {
    uint32_t n = 0u, i = 0u, j = 0u;
    while ((i < na || j < nb) && n < cap) {
        int take_a;
        if (i >= na)
            take_a = 0;
        else if (j >= nb)
            take_a = 1;
        else
            take_a = !ri_event_less(&b[j], &a[i]);   /* <=, so a stable pick */
        if (take_a)
            out[n++] = a[i++];
        else
            out[n++] = b[j++];
    }
    return n;
}

uint32_t ri_song_emit(const struct RITempoMap *map, uint32_t ppq,
    const struct RISectionRun *runs, uint32_t n,
    const struct RISchedOpts *opts, struct RIEvent *out, uint32_t cap) {
    return ri_song_emit_at(map, ppq, runs, n, opts, out, cap, 0ULL);
}

uint32_t ri_song_emit_at(const struct RITempoMap *map, uint32_t ppq,
    const struct RISectionRun *runs, uint32_t n,
    const struct RISchedOpts *opts, struct RIEvent *out, uint32_t cap,
    uint64_t start_tick) {
    /* TWO ACCUMULATORS, ALTERNATED. My first version merged `acc` into
     * itself, which is wrong and quietly so: after k writes the destination
     * index is i+j while the source index is only i, so every event taken
     * from the second run overwrote an unread event of the first. The
     * symptom was a song with a whole section missing rather than a crash,
     * which is the worst kind -- and it came out as one device having 32
     * events and the other 1. */
    static struct RIEvent accA[RI_SCHED_MAX_EVENTS];
    static struct RIEvent accB[RI_SCHED_MAX_EVENTS];
    struct RIEvent *acc = accA, *spare = accB;
    static struct RIEvent tmp[RI_SCHED_MAX_EVENTS];
    uint32_t total = 0u, i;
    if (!map || !runs || !out || ppq == 0u || cap == 0u || n == 0u)
        return 0u;
    for (i = 0u; i < n; i++)
        if (runs[i].device >= RI_ROUTE_NSECTIONS)
            return 0u;       /* the whole song is refused, not this section */
    for (i = 0u; i < n; i++) {
        uint32_t got;
        if (!runs[i].steps || runs[i].nsteps == 0u)
            continue;
        got = ri_sched_emit_timed(map, start_tick, ppq, runs[i].steps,
            runs[i].nsteps, runs[i].device, opts, tmp,
            RI_SCHED_MAX_EVENTS);
        if (got == 0u)
            continue;
        if (total > 0u) {
            struct RIEvent *swap;
            total = ri_song_merge(spare, RI_SCHED_MAX_EVENTS, acc, total,
                tmp, got);
            swap = acc; acc = spare; spare = swap;
        } else {
            memcpy(acc, tmp, (size_t)got * sizeof *acc);
            total = got;
        }
    }
    if (total == 0u)
        return 0u;
    /* Copy out last and TRUNCATE: a caller with a short buffer gets what
     * fitted, in order, rather than an array that stops being sorted. */
    if (total > cap)
        total = cap;
    memcpy(out, acc, (size_t)total * sizeof *out);
    return total;
}
