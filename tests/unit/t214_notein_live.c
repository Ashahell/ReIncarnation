/* t214_notein_live — R5d: the pump meets the live window.
 *
 * R5b's pump stamps at `e->cursor`. That is right for a one-shot renderer and
 * **wrong for the live session**, and R5b's test could not see it because it
 * never had a caller.
 *
 * THE LAW: `ri_live_render` windows every event to `[s0, s1)` where
 *
 *     s0 = s->sample_cursor      s1 = s0 + frames
 *
 * and **DROPS anything outside it** before `ri_engine_load`. It also rebases
 * survivors by `-s0`. So a live note must be stamped at an absolute sample
 * INSIDE that window, and the only such sample the live session can name is
 * `s0`.
 *
 * **WHY THE ORIGINAL WAS A SILENT FAILURE.** `ri_engine_load` sets
 * `e->cursor = 0` on every call, and the live session reloads the engine once
 * per buffer. So `e->cursor` is 0 at every pump, and sample 0 is inside
 * `[s0, s1)` **only for the first buffer**. Every later buffer's live notes
 * would be stamped below `s0` and **dropped by the window filter** — no
 * error, no count, no sound. The first note would play and the rest would
 * vanish, which is the shape of a defect that gets blamed on the MIDI input.
 *
 * **SO THE STAMP IS THE CALLER'S, AND THE ENGINE POINTER IS GONE.** There is
 * no "which cursor" question left to answer at the type level: two call sites
 * with different absolutes cannot be confused if neither can reach the
 * other's.
 *
 * **SEQ COMES FROM THE SESSION'S ONE COUNTER.** The §8 key ends in `seq`, and
 * every other producer in `ri_live_render` draws from `s->seq_next`. A pump
 * that invented its own sequence space would put a live note either ahead of
 * or behind every pattern event at the same sample for a reason that is an
 * accident of number size rather than a decision.
 */
#include <stdio.h>
#include <string.h>
#include "tests/helpers/ri_assert.h"
#include "engine/engine.h"
#include "midi_io/note_in.h"
#include "engine/live.h"
#include "engine/seq/songtrack.h"
#include "engine/seq/pattern.h"
#include "midi_io/note_in_app.h"

static struct RINoteIn NI;
static struct RINoteLive LIVE;
static struct RIEvent OUT[16];

/* A silent-but-present 808 bank, so the session's tick window is non-empty
 * and `s0` advances. The notes in it do not matter: what is under test is
 * the EXTERNAL note, and a silent pattern is what makes its contribution
 * unambiguous. */
static struct RIPatternBank BANK808;
static const struct RIPatternBank *s_b808[RI_SONGTRACK_INSTANCES] = { &BANK808 };
static struct RISongTrack ltrack;
/* THE SESSION'S OWN SCRATCH. I passed NULL at first, and that silently
 * disabled the very producer under test: `scratch_cap` is 0 when the array
 * is NULL, so every `n < s->scratch_cap` guard is false and the live note is
 * never even offered. A test that measured "no sound" could not tell a
 * refused note from a producer that never ran. */
static struct RIEvent LSCRATCH[512];

/* THE LIVE WINDOW, verbatim from engine/live.c. Reproduced here rather than
 * called, because the point is what the window DOES to a mis-stamped event
 * and a helper that did it would be a helper under test. */
static uint32_t window_filter(uint64_t s0, uint64_t frames,
    struct RIEvent *ev, uint32_t n) {
    uint32_t w = 0u, r;
    for (r = 0u; r < n; r++) {
        uint64_t a = ev[r].sample;
        if (a < s0 || a >= s0 + (uint64_t)frames)
            continue;
        ev[w] = ev[r];
        ev[w].sample = a - s0;
        w++;
    }
    return w;
}

static void setup(void) {
    ri_notein_init(&NI);
    ri_notein_live_init(&LIVE);
    ri_notein_bind(&NI, 1u, RI_NOTEIN_DEV_303A);
    ri_notein_bind(&NI, 10u, RI_NOTEIN_DEV_808);
}

/* Push into a caller-supplied pair, so the live case above can use its own
 * bindings rather than the file-scope ones. */
static void hit_in(struct RINoteIn *ni, struct RINoteLive *l, uint32_t ch,
    uint32_t note, uint32_t on) {
    struct RINoteInEvent ev;
    memset(&ev, 0, sizeof ev);
    ev.ch = (uint8_t)ch; ev.note = (uint8_t)note; ev.vel = 100u;
    ev.on = (uint8_t)on;
    if (ri_notein_note(ni, &ev) == 0)
        ri_notein_push(l, &ev);
}

static void hit(uint32_t ch, uint32_t note, uint32_t on) {
    struct RINoteInEvent ev;
    memset(&ev, 0, sizeof ev);
    ev.ch = (uint8_t)ch; ev.note = (uint8_t)note; ev.vel = 100u;
    ev.on = (uint8_t)on;
    if (ri_notein_note(&NI, &ev) == 0)
        ri_notein_push(&LIVE, &ev);
}

int main(void) {
    uint32_t got, kept;

    /* --- 1. A NOTE STAMPED AT s0 SURVIVES THE WINDOW ------------------ */
    setup();
    hit(10u, 36u, 1u);
    hit(1u, 60u, 1u);
    got = ri_notein_pump(&LIVE, 256000u, 7u, OUT, 16u);
    RI_ASSERT(got == 2u, "two pumped (%u)", (unsigned)got);
    RI_ASSERT(OUT[0].sample == 256000u && OUT[1].sample == 256000u,
        "both stamped at the window origin (%llu, %llu)",
        (unsigned long long)OUT[0].sample,
        (unsigned long long)OUT[1].sample);
    kept = window_filter(256000u, 256u, OUT, got);
    RI_ASSERT(kept == 2u, "and the window KEEPS them (%u)", (unsigned)kept);
    RI_ASSERT(OUT[0].sample == 0u && OUT[1].sample == 0u,
        "rebased to 0 for the engine (%llu, %llu)",
        (unsigned long long)OUT[0].sample,
        (unsigned long long)OUT[1].sample);

    /* --- 2. THE OLD STAMP IS DROPPED SILENTLY ------------------------- */
    /* The whole reason the signature changed. Sample 0 with s0 = 256000 is
     * BELOW the window, so the filter discards it with no error and no
     * count -- the first buffer would sound and every later one would not. */
    setup();
    hit(10u, 36u, 1u);
    got = ri_notein_pump(&LIVE, 0u, 1u, OUT, 16u);
    RI_ASSERT(got == 1u, "pumped at sample 0 (%u)", (unsigned)got);
    kept = window_filter(256000u, 256u, OUT, got);
    RI_ASSERT(kept == 0u,
        "and the window at 256000 DROPS it (%u) -- silently", (unsigned)kept);

    /* --- 3. A STAMP PAST THE END IS ALSO DROPPED --------------------- */
    setup();
    hit(10u, 36u, 1u);
    got = ri_notein_pump(&LIVE, 256000u + 256u, 1u, OUT, 16u);
    RI_ASSERT(got == 1u, "pumped at the far edge (%u)", (unsigned)got);
    kept = window_filter(256000u, 256u, OUT, got);
    RI_ASSERT(kept == 0u, "one sample past the end is outside (%u)",
        (unsigned)kept);

    /* --- 4. SEQ COMES FROM THE CALLER, ASCENDING ---------------------- */
    /* Every other producer in ri_live_render draws from s->seq_next. A pump
     * that invented its own numbers would order a live note against a
     * pattern event for an accident of magnitude. */
    setup();
    hit(1u, 60u, 1u);
    hit(1u, 62u, 1u);
    hit(1u, 64u, 1u);
    got = ri_notein_pump(&LIVE, 1000u, 41u, OUT, 16u);
    RI_ASSERT(got == 3u, "three pumped (%u)", (unsigned)got);
    RI_ASSERT(OUT[0].seq == 41u && OUT[1].seq == 42u && OUT[2].seq == 43u,
        "seq ascends from the caller's base: %u %u %u",
        (unsigned)OUT[0].seq, (unsigned)OUT[1].seq, (unsigned)OUT[2].seq);
    /* So the caller can advance its counter by the count and the next
     * producer's seq cannot collide with a note that has already gone. */
    RI_ASSERT(OUT[2].seq == 41u + got - 1u,
        "and the last is base + count - 1 (%u)", (unsigned)OUT[2].seq);

    /* --- 5. A PARTIAL PUMP LEAVES THE REST, STAMPED NEXT TIME ------- */
    /* The next buffer re-pumps at the NEXT s0, so a note that did not fit
     * is stamped then -- not now, and not lost. */
    setup();
    for (uint32_t i = 0; i < 3; i++)
        hit(1u, (uint32_t)(60u + i), 1u);
    got = ri_notein_pump(&LIVE, 1000u, 0u, OUT, 2u);
    RI_ASSERT(got == 2u, "two of three (%u)", (unsigned)got);
    RI_ASSERT(LIVE.queued == 1u, "one waits (%u)", (unsigned)LIVE.queued);
    got = ri_notein_pump(&LIVE, 1256u, 2u, OUT, 16u);
    RI_ASSERT(got == 1u, "the rest next buffer (%u)", (unsigned)got);
    RI_ASSERT(OUT[0].sample == 1256u, "stamped at the NEXT window (%llu)",
        (unsigned long long)OUT[0].sample);

    /* --- 6. NULLs ----------------------------------------------------- */
    RI_ASSERT(ri_notein_pump(0, 0u, 0u, OUT, 16u) == 0u, "NULL ring");
    RI_ASSERT(ri_notein_pump(&LIVE, 0u, 0u, 0, 16u) == 0u, "NULL out");
    RI_ASSERT(ri_notein_pump(&LIVE, 0u, 0u, OUT, 0u) == 0u, "zero capacity");

    /* --- 6b. THROUGH ri_live_render, ON A LATE BUFFER ----------------- */
    /* THE TEST THAT WOULD HAVE CAUGHT IT. Everything above is arithmetic on
     * a copied window filter; this is the real thing. A live note is played
     * on a buffer that is NOT the first -- which is every buffer after the
     * first -- and it has to be AUDIBLE. A cursor stamp would pass §1 and
     * vanish here, which is why the defect survived R5b's whole test.
     */
    {
        static struct RILiveSession LS;
        static float OL[256], OR[256];
        static struct RINoteIn LNI;
        static struct RINoteLive LL;
        uint32_t k, quiet;
        /* A BANK IS NEEDED, or `t1` never leaves 0 and the window is empty
         * on every buffer -- which is itself the reason the first version of
         * this test measured `s0 = 0` and could not tell a dropped note from
         * a session that had not started. An empty session and a wrongly
         * stamped note are the same silence. */
        ri_live_init(&LS, 96u, 48000u, 120u, RI_ENGINE_S808, LSCRATCH, 512u);
        ri_live_set_banks(&LS, s_b808, &ltrack, 0);
        ri_bank_init(&BANK808, 2u, RI_PATTERN_KIND_DRUM, RI_DRUM_CLASS_808);
        ri_track_init(&ltrack);
        ri_live_play(&LS);
        /* Advance the session so s0 is comfortably non-zero. A live note at
         * sample 0 is INSIDE the first window and outside every later one. */
        for (k = 0u; k < 4u; k++)
            ri_live_render(&LS, OL, OR, 256u);
        RI_ASSERT(LS.sample_cursor > 0u,
            "the session has moved on (s0 = %llu)",
            (unsigned long long)LS.sample_cursor);
        ri_notein_init(&LNI);
        ri_notein_live_init(&LL);
        ri_notein_bind(&LNI, 10u, RI_NOTEIN_DEV_808);
        ri_live_notein(&LS, &LL);
        hit_in(&LNI, &LL, 10u, 36u, 1u);
        memset(OL, 0, sizeof OL);
        memset(OR, 0, sizeof OR);
        RI_ASSERT(ri_live_render(&LS, OL, OR, 256u) == 256u, "rendered");
        quiet = 0u;
        for (k = 0u; k < 256u; k++)
            if (OL[k] == 0.0f && OR[k] == 0.0f)
                quiet++;
        RI_ASSERT(quiet < 256u,
            "the note SOUNDED on a late buffer (%u of 256 samples silent)",
            (unsigned)quiet);
    }

    /* --- 6c. THE SESSION'S seq_next ADVANCES PAST THE LIVE NOTES ----- */
    /* SD SURVIVED because nothing here looked at seq_next. If it does not
     * advance, the NEXT producer's first event takes a seq a live note
     * already used -- two different events compare equal under the §8 key,
     * and their order is then whatever the sort happens to do. That is the
     * same "one sequence space" law R5b wrote and this is where it is
     * actually kept. */
    {
        static struct RILiveSession LS2;
        static float OL2[256], OR2[256];
        static struct RINoteIn NI2;
        static struct RINoteLive L2;
        uint32_t before, after;
        ri_live_init(&LS2, 96u, 48000u, 120u, RI_ENGINE_S808, LSCRATCH, 512u);
        ri_live_set_banks(&LS2, s_b808, &ltrack, 0);
        ri_live_play(&LS2);
        ri_live_render(&LS2, OL2, OR2, 256u);
        ri_notein_init(&NI2);
        ri_notein_live_init(&L2);
        ri_notein_bind(&NI2, 10u, RI_NOTEIN_DEV_808);
        ri_live_notein(&LS2, &L2);
        hit_in(&NI2, &L2, 10u, 36u, 1u);
        hit_in(&NI2, &L2, 10u, 38u, 1u);
        before = LS2.seq_next;
        RI_ASSERT(ri_live_render(&LS2, OL2, OR2, 256u) == 256u, "rendered");
        after = LS2.seq_next;
        RI_ASSERT(after >= before + 2u,
            "seq_next advanced past TWO live notes (%u -> %u)",
            (unsigned)before, (unsigned)after);
        /* And the next pump continues from the session's counter rather than
         * restarting. The ring is EMPTY here -- the render drained it -- so
         * the note has to be pushed again; my first version assumed it was
         * still waiting and pumped zero. */
        {
            static struct RIEvent O2[4];
            RI_ASSERT(L2.queued == 0u,
                "the live render drained the ring (%u)", (unsigned)L2.queued);
            /* GM 38, not 39. 39 has no instrument in this build's partial
             * map, so it is refused and correctly never queues -- and my
             * first version of this line used 39 and read the refusal as a
             * broken pump. The refusal was right; the expectation was not. */
            hit_in(&NI2, &L2, 10u, 38u, 1u);
            RI_ASSERT(ri_notein_pump(&L2, 9999u, after, O2, 4u) == 1u,
                "the next note pumps");
            RI_ASSERT(O2[0].seq >= after,
                "carrying the session's counter forward (%u >= %u)",
                (unsigned)O2[0].seq, (unsigned)after);
        }
    }

    /* --- 7. A REFUSED NOTE NEVER REACHES THE PUMP -------------------- */
    setup();
    hit(10u, 36u, 1u);
    hit(10u, 36u, 0u);          /* a drum release: refused in R5a */
    got = ri_notein_pump(&LIVE, 1000u, 0u, OUT, 16u);
    RI_ASSERT(got == 1u, "only the note-on is pumped (%u)", (unsigned)got);

    RI_RESULT("note-in-live");
}