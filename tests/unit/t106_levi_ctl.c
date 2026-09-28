/* t106_levi_ctl — Levi control table (owner 2026-09-28, v1 slice 3c-i).
 * 0x0E block voice controls (cutoff/reso/mode/ratio) + PAT block resolve
 * with kinds/ranges/defaults/binds; auto ids nonzero, allowed, unique;
 * engine applies section-wide to all voices; fail-closed throughout.
 */
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "tests/helpers/ri_assert.h"
#include "gui/ctlreg.h"
#include "gui/panels.h"
#include "engine/engine.h"
#include "engine/dsp/levi.h"
#include "engine/seq/autolane.h"

static const struct RICtlDef *find_leg(uint32_t sec, const char *legend) {
    uint32_t i, n = ri_ctlreg_count();
    for (i = 0u; i < n; i++) {
        const struct RICtlDef *d = ri_ctlreg_at(i);
        if (d && d->section == sec && !strcmp(d->legend, legend))
            return d;
    }
    return 0;
}

int main(void) {
    const struct RICtlDef *d;
    struct RIEngine e;
    struct RIEvent ev;
    /* Sections + names exist. */
    RI_ASSERT(RI_SEC_LEVI == 18u, "voice section");
    RI_ASSERT(RI_SEC_PAT_LEVI == 19u, "pat section");
    RI_ASSERT(RI_SEC_MIX_LEVI == 20u, "mix section");
    RI_ASSERT(RI_SEC_COUNT == 21u, "count");
    RI_ASSERT(!strcmp(ri_ctlreg_section_name(RI_SEC_LEVI), "Levi"), "name");
    RI_ASSERT(!strcmp(ri_ctlreg_section_token(RI_SEC_LEVI), "levi"), "token");
    RI_ASSERT(!strcmp(ri_ctlreg_section_token(RI_SEC_PAT_LEVI), "pat-levi"), "pat token");
    /* Voice rows. */
    d = find_leg(RI_SEC_LEVI, "Cutoff");
    RI_ASSERT(d && d->kind == RI_CK_KNOB, "cutoff kind");
    RI_ASSERT(d->min_v == 0 && d->max_v == 127 && d->def_v == 96, "cutoff range");
    RI_ASSERT(d->bind == RI_BIND_LEVI && d->engine_id == RI_CTL_LEVI_CUTOFF, "cutoff bind");
    d = find_leg(RI_SEC_LEVI, "Reso");
    RI_ASSERT(d && d->kind == RI_CK_KNOB, "reso kind");
    RI_ASSERT(d->min_v == 0 && d->max_v == 127 && d->def_v == 32, "reso range");
    RI_ASSERT(d->bind == RI_BIND_LEVI && d->engine_id == RI_CTL_LEVI_RESO, "reso bind");
    d = find_leg(RI_SEC_LEVI, "Mode");
    RI_ASSERT(d && d->kind == RI_CK_SWITCH, "mode kind");
    RI_ASSERT(d->min_v == 0 && d->max_v == 1 && d->def_v == 0, "mode range");
    RI_ASSERT(d->bind == RI_BIND_LEVI && d->engine_id == RI_CTL_LEVI_MODE, "mode bind");
    d = find_leg(RI_SEC_LEVI, "Ratio");
    RI_ASSERT(d && d->kind == RI_CK_KNOB, "ratio kind");
    RI_ASSERT(d->min_v == 0 && d->max_v == 127 && d->def_v == 32, "ratio range");
    RI_ASSERT(d->bind == RI_BIND_LEVI && d->engine_id == RI_CTL_LEVI_RATIO, "ratio bind");
    /* PAT block mirrors the 303 shape. */
    d = find_leg(RI_SEC_PAT_LEVI, "Bank");
    RI_ASSERT(d && d->kind == RI_CK_SELECTOR, "pat bank");
    d = find_leg(RI_SEC_PAT_LEVI, "Pattern");
    RI_ASSERT(d && d->kind == RI_CK_SELECTOR, "pat pattern");
    /* Auto ids live, allowed, unique vs the 303 bridge law. */
    d = find_leg(RI_SEC_LEVI, "Cutoff");
    RI_ASSERT(ri_ctlreg_auto_id(d) == RI_CTL_LEVI_CUTOFF, "key = engine id");
    RI_ASSERT(ri_auto_allowed(RI_CTL_LEVI_CUTOFF), "allowed");
    RI_ASSERT(ri_auto_allowed(RI_CTL_LEVI_MODE), "allowed");
    RI_ASSERT(!ri_auto_allowed(0x0E04u), "unbound refused");
    /* Engine applies section-wide to every voice. */
    ri_engine_init(&e);
    memset(&ev, 0, sizeof ev);
    ev.type = RI_EV_AUTOMATION;
    ev.value = RI_CTL_LEVI_CUTOFF;
    ev.flags = 0u;
    ri_engine_apply_event(&e, &ev);
    {
        uint32_t v;
        for (v = 0u; v < RI_LEVI_NVOICES; v++)
            RI_ASSERT(e.slevi.v[v].cutoff < RI_LEVI_DEF_CUTOFF, "v%u cutoff moved", v);
    }
    ev.value = 0x0E04u;
    ri_engine_apply_event(&e, &ev);
    ev.value = RI_CTL_LEVI_MODE;
    ev.flags = 127u;
    ri_engine_apply_event(&e, &ev);
    RI_ASSERT(e.slevi.v[3].op[1].mode == RI_LEVI_PM, "mode set");
    ri_engine_apply_event(0, &ev);
    RI_RESULT("levictl");
}
