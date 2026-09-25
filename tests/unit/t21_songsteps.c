/* t21_songsteps — song-to-steps converter unit (WBS 2.1 builder step 1).
 * Hand-built RISong (no file IO, no codec): 5 steps covering plain,
 * slide, accent, rest, flam mix -> assert count + per-step note/flags.
 * Edges: cap truncation, NULL song/out, zero steps. Walker fit is
 * proven by shape (output feeds ri_sched_emit_* directly); the
 * song-file->golden-events path is a later integration step.
 */
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "engine/seq/songsteps.h"
#include "project/rbng.h"
#include "engine/seq/sched.h"

static int fails = 0;
#define CHECK(cond, ...) do { \
    if (!(cond)) { printf("FAIL "); printf(__VA_ARGS__); printf("\n"); fails++; } \
} while (0)

int main(void) {
    static struct RISong song;
    struct RIStep out[8];
    uint32_t n;
    memset(&song, 0, sizeof song);
    song.nsteps = 5u;
    song.steps[0].note = 45; song.steps[0].flags = 0;
    song.steps[1].note = 47; song.steps[1].flags = RI_RBNG_SLIDE;
    song.steps[2].note = 48; song.steps[2].flags = RI_RBNG_ACCENT;
    song.steps[3].note = 0;  song.steps[3].flags = RI_RBNG_REST;
    song.steps[4].note = 52; song.steps[4].flags = RI_RBNG_FLAM | RI_RBNG_ACCENT;

    n = ri_song_to_steps(&song, out, 8u);
    CHECK(n == 5u, "count %u want 5", n);
    CHECK(out[0].note == 45 && out[0].flags == 0, "step0 %u/%u",
          out[0].note, out[0].flags);
    CHECK(out[1].note == 47 && out[1].flags == RI_STEP_SLIDE, "step1 %u/%u",
          out[1].note, out[1].flags);
    CHECK(out[2].note == 48 && out[2].flags == RI_STEP_ACCENT, "step2 %u/%u",
          out[2].note, out[2].flags);
    CHECK(out[3].flags == RI_STEP_REST, "step3 flags %u", out[3].flags);
    CHECK(out[4].note == 52 &&
          out[4].flags == (RI_STEP_FLAM | RI_STEP_ACCENT), "step4 %u/%u",
          out[4].note, out[4].flags);

    /* Cap truncation: cap 2 -> first two steps only. */
    n = ri_song_to_steps(&song, out, 2u);
    CHECK(n == 2u, "trunc count %u want 2", n);
    CHECK(out[1].note == 47, "trunc step1 %u", out[1].note);

    /* Null/empty edges. */
    CHECK(ri_song_to_steps(NULL, out, 8u) == 0u, "null song nonzero");
    CHECK(ri_song_to_steps(&song, NULL, 8u) == 0u, "null out nonzero");
    CHECK(ri_song_to_steps(&song, out, 0u) == 0u, "cap0 nonzero");
    song.nsteps = 0u;
    CHECK(ri_song_to_steps(&song, out, 8u) == 0u, "empty nonzero");

    if (fails)
        printf("FAIL %d\n", fails);
    else
        printf("PASS t21_songsteps\n");
    return fails != 0;
}
