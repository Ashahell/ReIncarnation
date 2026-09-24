/* gui/knob_logic.h — pure drag→value mapping (Task 12, gate G12; P-18).
 *
 * Host-tested via tests/unit/t1_knob.c. No AROS/MUI includes: this TU
 * compiles on host AND AROS; the thin MCC shells in gui/widgets/ call
 * into it, so the owned-UX numbers live in ONE place.
 *
 * Spec §13 M2.1 locked values (E0 design decisions owned by this
 * project): knob 150 px = full 0..127 on EITHER axis (vertical kept
 * for ReBirth parity + horizontal added per owner 2026-09-24),
 * Shift-fine ×0.1 (1500 px full); fader 100 px = full travel with the same fine rule;
 * continuous notify while dragging + single commit event on release =
 * one undo unit; step-LED update ≤ 1 frame (33 ms); 16th chase
 * stutter-free at 174 BPM; zoom 1x/1.5x/2x authored at 2x.
 */
#ifndef RI_KNOB_LOGIC_H
#define RI_KNOB_LOGIC_H

/* Owned-UX numbers (P-18, TC-2.9.2 design gate). */
#define RI_KNOB_TRAVEL_PX 150.0
#define RI_FADER_TRAVEL_PX 100.0
#define RI_CTL_MAX 127.0
#define RI_FINE_DIV 10.0
#define RI_LED_FRAME_MS 33.0

/* Knob drag math, both axes (owner amendment 2026-09-24): start +
 * (dx+dy)·(127/travel)·(fine ? 0.1 : 1), clamped to 0..127. dy>0 =
 * upward drag = increase (callers negate screen-y deltas, where down
 * is positive, before calling); dx>0 = rightward drag = increase
 * (screen-x passes through). ReBirth vertical preserved; horizontal
 * added. Diagonal 45° counts double (documented). */
double ri_knob_drag_to_value(double start, double dx_px, double dy_px,
    int fine);

/* Clamp 2D accumulated drag travel so the mapped value stays in
 * [0,127]: when dx+dy exceeds the travel that maps start to an end
 * (×10 span in fine), scale both components toward zero. Reversal
 * then bites at once instead of unwinding dead overshoot (the grab
 * pins the pointer, so overshoot is unbounded without this).
 * No-op when already inside or eff == 0. */
void ri_knob_clamp_acc(double start, double *adx, double *ady, int fine);
double ri_fader_drag_to_value(double start, double dy_px, int fine);

/* Clamp to 0..127 (NaN fails closed to 0); quantize to nearest unit. */
double ri_ctl_clamp(double v);
int ri_ctl_quantize(double v);

/* Commit-on-release as pure event counting: moves notify, release
 * commits exactly one undo unit no matter how many moves happened. */
struct RiGesture {
    int begun;
    int moves;
    int commits;
};
/* Pointer angle for the custom knob renderer (TC-2.9.2 dial):
 * value 0..127 -> -135000..+135000 millidegrees (270-degree sweep,
 * 0 = straight up), round-half-up, clamped. Exact integer contract
 * (no fp, no trig — endpoint rendering is the draw routine's job). */
int ri_knob_pointer_mdeg(int value);

void ri_gesture_begin(struct RiGesture *g);
void ri_gesture_move(struct RiGesture *g);
void ri_gesture_end(struct RiGesture *g);

/* Step button: click toggles (nonzero → 0, zero → 1). */
int ri_step_toggle(int state);

/* LED chase: ms per 16th at bpm (bpm <= 0 fails closed to 0); lag_ok is
 * nonzero iff the LED lags pattern position by at most one frame. */
double ri_step16_ms(double bpm);
int ri_led_lag_ok(double lag_ms);

/* Chase position: 16th-grid index for a beat position, wrapped to
 * [0, nsteps). Fail-closed (0) on nsteps <= 0 or negative position. */
int ri_chase_step(double beat_pos_16ths, int nsteps);

/* Zoom levels 0/1/2 → 1x/1.5x/2x; scaled pixel size (rounded).
 * Unknown level fails closed (factor 0.0 / size 0). */
double ri_zoom_factor(int level);
int ri_zoom_scaled_px(int px, int level);

/* E1 programming behaviors (RB-338 contemporary reviews, spec §13):
 * 303 pitch-mode entry auto-advances one step (wrapped); 909 clicks
 * cycle accent none → L1 (dim) → L2 (bright) → none; flam on a step
 * is the green-glow flag source. */
int ri_pitch_advance(int step, int nsteps);
int ri_accent_cycle(int level);
int ri_flam_glow(int flam_steps);

#endif
