/* t225_strip_mute — the mixer strip green square must actually mute.
 *
 * Owner ear proof, 2026-10-14, mixer:
 *
 *   "all meters remain lit after you turn their panel off using the green
 *    square. ... all panels remain audible as well. Looks like the green
 *    square has no effect at all (other then changing color when pressed)."
 *
 * Cause, proven by the P-DEC1 sweep and by the registry:
 *
 *   R(MIX_LEVI, 0, SWITCH, "", "On/Off", 0, 1, 1, RI_MIDI_CC_NONE, 0, NONE, 0, 0)
 *
 * bind=NONE, automatable=0, and there was NO RI_AUTO_MIX_MUTE anywhere in the
 * engine -- the concept did not exist. So the press toggled b->val[strip][0]
 * (hence the colour change), ri_ctlreg_auto_id() returned 0, the push was
 * skipped, and nothing reached the engine.
 *
 * Owner ruling 2026-10-14: "add it."
 *
 * WHAT A MUTE MUST DO, and the second half is the part that is easy to get
 * wrong. A muted strip must stop feeding the MIXER OUTPUT *and* the delay
 * send. Scaling only the pan gains would leave the strip audible through
 * `sendbus`, which is summed across every section into one shared delay line
 * -- so a muted strip would keep colouring the delay. That is a real bug this
 * test exists to prevent, not a hypothetical.
 */
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "tests/helpers/ri_assert.h"
#include "gui/ctlreg.h"
#include "engine/engine.h"
#include "engine/seq/autolane.h"
#include "engine/seq/sched.h"

#define N 19200u  /* t52's length: 2048 is too short for the 303 to sound */
#define SR 48000.0f
#define DLINE 96000u
static float DLINE_BUF[DLINE];
static float LA[N], LB[N], RA[N], RB[N];

static int differ(const float *a, const float *b, uint32_t n) {
    uint32_t i;
    for (i = 0; i < n; i++)
        if (a[i] != b[i])
            return 1;
    return 0;
}

/* Render one section twice from a FRESH engine cursor each time: the cursor
 * does not rewind, so rendering twice off one instance compares a finished
 * render against an exhausted one. That mistake cost a debugging round in
 * t218 and would look exactly like a broken mute. */
/* The neutral baseline must load NO events. An earlier version called the
 * same fixture for both sides, so LA contained the same notes as LB and
 * differ() was trivially false -- which would have made every "muted is
 * silent" assertion below pass for the wrong reason. */
static void neutral(struct RIEngine *e) {
    static struct RIEvent none[1];
    ri_engine_init(e);
    ri_engine_defaults(e);
    /* Load ZERO events rather than skipping ri_engine_load: without it the
     * render short-reads, returns < N, and leaves the tail of the output
     * buffer holding the PREVIOUS render's samples. That stale tail is what
     * made "muted is bit-identical to neutral" fail for a reason that had
     * nothing to do with the mute. */
    ri_engine_load(e, none, 0, N, RI_ENGINE_S303A);
}

static void one(struct RIEngine *e, int with_delay) {
    static struct RIEvent ev[4];
    memset(ev, 0, sizeof ev);
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
    if (with_delay)
        ri_engine_set_delay(e, DLINE_BUF, DLINE);
    ri_engine_load(e, ev, 4, N, RI_ENGINE_S303A);
}

int main(void) {
    struct RIEngine e;
    uint32_t s;

    /* Neutral: nothing playing. */
    neutral(&e);
    RI_ASSERT(ri_engine_render(&e, LA, RA, N, SR) == N, "neutral render");

    /* Unmuted: the strip is audible. If this fails the fixture is wrong and
     * every "muted is silent" assertion below would pass for the wrong
     * reason. */
    one(&e, 0);
    RI_ASSERT(ri_engine_render(&e, LB, RB, N, SR) == N, "live render");
    RI_ASSERT(differ(LA, LB, N), "fixture: an unmuted strip must be audible");

    /* Muted: bit-identical to neutral. */
    one(&e, 0);
    RI_ASSERT(ri_engine_set_enable(&e, 0u, 0u) == 0,
        "enable accepted (section INDEX 0; RI_ENGINE_S303A is a 0x01 bitmask). 0 = OFF = silenced");
    RI_ASSERT(ri_engine_render(&e, LB, RB, N, SR) == N, "muted render");
    RI_ASSERT(!differ(LA, LB, N),
        "a muted strip must render bit-identical to neutral");

    /* Unmuting restores exactly what was there -- a mute is not a one-way
     * door, same rule the FX rack follows. */
    one(&e, 0);
    ri_engine_set_enable(&e, 0u, 0u);   /* OFF */
    ri_engine_set_enable(&e, 0u, 1u);   /* back ON */
    RI_ASSERT(ri_engine_render(&e, LB, RB, N, SR) == N, "re-enabled render");
    RI_ASSERT(differ(LA, LB, N), "unmuting must restore the strip");

    /* A muted strip must not feed the shared delay line. The send is summed
     * across sections, so scaling only the pan gains would leave the strip
     * audible through the delay while silent in the mix -- the exact "looks
     * muted but isn't" shape this whole session has been chasing. */
    one(&e, 1);
    ri_engine_set_send(&e, 0u, 100u);
    ri_engine_fx_set(&e, RI_FXID_DELAY_ENABLE, 1u);
    ri_engine_fx_set(&e, RI_FXID_DELAY_STEPS, 1u);
    ri_engine_fx_set(&e, RI_FXID_DELAY_FB, 90u);
    RI_ASSERT(ri_engine_render(&e, LB, RB, N, SR) == N, "send render");
    RI_ASSERT(differ(LA, LB, N), "fixture: an open send must be audible");
    one(&e, 1);
    ri_engine_set_send(&e, 0u, 100u);
    ri_engine_fx_set(&e, RI_FXID_DELAY_ENABLE, 1u);
    ri_engine_fx_set(&e, RI_FXID_DELAY_STEPS, 1u);
    ri_engine_fx_set(&e, RI_FXID_DELAY_FB, 90u);
    ri_engine_set_enable(&e, 0u, 0u);   /* OFF */
    RI_ASSERT(ri_engine_render(&e, LB, RB, N, SR) == N, "disabled send render");
    RI_ASSERT(!differ(LA, LB, N),
        "a muted strip must not keep colouring the shared delay");

    /* Every section the mixer exposes must accept a mute, and out-of-range
     * must fail rather than write past the array. */
    one(&e, 0);
    for (s = 0; s < RI_ROUTE_NSECTIONS; s++)
        RI_ASSERT(ri_engine_set_enable(&e, s, 1u) == 0, "mute section %u", s);
    RI_ASSERT(ri_engine_set_enable(&e, RI_ROUTE_NSECTIONS, 1u) == 2,
        "out-of-range section refused");
    RI_ASSERT(ri_engine_set_enable(0, 0u, 1u) == 2, "null engine refused");

    /* THE WIRING. A mute that the panel cannot reach is the defect we just
     * fixed; one it can reach but that resolves to no key is the same defect
     * wearing a new hat. Every strip's green square must now be deliverable. */
    {
        static const uint32_t secs[5] = {
            RI_SEC_MIX_SYNTH1, RI_SEC_MIX_SYNTH2, RI_SEC_MIX_808,
            RI_SEC_MIX_909, RI_SEC_MIX_LEVI
        };
        static const char *nm[5] = { "SYNTH1", "SYNTH2", "808", "909", "LEVI" };
        for (s = 0; s < 5u; s++) {
            uint16_t id = (uint16_t)((secs[s] << 8) | 0u);
            const struct RICtlDef *d = ri_ctlreg_find(id);
            RI_ASSERT(d != 0, "%s green square registered", nm[s]);
            RI_ASSERT(d && d->kind == RI_CK_SWITCH, "%s is a switch", nm[s]);
            RI_ASSERT(d && d->automatable == 1u, "%s mute is automatable", nm[s]);
            RI_ASSERT(d && ri_ctlreg_auto_id(d) != 0u,
                "%s mute must be deliverable (%04x)", nm[s],
                d ? ri_ctlreg_auto_id(d) : 0u);
            /* auto_strip_of maps route section 4 to RI_AUTO_STRIP_LEVI, so
             * the Levi's key is strip 5 even though its voice is 4. */
            RI_ASSERT(d && ri_ctlreg_auto_id(d) ==
                    RI_AUTO_ID_MIX(s == 4u ? RI_AUTO_STRIP_LEVI : s, RI_AUTO_MIX_MUTE),
                "%s mute key is MIX(strip=%u, MUTE)", nm[s],
                s == 4u ? RI_AUTO_STRIP_LEVI : s);
        }
    }

    /* MUTE must not collide with the low-byte indices already in use: a
     * duplicate would silently steal Level, Pan, Send or an insert. */
    RI_ASSERT(RI_AUTO_MIX_MUTE != RI_AUTO_MIX_LEVEL && RI_AUTO_MIX_MUTE != RI_AUTO_MIX_PAN
        && RI_AUTO_MIX_MUTE != RI_AUTO_MIX_SEND
        && RI_AUTO_MIX_MUTE != RI_AUTO_MIX_DIST
        && RI_AUTO_MIX_MUTE >= RI_AUTO_MIX_DIST + RI_ROUTE_NUNITS,
        "RI_AUTO_MIX_MUTE (%u) must sit clear of the insert range (%u..%u)",
        RI_AUTO_MIX_MUTE, RI_AUTO_MIX_DIST,
        RI_AUTO_MIX_DIST + RI_ROUTE_NUNITS - 1u);

    RI_RESULT("strip_mute");
    return 0;
}
