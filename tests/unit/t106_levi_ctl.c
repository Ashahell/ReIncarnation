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
    RI_ASSERT(ri_auto_allowed(RI_CTL_LEVI_ARPON), "arpon allowed");
    RI_ASSERT(ri_auto_allowed(RI_CTL_LEVI_ARPRATE), "arprate allowed");
    d = ri_ctlreg_find((uint16_t)((RI_SEC_LEVI << 8) | 52u));
    RI_ASSERT(d && d->bind == RI_BIND_LEVI && d->engine_id == RI_CTL_LEVI_ARPON, "arpon bind");
    d = ri_ctlreg_find((uint16_t)((RI_SEC_LEVI << 8) | 53u));
    RI_ASSERT(d && d->bind == RI_BIND_LEVI && d->engine_id == RI_CTL_LEVI_ARPRATE, "arprate bind");
    d = ri_ctlreg_find((uint16_t)((RI_SEC_LEVI << 8) | 54u));
    RI_ASSERT(d && d->bind == RI_BIND_LEVI && d->engine_id == RI_CTL_LEVI_SEQON, "seqon bind");
    d = ri_ctlreg_find((uint16_t)((RI_SEC_LEVI << 8) | 55u));
    RI_ASSERT(d && d->bind == RI_BIND_LEVI && d->engine_id == RI_CTL_LEVI_SEQLEN, "seqlen bind");
    d = ri_ctlreg_find((uint16_t)((RI_SEC_LEVI << 8) | 56u));
    RI_ASSERT(d && d->bind == RI_BIND_LEVI && d->engine_id == RI_CTL_LEVI_ROUTE0, "route0 bind");
    d = ri_ctlreg_find((uint16_t)((RI_SEC_LEVI << 8) | 63u));
    RI_ASSERT(d && d->bind == RI_BIND_LEVI && d->engine_id == RI_CTL_LEVI_ROUTE7, "route7 bind");
    RI_ASSERT(ri_auto_allowed(RI_CTL_LEVI_ROUTE0), "route0 allowed");
    RI_ASSERT(ri_auto_allowed(RI_CTL_LEVI_ROUTE7), "route7 allowed");
    {
        struct RILeviSet set;
        levi_init_set(&set);
        RI_ASSERT(set.arpon == 0u && set.arprate == 64u, "arp defaults");
        RI_ASSERT(levi_set_param_ui(&set, 0u, RI_CTL_LEVI_ARPON & 0xFFu, 1u) == 0 &&
            set.arpon == 1u, "arpon ui");
        RI_ASSERT(levi_set_param_ui(&set, 0u, RI_CTL_LEVI_ARPRATE & 0xFFu, 100u) == 0 &&
            set.arprate == 100u, "arprate ui");
        RI_ASSERT(levi_set_param_ui(0, 0u, RI_CTL_LEVI_ARPON & 0xFFu, 1u) == 2, "arp null");
        RI_ASSERT(set.seqon == 0u && set.seqlen == 16u, "seq defaults");
        RI_ASSERT(ri_auto_allowed(RI_CTL_LEVI_SEQON), "seqon allowed");
        RI_ASSERT(ri_auto_allowed(RI_CTL_LEVI_SEQLEN), "seqlen allowed");
        RI_ASSERT(levi_set_param_ui(&set, 0u, RI_CTL_LEVI_SEQON & 0xFFu, 1u) == 0 &&
            set.seqon == 1u, "seqon ui");
        RI_ASSERT(levi_set_param_ui(&set, 0u, RI_CTL_LEVI_SEQLEN & 0xFFu, 4u) == 0 &&
            set.seqlen == 4u, "seqlen ui");
        RI_ASSERT(levi_set_param_ui(&set, 0u, RI_CTL_LEVI_SEQLEN & 0xFFu, 0u) == 0 &&
            set.seqlen == 1u, "seqlen clamp lo");
        RI_ASSERT(levi_set_param_ui(&set, 0u, RI_CTL_LEVI_SEQLEN & 0xFFu, 99u) == 0 &&
            set.seqlen == 16u, "seqlen clamp hi");
        RI_ASSERT(levi_set_param_ui(&set, 0u, RI_CTL_LEVI_ROUTE0 & 0xFFu, 1u) == 0 &&
            set.mx.slot[0].on == 1u, "route0 ui");
        RI_ASSERT(levi_set_param_ui(&set, 0u, RI_CTL_LEVI_ROUTE7 & 0xFFu, 1u) == 0 &&
            set.mx.slot[7].on == 1u, "route7 ui");
        RI_ASSERT(levi_set_param_ui(&set, 0u, RI_CTL_LEVI_ROUTE7 & 0xFFu, 0u) == 0 &&
            set.mx.slot[7].on == 0u && set.mx.slot[0].on == 1u, "route7 off keeps 0");
    }
    /* Algo block rows (owner 2026-09-28, v2 slice 1d). */
    d = find_leg(RI_SEC_LEVI, "Algorithm");
    RI_ASSERT(d && d->kind == RI_CK_SELECTOR, "algo kind");
    RI_ASSERT(d->min_v == 0 && d->max_v == 7 && d->def_v == 0, "algo range");
    RI_ASSERT(d->bind == RI_BIND_LEVI && d->engine_id == RI_CTL_LEVI_ALGO, "algo bind");
    d = find_leg(RI_SEC_LEVI, "Morph");
    RI_ASSERT(d && d->kind == RI_CK_KNOB, "morph kind");
    RI_ASSERT(d->min_v == 0 && d->max_v == 100 && d->def_v == 0, "morph range");
    RI_ASSERT(d->bind == RI_BIND_LEVI && d->engine_id == RI_CTL_LEVI_MORPH, "morph bind");
    d = find_leg(RI_SEC_LEVI, "Op");
    RI_ASSERT(d && d->kind == RI_CK_SELECTOR && d->bind == RI_BIND_NONE, "opsel ui-only");
    d = find_leg(RI_SEC_LEVI, "Op Mode");
    RI_ASSERT(d && d->kind == RI_CK_SELECTOR, "opmode kind");
    RI_ASSERT(d->min_v == 0 && d->max_v == 6 && d->def_v == 0, "opmode range");
    RI_ASSERT(d->bind == RI_BIND_LEVI && d->engine_id == RI_CTL_LEVI_OPMODE, "opmode bind");
    RI_ASSERT(ri_ctlreg_auto_id(find_leg(RI_SEC_LEVI, "Algorithm")) == RI_CTL_LEVI_ALGO, "algo key");
    RI_ASSERT(ri_ctlreg_auto_id(find_leg(RI_SEC_LEVI, "Morph")) == RI_CTL_LEVI_MORPH, "morph key");
    RI_ASSERT(ri_ctlreg_auto_id(find_leg(RI_SEC_LEVI, "Op Mode")) == RI_CTL_LEVI_OPMODE, "opmode key");
    RI_ASSERT(ri_auto_allowed(RI_CTL_LEVI_ALGO) && ri_auto_allowed(RI_CTL_LEVI_MORPH) &&
        ri_auto_allowed(RI_CTL_LEVI_OPMODE), "algo keys allowed");
    d = find_leg(RI_SEC_LEVI, "Type");
    RI_ASSERT(d && d->kind == RI_CK_SELECTOR, "ftype kind");
    RI_ASSERT(d->min_v == 0 && d->max_v == 3 && d->def_v == 0, "ftype range");
    RI_ASSERT(d->bind == RI_BIND_LEVI && d->engine_id == RI_CTL_LEVI_FTYPE, "ftype bind");
    d = find_leg(RI_SEC_LEVI, "Drive");
    RI_ASSERT(d && d->kind == RI_CK_KNOB, "drive kind");
    RI_ASSERT(d->min_v == 0 && d->max_v == 127 && d->def_v == 0, "drive range");
    RI_ASSERT(d->bind == RI_BIND_LEVI && d->engine_id == RI_CTL_LEVI_DRIVE, "drive bind");
    RI_ASSERT(ri_ctlreg_auto_id(find_leg(RI_SEC_LEVI, "Type")) == RI_CTL_LEVI_FTYPE, "ftype key");
    RI_ASSERT(ri_ctlreg_auto_id(find_leg(RI_SEC_LEVI, "Drive")) == RI_CTL_LEVI_DRIVE, "drive key");
    RI_ASSERT(ri_auto_allowed(RI_CTL_LEVI_FTYPE) && ri_auto_allowed(RI_CTL_LEVI_DRIVE), "filter keys allowed");
    /* Bottom strip rows: analog + envelope, keys, engine apply. */
    d = find_leg(RI_SEC_LEVI, "Attack");
    RI_ASSERT(d && d->kind == RI_CK_KNOB, "attack kind");
    RI_ASSERT(d->bind == RI_BIND_LEVI && d->engine_id == RI_CTL_LEVI_ATTACK, "attack bind");
    RI_ASSERT(ri_ctlreg_auto_id(d) == RI_CTL_LEVI_ATTACK, "attack key");
    RI_ASSERT(ri_auto_allowed(RI_CTL_LEVI_CUTOFF2) && ri_auto_allowed(RI_CTL_LEVI_RESO2) &&
        ri_auto_allowed(RI_CTL_LEVI_ATTACK) && ri_auto_allowed(RI_CTL_LEVI_DECAY) &&
        ri_auto_allowed(RI_CTL_LEVI_SUSTAIN) && ri_auto_allowed(RI_CTL_LEVI_RELEASE) &&
        ri_auto_allowed(RI_CTL_LEVI_LOOP), "strip keys allowed");
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
    ev.value = RI_CTL_LEVI_CUTOFF2;
    ev.flags = 96u;
    ri_engine_apply_event(&e, &ev);
    RI_ASSERT(e.slevi.v[3].cutoff2 < RI_LEVI_DEF_CUTOFF, "cutoff2 applied");
    ev.value = RI_CTL_LEVI_ATTACK;
    ev.flags = 27u;
    ri_engine_apply_event(&e, &ev);
    RI_ASSERT(e.slevi.v[3].st[0][1].env.times[RI_LEVI_SEG_A] > 0.004f &&
        e.slevi.v[3].st[0][1].env.times[RI_LEVI_SEG_A] < 0.006f, "attack applied");
    ev.value = RI_CTL_LEVI_LOOP;
    ev.flags = 1u;
    ri_engine_apply_event(&e, &ev);
    RI_ASSERT(e.slevi.v[3].st[0][1].env.loop == 1u, "loop applied");
    /* Algo/morph/opmode keys apply section-wide (packed opmode). */
    ev.value = RI_CTL_LEVI_ALGO;
    ev.flags = 5u;
    ri_engine_apply_event(&e, &ev);
    {
        uint32_t v;
        for (v = 0u; v < RI_LEVI_NVOICES; v++)
            RI_ASSERT(levi_algo_get(&e.slevi, v) == 5, "v%u algo applied", v);
    }
    ev.value = RI_CTL_LEVI_MORPH;
    ev.flags = 50u;
    ri_engine_apply_event(&e, &ev);
    RI_ASSERT(levi_morph_get(&e.slevi, 3u) == 50, "morph applied");
    ev.value = RI_CTL_LEVI_OPMODE;
    ev.flags = (uint8_t)(3u * 16u + 2u);
    ri_engine_apply_event(&e, &ev);
    RI_ASSERT(e.slevi.v[3].op[3].mode == RI_LEVI_PWM, "opmode applied");
    RI_ASSERT(e.slevi.v[3].op[2].mode == RI_LEVI_FM, "other op untouched");
    ev.flags = (uint8_t)(8u * 16u);
    ri_engine_apply_event(&e, &ev);
    RI_ASSERT(e.slevi.v[3].op[3].mode == RI_LEVI_PWM, "bad op ignored");
    ri_engine_apply_event(0, &ev);
    RI_RESULT("levictl");
}
