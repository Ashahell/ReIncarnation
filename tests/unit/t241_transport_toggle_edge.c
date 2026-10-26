/* t241_transport_toggle_edge — a transport TOGGLE acts on the press edge.
 *
 * Why this exists. On 2026-10-26 the Dell's ev log held 1 493 800 TR PLAY /
 * TR STOP lines and NOTHING ELSE: 21.3 hours of the transport strobing at
 * 19.45 toggles/s, from ONE space key-down that was never released
 * (docs/evidence/midi/transport-storm/). A held key repeats its DOWN;
 * RI_KT_STOPPLAY reads the PLAY LED, so every repeat flipped the transport.
 *
 * The old t71 wrote that defect down as a law — two `RI_RAW_SPACE` presses
 * with no release between them, asserted as "space stops" then "space
 * plays". A keyboard never sends that: a second press is always preceded by
 * a release. So t71's two presses were one press and one repeat, and the
 * test that should have caught the strobe was the test that licensed it.
 *
 * The pins here are the ones that make the guard falsifiable:
 *   - a repeat DOWN does not toggle (the defect);
 *   - the release is not a toggle either, and it re-arms the key;
 *   - a release of a DIFFERENT key does not re-arm it;
 *   - tempo keys KEEP their auto-repeat, because holding + must ramp: the
 *     guard belongs to the toggle command, not to RI_KA_TRANSPORT.
 */
#include <stdio.h>
#include "tests/helpers/ri_assert.h"
#include "gui/panelui.h"
#include "gui/keymap.h"
#include "gui/secttr.h"

int main(void) {
    static struct RISectUI tr;
    struct RIPanelUI pu;
    int tempo0;

    ri_sui_init(&tr, RI_SEC_TRANSPORT);
    ri_panel_init(&pu);
    pu.tr = &tr;

    /* Stopped to begin with (the LED is what RI_KT_STOPPLAY reads). */
    RI_ASSERT(!ri_sui_led(&tr, RI_STR_PLAY, 0), "starts stopped");

    /* The press edge acts. */
    RI_ASSERT(ri_panel_key(&pu, RI_RAW_SPACE, 0) == 1, "press 1 changes");
    RI_ASSERT(ri_sui_led(&tr, RI_STR_PLAY, 0), "press 1 plays");

    /* The repeats that follow a held key are not presses. Before the guard
     * each of these flipped the transport — this is the 21-hour strobe. */
    RI_ASSERT(ri_panel_key(&pu, RI_RAW_SPACE, 0) == 0, "repeat 1 ignored");
    RI_ASSERT(ri_sui_led(&tr, RI_STR_PLAY, 0), "repeat 1 still playing");
    RI_ASSERT(ri_panel_key(&pu, RI_RAW_SPACE, 0) == 0, "repeat 2 ignored");
    RI_ASSERT(ri_sui_led(&tr, RI_STR_PLAY, 0), "repeat 2 still playing");

    /* The release is not a toggle, and it re-arms the key. */
    RI_ASSERT(ri_panel_key(&pu, RI_RAW_SPACE | RI_RAW_UPFLAG, 0) == 0, "release no toggle");
    RI_ASSERT(ri_sui_led(&tr, RI_STR_PLAY, 0), "release leaves it playing");
    RI_ASSERT(ri_panel_key(&pu, RI_RAW_SPACE, 0) == 1, "press 2 changes");
    RI_ASSERT(!ri_sui_led(&tr, RI_STR_PLAY, 0), "press 2 stops");

    /* Re-armed by the space release, NOT by any release: another key's up
     * while space is still down must leave the repeats ignored. */
    RI_ASSERT(ri_panel_key(&pu, RI_RAW_SPACE | RI_RAW_UPFLAG, 0) == 0, "release 2");
    RI_ASSERT(ri_panel_key(&pu, RI_RAW_SPACE, 0) == 1, "press 3 plays");
    RI_ASSERT(ri_panel_key(&pu, RI_RAW_KPPLUS | RI_RAW_UPFLAG, 0) == 0, "other up");
    RI_ASSERT(ri_panel_key(&pu, RI_RAW_SPACE, 0) == 0, "repeat after other up ignored");
    RI_ASSERT(ri_sui_led(&tr, RI_STR_PLAY, 0), "still playing after other up");

    /* The counter-pin: the guard is the TOGGLE, not the transport class.
     * A held tempo key must still ramp on every repeat. */
    tempo0 = (int)ri_sui_value(&tr, RI_STR_TEMPO);
    RI_ASSERT(ri_panel_key(&pu, RI_RAW_KPPLUS, 0) == 1, "+ steps");
    RI_ASSERT(ri_panel_key(&pu, RI_RAW_KPPLUS, 0) == 1, "+ repeats while held");
    RI_ASSERT((int)ri_sui_value(&tr, RI_STR_TEMPO) == tempo0 + 2, "held + ramps twice");

    /* A fresh panel starts unlatched (no held key survives a re-init). */
    ri_panel_init(&pu);
    pu.tr = &tr;
    RI_ASSERT(ri_panel_key(&pu, RI_RAW_SPACE, 0) == 1, "fresh panel toggles");

    RI_RESULT("transport_toggle_edge");
    return 0;
}
