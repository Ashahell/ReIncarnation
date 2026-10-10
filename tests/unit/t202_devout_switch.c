/* t202_devout_switch — R6f: the second E0 switch, and the note channel.
 *
 * The clock-out switch could have been shared with note output, and I
 * recommended sharing it until the case that breaks it was named:
 * SOMEONE SLAVING THEIR OWN DRUM MACHINE TO OUR CLOCK. They want the clock
 * and specifically NOT the notes -- otherwise turning on ReIncarnation's 808
 * fires the very machine they are driving, from a pattern they did not ask
 * to play. One switch makes that state unreachable.
 *
 * The codebase already decided this once: `RI_MIDI_SET_MMC_OUT` exists
 * SEPARATE from `RI_MIDI_SET_CLK_OUT`, and its own comment says why --
 * "different features with different consequences on a slave, and one E0
 * switch for both would make the safe choice (clock only) impossible to
 * express." This is the same argument one feature over. The RING and the
 * SENDER are still shared, which is why this costs two settings and not a
 * second transport.
 *
 * The other half is the melodic channel, and its default is the whole point:
 *
 *  - **AN UNASSIGNED NOTE CHANNEL IS NOT DEFAULTED.** 0 means "none", and
 *    the refusal lives in the emit path (t201/t200). Defaulting it to 1
 *    would put the 303 on channel 1 without asking, and the G7 remote is a
 *    documented ONE-CHANNEL path (manual p. 134) that has to keep working
 *    while other things are plugged in.
 *  - **THERE IS NO DRUM-CHANNEL SETTING, AND THERE CANNOT BE.** GM defines
 *    percussion on channel 10 and nowhere else; it is not a choice. A
 *    setting for it would be a control that cannot do anything.
 *
 * And the attach law, which is where the program change actually happens:
 *
 *  - **EXACTLY ONE PROGRAM CHANGE, NO MATTER HOW OFTEN YOU ATTACH.** The
 *    program change is a session announcement, not a per-note event; a
 *    drain that pushed one on every attach would fill a slave's channel
 *    with them. So `attach` is idempotent, and that is pinned by attaching
 *    twice.
 *  - **NO NOTE CHANNEL, NO PROGRAM CHANGE.** There is no melodic instrument
 *    to name, so there is nothing to announce -- but the DRUMS still work,
 *    because channel 10 is not a choice and needs no configuration. That
 *    asymmetry is the whole reason the drum override exists.
 *  - **ATTACHING FAILS CLOSED.** A NULL producer, a NULL ring, or a
 *    disabled ring sends nothing at all rather than half-announcing.
 */
#include <stdio.h>
#include <string.h>
#include "tests/helpers/ri_assert.h"
#include "midi_io/midi_bridge.h"
#include "app/core/live_driver.h"
#include "midi_io/midi_devout.h"
#include "midi_io/midi_out.h"

#define CH 5u

static struct RIMidiSettings S;
static struct RILiveDriver D;
static struct RIMidiOut O;
static struct RIDevOut V;
static uint8_t W[64];

static uint32_t drain(void) { return midi_out_read(&O, W, sizeof W); }

static void setup(int out_enabled) {
    midi_settings_defaults(&S);
    memset(&D, 0, sizeof D);
    D.session = 0;
    D.dev_out = &V;
    D.clk_out = &O;
    D.note_ch = -1;
    D.prog_sent = 0u;
    midi_out_init(&O, 48000u, 120000u, 0u);
    if (out_enabled)
        midi_out_enable(&O, 1);
    ri_devout_init(&V, 1);
}

int main(void) {
    /* --- E0: BOTH ARE OFF, AND THE CHANNEL IS NONE -------------------- */
    midi_settings_defaults(&S);
    RI_ASSERT(S.clk_out == 0u, "clock out is off (%u)", (unsigned)S.clk_out);
    RI_ASSERT(S.dev_out == 0u, "note out is off (%u)", (unsigned)S.dev_out);
    RI_ASSERT(S.note_ch == 0u,
        "and the melodic channel is UNASSIGNED, not defaulted (%u)",
        (unsigned)S.note_ch);

    /* --- dev_out is strictly 0/1, exactly like mmc_out ---------------- */
    RI_ASSERT(midi_settings_set(&S, RI_MIDI_SET_DEV_OUT, 1L) == 0
        && S.dev_out == 1u, "on");
    RI_ASSERT(midi_settings_set(&S, RI_MIDI_SET_DEV_OUT, 0L) == 0
        && S.dev_out == 0u, "off");
    /* Strictly, because a truthy value here would put notes on the wire
     * from a control that reads as a slider. Same law, same reason, as
     * MMC_OUT's. */
    RI_ASSERT(midi_settings_set(&S, RI_MIDI_SET_DEV_OUT, 2L) == 1
        && S.dev_out == 0u, "2 is refused (%u)", (unsigned)S.dev_out);
    RI_ASSERT(midi_settings_set(&S, RI_MIDI_SET_DEV_OUT, -1L) == 1
        && S.dev_out == 0u, "-1 is refused");

    /* --- the melodic channel is 0 (none) or 1..16 --------------------- */
    RI_ASSERT(midi_settings_set(&S, RI_MIDI_SET_NOTE_CH, 5L) == 0
        && S.note_ch == 5u, "channel 5 (%u)", (unsigned)S.note_ch);
    RI_ASSERT(midi_settings_set(&S, RI_MIDI_SET_NOTE_CH, 16L) == 0
        && S.note_ch == 16u, "channel 16 is the last (%u)",
        (unsigned)S.note_ch);
    RI_ASSERT(midi_settings_set(&S, RI_MIDI_SET_NOTE_CH, 0L) == 0
        && S.note_ch == 0u, "0 means unassigned (%u)", (unsigned)S.note_ch);
    RI_ASSERT(midi_settings_set(&S, RI_MIDI_SET_NOTE_CH, 17L) == 1
        && S.note_ch == 0u, "17 is refused, not wrapped (%u)",
        (unsigned)S.note_ch);
    /* NEGATIVE IS REFUSED TOO, and this one is not cosmetic: -1 stored as a
     * uint8 is 255, which is truthy, so `riapp`'s "0 means unassigned"
     * translation would turn a refusal into 254 -- a channel far outside
     * 0..15 and one nothing downstream would have checked for. */
    RI_ASSERT(midi_settings_set(&S, RI_MIDI_SET_NOTE_CH, -1L) == 1
        && S.note_ch == 0u, "-1 is refused (%u)", (unsigned)S.note_ch);
    /* Wrapping is the failure: 17 -> 1 would put the 303 on a channel
     * nobody chose, and 0 -> 16 on the remote. */
    RI_ASSERT(midi_settings_set(&S, RI_MIDI_SET_NOTE_CH, 0L) == 0, "0 again");
    RI_ASSERT(S.note_ch == 0u, "and 0 is still unassigned, not 16");

    /* --- the unknown field still refuses -------------------------------- */
    RI_ASSERT(midi_settings_set(&S, 99u, 1L) == 1, "an unknown field refuses");

    /* --- ATTACH WITH A CHANNEL: exactly one program change ------------ */
    setup(1);
    D.note_ch = (int)CH;
    ri_livedrv_devout_attach(&D);
    RI_ASSERT(drain() == 2u, "a two-byte program change (%u)",
        (unsigned)drain());
    RI_ASSERT(W[0] == (uint8_t)(0xC0u | CH), "on channel 5 (%02X)",
        (unsigned)W[0]);
    RI_ASSERT(W[1] == ri_devout_program(), "program %u (%u)",
        (unsigned)W[1], (unsigned)ri_devout_program());
    /* IDEMPOTENT. Attaching again must not announce again -- the program
     * change names the instrument for the session, and a stream of them
     * fills a slave's channel. */
    ri_livedrv_devout_attach(&D);
    ri_livedrv_devout_attach(&D);
    RI_ASSERT(drain() == 0u, "attaching twice more says nothing (%u)",
        (unsigned)drain());

    /* --- NO CHANNEL: no program change, but the drums still work ------ */
    setup(1);
    ri_livedrv_devout_attach(&D);        /* note_ch is -1 */
    RI_ASSERT(drain() == 0u,
        "with no melodic channel there is no instrument to name (%u)",
        (unsigned)drain());
    RI_ASSERT(D.prog_sent == 0u, "and nothing was marked sent (%u)",
        (unsigned)D.prog_sent);
    /* ATTACH MUST NOT LEAVE THE CHANNEL ASSIGNED AS A SIDE EFFECT.
     *
     * This is the survivor that mattered. An attach that answered "no
     * channel" by quietly setting one to 1 announces nothing -- the
     * producer's claim check refuses it -- so a test that only counts
     * bytes calls that equivalent. It is NOT: `note_ch` is left at 1, and
     * the very next melodic note would be emitted on channel 1. That is
     * precisely the law R6a exists to enforce, undone by a function whose
     * whole job is to configure the producer. So the check is on what
     * comes AFTER the attach, not on the attach itself. */
    {
        struct RINoteTapRec r;
        memset(&r, 0, sizeof r);
        r.kind = RI_NOTEK_NOTE;
        r.device = RI_DEVOUT_303A;
        r.note = 46u;
        r.flags = 0u;
        RI_ASSERT(D.note_ch == -1,
            "the driver still holds no channel (%d)", D.note_ch);
        RI_ASSERT(ri_devout_record(&V, W, sizeof W, CH, &r) == 0u,
            "and a melodic note is still refused, not sent on channel 1");
    }
    /* ...and the drums are unaffected, which is the asymmetry the whole
     * GM-channel-10 decision rests on. */
    {
        struct RINoteTapRec r;
        memset(&r, 0, sizeof r);
        r.kind = RI_NOTEK_NOTE;
        r.device = RI_DEVOUT_808;
        r.sound = 0u;
        r.note = 36u;
        r.flags = 0u;   /* plain velocity, carried by the producer */
        RI_ASSERT(ri_devout_record(&V, W, sizeof W, CH, &r) == 3u,
            "an 808 note still emits with no melodic channel");
        RI_ASSERT(W[0] == (uint8_t)(0x90u | RI_DEVOUT_GM_PERCUSSION),
            "on channel 10 (%02X)", (unsigned)W[0]);
        RI_ASSERT(midi_out_put(&O, W, 3u) == 3u, "and reaches the ring");
        RI_ASSERT(drain() == 3u, "three bytes, unannounced (%u)",
            (unsigned)drain());
    }

    /* --- FAILS CLOSED -------------------------------------------------- */
    setup(1);
    D.dev_out = 0;
    ri_livedrv_devout_attach(&D);
    RI_ASSERT(drain() == 0u, "a NULL producer announces nothing (%u)",
        (unsigned)drain());

    setup(1);
    D.clk_out = 0;
    D.note_ch = (int)CH;
    ri_livedrv_devout_attach(&D);
    RI_ASSERT(drain() == 0u, "a NULL ring announces nothing");
    /* ATTACH IS ALL-OR-NOTHING. Counting bytes is not enough: with no ring
     * the put fails anyway, so a byte count reads zero whether the attach
     * did nothing or did half its job. What matters is that it did not
     * leave the melodic channel CLAIMED on a producer nobody is draining. */
    {
        struct RINoteTapRec r;
        memset(&r, 0, sizeof r);
        r.kind = RI_NOTEK_NOTE;
        r.device = RI_DEVOUT_303A;
        r.note = 46u;
        RI_ASSERT(ri_devout_record(&V, W, sizeof W, CH, &r) == 0u,
            "a NULL-ring attach claimed nothing on the producer");
    }

    /* A DISABLED ring is refused by midi_out_put, so a half-attach cannot
     * leave a program change half-written. */
    setup(0);
    D.note_ch = (int)CH;
    ri_livedrv_devout_attach(&D);
    RI_ASSERT(drain() == 0u, "a disabled ring announces nothing (%u)",
        (unsigned)drain());
    RI_ASSERT(D.prog_sent == 0u,
        "and is NOT marked sent, so enabling later can still announce (%u)",
        (unsigned)D.prog_sent);
    midi_out_enable(&O, 1);
    ri_livedrv_devout_attach(&D);
    RI_ASSERT(drain() == 2u,
        "once enabled, attach announces exactly once (%u)", (unsigned)drain());

    /* --- a DISABLED producer announces nothing either ----------------- */
    setup(1);
    D.note_ch = (int)CH;
    ri_devout_enable(&V, 0);
    ri_livedrv_devout_attach(&D);
    RI_ASSERT(drain() == 0u, "a disabled producer announces nothing (%u)",
        (unsigned)drain());

    /* --- NULLs ---------------------------------------------------------- */
    ri_livedrv_devout_attach(0);

    RI_RESULT("devout-switch");
}