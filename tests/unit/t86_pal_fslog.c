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

/* The stamp file's on-disk layout, written here by hand so the test pins
 * the format rather than trusting the implementation to agree with itself.
 * platform/pal/ri_pal_fs.h documents the same five fields. */
static int stamp_write(const char *claim, unsigned long mode, long holder,
    long stamp, long modal_until) {
    char path[640];
    FILE *f;
    snprintf(path, sizeof(path), "%s/hb.age", claim);
    f = fopen(path, "wb");
    if (!f)
        return 1;
    fprintf(f, "%lu %lu %ld %ld %ld\n", 0x52495043ul, mode, holder, stamp,
        modal_until);
    fclose(f);
    return 0;
}

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
    /* Single-instance claim (2026-10-18, revised twice by review). The claim
     * is a directory whose creation is the atomic test-and-set, plus two
     * files inside it: `hb`, the LIVENESS LOCK (the holder keeps it open),
     * and `hb.age`, a stamp that is never held open and therefore always
     * readable. The split exists because a locked file cannot be read: with
     * the age locked away, a wedged holder looks exactly like a live one.
     *
     * The stamp layout is part of the contract and is written here by hand:
     * magic mode holder stamp modal_until. */
    {
        char claim[512], hb[576], age[640], dead[640];
        long now = (long)time(0);
        snprintf(claim, sizeof(claim), "%s/t86-claim", tmpd);
        snprintf(hb, sizeof(hb), "%s/hb", claim);
        snprintf(age, sizeof(age), "%s/hb.age", claim);
        snprintf(dead, sizeof(dead), "%s/t86-no-such-parent/claim", tmpd);
        rmdir(age); rmdir(hb); rmdir(claim);

        /* acquire -> 0, a second holder -> 1, refresh -> 0 */
        RI_ASSERT(ri_pal_claim_acquire(claim) == 0, "claim taken");
        RI_ASSERT(ri_pal_claim_acquire(claim) == 1, "second acquire is busy");
        RI_ASSERT(ri_pal_claim_refresh(claim) == 0, "refresh");
        RI_ASSERT(ri_pal_claim_acquire(claim) == 1, "live holder still busy");
        ri_pal_claim_release(claim);
        RI_ASSERT(ri_pal_claim_acquire(claim) == 0, "released then retaken");
        ri_pal_claim_release(claim);

        /* Dead holder: a stamp older than the takeover threshold. This is
         * the wedge/crash path, and it is the reason the age lives in its
         * own file. */
        RI_ASSERT(mkdir(claim, 0700) == 0, "stale dir made by hand");
        RI_ASSERT(stamp_write(claim, 0u, 1L,
            now - (long)RI_CLAIM_TAKEOVER_SEC - 30L, 0L) == 0, "stale stamp written");
        RI_ASSERT(ri_pal_claim_acquire(claim) == 0, "stale claim taken over");
        RI_ASSERT(ri_pal_claim_acquire(claim) == 1, "takeover is exclusive");
        ri_pal_claim_release(claim);

        /* The margin: past the staleness window but inside the takeover
         * margin, a LOCKED holder stays busy. That margin is what stops a
         * contender from stealing a claim from a live instance. */
        RI_ASSERT(mkdir(claim, 0700) == 0, "margin dir");
        RI_ASSERT(mkdir(hb, 0700) == 0, "hb locked for the margin case");
        RI_ASSERT(stamp_write(claim, 0u, 1L,
            now - (long)RI_CLAIM_STALE_SEC - 10L, 0L) == 0, "margin stamp");
        RI_ASSERT(ri_pal_claim_acquire(claim) == 1, "inside the margin: busy");

        /* Modal holder (2026-10-18 review, second pass): the main loop is
         * blocked in a requester, so nothing refreshes the stamp -- yet the
         * instance is perfectly healthy and must NOT be evicted. The modal
         * deadline is the only signal that can tell those apart from
         * outside the process. */
        RI_ASSERT(stamp_write(claim, 1u, 1L,
            now - (long)RI_CLAIM_TAKEOVER_SEC - 30L,
            now + (long)RI_CLAIM_MODAL_SEC) == 0, "modal stamp written");
        RI_ASSERT(ri_pal_claim_acquire(claim) == 1,
            "locked, stale, and modal -> still busy");
        RI_ASSERT(strstr(ri_pal_claim_last(), "modal") != 0,
            "reason names the modal holder: %s", ri_pal_claim_last());
        /* ...and once the deadline passes, the same state is a takeover. */
        RI_ASSERT(stamp_write(claim, 1u, 1L,
            now - (long)RI_CLAIM_TAKEOVER_SEC - 30L,
            now - 1L) == 0, "expired modal stamp");
        RI_ASSERT(ri_pal_claim_acquire(claim) == 0,
            "modal deadline expired -> taken over");

        /* Locked with no stamp at all: a holder that died between its two
         * writes. Nobody is refreshing anything, so it must not hold the
         * instrument. */
        ri_pal_claim_release(claim);
        RI_ASSERT(mkdir(claim, 0700) == 0, "no-stamp dir");
        RI_ASSERT(mkdir(hb, 0700) == 0, "hb locked for the no-stamp case");
        RI_ASSERT(ri_pal_claim_acquire(claim) == 0,
            "locked with no stamp is taken over");
        RI_ASSERT(strstr(ri_pal_claim_last(), "no stamp") != 0,
            "reason names the missing stamp: %s", ri_pal_claim_last());

        /* Undecidable: the parent does not exist. */
        RI_ASSERT(ri_pal_claim_acquire(dead) == 2, "undecidable is 2");
        RI_ASSERT(ri_pal_claim_acquire(0) == 2, "null path is 2");
        RI_ASSERT(strstr(ri_pal_claim_last(), "no path") != 0 ||
            strstr(ri_pal_claim_last(), "undecidable") != 0,
            "reason names the undecidable case: %s", ri_pal_claim_last());

        /* DISPLACEMENT DETECTION (2026-10-18 review, second pass). Writing
         * the stamp by path is not ownership: after a takeover the old
         * instance's refresh used to write straight into the new owner's
         * directory, succeed, and leave both instances convinced they held
         * the claim. The stamp names its holder, so refresh must notice. */
        ri_pal_claim_release(claim);
        RI_ASSERT(ri_pal_claim_acquire(claim) == 0, "claim for the loss test");
        RI_ASSERT(stamp_write(claim, 0u, 999L, now, 0L) == 0,
            "a foreign holder stamps the claim");
        RI_ASSERT(ri_pal_claim_refresh(claim) == RI_CLAIM_REFRESH_LOST,
            "refresh reports the displacement");
        RI_ASSERT(strstr(ri_pal_claim_last(), "displaced") != 0,
            "reason names the displacement: %s", ri_pal_claim_last());
        /* The modal stamp is refused once we are not the holder. */
        RI_ASSERT(ri_pal_claim_modal(claim, 1) != 0,
            "no modal stamp without the claim");

        /* Refresh on a claim we do not hold reports honestly (2). */
        ri_pal_claim_release(claim);
        RI_ASSERT(ri_pal_claim_refresh(claim) == 2, "refresh without the claim");
        ri_pal_claim_release(0);
        ri_pal_claim_modal(0, 1);
    }
    rmdir(tmpd);
    RI_RESULT("pal_fslog");
}
