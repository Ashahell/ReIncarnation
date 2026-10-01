/* t146_levi_perfsig — Levi P9a performance signals: note + release
 * velocity, poly/mono aftertouch, mod wheel, pitch bend, and the matrix
 * sources they feed (27..30, 33, 34).
 * Laws: defaults (127/127, no bend) leave the sound alone; each signal
 * reaches its source with the documented map; bend normalises against the
 * P6b bend range (0 reads 0) and composes with the P6b semitone offset;
 * copies land per block, not per call; note entry points keep the
 * allocator contract, setters the setter contract; null-safe; bounded
 * extremes.
 */
#include <stdio.h>
#include <string.h>
#include "tests/helpers/ri_assert.h"
#include "engine/dsp/levi.h"
#include "engine/dsp/levi_matrix.h"
#include "gui/sectlevi.h"
#include "gui/ctlreg.h"

#define SR 48000.0f
#define BS 256u

static struct RILeviSet A, B;

static void run(struct RILeviSet *s, uint32_t nblocks) {
    static float l[BS], r[BS];
    uint32_t i;
    for (i = 0u; i < nblocks; i++)
        levi_voice_render_sum_stereo(s, l, r, BS, SR);
}

static int feq(float a, float b) {
    float d = a > b ? a - b : b - a;
    float m = a > b ? a : b;
    m = m < 0.0f ? -m : m;
    return d <= m * 1e-5f + 1e-7f && d < 1e30f;
}

/* Route source -> VCA level, depth 100 (a band a voice moves audibly:
 * the level destination scales the amp by 1 + x, and the D.FILTER cutoff
 * is barely audible in the default patch). */
static void route_vca(struct RILeviSet *s, uint32_t src) {
    levi_set_mx_ui(s, 0u, 0u, (uint8_t)ri_levi_ms_to_ui(src));
    levi_set_mx_ui(s, 0u, 1u, RI_LEVI_DM_VCA);
    levi_set_mx_ui(s, 0u, 2u, 0u);
    levi_set_mx_ui(s, 0u, 3u, 100u);
}

/* 1 when the two sets' next stereo blocks differ (same-age twins). */
static int audible_diff(struct RILeviSet *a, struct RILeviSet *b) {
    static float la[BS], lb[BS], oa[BS], ob[BS];
    uint32_t k;
    levi_voice_render_sum_stereo(a, la, lb, BS, SR);
    levi_voice_render_sum_stereo(b, oa, ob, BS, SR);
    for (k = 0u; k < BS; k++)
        if (la[k] != oa[k] || lb[k] != ob[k])
            return 1;
    return 0;
}

/* Peak of the next stereo block: monotone with the VCA level route, so it
 * pins a source law without the pitch wander a bend also causes. */
static float peak_of(struct RILeviSet *s) {
    static float l[BS], r[BS];
    uint32_t k;
    float p = 0.0f;
    levi_voice_render_sum_stereo(s, l, r, BS, SR);
    for (k = 0u; k < BS; k++) {
        float m = l[k] > 0.0f ? l[k] : -l[k];
        if (m > p)
            p = m;
        m = r[k] > 0.0f ? r[k] : -r[k];
        if (m > p)
            p = m;
    }
    return p;
}

int main(void) {
    /* Defaults: live velocity 127 (bit-identical to the old note path),
     * no pressure, no wheel, no bend. */
    levi_init_set(&A);
    RI_ASSERT(A.pvel == 127u && A.rvel == 127u, "pending velocity defaults %u/%u", A.pvel, A.rvel);
    RI_ASSERT(A.v[0].nvel == 127u && A.v[0].nveloff == 127u, "voice velocity defaults %u/%u",
        A.v[0].nvel, A.v[0].nveloff);
    RI_ASSERT(A.press == 0u && A.wheel == 0u && A.bend == 0.0f, "signal defaults");
    levi_note_on(&A, 60u);
    run(&A, 5u);
    RI_ASSERT(A.v[0].nvel == 127u, "live note velocity %u", A.v[0].nvel);
    RI_ASSERT(feq(A.v[0].vel01, 1.0f), "vel source default %f", A.v[0].vel01);
    RI_ASSERT(feq(A.v[0].mpat01, 0.0f) && feq(A.v[0].wheel01, 0.0f) && feq(A.v[0].bsrc, 0.0f),
        "silent sources");

    /* Note velocity rides the allocator (POLY mode) and clamps. */
    levi_init_set(&A);
    RI_ASSERT(levi_note_vel(&A, 60u, 64u) == 1, "note_vel fires");
    RI_ASSERT(A.v[0].nvel == 64u, "nvel %u", A.v[0].nvel);
    RI_ASSERT(A.v[0].nveloff == 64u, "release velocity starts at note velocity %u", A.v[0].nveloff);
    run(&A, 5u);
    RI_ASSERT(feq(A.v[0].vel01, 64.0f / 127.0f), "vel01 %f", A.v[0].vel01);
    levi_init_set(&A);
    RI_ASSERT(levi_note_vel(&A, 60u, 200u) == 1, "note_vel clamps");
    RI_ASSERT(A.v[0].nvel == 127u, "nvel clamped %u", A.v[0].nvel);

    /* Release velocity is stamped on the voices the note-off releases. */
    RI_ASSERT(levi_note_rel_vel(&A, 60u, 10u) == 1, "rel_vel");
    RI_ASSERT(A.v[0].nveloff == 10u, "nveloff %u", A.v[0].nveloff);
    run(&A, 5u);
    RI_ASSERT(feq(A.v[0].veloff01, 10.0f / 127.0f), "veloff01 %f", A.v[0].veloff01);
    RI_ASSERT(levi_note_rel_vel(&A, 60u, 200u) == 1, "rel_vel clamps");
    RI_ASSERT(A.v[0].nveloff == 127u, "nveloff clamped %u", A.v[0].nveloff);
    levi_init_set(&A);
    levi_note_on(&A, 60u);
    RI_ASSERT(levi_note_off(&A, 60u) == 1, "note_off");
    RI_ASSERT(A.v[0].nveloff == 127u, "plain note-off keeps 127 %u", A.v[0].nveloff);
    /* Mono mode never re-fires on release: the note-off stamps voice 0. */
    levi_init_set(&A);
    levi_set_param_ui(&A, 0u, RI_CTL_LEVI_POLYMODE & 0xFFu, (uint8_t)RI_LEVI_POLY_MONO);
    levi_note_vel(&A, 60u, 90u);
    RI_ASSERT(levi_note_rel_vel(&A, 60u, 5u) == 1, "mono rel_vel");
    RI_ASSERT(A.v[0].nveloff == 5u, "mono release stamped %u", A.v[0].nveloff);
    RI_ASSERT(A.v[0].nvel == 90u, "mono note velocity kept %u", A.v[0].nvel);
    RI_ASSERT(levi_note_on(&A, 72u) == 1, "mono next key");
    RI_ASSERT(A.v[0].nvel == 90u, "the pending note velocity survives a release %u", A.v[0].nvel);
    /* Unison mode releases every voice it fired, each stamped. */
    levi_init_set(&A);
    levi_set_param_ui(&A, 0u, RI_CTL_LEVI_POLYMODE & 0xFFu, (uint8_t)RI_LEVI_POLY_UNISON);
    levi_note_vel(&A, 60u, 90u);
    RI_ASSERT(levi_note_rel_vel(&A, 60u, 7u) == 1, "unison rel_vel");
    RI_ASSERT(A.v[0].nveloff == 7u && A.v[1].nveloff == 7u, "unison release stamped %u/%u",
        A.v[0].nveloff, A.v[1].nveloff);
    RI_ASSERT(A.v[0].nvel == 90u, "unison note velocity kept %u", A.v[0].nvel);

    /* Poly aftertouch is per key (the sounding voice of that key only). */
    levi_init_set(&A);
    levi_note_on(&A, 60u);
    levi_note_on(&A, 64u);
    run(&A, 2u);
    RI_ASSERT(levi_polyat(&A, 60u, 127u) == 0, "polyat ok");
    RI_ASSERT(levi_polyat(&A, 67u, 127u) == 0, "polyat silent key ok");
    RI_ASSERT(feq(A.v[0].pat01, 0.0f), "not copied before a block %f", A.v[0].pat01);
    run(&A, 2u);
    RI_ASSERT(feq(A.v[0].pat01, 1.0f), "polyat reached voice 0 %f", A.v[0].pat01);
    RI_ASSERT(feq(A.v[1].pat01, 0.0f), "other key untouched %f", A.v[1].pat01);
    RI_ASSERT(levi_press(&A, 64u) == 0, "press ok");
    run(&A, 2u);
    RI_ASSERT(feq(A.v[0].mpat01, 64.0f / 127.0f) && feq(A.v[1].mpat01, 64.0f / 127.0f),
        "channel pressure reaches every voice %f/%f", A.v[0].mpat01, A.v[1].mpat01);
    RI_ASSERT(levi_press(&A, 200u) == 0, "press clamps");
    RI_ASSERT(A.press == 127u, "press clamped %u", A.press);

    /* A voice reused for another key drops the old key's pressure. */
    levi_init_set(&A);
    levi_set_param_ui(&A, 0u, RI_CTL_LEVI_POLYMODE & 0xFFu, (uint8_t)RI_LEVI_POLY_MONO);
    levi_note_on(&A, 60u);
    levi_polyat(&A, 60u, 127u);
    run(&A, 2u);
    RI_ASSERT(feq(A.v[0].pat01, 1.0f), "pressure present %f", A.v[0].pat01);
    RI_ASSERT(levi_note_on(&A, 67u) == 1, "mono takes the new key");
    RI_ASSERT(A.v[0].note == 67u, "voice 0 reused %u", A.v[0].note);
    run(&A, 2u);
    RI_ASSERT(feq(A.v[0].pat01, 0.0f), "reuse clears pressure %f", A.v[0].pat01);

    /* Mod wheel. */
    levi_init_set(&A);
    levi_note_on(&A, 60u);
    RI_ASSERT(levi_wheel(&A, 127u) == 0, "wheel ok");
    run(&A, 2u);
    RI_ASSERT(feq(A.v[0].wheel01, 1.0f), "wheel max %f", A.v[0].wheel01);
    RI_ASSERT(levi_wheel(&A, 200u) == 0, "wheel clamps");
    RI_ASSERT(A.wheel == 127u, "wheel clamped %u", A.wheel);
    levi_wheel(&A, 0u);
    run(&A, 2u);
    RI_ASSERT(feq(A.v[0].wheel01, 0.0f), "wheel zero %f", A.v[0].wheel01);

    /* Bend normalises against the P6b bend range: UI 0..127 maps 0..24
     * semitones and the default is 2, so range 0 reads 0 and 24 needs 127. */
    levi_init_set(&A);
    levi_note_on(&A, 60u);
    run(&A, 2u);
    RI_ASSERT(feq(A.v[0].vbendrng, 2.0f), "default bend range %f", A.v[0].vbendrng);
    RI_ASSERT(feq(A.v[0].bsrc, 0.0f), "bend centre %f", A.v[0].bsrc);
    RI_ASSERT(levi_bend(&A, 2.0f) == 0, "bend ok");
    run(&A, 2u);
    RI_ASSERT(feq(A.v[0].bsrc, 1.0f), "bend full %f", A.v[0].bsrc);
    RI_ASSERT(levi_bend(&A, -2.0f) == 0, "bend down ok");
    run(&A, 2u);
    RI_ASSERT(feq(A.v[0].bsrc, -1.0f), "bend full down %f", A.v[0].bsrc);
    /* Past the range the source clamps at both stops (bend 3 over the
     * 2 semitone range is 1.5 before the clamp). */
    RI_ASSERT(levi_bend(&A, 3.0f) == 0, "bend past the range up");
    run(&A, 2u);
    RI_ASSERT(feq(A.v[0].bsrc, 1.0f), "bend clamps up %f", A.v[0].bsrc);
    RI_ASSERT(levi_bend(&A, -3.0f) == 0, "bend past the range down");
    run(&A, 2u);
    RI_ASSERT(feq(A.v[0].bsrc, -1.0f), "bend clamps down %f", A.v[0].bsrc);
    RI_ASSERT(A.bend == -3.0f, "bend stored %f", A.bend);
    levi_set_param_ui(&A, 0u, RI_CTL_LEVI_VBENDRNG & 0xFFu, 0u);
    run(&A, 2u);
    RI_ASSERT(feq(A.v[0].bsrc, 0.0f), "bend range 0 reads 0 %f", A.v[0].bsrc);
    levi_set_param_ui(&A, 0u, RI_CTL_LEVI_VBENDRNG & 0xFFu, 127u);
    run(&A, 2u);
    RI_ASSERT(feq(A.v[0].vbendrng, 24.0f), "bend range 24 %f", A.v[0].vbendrng);
    RI_ASSERT(feq(A.v[0].bsrc, -3.0f / 24.0f), "bend reads at range 24 %f", A.v[0].bsrc);
    RI_ASSERT(levi_bend(&A, 40.0f) == 0, "bend clamp ok");
    RI_ASSERT(A.bend == 24.0f, "bend clamped to 24 %f", A.bend);
    levi_bend(&A, -1.0f);

    /* Bend moves exactly its semitones: 7 semitones of bend over the 24
     * semitone range sounds like the key 7 semitones up (tolerance: the
     * pitch is a table lookup times a power, not one rounding). */
    levi_init_set(&A);
    levi_set_param_ui(&A, 0u, RI_CTL_LEVI_VBENDRNG & 0xFFu, 127u);
    levi_note_on(&A, 60u);
    levi_init_set(&B);
    levi_set_param_ui(&B, 0u, RI_CTL_LEVI_VBENDRNG & 0xFFu, 127u);
    levi_note_on(&B, 67u);
    RI_ASSERT(levi_bend(&A, 7.0f) == 0, "bend up 7");
    run(&A, 5u);
    run(&B, 5u);
    {
        static float la[BS], ra[BS], lb[BS], rb[BS];
        uint32_t k;
        float d = 0.0f, p = 0.0f;
        levi_voice_render_sum_stereo(&A, la, ra, BS, SR);
        levi_voice_render_sum_stereo(&B, lb, rb, BS, SR);
        for (k = 0u; k < BS; k++) {
            float m = la[k] > lb[k] ? la[k] - lb[k] : lb[k] - la[k];
            if (m > d)
                d = m;
            m = la[k] > 0.0f ? la[k] : -la[k];
            if (m > p)
                p = m;
        }
        RI_ASSERT(p > 0.0f && d <= p * 1e-3f, "bend equals 7 semitones (diff %f of peak %f)", d, p);
    }

    /* Source laws: twins differing only in the signal a route reads, with
     * the VCA level at depth 100 — the route scales the amp by 1 + x, so a
     * source of 1.0 gives x = 57/100 and the high-source twin is louder. */
    levi_init_set(&A);
    route_vca(&A, RI_LEVI_MS_VELON);
    levi_note_vel(&A, 60u, 0u);
    levi_init_set(&B);
    route_vca(&B, RI_LEVI_MS_VELON);
    levi_note_vel(&B, 60u, 127u);
    run(&A, 5u);
    run(&B, 5u);
    {
        float pa = peak_of(&A), pb = peak_of(&B);
        RI_ASSERT(pb > pa * 1.5f, "VELON raises the level (%f vs %f)", pa, pb);
    }
    levi_init_set(&A);
    route_vca(&A, RI_LEVI_MS_POLYAT);
    levi_note_on(&A, 60u);
    levi_polyat(&A, 60u, 127u);
    levi_init_set(&B);
    route_vca(&B, RI_LEVI_MS_POLYAT);
    levi_note_on(&B, 60u);
    run(&A, 5u);
    run(&B, 5u);
    {
        float pa = peak_of(&A), pb = peak_of(&B);
        RI_ASSERT(pa > pb * 1.5f, "POLYAT raises the level (%f vs %f)", pa, pb);
    }
    levi_init_set(&A);
    route_vca(&A, RI_LEVI_MS_MONOAT);
    levi_note_on(&A, 60u);
    levi_press(&A, 127u);
    levi_init_set(&B);
    route_vca(&B, RI_LEVI_MS_MONOAT);
    levi_note_on(&B, 60u);
    run(&A, 5u);
    run(&B, 5u);
    {
        float pa = peak_of(&A), pb = peak_of(&B);
        RI_ASSERT(pa > pb * 1.5f, "MONOAT raises the level (%f vs %f)", pa, pb);
    }
    levi_init_set(&A);
    route_vca(&A, RI_LEVI_MS_WHEEL);
    levi_note_on(&A, 60u);
    levi_wheel(&A, 127u);
    levi_init_set(&B);
    route_vca(&B, RI_LEVI_MS_WHEEL);
    levi_note_on(&B, 60u);
    run(&A, 5u);
    run(&B, 5u);
    {
        float pa = peak_of(&A), pb = peak_of(&B);
        RI_ASSERT(pa > pb * 1.5f, "WHEEL raises the level (%f vs %f)", pa, pb);
    }
    /* VELOFF reads the release velocity, not the note-on one (a note-off
     * between renders, so the release tail is what carries it). */
    levi_init_set(&A);
    route_vca(&A, RI_LEVI_MS_VELOFF);
    levi_note_vel(&A, 60u, 127u);
    run(&A, 5u);
    levi_note_rel_vel(&A, 60u, 40u);
    run(&A, 5u);
    levi_init_set(&B);
    route_vca(&B, RI_LEVI_MS_VELOFF);
    levi_note_vel(&B, 60u, 127u);
    run(&B, 5u);
    levi_note_rel_vel(&B, 60u, 100u);
    run(&B, 5u);
    {
        float pa = peak_of(&A), pb = peak_of(&B);
        RI_ASSERT(pb > pa * 1.15f, "VELOFF raises the level (%f vs %f)", pa, pb);
    }
    /* BEND as a matrix source: -12 semitones over a 24 semitone range is
     * half scale, so the bent twin is the quieter one (the pitch also
     * moves, which is why this law reads the level, not the samples). */
    levi_init_set(&A);
    route_vca(&A, RI_LEVI_MS_BEND);
    levi_set_param_ui(&A, 0u, RI_CTL_LEVI_VBENDRNG & 0xFFu, 127u);
    levi_note_on(&A, 60u);
    levi_bend(&A, -12.0f);
    levi_init_set(&B);
    route_vca(&B, RI_LEVI_MS_BEND);
    levi_set_param_ui(&B, 0u, RI_CTL_LEVI_VBENDRNG & 0xFFu, 127u);
    levi_note_on(&B, 60u);
    run(&A, 5u);
    run(&B, 5u);
    {
        float pa = peak_of(&A), pb = peak_of(&B);
        RI_ASSERT(pa < pb * 0.75f, "BEND lowers the level (%f vs %f)", pa, pb);
    }
    /* No route = no sound change (the bit-identity law for songs). */
    levi_init_set(&A);
    levi_note_vel(&A, 60u, 64u);
    levi_init_set(&B);
    levi_note_on(&B, 60u);
    run(&A, 5u);
    run(&B, 5u);
    RI_ASSERT(!audible_diff(&A, &B), "velocity alone changes nothing");

    /* Source identity: every signal has its own source id and name. */
    RI_ASSERT(RI_LEVI_MS_VELON == 29u && RI_LEVI_MS_VELOFF == 30u && RI_LEVI_MS_POLYAT == 27u
        && RI_LEVI_MS_MONOAT == 28u && RI_LEVI_MS_WHEEL == 33u && RI_LEVI_MS_BEND == 34u, "source ids");
    RI_ASSERT(strlen(ri_levi_ms_name(RI_LEVI_MS_VELON)) > 0u, "velon name");
    RI_ASSERT(ri_levi_ms_to_ui(RI_LEVI_MS_VELON) != 0u, "velon in the UI list");

    /* Entry-point contracts and null-safety. */
    RI_ASSERT(levi_note_vel(0, 60u, 64u) == -1, "note_vel null");
    RI_ASSERT(levi_note_vel(&A, 200u, 64u) == -1, "note_vel bad note");
    RI_ASSERT(levi_note_rel_vel(0, 60u, 64u) == -1, "rel_vel null");
    RI_ASSERT(levi_note_rel_vel(&A, 200u, 64u) == -1, "rel_vel bad note");
    RI_ASSERT(levi_press(0, 64u) == 2, "press null");
    RI_ASSERT(levi_polyat(0, 60u, 64u) == 2, "polyat null");
    RI_ASSERT(levi_wheel(0, 64u) == 2, "wheel null");
    RI_ASSERT(levi_bend(0, 1.0f) == 2, "bend null");
    RI_ASSERT(levi_bend(&A, -1.0e9f) == 0 && A.bend == -24.0f, "bend floor %f", A.bend);

    /* Unison mode stacks every voice with the same velocity. */
    levi_init_set(&A);
    levi_set_param_ui(&A, 0u, RI_CTL_LEVI_POLYMODE & 0xFFu, (uint8_t)RI_LEVI_POLY_UNISON);
    RI_ASSERT(levi_note_vel(&A, 60u, 30u) == (int)A.ulimit, "unison fires the limit");
    RI_ASSERT(A.v[0].nvel == 30u && A.v[1].nvel == 30u, "unison velocity %u/%u", A.v[0].nvel, A.v[1].nvel);

    /* Bounded extremes storm (signals dancing, bends at both stops). */
    {
        static float l[BS], r[BS];
        int i, bad = 0;
        levi_init_set(&A);
        levi_note_on(&A, 30u);
        for (i = 0; i < 400; i++) {
            uint32_t k;
            if (i % 17 == 0)
                levi_bend(&A, (float)((i * 13) % 61) - 30.0f);
            if (i % 23 == 0)
                levi_press(&A, (uint8_t)((i * 7) % 128u));
            if (i % 29 == 0)
                levi_polyat(&A, 30u, (uint8_t)((i * 11) % 128u));
            if (i % 31 == 0)
                levi_wheel(&A, (uint8_t)((i * 5) % 128u));
            if (i % 37 == 0)
                levi_note_vel(&A, 40u + (uint8_t)(i % 40u), (uint8_t)((i * 3) % 200u));
            levi_voice_render_sum_stereo(&A, l, r, BS, SR);
            for (k = 0u; k < BS; k++)
                if (!((l[k] > -8.0f && l[k] < 8.0f) && (r[k] > -8.0f && r[k] < 8.0f)))
                    bad = 1;
        }
        RI_ASSERT(!bad, "extremes bounded");
    }

    RI_RESULT("t146_levi_perfsig");
}