/* t221_keybed — the keybed had no release edge, so it could not play notes.
 *
 * Owner spec 2026-10-13: "Match the hardware: the keys play notes, through
 * the Leviasynth note path MIDI input already uses (the ear proof heard it).
 * They write the pitch into the selected step only while STEP RECORD is
 * lit (manual: Step Recording p. 105, step record mode p. 117). That keeps
 * today's step entry, just behind the button the hardware uses for it.
 * Velocity is fixed (an E0 default, 100)."
 *
 * A key needs BOTH edges. Note-on on press is easy; note-off on release is
 * what stops a voice hanging, and there was nowhere for the release to go:
 * `ri_cev_button` knew `kind` was SELECTUP but only used it to cancel a drag.
 *
 * I first claimed no release was observable at all, from `RSectionDiag`
 * (`LONG buttons` is a cumulative counter, `last_hit` is a LEVEL). That was
 * wrong — RSectionDiag is the DIAGNOSTIC view. The event path already
 * splits it: rsection.mcc.c builds `kind` from SELECTDOWN/SELECTUP/MENUDOWN
 * and ri_cev_button branches on it. Recorded because the wrong reading would
 * have justified changing shared widget plumbing that was already correct.
 *
 * So the fix is one field on RICevState — `key_id`, the keybed key currently
 * held — carrying the same pollable shape `drag_id` and `rep_idx` already
 * use, so the shell can see a press and a release by comparing against what
 * it last saw. No change to rsection.mcc.c and no new MUI flags.
 *
 * WHAT THIS PINS: press records the held key; release clears it; a control
 * that is not a key must not set it; and the step pitch is still written on
 * press, because the owner's spec keeps today's step entry.
 */
#include <stdio.h>
#include <stdint.h>
#include "tests/helpers/ri_assert.h"
#include "app/core/canvas_events.h"
#include "gui/sectui.h"
#include "gui/sectlevi.h"
#include "engine/dsp/levi.h"
#include "gui/panelgeo.h"

#define L(id) ((uint16_t)((uint16_t)(RI_SEC_LEVI << 8) | (uint16_t)(id)))
#define KEY0 (uint32_t)RI_SLEVI_KEY0

int main(void) {
    const struct RIGeoSection *g = ri_geo_section(RI_SEC_LEVI);
    struct RICevState st;
    struct RISectUI ui;
    uint32_t k;
    int w, h;

    RI_ASSERT(g != 0, "Levi section laid out");
    RI_ASSERT(ri_sui_init(&ui, RI_SEC_LEVI) == 0, "levi panel init");
    w = ri_geo_px(g->w, 0);
    h = ri_geo_px(g->h, 0);

    /* Fresh state holds no key. */
    ri_cev_init(&st);
    RI_ASSERT(st.key_id == RI_CEV_NODRAG, "init: no key held");

    /* Press then release, for every key on the bed. */
    for (k = 0u; k < (uint32_t)RI_SLEVI_KEYS; k++) {
        uint32_t idx = KEY0 + k;
        int x0, y0, x1, y1;
        RI_ASSERT(ri_geo_bbox(g, L(idx), 0, &x0, &y0, &x1, &y1) == 0,
            "key %u has a box", idx);
        ri_cev_init(&st);
        ri_cev_button(&st, &ui, 0, g, 0,
            (x0 + x1) / 2, (y0 + y1) / 2, w, h, 0, 0u);
        RI_ASSERT(st.key_id == L(idx), "press on key %u holds it (%04x)", idx, st.key_id);
        ri_cev_button(&st, &ui, 0, g, 0,
            (x0 + x1) / 2, (y0 + y1) / 2, w, h, 1, 0u);
        RI_ASSERT(st.key_id == RI_CEV_NODRAG,
            "release on key %u clears it (%04x)", idx, st.key_id);
    }

    /* A control that is not a key must not claim to be one held. The arp
     * GATE knob is a KNOB, so pressing it starts a drag, not a note. */
    {
        int x0, y0, x1, y1;
        ri_cev_init(&st);
        RI_ASSERT(ri_geo_bbox(g, L(185), 0, &x0, &y0, &x1, &y1) == 0, "arp gate box");
        ri_cev_button(&st, &ui, 0, g, 0,
            (x0 + x1) / 2, (y0 + y1) / 2, w, h, 0, 0u);
        RI_ASSERT(st.key_id == RI_CEV_NODRAG, "a knob press holds no key");
        RI_ASSERT(st.drag_id == L(185), "a knob press starts a drag");
        ri_cev_button(&st, &ui, 0, g, 0,
            (x0 + x1) / 2, (y0 + y1) / 2, w, h, 1, 0u);
        RI_ASSERT(st.drag_id == RI_CEV_NODRAG, "release ends the drag");
        RI_ASSERT(st.key_id == RI_CEV_NODRAG, "and still holds no key");
    }

    /* A MOMENTARY BUTTON must not latch. The knob case above cannot see this
     * — a knob press takes the drag branch and never reaches the latch — so
     * without a button here, dropping the key-range guard entirely would
     * survive and every momentary switch in the panel would strand its
     * indicator lit. */
    {
        int x0, y0, x1, y1;
        ri_cev_init(&st);
        RI_ASSERT(ri_geo_bbox(g, L(RI_SLEVI_PAGEUP), 0, &x0, &y0, &x1, &y1) == 0, "pageup box");
        ri_cev_button(&st, &ui, 0, g, 0, (x0 + x1) / 2, (y0 + y1) / 2, w, h, 0, 0u);
        RI_ASSERT(st.key_id == RI_CEV_NODRAG,
            "PAGE UP is momentary and holds no key (%04x)", st.key_id);
        ri_cev_button(&st, &ui, 0, g, 0, (x0 + x1) / 2, (y0 + y1) / 2, w, h, 1, 0u);
        RI_ASSERT(st.key_id == RI_CEV_NODRAG, "and still none after release");
    }

    /* STEP RECORD gates the step ENTRY, not the note (owner 2026-10-13;
     * manual Step Recording p. 105, step record mode p. 117). A key press
     * always latches for the note, and writes a step only while the button
     * is lit. Both halves are checked: dropping the gate restores today's
     * unconditional entry, and dropping the latch loses the note. */
    {
        int x0, y0, x1, y1, k2;
        ri_slevi_init(&ui.u.slevi);
        ui.section = RI_SEC_LEVI;
        RI_ASSERT(ri_geo_bbox(g, L(KEY0 + 4), 0, &x0, &y0, &x1, &y1) == 0, "key box");
        /* Button off: press latches the note but writes no step. */
        ri_cev_init(&st);
        ri_cev_button(&st, &ui, 0, g, 0, (x0 + x1) / 2, (y0 + y1) / 2, w, h, 0, 0u);
        RI_ASSERT(st.key_id == L(KEY0 + 4), "note latches even with STEP RECORD off");
        /* Button lit: same press, and the step takes the pitch. */
        ri_slevi_init(&ui.u.slevi);
        ui.u.slevi.val[RI_SLEVI_STEPREC] = 1;
        ri_cev_init(&st);
        ri_cev_button(&st, &ui, 0, g, 0, (x0 + x1) / 2, (y0 + y1) / 2, w, h, 0, 0u);
        RI_ASSERT(st.key_id == L(KEY0 + 4), "note latches with STEP RECORD lit");
        k2 = (int)(st.key_id - L(KEY0)) + RI_SLEVI_MIDDLE_C;
        RI_ASSERT(k2 == RI_SLEVI_MIDDLE_C + 4, "key maps to note %d", k2);
        /* The button is a real control: registered, levered at the engine. */
        {
            const struct RICtlDef *d = ri_ctlreg_find(L(RI_SLEVI_STEPREC));
            RI_ASSERT(d != 0, "STEP RECORD is registered");
            RI_ASSERT(d && d->kind == RI_CK_SWITCH, "STEP RECORD is a SWITCH");
            RI_ASSERT(d && ri_ctlreg_auto_id(d) == RI_CTL_LEVI_SEQREC,
                "STEP RECORD drives the engine arm (%04x)", d ? ri_ctlreg_auto_id(d) : 0u);
        }
        RI_ASSERT(ri_geo_bbox(g, L(RI_SLEVI_STEPREC), 0, &x0, &y0, &x1, &y1) == 0,
            "STEP RECORD is hit-testable");
    }

    /* The step pitch is still written on press -- the owner's spec keeps
     * today's step entry, just behind the button the hardware uses. */
    {
        int x0, y0, x1, y1;
        ri_slevi_init(&ui.u.slevi);
        RI_ASSERT(ri_geo_bbox(g, L(KEY0 + 4), 0, &x0, &y0, &x1, &y1) == 0, "key box");
        ri_cev_init(&st);
        ri_cev_button(&st, &ui, 0, g, 0,
            (x0 + x1) / 2, (y0 + y1) / 2, w, h, 0, 0u);
        RI_ASSERT(st.key_id == L(KEY0 + 4), "key held");
        RI_ASSERT(st.key_id - L(KEY0) + RI_SLEVI_MIDDLE_C == RI_SLEVI_MIDDLE_C + 4,
            "key %u maps to note %d", KEY0 + 4, (int)(st.key_id - L(KEY0)) + RI_SLEVI_MIDDLE_C);
    }

    RI_RESULT("keybed");
}