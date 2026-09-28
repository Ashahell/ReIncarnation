/* t96_pattern_change — pattern selection flips the sounding slot.
 * A bar boundary exactly on a tick-window edge belongs to exactly one
 * window (the one starting there): mid-stream captures take effect at the
 * next pattern end. Found on the Dell 2026-09-27 (pattern edits inaudible):
 * single-tick windows tile the integers, so <=/>= skipped EVERY boundary.
 */
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "tests/helpers/ri_assert.h"
#include "app/core/riapp_core.h"
#include "engine/engine.h"
#include "engine/seq/player.h"
#include "engine/seq/songtrack.h"
#include "engine/seq/pattern.h"
#include "engine/seq/clock.h"

static void loud_slot3(struct RIAppCore *c) {
    uint32_t i;
    for (i = 0u; i < 16u; i++)
        ri_p303_set(&c->banks[0].pat[3], i, 12u, RI_STEP_ACCENT);
}

int main(void) {
    /* Unit level: edge-exact windows around the bar-2 boundary (tick 768). */
    {
        static struct RIPlayer p;
        static struct RISongTrack t;
        static struct RIPatternBank b;
        static struct RIEvent out[256];
        struct RITempoMap map;
        struct RISegment seg;
        struct RILoop loop;
        const struct RIPatternBank *b4[5] = { &b, 0, 0, 0, 0 };
        uint32_t got;
        ri_bank_init(&b, 0u, RI_PATTERN_KIND_303, 0u);
        ri_track_init(&t);
        ri_track_capture(&t, 2u, 0u, 3u);
        seg.start_tick = 0u;
        seg.ns_per_quarter = 500000000ULL;
        map.segs = &seg;
        map.n = 1u;
        map.ppq = 96u;
        map.sr = 48000u;
        loop.on = 0u;
        loop.start_bar = 0u;
        loop.len_bars = 0u;
        ri_player_init(&p, b4, &t, 0u);
        got = ri_player_block(&p, &t, &loop, &map, 96u, 767u, 768u, out, 256u);
        (void)got;
        RI_ASSERT(p.pending_slot[0] == 0u, "pre-edge untouched");
        got = ri_player_block(&p, &t, &loop, &map, 96u, 768u, 770u, out, 256u);
        (void)got;
        RI_ASSERT(p.pending_slot[0] == 3u, "edge-exact flip, got %u", p.pending_slot[0]);
    }
    /* Session level: captured slot sounds after the crossing. */
    {
        static struct RIAppCore a, b;
        static float aL[96000u * 4u], aR[96000u * 4u], bL[96000u * 4u], bR[96000u * 4u];
        uint32_t done = 0u, i;
        uint32_t barlen = 96000u; /* 1 bar @120bpm */
        ri_core_init(&a, 96u, 48000.0f, 120.0f, RI_ENGINE_S303A | RI_ENGINE_S808);
        ri_core_demo(&a);
        loud_slot3(&a);
        ri_core_init(&b, 96u, 48000.0f, 120.0f, RI_ENGINE_S303A | RI_ENGINE_S808);
        ri_core_demo(&b);
        loud_slot3(&b);
        ri_track_capture(&b.track, 2u, 0u, 3u);
        ri_core_play(&a);
        ri_core_play(&b);
        while (done < 96000u * 4u) {
            uint32_t gotA = ri_live_render(&a.session, aL + done, aR + done, 256u);
            uint32_t gotB = ri_live_render(&b.session, bL + done, bR + done, 256u);
            if (!gotA || !gotB)
                break;
            done += gotA;
        }
        RI_ASSERT(done == 96000u * 4u, "rendered %u", done);
        for (i = 0u; i < 3u; i++)
            RI_ASSERT(!memcmp(aL + barlen * i, bL + barlen * i, barlen * sizeof(float)),
                "pre-flip same bar %u", i);
        /* One-shot capture at bar 2 sounds during bar 3, then reverts to
         * the grid (bar 4 = slot 0). Persistence is the shell's sticky
         * re-capture (proven below), not the player. */
        RI_ASSERT(memcmp(aL + barlen * 3u, bL + barlen * 3u, barlen * sizeof(float)) != 0,
            "one-shot sounds bar 3");
    }
    /* Sticky: re-captured selections persist (what the shell does). */
    {
        static struct RIAppCore c;
        static float cL[256], cR[256];
        uint32_t n;
        ri_core_init(&c, 96u, 48000.0f, 120.0f, RI_ENGINE_S303A | RI_ENGINE_S808);
        ri_core_demo(&c);
        loud_slot3(&c);
        ri_track_capture(&c.track, 2u, 0u, 3u);
        ri_core_play(&c);
        for (n = 0u; n < 1500u; n++) {
            uint64_t bar = c.session.cursor_ticks / 384u;
            ri_core_capture_sel(&c, bar, 0u, 3u); /* shell: re-assert live sel */
            ri_live_render(&c.session, cL, cR, 256u);
        }
        RI_ASSERT(c.session.player.sounding_slot[0] == 3u, "sticky %u",
            c.session.player.sounding_slot[0]);
    }
    RI_RESULT("pattern_change");
}
