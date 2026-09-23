/* gui/panels.c — RIPanelDesc tables (Task 12, gate G12).
 * Pure C: every row is a stable control ID + default; ranges are all
 * 0..127 knobs/faders except transport switches. Defaults are the
 * neutral playing position (cutoff open-ish, volumes up, bus faders
 * full, transport stopped).
 */
#include "gui/panels.h"
#include "gui/knob_logic.h"

#define CTL(id, nm, dv) { (id), (nm), (dv), 0, 127 }

static const struct RIPanelControl RI_303A_CTLS[] = {
    CTL(RI_CTL_303A_BASE + 0, "cutoff", 96),
    CTL(RI_CTL_303A_BASE + 1, "reso", 32),
    CTL(RI_CTL_303A_BASE + 2, "envmod", 64),
    CTL(RI_CTL_303A_BASE + 3, "decay", 64),
    CTL(RI_CTL_303A_BASE + 4, "accent", 64),
    CTL(RI_CTL_303A_BASE + 5, "volume", 100),
};

static const struct RIPanelControl RI_303B_CTLS[] = {
    CTL(RI_CTL_303B_BASE + 0, "cutoff", 96),
    CTL(RI_CTL_303B_BASE + 1, "reso", 32),
    CTL(RI_CTL_303B_BASE + 2, "envmod", 64),
    CTL(RI_CTL_303B_BASE + 3, "decay", 64),
    CTL(RI_CTL_303B_BASE + 4, "accent", 64),
    CTL(RI_CTL_303B_BASE + 5, "volume", 100),
};

static const struct RIPanelControl RI_808_CTLS[] = {
    CTL(RI_CTL_808_BASE + 0, "level", 100),
    CTL(RI_CTL_808_BASE + 1, "tune", 64),
    CTL(RI_CTL_808_BASE + 2, "decay", 64),
    CTL(RI_CTL_808_BASE + 3, "snappy", 64),
    CTL(RI_CTL_808_BASE + 4, "tone", 64),
    CTL(RI_CTL_808_BASE + 5, "accent", 64),
};

static const struct RIPanelControl RI_909_CTLS[] = {
    CTL(RI_CTL_909_BASE + 0, "tune", 64),
    CTL(RI_CTL_909_BASE + 1, "level", 100),
    CTL(RI_CTL_909_BASE + 2, "decay", 64),
    CTL(RI_CTL_909_BASE + 3, "flamres", 64),
};

static const struct RIPanelControl RI_MIX_CTLS[] = {
    CTL(RI_CTL_MIX_BASE + 0, "bus1", 127),
    CTL(RI_CTL_MIX_BASE + 1, "bus2", 127),
    CTL(RI_CTL_MIX_BASE + 2, "bus3", 127),
    CTL(RI_CTL_MIX_BASE + 3, "bus4", 127),
    CTL(RI_CTL_MIX_BASE + 4, "master", 127),
    CTL(RI_CTL_MIX_BASE + 5, "send1", 0),
};

static const struct RIPanelControl RI_TRANSPORT_CTLS[] = {
    CTL(RI_CTL_MIX_BASE + 8, "play", 0),
    CTL(RI_CTL_MIX_BASE + 9, "stop", 0),
    CTL(RI_CTL_MIX_BASE + 10, "tempo", 120),
    CTL(RI_CTL_MIX_BASE + 11, "pattern", 0),
    CTL(RI_CTL_MIX_BASE + 12, "shuffle", 50),
};

/* Dummy panel for the TC-2.9.5 generality proof (test-local consumer:
 * the tables never learn about it — same lookup, no framework edits). */
static const struct RIPanelControl RI_DUMMY_CTLS[] = {
    CTL(0x0F00u, "dummy-knob", 64),
    CTL(0x0F01u, "dummy-fader", 32),
};

static const struct RIPanelDesc RI_DUMMY = {
    "dummy", 9, RI_DUMMY_CTLS, 2
};

#define NCTL(a) (unsigned int)(sizeof(a) / sizeof((a)[0]))

static const struct RIPanelDesc RI_PANELS[] = {
    { "303A", 0, RI_303A_CTLS, NCTL(RI_303A_CTLS) },
    { "303B", 1, RI_303B_CTLS, NCTL(RI_303B_CTLS) },
    { "808", 2, RI_808_CTLS, NCTL(RI_808_CTLS) },
    { "909", 3, RI_909_CTLS, NCTL(RI_909_CTLS) },
    { "mixer", 4, RI_MIX_CTLS, NCTL(RI_MIX_CTLS) },
    { "transport", 5, RI_TRANSPORT_CTLS, NCTL(RI_TRANSPORT_CTLS) },
};

unsigned int ri_panel_count(void) {
    return (unsigned int)(sizeof(RI_PANELS) / sizeof(RI_PANELS[0]));
}

const struct RIPanelDesc *ri_panel_get(unsigned int i) {
    if (i >= ri_panel_count())
        return 0;
    return &RI_PANELS[i];
}

const struct RIPanelControl *ri_panel_find_ctl(const struct RIPanelDesc *p,
                                               unsigned int ctl_id) {
    unsigned int i;
    if (!p)
        return 0;
    for (i = 0; i < p->nctls; i++) {
        if (p->ctls[i].ctl_id == ctl_id)
            return &p->ctls[i];
    }
    return 0;
}

const struct RIPanelDesc *ri_panel_dummy(void) {
    return &RI_DUMMY;
}

int ri_panel_default_ctl(const struct RIPanelDesc *p, unsigned int ctl_id) {
    const struct RIPanelControl *c = ri_panel_find_ctl(p, ctl_id);
    if (!c)
        return -1;
    return (int)c->def_value;
}

/* Doc table: docs/evidence/gui/panel-909-geometry.md (centers at
 * 1024×768, 56 px knobs at 1x). */
static const int RI_909_KNOB_X[4] = { 176, 400, 624, 848 };
static const int RI_909_KNOB_Y = 320;
static const int RI_909_KNOB_D = 56;

struct RIPanelRect ri_panel909_knob_rect(unsigned int index, int zoom) {
    struct RIPanelRect r = { 0, 0, 0, 0 };
    if (index >= 4u)
        return r;
    if (ri_zoom_factor(zoom) == 0.0)
        return r;
    r.x = ri_zoom_scaled_px(RI_909_KNOB_X[index], zoom);
    r.y = ri_zoom_scaled_px(RI_909_KNOB_Y, zoom);
    r.w = ri_zoom_scaled_px(RI_909_KNOB_D, zoom);
    r.h = ri_zoom_scaled_px(RI_909_KNOB_D, zoom);
    return r;
}
