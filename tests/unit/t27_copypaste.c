/* t27_copypaste — M2.4 GUI tail, TC-2.10.3 (copy/paste preserves
 * accent/slide flags):
 *
 * A 16-step pattern exercising every flag bit (REST, SLIDE, ACCENT,
 * FLAM, SLIDE|ACCENT, plain) round-trips through the song file
 * codec (write → read) and the song→steps converter with note AND
 * flags bit-identical at every step. Step flag bits are numerically
 * equal to the walker RI_STEP_* bits by contract (t1_formats §16),
 * so preservation here is preservation into playback.
 */
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "project/rbng.h"
#include "engine/seq/sched.h"
#include "engine/seq/songsteps.h"

static int fails = 0;
#define CHECK(cond, ...) do { \
    if (!(cond)) { printf("FAIL "); printf(__VA_ARGS__); printf("\n"); fails++; } \
} while (0)

int main(void) {
    /* note, flags — every flag bit + combinations + plain + rest */
    static const uint8_t NOTES[16] = {
        60, 62, 64, 65, 67, 69, 71, 72,
        60, 62, 64, 65, 67, 69, 71, 72
    };
    static const uint8_t FLAGS[16] = {
        0x00u, 0x01u, 0x02u, 0x04u, 0x08u, 0x03u, 0x00u, 0x02u,
        0x01u, 0x00u, 0x08u, 0x04u, 0x02u, 0x03u, 0x01u, 0x00u
    };
    struct RISong w, r;
    struct RIStep steps[64];
    char err[192];
    uint32_t i, n;
    rbng_song_init(&w);
    w.tempo = 140;
    w.ppq = 96;
    w.nsteps = 16;
    for (i = 0; i < 16; i++) {
        w.steps[i].note = NOTES[i];
        w.steps[i].flags = FLAGS[i];
    }
    CHECK(rbng_write_song("/tmp/ri/run/t27_copy.rbng", &w, err,
        sizeof err) == 0, "write: %s", err);
    memset(&r, 0, sizeof r);
    CHECK(rbng_read_song("/tmp/ri/run/t27_copy.rbng", &r, err,
        sizeof err) == 0, "read: %s", err);
    CHECK(r.nsteps == 16, "nsteps %u", r.nsteps);
    for (i = 0; i < 16; i++) {
        CHECK(r.steps[i].note == NOTES[i], "step %u note %u want %u", i,
            r.steps[i].note, NOTES[i]);
        CHECK(r.steps[i].flags == FLAGS[i], "step %u flags 0x%02x want 0x%02x",
            i, r.steps[i].flags, FLAGS[i]);
    }
    /* converter preserves into the walker domain */
    n = ri_song_to_steps(&r, steps, 64);
    CHECK(n == 16, "converted %u", n);
    for (i = 0; i < n; i++) {
        CHECK(steps[i].note == NOTES[i], "conv %u note", i);
        CHECK(steps[i].flags == FLAGS[i], "conv %u flags 0x%02x", i,
            steps[i].flags);
    }
    printf("copypaste: 16 steps note+flags identical thru file+convert\n");

    if (fails)
        printf("FAIL %d\n", fails);
    else
        printf("PASS t27_copypaste\n");
    return fails != 0;
}
