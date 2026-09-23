/* gui/knob_logic.c — pure drag→value mapping (Task 12, gate G12).
 * No AROS/MUI includes, no libm, no allocation: host + AROS safe.
 * Up-drag (dy > 0) increases; screen-y callers negate first.
 */
#include "gui/knob_logic.h"

double ri_ctl_clamp(double v) {
    if (!(v == v))
        return 0.0; /* NaN fails closed */
    if (v < 0.0)
        return 0.0;
    if (v > RI_CTL_MAX)
        return RI_CTL_MAX;
    return v;
}

static double drag_to_value(double start, double dy_px, double travel, int fine) {
    double step = RI_CTL_MAX / travel;
    double v = start + dy_px * step * (fine ? (1.0 / RI_FINE_DIV) : 1.0);
    return ri_ctl_clamp(v);
}

double ri_knob_drag_to_value(double start, double dy_px, int fine) {
    return drag_to_value(start, dy_px, RI_KNOB_TRAVEL_PX, fine);
}

double ri_fader_drag_to_value(double start, double dy_px, int fine) {
    return drag_to_value(start, dy_px, RI_FADER_TRAVEL_PX, fine);
}

int ri_ctl_quantize(double v) {
    double c = ri_ctl_clamp(v);
    return (int)(c + 0.5);
}

int ri_knob_pointer_mdeg(int value) {
    if (value < 0)
        value = 0;
    if (value > 127)
        value = 127;
    return (270000 * value + 63) / 127 - 135000;
}

void ri_gesture_begin(struct RiGesture *g) {
    if (!g)
        return;
    g->begun = 1;
    g->moves = 0;
    g->commits = 0;
}

void ri_gesture_move(struct RiGesture *g) {
    if (!g || !g->begun)
        return;
    g->moves++; /* notify only: commits untouched */
}

void ri_gesture_end(struct RiGesture *g) {
    if (!g || !g->begun || g->commits != 0)
        return;
    g->commits = 1; /* exactly one undo unit per gesture */
}

int ri_step_toggle(int state) {
    return state ? 0 : 1;
}

double ri_step16_ms(double bpm) {
    if (!(bpm > 0.0))
        return 0.0;
    return 15000.0 / bpm;
}

int ri_led_lag_ok(double lag_ms) {
    if (!(lag_ms == lag_ms) || lag_ms < 0.0)
        return 0;
    return lag_ms <= RI_LED_FRAME_MS ? 1 : 0;
}

int ri_chase_step(double beat_pos_16ths, int nsteps) {
    int s;
    if (nsteps <= 0 || !(beat_pos_16ths >= 0.0))
        return 0;
    s = (int)beat_pos_16ths;
    return s % nsteps;
}

double ri_zoom_factor(int level) {
    if (level == 0)
        return 1.0;
    if (level == 1)
        return 1.5;
    if (level == 2)
        return 2.0;
    return 0.0;
}

int ri_zoom_scaled_px(int px, int level) {
    double f = ri_zoom_factor(level);
    if (px < 0 || f <= 0.0)
        return 0;
    return (int)((double)px * f + 0.5);
}

int ri_pitch_advance(int step, int nsteps) {
    if (nsteps <= 0 || step < 0 || step >= nsteps)
        return 0;
    return (step + 1) % nsteps;
}

int ri_accent_cycle(int level) {
    if (level == 0)
        return 1;
    if (level == 1)
        return 2;
    return 0;
}

int ri_flam_glow(int flam_steps) {
    return flam_steps > 0 ? 1 : 0;
}
