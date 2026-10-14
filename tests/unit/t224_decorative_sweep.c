/* t224_decorative_sweep — a sweep for controls that LOOK live and are not.
 *
 * Owner ear proof, 2026-10-14, mixer:
 *
 *   "all meters remain lit after you turn their panel off using the green
 *    square. ... all panels remain audible as well. Looks like the green
 *    square has no effect at all (other then changing color when pressed)."
 *
 * Proven cause, all five strip rows:
 *
 *   R(MIX_LEVI, 0, SWITCH, "", "On/Off", 0, 1, 1, RI_MIDI_CC_NONE, 0, NONE, 0, 0)
 *
 * bind=NONE and automatable=0, and there is no RI_AUTO_MIX_MUTE anywhere in
 * the engine -- the concept does not exist. So the press toggles
 * b->val[strip][0] (hence the colour change), ri_ctlreg_auto_id() returns 0,
 * ri_panel_ctl_send() reports "already current", and the push is skipped.
 * Nothing reaches the engine: the strip stays audible and its meter keeps
 * moving.
 *
 * THIS IS THE FOURTH control this session that looked live and was not:
 *   1. P-FX1  the four FX On/Off switches -- had a target, unbound
 *   2. P-FX4  the displaced FX lamp      -- right model, stale pixels
 *   3. the mixer insert rockers          -- right model, stale pixels
 *   4. the strip mute                    -- no target at all (this test)
 *
 * Each was found one at a time because each is separate code. This sweep
 * looks for the SHAPE instead: an INTERACTIVE control that has hit-testable
 * geometry but cannot leave the panel.
 *
 * THE INVARIANT. A control that a user can click is one of KNOB / FADER /
 * SWITCH / BUTTON / STEP / SELECTOR, and if it has geometry then something
 * draws it and something hit-tests it. Such a row must be deliverable
 * (ri_ctlreg_auto_id() != 0) unless it is on the allowlist below with a
 * stated reason.
 *
 * WHAT IS DELIBERATELY NOT IN SCOPE, and why the allowlist is not a loophole:
 *   - METER rows are display-only and MUST stay unbound. t218/t219 pin
 *     that as a deliberate negative control; binding them would wire up
 *     controls with no business being writable.
 *   - LED / DISPLAY / LEGEND are not interactive and are excluded by kind.
 *   - Every allowlist entry carries the owner's reason for the control being
 *     inert. An entry with no reason is a finding, not an exemption.
 */
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "tests/helpers/ri_assert.h"
#include "gui/ctlreg.h"
#include "gui/panelgeo.h"

/* Controls a user can click. */
static int interactive(uint8_t k) {
    return k == RI_CK_KNOB || k == RI_CK_FADER || k == RI_CK_SWITCH
        || k == RI_CK_BUTTON || k == RI_CK_STEP || k == RI_CK_SELECTOR;
}

/* OWNER-EXEMPT, as RULES rather than ids so the list cannot rot as the
 * registry grows. Each entry carries the owner's reason; an exemption with no
 * reason is a finding.
 *
 * An exemption only ever lets an UNDELIVERABLE row pass. If a row on this
 * list later becomes deliverable it passes anyway, so a rule can never hide a
 * newly-wired control -- it can only excuse one the owner ruled inert. */
static const char *exempt_why(const struct RICtlDef *d) {
    /* Left-column MASTER (LEVI 69..73, "Master"/"Control 1..5"). Owner asked
     * what it drives on 2026-10-14 and the answer is still unknown: no engine
     * control exists for it, and t220 keeps it inert rather than inventing a
     * target. ReBirth's hardware MASTER is a velocity/level control this port
     * has no equivalent for. */
    if (d->section == RI_SEC_LEVI && d->group && strcmp(d->group, "Master") == 0)
        return "left-column MASTER: no engine control exists (t220, owner)";
    return 0;
}

int main(void) {
    unsigned sec, idx;
    int offenders = 0, exempt_seen = 0;

    for (sec = 0; sec < 32u; sec++) {
        const struct RIGeoSection *g = ri_geo_section(sec);
        if (!g)
            continue;                     /* section not laid out at all */
        for (idx = 0; idx < 256u; idx++) {
            uint16_t id = (uint16_t)((sec << 8) | idx);
            const struct RICtlDef *d = ri_ctlreg_find(id);
            int x0, y0, x1, y1;
            const char *why = 0;
            if (!d || !interactive(d->kind))
                continue;
            if (ri_geo_bbox(g, id, 0, &x0, &y0, &x1, &y1) != 0)
                continue;                 /* nothing draws or hit-tests it */
            why = exempt_why(d);
            if (why) { exempt_seen++; continue; }
            if (ri_ctlreg_auto_id(d) != 0u)
                continue;                 /* deliverable */
            printf("  DECORATIVE %04x  sec=%u idx=%u kind=%u  \"%s\"/\"%s\" "
                "cc=%u auto=%u bind=%u\n", id, sec, idx, d->kind,
                d->group ? d->group : "", d->legend ? d->legend : "",
                d->midi_cc, d->automatable, d->bind);
            offenders++;
        }
    }

    /* The sweep must still be looking at something, or it is vacuous: an
     * allowlist that grows to cover every interactive control proves
     * nothing. This pins that the population is large. */
    RI_ASSERT(ri_ctlreg_count() > 300u, "registry populated (%u rows)",
        ri_ctlreg_count());
    /* The exemptions must still be matching real rows, or the sweep has
     * quietly stopped exempting anything and the count means nothing. */
    RI_ASSERT(exempt_seen > 0, "exemptions still match %d real rows", exempt_seen);

    /* NOT a gate, and deliberately so. The first run flagged 135 rows, of
     * which only 5 were real: the invariant "no engine auto_id => decorative"
     * is UNSOUND, because a control can be entirely functional while acting
     * on panel state rather than on the engine. The pattern editor's Step 1-16
     * and Pitch C-C+ buttons write steps; Transport's Play/Stop/Record/Tap
     * drive the shell; Bank/Pattern/Shuffle are panel selection. All have no
     * auto_id and all do real work.
     *
     * So this reports rather than asserts, and prints the offenders for
     * triage. Gating on it would have failed the audit 135 times and taught
     * everyone to ignore it.
     *
     * THE SIGNAL, from the first run, and it is the confirmed ear defect:
     *   0400 0500 0600 0700 1400  ""/"On/Off"  -- all five mixer strip mutes
     * surfaced with no prior knowledge of which sections they lived in.
     *
     * A real gate needs the missing half: a declaration of what each control
     * is *supposed* to do. That is owner knowledge, not something a sweep can
     * infer, and guessing it is how a test ends up asserting nothing. */
    printf("  SWEEP: %d interactive+hit-testable row(s) with no engine target;\n"
           "         %d owner-exempt; %d need triage\n",
        offenders + exempt_seen, exempt_seen, offenders);
    RI_RESULT("decorative_sweep");
    return 0;
}
