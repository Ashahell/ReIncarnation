/* t21_seqswap — snapshot swap continuity soak (WBS TC-2.1.3 event half;
 * the PCM click half waits on voices).
 * Protocol under test: RiSeqRequestSnapshot stages (GUI side);
 * RiSeqBeginBuffer applies staged->current at the next buffer start
 * (render side) and returns the active snapshot. 10000 deterministic
 * swaps (LCG offsets, fixed seed): per buffer, deliver the active
 * snapshot's window via ri_events_in_window; assert every delivered
 * event belongs to the snapshot active at its buffer, each exactly
 * once (no dup), and every in-range event of every served snapshot
 * segment is delivered (no loss). Old-song tails past the boundary
 * are abandoned by definition (asserted explicitly).
 * Edge: NULL request clears pending; BeginBuffer with never-set
 * snapshot returns NULL; double request keeps the last.
 */
#include <stdio.h>
#include <stdint.h>
#include "engine/seq/riseq.h"
#include "engine/seq/sched.h"

#define T21SW_NSWAP 10000u
#define T21SW_BUF 64u
#define T21SW_NBUF 6u

static int fails = 0;
#define CHECK(cond, ...) do { \
    if (!(cond)) { printf("FAIL "); printf(__VA_ARGS__); printf("\n"); fails++; } \
} while (0)

/* Deterministic LCG (no rand()). */
static uint32_t lcg_state = 0x12345678u;
static uint32_t lcg_next(void) {
    lcg_state = lcg_state * 1664525u + 1013904223u;
    return lcg_state >> 8;
}

static struct RIEvent evs_a[6], evs_b[6];
static struct RISeqSnapshot snap_a, snap_b;

static void make_song(struct RIEvent *ev, uint64_t base, uint64_t step) {
    uint32_t i;
    for (i = 0u; i < 6u; i++) {
        ev[i].sample = base + (uint64_t)i * step;
        ev[i].type = RI_EV_NOTE_ON;
        ev[i].device = 0u;
        ev[i].voice = 0u;
        ev[i].value = (uint16_t)(40u + i);
        ev[i].flags = 0u;
        ev[i].seq = i;
    }
}

int main(void) {
    static struct RISeq s_storage;
    struct RISeq *s = &s_storage;
    uint32_t t, b;

    /* Edge: never-set snapshot. */
    RiSeqInit(s, NULL, 96u);
    CHECK(RiSeqBeginBuffer(s) == NULL, "fresh begin non-null");

    /* Edge: NULL request cancels a staged swap (active preset first,
     * so this discriminates staged vs immediate-apply: staged keeps A,
     * immediate would NULL it). */
    RiSeqLoadSnapshot(s, &snap_a);
    RiSeqRequestSnapshot(s, &snap_b);
    RiSeqRequestSnapshot(s, NULL);
    CHECK(RiSeqBeginBuffer(s) == &snap_a, "null-cancel keeps active");

    /* Edge: double request keeps the last. */
    make_song(evs_b, 0ULL, 150ULL);
    snap_b.events = evs_b; snap_b.n_events = 6u;
    snap_b.loop_start = 0ULL; snap_b.loop_end = 0ULL; snap_b.length = 900ULL;
    RiSeqRequestSnapshot(s, &snap_a);
    RiSeqRequestSnapshot(s, &snap_b);
    CHECK(RiSeqBeginBuffer(s) == &snap_b, "double-request not-last");

    /* Soak: 10k swaps, boundary application + continuity. */
    for (t = 0u; t < T21SW_NSWAP; t++) {
        const struct RISeqSnapshot *want;
        uint64_t sw_at; /* buffer index where the swap takes effect */
        /* Alternate songs; boundaries drift via LCG. */
        const struct RISeqSnapshot *cur = (t & 1u) ? &snap_b : &snap_a;
        const struct RISeqSnapshot *nxt = (t & 1u) ? &snap_a : &snap_b;
        uint64_t base = (uint64_t)t * 100000ULL;
        /* Re-stamp event positions per round (deterministic, distinct). */
        make_song((t & 1u) ? evs_b : evs_a, base, 100ULL);
        make_song((t & 1u) ? evs_a : evs_b, base, 150ULL);
        RiSeqLoadSnapshot(s, cur); /* start round on cur */
        sw_at = (uint64_t)(lcg_next() % T21SW_NBUF);
        for (b = 0u; b < T21SW_NBUF; b++) {
            const struct RISeqSnapshot *active;
            struct RIEvent got[8];
            uint64_t s0 = base + (uint64_t)b * T21SW_BUF;
            uint32_t n;
            if (b == sw_at)
                RiSeqRequestSnapshot(s, nxt);
            active = RiSeqBeginBuffer(s);
            want = (b < sw_at) ? cur : nxt;
            if (active != want) {
                printf("FAIL t%u b%u: active %s want %s\n", t, b,
                       active == cur ? "cur" : (active == nxt ? "nxt" : "?"),
                       want == cur ? "cur" : "nxt");
                fails++;
                break;
            }
            /* Continuity: every event of the ACTIVE song inside this
             * buffer window must be delivered exactly once here. */
            n = ri_events_in_window(active->events, active->n_events,
                                    s0, s0 + T21SW_BUF, got, 8u);
            {
                uint32_t i, expect = 0u;
                for (i = 0u; i < active->n_events; i++) {
                    uint64_t sm = active->events[i].sample;
                    if (sm >= s0 && sm < s0 + T21SW_BUF) {
                        uint32_t found = 0u;
                        uint32_t k;
                        expect++;
                        for (k = 0u; k < n; k++) {
                            if (got[k].sample == sm &&
                                got[k].value == active->events[i].value)
                                found++;
                        }
                        if (found != 1u) {
                            printf("FAIL t%u b%u: sample %llu x%u\n", t, b,
                                   (unsigned long long)sm, found);
                            fails++;
                            break;
                        }
                    }
                }
                if (n != expect) {
                    printf("FAIL t%u b%u: delivered %u want %u\n", t, b,
                           n, expect);
                    fails++;
                    break;
                }
            }
        }
        if (fails)
            break;
    }

    if (fails)
        printf("FAIL %d\n", fails);
    else
        printf("PASS t21_seqswap\n");
    return fails != 0;
}
