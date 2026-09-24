/* t36_303_ctls.c — 303 control-ID contract (§2.2 fix proof).
 * (a) 0x0305 IS wave (panel's old "volume" label was the lie): setting it
 *     must flip the waveform and leave level alone; 0x0306 carries volume.
 * (b) 303B (0x031x) shares the implementation: every 303B ID behaves exactly
 *     like its 303A twin on a twin voice.
 * (c) Tune (0x0307/0x0317): center 64 = concert pitch, ±12 st = octave,
 *     clamped ±24 st, bends live notes by the new/old ratio.
 * (d) Panel/engine one-table contract: every panel row ID is engine-handled,
 *     and every engine 303A/B ID is reachable from its panel.
 * RED-first: 303B unhandled, no tune, panel missing 0x0306/0x0307/0x0316/0x0317.
 */
#include <stdio.h>
#include <stdint.h>
#include <math.h>
#include "tests/helpers/ri_assert.h"
#include "engine/dsp/rb303.h"
#include "gui/panels.h"

static void twin(struct RB303Voice *a, struct RB303Voice *b) {
    rb303_init(a);
    rb303_init(b);
}

int main(void) {
    struct RB303Voice v, w;
    /* (a) wave vs volume. */
    twin(&v, &w);
    rb303_set_param(&v, RI_CTL_303A_WAVE, 100);
    RI_ASSERT(v.wave_square == 1, "wave 100 -> square");
    RI_ASSERT(v.volume == 1.0f, "wave must not move level");
    rb303_set_param(&v, RI_CTL_303A_WAVE, 0);
    RI_ASSERT(v.wave_square == 0, "wave 0 -> saw");
    rb303_set_param(&v, RI_CTL_303A_VOLUME, 100);
    RI_ASSERT(fabsf(v.volume - 100.0f / 127.0f) < 0.02f, "volume law: %f", v.volume);
    RI_ASSERT(v.wave_square == 0, "volume must not move wave");
    /* (b) 303B twin behavior. */
    rb303_set_param(&v, RI_CTL_303A_CUTOFF, 127);
    rb303_set_param(&w, RI_CTL_303B_CUTOFF, 127);
    RI_ASSERT(v.cutoff_hz == w.cutoff_hz, "303B cutoff != twin %f %f", v.cutoff_hz, w.cutoff_hz);
    rb303_set_param(&v, RI_CTL_303A_ACCENT, 96);
    rb303_set_param(&w, RI_CTL_303B_ACCENT, 96);
    RI_ASSERT(v.accent_amt == w.accent_amt, "303B accent != twin");
    rb303_set_param(&w, RI_CTL_303B_WAVE, 127);
    RI_ASSERT(w.wave_square == 1, "303B wave ignored");
    rb303_set_param(&w, RI_CTL_303B_VOLUME, 100);
    RI_ASSERT(fabsf(w.volume - 100.0f / 127.0f) < 0.02f, "303B volume ignored: %f", w.volume);
    /* (c) tune. */
    rb303_init(&v);
    rb303_set_param(&v, RI_CTL_303A_TUNE, 64);
    rb303_note(&v, 69, 0, 0);
    RI_ASSERT(v.target_freq == 440.0f, "tune center != 440: %f", v.target_freq);
    rb303_init(&v);
    rb303_set_param(&v, RI_CTL_303A_TUNE, 76);
    rb303_note(&v, 69, 0, 0);
    RI_ASSERT(v.target_freq == 880.0f, "tune +12 != octave: %f", v.target_freq);
    rb303_init(&v);
    rb303_set_param(&v, RI_CTL_303A_TUNE, 127);
    rb303_note(&v, 69, 0, 0);
    RI_ASSERT(v.target_freq == 1760.0f, "tune clamp != +24st: %f", v.target_freq);
    rb303_init(&v);
    rb303_set_param(&v, RI_CTL_303A_TUNE, 0);
    rb303_note(&v, 69, 0, 0);
    RI_ASSERT(v.target_freq == 110.0f, "tune clamp != -24st: %f", v.target_freq);
    /* live bend: sounding note follows the new/old ratio exactly. */
    rb303_init(&v);
    rb303_set_param(&v, RI_CTL_303A_TUNE, 64);
    rb303_note(&v, 69, 0, 0);
    rb303_set_param(&v, RI_CTL_303A_TUNE, 76);
    RI_ASSERT(v.target_freq == 880.0f && v.freq == 880.0f,
        "live bend != octave: %f %f", v.target_freq, v.freq);
    rb303_init(&v);
    rb303_set_param(&v, RI_CTL_303B_TUNE, 76);
    rb303_note(&v, 69, 0, 0);
    RI_ASSERT(v.target_freq == 880.0f, "303B tune ignored: %f", v.target_freq);
    /* (d) panel/engine contract. */
    {
        const struct RIPanelDesc *pa = ri_panel_get(0);
        const struct RIPanelDesc *pb = ri_panel_get(1);
        unsigned int id;
        RI_ASSERT(pa && pa->device == 0u, "303A slot");
        RI_ASSERT(pb && pb->device == 1u, "303B slot");
        for (id = 0x0300u; id <= 0x0307u; id++)
            RI_ASSERT(ri_panel_find_ctl(pa, id) != 0, "303A panel lacks %04x", id);
        for (id = 0x0310u; id <= 0x0317u; id++)
            RI_ASSERT(ri_panel_find_ctl(pb, id) != 0, "303B panel lacks %04x", id);
        /* every panel row is engine-handled (no drift back to ignored IDs). */
        {
            unsigned int i;
            for (i = 0; i < pa->nctls; i++)
                RI_ASSERT(pa->ctls[i].ctl_id < 0x0308u, "303A row outside engine map: %04x",
                    pa->ctls[i].ctl_id);
            for (i = 0; i < pb->nctls; i++)
                RI_ASSERT(pb->ctls[i].ctl_id >= 0x0310u && pb->ctls[i].ctl_id < 0x0318u,
                    "303B row outside engine map: %04x", pb->ctls[i].ctl_id);
        }
        /* defaults: waveform saw, volume full-ish, tune center. */
        RI_ASSERT(ri_panel_default_ctl(pa, 0x0305u) == 0, "wave default");
        RI_ASSERT(ri_panel_default_ctl(pa, 0x0306u) == 100, "volume default");
        RI_ASSERT(ri_panel_default_ctl(pa, 0x0307u) == 64, "tune default");
        RI_ASSERT(ri_panel_default_ctl(pb, 0x0315u) == 0, "303B wave default");
        RI_ASSERT(ri_panel_default_ctl(pb, 0x0316u) == 100, "303B volume default");
        RI_ASSERT(ri_panel_default_ctl(pb, 0x0317u) == 64, "303B tune default");
    }
    RI_RESULT("303_ctls");
}
