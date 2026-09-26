/* t84_pal_thread — portability T1: PAL atomics + SPSC hardening.
 * RED-first: needs platform/pal/ri_pal_thread.h (ri_atomic_u32/_ptr,
 * acquire load / release store). GREEN migrates ctlplane head/tail,
 * RIAutoPub front/staged, RISeq pending/active, AuLive cross-thread words
 * and live meters_seq onto it. No heap, no IO, bounded loops.
 */
#include <stdio.h>
#include <stdint.h>
#include <pthread.h>
#include "tests/helpers/ri_assert.h"
#include "platform/pal/ri_pal_thread.h"
#include "engine/seq/ctlplane.h"
#include "engine/seq/autolane.h"
#include "engine/seq/autolane_emit.h"
#include "engine/seq/riseq.h"
#include "engine/live.h"

#define T84_PING 200000u
#define T84_RING 50000u

static ri_atomic_u32 t_flag;
static ri_atomic_u32 t_payload;

static void *t84_producer(void *arg) {
    uint32_t i;
    (void)arg;
    for (i = 0u; i < T84_PING; i++) {
        ri_atomic_store_rel(&t_payload, i);
        ri_atomic_store_rel(&t_flag, i + 1u);
    }
    return 0;
}

static void *t84_consumer(void *arg) {
    uint32_t got = 0u;
    (void)arg;
    while (got < T84_PING) {
        uint32_t f = ri_atomic_load_acq(&t_flag);
        if (f > got) {
            uint32_t pay;
            got = f;
            pay = ri_atomic_load_acq(&t_payload);
            /* Law: the payload is never BEHIND the flag (release publishes
             * the write; acquire observes it). It may run AHEAD: the
             * producer can lap the consumer between the two reads. */
            if (pay + 1u < got) {
                printf("FAIL torn message: flag=%u payload=%u\n", got, pay);
                ri_fail_count++;
                return 0;
            }
        }
    }
    return 0;
}

static struct RIControlPlane t_q;
static uint32_t t_sent;
static uint32_t t_recv_vals[T84_RING];
static uint32_t t_recv_n;

static void *t84_writer(void *arg) {
    uint32_t i = 0u;
    (void)arg;
    while (i < T84_RING) {
        uint16_t key = (i & 1u) ? RI_CTL_303A_RESO : RI_CTL_303A_CUTOFF;
        uint8_t val = (uint8_t)(i & 127u);
        if (ri_ctl_pending(&t_q) < RI_CTL_CAP) {
            if (ri_ctl_send(&t_q, key, val) == 0) {
                i++;
                t_sent = i;
            }
        }
    }
    return 0;
}

static void *t84_reader(void *arg) {
    struct RIEvent ev[64];
    uint32_t seq = 0u, n, k;
    (void)arg;
    while (t_recv_n < T84_RING) {
        n = ri_ctl_drain(&t_q, ev, 64u, 0u, &seq);
        for (k = 0u; k < n && t_recv_n < T84_RING; k++) {
            t_recv_vals[t_recv_n++] = ((uint32_t)ev[k].value << 8) | ev[k].flags;
        }
    }
    return 0;
}

int main(void) {
    pthread_t pth[2];
    uint32_t i;
    /* Atomic round-trips. */
    {
        ri_atomic_u32 a;
        ri_atomic_ptr q;
        uint32_t sentinel = 0xA5A55A5Au;
        ri_atomic_store_rel(&a, 0u);
        RI_ASSERT(ri_atomic_load_acq(&a) == 0u, "atomic zero");
        ri_atomic_store_rel(&a, 0xDEADBEEFu);
        RI_ASSERT(ri_atomic_load_acq(&a) == 0xDEADBEEFu, "atomic word");
        ri_atomic_ptr_store_rel(&q, 0);
        RI_ASSERT(ri_atomic_ptr_load_acq(&q) == 0, "ptr null");
        ri_atomic_ptr_store_rel(&q, &sentinel);
        RI_ASSERT(ri_atomic_ptr_load_acq(&q) == (void *)&sentinel, "ptr word");
    }
    /* Message-passing: release store publishes payload to acquire load. */
    ri_atomic_store_rel(&t_flag, 0u);
    ri_atomic_store_rel(&t_payload, 0u);
    RI_ASSERT(pthread_create(&pth[0], 0, t84_producer, 0) == 0, "spawn prod");
    RI_ASSERT(pthread_create(&pth[1], 0, t84_consumer, 0) == 0, "spawn cons");
    RI_ASSERT(pthread_join(pth[0], 0) == 0, "join prod");
    RI_ASSERT(pthread_join(pth[1], 0) == 0, "join cons");
    RI_ASSERT(ri_atomic_load_acq(&t_flag) == T84_PING, "pings delivered");
    /* SPSC ring under threads: blocking on full/empty, FIFO order, no loss. */
    ri_ctl_init(&t_q);
    t_sent = 0u;
    t_recv_n = 0u;
    RI_ASSERT(pthread_create(&pth[0], 0, t84_writer, 0) == 0, "spawn writer");
    RI_ASSERT(pthread_create(&pth[1], 0, t84_reader, 0) == 0, "spawn reader");
    RI_ASSERT(pthread_join(pth[0], 0) == 0, "join writer");
    RI_ASSERT(pthread_join(pth[1], 0) == 0, "join reader");
    RI_ASSERT(t_sent == T84_RING, "sent all %u", t_sent);
    RI_ASSERT(t_recv_n == T84_RING, "received all %u", t_recv_n);
    for (i = 0u; i < T84_RING; i++) {
        uint16_t key = (i & 1u) ? RI_CTL_303A_RESO : RI_CTL_303A_CUTOFF;
        uint32_t want = ((uint32_t)key << 8) | (i & 127u);
        if (t_recv_vals[i] != want) {
            RI_ASSERT(0, "order break at %u: got 0x%x want 0x%x", i, t_recv_vals[i], want);
            break;
        }
    }
    RI_ASSERT(ri_ctl_pending(&t_q) == 0u, "ring empty at end");
    /* Publish + snapshot + meter seqlock ride on atomics. */
    {
        static struct RIAutoEv e0[4], e1[4];
        struct RIAutoPub pub;
        struct RISeq sq;
        struct RILiveSession live;
        struct RILiveMeters m;
        static struct RISeqSnapshot snap;
        ri_auto_pub_init(&pub, e0, 4u, e1, 4u);
        ri_auto_pub_request(&pub);
        ri_auto_pub_apply(&pub);
        RI_ASSERT(ri_auto_pub_front(&pub) != 0, "pub front live");
        RiSeqInit(&sq, 0, 96u);
        RiSeqRequestSnapshot(&sq, &snap);
        RI_ASSERT(RiSeqBeginBuffer(&sq) == &snap, "snapshot staged->active");
        ri_live_init(&live, 96u, 48000.0f, 120.0f, 1u, 0, 0u);
        ri_live_meters_begin(&live);
        ri_live_meters_end(&live);
        RI_ASSERT(ri_live_meters_read(&live, &m) == 0, "meters readable");
    }
    RI_RESULT("pal_thread");
}
