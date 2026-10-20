/* t234_levi_independent — the Leviasynth map does not depend on the R5 rack.
 *
 * O2's evidence said the channel-2 pair was "unclaimed", and a reading of the
 * logs concluded the Leviasynth had stopped answering its own channel between
 * N1j and O2. It had not: `unclaimed` is the R5 NOTE-IN rack's word, and the
 * Leviasynth has a separate map that runs BEFORE the rack's funnel in
 * `midi_drain()` (`app/riapp.c`, `midi_levi_message()` first, then the
 * NOTEIN gate). The two answers are independent by construction, and this
 * test pins that independence across the settings shape that changed:
 *
 *   notech unset (N1j): the 303A claims channel 2, so the same note plays the
 *                       303 AND reaches the Leviasynth map;
 *   notech=1 (O2):       the 303A asks for the G7 remote's channel and is
 *                       REFUSED, so the note-in rack says "unclaimed" -- and
 *                       the same note STILL reaches the Leviasynth map.
 *
 * A future change that made the Levi path conditional on the rack (for
 * example "return early when the channel is unclaimed") fails section 2.
 *
 * What this cannot prove: `midi_drain()` itself is AROS shell code and does
 * not link on the host. The end-to-end order is measured in the O2 raw log;
 * this test is the pure half that says the two maps answer separately.
 */
#include <stdio.h>
#include <string.h>
#include "tests/helpers/ri_assert.h"
#include "midi_io/midi_bridge.h"
#include "midi_io/midi_chan.h"
#include "midi_io/midi_levi.h"
#include "midi_io/note_in.h"

static struct RIMidiSettings S;
static struct RIMidiChan T;
static struct RINoteIn N;

/* The exact setup order of `midi_setup()`: settings -> channel table ->
 * note-in binds. Binds are logged in the guest; here they are assertions. */
static void setup(unsigned note_ch) {
    uint32_t i;
    static const uint16_t devs[RI_NOTEIN_DEV_COUNT] = {
        RI_NOTEIN_DEV_303A, RI_NOTEIN_DEV_303B,
        RI_NOTEIN_DEV_808, RI_NOTEIN_DEV_909
    };
    midi_settings_defaults(&S);
    S.channel = 1u;                 /* the G7 remote, as every run had it */
    S.levi_ch = 2u;                 /* the Leviasynth, as every run had it */
    S.note_ch = (uint8_t)note_ch;   /* 0 = unset, otherwise 1-based */
    midi_chan_defaults(&T);
    RI_ASSERT(midi_chan_bind(&T, RI_MCHAN_REMOTE, S.channel) == 0,
        "the remote keeps channel 1");
    RI_ASSERT(midi_chan_bind(&T, RI_MCHAN_LEVI, S.levi_ch) == 0,
        "the Leviasynth keeps channel 2");
    ri_notein_init(&N);
    for (i = 0u; i < RI_NOTEIN_DEV_COUNT; i++) {
        int ch = midi_settings_notein_ch(&S, devs[i]);
        if (ch < 0)
            continue;
        ri_notein_bind(&N, (uint32_t)ch, devs[i]);
    }
}

/* The Leviasynth half of one message, as midi_drain() calls it. */
static int levi(uint8_t st, uint8_t d1, uint8_t d2,
    struct RIMidiLeviAction *a) {
    return midi_levi_message(&T, st, d1, d2, a);
}

int main(void) {
    struct RIMidiLeviAction la;

    /* --- 1. notech UNSET (N1j): the same channel is both ------------- */
    setup(0u);
    RI_ASSERT(!ri_notein_unclaimed(&N, 1u),
        "note_ch unset: the 303A is on channel 2 (nibble 1)");
    RI_ASSERT(levi(0x91u, 36u, 100u, &la) == 1 &&
        la.kind == RI_LEVI_ACT_NOTE && la.note == 36u && la.val == 100u &&
        la.on == 1u,
        "and the channel-2 note reaches the Leviasynth map");
    {
        struct RINoteInEvent ev;
        memset(&ev, 0, sizeof ev);
        ev.ch = 1u; ev.note = 36u; ev.vel = 100u; ev.on = 1u;
        RI_ASSERT(ri_notein_note(&N, &ev) == 0 && ev.resolved &&
            ev.dev == RI_NOTEIN_DEV_303A,
            "while the R5 rack resolves it to the 303A (both, not either)");
    }

    /* --- 2. notech=1 (O2): the rack refuses; the Levi map does not --- */
    setup(1u);
    RI_ASSERT(midi_settings_notein_ch(&S, RI_NOTEIN_DEV_303A) == 0,
        "note_ch=1 is nibble 0, the remote's channel");
    RI_ASSERT(ri_notein_unclaimed(&N, 1u),
        "so the 303A was REFUSED and channel 2 is unclaimed -- O2's line");
    RI_ASSERT(levi(0x91u, 36u, 100u, &la) == 1 &&
        la.kind == RI_LEVI_ACT_NOTE && la.note == 36u && la.on == 1u,
        "and the SAME channel-2 note still reaches the Leviasynth map");
    RI_ASSERT(levi(0x81u, 36u, 0u, &la) == 1 &&
        la.kind == RI_LEVI_ACT_NOTE && la.note == 36u && la.on == 0u,
        "its release reaches it too, as N1j's 'off' did");
    {
        struct RINoteInEvent ev;
        memset(&ev, 0, sizeof ev);
        ev.ch = 1u; ev.note = 36u; ev.vel = 100u; ev.on = 1u;
        RI_ASSERT(ri_notein_note(&N, &ev) != 0 && !ev.resolved,
            "the R5 rack refuses it (the 'unclaimed' counter), separately");
    }

    /* --- 3. notech=3: both directions on free channels ---------------- */
    setup(3u);
    RI_ASSERT(!ri_notein_unclaimed(&N, 2u),
        "note_ch=3 binds the 303A on nibble 2");
    RI_ASSERT(levi(0x92u, 36u, 100u, &la) == 0,
        "a 303 note on channel 3 is NOT a Leviasynth note");
    RI_ASSERT(levi(0x91u, 36u, 100u, &la) == 1 &&
        la.kind == RI_LEVI_ACT_NOTE,
        "and a channel-2 note is the Leviasynth's, not the 303's");
    {
        struct RINoteInEvent ev;
        memset(&ev, 0, sizeof ev);
        ev.ch = 2u; ev.note = 36u; ev.vel = 100u; ev.on = 1u;
        RI_ASSERT(ri_notein_note(&N, &ev) == 0 && ev.resolved &&
            ev.dev == RI_NOTEIN_DEV_303A,
            "the channel-3 note is the 303A's (NOTECH=3 works inbound)");
    }

    /* --- 4. the remote channel is never the Leviasynth's -------------- */
    setup(1u);
    RI_ASSERT(levi(0x90u, 36u, 100u, &la) == 0,
        "a channel-1 note belongs to the G7 remote, not the Levi map");

    RI_RESULT("levi-independent");
}
