/* t39_meg_veg.c — dual-envelope 303 voice (§12.4b fix proof).
 * MEG (filter) follows the Decay knob; VEG (amp) is fixed and long; accent
 * forces minimum MEG decay; the accent sweep builds up across consecutive
 * accents; releases are 0.5 ms plain / 50 ms accented (E1 lineage values).
 * White-box state asserts (RB303Voice fields are public, t36 precedent)
 * plus output-level guards. RED-first: single env, no sweep, 5 ms release.
 */
#include <stdio.h>
#include <stdint.h>
#include <math.h>
#include "tests/helpers/ri_assert.h"
#include "engine/dsp/rb303.h"

#define SR 48000.0f

static void quiet_voice(struct RB303Voice *v) {
    rb303_init(v);
    rb303_set_param(v, RI_CTL_303A_CUTOFF, 80);
    rb303_set_param(v, RI_CTL_303A_RESO, 40);
    rb303_set_param(v, RI_CTL_303A_VOLUME, 127);
    rb303_set_param(v, RI_CTL_303A_WAVE, 0);
}

static void render_hold(struct RB303Voice *v, float seconds) {
    static float buf[4800];
    uint32_t total = (uint32_t)(seconds * SR + 0.5f);
    while (total > 0u) {
        uint32_t cc = total > 4800u ? 4800u : total;
        rb303_render(v, buf, cc, SR);
        total -= cc;
    }
}

int main(void) {
    struct RB303Voice v;
    /* (a) decay knob moves MEG, not VEG: 1 s gate-held, min vs max decay. */
    quiet_voice(&v);
    rb303_set_param(&v, RI_CTL_303A_DECAY, 0);
    rb303_note(&v, 69, 0, 0);
    render_hold(&v, 1.0f);
    {
        float meg_short = v.meg, veg_short = v.veg;
        quiet_voice(&v);
        rb303_set_param(&v, RI_CTL_303A_DECAY, 127);
        rb303_note(&v, 69, 0, 0);
        render_hold(&v, 1.0f);
        RI_ASSERT(fabsf(v.veg - veg_short) < 0.02f, "VEG follows decay knob: %f vs %f",
            v.veg, veg_short);
        RI_ASSERT(meg_short < 0.01f, "short MEG alive: %f", meg_short);
        RI_ASSERT(v.meg > 0.5f, "long MEG died: %f", v.meg);
    }
    /* (b) accent forces minimum MEG decay (decay knob at max). */
    quiet_voice(&v);
    rb303_set_param(&v, RI_CTL_303A_DECAY, 127);
    rb303_note(&v, 69, 0, 1);
    render_hold(&v, 0.5f);
    RI_ASSERT(v.meg < 0.15f, "accent MEG not minimum: %f", v.meg);
    quiet_voice(&v);
    rb303_set_param(&v, RI_CTL_303A_DECAY, 127);
    rb303_note(&v, 69, 0, 0);
    render_hold(&v, 0.5f);
    RI_ASSERT(v.meg > 0.8f, "plain MEG decayed: %f", v.meg);
    /* (c) sweep builds up across consecutive accents. */
    quiet_voice(&v);
    rb303_note(&v, 69, 0, 1);
    render_hold(&v, 0.05f);
    {
        float s1 = v.sweep;
        rb303_accent(&v);
        render_hold(&v, 0.005f);
        RI_ASSERT(v.sweep > s1, "no sweep buildup: %f -> %f", s1, v.sweep);
    }
    /* (d) releases: plain ~0.5 ms, accented ~50 ms (VEG tails). */
    quiet_voice(&v);
    rb303_note(&v, 69, 0, 0);
    render_hold(&v, 0.2f);
    rb303_release(&v);
    render_hold(&v, 0.01f);
    RI_ASSERT(v.veg < 1e-3f, "plain release not fast: %f", v.veg);
    quiet_voice(&v);
    rb303_note(&v, 69, 0, 1);
    render_hold(&v, 0.05f);
    rb303_release(&v);
    render_hold(&v, 0.01f);
    RI_ASSERT(v.veg > 0.5f, "accent release not slow: %f", v.veg);
    /* (e) output guards: notes sound, accents stay louder (t22 pins ratio). */
    {
        static float buf[4800];
        float peak = 0.0f;
        uint32_t i;
        quiet_voice(&v);
        rb303_note(&v, 69, 0, 0);
        rb303_render(&v, buf, 4800u, SR);
        for (i = 0; i < 4800u; i++) {
            float a = buf[i] < 0.0f ? -buf[i] : buf[i];
            if (a > peak)
                peak = a;
        }
        RI_ASSERT(peak > 0.01f, "voice silent: %f", peak);
    }
    RI_RESULT("meg_veg");
}
