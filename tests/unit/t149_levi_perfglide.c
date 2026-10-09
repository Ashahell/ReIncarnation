/* t149_levi_perfglide — Levi P9d: the glide button and chord mode.
 *
 * Laws: the glide button is the glide *mode* and nothing else, so a set with
 * the panel mode on and a set with the mode off and the button held render
 * bit-identical blocks; the mode still wins over the button (a glissando
 * stays a glissando), and the DM_VOICE glide-toggle offset still wins over
 * both; the button is live per block, not stamped at fire time (releasing it
 * stops a slide in progress, exactly as turning the voice's mode off does)
 * and it starts a slide on the very next note without waiting for a render;
 * a fresh voice still starts on pitch.
 *
 * Chord mode is off by default and an explicit "off" moves nothing. A pushed
 * chord fires its *on* lanes as held notes transposed by (played - root),
 * where the root is the lowest on lane, so the keyboard still transposes the
 * chord; transposition clamps at both ends of the keyboard; an empty chord is
 * silent (no fallback to the single note); a strike never enters chord mode;
 * a chord passes the P9c bias and zone routing (a chord is a keyboard feature
 * layered on the same routing); any key release releases the whole chord with
 * the pending release velocity and empties the hold list, and per-key
 * aftertouch lands on every held voice while a song lane voice -- not the
 * player's hold -- is left alone; both live note-on entry points fan out; the
 * song path (levi_trigger) is untouched.
 */
#include <stdio.h>
#include <string.h>
#include "tests/helpers/ri_assert.h"
#include "engine/dsp/levi.h"
#include "engine/dsp/levi_matrix.h"
#include "engine/seq/autolane.h"
#include "gui/sectlevi.h"
#include "gui/ctlreg.h"

#define SR 48000.0f
#define BS 256u

static struct RILeviSet A, B;

/* Render n blocks on one set. */
static void run(struct RILeviSet *s, uint32_t nblocks) {
    static float l[BS], r[BS];
    uint32_t i;
    for (i = 0u; i < nblocks; i++)
        levi_voice_render_sum_stereo(s, l, r, BS, SR);
}

/* 1 when the two sets' next n stereo blocks differ (same-age twins). */
static int diff_blocks(struct RILeviSet *a, struct RILeviSet *b, uint32_t n) {
    static float la[BS], lb[BS], oa[BS], ob[BS];
    uint32_t i, k;
    for (i = 0u; i < n; i++) {
        levi_voice_render_sum_stereo(a, la, lb, BS, SR);
        levi_voice_render_sum_stereo(b, oa, ob, BS, SR);
        for (k = 0u; k < BS; k++)
            if (la[k] != oa[k] || lb[k] != ob[k])
                return 1;
    }
    return 0;
}

static uint32_t active_count(const struct RILeviSet *s) {
    uint32_t v, n = 0u;
    for (v = 0u; v < RI_LEVI_NVOICES; v++)
        n += s->v[v].active ? 1u : 0u;
    return n;
}

/* Sorted list of the active voices' notes (the chord that sounded). */
static uint32_t sounding(const struct RILeviSet *s, uint8_t *out, uint32_t max) {
    uint32_t v, n = 0u, i, j;
    for (v = 0u; v < RI_LEVI_NVOICES && n < max; v++)
        if (s->v[v].active) {
            out[n++] = s->v[v].note;
            for (i = n - 1u; i > 0u && out[i] < out[i - 1u]; i--) {
                uint8_t t = out[i];
                out[i] = out[i - 1u];
                out[i - 1u] = t;
            }
        }
    for (j = 0u; j < n; j++)
        (void)j;
    return n;
}

static void glide_twin_setup(struct RILeviSet *s, uint32_t glt_time) {
    levi_init_set(s);
    levi_set_alloc_ui(s, RI_LEVI_POLY_MONO);
    levi_set_param_ui(s, 0u, RI_CTL_LEVI_VGLIDE & 0xFFu, 0u);
    levi_set_param_ui(s, 0u, RI_CTL_LEVI_VGLTIME & 0xFFu, glt_time);
}

/* The macro toggle: keytrack (note - 60)/60 as the source, so a note
 * above C4 drives it negative and the macro switches glide *off*. */
static void route_glidetgl(struct RILeviSet *s, int depth) {
    RI_ASSERT(ri_levi_matrix_route(&s->mx, 0u, RI_LEVI_MS_NOTE, RI_LEVI_DM_VOICE,
        RI_LEVI_DVO_GLIDETGL, depth) == 0, "glide toggle route");
}

static const uint8_t CH3[3] = { 60u, 64u, 67u };

int main(void) {
    /* ---- Keys, ids and the value law. ---- */
    RI_ASSERT(RI_CTL_LEVI_GLIDE == 0x0ECAu && RI_CTL_LEVI_CHORD == 0x0ECBu, "glide/chord keys");
    RI_ASSERT(RI_LEVI_NCHORD == 6u, "chord lanes");

    levi_init_set(&A);
    RI_ASSERT(A.p_glidehold == 0u && A.p_chord == 0u && A.p_chord_n == 0u
        && A.p_chord_on == 0u, "glide and chord default off");
    RI_ASSERT(levi_glide_hold(0, 1) == 2, "glide null refused");
    RI_ASSERT(levi_glide_hold(&A, 1) == 0 && A.p_glidehold == 1u, "glide hold on");
    RI_ASSERT(levi_glide_hold(&A, 0) == 0 && A.p_glidehold == 0u, "glide hold off");
    RI_ASSERT(levi_chord_mode(0, 1) == 2, "chord mode null refused");
    RI_ASSERT(levi_chord_mode(&A, 1) == 0 && A.p_chord == 1u, "chord mode on");
    RI_ASSERT(levi_chord_mode(&A, 0) == 0 && A.p_chord == 0u, "chord mode off");
    RI_ASSERT(levi_chord_set(0, 1u, CH3, 3u) == 2, "chord push null refused");

    /* ---- The glide button is the mode, bit for bit. ---- */
    glide_twin_setup(&A, 100u);
    levi_set_param_ui(&A, 0u, RI_CTL_LEVI_VGLIDE & 0xFFu, 1u);   /* mode glide */
    glide_twin_setup(&B, 100u);                                   /* mode off   */
    RI_ASSERT(levi_glide_hold(&B, 1) == 0, "button held");
    levi_note_vel(&A, 60u, 100u);
    levi_note_vel(&B, 60u, 100u);
    run(&A, 1u);
    run(&B, 1u);
    levi_note_vel(&A, 72u, 100u);
    levi_note_vel(&B, 72u, 100u);
    RI_ASSERT(A.v[0].glt == 0.0f && B.v[0].glt == 0.0f, "both slides start");
    RI_ASSERT(!diff_blocks(&A, &B, 8u), "the button is the glide mode, bit for bit");

    /* The mode still wins: a glissando stays a glissando under the button. */
    glide_twin_setup(&A, 100u);
    levi_set_param_ui(&A, 0u, RI_CTL_LEVI_VGLIDE & 0xFFu, 127u);  /* glissando */
    glide_twin_setup(&B, 100u);
    levi_set_param_ui(&B, 0u, RI_CTL_LEVI_VGLIDE & 0xFFu, 127u);
    RI_ASSERT(levi_glide_hold(&B, 1) == 0, "button held over glissando");
    levi_note_vel(&A, 60u, 100u);
    levi_note_vel(&B, 60u, 100u);
    run(&A, 1u);
    run(&B, 1u);
    levi_note_vel(&A, 72u, 100u);
    levi_note_vel(&B, 72u, 100u);
    RI_ASSERT(!diff_blocks(&A, &B, 8u), "the mode wins over the button");

    /* The macro toggle still wins over both (depth -127 on a note above C4). */
    glide_twin_setup(&A, 100u);
    route_glidetgl(&A, -100);
    RI_ASSERT(levi_glide_hold(&A, 1) == 0, "button held under the macro");
    glide_twin_setup(&B, 100u);
    route_glidetgl(&B, -100);
    levi_note_vel(&A, 72u, 100u);
    levi_note_vel(&B, 72u, 100u);
    run(&A, 1u);
    run(&B, 1u);
    levi_note_vel(&A, 84u, 100u);
    levi_note_vel(&B, 84u, 100u);
    RI_ASSERT(!diff_blocks(&A, &B, 8u), "the macro toggle wins over the button");

    /* The button brings no time of its own: a stored time of 0 still lands
     * instantly, bit-identical to the same set with the button up. */
    glide_twin_setup(&A, 0u);
    RI_ASSERT(levi_glide_hold(&A, 1) == 0, "button held at time 0");
    glide_twin_setup(&B, 0u);
    levi_note_vel(&A, 60u, 100u);
    levi_note_vel(&B, 60u, 100u);
    run(&A, 1u);
    run(&B, 1u);
    levi_note_vel(&A, 72u, 100u);
    levi_note_vel(&B, 72u, 100u);
    RI_ASSERT(!diff_blocks(&A, &B, 4u), "the button uses the stored glide time");

    /* ---- The button is live per block. ---- */
    glide_twin_setup(&A, 100u);
    RI_ASSERT(levi_glide_hold(&A, 1) == 0, "button held");
    levi_note_vel(&A, 60u, 100u);
    run(&A, 1u);
    RI_ASSERT(A.v[0].gforce == 1u, "the block refresh carries the button");
    /* It starts the next slide with no render in between. */
    levi_note_vel(&A, 72u, 100u);
    RI_ASSERT(A.v[0].glt == 0.0f && A.v[0].glsemi == -12.0f, "the button starts the slide");
    run(&A, 1u);
    RI_ASSERT(A.v[0].glt > 0.0f && A.v[0].glt < 1.0f, "the slide is running");
    RI_ASSERT(levi_glide_hold(&A, 0) == 0, "button up");
    run(&A, 1u);
    RI_ASSERT(A.v[0].gforce == 0u, "the block refresh drops the button");
    /* Handed back means the slide stops, bit-for-bit the same as turning
     * the voice's own mode off at that moment. (A twin whose *history*
     * lacked the slide cannot be compared: the gliding block changed the
     * accumulated oscillator phase.) */
    glide_twin_setup(&B, 100u);
    levi_set_param_ui(&B, 0u, RI_CTL_LEVI_VGLIDE & 0xFFu, 1u);
    levi_note_vel(&B, 60u, 100u);
    run(&B, 1u);
    levi_note_vel(&B, 72u, 100u);
    run(&B, 1u);
    RI_ASSERT(B.v[0].glt > 0.0f && B.v[0].glt < 1.0f, "the mode slide runs too");
    levi_set_param_ui(&B, 0u, RI_CTL_LEVI_VGLIDE & 0xFFu, 0u);
    run(&B, 1u);
    RI_ASSERT(!diff_blocks(&A, &B, 4u), "releasing the button hands the voice back");
    /* It never starts a slide retroactively. */
    glide_twin_setup(&A, 100u);
    levi_note_vel(&A, 60u, 100u);
    run(&A, 1u);
    levi_note_vel(&A, 72u, 100u);
    RI_ASSERT(A.v[0].glt == 1.0f, "no button, no slide");
    RI_ASSERT(levi_glide_hold(&A, 1) == 0, "button held after the note");
    run(&A, 1u);
    RI_ASSERT(A.v[0].glt == 1.0f, "the button does not slide a landed note");
    /* A fresh voice still starts on pitch. */
    glide_twin_setup(&A, 100u);
    RI_ASSERT(levi_glide_hold(&A, 1) == 0, "button held on a fresh voice");
    levi_note_vel(&A, 60u, 100u);
    RI_ASSERT(A.v[0].glt == 1.0f && A.v[0].glsemi == 0.0f, "a fresh voice is on pitch");
    /* The legato path retunes without retriggering, and that is where the
     * slide state is set too, so the button has to reach it. */
    glide_twin_setup(&A, 100u);
    levi_set_op_ui(&A, 0u, 0u, RI_LEVI_OP_LEGATO, 1u);
    RI_ASSERT(levi_glide_hold(&A, 1) == 0, "button held over legato");
    glide_twin_setup(&B, 100u);
    levi_set_op_ui(&B, 0u, 0u, RI_LEVI_OP_LEGATO, 1u);
    levi_set_param_ui(&B, 0u, RI_CTL_LEVI_VGLIDE & 0xFFu, 1u);
    levi_note_vel(&A, 60u, 100u);
    levi_note_vel(&B, 60u, 100u);
    run(&A, 1u);
    run(&B, 1u);
    levi_note_vel(&A, 72u, 100u);
    levi_note_vel(&B, 72u, 100u);
    RI_ASSERT(A.v[0].glt == 0.0f && A.v[0].glsemi == -12.0f,
        "the button slides a legato retune");
    RI_ASSERT(!diff_blocks(&A, &B, 8u), "the button is the mode on the legato path too");
    /* The ribbon's theremin retune is that same retune, so the button
     * slides there too -- one flag, every place a pitch moves. */
    glide_twin_setup(&A, 100u);
    levi_set_param_ui(&A, 0u, RI_CTL_LEVI_RBNMODE & 0xFFu, 3u);   /* theremin */
    RI_ASSERT(levi_glide_hold(&A, 1) == 0, "button held over the ribbon");
    levi_note_vel(&A, 60u, 100u);
    RI_ASSERT(levi_ribbon_touch(&A, 60u) == 0, "ribbon touch");
    run(&A, 1u);
    RI_ASSERT(levi_ribbon_move(&A, 72u) == 0, "ribbon move");
    RI_ASSERT(A.v[0].glt == 0.0f && A.v[0].glsemi == -12.0f,
        "the button slides a ribbon retune");
    glide_twin_setup(&B, 100u);
    levi_set_param_ui(&B, 0u, RI_CTL_LEVI_RBNMODE & 0xFFu, 3u);
    levi_note_vel(&B, 60u, 100u);
    RI_ASSERT(levi_ribbon_touch(&B, 60u) == 0, "ribbon touch");
    run(&B, 1u);
    RI_ASSERT(levi_ribbon_move(&B, 72u) == 0, "ribbon move");
    RI_ASSERT(B.v[0].glt == 1.0f, "no button, no ribbon slide");

    /* ---- Chord mode is off by default, bit for bit. ---- */
    levi_init_set(&A);
    levi_chord_set(&A, 0x7u, CH3, 3u);
    levi_init_set(&B);
    levi_chord_mode(&B, 0);
    RI_ASSERT(levi_note_vel(&A, 60u, 100u) == 1, "chord off is a single note");
    RI_ASSERT(levi_note_vel(&B, 60u, 100u) == 1, "plain single note");
    RI_ASSERT(A.v[0].note == 60u && B.v[0].note == 60u, "the pushed chord is inert while off");
    RI_ASSERT(!diff_blocks(&A, &B, 4u), "an explicit chord-off moves nothing");

    /* ---- Pushing a chord. ---- */
    levi_init_set(&A);
    RI_ASSERT(levi_chord_set(&A, 0x7u, CH3, 3u) == 0, "push three lanes");
    RI_ASSERT(A.p_chord_n == 3u && A.p_chord_on == 0x7u, "lane count and mask");
    RI_ASSERT(A.p_chord_note[0] == 60u && A.p_chord_note[1] == 64u && A.p_chord_note[2] == 67u,
        "lane notes stored");
    {
        static const uint8_t SIX[6] = { 40u, 44u, 47u, 52u, 59u, 64u };
        RI_ASSERT(levi_chord_set(&A, 0x3Fu, SIX, 6u) == 0, "six lanes fit");
        RI_ASSERT(A.p_chord_n == 6u && A.p_chord_on == 0x3Fu, "six lanes stored");
        RI_ASSERT(levi_chord_set(&A, 0x3Fu, SIX, 7u) == 2, "seven lanes refused");
        RI_ASSERT(levi_chord_set(&A, 0x3Fu, SIX, 2u) == 0, "push two lanes, all bits on");
        RI_ASSERT(A.p_chord_n == 2u && A.p_chord_on == 0x3u, "lanes past n are cleared");
        RI_ASSERT(A.p_chord_note[2] == 0u && A.p_chord_note[5] == 0u,
            "a shorter push clears the lane notes too");
        RI_ASSERT(levi_chord_set(&A, 0x3u, SIX, -1) == 2, "a negative lane count refused");
    }
    {
        static const uint8_t LOUD[2] = { 200u, 255u };
        RI_ASSERT(levi_chord_set(&A, 0x3u, LOUD, 2u) == 0, "push out-of-range lanes");
        RI_ASSERT(A.p_chord_note[0] == 127u && A.p_chord_note[1] == 127u,
            "lane notes clamp to the keyboard");
    }

    /* ---- The chord fires its on lanes, transposed by played - root. ---- */
    {
        static const struct { uint8_t played; uint8_t want[3]; } CASES[4] = {
            { 60u, { 60u, 64u, 67u } },   /* at the root: no shift */
            { 48u, { 48u, 52u, 55u } },   /* a fifth below: down an octave */
            { 72u, { 72u, 76u, 79u } },   /* an octave up */
            { 57u, { 57u, 61u, 64u } }    /* a semitone down */
        };
        uint32_t i;
        for (i = 0u; i < 4u; i++) {
            uint8_t got[4];
            uint32_t n;
            levi_init_set(&A);
            levi_chord_set(&A, 0x7u, CH3, 3u);
            RI_ASSERT(levi_chord_mode(&A, 1) == 0, "chord mode on");
            RI_ASSERT(levi_note_vel(&A, CASES[i].played, 100u) == 3,
                "a chord is three voices at %u", CASES[i].played);
            n = sounding(&A, got, 4u);
            RI_ASSERT(n == 3u && got[0] == CASES[i].want[0] && got[1] == CASES[i].want[1] &&
                got[2] == CASES[i].want[2], "chord at %u sounds %u %u %u (%u %u %u)",
                CASES[i].played, got[0], got[1], got[2],
                CASES[i].want[0], CASES[i].want[1], CASES[i].want[2]);
            RI_ASSERT(A.an == 3u && A.anotes[0] == CASES[i].want[0], "the chord is held");
        }
    }
    /* The root is the lowest *on* lane, not lane 0. */
    {
        static const uint8_t LANES[3] = { 60u, 64u, 67u };
        uint8_t got[4];
        levi_init_set(&A);
        levi_chord_set(&A, 0x6u, LANES, 3u);       /* lanes 1 and 2 on */
        levi_chord_mode(&A, 1);
        RI_ASSERT(levi_note_vel(&A, 60u, 100u) == 2, "two lanes on");
        RI_ASSERT(sounding(&A, got, 4u) == 2u && got[0] == 60u && got[1] == 63u,
            "the root is the lowest on lane, not lane 0 (%u %u)", got[0], got[1]);
    }
    /* Transposition clamps at both ends of the keyboard. */
    {
        static const uint8_t LOW[2] = { 60u, 67u };
        static const uint8_t HIGH[2] = { 60u, 67u };
        uint8_t got[4];
        levi_init_set(&A);
        levi_chord_set(&A, 0x3u, LOW, 2u);
        levi_chord_mode(&A, 1);
        RI_ASSERT(levi_note_vel(&A, 0u, 100u) == 2, "chord at the bottom");
        RI_ASSERT(sounding(&A, got, 4u) == 2u && got[0] == 0u && got[1] == 7u,
            "the low clamp floors, it does not wrap (%u %u)", got[0], got[1]);
        levi_init_set(&A);
        levi_chord_set(&A, 0x3u, HIGH, 2u);
        levi_chord_mode(&A, 1);
        RI_ASSERT(levi_note_vel(&A, 127u, 100u) == 2, "chord at the top");
        RI_ASSERT(sounding(&A, got, 4u) == 2u && got[0] == 127u && got[1] == 127u,
            "the high clamp caps, it does not wrap (%u %u)", got[0], got[1]);
        /* The root is the lowest *on* lane, so a lane above the root in
         * value can only reach below the keyboard by being clamped -- this
         * is the low clamp the other case cannot see (there the root lane
         * itself lands on the played key). */
        {
            static const uint8_t ABOVE[2] = { 70u, 62u };
            levi_init_set(&A);
            levi_chord_set(&A, 0x3u, ABOVE, 2u);
            levi_chord_mode(&A, 1);
            RI_ASSERT(levi_note_vel(&A, 4u, 100u) == 2, "chord under the root lane");
            RI_ASSERT(sounding(&A, got, 4u) == 2u && got[0] == 0u && got[1] == 4u,
                "a lane below the root floors at the bottom key (%u %u)", got[0], got[1]);
        }
    }
    /* An empty chord is silent: no fallback to the single note. */
    levi_init_set(&A);
    RI_ASSERT(levi_chord_set(&A, 0x7u, CH3, 3u) == 0, "push three lanes");
    RI_ASSERT(levi_chord_mode(&A, 1) == 0, "chord mode on");
    RI_ASSERT(levi_chord_set(&A, 0u, CH3, 0u) == 0, "push an empty chord");
    RI_ASSERT(levi_note_vel(&A, 60u, 100u) == 0, "an empty chord is silent");
    RI_ASSERT(active_count(&A) == 0u && A.an == 0u, "nothing sounded and nothing held");
    RI_ASSERT(levi_note_off(&A, 60u) == 0, "no release for a silent chord");

    /* A strike never enters chord mode. */
    levi_init_set(&A);
    levi_chord_set(&A, 0x7u, CH3, 3u);
    levi_chord_mode(&A, 1);
    RI_ASSERT(levi_note_strike(&A, 60u) == 1, "a strike is one note");
    RI_ASSERT(active_count(&A) == 1u && A.v[0].note == 60u && A.an == 0u,
        "the strike plays its own note and holds nothing");

    /* A chord is a keyboard feature: it passes the bias and the zones. */
    {
        uint8_t got[6];
        levi_init_set(&A);
        levi_chord_set(&A, 0x7u, CH3, 3u);
        levi_chord_mode(&A, 1);
        RI_ASSERT(levi_perf_set(&A, RI_LEVI_PF_OCT, 3) == 0, "octave +1");
        RI_ASSERT(levi_note_vel(&A, 60u, 100u) == 3, "a biased chord is three voices");
        RI_ASSERT(sounding(&A, got, 6u) == 3u && got[0] == 72u && got[1] == 76u && got[2] == 79u,
            "the bias shifts every lane (%u %u %u)", got[0], got[1], got[2]);
        levi_init_set(&A);
        levi_chord_set(&A, 0x3u, CH3, 2u);
        levi_chord_mode(&A, 1);
        levi_perf_set(&A, RI_LEVI_PF_MODE, 1);
        levi_perf_set(&A, RI_LEVI_PF_SPLITM, 0);
        RI_ASSERT(levi_note_vel(&A, 60u, 100u) == 4, "a dual chord is two voices per lane");
        RI_ASSERT(active_count(&A) == 4u, "four voices");
    }

    /* ---- Any key releases the whole chord. ---- */
    {
        uint32_t v, nrel;
        levi_init_set(&A);
        levi_chord_set(&A, 0x7u, CH3, 3u);
        levi_chord_mode(&A, 1);
        levi_note_vel(&A, 60u, 100u);
        run(&A, 2u);
        RI_ASSERT(levi_note_off(&A, 60u) == 3, "the release returns the chord count");
        for (v = 0u; v < RI_LEVI_NVOICES; v++)
            if (A.v[v].active)
                RI_ASSERT(A.v[v].menv[2].stage == RI_LEVI_SEG_R, "chord voice %u released", v);
        RI_ASSERT(A.an == 0u, "the hold list empties");
        /* Any key will do, not the one that played it. */
        levi_init_set(&A);
        levi_chord_set(&A, 0x7u, CH3, 3u);
        levi_chord_mode(&A, 1);
        levi_note_vel(&A, 60u, 100u);
        run(&A, 2u);
        RI_ASSERT(levi_note_off(&A, 96u) == 3, "an unplayed key releases the chord");
        nrel = 0u;
        for (v = 0u; v < RI_LEVI_NVOICES; v++)
            if (A.v[v].active && A.v[v].menv[2].stage == RI_LEVI_SEG_R)
                nrel++;
        RI_ASSERT(nrel == 3u, "all three released (%u)", nrel);
        /* The release velocity reaches every voice of the chord. */
        levi_init_set(&A);
        levi_chord_set(&A, 0x7u, CH3, 3u);
        levi_chord_mode(&A, 1);
        levi_note_vel(&A, 60u, 100u);
        run(&A, 2u);
        RI_ASSERT(levi_note_rel_vel(&A, 60u, 90u) == 3, "release velocity releases the chord");
        for (v = 0u; v < RI_LEVI_NVOICES; v++)
            if (A.v[v].active)
                RI_ASSERT(A.v[v].nveloff == 90u, "chord voice %u takes the release velocity", v);
        /* Per-key aftertouch lands on every held voice of the chord. */
        levi_init_set(&A);
        levi_chord_set(&A, 0x7u, CH3, 3u);
        levi_chord_mode(&A, 1);
        levi_note_vel(&A, 60u, 100u);
        RI_ASSERT(levi_polyat(&A, 60u, 100u) == 0, "pressure on the played key");
        for (v = 0u; v < RI_LEVI_NVOICES; v++)
            if (A.v[v].active)
                RI_ASSERT(A.pat[v] == 100u, "chord voice %u takes the pressure", v);
        /* A monophonic allocator still routes every lane -- chord mode
         * is a keyboard feature, not an allocator -- but its own law
         * reuses one voice, so the last lane is the one that sounds.
         * The release still goes through the chord path. */
        levi_init_set(&A);
        levi_set_alloc_ui(&A, RI_LEVI_POLY_MONO);
        levi_chord_set(&A, 0x7u, CH3, 3u);
        levi_chord_mode(&A, 1);
        RI_ASSERT(levi_note_vel(&A, 60u, 100u) == 3, "a mono allocator routes every lane");
        RI_ASSERT(active_count(&A) == 1u && A.v[0].note == 67u,
            "mono keeps its own law: one voice, the last lane");
        run(&A, 1u);
        RI_ASSERT(levi_note_off(&A, 60u) == 1 && A.v[0].menv[2].stage == RI_LEVI_SEG_R,
            "the chord release still releases the mono voice");
        RI_ASSERT(A.an == 0u, "the hold list empties in mono mode too");
    }

    /* Both live entry points fan out: a plain note-on is a chord too. */
    levi_init_set(&A);
    levi_chord_set(&A, 0x7u, CH3, 3u);
    RI_ASSERT(levi_chord_mode(&A, 1) == 0, "chord mode on");
    RI_ASSERT(levi_note_on(&A, 60u) == 3, "a plain note-on pushes the chord");

    /* The song path never sees a chord. */
    levi_init_set(&A);
    levi_chord_set(&A, 0x7u, CH3, 3u);
    levi_chord_mode(&A, 1);
    RI_ASSERT(levi_trigger(&A, 4u, 60u) == 0, "song lane trigger");
    RI_ASSERT(A.v[4].note == 60u && active_count(&A) == 1u, "the song lane plays its own note");

    /* A song lane voice is not the player's hold: a chord release and a
     * chord pressure are about the keys under the fingers, so the song
     * voice sounds on past them. */
    {
        uint32_t v;
        levi_init_set(&A);
        levi_chord_set(&A, 0x7u, CH3, 3u);
        levi_chord_mode(&A, 1);
        RI_ASSERT(levi_trigger(&A, 4u, 40u) == 0, "song lane voice");
        RI_ASSERT(levi_note_vel(&A, 60u, 100u) == 3, "a chord over a song lane voice");
        run(&A, 1u);
        RI_ASSERT(levi_polyat(&A, 60u, 100u) == 0, "pressure over a song lane voice");
        RI_ASSERT(A.pat[4] == 0u, "the song voice takes no chord pressure");
        for (v = 0u; v < RI_LEVI_NVOICES; v++)
            if (A.v[v].active && v != 4u)
                RI_ASSERT(A.pat[v] == 100u, "chord voice %u takes the pressure", v);
        RI_ASSERT(levi_note_off(&A, 60u) == 3, "only the chord voices release");
        RI_ASSERT(A.v[4].active && A.v[4].menv[2].stage != RI_LEVI_SEG_R,
            "the song lane voice still sounds after the chord release");
        RI_ASSERT(A.an == 0u, "the hold list held the chord only");
    }

    /* ---- The panel rows drive the device through the section-wide setter. ---- */
    levi_init_set(&A);
    RI_ASSERT(levi_set_param_ui(&A, 0u, RI_CTL_LEVI_GLIDE & 0xFFu, 1u) == 0 && A.p_glidehold == 1u,
        "the glide row reaches the device");
    RI_ASSERT(levi_set_param_ui(&A, 0u, RI_CTL_LEVI_CHORD & 0xFFu, 1u) == 0 && A.p_chord == 1u,
        "the chord row reaches the device");

    /* ---- Keys, registry rows and the panel page. ---- */
    {
        static const struct { uint32_t row; uint16_t key; } ROWS[2] = {
            { RI_SLEVI_GLIDE, RI_CTL_LEVI_GLIDE }, { RI_SLEVI_CHORD, RI_CTL_LEVI_CHORD }
        };
        uint32_t k;
        for (k = 0u; k < 2u; k++) {
            const struct RICtlDef *d = ri_ctlreg_find((uint16_t)((RI_SEC_LEVI << 8) | ROWS[k].row));
            RI_ASSERT(d, "row %u exists", ROWS[k].row);
            RI_ASSERT(d->kind == RI_CK_SWITCH && d->min_v == 0 && (uint32_t)d->max_v == 1u &&
                d->def_v == 0, "row %u is an off/on switch (%d %d %d)", ROWS[k].row,
                (int)d->min_v, (int)d->max_v, (int)d->def_v);
            RI_ASSERT(d->automatable && d->bind == RI_BIND_LEVI &&
                !strcmp(d->group, "Performance") && !strcmp(d->legend, k ? "Chord" : "Glide"),
                "row %u group/legend/bind", ROWS[k].row);
            RI_ASSERT(d->engine_id == ROWS[k].key, "row %u engine id %04x", ROWS[k].row, d->engine_id);
            RI_ASSERT(ri_auto_allowed(ROWS[k].key), "key %04x allowed", ROWS[k].key);
        }
        RI_ASSERT(RI_SLEVI_GLIDE == 226u && RI_SLEVI_CHORD == 227u && RI_SLEVI_NCTL == 228u,
            "panel rows (225 + the two buttons, P9d)");
        RI_ASSERT(ri_auto_allowed(RI_CTL_LEVI_CHORD) && !ri_auto_allowed(0x0ED1u),
            "0x0ED1 refused (past the live keys)");
    }
    {
        struct RISectLevi lv;
        char tx[32];
        ri_slevi_init(&lv);
        RI_ASSERT(ri_slevi_set_value(&lv, RI_SLEVI_MODULE, (int)RI_SLEVI_M_PERF) == 1, "perf page");
        RI_ASSERT(ri_slevi_page_count(&lv) == 1u, "performance is one page");
        RI_ASSERT(!strcmp(ri_slevi_page_title(&lv), "PERFORMANCE"), "page title");
        RI_ASSERT(ri_slevi_enc_live(&lv, 5u) && !strcmp(ri_slevi_enc_name(&lv, 5u), "GLIDE"),
            "slot 5 is the glide button");
        RI_ASSERT(ri_slevi_enc_live(&lv, 6u) && !strcmp(ri_slevi_enc_name(&lv, 6u), "CHORD"),
            "slot 6 is the chord button");
        RI_ASSERT(!ri_slevi_enc_live(&lv, 7u), "slot 7 still dead");
        lv.page = 0u;
        ri_slevi_enc_text(&lv, 5u, tx, sizeof tx);
        RI_ASSERT(!strcmp(tx, "OFF"), "glide off text %s", tx);
        RI_ASSERT(ri_slevi_set_value(&lv, RI_SLEVI_ENC0 + 5u, 127) == 1, "glide on");
        ri_slevi_enc_text(&lv, 5u, tx, sizeof tx);
        RI_ASSERT(!strcmp(tx, "ON"), "glide on text %s", tx);
        RI_ASSERT(ri_slevi_reset(&lv, RI_SLEVI_ENC0 + 5u) == 1 && lv.val[RI_SLEVI_GLIDE] == 0,
            "glide reset");
        RI_ASSERT(ri_slevi_set_value(&lv, RI_SLEVI_ENC0 + 6u, 127) == 1, "chord on");
        ri_slevi_enc_text(&lv, 6u, tx, sizeof tx);
        RI_ASSERT(!strcmp(tx, "ON"), "chord on text %s", tx);
        RI_ASSERT(ri_slevi_reset(&lv, RI_SLEVI_ENC0 + 6u) == 1 && lv.val[RI_SLEVI_CHORD] == 0,
            "chord reset");
        /* The buttons are not zone knobs: they stay live in Single mode. */
        RI_ASSERT(ri_slevi_enc_live(&lv, 5u) && ri_slevi_enc_live(&lv, 6u),
            "the buttons are live in single");
        RI_ASSERT(ri_slevi_set_value(&lv, RI_SLEVI_ENC0 + 1u, 127) == 1, "mode multi");
        RI_ASSERT(ri_slevi_enc_live(&lv, 5u) && ri_slevi_enc_live(&lv, 6u),
            "the buttons are live in multi");
    }

    /* ---- Null-safety and bounded extremes. ---- */
    RI_ASSERT(levi_note_on(0, 60u) == -1, "null note-on refused");
    RI_ASSERT(levi_chord_mode(&A, 1) == 0 && levi_note_vel(0, 60u, 100u) == -1, "null chord note");
    {
        static float l[BS], r[BS];
        uint32_t i;
        uint8_t lanes[RI_LEVI_NCHORD];
        for (i = 0u; i < RI_LEVI_NCHORD; i++)
            lanes[i] = (uint8_t)(40u + i * 7u);
        levi_init_set(&A);
        for (i = 0u; i < 400u; i++) {
            uint32_t k, bad = 0u;
            float x, y;
            levi_chord_set(&A, (uint32_t)(i % 64u), lanes, (int)(i % (RI_LEVI_NCHORD + 1u)));
            if (i % 3u == 0u)
                levi_chord_mode(&A, (int)(i % 2u));
            levi_glide_hold(&A, (int)((i / 2u) % 2u));
            levi_perf_set(&A, RI_LEVI_PF_OCT, (int)(i % 7u));
            levi_perf_set(&A, RI_LEVI_PF_MODE, (int)(i % 3u));
            if (i % 8u == 0u)
                levi_note_vel(&A, (uint8_t)(i % 128u), (uint8_t)(i * 37u));
            if (i % 11u == 0u)
                levi_note_strike(&A, (uint8_t)(i % 128u));
            if (i % 13u == 0u)
                levi_polyat(&A, (uint8_t)(i % 128u), (uint8_t)(i * 91u));
            if (i % 37u == 0u)
                levi_note_off(&A, (uint8_t)(i % 128u));
            levi_voice_render_sum_stereo(&A, l, r, BS, SR);
            for (k = 0u; k < BS; k++) {
                x = l[k];
                y = r[k];
                if (!(x > -1e20f && x < 1e20f) || !(y > -1e20f && y < 1e20f))
                    bad = 1u;
            }
            RI_ASSERT(!bad, "extremes stay finite at %u", i);
            RI_ASSERT(active_count(&A) <= RI_LEVI_NVOICES, "voice budget at %u", i);
            RI_ASSERT(A.an <= 16u, "hold list bounded at %u", i);
        }
    }
    RI_RESULT("levi_perfglide");
}