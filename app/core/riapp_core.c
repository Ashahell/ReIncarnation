/* riapp_core.c — portable application core bodies (portability plan T8).
 * Moved from app/riapp.c (demo song, bank table, transport, meters);
 * the MUI/AHI shell keeps its shadows and backend choice.
 */
#include "app/core/riapp_core.h"

#include "engine/engine.h"
#include "gui/livestate.h"

void ri_core_init(struct RIAppCore *c, uint32_t ppq, float sr, float bpm,
    uint32_t engine) {
    if (!c)
        return;
    ri_ctl_init(&c->ctl);
    ri_track_init(&c->track);
    ri_live_init(&c->session, ppq, sr, bpm, engine, c->scratch, RI_CORE_SCRATCH);
    {
        const struct RIPatternBank *b4[4];
        b4[0] = &c->banks[0];
        b4[1] = &c->banks[1];
        b4[2] = &c->banks[2];
        b4[3] = &c->banks[3];
        ri_live_set_banks(&c->session, b4, &c->track, 0);
    }
    ri_live_set_ctl(&c->session, &c->ctl);
    ri_engine_set_delay(&c->session.eng, c->dline, RI_CORE_DLINE);
}

void ri_core_demo(struct RIAppCore *c) {
    /* Demo line (owner 2026-09-27: Zombie Nation drive v2): B-minor hook
     * reconstructed from published attributes (Hooktheory: B minor, 140,
     * range B3-G4, repeaty ~quarter-note rhythm, i-iv-v, full-bar rest) —
     * NOT transcribed note-for-note; tonic-centered degrees, beat-4 rest
     * as the miniature of the hook's breath. Owner ear decides. */
    static const uint8_t keys[16] = { 0, 0, 3, 0, 5, 3, 0, 3, 0, 0, 3, 0, 6, 5, 3, 0 };
    static const uint8_t fl[16] = { RI_STEP_ACCENT, 0, 0, 0, RI_STEP_ACCENT, 0, 0, 0,
        RI_STEP_ACCENT, 0, 0, 0, 0, 0, 0, RI_STEP_REST };
    uint32_t i;
    struct RIPatternBank *ba, *bb, *b808, *b909;
    if (!c)
        return;
    ba = &c->banks[0];
    bb = &c->banks[1];
    b808 = &c->banks[2];
    b909 = &c->banks[3];
    ri_bank_init(ba, 0u, RI_PATTERN_KIND_303, 0u);
    ri_bank_init(bb, 1u, RI_PATTERN_KIND_303, 0u);
    ri_bank_init(b808, 2u, RI_PATTERN_KIND_DRUM, RI_DRUM_CLASS_808);
    ri_bank_init(b909, 3u, RI_PATTERN_KIND_DRUM, RI_DRUM_CLASS_909);
    for (i = 0u; i < 32u; i++) {
        ri_pattern_set_length(&ba->pat[i], 16u);
        ri_pattern_set_length(&bb->pat[i], 16u);
        ri_pattern_set_length(&b808->pat[i], 16u);
        ri_pattern_set_length(&b909->pat[i], 16u);
    }
    for (i = 0u; i < 16u; i++) {
        ri_p303_set(&ba->pat[0], i, keys[i], fl[i]);
        if ((i & 3u) == 0u)
            ri_pdrum_set(&b808->pat[0], i, RI_L808_BD, RI_HIT_LOW);
        if ((i & 3u) == 2u)
            ri_pdrum_set(&b808->pat[0], i, RI_L808_CH, RI_HIT_LOW);
        if (i == 4u || i == 12u)
            ri_pdrum_set(&b808->pat[0], i, RI_L808_SD, RI_HIT_LOW);
        if (i == 0u || i == 8u)
            ri_pdrum_set_ac(&b808->pat[0], i, 1); /* accented downbeats */
    }
    /* 909 showcase (owner 2026-09-27: timbre verdicts need demo content).
     * Supports, never fights, the 808 floor: crash on the one, rim+clap
     * doubling the backbeat, descending toms, open-hat sparkle. */
    ri_pdrum_set(&b909->pat[0], 0u, RI_L909_CC, RI_HIT_HIGH);
    ri_pdrum_set_ac(&b909->pat[0], 0u, 1);
    ri_pdrum_set(&b909->pat[0], 4u, RI_L909_RS, RI_HIT_LOW);
    ri_pdrum_set(&b909->pat[0], 4u, RI_L909_CP, RI_HIT_LOW);
    ri_pdrum_set(&b909->pat[0], 6u, RI_L909_LT, RI_HIT_LOW);
    ri_pdrum_set(&b909->pat[0], 7u, RI_L909_OH, RI_HIT_LOW);
    ri_pdrum_set(&b909->pat[0], 10u, RI_L909_MT, RI_HIT_LOW);
    ri_pdrum_set(&b909->pat[0], 12u, RI_L909_RS, RI_HIT_LOW);
    ri_pdrum_set(&b909->pat[0], 12u, RI_L909_CP, RI_HIT_LOW);
    ri_pdrum_set(&b909->pat[0], 14u, RI_L909_HT, RI_HIT_LOW);
    ri_pdrum_set(&b909->pat[0], 15u, RI_L909_OH, RI_HIT_LOW);
    ri_track_init(&c->track);
}

struct RIPatternBank *ri_core_bank(struct RIAppCore *c, uint32_t inst) {
    if (!c)
        return 0;
    return &c->banks[inst < 4u ? inst : 0u];
}

const struct RIPatternBank *ri_core_bank_ro(const struct RIAppCore *c, uint32_t inst) {
    if (!c)
        return 0;
    return &c->banks[inst < 4u ? inst : 0u];
}

void ri_core_play(struct RIAppCore *c) {
    if (c)
        ri_live_play(&c->session);
}

int ri_core_capture_sel(struct RIAppCore *c, uint64_t bar, uint32_t inst, uint8_t sel) {
    if (!c || inst >= 4u || sel > 31u)
        return 0;
    if (bar >= (uint64_t)RI_SONG_BARS)
        bar = (uint64_t)RI_SONG_BARS - 1u;
    if (ri_track_selected(&c->track, bar, inst) == sel)
        return 0;
    ri_track_capture(&c->track, bar, inst, sel);
    return 1;
}

void ri_core_stop(struct RIAppCore *c) {
    if (c)
        ri_live_stop(&c->session);
}

int ri_core_meters(struct RIAppCore *c, int *lvl303, int *lvl808,
    uint64_t *sixteenths, uint32_t mix_freq, int playing) {
    struct RILiveMeters m;
    if (!c || !lvl303 || !lvl808 || !sixteenths)
        return 0;
    if (ri_live_meters_read(&c->session, &m) != 0)
        return 0;
    *lvl303 = ri_live_meter_level(m.sec_peak[0]);
    *lvl808 = ri_live_meter_level(m.sec_peak[2]);
    *sixteenths = ri_live_16ths(m.samples, 120u, mix_freq ? mix_freq : 48000u);
    (void)playing;
    return 1;
}
