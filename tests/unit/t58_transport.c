/* t58_transport — §12.9a transport state machine.
 * Task 1 first (RED: transport.h does not exist yet).
 */
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "tests/helpers/ri_assert.h"
#include "engine/seq/transport.h"

int main(void) {
    struct RITransport t;
    uint64_t cur;
    int armed = 7;
    /* Law probe: after EVERY transition below that leaves STOPPED,
    * clicks must read 0. Checked inline at each site (not once at the
    * end) so a regression names its own transition. */
#define RI_T58_LAW(tt) RI_ASSERT((tt).state == RI_TR_STOPPED || (tt).clicks == 0u, "law")
    /* Play from STOPPED: continue, clicks 0. */
    t.state = RI_TR_STOPPED; t.clicks = 0; cur = 12345ULL;
    ri_tr_play(&t, &cur);
    RI_ASSERT(t.state == RI_TR_PLAYING, "play state");
    RI_ASSERT(cur == 12345ULL, "play cursor moved");
    RI_ASSERT(t.clicks == 0u, "play clicks");
    RI_T58_LAW(t);
    /* PLAYING + Play: no-op. */
    ri_tr_play(&t, &cur);
    RI_ASSERT(t.state == RI_TR_PLAYING && cur == 12345ULL, "play idempotent");
    RI_T58_LAW(t);
    /* RECORD + Stop: STOPPED click 1, cursor held. */
    t.state = RI_TR_RECORD; t.clicks = 0; cur = 999ULL;
    ri_tr_stop(&t, &cur, 0ULL, 0ULL);
    RI_ASSERT(t.state == RI_TR_STOPPED && t.clicks == 1u && cur == 999ULL, "rec stop");
    /* Stop law (ReBirth-inspired + deliberate extension — E1 ends at
    * the 3rd click; the 4th restarting as click 1 keeps every Stop press
    * meaningful and the machine total). The FIRST stop while STOPPED
    * (clicks==0) moves nothing — it only arms the next jump. */
    t.state = RI_TR_STOPPED; t.clicks = 1u; cur = 5000ULL;
    ri_tr_stop(&t, &cur, 777ULL, 0ULL);
    RI_ASSERT(cur == 777ULL && t.clicks == 2u, "stop2");
    ri_tr_stop(&t, &cur, 777ULL, 0ULL);
    RI_ASSERT(cur == 0ULL && t.clicks == 0u, "stop3");
    ri_tr_stop(&t, &cur, 777ULL, 0ULL);
    RI_ASSERT(cur == 0ULL && t.clicks == 1u, "stop4 restarts");
    /* First stop while STOPPED with clicks==0: arms only. */
    t.state = RI_TR_STOPPED; t.clicks = 0u; cur = 4242ULL;
    ri_tr_stop(&t, &cur, 777ULL, 0ULL);
    RI_ASSERT(cur == 4242ULL && t.clicks == 1u, "stop1 arms");
    /* Record button matrix. */
    t.state = RI_TR_STOPPED; t.clicks = 2u; cur = 11ULL;
    ri_tr_record(&t, &cur);
    RI_ASSERT(t.state == RI_TR_RECORD && cur == 11ULL && t.clicks == 0u, "rec from stop");
    RI_T58_LAW(t);
    t.state = RI_TR_PLAYING; cur = 11ULL;
    ri_tr_record(&t, &cur);
    RI_ASSERT(t.state == RI_TR_RECORD && cur == 11ULL, "punch in");
    RI_T58_LAW(t);
    ri_tr_record(&t, &cur);
    RI_ASSERT(t.state == RI_TR_PLAYING && cur == 11ULL, "punch out");
    RI_T58_LAW(t);
    /* Null-safe, armed untouched by every call above. Law holds on this
    * path too: the PLAYING exit zeroed clicks. */
    ri_tr_play(0, &cur); ri_tr_play(&t, 0); ri_tr_stop(0, 0, 0, 0); ri_tr_record(0, 0);
    RI_ASSERT(armed == 7, "armed touched");
    /* Bar helpers: ppq 96, bar = 384 ticks. */
    RI_ASSERT(ri_seq_tick_of_bar(96u, 0u) == 0u, "bar0 tick");
    RI_ASSERT(ri_seq_tick_of_bar(96u, 3u) == 1152u, "bar3 tick");
    RI_ASSERT(ri_seq_bar_at_tick(0u, 96u) == 0u, "tick0 bar");
    RI_ASSERT(ri_seq_bar_at_tick(383u, 96u) == 0u, "floor");
    RI_ASSERT(ri_seq_bar_at_tick(384u, 96u) == 1u, "rollover");
    RI_ASSERT(ri_seq_tick_of_bar(0u, 5u) == 5u * 384u, "ppq0 tick");
    RI_ASSERT(ri_seq_bar_at_tick(384u * 7u + 1u, 0u) == 7u, "ppq0 bar");
    /* Seeks take song_bars (not song_ticks): the caller owns the bar
    * count, so no tick->bar re-derivation and no tempo-map window. */
    t.state = RI_TR_STOPPED; t.clicks = 2u; cur = ri_seq_tick_of_bar(96u, 20u);
    ri_tr_seek_bars(&t, &cur, 96u, -10, 100u);
    RI_ASSERT(cur == ri_seq_tick_of_bar(96u, 10u) && t.clicks == 0u &&
        t.state == RI_TR_STOPPED, "seek back");
    RI_T58_LAW(t);
    ri_tr_seek_bars(&t, &cur, 96u, -50, 100u);
    RI_ASSERT(cur == 0u, "seek clamps front");
    RI_T58_LAW(t);
    ri_tr_seek_bars(&t, &cur, 96u, 500, 100u);
    RI_ASSERT(cur == ri_seq_tick_of_bar(96u, 99u), "seek clamps end");
    RI_T58_LAW(t);
    /* End-boundary pair: exactly-to-end vs one-beyond (inclusive/exclusive). */
    cur = ri_seq_tick_of_bar(96u, 99u);
    ri_tr_seek_bars(&t, &cur, 96u, 1, 100u);
    RI_ASSERT(cur == ri_seq_tick_of_bar(96u, 99u), "seek at end holds");
    cur = ri_seq_tick_of_bar(96u, 98u);
    ri_tr_seek_bars(&t, &cur, 96u, 1, 100u);
    RI_ASSERT(cur == ri_seq_tick_of_bar(96u, 99u), "seek into end");
    /* Empty / singleton songs. */
    cur = 55555ULL;
    ri_tr_seek_bars(&t, &cur, 96u, 10, 0u);
    RI_ASSERT(cur == 0u, "empty song tick0");
    cur = ri_seq_tick_of_bar(96u, 0u);
    ri_tr_seek_bars(&t, &cur, 96u, 5, 1u);
    RI_ASSERT(cur == 0u, "singleton holds");
    /* Malformed internal state: PLAYING must still exit with clicks 0. */
    t.state = RI_TR_PLAYING; t.clicks = 17u; cur = 0ULL;
    ri_tr_record(&t, &cur);
    RI_ASSERT(t.state == RI_TR_RECORD && t.clicks == 0u, "malformed record");
    RI_T58_LAW(t);
    t.state = RI_TR_PLAYING;
    ri_tr_seek_bars(&t, &cur, 96u, 1, 100u);
    RI_ASSERT(t.state == RI_TR_PLAYING, "seek keeps state");
    RI_T58_LAW(t);
    /* Property loops: round-trip every legal bar + interior tick stays
    * in-bar (stronger than the hand-picked 0/3/383/384 pins above). */
    {
        uint64_t b;
        for (b = 0u; b <= 999u; b++) {
            uint64_t tk = ri_seq_tick_of_bar(96u, b);
            RI_ASSERT(ri_seq_bar_at_tick(tk, 96u) == b, "bar roundtrip %llu",
                (unsigned long long)b);
        }
        for (b = 0u; b < 999u; b++) {
            uint64_t tk = ri_seq_tick_of_bar(96u, b) + 383u;
            RI_ASSERT(ri_seq_bar_at_tick(tk, 96u) == b, "bar interior %llu",
                (unsigned long long)b);
        }
    }
    /* Layer guard (cheap textual tripwire, NOT a dependency proof —
    * the compiler include structure in Task 4 is the real enforcement):
    * transport.h includes stdint.h and nothing else (no RISeq
    * visibility). Breaks here if anyone smuggles a struct into the
    * free functions. */
    {
        FILE *fh = fopen("engine/seq/transport.h", "r");
        char line[256]; int bad = 0;
        RI_ASSERT(fh != 0, "open header");
        if (fh) {
            while (fgets(line, sizeof line, fh)) {
                if (strstr(line, "#include") && !strstr(line, "stdint.h"))
                    bad = 1;
                if (strstr(line, "RISeq"))
                    bad = 1;
            }
            fclose(fh);
        }
        RI_ASSERT(!bad, "layer leak");
    }
    /* Display is a pure projection of (tick, ppq) — no clamp inside.
    * Clamping belongs to the cursor owner: the caller clamps the tick
    * (seek path) before projecting. Past-end therefore pins as raw bar
    * math here; the clamped-panel case is pinned by the caller-side
    * sequence below it. */
    {
        struct RIBarPos dp = ri_seq_bar_display(0u, 96u);
        RI_ASSERT(dp.bar == 1u && dp.beat == 1u && dp.sixteenth == 1u, "1.1.1");
        dp = ri_seq_bar_display(383u, 96u);
        RI_ASSERT(dp.bar == 1u && dp.beat == 4u && dp.sixteenth == 4u, "1.4.4");
        dp = ri_seq_bar_display(384u, 96u);
        RI_ASSERT(dp.bar == 2u && dp.beat == 1u && dp.sixteenth == 1u, "2.1.1");
        dp = ri_seq_bar_display(384u * 100u + 57u, 96u);
        RI_ASSERT(dp.bar == 101u, "past-end projects raw %u", dp.bar);
        dp = ri_seq_bar_display(0u, 0u);
        RI_ASSERT(dp.bar == 1u && dp.beat == 1u && dp.sixteenth == 1u, "ppq0 disp");
        dp = ri_seq_bar_display(96u, 1u); /* sub-minimum -> default, no div0:
        * raw ppq=1 would say bar 25; normalized 96 says (1,2,1). */
        RI_ASSERT(dp.bar == 1u && dp.beat == 2u && dp.sixteenth == 1u, "ppq1 safe");
        dp = ri_seq_bar_display(0u, 4u);
        RI_ASSERT(dp.bar == 1u && dp.beat == 1u && dp.sixteenth == 1u, "ppq4 min");
        /* Caller-side clamp: song of 100 bars, tick past the end shows
        * the last bar (spec §4 past-end display law). */
        {
            uint64_t song_ticks = ri_seq_tick_of_bar(96u, 100u);
            uint64_t past = song_ticks + 57u;
            uint64_t shown = past >= song_ticks
                ? ri_seq_tick_of_bar(96u, 100u - 1u) : past;
            dp = ri_seq_bar_display(shown, 96u);
            RI_ASSERT(dp.bar == 100u, "panel clamps %u", dp.bar);
        }
    }
#undef RI_T58_LAW
    RI_RESULT("transport");
}
