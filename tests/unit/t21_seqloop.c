/* t21_seqloop — WBS 2.1 snapshot contract + loop cursor (TC-2.1.2 math
 * half; the PCM double-render half waits on the song→events builder).
 * Hand-built snapshot (no builder yet): loop [9600, 19200) samples.
 * Pins: pre-loop passthrough (iter 0), exact wrap points (end -> start,
 * iter+1), mid-loop positions, second wrap, no-snapshot and degenerate
 * (end <= start) fallbacks. All exact integer asserts, deterministic.
 */
#include <stdio.h>
#include <stdint.h>
#include "engine/seq/riseq.h"
#include "engine/seq/sched.h"

static int fails = 0;
#define CHECK(cond, ...) do { \
    if (!(cond)) { printf("FAIL "); printf(__VA_ARGS__); printf("\n"); fails++; } \
} while (0)

static struct RIEvent k_evs[3];

int main(void) {
    static struct RISeq s_storage;
    struct RISeq *s = &s_storage;
    struct RISeqSnapshot snap;
    uint64_t iter, pos;

    k_evs[0].sample = 9600ULL; k_evs[0].type = RI_EV_NOTE_ON;
    k_evs[1].sample = 14400ULL; k_evs[1].type = RI_EV_NOTE_OFF;
    k_evs[2].sample = 20000ULL; k_evs[2].type = RI_EV_NOTE_ON; /* past end */
    snap.events = k_evs;
    snap.n_events = 3u;
    snap.loop_start = 9600ULL;
    snap.loop_end = 19200ULL;
    snap.length = 24000ULL;

    RiSeqInit(s, NULL, 96u);

    /* No snapshot yet: passthrough. */
    RiSeqAdvanceFrames(s, 100u);
    pos = RiSeqLoopPos(s, &iter);
    CHECK(pos == 100ULL && iter == 0ULL, "nosnap pos %llu iter %llu",
          (unsigned long long)pos, (unsigned long long)iter);

    RiSeqLoadSnapshot(s, &snap);

    /* Pre-loop still passthrough. */
    pos = RiSeqLoopPos(s, &iter);
    CHECK(pos == 100ULL && iter == 0ULL, "preloop pos %llu iter %llu",
          (unsigned long long)pos, (unsigned long long)iter);

    /* Loop start: iter 0. */
    RiSeqAdvanceFrames(s, 9500u); /* clock = 9600 */
    pos = RiSeqLoopPos(s, &iter);
    CHECK(pos == 9600ULL && iter == 0ULL, "start pos %llu iter %llu",
          (unsigned long long)pos, (unsigned long long)iter);

    /* Mid-loop. */
    RiSeqAdvanceFrames(s, 4800u); /* clock = 14400 */
    pos = RiSeqLoopPos(s, &iter);
    CHECK(pos == 14400ULL && iter == 0ULL, "mid pos %llu iter %llu",
          (unsigned long long)pos, (unsigned long long)iter);

    /* End-1 vs end (wrap): 19199 -> iter 0; 19200 -> pos 9600 iter 1. */
    RiSeqAdvanceFrames(s, 4799u); /* clock = 19199 */
    pos = RiSeqLoopPos(s, &iter);
    CHECK(pos == 19199ULL && iter == 0ULL, "end-1 pos %llu iter %llu",
          (unsigned long long)pos, (unsigned long long)iter);
    RiSeqAdvanceFrames(s, 1u); /* clock = 19200 */
    pos = RiSeqLoopPos(s, &iter);
    CHECK(pos == 9600ULL && iter == 1ULL, "wrap pos %llu iter %llu",
          (unsigned long long)pos, (unsigned long long)iter);

    /* Second wrap: 19200 + 9600 = 28800 -> pos 9600 iter 2. */
    RiSeqAdvanceFrames(s, 9600u); /* clock = 28800 */
    pos = RiSeqLoopPos(s, &iter);
    CHECK(pos == 9600ULL && iter == 2ULL, "wrap2 pos %llu iter %llu",
          (unsigned long long)pos, (unsigned long long)iter);

    /* Degenerate snapshot (end <= start): passthrough. */
    snap.loop_end = 9600ULL;
    RiSeqLoadSnapshot(s, &snap);
    pos = RiSeqLoopPos(s, &iter);
    CHECK(pos == 28800ULL && iter == 0ULL, "degen pos %llu iter %llu",
          (unsigned long long)pos, (unsigned long long)iter);

    if (fails)
        printf("FAIL %d\n", fails);
    else
        printf("PASS t21_seqloop\n");
    return fails != 0;
}
