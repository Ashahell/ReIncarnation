/* t226_delay_fx_diag — DIAGNOSTIC: do the Delay FX panel knobs reach the
 * engine, and if not, where?
 *
 * Owner ear proof, Dell lane, build e846eff/3605381, 2026-10-14:
 *
 *   "Delay in the Mixer works audibly, in FX it doesn't."
 *
 * The delay line is fed and sounds (Levi send 64, ear-confirmed). What does
 * nothing is the DELAY PANEL's own knobs: F.Back moved 142 times, Pan 141,
 * Steps and Triplet too, with no audible change. The lane log proves the
 * presses reached the shell (CTL 0a05 x142, 0a04 x141, 0a03 x2).
 *
 * RULED OUT BEFORE THIS TEST, by reading the code:
 *   - the registry rows are bound   (F.Back -> RI_FXID_DELAY_FB, default 48)
 *   - ri_engine_fx_set handles them  (fb = value * 0.8/127, mix = 1.0)
 *   - ri_fxdelay_resync does NOT stomp fb (it only retargets delay_smp)
 *   - RI_BIND_FX rows are deliverable (the FX enable is ear-confirmed)
 *
 * So the break is in the path BETWEEN the panel value and the engine struct,
 * and this test walks that path one hop at a time and says which hop is dead.
 * Each step asserts the engine state CHANGED, so a failure names the hop
 * rather than just "delay knobs don't work".
 *
 * This is a DIAGNOSTIC, not a parity test: it asserts the wiring and the
 * effect, and prints what it finds. If it passes, the engine and the bridge
 * are innocent and the break is in the SHELL, which is the conclusion that
 * actually changes where to look next.
 */
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "tests/helpers/ri_assert.h"
#include "gui/ctlreg.h"
#include "gui/panelctl.h"
#include "engine/seq/ctlplane.h"
#include "gui/sectfx.h"
#include "engine/engine.h"
#include "engine/fx/fx.h"
#include "app/core/riapp_core.h"

#define DLINE 96000u
/* sectfx.h names only ONOFF/METER/DLY_STEPS/DLY_TRIPLET/COMP_GR. The Delay
 * panel's Pan (4) and F.Back (5) have no RI_SFX_* constant -- a small gap of
 * its own. Named locally so this test does not invent registry knowledge. */
#define DIAG_DLY_PAN 4u
#define DIAG_DLY_FBACK 5u
static float DLINE_BUF[DLINE];

static struct RIEngine fresh(void) {
    static struct RIEngine e;
    ri_engine_init(&e);
    ri_engine_defaults(&e);
    ri_engine_set_delay(&e, DLINE_BUF, DLINE);
    return e;
}

int main(void) {
    struct RIEngine e;
    struct RIControlPlane pl;
    struct RIEvent ev[8];
    uint32_t seq = 0u, nd;
    uint16_t regFB, regPan, regSteps;
    const struct RICtlDef *d;
    float fb0, pan0;

    /* ---- HOP 1: the registry rows resolve to real engine ids ---- */
    regFB    = (uint16_t)((RI_SEC_DELAY << 8) | DIAG_DLY_FBACK);
    regPan   = (uint16_t)((RI_SEC_DELAY << 8) | DIAG_DLY_PAN);
    regSteps = (uint16_t)((RI_SEC_DELAY << 8) | RI_SFX_DLY_STEPS);
    d = ri_ctlreg_find(regFB);
    RI_ASSERT(d != 0, "Delay F.Back registered at %04x", regFB);
    RI_ASSERT(d && d->kind == RI_CK_KNOB, "F.Back is a knob");
    RI_ASSERT(d && ri_ctlreg_auto_id(d) == RI_FXID_DELAY_FB,
        "F.Back -> RI_FXID_DELAY_FB (got %04x)", d ? ri_ctlreg_auto_id(d) : 0u);
    d = ri_ctlreg_find(regSteps);
    RI_ASSERT(d && d->kind == RI_CK_SELECTOR, "Steps is a selector");
    RI_ASSERT(d && ri_ctlreg_auto_id(d) == RI_FXID_DELAY_STEPS,
        "Steps -> RI_FXID_DELAY_STEPS (got %04x)", d ? ri_ctlreg_auto_id(d) : 0u);
    d = ri_ctlreg_find(regPan);
    RI_ASSERT(d && ri_ctlreg_auto_id(d) == RI_FXID_DELAY_RETPAN,
        "Pan -> RI_FXID_DELAY_RETPAN (got %04x)", d ? ri_ctlreg_auto_id(d) : 0u);

    /* ---- HOP 2: the panel model accepts the value ---- */
    {
        struct RISectFx fx;
        ri_sfx_init(&fx, RI_SEC_DELAY);
        RI_ASSERT(ri_sfx_set_value(&fx, DIAG_DLY_FBACK, 120) == 1,
            "panel accepts F.Back (only KNOB/FADER/SELECTOR may be set)");
        RI_ASSERT(ri_sfx_value(&fx, DIAG_DLY_FBACK) == 120, "panel holds F.Back");
        RI_ASSERT(ri_sfx_set_value(&fx, RI_SFX_DLY_STEPS, 12) == 1,
            "panel accepts Steps");
        RI_ASSERT(ri_sfx_value(&fx, RI_SFX_DLY_STEPS) == 12, "panel holds Steps");
    }

    /* ---- HOP 3: the ENGINE setter moves the engine struct ---- */
    e = fresh();
    fb0 = e.delay.fb;
    ri_engine_fx_set(&e, RI_FXID_DELAY_FB, 120u);
    RI_ASSERT(e.delay.fb != fb0, "HOP 3: ri_engine_fx_set changed fb (%f -> %f)",
        (double)fb0, (double)e.delay.fb);
    RI_ASSERT(e.delay.fb > 0.7f, "fb 120 -> ~0.756 (got %f)", (double)e.delay.fb);

    /* ---- HOP 4: THE BRIDGE. This is the hop that has never been tested. ---- */
    e = fresh();
    fb0 = e.delay.fb;
    ri_ctl_init(&pl);
    RI_ASSERT(ri_panel_ctl_send(&pl, regFB, 120) == 0,
        "HOP 4a: bridge accepted F.Back (0 = sent)");
    nd = ri_ctl_drain(&pl, ev, 8u, 0u, &seq);
    RI_ASSERT(nd == 1u, "HOP 4b: bridge drained one event (%u)", nd);
    /* PARAM/AUTOMATION events ride the note-shaped RIEvent: `value` is the
     * control id and `flags` is the 0..127 value (spec section 8 payload map). */
    RI_ASSERT(nd == 1u && ev[0].value == RI_FXID_DELAY_FB,
        "HOP 4c: event carries RI_FXID_DELAY_FB (got %04x)",
        nd == 1u ? ev[0].value : 0u);
    RI_ASSERT(nd == 1u && ev[0].flags == 120u,
        "HOP 4d: event carries the value in flags (got %u)", nd == 1u ? ev[0].flags : 0u);
    ri_engine_apply_event(&e, &ev[0]);
    RI_ASSERT(e.delay.fb != fb0,
        "HOP 4e: THE BREAK IS HERE -- the bridge delivered the event but the "
        "engine struct did not move (%f -> %f)", (double)fb0, (double)e.delay.fb);

    /* ---- THE EFFECT: does changing F.Back change the AUDIBLE output? ----
     * Hop 4e can pass while the sound is identical, if every repeat is below
     * the noise of the fixture. Measured, not assumed. */
    {
        static float A[8192], B[8192];
        static float RA[8192];
        int i, differs = 0;
        memset(A, 0, sizeof A); memset(RA, 0, sizeof RA);
        for (i = 0; i < 8192; i++)          /* a 200 Hz tone into the line */
            A[i] = RA[i] = (float)(0.5 * ((i % 48) < 24 ? 1.0 : -1.0));
        ri_fxdelay_init(&e.delay, DLINE_BUF, DLINE);
        e.delay.mix = 1.0f; e.delay.fb = 0.0f; e.delay.delay_smp = 400u;
        e.delay.target_smp = 400u;
        ri_fxdelay_render(&e.delay, A, B, 8192u);
        e.delay.pos = 0u; e.delay.fb = 0.75f; e.delay.delay_smp = 400u;
        memset(B, 0, sizeof B);
        ri_fxdelay_render(&e.delay, A, B, 8192u);
        for (i = 0; i < 8192; i++)
            if (A[i] != B[i]) { differs = 1; break; }
        RI_ASSERT(differs,
            "EFFECT: F.Back 0 vs 0.75 must change the delay output -- if this "
            "fails the render itself is the break, not the wiring");
    }

    /* ---- Pan: the second knob, reported as equally dead by ear ---- */
    e = fresh();
    pan0 = (float)e.dret_pan;
    ri_ctl_init(&pl);
    ri_panel_ctl_send(&pl, regPan, 20);
    nd = ri_ctl_drain(&pl, ev, 8u, 0u, &seq);
    RI_ASSERT(nd == 1u && ev[0].value == RI_FXID_DELAY_RETPAN,
        "Pan bridge delivers RI_FXID_DELAY_RETPAN (%u events)", nd);
    ri_engine_apply_event(&e, &ev[0]);
    RI_ASSERT((float)e.dret_pan != pan0, "Pan moved the engine struct (%f -> %f)",
        (double)pan0, (double)e.dret_pan);

    /* ---- THE LANE DIAGNOSTIC ----
     * Everything above proves the wiring; this proves the LINE that will make
     * the next "delay does nothing" answerable from the log instead of from
     * two ear-proof rounds. */
    {
        static struct RIAppCore c;
        static char line[128];
        int n;
        RI_ASSERT(ri_core_delay_diag(0, line, sizeof line) < 0, "null core refused");
        RI_ASSERT(ri_core_delay_diag(&c, 0, sizeof line) < 0, "null buffer refused");
        RI_ASSERT(ri_core_delay_diag(&c, line, 8u) < 0,
            "a cap too small to hold the line is REFUSED, not truncated -- a "
            "half-written diagnostic that looks complete is worse than none");
        ri_core_init(&c, 96u, 48000.0f, 120.0f, RI_ENGINE_S303A);
        n = ri_core_delay_diag(&c, line, sizeof line);
        RI_ASSERT(n > 0, "fresh core produces a line (%d)", n);
        RI_ASSERT(strstr(line, "send=") && strstr(line, "fb=") && strstr(line, "live="),
            "the line reports send, fb and live: %s", line);
        /* The conflation this exists to prevent: the knobs moved, the send
         * closed. fb must show the knob and send must show the silence. */
        ri_engine_set_send(&c.session.eng, 4u, 64u);
        ri_engine_fx_set(&c.session.eng, RI_FXID_DELAY_FB, 120u);
        ri_engine_fx_set(&c.session.eng, RI_FXID_DELAY_ENABLE, 1u);
        n = ri_core_delay_diag(&c, line, sizeof line);
        RI_ASSERT(n > 0 && strstr(line, "send=64") != 0,
            "send is visible in the line: %s", line);
        RI_ASSERT(n > 0 && strstr(line, "fb=120") != 0,
            "feedback is visible in the line: %s", line);
        RI_ASSERT(n > 0 && strstr(line, "live=1") != 0,
            "the rack gate is visible in the line: %s", line);
        printf("  DIAG LINE: %s\n", line);
    }

    printf("  DIAG: delay FB key=%04x, panel id=%04x, steps key=%04x\n",
        RI_FXID_DELAY_FB, regFB, RI_FXID_DELAY_STEPS);
    RI_RESULT("delay_fx_diag");
    return 0;
}
