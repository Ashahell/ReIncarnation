/* panelctl.c — panel value change -> control plane bodies (G9b Step 2).
 * See header for the law. Render-contract safe: bounded, caller storage. */
#include "gui/panelctl.h"
#include "gui/ctlreg.h"
#include "gui/sectlevi.h"
#include "gui/sectui.h"
#include "engine/seq/ctlplane.h"
#include "engine/seq/autolane.h"

int ri_panel_ctl_send(struct RIControlPlane *ctl, uint16_t reg_id, int value) {
    const struct RICtlDef *d;
    uint16_t key;
    uint8_t v;
    if (!ctl)
        return 2;
    d = ri_ctlreg_find(reg_id);
    if (!d)
        return 1;
    v = (value < 0) ? 0u : (value > 127) ? 127u : (uint8_t)value;
    /* S4b: the master song-data fader rides the normal lane key (0x0B50
     * via ri_ctlreg_auto_id) to the post-master gain. */
    key = ri_ctlreg_auto_id(d);
    if (key == 0u)
        return 1;
    return (ri_ctl_send(ctl, key, v) == 0) ? 0 : 1;
}

int ri_panel_ctl_send_key(struct RIControlPlane *ctl, uint16_t key, int value) {
    uint8_t v;
    if (!ctl)
        return 2;
    if (!ri_auto_allowed(key))
        return 1;
    v = (value < 0) ? 0u : (value > 127) ? 127u : (uint8_t)value;
    return (ri_ctl_send(ctl, key, v) == 0) ? 0 : 1;
}

/* Drain budget for the Levi adoption: half the plane, so a full page
 * (8 slots) never fills it between drains. */
#define LEVI_ADOPT_BUDGET (RI_CTL_CAP / 2u)

static void adopt_page_zero(struct RISectLevi *s) {
    while (ri_slevi_press(s, RI_SLEVI_PAGEUP))
        ;
}

/* One encoder slot exactly the way a live knob turn sends it
 * (app/riapp.c sync_values): explicit per-op key when the slot has one,
 * else the registry row of the slot's target at the panel's value. */
static uint32_t adopt_slot(struct RISectLevi *s, struct RIControlPlane *ctl,
    uint32_t k, void *dctx, ri_panel_drain_fn drain) {
    uint16_t key = 0u;
    int kv = 0;
    if (!ri_slevi_enc_live(s, k))
        return 0u;
    if (ri_slevi_ctl_key(s, RI_SLEVI_ENC0 + k, &key, &kv) == 1) {
        if (ri_panel_ctl_send_key(ctl, key, kv) != 0)
            return 0u;
    } else {
        uint32_t t = ri_slevi_ctl_idx(s, RI_SLEVI_ENC0 + k);
        uint16_t reg = (uint16_t)(((uint16_t)RI_SEC_LEVI << 8) | (uint16_t)(t & 0xFFu));
        if (ri_panel_ctl_send(ctl, reg, ri_slevi_value(s, t)) != 0)
            return 0u;
    }
    if (ri_ctl_pending(ctl) >= LEVI_ADOPT_BUDGET)
        drain(dctx);
    return 1u;
}

/* Every page of the current module (caller opens the module first). */
static uint32_t adopt_module_pages(struct RISectLevi *s, struct RIControlPlane *ctl,
    void *dctx, ri_panel_drain_fn drain) {
    uint32_t sent = 0u, pg, k, np = ri_slevi_page_count(s);
    adopt_page_zero(s);
    for (pg = 0u; pg < np; pg++) {
        if (pg > 0u && !ri_slevi_press(s, RI_SLEVI_PAGEDN))
            break;
        for (k = 0u; k < RI_SLEVI_NENC; k++)
            sent += adopt_slot(s, ctl, k, dctx, drain);
    }
    return sent;
}

/* OSC module on oscillator o (setting OPSEL while it is already open
 * steps the page, so skip the set then). */
static void adopt_open_osc(struct RISectLevi *s, uint32_t o) {
    if (s->val[RI_SLEVI_MODULE] != (int16_t)RI_SLEVI_M_OSC || s->opsel != (uint8_t)o)
        ri_slevi_set_value(s, RI_SLEVI_OPSEL, (int)o);
    adopt_page_zero(s);
}

uint32_t ri_panel_levi_adopt(struct RISectLevi *s, struct RIControlPlane *ctl,
    void *dctx, ri_panel_drain_fn drain) {
    uint32_t sent = 0u, i, m, o;
    int16_t saved_module;
    uint8_t saved_page, saved_opsel;
    if (!s || !ctl || !drain)
        return 0u;
    saved_module = s->val[RI_SLEVI_MODULE];
    saved_page = s->page;
    saved_opsel = s->opsel;
    /* Every keyed registry row at the panel's current value. Keyless
     * rows (encoders, module/page/display placeholders) report nonzero
     * and are skipped, same as a live turn of one. */
    for (i = 0u; i < RI_SLEVI_NCTL; i++) {
        uint16_t reg = (uint16_t)(((uint16_t)RI_SEC_LEVI << 8) | (uint16_t)i);
        if (ri_panel_ctl_send(ctl, reg, ri_slevi_value(s, i)) == 0) {
            sent++;
            if (ri_ctl_pending(ctl) >= LEVI_ADOPT_BUDGET)
                drain(dctx);
        }
    }
    /* Every live encoder slot on every module page. The OSC module shows
     * the selected oscillator, so open all eight (group pages cover the
     * same params per encoder and ride along). */
    for (m = 0u; m < RI_SLEVI_NMOD; m++) {
        if (m == RI_SLEVI_M_OSC) {
            for (o = 0u; o < RI_LEVI_NOPS; o++) {
                adopt_open_osc(s, o);
                sent += adopt_module_pages(s, ctl, dctx, drain);
            }
        } else {
            ri_slevi_set_value(s, RI_SLEVI_MODULE, (int)m);
            sent += adopt_module_pages(s, ctl, dctx, drain);
        }
    }
    /* Back where the panel was (OPSEL first: it always lands on OSC). */
    if (s->opsel != saved_opsel)
        ri_slevi_set_value(s, RI_SLEVI_OPSEL, (int)saved_opsel);
    ri_slevi_set_value(s, RI_SLEVI_MODULE, (int)saved_module);
    adopt_page_zero(s);
    for (i = 0u; i < saved_page; i++)
        ri_slevi_press(s, RI_SLEVI_PAGEDN);
    if (ri_ctl_pending(ctl))
        drain(dctx);
    return sent;
}

/* MIDI value push: what changed on the panel since the shadow was taken
 * rides the bridge, exactly one message per control (the live law). */
static int push_is_value(uint32_t kind) {
    return kind == RI_CK_KNOB || kind == RI_CK_FADER || kind == RI_CK_SWITCH ||
        kind == RI_CK_SELECTOR || kind == RI_CK_DISPLAY;
}

static uint32_t push_walk(struct RISectUI **uis, const uint8_t *sections,
    uint32_t nsec, struct RIControlPlane *ctl, uint8_t *shadow, int send) {
    uint32_t i, k, out = 0u;
    uint32_t n = ri_ctlreg_count();
    if (!uis || !sections || !shadow)
        return 0u;
    for (i = 0u; i < nsec; i++) {
        struct RISectUI *u = uis[i];
        if (!u)
            continue;
        for (k = 0u; k < n; k++) {
            const struct RICtlDef *d = ri_ctlreg_at(k);
            uint32_t idx;
            int v;
            if (!d || d->section != sections[i] || !push_is_value(d->kind))
                continue;
            idx = d->reg_id & 0xFFu;
            v = ri_sui_value(u, idx);
            if (v < 0)
                v = 0;
            if (v > 127)
                v = 127;
            if (send) {
                if (shadow[i * 256u + idx] == (uint8_t)v)
                    continue;
                if (ri_panel_ctl_send(ctl, d->reg_id, v) != 0)
                    continue;
                shadow[i * 256u + idx] = (uint8_t)v;
                out++;
            } else {
                shadow[i * 256u + idx] = (uint8_t)v;
                out++;
            }
        }
    }
    return out;
}

uint32_t ri_panel_midi_push(struct RISectUI **uis, const uint8_t *sections,
    uint32_t nsec, struct RIControlPlane *ctl, uint8_t *shadow) {
    if (!ctl)
        return 0u;
    return push_walk(uis, sections, nsec, ctl, shadow, 1);
}

uint32_t ri_panel_midi_shadow_init(struct RISectUI **uis,
    const uint8_t *sections, uint32_t nsec, uint8_t *shadow) {
    return push_walk(uis, sections, nsec, 0, shadow, 0);
}
