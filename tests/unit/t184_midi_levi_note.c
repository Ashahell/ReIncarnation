/* t184_midi_levi_note — M4c/M4d: the Leviasynth channel's notes and
 * performance signals reach the ENGINE, and only through the control
 * plane. Host-driven, the same route RIAPP uses: a parsed message
 * becomes a control-plane send, the live render drains it into the
 * engine, and the engine's own state and audio are the observable.
 *
 * Laws:
 * - a note on the Leviasynth channel makes sound; its note off stops it;
 * - a note with velocity 0 is a note off (MIDI 1.0);
 * - pitch bend, channel aftertouch, poly aftertouch and the mod wheel are
 *   the engine's own performance state, with the manual's Bend Range
 *   (RI_CTL_LEVI_VBENDRNG) scaling the bend;
 * - a note that arrives on the remote channel reaches none of this.
 */
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "tests/helpers/ri_assert.h"
#include "engine/live.h"
#include "engine/engine.h"
#include "engine/seq/ctlplane.h"
#include "engine/seq/pattern.h"
#include "engine/seq/songtrack.h"
#include "midi_io/midi_chan.h"
#include "midi_io/midi_levi.h"
#include "engine/dsp/levi.h"

#define SR 48000.0f
#define PPQ 96u
#define BLOCK 256u
#define N (BLOCK * 96u)

static float s_l[N], s_r[N];
static struct RIEvent s_ev[64];

static struct RILiveSession s_ses;
static struct RIControlPlane s_ctl;
static struct RISongTrack s_tr;
static struct RIPatternBank s_bank;

/* Render `blocks` blocks, returning the energy of the last one. */
static uint32_t run(uint32_t blocks) {
    uint32_t b, i, k = 0u;
    for (b = 0u; b < blocks; b++) {
        uint32_t got = ri_live_render(&s_ses, s_l, s_r, BLOCK);
        if (!got)
            break;
    }
    for (i = 0u; i < N; i++)
        if (fabsf(s_l[i]) > 1e-6f)
            k++;
    return k;
}

/* Push one parsed message the way RIAPP does. */
static int push(const struct RIMidiChan *t, uint8_t st, uint8_t d1, uint8_t d2) {
    struct RIMidiLeviAction a;
    if (!midi_levi_message(t, st, d1, d2, &a))
        return 0;
    if (a.kind == RI_LEVI_ACT_PARAM)
        ri_ctl_send(&s_ctl, a.key, (uint8_t)a.val);
    else
        ri_ctl_send2(&s_ctl, a.key, (uint8_t)a.val, a.hi);   /* notes, bend, AT */
    return 1;
}

int main(void) {
    const struct RIPatternBank *b5[RI_SONGTRACK_INSTANCES];
    struct RIMidiChan t;
    uint32_t k, pat_sum;

    ri_bank_init(&s_bank, 4u, RI_PATTERN_KIND_LEVI, 0u);
    b5[0] = b5[1] = b5[2] = b5[3] = 0;
    b5[4] = &s_bank;
    ri_ctl_init(&s_ctl);
    ri_live_init(&s_ses, PPQ, SR, 120.0f, RI_ENGINE_SLEVI, s_ev, 64u);
    ri_live_set_banks(&s_ses, b5, &s_tr, 0);
    ri_live_set_ctl(&s_ses, &s_ctl);
    midi_chan_defaults(&t);
    ri_live_play(&s_ses);

    /* Silent until a live note arrives: nothing in the pattern plays it. */
    RI_ASSERT(run(16u) == 0u, "silent before any note");

    /* --- a note on makes sound, its note off stops it ------------------- */
    RI_ASSERT(push(&t, 0x91u, 60u, 100u) == 1, "note on is a Leviasynth message");
    RI_ASSERT(run(16u) > 0u, "the note sounds");
    /* A short release so the stop is observable inside the test. */
    ri_ctl_send(&s_ctl, RI_CTL_LEVI_RELEASE, 0u);
    RI_ASSERT(push(&t, 0x81u, 60u, 0u) == 1, "note off");
    RI_ASSERT(run(96u) == 0u, "the note stops");

    /* Velocity 0 on a note-on is a note off (MIDI 1.0), not a silent press. */
    RI_ASSERT(push(&t, 0x91u, 64u, 90u) == 1, "second note on");
    RI_ASSERT(run(8u) > 0u, "the second note sounds");
    RI_ASSERT(push(&t, 0x91u, 64u, 0u) == 1, "note on with velocity 0");
    RI_ASSERT(run(96u) == 0u, "velocity-0 note off stops it");

    /* --- performance signals ------------------------------------------- */
    /* Bend range 0 first: the default range makes a small bend small. */
    ri_ctl_send(&s_ctl, RI_CTL_LEVI_VBENDRNG, 24u);
    run(2u);
    RI_ASSERT(fabsf(s_ses.eng.slevi.bend) < 0.001f, "bend starts centred");
    RI_ASSERT(push(&t, 0xE1u, 0x00u, 0x60u) == 1, "pitch bend up");
    run(2u);
    RI_ASSERT(s_ses.eng.slevi.bend > 0.5f, "bend moved up (%f)",
        (double)s_ses.eng.slevi.bend);
    RI_ASSERT(push(&t, 0xE1u, 0x00u, 0x40u) == 1, "pitch bend centre (14-bit 8192)");
    run(2u);
    RI_ASSERT(fabsf(s_ses.eng.slevi.bend) < 0.001f, "bend back to centre");

    RI_ASSERT(push(&t, 0xD1u, 90u, 0u) == 1, "channel aftertouch");
    run(2u);
    RI_ASSERT(s_ses.eng.slevi.press == 90u, "channel pressure %u",
        (unsigned)s_ses.eng.slevi.press);

    RI_ASSERT(push(&t, 0xB1u, 1u, 100u) == 1, "mod wheel CC 1");
    run(2u);
    RI_ASSERT(s_ses.eng.slevi.wheel == 100u, "mod wheel %u",
        (unsigned)s_ses.eng.slevi.wheel);

    /* Poly aftertouch needs a voice: hold a note, then press it. */
    RI_ASSERT(push(&t, 0x91u, 67u, 100u) == 1, "note for poly aftertouch");
    run(8u);
    RI_ASSERT(push(&t, 0xA1u, 67u, 80u) == 1, "poly aftertouch");
    run(2u);
    pat_sum = 0u;
    for (k = 0u; k < RI_LEVI_NVOICES; k++)
        pat_sum += s_ses.eng.slevi.pat[k];
    RI_ASSERT(pat_sum == 80u, "poly pressure landed (%u)", pat_sum);
    RI_ASSERT(push(&t, 0x81u, 67u, 0u) == 1, "release the poly note");

    /* --- and none of this is reachable from the remote channel ---------- */
    RI_ASSERT(run(96u) == 0u, "silent again");
    RI_ASSERT(push(&t, 0x90u, 60u, 100u) == 0, "a note on channel 1 is not ours");
    RI_ASSERT(push(&t, 0xE0u, 0x00u, 0x40u) == 0, "bend on channel 1 is not ours");
    RI_ASSERT(run(16u) == 0u, "still silent");
    RI_ASSERT(fabsf(s_ses.eng.slevi.bend) < 0.001f, "bend untouched");

    ri_live_stop(&s_ses);
    RI_RESULT("levi-note");
}