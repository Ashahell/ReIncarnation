/* t1_knob — Task 12 (gate G12): GUI pure-logic tests.
 * Gates (brief order): knob 150 px = full 0..127 within ±5%; Shift-fine
 * ×0.1 within ±10% (1500 px full); clamp at both ends; commit-on-release
 * single-event rule (N moves → exactly 1 commit); fader 100 px full;
 * step toggle; LED ≤33 ms + 174 BPM chase-scripts; zoom table; panel
 * tables (6 panels, stable IDs, dummy via same lookup); E1 programming
 * helpers (pitch advance, accent cycle, flam glow).
 * Analysis may use libm (tests/ only; gui/ stays kernels-free).
 */
#include <stdio.h>
#include <math.h>
#include "tests/helpers/ri_assert.h"
#include "gui/knob_logic.h"
#include "gui/panels.h"

int main(void) {
    struct RiGesture g;
    double v;
    int i;

    /* --- 0. knob full travel: 150 px up from 0 → 127 ±5% --- */
    v = ri_knob_drag_to_value(0.0, 0.0, 150.0, 0);
    RI_ASSERT(fabs(v - 127.0) <= 127.0 * 0.05, "knob full %f", v);
    /* partial: 75 px → half ±5% */
    v = ri_knob_drag_to_value(0.0, 0.0, 75.0, 0);
    RI_ASSERT(fabs(v - 63.5) <= 127.0 * 0.05, "knob half %f", v);
    /* direction: down-drag decreases */
    v = ri_knob_drag_to_value(64.0, 0.0, -75.0, 0);
    RI_ASSERT(v < 64.0, "knob down %f", v);
    RI_ASSERT(fabs(v - 0.5) <= 127.0 * 0.05, "knob down val %f", v);

    /* --- 1. fine ×0.1 ±10%: 1500 px fine == 150 px coarse --- */    v = ri_knob_drag_to_value(0.0, 0.0, 1500.0, 1);
    RI_ASSERT(fabs(v - 127.0) <= 127.0 * 0.10, "fine full %f", v);
    {
        double a = ri_knob_drag_to_value(10.0, 0.0, 150.0, 1);
        double b = ri_knob_drag_to_value(10.0, 0.0, 15.0, 0);
        RI_ASSERT(fabs(a - b) <= 127.0 * 0.10, "fine ratio %f vs %f", a, b);
    }

    /* --- 1b. horizontal axis (owner amendment 2026-09-24, both-axes
     * mapping): right increases like up does; eff = dx + dy. --- */
    v = ri_knob_drag_to_value(0.0, 150.0, 0.0, 0);
    RI_ASSERT(fabs(v - 127.0) <= 127.0 * 0.05, "knob right full %f", v);
    v = ri_knob_drag_to_value(64.0, -75.0, 0.0, 0);
    RI_ASSERT(fabs(v - 0.5) <= 127.0 * 0.05, "knob left val %f", v);
    /* diagonal adds (documented 2x on 45°): 75+75 = full */
    v = ri_knob_drag_to_value(0.0, 75.0, 75.0, 0);
    RI_ASSERT(fabs(v - 127.0) <= 127.0 * 0.05, "knob diag %f", v);
    /* opposing axes cancel */
    v = ri_knob_drag_to_value(64.0, 50.0, -50.0, 0);
    RI_ASSERT(fabs(v - 64.0) <= 1.0, "knob cancel %f", v);
    /* fine applies to horizontal too */
    v = ri_knob_drag_to_value(0.0, 1500.0, 0.0, 1);
    RI_ASSERT(fabs(v - 127.0) <= 127.0 * 0.10, "fine horiz %f", v);

    /* --- 2. clamp: overshoot sticks, NaN fails closed --- */
    RI_ASSERT(ri_knob_drag_to_value(120.0, 0.0, 500.0, 0) == 127.0, "clamp hi");
    RI_ASSERT(ri_knob_drag_to_value(7.0, 0.0, -500.0, 0) == 0.0, "clamp lo");
    RI_ASSERT(ri_ctl_clamp(0.0 / 0.0) == 0.0, "nan closed");
    RI_ASSERT(ri_ctl_quantize(200.0) == 127, "quant hi");
    RI_ASSERT(ri_ctl_quantize(-3.0) == 0, "quant lo");
    RI_ASSERT(ri_ctl_quantize(63.5) == 64, "quant round");

    /* --- 2b. accumulator clamp (grab reversal bites at once): --- */
    {
        double ax, ay;
        ax = 500.0; ay = 0.0;
        ri_knob_clamp_acc(64.0, &ax, &ay, 0);
        RI_ASSERT(fabs(ax - 74.41) < 0.1 && ay == 0.0, "acc hi %f", ax);
        ax = -500.0; ay = 0.0;
        ri_knob_clamp_acc(64.0, &ax, &ay, 0);
        RI_ASSERT(fabs(ax + 75.59) < 0.1 && ay == 0.0, "acc lo %f", ax);
        ax = 1000.0; ay = 1000.0;
        ri_knob_clamp_acc(0.0, &ax, &ay, 1);
        RI_ASSERT(fabs(ax - 750.0) < 0.1 && fabs(ay - 750.0) < 0.1,
            "acc fine %f,%f", ax, ay);
        ax = 10.0; ay = 20.0;
        ri_knob_clamp_acc(64.0, &ax, &ay, 0);
        RI_ASSERT(ax == 10.0 && ay == 20.0, "acc inside %f,%f", ax, ay);
    }

    /* --- 3. commit-on-release: N moves → exactly 1 commit --- */
    ri_gesture_begin(&g);
    for (i = 0; i < 40; i++)
        ri_gesture_move(&g);
    RI_ASSERT(g.commits == 0, "no commit mid-drag");
    ri_gesture_end(&g);
    RI_ASSERT(g.commits == 1, "one commit on release");
    ri_gesture_end(&g);
    RI_ASSERT(g.commits == 1, "double release still one");
    /* end without begin: no commit */
    {
        struct RiGesture h = { 0, 0, 0 };
        ri_gesture_end(&h);
        RI_ASSERT(h.commits == 0, "stray end commits");
        ri_gesture_move(&h);
        RI_ASSERT(h.moves == 0, "stray move counts");
    }

    /* --- 4. fader: 100 px full ±5%, fine shared --- */
    v = ri_fader_drag_to_value(0.0, 100.0, 0);
    RI_ASSERT(fabs(v - 127.0) <= 127.0 * 0.05, "fader full %f", v);
    v = ri_fader_drag_to_value(0.0, 1000.0, 1);
    RI_ASSERT(fabs(v - 127.0) <= 127.0 * 0.10, "fader fine %f", v);

    /* --- 5. step toggle + LED/chase --- */
    RI_ASSERT(ri_step_toggle(0) == 1, "step on");
    RI_ASSERT(ri_step_toggle(1) == 0, "step off");
    RI_ASSERT(ri_step_toggle(7) == 0, "step nonzero off");
    RI_ASSERT(ri_led_lag_ok(33.0) == 1, "led edge ok");
    RI_ASSERT(ri_led_lag_ok(33.1) == 0, "led over miss");
    RI_ASSERT(ri_led_lag_ok(-1.0) == 0, "led neg miss");
    {
        double s16 = ri_step16_ms(174.0);
        RI_ASSERT(fabs(s16 - 15000.0 / 174.0) < 1e-9, "174 chase %f", s16);
        RI_ASSERT(s16 > RI_LED_FRAME_MS, "174 step exceeds frame (chase must update per step)");
        RI_ASSERT(ri_step16_ms(0.0) == 0.0, "bpm0 closed");
    }
    RI_ASSERT(ri_chase_step(0.0, 16) == 0, "chase 0");
    RI_ASSERT(ri_chase_step(17.0, 16) == 1, "chase wrap");
    RI_ASSERT(ri_chase_step(-1.0, 16) == 0, "chase neg closed");
    RI_ASSERT(ri_chase_step(5.0, 0) == 0, "chase n0 closed");

    /* --- 6. zoom table --- */
    RI_ASSERT(ri_zoom_factor(0) == 1.0, "zoom 1x");
    RI_ASSERT(ri_zoom_factor(1) == 1.5, "zoom 1.5x");
    RI_ASSERT(ri_zoom_factor(2) == 2.0, "zoom 2x");
    RI_ASSERT(ri_zoom_factor(3) == 0.0, "zoom bad closed");
    RI_ASSERT(ri_zoom_scaled_px(100, 2) == 200, "zoom px");
    RI_ASSERT(ri_zoom_scaled_px(100, 1) == 150, "zoom px15");
    RI_ASSERT(ri_zoom_scaled_px(100, 9) == 0, "zoom px bad");

    /* --- 7. E1 programming helpers --- */
    RI_ASSERT(ri_pitch_advance(3, 16) == 4, "pitch adv");
    RI_ASSERT(ri_pitch_advance(15, 16) == 0, "pitch wrap");
    RI_ASSERT(ri_pitch_advance(0, 0) == 0, "pitch n0");
    RI_ASSERT(ri_accent_cycle(0) == 1, "acc 0->1");
    RI_ASSERT(ri_accent_cycle(1) == 2, "acc 1->2");
    RI_ASSERT(ri_accent_cycle(2) == 0, "acc 2->0");
    RI_ASSERT(ri_flam_glow(1) == 1, "flam glow");
    RI_ASSERT(ri_flam_glow(0) == 0, "flam dark");

    /* --- 8. panel tables: 6 panels, stable IDs, dummy same path --- */
    RI_ASSERT(ri_panel_count() == 6u, "6 panels");
    RI_ASSERT(ri_panel_get(6) == 0, "panel oob null");
    {
        const struct RIPanelDesc *p = ri_panel_get(0);
        const struct RIPanelControl *c;
        RI_ASSERT(p && p->device == 0u, "303A slot");
        c = ri_panel_find_ctl(p, RI_CTL_303A_BASE + 0);
        RI_ASSERT(c && c->def_value == 96, "cutoff default");
        RI_ASSERT(ri_panel_find_ctl(p, 0xDEADu) == 0, "unknown ctl null");
        RI_ASSERT(ri_panel_find_ctl(0, RI_CTL_303A_BASE) == 0, "null panel null");
    }
    {
        const struct RIPanelDesc *t = ri_panel_get(5);
        RI_ASSERT(t && t->device == 5u, "transport slot");
        RI_ASSERT(ri_panel_find_ctl(t, RI_CTL_MIX_BASE + 10) != 0, "tempo ctl");
    }
    {
        const struct RIPanelDesc *d = ri_panel_dummy();
        RI_ASSERT(d && d->nctls == 2u, "dummy shape");
        RI_ASSERT(ri_panel_find_ctl(d, 0x0F00u) != 0, "dummy same lookup");
    }
    /* ID blocks disjoint across all panels */
    {
        unsigned int a, b, x, y;
        for (a = 0; a < ri_panel_count(); a++) {
            const struct RIPanelDesc *pa = ri_panel_get(a);
            for (b = a + 1; b < ri_panel_count(); b++) {
                const struct RIPanelDesc *pb = ri_panel_get(b);
                for (x = 0; x < pa->nctls; x++)
                    for (y = 0; y < pb->nctls; y++)
                        RI_ASSERT(pa->ctls[x].ctl_id != pb->ctls[y].ctl_id,
                            "id clash %x", pa->ctls[x].ctl_id);
            }
        }
    }

    RI_RESULT("knob");
}
