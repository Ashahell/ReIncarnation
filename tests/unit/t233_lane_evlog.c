/* t233_lane_evlog — the lane-log analyser must name the known defects, and
 * must NOT cry wolf on a session that is clean.
 *
 * WHY THIS TEST EXISTS. In one session the same shape -- a control that looked
 * live and was not -- produced six faults. Every one was found by the owner
 * pressing something, hearing a silence, and me then reading the log. Two of
 * those rounds were lost to a subtler mistake: a symptom assembled from
 * evidence that never overlapped. The owner moved Delay F.Back 142 times in a
 * session where the delay send sat at 0, and separately raised the send to 64 in
 * a session that never touched a delay knob. Neither log shows a fault. Both
 * read as "the FX knobs do nothing", and two ear-proof cycles went into proving
 * they did not.
 *
 * `scripts/lane_evlog.py` exists so that class of mistake is a `grep`, not an
 * ear-proof round. This test replays the CAPTURED logs -- which are evidence
 * already on disk -- and requires the findings that are known to be true.
 *
 * THE ANTI-TAUTOLOGY GUARD IS THE SECOND HALF, and it is the half that matters.
 * A finding tool that reports everything is worse than none, because it trains
 * the reader to skim. The synthetic session below is deliberately clean --
 * the mixer IS touched, the delay send IS opened, switches alternate -- and the
 * analyser must find NOTHING in it. A tool that fires on clean input fails here.
 *
 * THIS TEST WAS NEVER RED: the analyser already existed when it was written, so
 * it is a PIN, and `tests/mutate_lane_evlog.py` is what gives it teeth.
 */
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "tests/helpers/ri_assert.h"
#include <unistd.h>

#ifndef RI_REPO_ROOT
#define RI_REPO_ROOT "."
#endif

static char *run(const char *log) {
    static char buf[65536];
    char cmd[1024];
    FILE *f;
    size_t n;
    snprintf(cmd, sizeof cmd,
        "cd '%s' && python3 scripts/lane_evlog.py --json '%s' 2>/dev/null",
        RI_REPO_ROOT, log);
    f = popen(cmd, "r");
    if (!f) return 0;
    n = fread(buf, 1u, sizeof buf - 1u, f);
    buf[n] = '\0';
    pclose(f);
    return n ? buf : 0;
}

static int has(const char *json, const char *code, const char *log) {
    return json && strstr(json, code) != 0 && (!log || strstr(json, log) != 0);
}

/* Build a session that is CLEAN by construction: the mixer is used, the delay
 * send is opened, and every switch alternates 1,0,1,0 so nothing repeats. */
static void write_clean(const char *path) {
    FILE *f = fopen(path, "w");
    int i;
    if (!f) return;
    fprintf(f, "ev 1 0 RUN frames=256 vol=RAM: build=clean\n");
    for (i = 0; i < 12; i++)
        fprintf(f, "ev %d 100 CTL %04x=%d\n", i + 2, 0x1404, 64);   /* send, static */
    for (i = 0; i < 40; i++)
        fprintf(f, "ev %d 200 CTL %04x=%d\n", 20 + i, 0x0900, i & 1); /* PCF: alt */
    for (i = 0; i < 30; i++)
        fprintf(f, "ev %d 300 CTL %04x=%d\n", 70 + i, 0x0a05, 127 - i);/* F.Back */
    for (i = 0; i < 20; i++)
        fprintf(f, "ev %d 400 CTL %04x=%d\n", 110 + i, 0x0406, i & 1);/* insert */
    fclose(f);
}

int main(void) {
    char path[512];
    char *j;

    /* ---- the delay finding, on the two captures it explains ----
     * Both sessions moved delay parameters and never opened a mixer control.
     * That is the whole explanation for "Delay in FX does nothing", and it
     * cost two ear-proof cycles to establish by hand. */
    j = run("docs/evidence/midi/dell8-pfx4-earproof.log");
    RI_ASSERT(j != 0, "the analyser produced output for dell8");
    RI_ASSERT(has(j, "DELAY_SEND_NEVER_TOUCHED", 0),
        "dell8: delay params moved with the send never opened");
    RI_ASSERT(has(j, "MIXER_NEVER_TOUCHED", 0),
        "dell8: no mixer control was ever moved");

    j = run("docs/evidence/midi/dell11-pfx4-delay-still-silent.log");
    RI_ASSERT(j != 0, "the analyser produced output for dell11");
    RI_ASSERT(has(j, "DELAY_SEND_NEVER_TOUCHED", 0),
        "dell11: the same finding, independently");

    /* ---- the switch finding, ASSERTED PRESENT ----
     * mutate_lane_evlog m4 ("the switch finding never fires") SURVIVED the
     * first version of this test, because the switch finding was only ever
     * checked for ABSENCE on the clean session. A finding asserted only in its
     * negative is untested. dell8's 0a00 reads 110101101 -- two 1s in a row --
     * so the finding must be reported there. */
    RI_ASSERT(has(j, "SWITCH_REPEATED_VALUE", 0),
        "dell8: a 0/1 enable switch reported the same value in a row");

    /* ---- a capture nobody analysed: 1077 panel events, zero mixer ----
     * fxcheck-ev.log sat on disk for the whole session unexamined. It has the
     * same shape, which is the point: the tool finds it without anyone having
     * thought to look. */
    j = run("docs/evidence/midi/fxcheck-ev.log");
    RI_ASSERT(j != 0, "the analyser produced output for fxcheck");
    RI_ASSERT(has(j, "MIXER_NEVER_TOUCHED", 0),
        "fxcheck: 1077 panel events and the mixer was never touched");

    /* ---- THE DISCRIMINATOR ----
     * A session that DID use the mixer must not be told it did not. Without
     * this the tool could fire on everything and still pass the three above. */
    j = run("docs/evidence/midi/m3-owner-fail.log");
    RI_ASSERT(j != 0, "the analyser produced output for m3-owner-fail");
    RI_ASSERT(!has(j, "MIXER_NEVER_TOUCHED", 0),
        "m3-owner-fail touched the mixer 78 times -- it must NOT be flagged");
    RI_ASSERT(!has(j, "DELAY_SEND_NEVER_TOUCHED", 0),
        "and must not be told the delay send was closed");

    /* ---- the anti-tautology guard ---- */
    snprintf(path, sizeof path, "/tmp/ri-t233-clean-%u.log", (unsigned)getpid());
    write_clean(path);
    j = run(path);
    RI_ASSERT(j != 0, "the analyser produced output for the clean session");
    RI_ASSERT(!has(j, "MIXER_NEVER_TOUCHED", 0),
        "a clean session must not be flagged MIXER_NEVER_TOUCHED");
    RI_ASSERT(!has(j, "DELAY_SEND_NEVER_TOUCHED", 0),
        "a clean session must not be flagged DELAY_SEND_NEVER_TOUCHED");
    RI_ASSERT(!has(j, "SWITCH_REPEATED_VALUE", 0),
        "a clean session whose switches alternate must not be flagged");
    RI_ASSERT(!has(j, "NO_PANEL_PUSH", 0),
        "a clean session has panel pushes");
    remove(path);

    /* ---- an empty session must say so, not pass silently ---- */
    snprintf(path, sizeof path, "/tmp/ri-t233-empty-%u.log", (unsigned)getpid());
    { FILE *f = fopen(path, "w"); if (f) { fprintf(f, "ev 1 0 RUN build=x\n"); fclose(f); } }
    j = run(path);
    RI_ASSERT(has(j, "NO_PANEL_PUSH", 0),
        "a session with no panel traffic must be reported, not passed");
    remove(path);

    RI_RESULT("lane_evlog");
    return 0;
}
