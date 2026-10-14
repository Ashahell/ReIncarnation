/* t218_fx_enable — the four app-level FX canvases must be switchable on.
 *
 * Owner ear proof 2026-10-13: "the FX modules don't work at all". Traced to
 * the On/Off switch of each FX section being registered
 *
 *   R(PCF|DELAY|DIST|COMP, 0, SWITCH, "", "On/Off", ..., cc=RI_MIDI_CC_NONE,
 *     automatable=1, bind=RI_BIND_NONE, engine_id=0, ...)
 *
 * so ri_ctlreg_auto_id() hits its `default: return 0u`, ri_panel_ctl_send()
 * returns 1, and push_walk() skips the control. The switch can never leave
 * the panel. The engine defines 16 RI_FXID_* and not one of them is an
 * enable or bypass, so there is currently nothing for it to send TO.
 *
 * This pins DELIVERABILITY, not audibility. The engine-side enable and the
 * render effect are separate work; this test is the gate that stops the
 * switch from staying decorative.
 *
 * The meter assertions at the end are a deliberate negative control: a
 * meter is display-only and MUST keep auto_id == 0. Without them the
 * cheapest way to make this pass would be to bind every FX row, which would
 * wire up controls that have no business being writable. t60 already pins
 * mixer mutes at 0 for the same reason ("mute: no key").
 */
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "tests/helpers/ri_assert.h"
#include "gui/ctlreg.h"
#include "engine/engine.h"
#include "engine/seq/sched.h"
#include "engine/dsp/rb303.h"

#define SR 48000.0f
#define N 19200u

static float LA[N], RA[N], LB[N], RB[N];

static void duet(struct RIEngine *e, uint32_t sections) {
    static struct RIEvent ev[4];
    ev[0].sample = 0;     ev[0].type = RI_EV_NOTE_ON;  ev[0].device = 0;
    ev[0].voice = 0;      ev[0].value = 69;            ev[0].flags = 0; ev[0].seq = 0;
    ev[1].sample = 4800;  ev[1].type = RI_EV_NOTE_OFF; ev[1].device = 0;
    ev[1].voice = 0;      ev[1].value = 0;             ev[1].flags = 0; ev[1].seq = 1;
    ev[2].sample = 9600;  ev[2].type = RI_EV_NOTE_ON;  ev[2].device = 1;
    ev[2].voice = 0;      ev[2].value = 81;            ev[2].flags = 0; ev[2].seq = 2;
    ev[3].sample = 14400; ev[3].type = RI_EV_NOTE_OFF; ev[3].device = 1;
    ev[3].voice = 0;      ev[3].value = 0;             ev[3].flags = 0; ev[3].seq = 3;
    ri_engine_init(e);
    ri_engine_defaults(e);
    ri_engine_load(e, ev, 4, N, sections);
}

static int differ(const float *a, const float *b, uint32_t n) {
    uint32_t i;
    for (i = 0; i < n; i++)
        if (a[i] != b[i])
            return 1;
    return 0;
}

/* The Levi is the only section that renders STEREO: section 4 goes through
 * engine_section_stereo, sections 0-3 through engine_section. The rack gate
 * therefore exists in TWO copies of the render guard, and a 303A-only
 * fixture exercises only one of them — which is how mutant m5 survived the
 * first mutation run. A Levi chord gives the stereo copy real signal. */
static void leviduo(struct RIEngine *e) {
    static struct RIEvent ev[2];
    ev[0].sample = 0;    ev[0].type = RI_EV_NOTE_ON;  ev[0].device = 4u;
    ev[0].voice = 0;     ev[0].value = 60;            ev[0].flags = 0; ev[0].seq = 0;
    ev[1].sample = 9600; ev[1].type = RI_EV_NOTE_OFF; ev[1].device = 4u;
    ev[1].voice = 0;     ev[1].value = 0;             ev[1].flags = 0; ev[1].seq = 1;
    ri_engine_init(e);
    ri_engine_defaults(e);
    ri_engine_load(e, ev, 2, N, RI_ENGINE_SLEVI);
}

static const struct RICtlDef *find_leg(uint8_t sec, const char *grp, const char *leg) {
    uint32_t i;
    for (i = 0; i < ri_ctlreg_count(); i++) {
        const struct RICtlDef *d = ri_ctlreg_at(i);
        if (d && d->section == sec && !strcmp(d->group, grp) && !strcmp(d->legend, leg))
            return d;
    }
    return 0;
}

static const char *sec_name(uint8_t s) { return ri_ctlreg_section_name(s); }

int main(void) {
    static const uint8_t FXSEC[4] = { RI_SEC_PCF, RI_SEC_DELAY, RI_SEC_DIST, RI_SEC_COMP };
    uint16_t ids[4];
    uint32_t i, j;

    for (i = 0; i < 4u; i++) {
        const struct RICtlDef *d = find_leg(FXSEC[i], "", "On/Off");
        RI_ASSERT(d != 0, "%s: On/Off switch exists", sec_name(FXSEC[i]));
        if (!d)
            continue;
        RI_ASSERT(d->kind == RI_CK_SWITCH, "%s: On/Off is a SWITCH", sec_name(FXSEC[i]));
        RI_ASSERT(d->max_v >= 1, "%s: On/Off has an on position", sec_name(FXSEC[i]));
        /* It is already declared automatable; it just had no key to
         * automate INTO. That is the whole defect. */
        RI_ASSERT(d->automatable, "%s: On/Off declared automatable", sec_name(FXSEC[i]));
        ids[i] = ri_ctlreg_auto_id(d);
        RI_ASSERT(ids[i] != 0u,
            "%s: On/Off must have a delivery key (auto_id==0 means the switch "
            "can never leave the panel)", sec_name(FXSEC[i]));
    }

    /* One module's enable must not drive another's: four switches sharing a
     * key would look wired while silently cross-wiring the rack. */
    for (i = 0; i < 4u; i++) {
        for (j = i + 1u; j < 4u; j++) {
            if (ids[i] == 0u || ids[j] == 0u)
                continue;
            RI_ASSERT(ids[i] != ids[j],
                "%s and %s enable keys must differ (%04x)",
                sec_name(FXSEC[i]), sec_name(FXSEC[j]), ids[i]);
        }
    }

    /* Negative control: meters stay display-only. Binding these would be a
     * false fix, not a fix. */
    for (i = 0; i < 4u; i++) {
        const struct RICtlDef *m = find_leg(FXSEC[i], "", "Meter");
        RI_ASSERT(m != 0, "%s: Meter exists", sec_name(FXSEC[i]));
        if (m) {
            RI_ASSERT(m->kind == RI_CK_METER, "%s: Meter is a METER", sec_name(FXSEC[i]));
            RI_ASSERT(ri_ctlreg_auto_id(m) == 0u,
                "%s: Meter must stay unwritable", sec_name(FXSEC[i]));
        }
    }

    /* Sanity: the four sections are distinct and are the FX range. */
    RI_ASSERT(RI_SEC_PCF < RI_SEC_DELAY && RI_SEC_DELAY < RI_SEC_DIST &&
              RI_SEC_DIST < RI_SEC_COMP, "FX sections are contiguous");
    RI_ASSERT(ri_ctlreg_auto_id(0) == 0u, "null def has no key");

    /* ---- The engine half. Everything above is DELIVERABILITY: it would all
     * still pass with the registry ids in place and ri_engine_fx_set simply
     * ignoring them, which is the previous bug wearing a different hat. So
     * the switch is also pinned at the point where it must do something. */
    {
        static const uint32_t EIDS[4] = {
            RI_FXID_PCF_ENABLE, RI_FXID_DELAY_ENABLE,
            RI_FXID_DIST_ENABLE, RI_FXID_COMP_ENABLE
        };
        static const uint32_t SLOT[4] = {
            RI_ENGINE_FX_PCF, RI_ENGINE_FX_DELAY,
            RI_ENGINE_FX_DIST, RI_ENGINE_FX_COMP
        };
        struct RIEngine e;
        for (i = 0; i < 4u; i++) {
            duet(&e, RI_ENGINE_S303A);
            /* Default LIVE: nothing that was audible before the ruling may
             * stop being audible merely because a song was loaded. */
            RI_ASSERT(e.fx_on[SLOT[i]] == 1u,
                "fx %u must default live", SLOT[i]);
            ri_engine_fx_set(&e, EIDS[i], 0u);
            RI_ASSERT(e.fx_on[SLOT[i]] == 0u, "fx %u off", SLOT[i]);
            ri_engine_fx_set(&e, EIDS[i], 1u);
            RI_ASSERT(e.fx_on[SLOT[i]] == 1u, "fx %u on", SLOT[i]);
        }
        /* Any nonzero value is live (the knob is a switch, not a gate). */
        duet(&e, RI_ENGINE_S303A);
        ri_engine_fx_set(&e, RI_FXID_DIST_ENABLE, 127u);
        RI_ASSERT(e.fx_on[RI_ENGINE_FX_DIST] == 1u, "nonzero = live");

        /* The real thing: OFF must render bit-identical to a neutral engine,
         * and switching back on must restore exactly what was there. Routing
         * ownership is untouched — the rack gate and "which strip owns the
         * insert" stay independent, which is the whole point of the design.
         * Every case gets its OWN duet(): the engine cursor does not rewind,
         * so rendering twice off one instance compares a finished render
         * against an exhausted one. That mistake cost one debugging round
         * here and would have looked like a broken gate. */
        duet(&e, RI_ENGINE_S303A);
        RI_ASSERT(ri_engine_render(&e, LA, RA, N, SR) == N, "neutral render");
        duet(&e, RI_ENGINE_S303A);
        ri_engine_assign_insert(&e, RI_ROUTE_DIST, 0);
        ri_engine_fx_set(&e, RI_FXID_DIST_DRIVE, 100);
        RI_ASSERT(ri_engine_render(&e, LB, RB, N, SR) == N, "rack-on render");
        RI_ASSERT(differ(LA, LB, N), "engaged+driven dist must be audible");
        /* Same setup, rack switched off: byte-identical to neutral. */
        duet(&e, RI_ENGINE_S303A);
        ri_engine_assign_insert(&e, RI_ROUTE_DIST, 0);
        ri_engine_fx_set(&e, RI_FXID_DIST_DRIVE, 100);
        ri_engine_fx_set(&e, RI_FXID_DIST_ENABLE, 0u);
        RI_ASSERT(ri_engine_render(&e, LB, RB, N, SR) == N, "rack-off render");
        RI_ASSERT(!differ(LA, LB, N),
            "dist OFF must render bit-identical to neutral");
        RI_ASSERT(ri_route_owner(&e.route, RI_ROUTE_DIST) == 0,
            "the rack switch must not touch route ownership");
        /* Off then on again before rendering: audible, so the switch is a live
         * control and not a one-way door. */
        duet(&e, RI_ENGINE_S303A);
        ri_engine_assign_insert(&e, RI_ROUTE_DIST, 0);
        ri_engine_fx_set(&e, RI_FXID_DIST_DRIVE, 100);
        ri_engine_fx_set(&e, RI_FXID_DIST_ENABLE, 0u);
        ri_engine_fx_set(&e, RI_FXID_DIST_ENABLE, 1u);
        RI_ASSERT(ri_engine_render(&e, LB, RB, N, SR) == N, "re-enabled render");
        RI_ASSERT(differ(LA, LB, N), "dist re-enabled must be audible again");

        /* Same gate, STEREO copy (Levi, section 4). Two separate guards
         * exist; this is the one a mono-only fixture cannot see. */
        leviduo(&e);
        RI_ASSERT(ri_engine_render(&e, LA, RA, N, SR) == N, "levi neutral render");
        leviduo(&e);
        ri_engine_assign_insert(&e, RI_ROUTE_DIST, 4);
        ri_engine_fx_set(&e, RI_FXID_DIST_DRIVE, 100);
        RI_ASSERT(ri_engine_render(&e, LB, RB, N, SR) == N, "levi rack-on render");
        RI_ASSERT(differ(LA, LB, N), "stereo dist must be audible when on");
        leviduo(&e);
        ri_engine_assign_insert(&e, RI_ROUTE_DIST, 4);
        ri_engine_fx_set(&e, RI_FXID_DIST_DRIVE, 100);
        ri_engine_fx_set(&e, RI_FXID_DIST_ENABLE, 0u);
        RI_ASSERT(ri_engine_render(&e, LB, RB, N, SR) == N, "levi rack-off render");
        RI_ASSERT(!differ(LA, LB, N),
            "stereo dist OFF must render bit-identical to neutral");
    }

    /* Displacing COMP must darken its gain-reduction meter (owner 2026-10-15:
     * the COMP bar misbehaves alongside the stuck PCF/Delay meters). The GR
     * peak-hold lives in the comp struct and no render block runs once the
     * unit is displaced, so without an explicit reset at the transition the
     * last reduction freezes on screen -- the same defect class as the input
     * meters, on the other meter of the same canvas. */
    {
        struct RIEngine ge;
        float gr;
        duet(&ge, RI_ENGINE_S303A);
        ri_engine_assign_insert(&ge, RI_ROUTE_COMP, 0);
        ri_engine_fx_set(&ge, RI_FXID_COMP_THRESH, 0u); /* -40 dB: most sensitive */
        ri_engine_fx_set(&ge, RI_FXID_COMP_ENABLE, 1u);
        RI_ASSERT(ri_engine_render(&ge, LA, RA, N, SR) == N, "comp render");
        gr = ri_engine_comp_gr(&ge);
        RI_ASSERT(gr < 0.0f, "engaged COMP must show reduction (%.4g)",
            (double)gr);
        ri_engine_fx_set(&ge, RI_FXID_PCF_ENABLE, 1u); /* displaces COMP */
        gr = ri_engine_comp_gr(&ge);
        RI_ASSERT(gr == 0.0f, "displaced COMP GR must read 0 dB (%.4g)",
            (double)gr);
    }

    RI_RESULT("fxenable");
}