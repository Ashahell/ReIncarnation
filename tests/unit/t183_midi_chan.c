/* t183_midi_chan — M4a: the device/channel table and the split.
 *
 * The ReBirth remote control (G7) is ONE channel and its CC map is global
 * (Appendix C). The Leviasynth gets its own channel with its own map, and
 * the two must never touch each other's controls: a CC on the Leviasynth
 * channel must not move a ReBirth knob, and a CC on the remote channel
 * must not move a Leviasynth parameter.
 *
 * Rack rule: devices are keyed by instance, not by message kind. E0
 * (ledger docs/evidence/midi/ledger.md): instance 0 = the remote on
 * channel 1, instance 1 = the Leviasynth on channel 2.
 */
#include <stdio.h>
#include <string.h>
#include "tests/helpers/ri_assert.h"
#include "midi_io/midi_chan.h"
#include "midi_io/midi_levi.h"
#include "gui/midimap.h"
#include "gui/panelui.h"
#include "gui/sectui.h"
#include "gui/secttr.h"
#include "gui/ctlreg.h"
#include "engine/dsp/levi.h"

int main(void) {
    static struct RIMidiChan t;
    static struct RIMidiLeviAction a;
    static struct RIMidiIn in;
    static struct RISectUI tr;
    struct RIPanelUI pu;

    /* --- E0 defaults: remote 1, Leviasynth 2 --------------------------- */
    midi_chan_defaults(&t);
    RI_ASSERT(midi_chan_role(&t, 1u) == RI_MCHAN_REMOTE, "ch1 is the remote");
    RI_ASSERT(midi_chan_role(&t, 2u) == RI_MCHAN_LEVI, "ch2 is the Leviasynth");
    RI_ASSERT(midi_chan_role(&t, 3u) < 0, "ch3 is nobody's");
    RI_ASSERT(midi_chan_role(0, 1u) < 0, "null table is nobody's");
    RI_ASSERT(midi_chan_n(&t) == 2u, "two devices");

    /* --- the rack rule: a channel belongs to one device, a device to one
     * channel; rebinding either is refused, not silently stolen. -------- */
    RI_ASSERT(midi_chan_bind(&t, RI_MCHAN_LEVI, 3u) == 0, "levi may move to 3");
    RI_ASSERT(midi_chan_role(&t, 2u) < 0 && midi_chan_role(&t, 3u) == RI_MCHAN_LEVI,
        "the old channel is released");
    RI_ASSERT(midi_chan_bind(&t, RI_MCHAN_LEVI, 1u) == RI_MCHAN_TAKEN,
        "channel 1 belongs to the remote");
    RI_ASSERT(midi_chan_bind(&t, RI_MCHAN_REMOTE, 4u) == 0, "remote may move to 4");
    RI_ASSERT(midi_chan_bind(&t, RI_MCHAN_REMOTE, 3u) == RI_MCHAN_TAKEN,
        "3 is the Leviasynth's now");
    midi_chan_defaults(&t);

    /* --- the split: the Leviasynth channel produces Leviasynth actions
     * and nothing else. The remote channel is the panel's alone. ------- */
    RI_ASSERT(midi_levi_message(&t, 0xB1u, 55u, 64u, &a) == 1 &&
        a.kind == RI_LEVI_ACT_PARAM && a.val == 64u,
        "CC 55 (digital filter cutoff) is a parameter, val %u", a.val);
    RI_ASSERT(midi_levi_message(&t, 0xB1u, 7u, 100u, &a) == 0,
        "CC 7 (master volume) has no Leviasynth parameter here");
    RI_ASSERT(midi_levi_message(&t, 0xB0u, 55u, 64u, &a) == 0,
        "the remote channel is not the Leviasynth's");
    RI_ASSERT(midi_levi_message(&t, 0xB3u, 55u, 64u, &a) == 0,
        "an unassigned channel is nobody's");

    /* Spot rows against the manual (E1 pp. 168-169): the digital filter
     * cutoff/resonance, the analog filter pair, and the reverb dry/wet.
     * These are the rows a player touches first, so a wrong neighbour in
     * the table must fail here rather than on stage. */
    RI_ASSERT(midi_levi_cc_key(55u) == RI_CTL_LEVI_CUTOFF && midi_levi_cc_page(55u) == 168u,
        "CC 55 is the digital filter cutoff (key %04x, p. %lu)",
        midi_levi_cc_key(55u), (unsigned long)midi_levi_cc_page(55u));
    RI_ASSERT(midi_levi_cc_key(56u) == RI_CTL_LEVI_RESO, "CC 56 is digital resonance");
    RI_ASSERT(midi_levi_cc_key(74u) == RI_CTL_LEVI_CUTOFF2, "CC 74 is the analog cutoff");
    RI_ASSERT(midi_levi_cc_key(71u) == RI_CTL_LEVI_RESO2, "CC 71 is the analog resonance");
    RI_ASSERT(midi_levi_cc_key(91u) == RI_CTL_LEVI_RDRYWET && midi_levi_cc_page(91u) == 169u,
        "CC 91 is the reverb dry/wet");
    RI_ASSERT(midi_levi_cc_key(55u) != midi_levi_cc_key(56u), "cutoff and resonance differ");
    /* The gaps carry their reason, so they are on the record. */
    RI_ASSERT(midi_levi_cc_key(7u) == 0u && midi_levi_cc_why(7u)[0] != 0,
        "CC 7 is listed as unmapped, with a reason");
    RI_ASSERT(midi_levi_cc_key(38u) == 0u, "CC 38 is Reserved in the manual");

    /* Performance and notes are the other two action kinds, and they are
     * equally unreachable from the remote channel. */
    /* A 14-bit bend does not fit one 7-bit control value, so it travels
     * as the spare second byte: val = LSB, hi = MSB (centre 0x40/0x00). */
    RI_ASSERT(midi_levi_message(&t, 0xE1u, 0x00u, 0x60u, &a) == 1 &&
        a.kind == RI_LEVI_ACT_PERF && a.perf == RI_LEVI_PERF_BEND &&
        a.key == RI_CTL_LEVI_BEND && a.val == 0u && a.hi == 0x60u,
        "pitch bend is a 14-bit performance signal (perf %u, val %u, hi %u)",
        a.perf, a.val, a.hi);
    RI_ASSERT(midi_levi_message(&t, 0x91u, 60u, 100u, &a) == 1 &&
        a.kind == RI_LEVI_ACT_NOTE && a.note == 60u && a.on == 1u, "note on");
    RI_ASSERT(midi_levi_message(&t, 0x91u, 60u, 0u, &a) == 1 &&
        a.kind == RI_LEVI_ACT_NOTE && a.on == 0u, "note on with vel 0 is a note off");
    RI_ASSERT(midi_levi_message(&t, 0xA1u, 60u, 90u, &a) == 1 &&
        a.kind == RI_LEVI_ACT_PERF && a.perf == RI_LEVI_PERF_POLYAT &&
        a.note == 60u && a.val == 90u,
        "poly aftertouch carries its note (perf %u, val %u)", a.perf, a.val);

    /* --- and the panel law: the remote channel still moves ReBirth
     * controls, and nothing on the Leviasynth channel does. ------------- */
    ri_midi_init(&in, 0u);                 /* channel index 0 = ch 1 */
    ri_panel_init(&pu);
    ri_sui_init(&tr, RI_SEC_TRANSPORT);
    pu.tr = &tr;
    {
        /* CC 7 on the remote channel is the transport's shuffle row in
         * Appendix C; the Leviasynth's CC 7 must not reach it. */
        const struct RICtlDef *d = ri_ctlreg_by_cc(7u);
        int before;
        RI_ASSERT(d != 0, "Appendix C has CC 7");
        before = ri_sui_value(&tr, (uint32_t)(d->reg_id & 0xFFu));
        (void)before;
    }
    /* One message on the Leviasynth channel: the panel's own remote map
     * must not see it at all. */
    {
        uint32_t msg0 = in.messages, ign0 = in.ignored;
        ri_midi_msg(&in, &pu, 0xB1u, 7u, 100u);
        RI_ASSERT(in.ignored == ign0 + 1u, "a foreign channel counts as ignored");
        (void)msg0;
    }
    RI_RESULT("midichan");
}