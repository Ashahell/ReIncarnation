/* t215_notein_settings — R5e: which channel each machine answers on.
 *
 * R5a decided the SHAPE (drums implicit and counted, 303s opt-in) and R5f
 * decided there is deliberately NO drum channel setting. R5e is where those
 * two meet a real settings struct, and the risk is that they quietly
 * disagree — a device silent on the way out and loud on the way in.
 *
 * **THE 808 IS IMPLICIT ON GM CHANNEL 10, AND THAT IS NOT A CHOICE.** R6f
 * wrote it: "There is deliberately NO drum channel setting: GM defines
 * percussion on channel 10 and it is not a choice, so a control for it could
 * not do anything." The same reasoning holds inbound, so there is no
 * `drum_ch` field and the answer is 9 (0-based) whatever the settings say.
 *
 * **THE 909 IS OPT-IN AND NEVER DEFAULTED.** It cannot be implicit too: two
 * devices cannot share channel 10, and `ri_notein_bind` refuses the second.
 * Picking a winner would be a guess dressed as a default — so `in909_ch` of 0
 * means unassigned, exactly as R6f's `note_ch` of 0 does.
 *
 * **THE 303s FOLLOW `note_ch`**, the channel R6f already defined, so there is
 * one melodic setting rather than two that can disagree.
 *
 * **VELOCITY 0 ON A NOTE-ON STATUS IS A NOTE-OFF.** MIDI says so, and every
 * DAW uses it to save bytes. Reading it as a note-on would leave the note
 * sounding forever — which is R5c's exact bug, reintroduced from the wire.
 */
#include <stdio.h>
#include <string.h>
#include "tests/helpers/ri_assert.h"
#include "midi_io/midi_bridge.h"
#include "midi_io/note_in.h"

static struct RIMidiSettings S;

static void def(void) {
    midi_settings_defaults(&S);
}

int main(void) {
    /* --- 1. THE 808 IS GM 10, WHATEVER THE SETTINGS SAY --------------- */
    def();
    RI_ASSERT(midi_settings_notein_ch(&S, RI_NOTEIN_DEV_808) == 9,
        "the 808 is channel 10 by default (%d)",
        midi_settings_notein_ch(&S, RI_NOTEIN_DEV_808));
    /* Move every other setting. The drum channel does not move, because it
     * was never a setting. */
    S.channel = 1u; S.levi_ch = 3u; S.note_ch = 5u; S.in909_ch = 7u;
    RI_ASSERT(midi_settings_notein_ch(&S, RI_NOTEIN_DEV_808) == 9,
        "and STILL channel 10 after every other setting moved (%d)",
        midi_settings_notein_ch(&S, RI_NOTEIN_DEV_808));

    /* --- 2. THE 303s FOLLOW note_ch ----------------------------------- */
    def();
    RI_ASSERT(S.note_ch == 0u, "note_ch is unset by default (%u)",
        (unsigned)S.note_ch);
    RI_ASSERT(midi_settings_notein_ch(&S, RI_NOTEIN_DEV_303A) ==
        midi_settings_note_ch(&S),
        "and an unset note_ch gives the 303A the SAME answer as R6f (%d)",
        midi_settings_notein_ch(&S, RI_NOTEIN_DEV_303A));
    S.note_ch = 4u;
    RI_ASSERT(midi_settings_notein_ch(&S, RI_NOTEIN_DEV_303A) == 3,
        "an explicit note_ch of 4 is 0-BASED 3 (%d)",
        midi_settings_notein_ch(&S, RI_NOTEIN_DEV_303A));
    RI_ASSERT(midi_settings_notein_ch(&S, RI_NOTEIN_DEV_303B) == 3,
        "and the 303B follows it -- ONE melodic setting, not two");

    /* --- 3. THE 909 IS OPT-IN AND NEVER DEFAULTED --------------------- */
    def();
    RI_ASSERT(midi_settings_notein_ch(&S, RI_NOTEIN_DEV_909) == -1,
        "the 909 is UNASSIGNED by default (%d)",
        midi_settings_notein_ch(&S, RI_NOTEIN_DEV_909));
    /* An unset note_ch does NOT drag the 909 in with it -- that would be the
     * implicit-drum decision leaking onto a second device. */
    S.note_ch = 6u;
    RI_ASSERT(midi_settings_notein_ch(&S, RI_NOTEIN_DEV_909) == -1,
        "and a note_ch does not assign it (%d)",
        midi_settings_notein_ch(&S, RI_NOTEIN_DEV_909));
    S.in909_ch = 11u;
    RI_ASSERT(midi_settings_notein_ch(&S, RI_NOTEIN_DEV_909) == 10,
        "an explicit in909_ch of 11 is 0-BASED 10 (%d)",
        midi_settings_notein_ch(&S, RI_NOTEIN_DEV_909));

    /* --- 4. THE TWO DRUM DEVICES NEVER COLLIDE BY DEFAULT ------------ */
    /* The point of §3, stated as the observable it prevents. */
    def();
    {
        int a = midi_settings_notein_ch(&S, RI_NOTEIN_DEV_808);
        int b = midi_settings_notein_ch(&S, RI_NOTEIN_DEV_909);
        RI_ASSERT(a != b, "the 808 (%d) and the 909 (%d) never share",
            a, b);
    }
    /* And a user who sets in909_ch to 10 DOES collide -- which ri_notein_bind
     * refuses, rather than letting one device silently shadow the other. */
    S.in909_ch = 10u;
    {
        struct RINoteIn n;
        ri_notein_init(&n);
        RI_ASSERT(ri_notein_bind(&n, 9u, RI_NOTEIN_DEV_808) == 0,
            "the 808 takes 10");
        RI_ASSERT(ri_notein_bind(&n, 9u, RI_NOTEIN_DEV_909) != 0,
            "and the 909 asking for the same channel is REFUSED, not shadowed");
    }

    /* --- 5. AN UNKNOWN DEVICE IS -1, NOT CHANNEL 10 ------------------ */
    RI_ASSERT(midi_settings_notein_ch(&S, 99u) == -1, "a device past the last");
    RI_ASSERT(midi_settings_notein_ch(0, RI_NOTEIN_DEV_808) == -1, "NULL");

    RI_RESULT("notein-settings");
}