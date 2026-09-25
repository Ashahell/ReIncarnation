/* t59_songtrack — §12.9b song track model.
 * Task 1 first (RED: songtrack.h does not exist yet).
 */
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "tests/helpers/ri_assert.h"
#include "engine/seq/songtrack.h"

/* Song geometry comes from transport.h; the track must not fork it. */
typedef char ri_t59_bars_match[(RI_SONGTRACK_BARS == RI_SONG_BARS) ? 1 : -1];

int main(void) {
    struct RISongTrack t;
    uint64_t b;
    uint32_t i;
    (void)sizeof(ri_t59_bars_match);
    RI_ASSERT(RI_SONGTRACK_BARS == 999u, "bars %u", RI_SONGTRACK_BARS);
    ri_track_init(&t);
    RI_ASSERT(ri_track_is_empty(&t) == 1, "fresh empty");
    RI_ASSERT(ri_track_selected(&t, 0u, 0u) == 0u, "init slot");
    RI_ASSERT(ri_track_selected(&t, 998u, 3u) == 0u, "init tail");
    /* Fail-closed reads: bar 999/5000, instance 4/255, NULL track. */
    RI_ASSERT(ri_track_selected(&t, 0u, 4u) == 0u, "read inst 4");
    RI_ASSERT(ri_track_selected(&t, 0u, 255u) == 0u, "read inst 255");
    RI_ASSERT(ri_track_selected(0, 0u, 0u) == 0u, "read null");
    /* Boundary reads are pinned through a WRAPPER, so a botched range check
     * lands in `guard` (a wrong value and a wrong byte) instead of reading
     * undefined memory past a bare array and happening to see zero. */
    {
        struct { struct RISongTrack t; unsigned char guard[32]; } w;
        uint32_t g;
        memset(&w, 0x5A, sizeof w);
        ri_track_init(&w.t);
        RI_ASSERT(ri_track_selected(&w.t, 999u, 0u) == 0u, "read bar 999");
        RI_ASSERT(ri_track_selected(&w.t, 5000u, 0u) == 0u, "read bar 5000");
        for (g = 0u; g < sizeof w.guard; g++)
            RI_ASSERT(w.guard[g] == 0x5A, "read past the grid");
    }
    /* Capture writes exactly; neighbors untouched. */
    RI_ASSERT(ri_track_capture(&t, 5u, 1u, 7u) == 0, "capture rc");
    RI_ASSERT(ri_track_selected(&t, 5u, 1u) == 7u, "capture stored");
    RI_ASSERT(ri_track_is_empty(&t) == 0, "non-empty");
    RI_ASSERT(ri_track_selected(&t, 4u, 1u) == 0u, "neighbor low");
    RI_ASSERT(ri_track_selected(&t, 6u, 1u) == 0u, "neighbor high");
    RI_ASSERT(ri_track_selected(&t, 5u, 0u) == 0u, "neighbor inst");
    /* Same-bar re-capture overwrites. */
    RI_ASSERT(ri_track_capture(&t, 5u, 1u, 3u) == 0, "recapture rc");
    RI_ASSERT(ri_track_selected(&t, 5u, 1u) == 3u, "recapture wins");
    /* Refusals: nothing stored, neighbors untouched. */
    RI_ASSERT(ri_track_capture(&t, 999u, 0u, 9u) == 2, "cap bar 999");
    RI_ASSERT(ri_track_capture(&t, 0u, 4u, 9u) == 2, "cap inst 4");
    RI_ASSERT(ri_track_capture(&t, 0u, 0u, 32u) == 2, "cap slot 32");
    RI_ASSERT(ri_track_capture(0, 0u, 0u, 1u) == 2, "cap null");
    RI_ASSERT(ri_track_selected(&t, 998u, 0u) == 0u, "refusal leak");
    /* Highest legal values land. */
    RI_ASSERT(ri_track_capture(&t, 998u, 3u, 31u) == 0, "top rc");
    RI_ASSERT(ri_track_selected(&t, 998u, 3u) == 31u, "top stored");
    /* Composition with the transport quantizer: a mid-measure flip at
     * bar 998 quantizes to 998 (never 999) and stores there. */
    RI_ASSERT(ri_bar_quantize_next(998u * 384u + 200u, 96u) == 998u, "q 998");
    RI_ASSERT(ri_track_capture(&t, ri_bar_quantize_next(998u * 384u + 200u, 96u),
        0u, 9u) == 0, "q cap rc");
    RI_ASSERT(ri_track_selected(&t, 998u, 0u) == 9u, "q cap stored");
    /* Layer guard: songtrack.h sees transport.h geometry only. */
    {
        FILE *fh = fopen("engine/seq/songtrack.h", "r");
        char line[256];
        int bad = 0, has_transport = 0;
        RI_ASSERT(fh != 0, "open header");
        if (fh) {
            while (fgets(line, sizeof line, fh)) {
                if (strstr(line, "transport.h"))
                    has_transport = 1;
                /* BANNED set, permanent: the emitter's types live in
                 * songtrack_emit.h (Task 2), so this guard is never narrowed.
                 * Never delete it to make a build pass. */
                if (strstr(line, "RISeq") || strstr(line, "RITransport") ||
                    strstr(line, "RILoop") || strstr(line, "sched.h") ||
                    strstr(line, "clock.h") || strstr(line, "pattern.h"))
                    bad = 1;
            }
            fclose(fh);
        }
        RI_ASSERT(has_transport, "no transport include");
        RI_ASSERT(!bad, "layer leak");
    }
    /* Property loop: all-999 round-trip. */
    for (b = 0u; b < RI_SONGTRACK_BARS; b++)
        for (i = 0u; i < RI_SONGTRACK_INSTANCES; i++)
            RI_ASSERT(ri_track_capture(&t, b, i, (uint8_t)(b % 32u)) == 0, "prop cap");
    for (b = 0u; b < RI_SONGTRACK_BARS; b++)
        for (i = 0u; i < RI_SONGTRACK_INSTANCES; i++)
            RI_ASSERT(ri_track_selected(&t, b, i) == (uint8_t)(b % 32u),
                "roundtrip %llu/%u", (unsigned long long)b, i);
    RI_RESULT("songtrack");
}
