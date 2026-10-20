/* t237_canvas_ownership — the panel's page build must not hand Zune a
 * canvas the app does not own, and a failed page must not leave the
 * canvases behind.
 *
 * WHY THIS TEST EXISTS (2026-10-20, the owner's third follow-up). Zune's
 * group creation FAILS when a Child tag is NULL, and a failing group
 * DISPOSES the children it already took: the family's dispose walks the
 * child list and disposes every child in it
 * (workbench/libs/muimaster/classes/family.c). So a device row built as
 *
 *     MUI_NewObject(MUIC_Group, ..., Child, pat, Child, voice, TAG_DONE)
 *
 * with `voice` NULL disposes `pat` -- BEHIND THE APP'S BACK -- while
 * s_canvas[] still points at it. The teardown then disposes that canvas a
 * second time: a double free, on a Zune object.
 *
 * The rule, in three parts:
 *
 *   1. a Child tag never takes a function CALL: the canvas is looked up
 *      into a local first, so Zune is never handed a lookup that can be
 *      NULL;
 *   2. the failure path DISPOSES the page it had built -- a partly built
 *      page is a leak, and it holds the canvases of the rows in it;
 *   3. the failure path RELEASES the s_canvas[] entries of the canvases
 *      that went with the page: a stale entry is a second dispose, and
 *      the teardown asks every canvas for its parent (MUIA_Parent), which
 *      on a freed canvas is a use-after-free.
 *
 * The check is structural and the COMMENTS ARE BLANKED FIRST, so a comment
 * cannot satisfy it — the 2026-10-12 lesson (a gate that passed with the
 * fix deleted, because the fix's comment carried the text the search looked
 * for). It is in the spirit of t236_midi_teardown_order, which pins the
 * other half of the same rule: the teardown must dispose what has no
 * owner, and the classes must be dropped last.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "tests/helpers/ri_assert.h"
#include "tests/helpers/ri_srcscan.h"

#ifndef RI_REPO_ROOT
#define RI_REPO_ROOT "."
#endif

/* The argument of a Child tag must be a plain name, not a call: "Child,
 * (IPTR)canvas_for_section(...)" is how a NULL reaches Zune. Returns the
 * offset of the offending "(", or -1. */
static long child_tag_call(const char *src, long len, long from) {
    long at = from;
    while ((at = ri_src_find(src, len, "Child, (IPTR)", at)) > 0) {
        long i = at + (long)strlen("Child, (IPTR)");
        while (i < len && src[i] != ',' && src[i] != ')' && src[i] != '\n') {
            if (src[i] == '(')
                return i;
            i++;
        }
        at = i;
    }
    return -1;
}

int main(void) {
    char path[512];
    char *src;
    long len = 0;
    long fn_start = 0, fn_end = 0;
    const char *root = getenv("RI_REPO_ROOT");
    if (!root || !root[0])
        root = RI_REPO_ROOT;

    snprintf(path, sizeof path, "%s/app/riapp.c", root);
    src = ri_src_slurp(path, &len);
    if (!src) {
        printf("FAIL: cannot read %s\n", path);
        RI_RESULT("canvas-ownership");
    }
    /* Comments go first: a comment must not be able to satisfy this gate. */
    ri_src_blank_comments(src, len);

    /* 1. NO CHILD TAG TAKES A CALL. This is the whole double free: a group
     *    built with a lookup that can be NULL fails, and a failing group
     *    disposes the children it already took. */
    {
        long at = child_tag_call(src, len, 0);
        RI_ASSERT(at < 0,
            "a Child tag takes a function call, at offset %ld: Zune's group "
            "creation fails on a NULL Child and a failing group DISPOSES the "
            "children it already took, so the canvas next to it is freed "
            "behind the app's back while s_canvas[] still points at it",
            at);
    }

    /* 2. THE DEVICE PAGE: its failure path disposes the page and releases
     *    the canvas entries that went with it. */
    if (ri_src_function_range(src, len, "tab_device_page", &fn_start, &fn_end) != 0) {
        printf("FAIL: tab_device_page() is not in the source\n");
        RI_RESULT("canvas-ownership");
    }
    {
        /* the failure path is the code AFTER the label: the disposal and
         * the release have to be there, or the failure path leaks the
         * page and keeps the stale entries. */
        long at_label = ri_src_find(src, len, "\nfail:", fn_start);
        long at;
        long from = fn_start;
        RI_ASSERT(at_label > fn_start && at_label < fn_end,
            "tab_device_page() has no failure path: a missing canvas has to "
            "fail the build, and it has to be the build that fails -- Zune "
            "must never be handed a NULL Child");
        if (at_label > 0)
            from = at_label;
        at = ri_src_find(src, len, "MUI_DisposeObject(page)", from);
        RI_ASSERT(at > fn_start && at < fn_end,
            "tab_device_page() never disposes the page it built: a partly "
            "built page is a leak, and it holds the canvases of the rows in it");
        at = ri_src_find(src, len, "canvas_entry_release(", from);
        RI_ASSERT(at > fn_start && at < fn_end,
            "tab_device_page() never releases the s_canvas[] entries of the "
            "canvases the disposed page took with it: a stale entry is a "
            "second dispose, and the teardown asks every canvas for its "
            "parent, which on a freed canvas is a use-after-free");
        /* the lookup must be CHECKED before the row group is created: the
         * check jumps to the failure path, and it sits between the lookup
         * and the group creation. */
        {
            long at_lookup = ri_src_find(src, len, "canvas_for_section(", fn_start);
            long at_group = ri_src_find(src, len, "MUI_NewObject(MUIC_Group, MUIA_Group_Horiz",
                                        fn_start);
            long at_fail = ri_src_find(src, len, "goto fail", fn_start);
            RI_ASSERT(at_lookup > fn_start && at_lookup < fn_end,
                "tab_device_page() never looks a canvas up");
            RI_ASSERT(at_group > fn_start && at_group < fn_end,
                "tab_device_page() never creates a device row group");
            if (at_lookup > 0 && at_group > 0) {
                RI_ASSERT(at_fail > at_lookup && at_fail < at_group,
                    "the missing-canvas check is not between the lookup (%ld) "
                    "and the row group (%ld): a NULL Child reaches Zune, and "
                    "Zune disposes the canvas next to it",
                    at_lookup, at_group);
            }
        }
    }

    /* 3. THE RACK PAGE: the same release, for the mix and FX strips. */
    if (ri_src_function_range(src, len, "rack_page_gap", &fn_start, &fn_end) == 0) {
        long at = ri_src_find(src, len, "canvas_entry_release(", fn_start);
        RI_ASSERT(at > fn_start && at < fn_end,
            "rack_page_gap() never releases the s_canvas[] entries of the "
            "canvases its slots took with it: a stale entry is a second "
            "dispose, and the teardown asks every canvas for its parent, "
            "which on a freed canvas is a use-after-free");
    }

    /* 4. THE DIAGNOSTIC THAT FORCES THE FAILURE MUST BE ARMED BY A FLAG.
     *
     * WHY THIS IS A GATE AND NOT A NOTE (2026-10-20). The switch that
     * makes the page build fail on purpose keys the empty lookup on a
     * SECTION id, and section 0 is RI_SEC_SYNTH1 -- a real section, the
     * first row of the synth tab. So an arming written as "the section
     * value, with 0 meaning not armed" silently DISABLED the switch for
     * exactly the row the lane wanted to fail: the panel built normally,
     * the lane measured a healthy run, and the A/B read as a pass. The
     * diagnostic did not fire, so it measured NOTHING.
     *
     * That is the 2026-10-12 lesson in a new shape: a check that cannot
     * fail is not a check. The arming must be a separate FLAG from the
     * section it names, and the lookup must test the flag.
     */
    {
        long at_armed = ri_src_find(src, len, "s_fail_armed = 1", 0);
        long at_test = ri_src_find(src, len, "s_fail_armed &&", 0);
        long at_def = ri_src_find(src, len, "canvas_for_section(ULONG sec)", 0);
        RI_ASSERT(at_armed > 0,
            "the forced-failure diagnostic is never ARMED: RIAPP_FAIL_PAGE "
            "cannot make the page build fail, so the A/B on the lane measures "
            "a healthy run and reads as a pass");
        RI_ASSERT(at_test > 0,
            "canvas_for_section() does not test an ARMED flag: an arming "
            "written as a section value with 0 meaning 'not armed' is dead "
            "for the synth tab's first row, because section 0 is "
            "RI_SEC_SYNTH1 -- a real section");
        /* The flag must be tested INSIDE canvas_for_section(): the lookup is
         * the only place that can come back empty. */
        if (at_test > 0 && at_def > 0)
            RI_ASSERT(at_test > at_def,
                "the armed flag is tested at %ld, BEFORE canvas_for_section() at "
                "%ld: the lookup that comes back empty must be the one that "
                "consults the flag", at_test, at_def);
    }

    free(src);
    RI_RESULT("canvas-ownership");
}
