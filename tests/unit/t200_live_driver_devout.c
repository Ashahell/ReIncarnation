/* t200_live_driver_devout — R6d: the render's notes onto the wire.
 *
 * R6c turned one event into bytes and R6d's tap recorded them; this is the
 * part that actually ships them. It is the first test in the R6 chain that
 * touches `live_driver.c`, and the laws here are about the SEAM rather than
 * about MIDI:
 *
 *  - THE DRAIN IS OFF BY DEFAULT AND OFF MEANS NOTHING LEAVES THE TAP.
 *   Not "the tap fills and nobody reads it": every caller today has
 *   `dev_out == NULL`, and a drain that ran anyway would cost the audio path
 *   a ring walk per block for a feature nobody turned on.
 *
 *  - AN UNASSIGNED CHANNEL REFUSES, AND IT REFUSES BEFORE `ri_devout_note`.
 *   That function CLAMPS a channel above 15 rather than refusing it, which
 *   is right at its own boundary (channel 16 must not wrap onto the G7
 *   remote) and catastrophic here: an unassigned "channel 255" would clamp
 *   to 15 and put every note on an instrument nobody chose. So the drain
 *   checks its own sentinel and counts the refusal itself.
 *
 *  - THE LATE ACCENT EMITS NOTHING AND IS COUNTED. A total accent is applied
 *   retroactively to notes already sent; MIDI has no way to make a sent note
 *   louder. The counter is the point: an accuracy limit nobody can measure
 *   is a limit nobody can decide about.
 *
 *  - A NOTE IS ALL-OR-NOTHING INTO THE RING. `midi_out`'s own
 *   `put_run()` already refuses a partial 3-byte message; the drain must go
 *   through that path rather than pushing bytes itself, or a note could be
 *   half-written under backpressure and the DAW would assemble a note from a
 *   stale byte and a fresh one.
 *
 *  - A BOUNDED DRAIN. The tap is bounded, but a caller that stopped
 *   draining it must not turn into unbounded per-block work.
 */
#include <stdio.h>
#include <string.h>
#include "tests/helpers/ri_assert.h"
#include "app/core/live_driver.h"
#include "engine/live.h"
#include "engine/seq/pattern.h"
#include "engine/seq/songtrack.h"
#include "engine/dsp/rb808.h"
#include "engine/dsp/rb909.h"
#include "midi_io/midi_devout.h"
#include "midi_io/midi_out.h"

#define SR     48000u
#define FRAMES 256u
#define CH     5u

static struct RILiveDriver s_drv;
static struct RILiveSession s_ses;
static struct RISongTrack s_tr;
static struct RIPatternBank s_bank;
static struct RIEvent s_ev[512];
static struct RIMidiOut s_out;
static struct RIDevOut s_dev;
static int16_t s_pcm[FRAMES * 2u];
static float s_fl[FRAMES], s_fr[FRAMES];
static uint8_t s_wire[4096];

static void fixture(void) {
    const struct RIPatternBank *b5[5];
    uint32_t q;
    memset(&s_drv, 0, sizeof s_drv);
    memset(&s_ses, 0, sizeof s_ses);
    ri_bank_init(&s_bank, 4u, RI_PATTERN_KIND_LEVI, 0u);
    for (q = 0u; q < 32u; q++)
        ri_pattern_set_length(&s_bank.pat[q], 16u);
    b5[0] = b5[1] = b5[2] = b5[3] = 0;
    b5[4] = &s_bank;
    ri_live_init(&s_ses, 96u, (float)SR, 120.0f, RI_ENGINE_SLEVI, s_ev, 512u);
    ri_live_set_banks(&s_ses, b5, &s_tr, 0u);
    ri_livedrv_init(&s_drv, &s_ses, FRAMES, 0);
    midi_out_init(&s_out, SR, 120000u, 0u);
    ri_devout_init(&s_dev, 1);
    ri_devout_assign(&s_dev, 0u, CH);
    s_drv.note_ch = -1;              /* unassigned: the fail-closed default */
}

/* Inject one engine event. The drain reads the session's engine, so this is
 * how a note gets into the tap without building a whole song. */
static void ev(uint32_t type, uint32_t dev, uint32_t voice, uint32_t value,
    uint32_t flags) {
    struct RIEvent e;
    memset(&e, 0, sizeof e);
    e.type = (uint16_t)type;
    e.device = (uint16_t)dev;
    e.voice = (uint16_t)voice;
    e.value = value;
    e.flags = (uint16_t)flags;
    ri_engine_apply_event(&s_ses.eng, &e);
}

static uint32_t render(void) {
    ri_livedrv_render(&s_drv, s_pcm, s_fl, s_fr, FRAMES);
    return midi_out_read(&s_out, s_wire, sizeof s_wire);
}

/* The attached feature: notes on, ring on. */
static void attach(void) {
    midi_out_enable(&s_out, 1);
    s_drv.clk_out = &s_out;
    s_drv.dev_out = &s_dev;
    s_drv.note_ch = (int)CH;
}

int main(void) {
    uint32_t n;

    /* --- NULL dev_out: the tap keeps everything, the ring gets nothing -- */
    fixture();
    s_drv.clk_out = &s_out;
    midi_out_enable(&s_out, 1);
    s_drv.note_ch = (int)CH;
    ev(RI_EV_NOTE_ON, 0u, 0u, 46u, 0u);
    ev(RI_EV_NOTE_ON, 2u, 5u, 5u, 0u);
    n = render();
    RI_ASSERT(n == 0u, "no dev_out means no bytes (%u)", (unsigned)n);
    RI_ASSERT(ri_engine_note_pending(&s_ses.eng) == 2u,
        "and the tap still holds them for whoever does attach (%u)",
        (unsigned)ri_engine_note_pending(&s_ses.eng));
    RI_ASSERT(ri_devout_emitted(&s_dev) == 0u, "nothing was emitted");

    /* --- DRUMS DO NOT NEED A MELODIC CHANNEL ------------------------ */
    /* GM defines percussion on channel 10, so the drum channel is not a
     * choice, and making the user configure a melodic channel before the
     * 808 can be heard is a step that buys nothing. The melodic path is
     * still refused below -- this must not quietly make an unassigned
     * melodic channel legal. */
    fixture();
    s_drv.clk_out = &s_out;
    midi_out_enable(&s_out, 1);
    s_drv.dev_out = &s_dev;
    s_drv.note_ch = -1;                  /* still unassigned */
    ev(RI_EV_NOTE_ON, 2u, 5u, 5u, 0u);  /* an 808 hit */
    n = render();
    RI_ASSERT(n == 3u, "a drum reaches the wire with no melodic channel (%u)",
        (unsigned)n);
    RI_ASSERT(s_wire[0] == (uint8_t)(0x90u | RI_DEVOUT_GM_PERCUSSION),
        "on GM channel 10 (%02X)", (unsigned)s_wire[0]);
    RI_ASSERT(s_drv.devout_refused == 0u,
        "and nothing was refused (%lu)", (unsigned long)s_drv.devout_refused);
    ev(RI_EV_NOTE_ON, 0u, 0u, 46u, 0u);  /* a 303 note, same state */
    n = render();
    RI_ASSERT(n == 0u, "but a melodic note with no channel still does not (%u)",
        (unsigned)n);
    RI_ASSERT(s_drv.devout_refused == 1u, "and IS refused (%lu)",
        (unsigned long)s_drv.devout_refused);

    /* --- attached, UNASSIGNED channel: refused, and counted ----------- */
    /* s_drv.note_ch is still -1. This must NOT reach ri_devout_note, which
     * clamps a channel above 15 instead of refusing it -- 255 would clamp
     * to 15 and put the note on an instrument nobody chose. */
    fixture();
    s_drv.clk_out = &s_out;
    midi_out_enable(&s_out, 1);
    s_drv.dev_out = &s_dev;
    ev(RI_EV_NOTE_ON, 0u, 0u, 46u, 0u);
    n = render();
    RI_ASSERT(n == 0u, "an unassigned channel sends nothing (%u)", (unsigned)n);
    RI_ASSERT(ri_devout_emitted(&s_dev) == 0u, "and emits nothing");
    RI_ASSERT(s_drv.devout_refused == 1u,
        "and the refusal is COUNTED, not swallowed (%lu)",
        (unsigned long)s_drv.devout_refused);

    /* --- attached and assigned: the bytes ----------------------------- */
    fixture();
    attach();
    ev(RI_EV_NOTE_ON, 0u, 0u, 46u, 0u);
    n = render();
    RI_ASSERT(n == 3u, "a 303 note is three bytes (%u)", (unsigned)n);
    RI_ASSERT(s_wire[0] == (uint8_t)(0x90u | CH),
        "note-on on the ASSIGNED channel %u (%02X)", (unsigned)CH,
        (unsigned)s_wire[0]);
    RI_ASSERT(s_wire[1] == 46u, "the engine's note (%u)", (unsigned)s_wire[1]);
    RI_ASSERT(s_wire[2] == ri_devout_velocity(0u), "plain velocity (%u)",
        (unsigned)s_wire[2]);

    /* --- accent is velocity ------------------------------------------ */
    fixture();
    attach();
    ev(RI_EV_NOTE_ON, 0u, 0u, 46u, RI_EVFLAG_ACCENT);
    n = render();
    RI_ASSERT(n == 3u && s_wire[2] == ri_devout_velocity(RI_EVFLAG_ACCENT),
        "an accent is louder on the wire too (%u vs %u)", (unsigned)s_wire[2],
        (unsigned)ri_devout_velocity(RI_EVFLAG_ACCENT));

    /* --- the drum path, THROUGH THE USER'S SLOT MAP -------------------- */
    /* Lane 5 default is the rim shot. Move it to the kick: the wire must
     * follow, because the resolution happened in the engine and the drain
     * was handed a sound, not a lane. */
    fixture();
    attach();
    s_ses.eng.s808.slot[5] = RB808_BD;
    ev(RI_EV_NOTE_ON, 2u, 5u, 5u, 0u);
    n = render();
    RI_ASSERT(n == 3u, "an 808 hit is three bytes (%u)", (unsigned)n);
    RI_ASSERT(s_wire[1] == 36u,
        "and it is the sound the slot map NOW holds, bass drum 36 (%u)",
        (unsigned)s_wire[1]);

    fixture();
    attach();
    s_ses.eng.s808.slot[5] = RB808_RS;
    ev(RI_EV_NOTE_ON, 2u, 5u, 5u, 0u);
    n = render();
    RI_ASSERT(n == 3u && s_wire[1] == 37u,
        "moving it back moves the wire back, rim shot 37 (%u)",
        (unsigned)s_wire[1]);

    /* --- the 909 through its static voice table ---------------------- */
    fixture();
    attach();
    ev(RI_EV_NOTE_ON, 3u, 3u, 3u, 0u);
    n = render();
    RI_ASSERT(n == 3u, "a 909 hit is three bytes (%u)", (unsigned)n);
    RI_ASSERT(s_wire[1] == ri_devout_drum909_note(RI_LANE_TO_RB909_VOICE[3]),
        "and carries the resolved voice's note (%u vs %u)", (unsigned)s_wire[1],
        (unsigned)ri_devout_drum909_note(RI_LANE_TO_RB909_VOICE[3]));

    /* --- a slide is legato and emits NOTHING ------------------------- */
    fixture();
    attach();
    ev(RI_EV_NOTE_ON, 0u, 0u, 40u, 0u);
    ev(RI_EV_NOTE_CONTINUE, 0u, 0u, 42u, RI_EVFLAG_SLIDE);
    n = render();
    RI_ASSERT(n == 3u, "only the note-on reaches the wire (%u)", (unsigned)n);
    RI_ASSERT(s_wire[1] == 40u, "and the slide did not re-attack at 42 (%u)",
        (unsigned)s_wire[1]);
    RI_ASSERT(ri_devout_legato(&s_dev) == 1u,
        "the slide is COUNTED as a legato (%u)",
        (unsigned)ri_devout_legato(&s_dev));

    /* --- a release is a real 0x8n ------------------------------------ */
    fixture();
    attach();
    ev(RI_EV_NOTE_ON, 0u, 0u, 46u, 0u);
    ev(RI_EV_NOTE_OFF, 0u, 0u, 46u, 0u);
    n = render();
    RI_ASSERT(n == 6u, "note-on and note-off are both sent (%u)", (unsigned)n);
    RI_ASSERT(s_wire[0] == (uint8_t)(0x90u | CH), "on first (%02X)",
        (unsigned)s_wire[0]);
    RI_ASSERT(s_wire[3] == (uint8_t)(0x80u | CH),
        "then a REAL 0x8n, not a velocity-0 note-on (%02X)", (unsigned)s_wire[3]);

    /* --- the LATE ACCENT: nothing on the wire, counted ---------------- */
    fixture();
    attach();
    ev(RI_EV_NOTE_ON, 2u, 5u, 5u, 0u);
    ev(RI_EV_ACCENT, 2u, RI_VOICE_ALL, 0u, 0u);
    n = render();
    RI_ASSERT(n == 3u, "the accent itself puts no byte on the wire (%u)",
        (unsigned)n);
    RI_ASSERT(s_drv.late_accents == 1u,
        "but it IS counted, so the limit is measurable (%lu)",
        (unsigned long)s_drv.late_accents);

    /* --- ordering: the note, then the late accent, then the next note -- */
    fixture();
    attach();
    ev(RI_EV_NOTE_ON, 0u, 0u, 46u, 0u);
    ev(RI_EV_ACCENT, 2u, RI_VOICE_ALL, 0u, 0u);
    ev(RI_EV_NOTE_ON, 0u, 0u, 47u, 0u);
    n = render();
    RI_ASSERT(n == 6u, "two notes, accent between, two of three (%u)",
        (unsigned)n);
    RI_ASSERT(s_wire[1] == 46u && s_wire[4] == 47u,
        "in order, with the accent as silence between them (%u, %u)",
        (unsigned)s_wire[1], (unsigned)s_wire[4]);

    /* --- internal events never reach the wire ------------------------- */
    fixture();
    attach();
    ev(RI_EV_FLAM, 3u, 3u, 4u, 0u);
    ev(RI_EV_PARAM, 0u, 0u, 7u, 0u);
    ev(RI_EV_METER, 0u, 0u, 1u, 0u);
    n = render();
    RI_ASSERT(n == 0u, "flam, param and meter are silent on the wire (%u)",
        (unsigned)n);

    /* --- BACKPRESSURE: the note goes in WHOLE, or not at all ----------- */
    /* Fill the ring to one byte short, then push a three-byte note.
     *
     * My first version asserted it would be REFUSED, and that was me
     * describing a design midi_out does not have. `put_run()`'s actual law
     * is drop-oldest-to-make-room and then write the whole message: it
     * would rather lose two stale clock bytes than lose a note that just
     * played, and it would rather refuse outright than half-write one --
     * a half-written note is assembled by the receiver out of a stale byte
     * and a fresh one. Both halves of that are worth pinning, so: the note
     * lands whole, the two oldest go, and the drop is counted. */
    fixture();
    midi_out_enable(&s_out, 1);   /* a DISABLED ring refuses every write */
    {
        uint8_t one = 0xF8u;
        uint8_t note[3];
        uint32_t dropped_before;
        note[0] = (uint8_t)(0x90u | CH);
        note[1] = 46u;
        note[2] = 64u;
        /* THE RING RESERVES ONE SLOT. Occupancy is (head - tail) & (CAP-1),
         * so "full" is 255 and not 256: head and tail must never meet, or
         * an empty ring reads as full. Filling to 256 and expecting the
         * write to be refused is a test written against the wrong capacity,
         * and it is how a ring silently loses its last slot. */
        while (midi_out_pending(&s_out) < RI_MIDIOUT_CAP - 1u)
            midi_out_put(&s_out, &one, 1u);
        RI_ASSERT(midi_out_pending(&s_out) == RI_MIDIOUT_CAP - 1u,
            "the ring is full at CAP-1, not CAP (%u of %u)",
            (unsigned)midi_out_pending(&s_out), (unsigned)RI_MIDIOUT_CAP);
        dropped_before = midi_out_dropped(&s_out);
        RI_ASSERT(midi_out_put(&s_out, note, 3u) == 3u,
            "the note is written WHOLE, not refused and not truncated");
        RI_ASSERT(midi_out_pending(&s_out) == RI_MIDIOUT_CAP - 1u,
            "still exactly full -- the write took the room it made (%u)",
            (unsigned)midi_out_pending(&s_out));
        RI_ASSERT(midi_out_dropped(&s_out) == dropped_before + 3u,
            "and the three oldest bytes it displaced are COUNTED (%u -> %u)",
            (unsigned)dropped_before, (unsigned)midi_out_dropped(&s_out));
        /* And the note is intact at the TAIL: what was displaced was the
         * two oldest, not part of the message. */
        {
            static uint8_t all[512];
            uint32_t got = midi_out_read(&s_out, all, sizeof all);
            uint32_t last = (got ? got : 1u) - 1u;
            RI_ASSERT(got == RI_MIDIOUT_CAP - 1u,
                "the whole ring drains (%u)", (unsigned)got);
            RI_ASSERT(all[last - 2u] == note[0] && all[last - 1u] == note[1]
                && all[last] == note[2],
                "and the note is intact at the tail (%02X %02X %02X)",
                (unsigned)all[last - 2u], (unsigned)all[last - 1u],
                (unsigned)all[last]);
        }
        /* A message LONGER THAN THE RING is refused whole, not truncated.
         * (a partial SPP is a locate assembled from a stale byte.) */
        {
            static uint8_t big[300];
            memset(big, 0x90u, sizeof big);
            RI_ASSERT(midi_out_put(&s_out, big, sizeof big) == 0u,
                "a message longer than the ring is refused, not truncated");
        }
        /* WHICH BYTES GO is the whole of put_run's drop policy, and reading
         * the ring back is the only way to see it. My first version filled
         * the ring with identical clock bytes, which made dropping the
         * oldest and overwriting the newest produce THE SAME 255 BYTES:
         * both left 252 fills and a note at the tail, and a mutant that
         * moves the loss from the oldest to the newest survived. The fill
         * has to be non-uniform to see the difference at all. */
        fixture();
        midi_out_enable(&s_out, 1);
        {
            static uint8_t all[512];
            uint8_t note[3];
            uint32_t got;
            note[0] = (uint8_t)(0x90u | CH);
            note[1] = 46u;
            note[2] = 64u;
            while (midi_out_pending(&s_out) < RI_MIDIOUT_CAP - 1u) {
                uint8_t seq = (uint8_t)(midi_out_pending(&s_out) & 0x7Fu);
                midi_out_put(&s_out, &seq, 1u);
            }
            RI_ASSERT(midi_out_put(&s_out, note, 3u) == 3u, "the note lands");
            got = midi_out_read(&s_out, all, sizeof all);
            RI_ASSERT(got == RI_MIDIOUT_CAP - 1u, "the ring drains (%u)",
                (unsigned)got);
            RI_ASSERT(all[0] == 3u,
                "the THREE OLDEST went: the first survivor is slot 3, not "
                "slot 0 (got %u)", (unsigned)all[0]);
            RI_ASSERT(all[1] == 4u && all[2] == 5u,
                "and the rest of the fill is undisturbed (%u, %u)",
                (unsigned)all[1], (unsigned)all[2]);
            RI_ASSERT(all[got - 3u] == note[0] && all[got - 2u] == note[1]
                && all[got - 1u] == note[2],
                "with the note intact at the tail");
        }
        /* AND THE INNER `need >= CAP-1` GUARD DROPS NOTHING WHEN IT REFUSES.
         * This is CAP-1, not CAP: the up-front length guard already caught
         * the ring-length message, so a mutant that made the inner guard
         * "salvage" the shortfall by dropping 254 bytes instead of
         * refusing still returned 0 -- while silently discarding a
         * caller's clock on the way out. A return value alone is not enough
         * to pin a drop policy. */
        {
            static uint8_t big2[RI_MIDIOUT_CAP - 1u];
            uint32_t d0;
            while (midi_out_pending(&s_out) < RI_MIDIOUT_CAP - 1u) {
                uint8_t one = 0xF8u;
                midi_out_put(&s_out, &one, 1u);
            }
            memset(big2, 0x90u, sizeof big2);
            d0 = midi_out_dropped(&s_out);
            RI_ASSERT(midi_out_put(&s_out, big2, sizeof big2) == 0u,
                "a full-ring-length message is refused");
            RI_ASSERT(midi_out_dropped(&s_out) == d0,
                "and refusing it dropped NOTHING (%u -> %u)", (unsigned)d0,
                (unsigned)midi_out_dropped(&s_out));
        }

    /* --- A DISABLED RING REFUSES A PUT, OUTRIGHT ---------------------- */
    /* Off means not one byte reaches the ring, exactly as for the render.
     * Removing the `enabled` test from put_run would let any caller bypass
     * E0 by calling midi_out_put directly, and no amount of disciplined
     * calling is a substitute for the check being there. */
    fixture();
    {
        uint8_t one = 0xF8u, three[3];
        three[0] = (uint8_t)(0x90u | CH);
        three[1] = 46u;
        three[2] = 64u;
        RI_ASSERT(midi_out_put(&s_out, &one, 1u) == 0u,
            "a disabled ring refuses a clock byte");
        RI_ASSERT(midi_out_put(&s_out, three, 3u) == 0u,
            "and refuses a note");
        RI_ASSERT(midi_out_pending(&s_out) == 0u,
            "so E0 means an EMPTY ring, not a full one nobody reads (%u)",
            (unsigned)midi_out_pending(&s_out));
        midi_out_enable(&s_out, 1);
        RI_ASSERT(midi_out_put(&s_out, three, 3u) == 3u,
            "and enabling it is what lets bytes through");
    }

    /* --- ri_devout_record ON ITS OWN ----------------------------------- */
    /* The drain refuses a late accent AND ri_devout_record refuses it, and
     * two mutants survived for exactly that reason: through live_driver the
     * second refusal is unreachable, so the test could not tell whether it
     * was there. It is not decoration -- ri_devout_record is a public entry
     * point and the next caller will not have the drain's guard in front of
     * it, so it is pinned here where it can be reached. */
    fixture();
    midi_out_enable(&s_out, 1);
    {
        static struct RINoteTapRec rec;
        static uint8_t buf[8];
        uint32_t w;
        ri_devout_assign(&s_dev, 0u, CH);

        memset(&rec, 0, sizeof rec);
        rec.kind = RI_NOTEK_LATE_ACCENT;
        rec.device = RI_DEVOUT_808;
        RI_ASSERT(ri_devout_record(&s_dev, buf, sizeof buf, CH, &rec) == 0u,
            "a late accent record emits nothing of its own accord");
        RI_ASSERT(ri_devout_emitted(&s_dev) == 0u, "and emits nothing");

        /* An IMPOSSIBLE kind must be refused too, not treated as a note. */
        rec.kind = 99u;
        rec.note = 46u;
        w = ri_devout_record(&s_dev, buf, sizeof buf, CH, &rec);
        RI_ASSERT(w == 0u, "an unknown record kind is refused, not guessed (%u)",
            (unsigned)w);

        /* The three things the record carries, each one losing it. */
        rec.kind = RI_NOTEK_NOTE;
        rec.device = RI_DEVOUT_303A;
        rec.note = 46u;
        rec.flags = RI_EVFLAG_ACCENT;
        RI_ASSERT(ri_devout_record(&s_dev, buf, sizeof buf, CH, &rec) == 3u,
            "a plain record emits");
        RI_ASSERT(buf[2] == ri_devout_velocity(RI_EVFLAG_ACCENT),
            "with its accent (%u)", (unsigned)buf[2]);
        rec.flags = RI_EVFLAG_SLIDE;
        RI_ASSERT(ri_devout_record(&s_dev, buf, sizeof buf, CH, &rec) == 0u,
            "a slide emits nothing");
        rec.flags = 0u;
        rec.is_off = 1u;
        RI_ASSERT(ri_devout_record(&s_dev, buf, sizeof buf, CH, &rec) == 3u,
            "a record marked off emits an off");
        RI_ASSERT(buf[0] == (uint8_t)(0x80u | CH), "and it is a real 0x8n (%02X)",
            (unsigned)buf[0]);
    }
    }

    /* --- the tap drains across blocks, not all at once ---------------- */
    fixture();
    attach();
    {
        uint32_t k;
        for (k = 0u; k < 300u; k++)
            ev(RI_EV_NOTE_ON, 0u, 0u, 40u + (k % 12u), 0u);
    }
    {
        uint32_t first = render();
        RI_ASSERT(first > 0u, "the first block drained something (%u)",
            (unsigned)first);
        RI_ASSERT(ri_engine_note_pending(&s_ses.eng) < 300u,
            "but not all of it -- the drain is bounded per block (%u left)",
            (unsigned)ri_engine_note_pending(&s_ses.eng));
    }

    /* --- nulls --------------------------------------------------------- */
    RI_ASSERT(midi_out_put(0, s_wire, 3u) == 0u, "NULL ring");
    RI_ASSERT(midi_out_put(&s_out, 0, 3u) == 0u, "NULL bytes");
    RI_ASSERT(midi_out_put(&s_out, s_wire, 0u) == 0u, "zero bytes");
    RI_ASSERT(ri_devout_record(0, s_wire, sizeof s_wire, CH, 0) == 0u,
        "NULL producer");
    RI_ASSERT(ri_devout_record(&s_dev, s_wire, sizeof s_wire, CH, 0) == 0u,
        "NULL record");

    RI_RESULT("live-driver-devout");
}