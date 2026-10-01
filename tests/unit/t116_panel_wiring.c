/* t116_panel_wiring — owner 2026-09-29: every knob, button and slider
 * correctly and completely wired. For every registry value control
 * (KNOB/FADER/SWITCH/SELECTOR/DISPLAY) with a lane key, one bridge send
 * drained into ri_engine_apply_event must equal the direct setter call
 * (whole-engine memcmp on twin fresh engines). Controls the bridge
 * refuses must be bind-NONE placeholders (or the live-only master).
 * Step/button/pattern/transport controls travel press/state paths and
 * stay out of this test by design.
 */
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stddef.h>
#include "tests/helpers/ri_assert.h"
#include "gui/ctlreg.h"
#include "gui/panelctl.h"
#include "gui/sectui.h"
#include "gui/secttr.h"
#include "engine/engine.h"
#include "engine/live.h"
#include "engine/seq/ctlplane.h"
#include "engine/seq/autolane.h"
#include "engine/dsp/rb303.h"
#include "engine/dsp/rb808.h"
#include "engine/dsp/rb909.h"
#include "engine/dsp/levi.h"

static int is_value(uint32_t kind) {
    return kind == RI_CK_KNOB || kind == RI_CK_FADER || kind == RI_CK_SWITCH ||
        kind == RI_CK_SELECTOR || kind == RI_CK_DISPLAY;
}

/* P7b: RILeviFx embeds two RIReverb cores whose line pointers are absolute
 * per instance (non-owning views of the set's static backing), so a raw
 * whole-engine memcmp always differs. Compare around the pointer words and
 * pin the handles' scalar fields instead. */
static int engines_equal(const struct RIEngine *a, const struct RIEngine *b) {
    size_t r0 = offsetof(struct RIEngine, slevi) +
        offsetof(struct RILeviSet, fx) + offsetof(struct RILeviFx, rvl);
    size_t r1 = r0 + 2u * sizeof(struct RIReverb);
    const struct RILeviFx *fa = &a->slevi.fx, *fb = &b->slevi.fx;
    if (memcmp(a, b, r0))
        return 0;
    if (memcmp((const unsigned char *)a + r1, (const unsigned char *)b + r1,
            sizeof *a - r1))
        return 0;
    return fa->rvl.fb == fb->rvl.fb && fa->rvr.fb == fb->rvr.fb &&
        fa->rvl.cap == fb->rvl.cap && fa->rvr.cap == fb->rvr.cap &&
        !memcmp(fa->rvl.len, fb->rvl.len, sizeof fa->rvl.len) &&
        !memcmp(fa->rvr.len, fb->rvr.len, sizeof fa->rvr.len);
}

/* Direct-setter mirror of engine_automation for one lane key (see
 * engine_automation; any drift between the two is the bug). */
static void direct(struct RIEngine *e, uint16_t key, uint8_t val) {
    uint32_t blk = key & 0xFF00u, hi = (key >> 4) & 0xFu, lo = key & 0xFu, v;
    if (blk == 0x0300u) {
        if (hi == 0u)
            rb303_set_param(&e->v303a, key, val);
        else if (hi == 1u)
            rb303_set_param(&e->v303b, key, val);
        return;
    }
    if (blk == 0x0A00u) {
        ri_engine_fx_set(e, key, val);
        return;
    }
    if (blk == RI_AUTO_BLK_808) {
        if (hi == (RI_CTL_808_ACCENT & 0xFu)) {
            if (lo == 0u)
                for (v = 0u; v < RI_808_NSOUNDS; v++)
                    rb808_set_param(&e->s808.v[v], RI_CTL_808_ACCENT, val);
        } else if (hi < (RI_CTL_808_ACCENT & 0xFu) && lo < RI_808_NSOUNDS) {
            rb808_set_param(&e->s808.v[lo], RI_CTL_808_LEVEL + hi, val);
        }
        return;
    }
    if (blk == RI_AUTO_BLK_909) {
        if (lo == RI_AUTO_909_HATPAIR && hi == (RI_CTL_909_LEVEL & 0xFu))
            rb909_set_hat_level(&e->s909, val);
        else if (hi <= (RI_CTL_909_DECAY & 0xFu) && lo < RI_909_NVOICES)
            rb909_set_param(&e->s909.v[lo], RI_CTL_909_TUNE + hi, val);
        return;
    }
    if (blk == RI_AUTO_BLK_LEVI) {
        for (v = 0u; v < RI_LEVI_NVOICES; v++)
            levi_set_param_ui(&e->slevi, v, key & 0xFFu, val);
        return;
    }
    if (blk == RI_AUTO_BLK_MIX && hi >= 1u && hi <= RI_AUTO_STRIP_LEVI + 1u) {
        uint32_t strip = hi - 1u;
        int owner = strip == RI_AUTO_STRIP_MASTER ? RI_ROUTE_MASTER
            : strip == RI_AUTO_STRIP_LEVI ? 4 : (int)strip;
        if (lo >= RI_AUTO_MIX_DIST && lo < RI_AUTO_MIX_DIST + RI_ROUTE_NUNITS) {
            uint32_t unit = lo - RI_AUTO_MIX_DIST;
            if (strip == RI_AUTO_STRIP_MASTER && unit != RI_ROUTE_COMP)
                return;
            if (val)
                ri_route_assign(&e->route, unit, owner);
            else if (ri_route_owner(&e->route, unit) == owner)
                ri_route_assign(&e->route, unit, RI_ROUTE_NONE);
            return;
        }
        if (strip == RI_AUTO_STRIP_MASTER) {
            if (lo == RI_AUTO_MIX_LEVEL)
                ri_engine_set_master(e, val);
            return;
        }
        if (lo == RI_AUTO_MIX_LEVEL)
            ri_engine_set_level(e, (uint32_t)owner, val);
        else if (lo == RI_AUTO_MIX_PAN)
            ri_engine_set_pan(e, (uint32_t)owner, val);
        else if (lo == RI_AUTO_MIX_SEND)
            ri_engine_set_send(e, (uint32_t)owner, val);
    }
}

int main(void) {
    uint32_t n = ri_ctlreg_count(), k, ntested = 0u, nrefused = 0u, nalt = 0u;
    uint32_t testedsec[RI_SEC_COUNT] = { 0 };
    for (k = 0u; k < n; k++) {
        const struct RICtlDef *d = ri_ctlreg_at(k);
        struct RIControlPlane pl;
        struct RIEngine ea, eb;
        struct RIEvent ev[8];
        uint32_t seq = 0u, nd, q;
        uint16_t key;
        int v;
        if (!d || !is_value(d->kind))
            continue;
        key = ri_ctlreg_auto_id(d);
        if (key == 0u) {
            /* Refused controls must be documented placeholders, the
             * live-only master, or the session-followed tempo. */
            struct RIControlPlane p2;
            ri_ctl_init(&p2);
            if (d->section == RI_SEC_MASTER && d->kind == RI_CK_FADER) {
                /* Live-only monitoring fader (t115): sounds now, no lane. */
                struct RIEngine ea, eb;
                struct RIEvent ev[8];
                uint32_t seq = 0u, nd, q;
                memset(&ea, 0, sizeof ea);
                memset(&eb, 0, sizeof eb);
                ri_engine_init(&ea);
                ri_engine_init(&eb);
                ri_engine_defaults(&ea);
                ri_engine_defaults(&eb);
                RI_ASSERT(ri_panel_ctl_send(&p2, d->reg_id, d->max_v) == 0,
                    "master bridge refuses");
                nd = ri_ctl_drain(&p2, ev, 8u, 0u, &seq);
                RI_ASSERT(nd == 1u, "master drains one");
                for (q = 0u; q < nd; q++)
                    ri_engine_apply_event(&ea, &ev[q]);
                ri_engine_set_master(&eb, (uint8_t)(d->max_v & 127));
                RI_ASSERT(engines_equal(&ea, &eb), "master wiring");
                ntested++;
                continue;
            }
            if (d->section == RI_SEC_TRANSPORT && d->kind == RI_CK_DISPLAY &&
                (d->reg_id & 0xFFu) == RI_STR_TEMPO) {
                /* Tempo follows the session (bpm setter, tested below). */
                nalt++;
                continue;
            }
            RI_ASSERT(ri_panel_ctl_send(&p2, d->reg_id, d->max_v) != 0,
                "bridge sends keyless %04x", d->reg_id);
            RI_ASSERT(d->bind == RI_BIND_NONE, "keyless but bound %04x", d->reg_id);
            nrefused++;
            continue;
        }
        /* Wired: bridge -> plane -> drain -> apply == direct setter. */
        memset(&ea, 0, sizeof ea);
        memset(&eb, 0, sizeof eb);
        ri_engine_init(&ea);
        ri_engine_init(&eb);
        ri_engine_defaults(&ea);
        ri_engine_defaults(&eb);
        ri_ctl_init(&pl);
        v = d->max_v;
        RI_ASSERT(ri_panel_ctl_send(&pl, d->reg_id, v) == 0,
            "bridge refuses %04x", d->reg_id);
        nd = ri_ctl_drain(&pl, ev, 8u, 0u, &seq);
        RI_ASSERT(nd == 1u, "drain one %04x", d->reg_id);
        for (q = 0u; q < nd; q++)
            ri_engine_apply_event(&ea, &ev[q]);
        direct(&eb, key, (uint8_t)(v & 127));
        RI_ASSERT(engines_equal(&ea, &eb), "wiring %04x %s/%s",
            d->reg_id, d->group, d->legend);
        ntested++;
        testedsec[d->section]++;
    }
    RI_ASSERT(ntested >= 100u, "wired %u controls", ntested);
    RI_ASSERT(nrefused >= 5u, "placeholders %u", nrefused);
    RI_ASSERT(nalt == 1u, "session-followed %u", nalt);
    /* Tempo follow: the knob was display-only; the session adopts bpm. */
    {
        struct RILiveSession s;
        struct RIEvent sc[8];
        static float ol[64], or_[64];
        memset(&s, 0, sizeof s);
        ri_live_init(&s, 96u, 48000.0f, 140.0f, RI_ENGINE_S303A, sc, 8u);
        RI_ASSERT(s.bpm == 140.0f, "init bpm");
        ri_live_set_bpm(&s, 120.0f);
        RI_ASSERT(s.bpm == 120.0f, "set bpm");
        RI_ASSERT(s.nspq == 500000000ULL, "nspq follows");
        ri_live_set_bpm(&s, 10.0f);
        RI_ASSERT(s.bpm == 120.0f, "reject low");
        ri_live_set_bpm(&s, 600.0f);
        RI_ASSERT(s.bpm == 120.0f, "reject high");
        ri_live_set_bpm(0, 120.0f);
        ri_live_play(&s);
        RI_ASSERT(ri_live_render(&s, ol, or_, 64u) == 64u, "render");
        RI_ASSERT(s.eng.tempo == 120.0f, "engine adopts");
    }
    for (k = 0u; k < RI_SEC_COUNT; k++) {
        uint32_t j, has = 0u;
        for (j = 0u; j < n; j++) {
            const struct RICtlDef *d = ri_ctlreg_at(j);
            if (d && d->section == k && is_value(d->kind) && ri_ctlreg_auto_id(d) != 0u)
                has = 1u;
        }
        if (has)
            RI_ASSERT(testedsec[k] > 0u, "sec %u unwired", k);
    }
    RI_RESULT("panel_wiring");
}
