/* t86_pal_fslog — portability T6: paths, dir scan, logging.
 * Host backend: join/list/read-write round-trip + log_format parity.
 */
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdarg.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <sys/stat.h>
#include "tests/helpers/ri_assert.h"
#include "platform/pal/ri_pal_fs.h"
#include "platform/pal/ri_pal_log.h"

static int t_count;
static int t_cb(void *u, const char *name) {
    (void)u;
    (void)name;
    t_count++;
    return 0;
}

static int t_fmt(char *out, uint32_t cap, const char *fmt, ...) {
    va_list ap;
    int n;
    va_start(ap, fmt);
    n = ri_log_format(out, cap, fmt, ap);
    va_end(ap);
    return n;
}

int main(void) {
    char dir[256], leaf[256], tmpd[256], sub[512];
    uint32_t got = 0u;
    uint8_t wbuf[64], rbuf[64];
    uint32_t i;
    /* Every path enum resolves (PACKS added for the 909 pack). */
    RI_ASSERT(ri_pal_path(RI_PATH_MODS, dir, sizeof(dir)) == 0, "mods");
    RI_ASSERT(ri_pal_path(RI_PATH_PACKS, dir, sizeof(dir)) == 0, "packs");
    RI_ASSERT(ri_pal_path_join(leaf, sizeof(leaf), dir, "x") == 0, "packs join");
    /* path_join. */
    RI_ASSERT(ri_pal_path_join(dir, sizeof(dir), "/tmp/ri", "x.wav") == 0, "join ok");
    RI_ASSERT(strcmp(dir, "/tmp/ri/x.wav") == 0, "join %s", dir);
    RI_ASSERT(ri_pal_path_join(dir, sizeof(dir), "/tmp/ri/", "x.wav") == 0, "join slash ok");
    RI_ASSERT(strcmp(dir, "/tmp/ri/x.wav") == 0, "join slash %s", dir);
    RI_ASSERT(ri_pal_path_join(dir, 8u, "/tmp/ri", "x.wav") != 0, "join trunc");
    RI_ASSERT(ri_pal_path_join(0, 0u, "a", "b") != 0, "join null");
    /* read/write round-trip. */
    snprintf(tmpd, sizeof(tmpd), "/tmp/ri/t86-%d", (int)getpid());
    mkdir(tmpd, 0700);
    RI_ASSERT(ri_pal_path_join(leaf, sizeof(leaf), tmpd, "rw.bin") == 0, "leaf");
    for (i = 0u; i < sizeof(wbuf); i++)
        wbuf[i] = (uint8_t)(i * 3u + 1u);
    RI_ASSERT(ri_pal_write_file(leaf, wbuf, sizeof(wbuf)) == 0, "write");
    memset(rbuf, 0, sizeof(rbuf));
    RI_ASSERT(ri_pal_read_file(leaf, rbuf, sizeof(rbuf), &got) == 0, "read");
    RI_ASSERT(got == sizeof(wbuf), "got %u", got);
    RI_ASSERT(memcmp(wbuf, rbuf, sizeof(wbuf)) == 0, "roundtrip");
    RI_ASSERT(ri_pal_read_file("/tmp/ri/t86-no-such-file.bin", rbuf, sizeof(rbuf), &got) != 0, "read miss");
    /* list_dirs sees the subdir we make. */
    snprintf(sub, sizeof(sub), "%s/sub", tmpd);
    mkdir(sub, 0700);
    t_count = 0;
    RI_ASSERT(ri_pal_list_dirs(tmpd, t_cb, 0) == 0, "scan");
    RI_ASSERT(t_count >= 1, "scan found %d", t_count);
    RI_ASSERT(ri_pal_list_dirs("/tmp/ri/t86-no-such-dir", t_cb, 0) != 0, "scan miss");
    /* log_format parity with snprintf. */
    {
        char a[128], b[128];
        t_fmt(a, sizeof(a), "n=%d s=%s u=%lu x=%lx", -7, "hi", 3000000000ul, 0xdeadbeeful);
        snprintf(b, sizeof(b), "n=%d s=%s u=%lu x=%lx", -7, "hi", 3000000000ul, 0xdeadbeeful);
        RI_ASSERT(strcmp(a, b) == 0, "fmt %s", a);
        RI_ASSERT(t_fmt(a, 8u, "hello world") >= 0, "fmt trunc ok");
        RI_ASSERT(t_fmt(0, 0u, "x") < 0, "fmt null");
    }
    unlink(leaf);
    rmdir(sub);
    /* Single-instance claim (2026-10-18). The app must REFUSE to run when
     * another instance holds the claim -- the requester that used to warn
     * and continue let two instances write the same prefs. The contract is
     * three behaviours, and only the first is obvious:
     *   acquire -> 0, second acquire -> 1 (busy), release frees it;
     *   a LIVE holder's heartbeat keeps a contender out;
     *   a DEAD holder's heartbeat (crash, no reboot) must not lock the
     *   instrument forever -- past the staleness window the contender
     *   takes the claim over.
     * 2 (undecidable) must be reserved for "cannot tell", and the app
     * runs anyway in that case: an unopenable claim must never keep the
     * instrument unopenable. */
    {
        char claim[512], hb[576], dead[640];
        long now, old;
        FILE *f;
        snprintf(claim, sizeof(claim), "%s/t86-claim", tmpd);
        snprintf(hb, sizeof(hb), "%s/hb", claim);
        snprintf(dead, sizeof(dead), "%s/dead", claim);
        rmdir(hb); rmdir(claim);                     /* clean slate */
        RI_ASSERT(ri_pal_claim_acquire(claim) == 0, "claim taken");
        RI_ASSERT(ri_pal_claim_acquire(claim) == 1, "second acquire is busy");
        RI_ASSERT(ri_pal_claim_refresh(claim) == 0, "refresh");
        RI_ASSERT(ri_pal_claim_acquire(claim) == 1, "live holder still busy");
        ri_pal_claim_release(claim);
        RI_ASSERT(ri_pal_claim_acquire(claim) == 0, "released then retaken");
        /* Dead holder: a heartbeat older than the staleness window. */
        ri_pal_claim_release(claim);
        RI_ASSERT(mkdir(claim, 0700) == 0, "stale dir made by hand");
        f = fopen(hb, "wb");
        RI_ASSERT(f != 0, "stale hb open");
        if (f) {
            now = (long)time(0);
            old = now - (long)RI_CLAIM_STALE_SEC - 60L;
            fprintf(f, "%ld\n", old);
            fclose(f);
        }
        RI_ASSERT(ri_pal_claim_acquire(claim) == 0, "stale claim taken over");
        RI_ASSERT(ri_pal_claim_acquire(claim) == 1, "takeover is exclusive");
        ri_pal_claim_release(claim);
        /* Undecidable: the parent does not exist, so the claim cannot be
         * created and its state cannot be read. */
        snprintf(dead, sizeof(dead), "%s/t86-no-such-parent/claim", tmpd);
        RI_ASSERT(ri_pal_claim_acquire(dead) == 2, "undecidable is 2");
        RI_ASSERT(ri_pal_claim_acquire(0) == 2, "null path is 2");
        /* The reason string is part of the contract: a lock that decides
         * silently cannot be told apart from a broken one on a lane. */
        RI_ASSERT(ri_pal_claim_acquire(dead) == 2, "undecidable again");
        RI_ASSERT(strstr(ri_pal_claim_last(), "undecidable") != 0,
            "reason names the undecidable case: %s", ri_pal_claim_last());
        /* Two different dead-looking claims, two different answers:
         *   heartbeat PRESENT but unreadable -> BUSY. Failing safe: an I/O
         *   problem is not evidence of death, and treating it as death
         *   lets any process that cannot read one small file evict the
         *   running instance and become a second writer.
         *   heartbeat ABSENT -> takeover. A live holder writes it the
         *   moment it takes the claim, so no heartbeat means nobody is
         *   heartbeating; this is the recovery path for a crash between
         *   the directory creation and that first write, and it is what
         *   keeps a leftover claim from wedging the instrument forever. */
        RI_ASSERT(mkdir(claim, 0700) == 0, "undated claim dir");
        RI_ASSERT(ri_pal_claim_acquire(claim) == 0, "undated claim is taken over");
        RI_ASSERT(strstr(ri_pal_claim_last(), "undated") != 0,
            "reason names the undated claim: %s", ri_pal_claim_last());
        ri_pal_claim_release(claim);
        RI_ASSERT(mkdir(claim, 0700) == 0, "claim dir again");
        RI_ASSERT(mkdir(hb, 0700) == 0, "hb is a directory: present, unreadable");
        /* Locked-but-stamped: the wedge case the second review found. The
         * heartbeat alone cannot answer it (it is locked), so the answer
         * comes from the UNLOCKED stamp file: fresh -> busy, old ->
         * takeover. This is the branch that turns "a reboot is the only
         * crash recovery" into "a wedged instance is taken over". */
        {
            char age[640];
            FILE *f;
            long now = (long)time(0);
            snprintf(age, sizeof(age), "%s/hb.age", claim);
            f = fopen(age, "wb");
            RI_ASSERT(f != 0, "stamp file written");
            if (f) { fprintf(f, "%ld\n", now); fclose(f); }
            RI_ASSERT(ri_pal_claim_acquire(claim) == 1,
                "locked with a FRESH stamp stays busy");
            RI_ASSERT(strstr(ri_pal_claim_last(), "fresh") != 0,
                "reason names the fresh stamp: %s", ri_pal_claim_last());
            /* The boundary that makes the threshold mean something: an age
             * past RI_CLAIM_STALE_SEC but not past the takeover threshold
             * must STILL be busy. That margin is what stops a contender that
             * read the stamp just before a refresh from stealing a live
             * claim, so it is pinned rather than left to the constant. */
            f = fopen(age, "wb");
            RI_ASSERT(f != 0, "stamp file rewritten");
            if (f) {
                fprintf(f, "%ld\n", now - (long)RI_CLAIM_STALE_SEC - 10L);
                fclose(f);
            }
            RI_ASSERT(ri_pal_claim_acquire(claim) == 1,
                "inside the margin: still busy");
            RI_ASSERT(strstr(ri_pal_claim_last(), "fresh") != 0,
                "margin case reports busy: %s", ri_pal_claim_last());
            f = fopen(age, "wb");
            RI_ASSERT(f != 0, "stamp file rewritten again");
            if (f) {
                fprintf(f, "%ld\n", now - (long)RI_CLAIM_TAKEOVER_SEC - 30L);
                fclose(f);
            }
            RI_ASSERT(ri_pal_claim_acquire(claim) == 0,
                "locked with an OLD stamp is taken over");
            RI_ASSERT(strstr(ri_pal_claim_last(), "old") != 0,
                "reason names the stale stamp: %s", ri_pal_claim_last());
            RI_ASSERT(ri_pal_claim_acquire(claim) == 1,
                "the takeover is exclusive");
            ri_pal_claim_release(claim);
            /* Locked, and NO stamp at all: a holder that died between its
             * two writes. Nobody is refreshing anything, so this is not a
             * live instance and must not hold the instrument. */
            RI_ASSERT(mkdir(claim, 0700) == 0, "claim dir for the no-stamp case");
            RI_ASSERT(mkdir(hb, 0700) == 0, "hb locked again");
            RI_ASSERT(ri_pal_claim_acquire(claim) == 0,
                "locked with no stamp is taken over");
            RI_ASSERT(strstr(ri_pal_claim_last(), "no stamp") != 0,
                "reason names the missing stamp: %s", ri_pal_claim_last());
        }
        rmdir(hb);
        ri_pal_claim_release(claim);
        rmdir(claim);
        /* Refresh reports honestly: on a claim we no longer hold (released,
         * taken over, or wiped with RAM:) it cannot write a heartbeat and
         * says 2 rather than claiming success -- the caller uses this to
         * notice it is no longer the singleton. */
        RI_ASSERT(ri_pal_claim_refresh(claim) == 2, "refresh without the claim");
        ri_pal_claim_release(claim);
        ri_pal_claim_release(0);
    }
    rmdir(tmpd);
    RI_RESULT("pal_fslog");
}
