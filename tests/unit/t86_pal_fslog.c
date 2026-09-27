/* t86_pal_fslog — portability T6: paths, dir scan, logging.
 * Host backend: join/list/read-write round-trip + log_format parity.
 */
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdarg.h>
#include <stdlib.h>
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
    rmdir(tmpd);
    RI_RESULT("pal_fslog");
}
