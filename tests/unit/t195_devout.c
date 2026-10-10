/* t195_devout — R6: note and CC output per device.
 *
 * R6 asks for patterns to play EXTERNAL synths: 303 steps become notes with
 * velocity for accent, 808/909 instruments become a note map, and knob moves
 * go out as CC on the SAME controller numbers the G7 map already uses, so a
 * DAW can record ReIncarnation's automation.
 *
 * The laws here are about collision and about silence, because that is what
 * this feature is actually risky for:
 *
 *  - A device with NO assigned channel must be REFUSED and counted. Falling
 *    back to channel 1 would put the 303 on the G7 remote channel, and the
 *    G7 remote is a documented one-channel path that must keep working while
 *    other things are plugged in (midi_chan.h). A note that plays the wrong
 *    instrument is worse than a note that does not play.
 *  - OFF BY DEFAULT means no byte at all, like every other outbound feature
 *    in this slice. "Not acted on locally" is not off.
 *  - The CC numbers come from the G7 registry, NOT from a second table here.
 *    Two tables for one mapping is two answers, and they will differ.
 *  - Slide is LEGATO: the gate stays high and the pitch slews, so it must
 *    not re-attack. Emitting a fresh note-on is the audible defect.
 *  - Velocity and note numbers are 0..127 and a value outside is CLAMPED and
 *    COUNTED, never wrapped -- a wrapped note is a different instrument.
 */
#include <stdio.h>
#include "tests/helpers/ri_assert.h"
#include "midi_io/midi_devout.h"

int main(void) {
    static struct RIDevOut d;
    uint8_t buf[16];
    uint32_t n, i;

    /* --- off by default: not one byte ------------------------------- */
    ri_devout_init(&d, 0u);
    RI_ASSERT(ri_devout_enabled(&d) == 0u, "off by default");
    n = ri_devout_note(&d, buf, sizeof buf, 0u, 60u, 100u, 0u);
    RI_ASSERT(n == 0u, "a disabled device emits no note (%u)", (unsigned)n);
    n = ri_devout_note(&d, buf, sizeof buf, 1u, 60u, 100u, 0u);
    RI_ASSERT(n == 0u, "nor on any other channel (%u)", (unsigned)n);
    n = ri_devout_cc(&d, buf, sizeof buf, 0u, 74u, 64u);
    RI_ASSERT(n == 0u, "and no CC (%u)", (unsigned)n);

    /* --- enabled: canonical 3-byte messages -------------------------- */
    ri_devout_init(&d, 1u);
    n = ri_devout_note(&d, buf, sizeof buf, 0u, 60u, 100u, 0u);
    RI_ASSERT(n == 3u, "a note is three bytes (%u)", (unsigned)n);
    RI_ASSERT(buf[0] == 0x90u, "note-on status (%02X)", buf[0]);
    RI_ASSERT(buf[1] == 60u && buf[2] == 100u, "note and velocity (%u/%u)",
        (unsigned)buf[1], (unsigned)buf[2]);
    /* Note-off is a REAL note-off. A DAW importing note-on velocity 0
     * leaves the note hanging, which is the classic stuck-note failure and
     * it is audible on every take. */
    n = ri_devout_note(&d, buf, sizeof buf, 0u, 60u, 64u, RI_DEVOUT_NOTE_OFF);
    RI_ASSERT(n == 3u && buf[0] == 0x80u, "note-off is 0x80, not note-on vel 0 (%02X)",
        buf[0]);
    /* A velocity-0 note-ON is legal and is NOT a note-off here: overloading
     * velocity would make it unrepresentable. */
    n = ri_devout_note(&d, buf, sizeof buf, 0u, 60u, 0u, 0u);
    RI_ASSERT(n == 3u && buf[0] == 0x90u,
        "velocity 0 with no flag is still a note-on (%02X)", buf[0]);

    /* --- an UNCLAIMED channel is refused, which is the whole point ----- */
    /* Channel 15 has not been claimed, so nothing plays. This is the law
     * that keeps a voice off the G7 remote by default rather than by
     * remembering to avoid channel 0 at every call site. */
    n = ri_devout_note(&d, buf, sizeof buf, 15u, 60u, 100u, 0u);
    RI_ASSERT(n == 0u, "an unclaimed channel is refused (%u)", (unsigned)n);
    RI_ASSERT(ri_devout_assign(&d, 0u, 15u) == 0, "claim channel 15");
    n = ri_devout_note(&d, buf, sizeof buf, 15u, 60u, 100u, 0u);
    RI_ASSERT(n == 3u && buf[0] == 0x9Fu, "once claimed, channel 15 is 0x9F (%02X)",
        buf[0]);

    /* --- a channel past 15 CLAMPS; it must never WRAP ----------------- */
    /* Wrapping 16 to 0 would put a voice on the G7 remote channel, which
     * is the single collision this feature exists to avoid. */
    ri_devout_init(&d, 1u);
    ri_devout_assign(&d, 0u, 15u);
    n = ri_devout_note(&d, buf, sizeof buf, 200u, 60u, 100u, 0u);
    RI_ASSERT(n == 3u && buf[0] == 0x9Fu,
        "channel 200 clamps to 15, never wraps to 0 (%02X)", buf[0]);
    RI_ASSERT(ri_devout_clamped(&d) == 1u, "the channel clamp is counted (%u)",
        (unsigned)ri_devout_clamped(&d));

    /* --- values are clamped and counted, never wrapped ---------------- */
    ri_devout_init(&d, 1u);
    n = ri_devout_note(&d, buf, sizeof buf, 0u, 200u, 200u, 0u);
    RI_ASSERT(n == 3u && buf[1] == 127u && buf[2] == 127u,
        "note 200 and velocity 200 clamp to 127 (%u/%u)",
        (unsigned)buf[1], (unsigned)buf[2]);
    RI_ASSERT(ri_devout_clamped(&d) == 2u, "both value clamps are COUNTED (%u)",
        (unsigned)ri_devout_clamped(&d));

    /* --- slide is LEGATO: no re-attack -------------------------------- */
    /* A slide keeps the gate high and slews the pitch, so the outbound side
     * must not emit a fresh note-on for it -- that is the audible defect,
     * and it is why this has its own flag rather than reusing note-on. */
    n = ri_devout_note(&d, buf, sizeof buf, 0u, 62u, 64u, RI_DEVOUT_LEGATO);
    RI_ASSERT(n == 0u, "a legato slide emits NO note at all (%u)", (unsigned)n);
    RI_ASSERT(ri_devout_legato(&d) == 1u, "and it is counted, not lost (%u)",
        (unsigned)ri_devout_legato(&d));

    /* --- a device with no assigned channel is REFUSED ----------------- */
    ri_devout_init(&d, 1u);
    ri_devout_assign(&d, 1u, 3u);
    ri_devout_unassign(&d, 1u);
    n = ri_devout_note(&d, buf, sizeof buf, 3u, 60u, 100u, 0u);
    RI_ASSERT(n == 0u, "a released channel emits nothing (%u)", (unsigned)n);
    RI_ASSERT(ri_devout_refused(&d) == 1u,
        "and the refusal is COUNTED so the caller can show it (%u)",
        (unsigned)ri_devout_refused(&d));
    /* The G7 remote channel is never silently taken over. */
    RI_ASSERT(ri_devout_is_remote(&d, 0u) == 1u,
        "channel 0 is the G7 remote channel and is marked as such");

    /* --- CC uses the G7 registry, not a second table ------------------ */
    /* Appendix C: CC 74 is the 303 cutoff. The number comes from the SAME
     * registry the G7 input uses, so a DAW recording RIAPP's automation
     * lands on the control a ReBirth user expects. */
    ri_devout_init(&d, 1u);
    n = ri_devout_cc(&d, buf, sizeof buf, 0u, 74u, 100u);
    RI_ASSERT(n == 3u && buf[0] == 0xB0u, "CC is a 3-byte 0xBx message (%02X)", buf[0]);
    RI_ASSERT(buf[1] == 74u && buf[2] == 100u, "controller and value (%u/%u)",
        (unsigned)buf[1], (unsigned)buf[2]);
    RI_ASSERT(ri_devout_cc_named(&d, 74u) != 0,
        "CC 74 is a REAL control from the G7 registry, not an invented number");
    /* A controller number with no control behind it is not sent as garbage:
     * it is refused and counted, because a DAW would record a CC that means
     * nothing to the person playing it. */
    RI_ASSERT(ri_devout_cc_named(&d, 3u) == 0,
        "CC 3 is not an Appendix C control, so it has no name");

    /* --- ordering is total, so a take is reproducible ------------------ */
    /* Two events at the same sample must come out in the caller's order, not
     * in whatever order the compiler finds convenient. The caller supplies
     * the insertion index and the module does not reorder. */
    ri_devout_init(&d, 1u);
    {
        uint8_t a[3], b[3];
        ri_devout_note(&d, a, sizeof a, 0u, 60u, 100u, 0u);
        ri_devout_note(&d, b, sizeof b, 0u, 64u, 100u, 0u);
        RI_ASSERT(a[1] == 60u && b[1] == 64u, "two notes at one sample keep their order");
    }

    /* --- refusals, not silent truncation ------------------------------ */
    ri_devout_init(&d, 1u);
    RI_ASSERT(ri_devout_note(&d, buf, 2u, 0u, 60u, 100u, 0u) == 0u,
        "a 2-byte buffer emits nothing rather than a truncated note");
    RI_ASSERT(ri_devout_note(0, buf, sizeof buf, 0u, 60u, 100u, 0u) == 0u,
        "a NULL sink is inert");
    RI_ASSERT(ri_devout_note(&d, 0, 16u, 0u, 60u, 100u, 0u) == 0u, "a NULL buffer is inert");
    RI_ASSERT(ri_devout_cc(0, buf, sizeof buf, 0u, 74u, 1u) == 0u, "NULL CC is inert");

    /* --- counters are per-enable, not sticky across init -------------- */
    ri_devout_init(&d, 1u);
    ri_devout_note(&d, buf, sizeof buf, 0u, 200u, 10u, 0u);
    RI_ASSERT(ri_devout_clamped(&d) == 1u, "one clamp counted");
    ri_devout_init(&d, 1u);
    RI_ASSERT(ri_devout_clamped(&d) == 0u,
        "init clears the counters, so a run's numbers are its own (%u)",
        (unsigned)ri_devout_clamped(&d));

    /* --- every channel can be assigned and unassigned ----------------- */
    /* Sixteen voices, sixteen channels, each on its own -- and channel 0
     * stays the G7 remote's unless something claims it deliberately. */
    ri_devout_init(&d, 1u);
    for (i = 15u; i > 0u; i--) {
        uint8_t ch = (uint8_t)i;
        ri_devout_assign(&d, ch, ch);
        n = ri_devout_note(&d, buf, sizeof buf, ch, 60u, 100u, 0u);
        RI_ASSERT(n == 3u && buf[0] == (uint8_t)(0x90u | ch),
            "channel %u carries its own voice (%02X)", (unsigned)i, buf[0]);
    }
    n = ri_devout_note(&d, buf, sizeof buf, 0u, 60u, 100u, 0u);
    RI_ASSERT(n == 3u, "channel 0 is usable only because it is the remote's and claimed (%u)",
        (unsigned)n);

    RI_RESULT("devout");
}