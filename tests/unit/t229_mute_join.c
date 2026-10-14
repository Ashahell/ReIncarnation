/* t229_mute_join — the JOIN between the panel switch and the engine flag.
 *
 * Owner ear proof, Dell lane, build 514f4ee, 2026-10-14:
 *
 *   "when we turn off all 5 green squares in the Mixer, we still hear all
 *    sounds. when we turn them all on again, it's all quiet. that's not
 *    correct. it should be quiet when all green squares are off."
 *
 * INVERTED. Exactly inverted, and both halves were individually correct:
 *
 *   panel   R(MIX_*, 0, SWITCH, "", "On/Off", 0, 1, 1, ...)   def_v = 1
 *           b->val[strip][0] toggles, and the lamp is `val != 0`
 *               -> 1 means ENABLED, and the lamp is lit when audible
 *   engine  ri_engine_set_mute(e, s, v):  mute[s] = v ? 1 : 0
 *               -> 1 means MUTED
 *
 * The shell pushes the panel value VERBATIM, so a lit square pushed 1, and
 * the engine read that as mute. Press everything off -> every strip pushes 0
 * -> nothing is muted -> all five still audible. Press everything on -> every
 * strip pushes 1 -> everything muted -> silence. Precisely what was reported.
 *
 * WHY t225 AND t116 BOTH PASSED, which is the part worth keeping:
 *
 *  - `t225` tested the ENGINE in isolation: `set_mute(s, 1)` silences. True,
 *    and worth nothing, because nobody checked which value the panel sends.
 *  - `t116` round-trips panel -> bridge -> apply against a `direct()` that is a
 *    REFERENCE MIRROR of the engine's dispatch. Both sides were inverted
 *    identically, so they agreed perfectly and the gate confirmed nothing.
 *    Its own comment says it proves agreement, not correctness. That caveat
 *    cost nothing until now, when it hid an inversion for one whole session.
 *
 * So the invariant that was missing is the JOIN: the value the panel means by
 * "on" must be the value the engine reads as "audible". This test drives the
 * panel value through the real control plane -- the same path `sync_values`
 * uses -- and asserts on what comes OUT of the renderer.
 */
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "tests/helpers/ri_assert.h"
#include "gui/ctlreg.h"
#include "gui/panelctl.h"
#include "gui/sectmix.h"
#include "engine/engine.h"
#include "engine/seq/autolane.h"
#include "engine/seq/ctlplane.h"

#define N 19200u
#define SR 48000.0f
static float LA[N], LB[N], RA[N], RB[N];

static int differ(const float *a, const float *b, uint32_t n) {
    uint32_t i;
    for (i = 0; i < n; i++)
        if (a[i] != b[i]) return 1;
    return 0;
}

/* Push a strip's green square through the real bridge, exactly as sync_values
 * does: registry id -> ri_panel_ctl_send -> drain -> apply. */
static void push_square(struct RIEngine *e, struct RIControlPlane *pl,
    uint32_t section, int on) {
    struct RIEvent ev[8];
    uint32_t seq = 0u, nd;
    uint16_t id = (uint16_t)((section << 8) | 0u);
    ri_ctl_init(pl);
    ri_panel_ctl_send(pl, id, on);
    nd = ri_ctl_drain(pl, ev, 8u, 0u, &seq);
    RI_ASSERT(nd == 1u, "square %04x=%d delivered (%u)", id, on, nd);
    for (seq = 0; seq < nd; seq++)
        ri_engine_apply_event(e, &ev[seq]);
}

/* The neutral baseline must load ZERO events. Loading the same notes as the
 * fixture makes differ() trivially false, which silently turns every
 * "silent" assertion into a tautology -- and that is exactly what happened the
 * first time this ran: the all-off case "passed" against itself. */
static void neutral(struct RIEngine *e) {
    static struct RIEvent none[1];
    ri_engine_init(e);
    ri_engine_defaults(e);
    ri_engine_load(e, none, 0, N, RI_ENGINE_S303A);
}

static void load(struct RIEngine *e) {
    static struct RIEvent ev[4];
    ev[0].sample = 0;     ev[0].type = RI_EV_NOTE_ON;  ev[0].device = 0;
    ev[0].voice = 0;      ev[0].value = 69;            ev[0].flags = 0; ev[0].seq = 0;
    ev[1].sample = 4800;  ev[1].type = RI_EV_NOTE_OFF; ev[1].device = 0;
    ev[1].voice = 0;      ev[1].value = 0;             ev[1].flags = 0; ev[1].seq = 1;
    ev[2].sample = 9600;  ev[2].type = RI_EV_NOTE_ON;  ev[2].device = 0;
    ev[2].voice = 0;      ev[2].value = 81;            ev[2].flags = 0; ev[2].seq = 2;
    ev[3].sample = 14400; ev[3].type = RI_EV_NOTE_OFF; ev[3].device = 0;
    ev[3].voice = 0;      ev[3].value = 0;             ev[3].flags = 0; ev[3].seq = 3;
    ri_engine_init(e);
    ri_engine_defaults(e);
    ri_engine_load(e, ev, 4, N, RI_ENGINE_S303A);
}

int main(void) {
    static const uint32_t secs[5] = {
        RI_SEC_MIX_SYNTH1, RI_SEC_MIX_SYNTH2, RI_SEC_MIX_808,
        RI_SEC_MIX_909, RI_SEC_MIX_LEVI
    };
    struct RIEngine e;
    struct RIControlPlane pl;
    uint32_t k;

    /* Neutral, and the fixture's own baseline: an all-ON engine must be
     * AUDIBLE. If this fails the fixture is wrong and every "off is silent"
     * assertion below would pass for the wrong reason. */
    neutral(&e);
    RI_ASSERT(ri_engine_render(&e, LA, RA, N, SR) == N, "neutral render");
    load(&e);
    RI_ASSERT(ri_engine_render(&e, LB, RB, N, SR) == N, "live render");
    RI_ASSERT(differ(LA, LB, N), "fixture: an unmuted strip must be audible");

    /* THE REPORTED CASE, both halves.
     *
     * "turn off all 5 green squares ... we still hear all sounds" -- so with
     * every square pushed to 0 the render must be bit-identical to neutral.
     *
     * "when we turn them all on again, it's all quiet" -- with every square
     * pushed to 1 it must be AUDIBLE again.
     *
     * The panel's own default is 1 (`def_v = 1`), and its lamp reads
     * `val != 0`, so 1 is unambiguously "on / enabled" on the panel side.
     * These two assertions are the inversion, stated as the owner stated it. */
    load(&e);
    for (k = 0; k < 5u; k++)
        push_square(&e, &pl, secs[k], 0);
    RI_ASSERT(ri_engine_render(&e, LB, RB, N, SR) == N, "all-off render");
    RI_ASSERT(!differ(LA, LB, N),
        "square=0 must be SILENT (this is the owner's report: all five off, "
        "still audible)");

    load(&e);
    for (k = 0; k < 5u; k++)
        push_square(&e, &pl, secs[k], 1);
    RI_ASSERT(ri_engine_render(&e, LB, RB, N, SR) == N, "all-on render");
    RI_ASSERT(differ(LA, LB, N),
        "square=1 must be AUDIBLE (this is the owner's report: all five on, "
        "silence)");

    /* Per-strip selectivity, both directions. The fixture sounds ONLY
     * section 0 (303A / Synth1), so this is the discriminating pair: a fix
     * that silently inverts or zeroes the whole array passes the all-off and
     * all-on cases above and fails here.
     *
     * (The first version of this block turned SYNTH1 off and expected sound,
     * which is wrong -- SYNTH1 is the only thing playing, so turning it off
     * must be silent. That was a bug in the test, not the engine.) */
    load(&e);
    for (k = 0; k < 5u; k++)
        push_square(&e, &pl, secs[k], 1);
    push_square(&e, &pl, RI_SEC_MIX_SYNTH2, 0);   /* a strip that is NOT playing */
    RI_ASSERT(ri_engine_render(&e, LB, RB, N, SR) == N, "selective render");
    RI_ASSERT(differ(LA, LB, N),
        "switching OFF a strip that is not sounding must not silence the mix");

    load(&e);
    for (k = 0; k < 5u; k++)
        push_square(&e, &pl, secs[k], 1);
    push_square(&e, &pl, RI_SEC_MIX_SYNTH1, 0);   /* the strip that IS playing */
    RI_ASSERT(ri_engine_render(&e, LB, RB, N, SR) == N, "playing-strip-off render");
    RI_ASSERT(!differ(LA, LB, N),
        "switching OFF the sounding strip must silence it -- and only it");

    /* THE LAMP AND THE SOUND MUST AGREE. The panel lamp is `val != 0` on a
     * switch whose default is 1; the engine must read that same 1 as audible.
     * If these can disagree, the panel is lying again -- the shape this whole
     * session has been removing. */
    {
        struct RIMixBoard b;
        ri_smix_init(&b);
        RI_ASSERT(ri_smix_led(&b, RI_SEC_MIX_LEVI, 0u) == 1,
            "the square starts LIT at rest (def_v = 1)");
        ri_smix_press(&b, RI_SEC_MIX_LEVI, 0u);
        RI_ASSERT(ri_smix_led(&b, RI_SEC_MIX_LEVI, 0u) == 0,
            "pressing it goes dark");
        RI_ASSERT(ri_smix_value(&b, RI_SEC_MIX_LEVI, 0u) == 0,
            "and the value the bridge pushes is 0 -- so 0 must mean SILENT");
    }

    RI_RESULT("mute_join");
    return 0;
}
