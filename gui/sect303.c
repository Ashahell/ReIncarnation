/* gui/sect303.c — 303 section front-panel behaviour (§12.10 G3/G4).
 * Rules (ReBirth 2.0.1 Owner's Manual):
 * - Step: next step; at the last step of the pattern -> first (p. 41, 154).
 * - Back: previous step; at the first step -> last step of the pattern.
 * - Pitch key: sets the pitch of the edit step only; in Pitch Mode the
 *   edit step then advances (p. 42). Note/Pause is untouched.
 * - Pitch Mode: toggles; turning it off and immediately on again returns
 *   to step one (p. 40, 153).
 * - Down / Up / Accent / Slide / Note-Pause: toggle on the edit step;
 *   Up and Down may both be set (= neither, p. 154).
 * - Clear: whole pattern to Pause, low C, no flags (p. 52, 153).
 */
#include "gui/sect303.h"
#include "gui/ctlreg.h"
#include "engine/seq/sched.h"

static const struct RICtlDef *def(const struct RISect303 *s, uint32_t idx) {
    return ri_ctlreg_find((uint16_t)((s->section << 8) | idx));
}

int ri_s303_init(struct RISect303 *s, uint8_t section) {
    uint32_t i;
    if (!s || (section != RI_SEC_SYNTH1 && section != RI_SEC_SYNTH2))
        return 2;
    s->section = section;
    s->edit_step = 0;
    s->pitch_mode = 0;
    s->pm_rearm = 0;
    for (i = 0; i < RI_S303_NCTL; i++) {
        const struct RICtlDef *d = def(s, i);
        s->val[i] = d ? d->def_v : 0;
    }
    ri_pattern_init(&s->pat, RI_PATTERN_KIND_303, 0);
    return 0;
}

static struct RI303Row *row(struct RISect303 *s) {
    return &s->pat.row.r303[s->edit_step % RI_PATTERN_STEPS];
}

static void step_by(struct RISect303 *s, int dir) {
    uint32_t len = s->pat.length ? s->pat.length : 16u;
    uint32_t e = s->edit_step % len;
    if (dir > 0)
        e = (e + 1u == len) ? 0u : e + 1u;
    else
        e = e == 0u ? len - 1u : e - 1u;
    s->edit_step = (uint8_t)e;
}

static int toggle_flag(struct RISect303 *s, uint8_t bit) {
    row(s)->flags ^= bit;
    return 1;
}

int ri_s303_press(struct RISect303 *s, uint32_t idx) {
    int rearm = 0, rc = 0;
    if (!s || idx >= RI_S303_NCTL)
        return 0;
    if (idx >= RI_S303_KEY0 && idx < RI_S303_KEY0 + RI_303_KEYS) {
        row(s)->key = (uint8_t)(idx - RI_S303_KEY0);
        if (s->pitch_mode)
            step_by(s, +1);
        rc = 1;
    } else {
        switch (idx) {
        case RI_S303_WAVE:
            s->val[idx] = (int16_t)(s->val[idx] ? 0 : 1);
            rc = 1;
            break;
        case RI_S303_DOWN: rc = toggle_flag(s, RI_STEP_DOWN); break;
        case RI_S303_UP: rc = toggle_flag(s, RI_STEP_UP); break;
        case RI_S303_ACCENT: rc = toggle_flag(s, RI_STEP_ACCENT); break;
        case RI_S303_SLIDE: rc = toggle_flag(s, RI_STEP_SLIDE); break;
        case RI_S303_NOTEPAUSE: rc = toggle_flag(s, RI_STEP_REST); break;
        case RI_S303_STEP: step_by(s, +1); rc = 1; break;
        case RI_S303_BACK: step_by(s, -1); rc = 1; break;
        case RI_S303_CLEAR: ri_pattern_clear(&s->pat); rc = 1; break;
        case RI_S303_PITCHMODE:
            if (s->pitch_mode) {
                s->pitch_mode = 0;
                rearm = 1; /* a second click right away returns to step 1 */
            } else {
                s->pitch_mode = 1;
                if (s->pm_rearm)
                    s->edit_step = 0;
            }
            rc = 1;
            break;
        default:
            return 0; /* knobs and the display are not buttons */
        }
    }
    s->pm_rearm = (uint8_t)rearm;
    return rc;
}

int ri_s303_set_value(struct RISect303 *s, uint32_t idx, int v) {
    const struct RICtlDef *d;
    if (!s || idx >= RI_S303_NCTL)
        return 0;
    d = def(s, idx);
    if (!d || (d->kind != RI_CK_KNOB && d->kind != RI_CK_SWITCH))
        return 0;
    if (v < d->min_v)
        v = d->min_v;
    if (v > d->max_v)
        v = d->max_v;
    if (s->val[idx] == v)
        return 0;
    s->val[idx] = (int16_t)v;
    s->pm_rearm = 0;
    return 1;
}

int ri_s303_reset(struct RISect303 *s, uint32_t idx) {
    const struct RICtlDef *d = s && idx < RI_S303_NCTL ? def(s, idx) : 0;
    return d ? ri_s303_set_value(s, idx, d->def_v) : 0;
}

int ri_s303_led(const struct RISect303 *s, uint32_t idx, uint32_t which) {
    const struct RI303Row *r;
    if (!s || idx >= RI_S303_NCTL)
        return 0;
    r = &s->pat.row.r303[s->edit_step % RI_PATTERN_STEPS];
    if (idx >= RI_S303_KEY0 && idx < RI_S303_KEY0 + RI_303_KEYS)
        return r->key == idx - RI_S303_KEY0;
    switch (idx) {
    case RI_S303_DOWN: return (r->flags & RI_STEP_DOWN) != 0;
    case RI_S303_UP: return (r->flags & RI_STEP_UP) != 0;
    case RI_S303_ACCENT: return (r->flags & RI_STEP_ACCENT) != 0;
    case RI_S303_SLIDE: return (r->flags & RI_STEP_SLIDE) != 0;
    case RI_S303_NOTEPAUSE:
        return which ? (r->flags & RI_STEP_REST) != 0 : (r->flags & RI_STEP_REST) == 0;
    case RI_S303_PITCHMODE: return s->pitch_mode != 0;
    default: return 0;
    }
}

int ri_s303_display(const struct RISect303 *s) {
    return s ? (int)(s->edit_step % RI_PATTERN_STEPS) + 1 : 0;
}
