/* t83_panelctl — G9b Step 2: panel value change -> control plane.
 * One canvas change produces exactly one message with the right lane key
 * and value, for a sample of controls from every bound section; unbound
 * controls queue nothing; values clamp to 0..127. Meter snapshot: the GUI
 * copies through ri_live_meters_read (0 ok, 1 while publishing).
 * RED-first: stub bridge returns 1, stub read returns 2.
 */
#include <stdio.h>
#include <stdint.h>
#include "tests/helpers/ri_assert.h"
#include "gui/panelctl.h"
#include "gui/ctlreg.h"
#include "engine/seq/ctlplane.h"
#include "engine/seq/autolane.h"
#include "engine/seq/sched.h"
#include "engine/dsp/rb303.h"
#include "engine/dsp/rb808.h"
#include "engine/dsp/rb909.h"
#include "engine/fx/fx.h"
#include "engine/fx/route.h"
#include "engine/live.h"

#define SR 48000.0f

struct t83_case { uint8_t section; uint8_t idx; int value; uint16_t key; };

int main(void) {
    static const struct t83_case bound[] = {
        { RI_SEC_SYNTH1, 2u, 48, RI_CTL_303A_CUTOFF },
        { RI_SEC_SYNTH2, 2u, 90, RI_CTL_303B_CUTOFF },
        { RI_SEC_808, 1u, 100, RI_AUTO_ID_808(RI_CTL_808_LEVEL, RB808_BD) },
        { RI_SEC_909, 1u, 100, RI_AUTO_ID_909(RI_CTL_909_LEVEL, RB909_BD) },
        { RI_SEC_MIX_SYNTH1, 2u, 72, RI_AUTO_ID_MIX(0u, RI_AUTO_MIX_LEVEL) },
        { RI_SEC_MIX_SYNTH2, 2u, 72, RI_AUTO_ID_MIX(1u, RI_AUTO_MIX_LEVEL) },
        { RI_SEC_MIX_808, 2u, 72, RI_AUTO_ID_MIX(2u, RI_AUTO_MIX_LEVEL) },
        { RI_SEC_MIX_909, 3u, 40, RI_AUTO_ID_MIX(3u, RI_AUTO_MIX_PAN) },
        { RI_SEC_MASTER, 3u, 1, RI_AUTO_ID_MIX(RI_AUTO_STRIP_MASTER,
            RI_AUTO_MIX_DIST + RI_ROUTE_COMP) },
        { RI_SEC_PCF, 4u, 64, RI_FXID_PCF_BASE },
        { RI_SEC_DELAY, 5u, 48, RI_FXID_DELAY_FB },
        { RI_SEC_DIST, 2u, 64, RI_FXID_DIST_DRIVE },
        { RI_SEC_COMP, 2u, 64, RI_FXID_COMP_RATIO },
    };
    struct RIControlPlane q;
    struct RIEvent ev[32];
    uint32_t seq = 0u, n, i;
    uint32_t before;

    ri_ctl_init(&q);
    for (i = 0u; i < sizeof bound / sizeof bound[0]; i++) {
        uint16_t reg = (uint16_t)(((uint16_t)bound[i].section << 8) | bound[i].idx);
        before = ri_ctl_pending(&q);
        RI_ASSERT(ri_panel_ctl_send(&q, reg, bound[i].value) == 0, "send s%u.%u",
            bound[i].section, bound[i].idx);
        RI_ASSERT(ri_ctl_pending(&q) == before + 1u, "exactly one queued s%u.%u",
            bound[i].section, bound[i].idx);
    }
    n = ri_ctl_drain(&q, ev, 32u, 0u, &seq);
    RI_ASSERT(n == sizeof bound / sizeof bound[0], "drain all %u", n);
    for (i = 0u; i < n; i++) {
        RI_ASSERT(ev[i].value == bound[i].key, "key s%u.%u",
            bound[i].section, bound[i].idx);
        RI_ASSERT(ev[i].flags == (uint16_t)bound[i].value, "value s%u.%u",
            bound[i].section, bound[i].idx);
        RI_ASSERT(ev[i].type == RI_EV_AUTOMATION, "automation type");
    }

    /* Unbound controls queue nothing: pitch buttons, 909 Attack (no engine
     * param), transport Play (own route), unknown reg_id. */
    before = ri_ctl_pending(&q);
    RI_ASSERT(ri_panel_ctl_send(&q, (RI_SEC_SYNTH1 << 8) | 7u, 1) == 1, "pitch none");
    RI_ASSERT(ri_panel_ctl_send(&q, (RI_SEC_909 << 8) | 3u, 64) == 1, "attack none");
    RI_ASSERT(ri_panel_ctl_send(&q, (RI_SEC_TRANSPORT << 8) | 4u, 1) == 1, "play none");
    RI_ASSERT(ri_panel_ctl_send(&q, (RI_SEC_PAT_SYNTH1 << 8) | 2u, 3) == 1, "pat none");
    RI_ASSERT(ri_panel_ctl_send(&q, (18u << 8) | 0u, 64) == 1, "unknown none");
    RI_ASSERT(ri_ctl_pending(&q) == before, "unbound queues zero");
    RI_ASSERT(ri_panel_ctl_send(0, (RI_SEC_SYNTH1 << 8) | 2u, 64) == 2, "null plane");

    /* Values clamp to the 0..127 lane range. */
    ri_ctl_init(&q);
    RI_ASSERT(ri_panel_ctl_send(&q, (RI_SEC_SYNTH1 << 8) | 2u, 200) == 0, "clamp hi sent");
    RI_ASSERT(ri_panel_ctl_send(&q, (RI_SEC_SYNTH1 << 8) | 2u, -5) == 0, "clamp lo sent");
    seq = 0u;
    n = ri_ctl_drain(&q, ev, 8u, 0u, &seq);
    RI_ASSERT(n == 2u, "clamp drain 2");
    RI_ASSERT(ev[0].value == RI_CTL_303A_CUTOFF && ev[0].flags == 127u, "clamp hi 127");
    RI_ASSERT(ev[1].value == RI_CTL_303A_CUTOFF && ev[1].flags == 0u, "clamp lo 0");

    /* Meter snapshot: stopped session publishes zeros; the GUI copies
     * through read; an open publish reads busy. */
    {
        static struct RILiveSession s;
        static struct RIEvent scratch[64];
        struct RILiveMeters m;
        ri_live_init(&s, 96u, SR, 120.0f, RI_ENGINE_S303A | RI_ENGINE_S808,
            scratch, 64u);
        RI_ASSERT(ri_live_meters_read(&s, &m) == 0, "read init");
        RI_ASSERT(m.samples == 0u && m.xruns == 0u, "init zeros");
        RI_ASSERT(ri_live_meters_read(0, &m) == 2, "read null session");
        RI_ASSERT(ri_live_meters_read(&s, 0) == 2, "read null out");
        ri_live_meters_begin(&s);
        RI_ASSERT(ri_live_meters_read(&s, &m) == 1, "busy while publishing");
        ri_live_meters_end(&s);
        RI_ASSERT(ri_live_meters_read(&s, &m) == 0, "ok after publish");
    }

    RI_RESULT("panelctl");
}
