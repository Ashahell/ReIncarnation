/* t181_midi_bridge — M2: bridge router, settings, MIDI value push.
 * The portable core every backend feeds: realtime/system common run the
 * owned follower (intents queue), everything queues for the GUI (LEDs
 * included, RISECT parity); flood drops the oldest and counts (P-19);
 * settings parse with E0 defaults; changed panel values push through
 * the bridge; the G7 subset through the router drives all four foci.
 * RED on HEAD: no router existed (link error is the red).
 */
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "tests/helpers/ri_assert.h"
#include "midi_io/midi_bridge.h"
#include "midi_io/midi_follow.h"
#include "gui/panelctl.h"
#include "gui/ctlreg.h"
#include "gui/sectui.h"
#include "gui/panelui.h"
#include "gui/midimap.h"
#include "engine/dsp/rb303.h"
#include "engine/seq/ctlplane.h"
#include "engine/seq/sched.h"

static void feed3(struct RIMidiBridge *b, uint8_t a, uint8_t c, uint8_t d, uint64_t t) {
    midi_bridge_feed(b, a, c, d, t);
}

int main(void) {
    static struct RIMidiBridge b;
    static struct RIMidiMsg msgs[300];
    static struct RIMidiIntent its[70];
    uint32_t i, n;
    /* NULL-safe. */
    midi_bridge_feed(0, 0x90u, 60u, 100u, 0u);
    RI_ASSERT(midi_bridge_read_ch(0, msgs, 300u) == 0u, "null read");
    RI_ASSERT(midi_bridge_read_in(0, its, 70u) == 0u, "null intent");
    RI_ASSERT(midi_bridge_pending_ch(0) == 0u, "null pending");
    midi_bridge_note_camd_drop(0);
    midi_bridge_note_link(0, 1u);
    /* Order, bytes, stamps; realtime queues but emits no intent. */
    midi_bridge_init(&b);
    feed3(&b, 0x90u, 0x3Cu, 0x40u, 12000u);
    feed3(&b, 0xF8u, 0u, 0u, 13000u);
    feed3(&b, 0xB0u, 0x26u, 0x7Fu, 24000u);
    RI_ASSERT(midi_bridge_pending_ch(&b) == 3u, "queued %u", midi_bridge_pending_ch(&b));
    RI_ASSERT(midi_bridge_pending_in(&b) == 0u, "no intent");
    n = midi_bridge_read_ch(&b, msgs, 300u);
    RI_ASSERT(n == 3u, "drained %u", n);
    RI_ASSERT(msgs[0].b[0] == 0x90u && msgs[0].b[1] == 0x3Cu && msgs[0].b[2] == 0x40u &&
        msgs[0].n == 3u && msgs[0].t_us == 12000u, "msg0");
    RI_ASSERT(msgs[1].b[0] == 0xF8u && msgs[1].n == 1u && msgs[1].t_us == 13000u, "tick msg");
    RI_ASSERT(msgs[2].b[0] == 0xB0u && msgs[2].t_us == 24000u, "msg2");
    RI_ASSERT(midi_bridge_pending_ch(&b) == 0u, "empty");
    /* Transport through the router: arm on FA, fire on F8, stop on FC. */
    midi_bridge_init(&b);
    feed3(&b, 0xFAu, 0u, 0u, 1000u);
    RI_ASSERT(midi_bridge_pending_in(&b) == 0u, "fa arms silently");
    feed3(&b, 0xF8u, 0u, 0u, 2000u);
    RI_ASSERT(midi_bridge_pending_in(&b) == 1u, "f8 fires");
    n = midi_bridge_read_in(&b, its, 70u);
    RI_ASSERT(n == 1u && its[0].it.kind == RI_FOLLOW_PLAY_START && its[0].t_us == 2000u,
        "start intent");
    feed3(&b, 0xFCu, 0u, 0u, 3000u);
    n = midi_bridge_read_in(&b, its, 70u);
    RI_ASSERT(n == 1u && its[0].it.kind == RI_FOLLOW_STOP, "stop intent");
    /* SPP seeks stopped, ignored running (counted). */
    midi_bridge_init(&b);
    feed3(&b, 0xF2u, 0x0Au, 0x00u, 1000u);
    n = midi_bridge_read_in(&b, its, 70u);
    RI_ASSERT(n == 1u && its[0].it.kind == RI_FOLLOW_SEEK && its[0].it.seek_16ths == 40u,
        "spp seek %u", n ? its[0].it.seek_16ths : 0u);
    feed3(&b, 0xFAu, 0u, 0u, 2000u);
    feed3(&b, 0xF8u, 0u, 0u, 3000u);
    n = midi_bridge_read_in(&b, its, 70u);
    RI_ASSERT(n == 1u && its[0].it.kind == RI_FOLLOW_PLAY_START, "running");
    feed3(&b, 0xF2u, 0x00u, 0x01u, 4000u);
    RI_ASSERT(midi_bridge_pending_in(&b) == 0u, "spp running silent");
    RI_ASSERT(b.follow.spp_ignored == 1u, "spp counted");
    /* Flood: oldest dropped and counted (P-19). */
    midi_bridge_init(&b);
    for (i = 0u; i < RI_MBR_CH_CAP + 10u; i++)
        feed3(&b, 0xB0u, (uint8_t)(i & 0x7Fu), 0u, 1000u + i);
    RI_ASSERT(midi_bridge_pending_ch(&b) == RI_MBR_CH_CAP, "capped");
    RI_ASSERT(b.ch_dropped == 10u, "ch drops %u", b.ch_dropped);
    n = midi_bridge_read_ch(&b, msgs, 300u);
    RI_ASSERT(n == RI_MBR_CH_CAP && msgs[0].b[1] == 10u, "oldest dropped");
    for (i = 0u; i < 30u; i++) {
        midi_bridge_feed(&b, 0xFCu, 0u, 0u, 2000u + i);
        midi_bridge_feed(&b, 0xFAu, 0u, 0u, 2500u + i);
        midi_bridge_feed(&b, 0xF8u, 0u, 0u, 3000u + i);
        midi_bridge_feed(&b, 0xFCu, 0u, 0u, 3500u + i);
    }
    RI_ASSERT(b.in_dropped == 30u * 3u - RI_MBR_IN_CAP, "intent drops %u", b.in_dropped);
    /* Backend reports. */
    midi_bridge_note_camd_drop(&b);
    RI_ASSERT(b.camd_dropped == 1u, "camd drop");
    midi_bridge_note_link(&b, 1u);
    RI_ASSERT(b.link_lost == 1u, "link lost");
    midi_bridge_note_link(&b, 0u);
    RI_ASSERT(b.link_lost == 0u, "link back");
    /* Settings: E0 defaults, accept, reject-keeps. */
    {
        static struct RIMidiSettings s;
        midi_settings_defaults(&s);
        RI_ASSERT(!strcmp(s.cluster, "riapp"), "cluster %s", s.cluster);
        RI_ASSERT(s.channel == 1u && s.sync == RI_SYNC_INTERNAL && s.levi_ch == 2u &&
            s.clk_out == 0u && s.lat_ms == 0, "defaults");
        RI_ASSERT(midi_settings_set(&s, RI_MIDI_SET_CHANNEL, 3L) == 0 && s.channel == 3u, "ch");
        RI_ASSERT(midi_settings_set(&s, RI_MIDI_SET_SYNC, 1L) == 0 && s.sync == 1u, "sync");
        RI_ASSERT(midi_settings_set(&s, RI_MIDI_SET_LEVI_CH, 5L) == 0 && s.levi_ch == 5u, "levi");
        RI_ASSERT(midi_settings_set(&s, RI_MIDI_SET_CLK_OUT, 1L) == 0 && s.clk_out == 1u, "clk");
        RI_ASSERT(midi_settings_set(&s, RI_MIDI_SET_LAT_MS, -50L) == 0 && s.lat_ms == -50, "lat");
        RI_ASSERT(midi_settings_set(&s, RI_MIDI_SET_CHANNEL, 17L) == 1 && s.channel == 3u, "ch rej");
        RI_ASSERT(midi_settings_set(&s, RI_MIDI_SET_CHANNEL, 0L) == 1 && s.channel == 3u, "ch rej0");
        RI_ASSERT(midi_settings_set(&s, RI_MIDI_SET_SYNC, 2L) == 1 && s.sync == 1u, "sync rej");
        RI_ASSERT(midi_settings_set(&s, RI_MIDI_SET_LEVI_CH, 0L) == 1 && s.levi_ch == 5u, "levi rej");
        RI_ASSERT(midi_settings_set(&s, RI_MIDI_SET_CLK_OUT, 7L) == 1 && s.clk_out == 1u, "clk rej");
        RI_ASSERT(midi_settings_set(&s, RI_MIDI_SET_LAT_MS, 201L) == 1 && s.lat_ms == -50, "lat rej");
        RI_ASSERT(midi_settings_set(&s, 99u, 1L) == 1, "field rej");
        RI_ASSERT(midi_settings_set(0, RI_MIDI_SET_CHANNEL, 1L) == 1, "null rej");
    }
    /* Value push: shadow, one send per change, right lane key. */
    {
        static struct RISectUI u303;
        static struct RISectUI *uis[1];
        static uint8_t sections[1];
        static uint8_t shadow[256];
        static struct RIControlPlane pl;
        static struct RIEvent ev[8];
        uint32_t seq = 0u, nd, covered;
        const struct RICtlDef *d = ri_ctlreg_find((uint16_t)((uint16_t)RI_SEC_SYNTH1 << 8) | 2u);
        RI_ASSERT(d != 0 && ri_ctlreg_auto_id(d) == RI_CTL_303A_CUTOFF, "cutoff row");
        RI_ASSERT(ri_sui_init(&u303, RI_SEC_SYNTH1) == 0, "sui init");
        uis[0] = &u303;
        sections[0] = RI_SEC_SYNTH1;
        ri_ctl_init(&pl);
        covered = ri_panel_midi_shadow_init(uis, sections, 1u, shadow);
        RI_ASSERT(covered > 0u, "covered %u", covered);
        RI_ASSERT(ri_panel_midi_push(uis, sections, 1u, &pl, shadow) == 0u, "quiet");
        RI_ASSERT(ri_ctl_pending(&pl) == 0u, "nothing queued");
        RI_ASSERT(ri_sui_set(&u303, 2u, 100) == 1, "cutoff turn");
        RI_ASSERT(ri_panel_midi_push(uis, sections, 1u, &pl, shadow) == 1u, "one send");
        RI_ASSERT(ri_panel_midi_push(uis, sections, 1u, &pl, shadow) == 0u, "shadow holds");
        nd = ri_ctl_drain(&pl, ev, 8u, 0u, &seq);
        RI_ASSERT(nd == 1u && ev[0].value == RI_CTL_303A_CUTOFF && (ev[0].flags & 127u) == 100u,
            "lane key");
        RI_ASSERT(ri_panel_midi_push(0, sections, 1u, &pl, shadow) == 0u, "null uis");
        RI_ASSERT(ri_panel_midi_push(uis, sections, 1u, 0, shadow) == 0u, "null plane");
    }
    /* G7 subset through the router: CC, focus, play, channel split, LEDs. */
    {
        static struct RISectUI s808, str, s303;
        static struct RIPanelUI p;
        static struct RIMidiIn m;
        uint32_t got = 0u, i;
        ri_panel_init(&p);
        RI_ASSERT(ri_sui_init(&s808, RI_SEC_808) == 0, "808 init");
        RI_ASSERT(ri_sui_init(&str, RI_SEC_TRANSPORT) == 0, "tr init");
        RI_ASSERT(ri_sui_init(&s303, RI_SEC_SYNTH1) == 0, "303 init");
        p.drum[0] = &s808;
        p.tr = &str;
        p.synth[0] = &s303;
        ri_midi_init(&m, 0u);
        midi_bridge_init(&b);
        feed3(&b, 0xB0u, 38u, 127u, 1000u);   /* CC 38: 808 BD level max */
        feed3(&b, 0x90u, 67u, 100u, 2000u);   /* focus 808 */
        feed3(&b, 0x90u, 69u, 100u, 3000u);   /* Play */
        feed3(&b, 0x91u, 69u, 100u, 4000u);   /* channel 2: ignored */
        feed3(&b, 0xF8u, 0u, 0u, 5000u);      /* clock: LED + clocks */
        n = midi_bridge_read_ch(&b, msgs, 300u);
        RI_ASSERT(n == 5u, "five queued %u", n);
        for (i = 0u; i < n; i++)
            got += (uint32_t)ri_midi_msg(&m, &p, msgs[i].b[0], msgs[i].b[1], msgs[i].b[2]);
        RI_ASSERT(got > 0u, "panel moved");
        RI_ASSERT(ri_sui_value(&s808, 1u) == 127, "bd level %d", ri_sui_value(&s808, 1u));
        RI_ASSERT(p.focus == RI_FOCUS_808, "focus 808, got %u", p.focus);
        RI_ASSERT(m.ignored >= 1u, "ch2 ignored %u", m.ignored);
        RI_ASSERT(m.midi_led_ms == RI_MIDI_LED_MS, "led lit");
        RI_ASSERT(m.clocks == 1u, "clock counted %u", m.clocks);
        RI_ASSERT(m.messages == 5u, "messages %u", m.messages);
    }
    RI_RESULT("midibridge");
}
