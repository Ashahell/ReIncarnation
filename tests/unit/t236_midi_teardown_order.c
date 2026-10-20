/* t236_midi_teardown_order — every exit path of riapp_main() must tear the
 * MIDI tasks down.
 *
 * WHY THIS TEST EXISTS (2026-10-20, follow-up to the O1 review). On AROS a
 * task that is still running when its process exits is a task executing
 * UNLOADED CODE. The bridge task and the clock/device sender task are
 * started by midi_prepare(), and every EARLY RETURN of riapp_main() used to
 * skip the teardown:
 *
 *   rc 6  the render-may-not-start guard (new in the 2026-10-20 split);
 *   rc 5  the canvas create failure;
 *   rc 5  the rack row failure;
 *   rc 7  the window failure;
 *   rc 8  the application failure.
 *
 * Most of those already followed the MIDI setup; the rc 6 path is newly
 * exposed by midi_prepare() moving earlier.
 *
 * The rule: EVERY return of riapp_main() that comes after the midi_prepare()
 * call must be preceded by the shared teardown, and the teardown must stop
 * the MIDI tasks. The check is structural and the COMMENTS ARE BLANKED
 * FIRST, so a comment cannot satisfy it — the 2026-10-12 lesson (a gate
 * that passed with the fix deleted, because the fix's comment carried the
 * text the search looked for).
 *
 * It is in the spirit of t235_midi_wiring_order, which pins the other half
 * of the same review: the MIDI outputs must be wired BEFORE the render
 * starts. Between them, the two gates say what the code must do in both
 * directions — set up before the render, tear down after it.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "tests/helpers/ri_assert.h"
#include "tests/helpers/ri_srcscan.h"

#ifndef RI_REPO_ROOT
#define RI_REPO_ROOT "."
#endif

int main(void) {
    char path[512];
    char *src;
    long len = 0;
    long main_start = 0, main_end = 0;
    long at_prepare, at_teardown_call = -1, at_close = -1;
    const char *root = getenv("RI_REPO_ROOT");
    if (!root || !root[0])
        root = RI_REPO_ROOT;

    snprintf(path, sizeof path, "%s/app/riapp.c", root);
    src = ri_src_slurp(path, &len);
    if (!src) {
        printf("FAIL: cannot read %s\n", path);
        RI_RESULT("midi-teardown-order");
    }
    /* Comments go first: a comment must not be able to satisfy this gate. */
    ri_src_blank_comments(src, len);

    if (ri_src_function_range(src, len, "riapp_main", &main_start, &main_end) != 0) {
        printf("FAIL: riapp_main() is not in the source\n");
        RI_RESULT("midi-teardown-order");
    }
    RI_ASSERT(main_end > main_start, "riapp_main() has no body");

    /* The teardown helper, and the MIDI close inside it. */
    {
        long th_start = 0, th_end = 0;
        RI_ASSERT(ri_src_function_range(src, len, "riapp_teardown", &th_start,
                                       &th_end) == 0,
            "riapp_teardown() is missing: there is no ONE teardown for every "
            "exit path");
        if (th_start > 0) {
            at_close = ri_src_find(src, len, "ri_pal_midi_close(", th_start);
            RI_ASSERT(at_close > 0 && at_close < th_end,
                "riapp_teardown() does not call ri_pal_midi_close(): the "
                "bridge task and the clock/device sender would stay running");
        }
    }

    /* The render may not start before the outputs are wired (t235's rule, and
     * the point after which a teardown is owed). */
    at_prepare = ri_src_find(src, len, "midi_prepare();", main_start);
    RI_ASSERT(at_prepare > main_start && at_prepare < main_end,
        "riapp_main() never calls midi_prepare()");
    if (at_prepare < 0 || at_prepare > main_end) {
        free(src);
        RI_RESULT("midi-teardown-order");
    }

    /* EVERY return after the MIDI setup must be preceded by the teardown. */
    at_teardown_call = ri_src_find(src, len, "riapp_teardown();", main_start);
    RI_ASSERT(at_teardown_call > 0,
        "riapp_main() never calls riapp_teardown(): no exit path tears the "
        "MIDI tasks down");
    {
        long at = at_prepare;
        int n_returns = 0;
        while ((at = ri_src_find(src, len, "return", at + 1)) > 0 && at < main_end) {
            n_returns++;
            RI_ASSERT(at_teardown_call > 0 && at_teardown_call < at,
                "the return at %ld comes BEFORE the teardown call at %ld: this "
                "exit path leaves the bridge and the clock/device sender "
                "running, which on AROS is a task executing unloaded code",
                at, at_teardown_call);
        }
        RI_ASSERT(n_returns >= 1, "riapp_main() has no return at all");
    }

    free(src);
    RI_RESULT("midi-teardown-order");
}
