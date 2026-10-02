/* t158_audio_failure_loud — a lost audio path must not be silent.
 *
 * Why this exists (owner 2026-10-02): two RIAPP instances on one Dell. The
 * first held ahi.device; the second failed AHI_AllocAudioA and fell back to
 * the null backend. app/riapp.c logged exactly one line --
 *
 *   audio: AHI unavailable - null backend active (offline render only) [err 4]
 *
 * -- and carried on. The window opened, the panels drew, the playlist loaded
 * and the transport played, with no sound and NO requester. Worse than a
 * requester: a requester at least says something happened. Proven on
 * hardware, deliberately: two processes in `status`, two panels in the window
 * list, zero requesters.
 *
 * The law:
 *   - err == RI_AUDIO_ERR_ABSENT (ahi.device would not open, i.e. no AHI
 *     hardware) is the DOCUMENTED offline-render fallback and stays quiet.
 *     This carve-out is load-bearing: RI_AUDIO_NULL_MSG is byte-exact and
 *     pinned by tests/unit/t6_w1backend.c, and a machine with no sound card
 *     must not be nagged about it.
 *   - every other step means a device WAS reachable and the audio path was
 *     lost -- alloc (4, the contended case), load, task, port, open timeout,
 *     and err 0 ("opened fine, then au_live_run failed"). All of those are
 *     reported.
 *   - an unrecognised code is reported too (fail loud, never fail silent).
 */
#include <stdio.h>
#include "app/core/riapp_core.h"
#include "tests/helpers/ri_assert.h"

/* The seven failure steps audio_ahi_live.h documents, and the verdict for
 * each. err 2 is the only quiet one; asserting the whole table (rather than
 * "everything except 2 is loud") is what makes a mutant that flips a single
 * comparison fail here instead of surviving. */
static const struct { long err; int loud; const char *what; } kTable[] = {
    { 1L, 1, "msgport" },
    { RI_AUDIO_ERR_ABSENT, 0, "ahi.device absent -> documented offline render" },
    { 3L, 1, "mode" },
    { 4L, 1, "AHI_AllocAudioA failed (another AHI client holds the card)" },
    { 5L, 1, "AHI_LoadSound" },
    { 6L, 1, "render task / signal" },
    { 7L, 1, "open handshake timeout" },
};

int main(void) {
    unsigned i;
    int quiet = 0, loud = 0;

    for (i = 0; i < sizeof kTable / sizeof kTable[0]; i++) {
        int got = ri_core_audio_failure_is_loud(kTable[i].err);
        RI_ASSERT(got == kTable[i].loud, "err %ld (%s): loud=%d, want %d",
            kTable[i].err, kTable[i].what, got, kTable[i].loud);
        if (kTable[i].loud) loud++; else quiet++;
    }

    /* Exactly one documented step is the quiet hardware-absent case. If a
     * future step is added this fails, which is the point: a new failure mode
     * must be classified deliberately, not fall through as silent. */
    RI_ASSERT(loud == 6, "loud steps %d, want 6", loud);
    RI_ASSERT(quiet == 1, "quiet steps %d, want 1", quiet);

    /* The carve-out must name the same value the AHI backend sets. If
     * RI_AUDIO_ERR_ABSENT is ever renumbered this is where it shows up: the
     * absent case would start nagging, or -- worse -- a real failure would
     * start passing quietly. */
    RI_ASSERT(RI_AUDIO_ERR_ABSENT == 2L, "absent step is %ld, want 2",
        RI_AUDIO_ERR_ABSENT);

    /* err 0 means au_live_open succeeded and au_live_run then failed: the
     * device was there and the path died anyway. Loud, or the user is again
     * left with a silent app. */
    RI_ASSERT(ri_core_audio_failure_is_loud(0L) == 1, "err 0 (run failed) must be loud");

    /* Fail loud on anything unrecognised. A silent unknown is how this defect
     * shipped in the first place. */
    RI_ASSERT(ri_core_audio_failure_is_loud(-1L) == 1, "unknown -1 must be loud");
    RI_ASSERT(ri_core_audio_failure_is_loud(99L) == 1, "unknown 99 must be loud");

    /* The reported verdict is strictly boolean, not merely non-zero. */
    RI_ASSERT(ri_core_audio_failure_is_loud(4L) == 1, "contended alloc must be exactly 1");
    RI_ASSERT(ri_core_audio_failure_is_loud(RI_AUDIO_ERR_ABSENT) == 0,
        "absent must be exactly 0");

    RI_RESULT("audio_failure_loud");
}