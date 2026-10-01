/* t91_canvas_events — portability T3b: pure canvas event logic.
 * Synthetic events on a 303 section, mirroring t70/t71 behaviours:
 * focus click, knob drag with the 150-px law, arrow repeat on ticks,
 * key routing/ownership, menu reset.
 */
#include <stdio.h>
#include <stdint.h>
#include "tests/helpers/ri_assert.h"
#include "app/core/canvas_events.h"
#include "platform/pal/ri_pal_input.h"
#include "gui/ctlreg.h"

static struct RISectUI T_UI;
static struct RIPanelUI T_PANEL;
static struct RICevState T_ST;
static const struct RIGeoSection *T_GEO;

/* First canvas point hitting a knob control (bounded scan). */
static int find_knob(int *x, int *y) {
    int ix, iy;
    for (iy = 0; iy < 1000; iy += 4) {
        for (ix = 0; ix < 2000; ix += 4) {
            int opt = -1;
            uint16_t id = ri_geo_hit_opt(T_GEO, ix, iy, 0, &opt);
            if (id != 0xFFFFu) {
                const struct RICtlDef *cd =
                    ri_ctlreg_find((uint16_t)((T_UI.section << 8) | (id & 0xFFu)));
                if (cd && cd->kind == RI_CK_KNOB && opt < 0) {
                    *x = ix;
                    *y = iy;
                    return 1;
                }
            }
        }
    }
    return 0;
}

/* First canvas point hitting the given control index as a plain button. */
static int find_button(const struct RIGeoSection *geo, uint32_t idx, int *x, int *y) {
    int ix, iy;
    for (iy = 0; iy < 1000; iy += 2) {
        for (ix = 0; ix < 2000; ix += 2) {
            uint16_t id = ri_geo_hit_opt(geo, ix, iy, 0, &(int){ 0 });
            if (id != 0xFFFFu && (id & 0xFFu) == idx) {
                *x = ix;
                *y = iy;
                return 1;
            }
        }
    }
    return 0;
}

/* First canvas point hitting a value-display arrow in the given geo. */
static int find_stepper(const struct RIGeoSection *geo, int *x, int *y) {
    int ix, iy;
    for (iy = 0; iy < 1000; iy += 4) {
        for (ix = 0; ix < 2000; ix += 4) {
            int opt = -1;
            uint16_t id = ri_geo_hit_opt(geo, ix, iy, 0, &opt);
            if (id != 0xFFFFu && (opt == RI_GEO_HIT_UP || opt == RI_GEO_HIT_DOWN)) {
                *x = ix;
                *y = iy;
                return 1;
            }
        }
    }
    return 0;
}

int main(void) {
    int kx = 0, ky = 0, sx = 0, sy = 0, v0, v1;
    uint32_t r;
    ri_sui_init(&T_UI, RI_SEC_SYNTH1);
    ri_panel_init(&T_PANEL);
    ri_cev_init(&T_ST);
    T_GEO = ri_geo_section(RI_SEC_SYNTH1);
    RI_ASSERT(T_GEO != 0, "geo");
    RI_ASSERT(T_ST.drag_id == RI_CEV_NODRAG, "init");
    /* Ticks with no armed repeat do nothing. */
    RI_ASSERT(ri_cev_tick(&T_ST, &T_UI) == 0u, "idle tick");
    RI_ASSERT(ri_cev_tick(0, &T_UI) == 0u, "null tick");
    /* Keys: no owner / no panel / unmapped do nothing. */
    RI_ASSERT(ri_cev_key(&T_ST, &T_UI, &T_PANEL, 0, RI_KEY_SPACE, 0u) == 0u, "no owner");
    RI_ASSERT(ri_cev_key(&T_ST, &T_UI, 0, 1, RI_KEY_SPACE, 0u) == 0u, "no panel");
    RI_ASSERT(ri_cev_key(&T_ST, &T_UI, &T_PANEL, 1, 0x7Fu, 0u) == 0u, "unmapped");
    /* Space = transport stop/play: eaten, and it flips transport state. */
    r = ri_cev_key(&T_ST, &T_UI, &T_PANEL, 1, RI_KEY_SPACE, 0u);
    RI_ASSERT(r & RI_CEV_EAT, "space eaten");
    /* Ctrl+2 = tab request 1: eaten, no canvas state change. */
    T_PANEL.tab_req = -1;
    r = ri_cev_key(&T_ST, &T_UI, &T_PANEL, 1, 0x02u, RI_QUAL_CONTROL);
    RI_ASSERT((r & RI_CEV_EAT) && !(r & RI_CEV_CHANGED), "tab eaten");
    RI_ASSERT(T_PANEL.tab_req == 1, "tab request");
    /* Focus click on background: changed (focus), not eaten. */
    {
        struct RISectUI ui808;
        ri_sui_init(&ui808, RI_SEC_808);
        r = ri_cev_button(&T_ST, &ui808, &T_PANEL, T_GEO, 0, 5, 5, 2000, 1000, 0, 0u);
        RI_ASSERT((r & RI_CEV_CHANGED) != 0u, "focus changed");
        RI_ASSERT((r & RI_CEV_EAT) == 0u, "focus not eaten");
        RI_ASSERT(T_PANEL.focus == RI_FOCUS_808, "focus 808");
    }
    /* Knob drag with the 150-px law. */
    RI_ASSERT(find_knob(&kx, &ky), "knob at %d,%d", kx, ky);
    v0 = ri_sui_value(&T_UI, (uint32_t)(ri_geo_hit_opt(T_GEO, kx, ky, 0, &(int){ 0 }) & 0xFFu));
    r = ri_cev_button(&T_ST, &T_UI, &T_PANEL, T_GEO, 0, kx, ky, 2000, 1000, 0, 0u);
    RI_ASSERT(r & RI_CEV_EAT, "drag armed");
    RI_ASSERT(T_ST.drag_id != RI_CEV_NODRAG, "drag id");
    r = ri_cev_move(&T_ST, &T_UI, kx + 150, ky, 0);
    RI_ASSERT(r & RI_CEV_EAT, "drag eaten");
    v1 = ri_sui_value(&T_UI, (uint32_t)(ri_geo_hit_opt(T_GEO, kx, ky, 0, &(int){ 0 }) & 0xFFu));
    RI_ASSERT(v1 != v0, "drag moved %d->%d", v0, v1);
    r = ri_cev_button(&T_ST, &T_UI, &T_PANEL, T_GEO, 0, kx + 150, ky, 2000, 1000, 1, 0u);
    RI_ASSERT(r & RI_CEV_EAT, "up eaten");
    RI_ASSERT(T_ST.drag_id == RI_CEV_NODRAG, "drag cleared");
    RI_ASSERT(ri_cev_move(&T_ST, &T_UI, kx + 160, ky, 0) == 0u, "move after up");
    /* Arrow repeat on the transport tempo stepper (steppers live in
     * FX/transport layouts, not in synth sections): down arms, 4th tick
     * steps, up clears. */
    {
        struct RISectUI trui;
        const struct RIGeoSection *trgeo = ri_geo_section(RI_SEC_TRANSPORT);
        RI_ASSERT(trgeo != 0, "tr geo");
        ri_sui_init(&trui, RI_SEC_TRANSPORT);
        RI_ASSERT(find_stepper(trgeo, &sx, &sy), "stepper at %d,%d", sx, sy);
        {
            uint32_t idx = ri_geo_hit_opt(trgeo, sx, sy, 0, &(int){ 0 }) & 0xFFu;
            int w0 = ri_sui_value(&trui, idx);
            r = ri_cev_button(&T_ST, &trui, &T_PANEL, trgeo, 0, sx, sy, 2000, 1000, 0, 0u);
            RI_ASSERT(r & RI_CEV_EAT, "step eaten");
            RI_ASSERT(ri_cev_tick(&T_ST, &trui) == 0u, "tick1");
            RI_ASSERT(ri_cev_tick(&T_ST, &trui) == 0u, "tick2");
            RI_ASSERT(ri_cev_tick(&T_ST, &trui) == 0u, "tick3");
            r = ri_cev_tick(&T_ST, &trui);
            RI_ASSERT((r & RI_CEV_CHANGED) != 0u, "tick4 steps");
            RI_ASSERT(ri_sui_value(&trui, idx) != w0, "stepped");
            ri_cev_button(&T_ST, &trui, &T_PANEL, trgeo, 0, sx, sy, 2000, 1000, 1, 0u);
            RI_ASSERT(ri_cev_tick(&T_ST, &trui) == 0u, "tick after up");
        }
    }
    /* Tap tempo (P9e): the click path carries the press time, so a tap
     * works from a canvas event; without one it is a quiet press. */
    {
        struct RISectUI trui;
        const struct RIGeoSection *trgeo = ri_geo_section(RI_SEC_TRANSPORT);
        ri_sui_init(&trui, RI_SEC_TRANSPORT);
        RI_ASSERT(find_button(trgeo, RI_STR_TAP, &sx, &sy), "tap button at %d,%d", sx, sy);
        RI_ASSERT(ri_geo_hit_opt(trgeo, sx, sy, 0, &(int){ 0 }) ==
            (uint16_t)((RI_SEC_TRANSPORT << 8) | RI_STR_TAP), "tap button hit");
        r = ri_cev_button(&T_ST, &trui, &T_PANEL, trgeo, 0, sx, sy, 2000, 1000, 0, 0u);
        RI_ASSERT((r & RI_CEV_EAT) != 0u && (r & RI_CEV_CHANGED) == 0u,
            "a clockless tap is eaten and silent");
        r = ri_cev_button(&T_ST, &trui, &T_PANEL, trgeo, 0, sx, sy, 2000, 1000, 0, 100u);
        RI_ASSERT((r & RI_CEV_CHANGED) == 0u, "the first tap only starts the run");
        r = ri_cev_button(&T_ST, &trui, &T_PANEL, trgeo, 0, sx, sy, 2000, 1000, 0, 1100u);
        RI_ASSERT((r & RI_CEV_CHANGED) != 0u, "the second tap repaints");
        RI_ASSERT(ri_sui_value(&trui, RI_STR_TEMPO) == 60, "1000 ms apart is 60 bpm");
    }
    /* Menu-down resets a control. */
    r = ri_cev_button(&T_ST, &T_UI, &T_PANEL, T_GEO, 0, kx, ky, 2000, 1000, 2, 0u);
    RI_ASSERT(r & RI_CEV_EAT, "menu eaten");
    /* Select-up with nothing armed is quiet. */
    ri_cev_init(&T_ST);
    RI_ASSERT(ri_cev_button(&T_ST, &T_UI, &T_PANEL, T_GEO, 0, kx, ky, 2000, 1000, 1, 0u) == 0u, "idle up");
    RI_RESULT("canvas_events");
}
