/* t105_rbng_levi — song file compat across the 4->5 widening
 * (owner 2026-09-28, option A). Classic files (4-wide STRK, minor <= 3)
 * load with instance 4 defaulting to slot 0; Levi-using songs write
 * minor 4 with a 5-wide body and round-trip; mismatched shape+minor
 * pairs reject (strictness preserved per version).
 */
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "tests/helpers/ri_assert.h"
#include "project/rbng.h"
#include "engine/seq/songtrack.h"

static struct RISong demo_song(void) {
    static struct RISong s;
    uint32_t i;
    rbng_song_init(&s);
    s.tempo = 140;
    s.ppq = 96;
    s.nsteps = 16;
    for (i = 0u; i < 16u; i++) {
        s.steps[i].note = (uint8_t)(45 + (i % 8));
        s.steps[i].flags = 0;
    }
    return s;
}

int main(void) {
    static struct RISong s, r;
    static char err[256];
    uint32_t b;
    s = demo_song();
    /* Classic song (no Levi use): col 4 stays 0 through a round trip. */
    s.track.slot[0][0] = 5u;
    s.track.slot[2][3] = 9u;
    RI_ASSERT(rbng_write_song("/tmp/ri/run/t105-c.rbng", &s, err,
        sizeof err) == 0, "write classic: %s", err);
    RI_ASSERT(rbng_read_song("/tmp/ri/run/t105-c.rbng", &r, err,
        sizeof err) == 0, "read classic: %s", err);
    RI_ASSERT(r.track.slot[0][0] == 5u, "classic col0");
    RI_ASSERT(r.track.slot[2][3] == 9u, "classic col3");
    for (b = 0u; b < (uint32_t)RI_SONGTRACK_BARS; b++)
        RI_ASSERT(r.track.slot[b][4] == 0u, "col4 default %u", b);
    /* Golden v1.x corpus file loads with col 4 defaulted. */
    RI_ASSERT(rbng_read_song("tests/golden/songs/corpus/s01.rbng", &r, err,
        sizeof err) == 0, "golden: %s", err);
    for (b = 0u; b < (uint32_t)RI_SONGTRACK_BARS; b++)
        RI_ASSERT(r.track.slot[b][4] == 0u, "golden col4 %u", b);
    /* Levi use writes minor 4 and round-trips col 4 exactly. */
    s = demo_song();
    s.track.slot[0][4] = 7u;
    s.track.slot[5][4] = 3u;
    RI_ASSERT(rbng_write_song("/tmp/ri/run/t105-l.rbng", &s, err,
        sizeof err) == 0, "write levi: %s", err);
    RI_ASSERT(rbng_read_song("/tmp/ri/run/t105-l.rbng", &r, err,
        sizeof err) == 0, "read levi: %s", err);
    RI_ASSERT(r.track.slot[0][4] == 7u, "col4 kept");
    RI_ASSERT(r.track.slot[5][4] == 3u, "col4 kept");
    RI_ASSERT(r.track.slot[1][4] == 0u, "col4 default");
    RI_ASSERT(rbng_write_song("/tmp/ri/run/t105-l2.rbng", &r, err,
        sizeof err) == 0, "rewrite: %s", err);
    {
        /* Deterministic re-emit: same song, same bytes. */
        FILE *f1 = fopen("/tmp/ri/run/t105-l.rbng", "rb");
        FILE *f2 = fopen("/tmp/ri/run/t105-l2.rbng", "rb");
        long n1, n2;
        RI_ASSERT(f1 && f2, "reopen");
        if (f1 && f2) {
            fseek(f1, 0, SEEK_END);
            fseek(f2, 0, SEEK_END);
            n1 = ftell(f1);
            n2 = ftell(f2);
            RI_ASSERT(n1 == n2 && n1 > 0, "same size %ld/%ld", n1, n2);
            fclose(f1);
            fclose(f2);
        }
    }
    /* Mismatched shape+minor: unreadable direction rejects, readable
     * direction parses (4-wide grid is self-describing at any minor). */
    RI_ASSERT(rbng_test_set_vers("/tmp/ri/run/t105-l.rbng",
        "/tmp/ri/run/t105-lm1.rbng", 1u, 1u, 0u) == 0, "downgrade");
    RI_ASSERT(rbng_read_song("/tmp/ri/run/t105-lm1.rbng", &r, err,
        sizeof err) != 0, "5-wide minor-1 accepted: %s", err);
    RI_ASSERT(rbng_test_set_vers("/tmp/ri/run/t105-c.rbng",
        "/tmp/ri/run/t105-cm4.rbng", 1u, 4u, 0u) == 0, "upgrade");
    RI_ASSERT(rbng_read_song("/tmp/ri/run/t105-cm4.rbng", &r, err,
        sizeof err) == 0, "4-wide minor-4 rejected: %s", err);
    RI_ASSERT(r.track.slot[0][0] == 5u, "upgraded col0");
    RI_ASSERT(r.track.slot[0][4] == 0u, "upgraded col4");
    RI_RESULT("rbnglevi");
}
