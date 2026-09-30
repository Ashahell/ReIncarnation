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
        const struct RIPatternBank *b4[5];
        b4[0] = &c->banks[0];
        b4[1] = &c->banks[1];
        b4[2] = &c->banks[2];
        b4[3] = &c->banks[3];
        b4[4] = &c->banks[4];
        ri_live_set_banks(&c->session, b4, &c->track, 0);
    }
    ri_live_set_ctl(&c->session, &c->ctl);
    ri_engine_set_delay(&c->session.eng, c->dline, RI_CORE_DLINE);
    ri_auto_pub_init(&c->pub, c->autoev[0], RI_CORE_AUTO_CAP, c->autoev[1], RI_CORE_AUTO_CAP);
    c->carry.next = 0u;
    c->pass.npunched = c->pass.ntouched = 0u;
    ri_live_set_auto(&c->session, &c->pub, &c->carry, &c->pass);
    c->song_bars = 0u;
    c->song_sections = engine;
    c->song_bpm = bpm;
}

int ri_core_pattern_silent(const struct RIPatternBank *b, uint32_t slot) {
    const struct RIPattern *p;
    uint32_t i;
    if (!b || slot >= RI_PATTERN_BANK_PATTERNS)
        return 1;
    p = &b->pat[slot];
    for (i = 0u; i < p->length && i < RI_PATTERN_STEPS; i++) {
        if (p->kind == RI_PATTERN_KIND_303 && !(p->row.r303[i].flags & RI_STEP_REST))
            return 0;
        if (p->kind == RI_PATTERN_KIND_DRUM && p->row.drum[i].on)
            return 0;
        if (p->kind == RI_PATTERN_KIND_LEVI && p->row.levi[i].on)
            return 0;
    }
    return 1;
}

int ri_core_load_song(struct RIAppCore *c, const struct RICoreSong *s) {
    static const uint8_t KIND[RI_SONGTRACK_INSTANCES] = { RI_PATTERN_KIND_303, RI_PATTERN_KIND_303,
        RI_PATTERN_KIND_DRUM, RI_PATTERN_KIND_DRUM, RI_PATTERN_KIND_LEVI };
    static const uint8_t DCLS[RI_SONGTRACK_INSTANCES] = { 0u, 0u, RI_DRUM_CLASS_808, RI_DRUM_CLASS_909, 0u };
    static uint32_t tk[RI_CORE_AUTO_CAP];
    uint32_t i, bar, rc = 0u;
    if (!c || !s || !s->track || s->nauto > RI_CORE_AUTO_CAP || (s->nauto && (!s->auto_tick || !s->auto_ctl ||
        !s->auto_val)))
        return 2;
    for (i = 0u; i < RI_SONGTRACK_INSTANCES; i++) {
        if (s->bank[i])
            c->banks[i] = *s->bank[i];
        else
            ri_bank_init(&c->banks[i], (uint8_t)i, KIND[i], DCLS[i]);
        c->banks[i].instance = (uint8_t)i;
    }
    c->track = *s->track;
    if (s->bpm >= 30.0f && s->bpm <= 300.0f) {
        ri_live_set_bpm(&c->session, s->bpm);
        c->song_bpm = s->bpm;
    }
    /* Automation: song-ppq ticks onto the session ppq, then publish. */
    for (i = 0u; i < s->nauto; i++)
        tk[i] = s->ppq && s->ppq != c->session.ppq
            ? (uint32_t)(((uint64_t)s->auto_tick[i] * c->session.ppq + s->ppq / 2u) / s->ppq) : s->auto_tick[i];
    if (ri_auto_load_triples(ri_auto_pub_back(&c->pub), tk, s->auto_ctl, s->auto_val, s->nauto) != 0) {
        (void)ri_auto_load_triples(ri_auto_pub_back(&c->pub), tk, s->auto_ctl, s->auto_val, 0u);
        rc = 2u;
    }
    ri_auto_pub_request(&c->pub);
    ri_auto_pub_apply(&c->pub);
    ri_auto_pub_resync(&c->pub);
    ri_auto_carry_reindex(&c->carry, ri_auto_pub_front(&c->pub), 0u);
    ri_live_set_auto(&c->session, &c->pub, &c->carry, &c->pass);
    /* (the player re-arms on the new track at play: ri_live_play) */
    /* Length and devices: through the last bar anything sounds. */
    c->song_bars = 0u;
    c->song_sections = 0u;
    for (bar = 0u; bar < RI_SONGTRACK_BARS; bar++)
        for (i = 0u; i < RI_SONGTRACK_INSTANCES; i++)
            if (!ri_core_pattern_silent(&c->banks[i], ri_track_selected(&c->track, bar, i))) {
                c->song_bars = bar + 1u;
                c->song_sections |= (uint32_t)RI_ENGINE_S303A << i;
            }
    return (int)rc;
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
    struct RIPatternBank *ba, *bb, *b808, *b909, *blevi;
    if (!c)
        return;
    ba = &c->banks[0];
    bb = &c->banks[1];
    b808 = &c->banks[2];
    b909 = &c->banks[3];
    blevi = &c->banks[4];
    ri_bank_init(ba, 0u, RI_PATTERN_KIND_303, 0u);
    ri_bank_init(bb, 1u, RI_PATTERN_KIND_303, 0u);
    ri_bank_init(b808, 2u, RI_PATTERN_KIND_DRUM, RI_DRUM_CLASS_808);
    ri_bank_init(b909, 3u, RI_PATTERN_KIND_DRUM, RI_DRUM_CLASS_909);
    ri_bank_init(blevi, 4u, RI_PATTERN_KIND_LEVI, 0u);
    /* Levi demo part (owner 2026-09-28: full part). Bm-G-D-A quarter
     * triads under the Zombie drive; 3-note voicings, lanes 3-5 rest.
     * Supports the 303A line, never fights the drums. */
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
     * doubling the backbeat, descending toms, open-hat sparkle.
     * Accented (the 808 hits are accented; unaccented 909 sat thin). */
    ri_pdrum_set(&b909->pat[0], 0u, RI_L909_CC, RI_HIT_HIGH);
    ri_pdrum_set_ac(&b909->pat[0], 0u, 1);
    ri_pdrum_set(&b909->pat[0], 4u, RI_L909_RS, RI_HIT_HIGH);
    ri_pdrum_set(&b909->pat[0], 4u, RI_L909_CP, RI_HIT_HIGH);
    ri_pdrum_set_ac(&b909->pat[0], 4u, 1);
    ri_pdrum_set(&b909->pat[0], 6u, RI_L909_LT, RI_HIT_LOW);
    ri_pdrum_set(&b909->pat[0], 7u, RI_L909_OH, RI_HIT_LOW);
    ri_pdrum_set(&b909->pat[0], 10u, RI_L909_MT, RI_HIT_LOW);
    ri_pdrum_set(&b909->pat[0], 12u, RI_L909_RS, RI_HIT_HIGH);
    ri_pdrum_set(&b909->pat[0], 12u, RI_L909_CP, RI_HIT_HIGH);
    ri_pdrum_set_ac(&b909->pat[0], 12u, 1);
    ri_pdrum_set(&b909->pat[0], 14u, RI_L909_HT, RI_HIT_LOW);
    ri_pdrum_set(&b909->pat[0], 15u, RI_L909_OH, RI_HIT_LOW);
    /* Levi demo part (owner 2026-09-28: full part). Bm-G-D-A quarter
     * triads under the Zombie drive; 3-note voicings, lanes 3-5 rest.
     * Supports the 303A line, never fights the drums. */
    ri_levi_set(&blevi->pat[0], 0u, 0u, 59u, 1);
    ri_levi_set(&blevi->pat[0], 0u, 1u, 62u, 1);
    ri_levi_set(&blevi->pat[0], 0u, 2u, 66u, 1);
    ri_levi_set(&blevi->pat[0], 4u, 0u, 55u, 1);
    ri_levi_set(&blevi->pat[0], 4u, 1u, 59u, 1);
    ri_levi_set(&blevi->pat[0], 4u, 2u, 62u, 1);
    ri_levi_set(&blevi->pat[0], 8u, 0u, 62u, 1);
    ri_levi_set(&blevi->pat[0], 8u, 1u, 66u, 1);
    ri_levi_set(&blevi->pat[0], 8u, 2u, 69u, 1);
    ri_levi_set(&blevi->pat[0], 12u, 0u, 57u, 1);
    ri_levi_set(&blevi->pat[0], 12u, 1u, 61u, 1);
    ri_levi_set(&blevi->pat[0], 12u, 2u, 64u, 1);
    ri_track_init(&c->track);
}

struct RIPatternBank *ri_core_bank(struct RIAppCore *c, uint32_t inst) {
    if (!c)
        return 0;
    return &c->banks[inst < 5u ? inst : 0u];
}

const struct RIPatternBank *ri_core_bank_ro(const struct RIAppCore *c, uint32_t inst) {
    if (!c)
        return 0;
    return &c->banks[inst < 5u ? inst : 0u];
}

void ri_core_play(struct RIAppCore *c) {
    if (c)
        ri_live_play(&c->session);
}

int ri_core_capture_sel(struct RIAppCore *c, uint64_t bar, uint32_t inst, uint8_t sel) {
    if (!c || inst >= 5u || sel > 31u)
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
