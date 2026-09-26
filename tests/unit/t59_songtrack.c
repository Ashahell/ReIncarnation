/* t59_songtrack — §12.9b song track model.
 * Task 1 first (RED: songtrack.h does not exist yet).
 */
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "tests/helpers/ri_assert.h"
#include "engine/seq/songtrack.h"
#include "engine/seq/songtrack_emit.h"   /* emitter: RIEvent/RITempoMap users */
#include "project/rbng.h"   /* STRK read/write + rbng_test_* helpers */

/* Song geometry comes from transport.h; the track must not fork it. */
typedef char ri_t59_bars_match[(RI_SONGTRACK_BARS == RI_SONG_BARS) ? 1 : -1];

int main(void) {
    struct RISongTrack t;
    uint64_t b;
    uint32_t i;
    (void)sizeof(ri_t59_bars_match);
    RI_ASSERT(RI_SONGTRACK_BARS == 999u, "bars %u", RI_SONGTRACK_BARS);
    ri_track_init(&t);
    RI_ASSERT(ri_track_is_empty(&t) == 1, "fresh empty");
    RI_ASSERT(ri_track_selected(&t, 0u, 0u) == 0u, "init slot");
    RI_ASSERT(ri_track_selected(&t, 998u, 3u) == 0u, "init tail");
    /* Fail-closed reads: bar 999/5000, instance 4/255, NULL track. */
    RI_ASSERT(ri_track_selected(&t, 0u, 4u) == 0u, "read inst 4");
    RI_ASSERT(ri_track_selected(&t, 0u, 255u) == 0u, "read inst 255");
    RI_ASSERT(ri_track_selected(0, 0u, 0u) == 0u, "read null");
    /* Boundary reads are pinned through a WRAPPER, so a botched range check
     * lands in `guard` (a wrong value and a wrong byte) instead of reading
     * undefined memory past a bare array and happening to see zero. */
    {
        struct { struct RISongTrack t; unsigned char guard[32]; } w;
        uint32_t g;
        memset(&w, 0x5A, sizeof w);
        ri_track_init(&w.t);
        RI_ASSERT(ri_track_selected(&w.t, 999u, 0u) == 0u, "read bar 999");
        RI_ASSERT(ri_track_selected(&w.t, 5000u, 0u) == 0u, "read bar 5000");
        for (g = 0u; g < sizeof w.guard; g++)
            RI_ASSERT(w.guard[g] == 0x5A, "read past the grid");
    }
    /* Capture writes exactly; neighbors untouched. */
    RI_ASSERT(ri_track_capture(&t, 5u, 1u, 7u) == 0, "capture rc");
    RI_ASSERT(ri_track_selected(&t, 5u, 1u) == 7u, "capture stored");
    RI_ASSERT(ri_track_is_empty(&t) == 0, "non-empty");
    RI_ASSERT(ri_track_selected(&t, 4u, 1u) == 0u, "neighbor low");
    RI_ASSERT(ri_track_selected(&t, 6u, 1u) == 0u, "neighbor high");
    RI_ASSERT(ri_track_selected(&t, 5u, 0u) == 0u, "neighbor inst");
    /* Same-bar re-capture overwrites. */
    RI_ASSERT(ri_track_capture(&t, 5u, 1u, 3u) == 0, "recapture rc");
    RI_ASSERT(ri_track_selected(&t, 5u, 1u) == 3u, "recapture wins");
    /* Refusals: nothing stored, neighbors untouched. */
    RI_ASSERT(ri_track_capture(&t, 999u, 0u, 9u) == 2, "cap bar 999");
    RI_ASSERT(ri_track_capture(&t, 0u, 4u, 9u) == 2, "cap inst 4");
    RI_ASSERT(ri_track_capture(&t, 0u, 0u, 32u) == 2, "cap slot 32");
    RI_ASSERT(ri_track_capture(0, 0u, 0u, 1u) == 2, "cap null");
    RI_ASSERT(ri_track_selected(&t, 998u, 0u) == 0u, "refusal leak");
    /* Highest legal values land. */
    RI_ASSERT(ri_track_capture(&t, 998u, 3u, 31u) == 0, "top rc");
    RI_ASSERT(ri_track_selected(&t, 998u, 3u) == 31u, "top stored");
    /* Composition with the transport quantizer: a mid-measure flip at
     * bar 998 quantizes to 998 (never 999) and stores there. */
    RI_ASSERT(ri_bar_quantize_next(998u * 384u + 200u, 96u) == 998u, "q 998");
    RI_ASSERT(ri_track_capture(&t, ri_bar_quantize_next(998u * 384u + 200u, 96u),
        0u, 9u) == 0, "q cap rc");
    RI_ASSERT(ri_track_selected(&t, 998u, 0u) == 9u, "q cap stored");
    /* Layer guard: songtrack.h sees transport.h geometry only. */
    {
        FILE *fh = fopen("engine/seq/songtrack.h", "r");
        char line[256];
        int bad = 0, has_transport = 0;
        RI_ASSERT(fh != 0, "open header");
        if (fh) {
            while (fgets(line, sizeof line, fh)) {
                if (strstr(line, "transport.h"))
                    has_transport = 1;
                /* BANNED set, permanent: the emitter's types live in
                 * songtrack_emit.h (Task 2), so this guard is never narrowed.
                 * Never delete it to make a build pass. */
                if (strstr(line, "RISeq") || strstr(line, "RITransport") ||
                    strstr(line, "RILoop") || strstr(line, "sched.h") ||
                    strstr(line, "clock.h") || strstr(line, "pattern.h"))
                    bad = 1;
            }
            fclose(fh);
        }
        RI_ASSERT(has_transport, "no transport include");
        RI_ASSERT(!bad, "layer leak");
    }
    /* Layer guard 2: the emitter header may add sched/clock, nothing else. */
    {
        FILE *fh = fopen("engine/seq/songtrack_emit.h", "r");
        char line[256];
        int bad = 0, has_model = 0;
        RI_ASSERT(fh != 0, "open emit header");
        if (fh) {
            while (fgets(line, sizeof line, fh)) {
                if (strstr(line, "songtrack.h\""))
                    has_model = 1;
                if (strstr(line, "RISeq") || strstr(line, "RITransport") ||
                    strstr(line, "pattern.h") || strstr(line, "rbng.h"))
                    bad = 1;
            }
            fclose(fh);
        }
        RI_ASSERT(has_model, "emit header must build on the model");
        RI_ASSERT(!bad, "emit layer leak");
    }
    /* Property loop: all-999 round-trip. */
    for (b = 0u; b < RI_SONGTRACK_BARS; b++)
        for (i = 0u; i < RI_SONGTRACK_INSTANCES; i++)
            RI_ASSERT(ri_track_capture(&t, b, i, (uint8_t)(b % 32u)) == 0, "prop cap");
    for (b = 0u; b < RI_SONGTRACK_BARS; b++)
        for (i = 0u; i < RI_SONGTRACK_INSTANCES; i++)
            RI_ASSERT(ri_track_selected(&t, b, i) == (uint8_t)(b % 32u),
                "roundtrip %llu/%u", (unsigned long long)b, i);
    /* ---- emission ---- */
    {
        /* 140 BPM fixture, same shape as t55/pattern_emit tests. */
        static struct RISegment seg = { 0ULL, 428571428ULL };
        struct RITempoMap map;
        struct RISongTrack tr;
        struct RITrackCarry carry;
        struct RIEvent ev[64];
        uint32_t n, seq, k;
        map.segs = &seg; map.n = 1u; map.ppq = 96u; map.sr = 48000u;

        /* Establishment: a fresh carry emits all four slots at the bar. */
        ri_track_init(&tr);
        ri_track_capture(&tr, 3u, 0u, 5u);
        memset(&carry, 0, sizeof carry);
        n = 0u; seq = 0u;
        RI_ASSERT(ri_track_emit_measure(&tr, 3u, &carry, &map, 96u, ev, &n, 64u, &seq) == 4u,
            "establish added %u", n);
        RI_ASSERT(n == 4u && seq == 4u, "establish count");
        for (k = 0u; k < 4u; k++) {
            RI_ASSERT(ev[k].type == RI_EV_PATTERN_CHANGE, "type %u", ev[k].type);
            RI_ASSERT(ev[k].device == (uint16_t)k && ev[k].voice == 0u, "dev %u", k);
            RI_ASSERT(ev[k].sample == ri_map_tick(&map, ri_seq_tick_of_bar(96u, 3u)),
                "sample %u", k);
            RI_ASSERT(ev[k].seq == k, "seq %u", k);
        }
        RI_ASSERT(ev[0].value == 5u && ev[1].value == 0u, "values");
        RI_ASSERT(carry.known == 0x0Fu && carry.prev[0] == 5u, "carry seeded");

        /* Same bar again: no change, no events. */
        n = 0u;
        RI_ASSERT(ri_track_emit_measure(&tr, 3u, &carry, &map, 96u, ev, &n, 64u, &seq) == 0u,
            "steady added");

        /* Change to slot 0 fires (slot 0 is first-class), and back again. */
        ri_track_capture(&tr, 3u, 0u, 0u);
        n = 0u;
        RI_ASSERT(ri_track_emit_measure(&tr, 3u, &carry, &map, 96u, ev, &n, 64u, &seq) == 1u,
            "to-zero added");
        RI_ASSERT(ev[0].device == 0u && ev[0].value == 0u, "to-zero ev");
        ri_track_capture(&tr, 3u, 1u, 7u);
        n = 0u;
        RI_ASSERT(ri_track_emit_measure(&tr, 3u, &carry, &map, 96u, ev, &n, 64u, &seq) == 1u,
            "one-lane added");
        RI_ASSERT(ev[0].device == 1u && ev[0].value == 7u, "one-lane ev");

        /* End boundary is never a valid start, even with force pending. */
        memset(&carry, 0, sizeof carry);
        n = 0u;
        RI_ASSERT(ri_track_emit_measure(&tr, 999u, &carry, &map, 96u, ev, &n, 64u, &seq) == 0u,
            "measure bar 999");
        RI_ASSERT(carry.known == 0u, "999 left carry cold");

        /* Cap pressure: 4 pending, cap 2 -> instances 0,1 go out; 2,3 stay
         * PENDING (carry mirrors what was emitted) and are re-sent at the
         * next measure call — late, never lost. */
        memset(&carry, 0, sizeof carry);
        n = 0u;
        RI_ASSERT(ri_track_emit_measure(&tr, 4u, &carry, &map, 96u, ev, &n, 2u, &seq) == 2u,
            "cap added");
        RI_ASSERT(n == 2u && carry.known == 0x03u, "cap state %u", (unsigned)carry.known);
        n = 0u;
        RI_ASSERT(ri_track_emit_measure(&tr, 4u, &carry, &map, 96u, ev, &n, 64u, &seq) == 2u,
            "capped changes re-sent");
        RI_ASSERT(ev[0].device == 2u && ev[1].device == 3u && carry.known == 0x0Fu, "re-sent lanes");

        /* NULL safety. */
        n = 0u;
        RI_ASSERT(ri_track_emit_measure(0, 0u, &carry, &map, 96u, ev, &n, 64u, &seq) == 0u,
            "measure null track");
        RI_ASSERT(ri_track_emit_measure(&tr, 0u, 0, &map, 96u, ev, &n, 64u, &seq) == 0u,
            "measure null carry");
        RI_ASSERT(ri_track_emit_measure(&tr, 0u, &carry, 0, 96u, ev, &n, 64u, &seq) == 0u,
            "measure null map");
        RI_ASSERT(ri_track_emit_measure(&tr, 0u, &carry, &map, 96u, 0, &n, 64u, &seq) == 0u,
            "measure null out");
        RI_ASSERT(ri_track_emit_measure(&tr, 0u, &carry, &map, 96u, ev, 0, 64u, &seq) == 0u,
            "measure null n");
        RI_ASSERT(ri_track_emit_measure(&tr, 0u, &carry, &map, 96u, ev, &n, 64u, 0) == 0u,
            "measure null seq");
        RI_ASSERT(ri_song_ended(998u) == 0 && ri_song_ended(999u) == 1,
            "ended predicate");
    }
    /* ---- range walker: establishment spans windows, wrap keeps phase ---- */
    {
        static struct RISegment seg = { 0ULL, 428571428ULL };
        struct RITempoMap map;
        struct RISongTrack tr;
        struct RITrackCarry carry;
        struct RILoop lp;
        struct RIEvent ev[64];
        uint32_t n, k;
        map.segs = &seg; map.n = 1u; map.ppq = 96u; map.sr = 48000u;
        ri_track_init(&tr);
        ri_track_capture(&tr, 5u, 0u, 6u);

        /* Loop OFF: bars 3,4,5,6. Establishment at 3 (4 events), nothing at
         * 4, the rise at 5 (+1), and the fall back to 0 at 6 (+1). */
        memset(&carry, 0, sizeof carry);
        n = ri_track_emit_range(&tr, 3u, 4u, 0, &map, 96u, &carry, ev, 64u);
        RI_ASSERT(n == 6u, "offline range %u", n);
        RI_ASSERT(ev[0].sample == ri_map_tick(&map, ri_seq_tick_of_bar(96u, 3u)),
            "offline first sample");
        RI_ASSERT(ev[4].sample == ri_map_tick(&map, ri_seq_tick_of_bar(96u, 5u)),
            "offline change sample");

        /* The NEXT window does not re-establish (carry crosses blocks). */
        n = ri_track_emit_range(&tr, 3u, 2u, 0, &map, 96u, &carry, ev, 64u);
        RI_ASSERT(n == 0u, "second window re-established %u", n);

        /* Loop ON [4,6): 4 bars from 3 -> 3(est 4), 4(0), 5(+1), wrap->4(+1) = 6. */
        lp.on = 1u; lp.start_bar = 4u; lp.len_bars = 2u;
        memset(&carry, 0, sizeof carry);
        n = ri_track_emit_range(&tr, 3u, 4u, &lp, &map, 96u, &carry, ev, 64u);
        RI_ASSERT(n == 6u, "wrap4 count %u", n);
        if (n > 0u)
            RI_ASSERT(ev[n - 1u].sample == ri_map_tick(&map, ri_seq_tick_of_bar(96u, 4u)),
                "wrap lands on the loop start here");

        /* PHASE PIN — 8 bars: 3,4,5,4,5,4,5,4 -> 4+0+1+1+1+1+1+1 = 10. */
        memset(&carry, 0, sizeof carry);
        n = ri_track_emit_range(&tr, 3u, 8u, &lp, &map, 96u, &carry, ev, 64u);
        RI_ASSERT(n == 10u, "phase count %u (6 = wrap lost phase)", n);
        if (n >= 2u) {
            RI_ASSERT(ev[n - 1u].sample == ri_map_tick(&map, ri_seq_tick_of_bar(96u, 4u)),
                "phase tail bar 4");
            RI_ASSERT(ev[n - 2u].sample == ri_map_tick(&map, ri_seq_tick_of_bar(96u, 5u)),
                "phase tail bar 5");
        }

        /* Starting past the loop end folds into the loop (both bars legal). */
        memset(&carry, 0, sizeof carry);
        n = ri_track_emit_range(&tr, 900u, 2u, &lp, &map, 96u, &carry, ev, 64u);
        RI_ASSERT(n == 5u, "fold count %u", n);
        RI_ASSERT(ev[0].sample == ri_map_tick(&map, ri_seq_tick_of_bar(96u, 4u)),
            "fold lands in loop");

        /* Zero-length loop with on set behaves as OFF (no modulo by 0). */
        lp.on = 1u; lp.start_bar = 4u; lp.len_bars = 0u;
        memset(&carry, 0, sizeof carry);
        n = ri_track_emit_range(&tr, 3u, 4u, &lp, &map, 96u, &carry, ev, 64u);
        RI_ASSERT(n == 6u, "zero-len loop %u", n);

        /* Loop ON never reaches bar 999 (E1: end-of-song cannot fire). */
        lp.on = 1u; lp.start_bar = 4u; lp.len_bars = 2u;
        memset(&carry, 0, sizeof carry);
        n = ri_track_emit_range(&tr, 990u, 20u, &lp, &map, 96u, &carry, ev, 64u);
        RI_ASSERT(n > 0u, "looped walk emitted");
        for (k = 0u; k < n; k++)
            RI_ASSERT(ev[k].sample < ri_map_tick(&map, ri_seq_tick_of_bar(96u, 999u)),
                "loop reached the end boundary");

        /* A RAW loop past the song end is normalized first (ri_loop_clamp):
         * [995, 1005) -> [995, 999), so the walk wraps instead of stopping at
         * 999. Track: 995/inst1 = 3, so every pass over 995,996 adds 2.
         * 20 bars from 990: est 4 + four passes x 2 = 12. Unclamped: 6. */
        ri_track_capture(&tr, 995u, 1u, 3u);
        lp.on = 1u; lp.start_bar = 995u; lp.len_bars = 10u;
        memset(&carry, 0, sizeof carry);
        n = ri_track_emit_range(&tr, 990u, 20u, &lp, &map, 96u, &carry, ev, 64u);
        RI_ASSERT(n == 12u, "raw loop count %u (6 = loop not clamped)", n);
        for (k = 0u; k < n; k++)
            RI_ASSERT(ev[k].sample < ri_map_tick(&map, ri_seq_tick_of_bar(96u, 999u)),
                "raw loop reached the end boundary");
        ri_track_capture(&tr, 995u, 1u, 0u);

        /* No loop: the walk truncates before the end boundary. */
        memset(&carry, 0, sizeof carry);
        n = ri_track_emit_range(&tr, 997u, 5u, 0, &map, 96u, &carry, ev, 64u);
        RI_ASSERT(n == 4u, "truncate count %u", n);

        /* Edges. */
        RI_ASSERT(ri_track_emit_range(0, 0u, 4u, 0, &map, 96u, &carry, ev, 64u) == 0u, "range null t");
        RI_ASSERT(ri_track_emit_range(&tr, 0u, 0u, 0, &map, 96u, &carry, ev, 64u) == 0u, "range count 0");
        RI_ASSERT(ri_track_emit_range(&tr, 0u, 4u, 0, &map, 96u, &carry, ev, 0u) == 0u, "range cap 0");
        RI_ASSERT(ri_track_emit_range(&tr, 0u, 4u, 0, 0, 96u, &carry, ev, 64u) == 0u, "range null map");
        RI_ASSERT(ri_track_emit_range(&tr, 0u, 4u, 0, &map, 96u, 0, ev, 64u) == 0u, "range null carry");
        RI_ASSERT(ri_track_emit_range(&tr, 0u, 4u, 0, &map, 96u, &carry, 0, 64u) == 0u, "range null out");
        memset(&carry, 0, sizeof carry);
        RI_ASSERT(ri_track_emit_range(&tr, 999u, 4u, 0, &map, 96u, &carry, ev, 64u) == 0u,
            "range past end no loop");
    }
    /* ---- replay + sweep properties (spec §6) ---- */
    {
        static struct RISegment seg = { 0ULL, 428571428ULL };
        static struct RIEvent big[4096];
        struct RITempoMap map;
        struct RISongTrack tr;
        struct RITrackCarry carry;
        uint8_t cur[4];
        uint32_t n, e, i;
        uint64_t b;
        map.segs = &seg; map.n = 1u; map.ppq = 96u; map.sr = 48000u;
        ri_track_init(&tr);
        for (b = 0u; b < RI_SONGTRACK_BARS; b++)
            for (i = 0u; i < RI_SONGTRACK_INSTANCES; i++)
                (void)ri_track_capture(&tr, b, i, (uint8_t)(((b / (i + 1u)) * 7u + i) % 32u));
        memset(&carry, 0, sizeof carry);
        n = ri_track_emit_range(&tr, 0u, RI_SONGTRACK_BARS, 0, &map, 96u, &carry, big, 4096u);
        memset(cur, 0xFF, sizeof cur); /* unset lanes can never match */
        e = 0u;
        for (b = 0u; b < RI_SONGTRACK_BARS; b++) {
            uint64_t smp = ri_map_tick(&map, ri_seq_tick_of_bar(96u, b));
            while (e < n && big[e].sample == smp) {
                RI_ASSERT(big[e].type == RI_EV_PATTERN_CHANGE && big[e].device < 4u, "replay ev %u", e);
                if (big[e].device < 4u)
                    cur[big[e].device] = (uint8_t)big[e].value;
                e++;
            }
            for (i = 0u; i < RI_SONGTRACK_INSTANCES; i++)
                RI_ASSERT(cur[i] == ri_track_selected(&tr, b, i),
                    "replay bar %llu inst %u", (unsigned long long)b, i);
        }
        RI_ASSERT(e == n, "replay consumed %u of %u", e, n);
        ri_track_init(&tr);
        for (b = 0u; b < RI_SONGTRACK_BARS; b++)
            (void)ri_track_capture(&tr, b, 0u, (uint8_t)(b % 2u));
        memset(&carry, 0, sizeof carry);
        n = ri_track_emit_range(&tr, 0u, RI_SONGTRACK_BARS, 0, &map, 96u, &carry, big, 4096u);
        memset(&carry, 0, sizeof carry);
        n = ri_track_emit_range(&tr, 0u, RI_SONGTRACK_BARS, 0, &map, 96u, &carry, big, 4096u);
        RI_ASSERT(n == 4u + 998u, "sweep %u", n);
    }
    /* ---- edits ---- */
    {
        struct RISongTrack tr;
        struct RITrackClip clip;
        uint8_t s4[4];
        uint8_t bad4[4];
        uint64_t b;
        uint32_t i;

        /* init-song: every bar, every instance. */
        s4[0] = 3u; s4[1] = 1u; s4[2] = 0u; s4[3] = 2u;
        ri_track_init(&tr);
        ri_track_capture(&tr, 500u, 0u, 9u);
        ri_track_init_song(&tr, s4);
        for (b = 0u; b < RI_SONGTRACK_BARS; b++)
            for (i = 0u; i < RI_SONGTRACK_INSTANCES; i++)
                RI_ASSERT(ri_track_selected(&tr, b, i) == s4[i],
                    "init song %llu/%u", (unsigned long long)b, i);
        /* Slot law (one law, no masking): {32,40,1,2} is refused whole and
         * the track keeps the previous init-song content. */
        bad4[0] = 32u; bad4[1] = 40u; bad4[2] = 1u; bad4[3] = 2u;
        RI_ASSERT(ri_track_init_song(&tr, bad4) == 2, "init song refuses slot 32/40");
        RI_ASSERT(ri_track_selected(&tr, 0u, 0u) == 3u && ri_track_selected(&tr, 998u, 2u) == 0u,
            "refused init song left the track untouched");
        RI_ASSERT(ri_track_init_loop(&tr, bad4, 0u, 4u) == 2 && ri_track_selected(&tr, 1u, 1u) == 1u,
            "init loop refuses bad slots, all-or-nothing");

        /* init-loop: EVERY bar inside the loop (E1 p. 176), nothing outside. */
        ri_track_init(&tr);
        ri_track_capture(&tr, 12u, 0u, 9u);   /* prior content inside */
        ri_track_capture(&tr, 20u, 0u, 9u);   /* content outside */
        ri_track_init_loop(&tr, s4, 10u, 4u);
        for (b = 10u; b < 14u; b++)
            for (i = 0u; i < RI_SONGTRACK_INSTANCES; i++)
                RI_ASSERT(ri_track_selected(&tr, b, i) == s4[i],
                    "init loop %llu/%u", (unsigned long long)b, i);
        RI_ASSERT(ri_track_selected(&tr, 14u, 0u) == 0u, "init loop above");
        RI_ASSERT(ri_track_selected(&tr, 20u, 0u) == 9u, "init loop outside kept");
        /* Loop range clamps at the song end; zero len and past-end are no-ops. */
        ri_track_init(&tr);
        ri_track_init_loop(&tr, s4, 997u, 10u);
        RI_ASSERT(ri_track_selected(&tr, 997u, 0u) == 3u &&
            ri_track_selected(&tr, 998u, 3u) == 2u, "init loop clamped");
        ri_track_init(&tr);
        RI_ASSERT(ri_track_init_loop(&tr, s4, 10u, 0u) == 0 &&
            ri_track_init_loop(&tr, s4, 999u, 4u) == 0, "empty ranges are no-ops, not refusals");
        RI_ASSERT(ri_track_is_empty(&tr) == 1, "init loop no-ops");

        /* copy: pure, clamped, and overflow-proof on a huge len. */
        ri_track_init(&tr);
        for (b = 0u; b < RI_SONGTRACK_BARS; b++)
            for (i = 0u; i < RI_SONGTRACK_INSTANCES; i++)
                (void)ri_track_capture(&tr, b, i, (uint8_t)(b % 8u));
        ri_track_copy(&tr, 5u, 4u, &clip);
        RI_ASSERT(clip.len == 4u, "copy len %u", (unsigned)clip.len);
        RI_ASSERT(clip.slot[0][0] == 5u && clip.slot[3][0] == 0u, "copy rows");
        RI_ASSERT(ri_track_selected(&tr, 5u, 0u) == 5u, "copy is pure");
        ri_track_copy(&tr, 998u, 5u, &clip);
        RI_ASSERT(clip.len == 1u, "copy clamps %u", (unsigned)clip.len);
        /* Overflow pin, non-zero start: 500 + (2^64-1) WRAPS to 499, so a bare
         * `start + len > BARS` check waves the huge len through and the fill
         * loop runs off the grid. The clamp is evaluated before any addition. */
        ri_track_copy(&tr, 500u, ~(uint64_t)0, &clip);
        RI_ASSERT(clip.len == 499u, "copy huge len %u", (unsigned)clip.len);
        ri_track_copy(&tr, 0u, ~(uint64_t)0, &clip);
        RI_ASSERT(clip.len == 999u, "copy huge len at 0 %u", (unsigned)clip.len);

        /* cut: clip holds the range, the tail closes the gap, the freed end
         * fills with slot 0 without reading past bar 998. */
        ri_track_cut(&tr, 2u, 3u, &clip);
        RI_ASSERT(clip.len == 3u && clip.slot[0][0] == 2u && clip.slot[2][0] == 4u, "cut clip");
        RI_ASSERT(ri_track_selected(&tr, 2u, 0u) == 5u, "gap closed");
        RI_ASSERT(ri_track_selected(&tr, 5u, 0u) == 0u, "shift alias");
        RI_ASSERT(ri_track_selected(&tr, 995u, 0u) == 6u, "shift tail from 998");
        RI_ASSERT(ri_track_selected(&tr, 996u, 0u) == 0u &&
            ri_track_selected(&tr, 998u, 0u) == 0u, "freed end zeroed");
        /* Cut aimed past the end clamps to the last bar available. */
        ri_track_cut(&tr, 997u, 5u, &clip);
        RI_ASSERT(clip.len == 2u, "cut end len %u", (unsigned)clip.len);
        RI_ASSERT(ri_track_selected(&tr, 997u, 0u) == 0u &&
            ri_track_selected(&tr, 998u, 0u) == 0u, "cut end zeroed");
        /* No-op cuts still clear the clip (stale clipboard is a bug source). */
        ri_track_cut(&tr, 999u, 3u, &clip);
        RI_ASSERT(clip.len == 0u, "cut past end");
        ri_track_cut(&tr, 0u, 0u, &clip);
        RI_ASSERT(clip.len == 0u, "cut zero len");

        /* paste inserts (tail shifts right, drops past the end). */
        ri_track_init(&tr);
        for (b = 0u; b < 6u; b++)
            (void)ri_track_capture(&tr, b, 0u, (uint8_t)(b + 1u));
        ri_track_copy(&tr, 0u, 2u, &clip);
        RI_ASSERT(clip.len == 2u && clip.slot[0][0] == 1u && clip.slot[1][0] == 2u,
            "paste pre");
        ri_track_paste(&tr, 3u, &clip);
        RI_ASSERT(ri_track_selected(&tr, 2u, 0u) == 3u, "insert kept bar 2");
        RI_ASSERT(ri_track_selected(&tr, 3u, 0u) == 1u && ri_track_selected(&tr, 4u, 0u) == 2u,
            "inserted clip");
        RI_ASSERT(ri_track_selected(&tr, 5u, 0u) == 4u && ri_track_selected(&tr, 6u, 0u) == 5u,
            "tail shifted");
        RI_ASSERT(ri_track_selected(&tr, 8u, 0u) == 0u, "tail end clear");
        /* Overflow: the 2-bar clip pasted at bar 998 keeps only bar 998. */
        RI_ASSERT(ri_track_paste(&tr, 998u, &clip) == 0, "overflow paste rc");
        RI_ASSERT(ri_track_selected(&tr, 998u, 0u) == 1u, "paste dropped overflow");
        /* Hand-built clip with an out-of-range slot in its LAST row is refused
         * whole: nothing shifts, nothing is written (validate before write). */
        clip.len = 2u;
        memset(clip.slot, 0, sizeof clip.slot);
        clip.slot[1][3] = 40u;
        RI_ASSERT(ri_track_paste(&tr, 0u, &clip) == 2, "paste refuses clip slot 40");
        RI_ASSERT(ri_track_selected(&tr, 0u, 0u) == 1u && ri_track_selected(&tr, 3u, 0u) == 1u,
            "refused paste shifted nothing");
        RI_ASSERT(ri_track_paste_replace(&tr, 0u, &clip) == 2 && ri_track_selected(&tr, 0u, 0u) == 1u,
            "paste-replace refuses too");
        clip.len = 1000u;
        clip.slot[1][3] = 0u;
        RI_ASSERT(ri_track_paste(&tr, 0u, &clip) == 2, "clip len 1000 is malformed");

        /* paste-replace overwrites in place, leaving the tail alone. */
        ri_track_init(&tr);
        for (b = 0u; b < 8u; b++)
            (void)ri_track_capture(&tr, b, 0u, (uint8_t)(b + 1u));
        ri_track_copy(&tr, 0u, 2u, &clip);
        ri_track_paste_replace(&tr, 3u, &clip);
        RI_ASSERT(ri_track_selected(&tr, 3u, 0u) == 1u && ri_track_selected(&tr, 4u, 0u) == 2u,
            "replace wrote");
        RI_ASSERT(ri_track_selected(&tr, 2u, 0u) == 3u && ri_track_selected(&tr, 5u, 0u) == 6u,
            "replace left neighbors");

        /* NULL / degenerate edges. */
        RI_ASSERT(ri_track_init_song(0, s4) == 2 && ri_track_init_song(&tr, 0) == 2, "init song null");
        RI_ASSERT(ri_track_init_loop(0, s4, 0u, 1u) == 2 && ri_track_init_loop(&tr, 0, 0u, 1u) == 2,
            "init loop null");
        ri_track_copy(&tr, 0u, 1u, 0);
        ri_track_copy(0, 0u, 1u, &clip);
        ri_track_cut(&tr, 0u, 1u, 0);
        ri_track_cut(0, 0u, 1u, &clip);
        RI_ASSERT(ri_track_paste(&tr, 0u, 0) == 2 && ri_track_paste(0, 0u, &clip) == 2, "paste null");
        RI_ASSERT(ri_track_paste_replace(&tr, 0u, 0) == 2 && ri_track_paste_replace(0, 0u, &clip) == 2,
            "replace null");
        clip.len = 0u;
        RI_ASSERT(ri_track_paste(&tr, 0u, &clip) == 0 && ri_track_paste_replace(&tr, 0u, &clip) == 0,
            "empty clip is a no-op");
    }
    /* ---- STRK codec: round-trip, three rejects, legacy shape, and the
     * track-without-banks minor fix. ---- */
    {
        const char *FA = "/tmp/ri/run/t59-strk.rbng";
        const char *FM = "/tmp/ri/run/t59-mut.rbng";
        const char *FL = "/tmp/ri/run/t59-legacy.rbng";
        struct RISong s, r;
        char err[256];
        unsigned char buf[16384];
        size_t nn;
        uint32_t k, soff = 0u;
        int found = 0, has_strk;
        FILE *f;

        /* song_valid requires nsteps != 0: every writer test sets it. */
        rbng_song_init(&s);
        s.nsteps = 4u;
        for (k = 0u; k < 4u; k++) {
            s.steps[k].note = (uint8_t)(45u + k);
            s.steps[k].flags = 0u;
        }
        RI_ASSERT(s.nbanks == 0u, "fresh banks");
        RI_ASSERT(ri_track_capture(&s.track, 10u, 0u, 9u) == 0, "codec cap a");
        RI_ASSERT(ri_track_capture(&s.track, 998u, 3u, 31u) == 0, "codec cap b");
        RI_ASSERT(rbng_write_song(FA, &s, err, sizeof err) == 0, "strk write %s", err);

        f = fopen(FA, "rb");
        RI_ASSERT(f != 0, "strk open");
        nn = f ? fread(buf, 1u, sizeof buf, f) : 0u;
        if (f)
            fclose(f);
        RI_ASSERT(nn > 32u, "strk file size %u", (unsigned)nn);
        /* A track without banks still forces the file minor to 1: writing it
         * as minor 0 would make the file unreadable by its own reader. */
        RI_ASSERT(buf[12] == 'V' && buf[13] == 'E' && buf[14] == 'R' && buf[15] == 'S',
            "VERS id");
        RI_ASSERT(buf[20] == 0u && buf[21] == 1u, "major");
        RI_ASSERT(buf[22] == 0u && buf[23] == 1u, "minor forced to 1");
        for (k = 0u; k + 8u <= (uint32_t)nn; k++) {
            if (memcmp(buf + k, "STRK", 4) == 0) {
                unsigned sz = (unsigned)(((unsigned)buf[k + 6] << 8) | (unsigned)buf[k + 7]);
                RI_ASSERT(buf[k + 4] == 0u && buf[k + 5] == 0u && sz == 3996u,
                    "STRK size %u", sz);
                soff = k;
                found = 1;
                break;
            }
        }
        RI_ASSERT(found, "no STRK chunk");
        /* Body is row-major (bar, then instance): offset 8 + bar*4 + inst. */
        RI_ASSERT(buf[soff + 8u + (10u * 4u) + 0u] == 9u, "strk body a");
        RI_ASSERT(buf[soff + 8u + (998u * 4u) + 3u] == 31u, "strk body b");

        /* Round-trip. */
        memset(&r, 0xA5, sizeof r);
        RI_ASSERT(rbng_read_song(FA, &r, err, sizeof err) == 0, "strk read %s", err);
        RI_ASSERT(memcmp(&s.track, &r.track, sizeof s.track) == 0, "track roundtrip");
        RI_ASSERT(r.nsteps == 4u && r.nbanks == 0u, "song fields kept");

        /* Reject (a): body size - 1 (exact-length law). */
        {
            unsigned char one[4];
            one[0] = 0u; one[1] = 0u; one[2] = 0x0Fu; one[3] = 0x9Bu;
            RI_ASSERT(rbng_test_patch_bytes(FA, FM, soff + 4u, one, 4u) == 0, "patch size");
            memset(&r, 0xA5, sizeof r);
            RI_ASSERT(rbng_read_song(FM, &r, err, sizeof err) != 0, "badlen accepted");
            RI_ASSERT(strstr(err, "STRK length mismatch") != 0, "badlen reason: %s", err);
            RI_ASSERT(ri_track_is_empty(&r.track) == 1, "badlen stored a track");
        }
        /* Reject (b): the LAST body byte (bar 998, instance 3) out of range. */
        {
            unsigned char v = 32u;
            RI_ASSERT(rbng_test_patch_bytes(FA, FM, soff + 8u + RI_RBNG_STRK_BYTES - 1u, &v, 1u) == 0,
                "patch slot");
            memset(&r, 0xA5, sizeof r);
            RI_ASSERT(rbng_read_song(FM, &r, err, sizeof err) != 0, "slot32 accepted");
            RI_ASSERT(strstr(err, "STRK slot out of range") != 0, "slot32 reason: %s", err);
            RI_ASSERT(ri_track_is_empty(&r.track) == 1, "slot32 partially stored a track");
        }
        /* Reject (c): STRK at file minor 0 (BANK precedent). */
        RI_ASSERT(rbng_test_set_vers(FA, FM, 1u, 0u, 0u) == 0, "setvers");
        memset(&r, 0xA5, sizeof r);
        RI_ASSERT(rbng_read_song(FM, &r, err, sizeof err) != 0, "minor0+STRK accepted");
        RI_ASSERT(strstr(err, "STRK requires 1.1") != 0, "minor0 reason: %s", err);

        /* Legacy shape: an empty track is omitted and the file stays minor 0. */
        rbng_song_init(&s);
        s.nsteps = 4u;
        for (k = 0u; k < 4u; k++) {
            s.steps[k].note = (uint8_t)(45u + k);
            s.steps[k].flags = 0u;
        }
        RI_ASSERT(ri_track_is_empty(&s.track) == 1, "fresh track empty");
        RI_ASSERT(rbng_write_song(FL, &s, err, sizeof err) == 0, "legacy write %s", err);
        f = fopen(FL, "rb");
        RI_ASSERT(f != 0, "legacy open");
        nn = f ? fread(buf, 1u, sizeof buf, f) : 0u;
        if (f)
            fclose(f);
        RI_ASSERT(buf[22] == 0u && buf[23] == 0u, "legacy minor 0");
        has_strk = 0;
        for (k = 0u; k + 4u <= (uint32_t)nn; k++)
            if (memcmp(buf + k, "STRK", 4) == 0)
                has_strk = 1;
        RI_ASSERT(!has_strk, "legacy file carries STRK");
        memset(&r, 0xA5, sizeof r);
        RI_ASSERT(rbng_read_song(FL, &r, err, sizeof err) == 0, "legacy read %s", err);
        RI_ASSERT(ri_track_is_empty(&r.track) == 1, "legacy track not empty");
        for (k = 0u; k < 4u; k++)
            RI_ASSERT(ri_track_selected(&r.track, 500u, k) == 0u, "v1.0 default slot 0");
    }
    /* ---- ATRK automation chunk (v1.2): 3000-event round-trip, minor
     * iff present, legacy AUTO-only byte-identity. ---- */
    {
        const char *FB = "/tmp/ri/run/t59-atrk.rbng";
        const char *FM2 = "/tmp/ri/run/t59-atrk-mut.rbng";
        const char *FL2 = "/tmp/ri/run/t59-atrk-legacy.rbng";
        const char *F1 = "/tmp/ri/run/t59-atrk-auto1.rbng";
        const char *F2 = "/tmp/ri/run/t59-atrk-auto2.rbng";
        struct RISong s, r;
        static struct RBAutoEv big[3000], back[3000];
        char err[256];
        unsigned char buf[64], f1[65536], f2[65536];
        size_t nn, n1, n2;
        uint32_t k;
        FILE *f;
        rbng_song_init(&s);
        s.nsteps = 4u;
        for (k = 0u; k < 4u; k++) {
            s.steps[k].note = (uint8_t)(45u + k);
            s.steps[k].flags = 0u;
        }
        for (k = 0u; k < 3000u; k++) {
            big[k].tick = k * 127u;
            big[k].ctl = (uint16_t)(((k & 1u) != 0u) ? 0x0310u + (k % 8u) : 0x0300u + (k % 8u));
            big[k].val = (uint8_t)(k % 128u);
        }
        s.atrk = big;
        s.natrk = 3000u;
        s.atrk_cap = 3000u;
        RI_ASSERT(rbng_write_song(FB, &s, err, sizeof err) == 0, "atrk write %s", err);
        f = fopen(FB, "rb");
        RI_ASSERT(f != 0, "atrk open");
        nn = f ? fread(buf, 1u, sizeof buf, f) : 0u;
        if (f)
            fclose(f);
        RI_ASSERT(nn >= 24u, "atrk file size %u", (unsigned)nn);
        RI_ASSERT(buf[20] == 0u && buf[21] == 1u, "atrk major");
        RI_ASSERT(buf[22] == 0u && buf[23] == 2u, "minor is 2 with ATRK");
        memset(&r, 0xA5, sizeof r);
        r.atrk = back;
        r.atrk_cap = 3000u;
        r.natrk = 0u;
        RI_ASSERT(rbng_read_song(FB, &r, err, sizeof err) == 0, "atrk read %s", err);
        RI_ASSERT(r.natrk == 3000u, "atrk count %u", r.natrk);
        RI_ASSERT(memcmp(back, big, 3000u * sizeof back[0]) == 0, "atrk round-trip bytes");
        RI_ASSERT(r.nsteps == 4u, "atrk song fields kept");
        /* Minor iff present: no ATRK leaves minor at/below 1. */
        rbng_song_init(&s);
        s.nsteps = 4u;
        for (k = 0u; k < 4u; k++) {
            s.steps[k].note = (uint8_t)(45u + k);
            s.steps[k].flags = 0u;
        }
        RI_ASSERT(rbng_write_song(FL2, &s, err, sizeof err) == 0, "nominor write %s", err);
        f = fopen(FL2, "rb");
        RI_ASSERT(f != 0, "nominor open");
        nn = f ? fread(buf, 1u, sizeof buf, f) : 0u;
        if (f)
            fclose(f);
        RI_ASSERT(nn >= 24u, "nominor size %u", (unsigned)nn);
        RI_ASSERT(buf[23] != 2u, "minor raised without ATRK: %u", buf[23]);
        /* Legacy byte-identity: AUTO-only file rewrites byte-identical. */
        rbng_song_init(&s);
        s.nsteps = 4u;
        for (k = 0u; k < 4u; k++) {
            s.steps[k].note = (uint8_t)(45u + k);
            s.steps[k].flags = 0u;
        }
        s.nauto = 3u;
        s.auto_ev[0].tick = 0u; s.auto_ev[0].ctl = 0x0300u; s.auto_ev[0].val = 1u;
        s.auto_ev[1].tick = 384u; s.auto_ev[1].ctl = 0x0312u; s.auto_ev[1].val = 2u;
        s.auto_ev[2].tick = 768u; s.auto_ev[2].ctl = 0x0301u; s.auto_ev[2].val = 3u;
        RI_ASSERT(rbng_write_song(F1, &s, err, sizeof err) == 0, "auto write %s", err);
        memset(&r, 0xA5, sizeof r);
        RI_ASSERT(rbng_read_song(F1, &r, err, sizeof err) == 0, "auto read %s", err);
        RI_ASSERT(r.nauto == 3u, "auto count kept %u", r.nauto);
        RI_ASSERT(rbng_write_song(F2, &r, err, sizeof err) == 0, "auto rewrite %s", err);
        f = fopen(F1, "rb");
        n1 = f ? fread(f1, 1u, sizeof f1, f) : 0u;
        if (f)
            fclose(f);
        f = fopen(F2, "rb");
        n2 = f ? fread(f2, 1u, sizeof f2, f) : 0u;
        if (f)
            fclose(f);
        RI_ASSERT(n1 == n2 && n1 > 0u, "rewrite sizes %u/%u", (unsigned)n1, (unsigned)n2);
        RI_ASSERT(n1 == n2 && memcmp(f1, f2, n1) == 0, "legacy byte-identity");
        /* Rejects, each with its own err text and nothing stored. */
        {
            unsigned char mut[8];
            uint32_t soff = 0u, q;
            struct RISong r2;
            f = fopen(FB, "rb");
            n1 = f ? fread(f1, 1u, sizeof f1, f) : 0u;
            if (f)
                fclose(f);
            for (q = 0u; q + 8u <= (uint32_t)n1; q++)
                if (memcmp(f1 + q, "ATRK", 4) == 0) {
                    soff = q;
                    break;
                }
            RI_ASSERT(soff > 0u, "ATRK chunk found");
            /* Minor 0 + ATRK. */
            RI_ASSERT(rbng_test_set_vers(FB, FM2, 1u, 0u, 0u) == 0, "mut minor");
            rbng_song_init(&r2);
            r2.atrk = back;
            r2.atrk_cap = 3000u;
            RI_ASSERT(rbng_read_song(FM2, &r2, err, sizeof err) != 0,
                "minor0+ATRK accepted");
            RI_ASSERT(strstr(err, "ATRK requires 1.2") != 0, "minor0 reason: %s", err);
            RI_ASSERT(r2.natrk == 0u, "minor0 stored %u", r2.natrk);
            /* Bad value in the LAST record. */
            mut[0] = 200u;
            RI_ASSERT(rbng_test_patch_bytes(FB, FM2,
                soff + 12u + 2999u * 8u + 6u, mut, 1u) == 0, "mut value");
            rbng_song_init(&r2);
            r2.atrk = back;
            r2.atrk_cap = 3000u;
            RI_ASSERT(rbng_read_song(FM2, &r2, err, sizeof err) != 0,
                "badval accepted");
            RI_ASSERT(strstr(err, "ATRK bad value") != 0, "badval reason: %s", err);
            RI_ASSERT(r2.natrk == 0u, "badval stored %u", r2.natrk);
            /* Unsorted: swap the last two tick words. */
            {
                unsigned char t_hi[4], t_lo[4];
                uint32_t t1 = 2998u * 127u, t2 = 2999u * 127u, k;
                for (k = 0u; k < 4u; k++) {
                    t_hi[k] = (unsigned char)(t1 >> (8u * (3u - k)));
                    t_lo[k] = (unsigned char)(t2 >> (8u * (3u - k)));
                }
                RI_ASSERT(rbng_test_patch_bytes(FB, FM2, soff + 12u + 2998u * 8u,
                    t_lo, 4u) == 0, "mut unsort a");
                {
                    /* second patch chains file->file; reuse FM2 via F1 slot */
                    unsigned char tmp[65536];
                    size_t nnx;
                    FILE *fx = fopen(FM2, "rb");
                    nnx = fx ? fread(tmp, 1u, sizeof tmp, fx) : 0u;
                    if (fx)
                        fclose(fx);
                    RI_ASSERT(nnx > soff + 12u + 2999u * 8u + 4u, "mut readback");
                    for (k = 0u; k < 4u; k++)
                        tmp[soff + 12u + 2999u * 8u + k] = t_hi[k];
                    fx = fopen(FM2, "wb");
                    RI_ASSERT(fx != 0, "mut rewrite");
                    if (fx) {
                        fwrite(tmp, 1u, nnx, fx);
                        fclose(fx);
                    }
                }
                rbng_song_init(&r2);
                r2.atrk = back;
                r2.atrk_cap = 3000u;
                RI_ASSERT(rbng_read_song(FM2, &r2, err, sizeof err) != 0,
                    "unsorted accepted");
                RI_ASSERT(strstr(err, "ATRK unsorted") != 0, "unsorted reason: %s", err);
                RI_ASSERT(r2.natrk == 0u, "unsorted stored %u", r2.natrk);
            }
            /* Duplicate: last record copies the previous tick+ctl. */
            {
                unsigned char dup[6];
                uint32_t t1 = 2998u * 127u;
                uint16_t c1 = (uint16_t)(0x0300u + (2998u % 8u));
                dup[0] = (unsigned char)(t1 >> 24u);
                dup[1] = (unsigned char)(t1 >> 16u);
                dup[2] = (unsigned char)(t1 >> 8u);
                dup[3] = (unsigned char)t1;
                dup[4] = (unsigned char)(c1 >> 8u);
                dup[5] = (unsigned char)c1;
                RI_ASSERT(rbng_test_patch_bytes(FB, FM2, soff + 12u + 2999u * 8u,
                    dup, 6u) == 0, "mut dup");
                rbng_song_init(&r2);
                r2.atrk = back;
                r2.atrk_cap = 3000u;
                RI_ASSERT(rbng_read_song(FM2, &r2, err, sizeof err) != 0,
                    "duplicate accepted");
                RI_ASSERT(strstr(err, "ATRK duplicate") != 0, "dup reason: %s", err);
                RI_ASSERT(r2.natrk == 0u, "dup stored %u", r2.natrk);
            }
            /* Tick past bar 999. */
            {
                unsigned char te[4];
                uint32_t t = 999u * 384u, k;
                for (k = 0u; k < 4u; k++)
                    te[k] = (unsigned char)(t >> (8u * (3u - k)));
                RI_ASSERT(rbng_test_patch_bytes(FB, FM2, soff + 12u + 2999u * 8u,
                    te, 4u) == 0, "mut tick");
                rbng_song_init(&r2);
                r2.atrk = back;
                r2.atrk_cap = 3000u;
                RI_ASSERT(rbng_read_song(FM2, &r2, err, sizeof err) != 0,
                    "bigtick accepted");
                RI_ASSERT(strstr(err, "ATRK tick past bar 999") != 0,
                    "bigtick reason: %s", err);
                RI_ASSERT(r2.natrk == 0u, "bigtick stored %u", r2.natrk);
            }
            /* ID not allowed. */
            {
                unsigned char id[2] = { 0x04u, 0x01u };
                RI_ASSERT(rbng_test_patch_bytes(FB, FM2, soff + 12u + 2999u * 8u + 4u,
                    id, 2u) == 0, "mut id");
                rbng_song_init(&r2);
                r2.atrk = back;
                r2.atrk_cap = 3000u;
                RI_ASSERT(rbng_read_song(FM2, &r2, err, sizeof err) != 0,
                    "badid accepted");
                RI_ASSERT(strstr(err, "ATRK ID not allowed") != 0, "badid reason: %s", err);
                RI_ASSERT(r2.natrk == 0u, "badid stored %u", r2.natrk);
            }
            /* NULL buffer and over-capacity. */
            rbng_song_init(&r2);
            RI_ASSERT(rbng_read_song(FB, &r2, err, sizeof err) != 0,
                "nullbuf accepted");
            RI_ASSERT(strstr(err, "ATRK without buffer") != 0, "nullbuf reason: %s", err);
            RI_ASSERT(r2.natrk == 0u, "nullbuf stored %u", r2.natrk);
            rbng_song_init(&r2);
            r2.atrk = back;
            r2.atrk_cap = 2999u;
            RI_ASSERT(rbng_read_song(FB, &r2, err, sizeof err) != 0,
                "overcap accepted");
            RI_ASSERT(strstr(err, "ATRK buffer too small") != 0, "overcap reason: %s", err);
            RI_ASSERT(r2.natrk == 0u, "overcap stored %u", r2.natrk);
            /* AUTO+ATRK together: splice a 10-byte empty AUTO chunk after
             * ATRK and fix the FORM size. */
            {
                unsigned char fs[65536];
                unsigned char au[10] = { 'A', 'U', 'T', 'O', 0, 0, 0, 2, 0, 0 };
                uint32_t total, aend, k;
                FILE *fx = fopen(FB, "rb");
                size_t nnx = fx ? fread(fs, 1u, sizeof fs, fx) : 0u;
                if (fx)
                    fclose(fx);
                RI_ASSERT(nnx > soff + 8u + 24004u, "splice readback %u", (unsigned)nnx);
                total = ((uint32_t)fs[4] << 24) | ((uint32_t)fs[5] << 16) |
                    ((uint32_t)fs[6] << 8) | (uint32_t)fs[7];
                aend = soff + 8u + 24004u;
                memmove(fs + aend + 10u, fs + aend, nnx - aend);
                for (k = 0u; k < 10u; k++)
                    fs[aend + k] = au[k];
                nnx += 10u;
                total += 10u;
                fs[4] = (unsigned char)(total >> 24u);
                fs[5] = (unsigned char)(total >> 16u);
                fs[6] = (unsigned char)(total >> 8u);
                fs[7] = (unsigned char)total;
                fx = fopen(FM2, "wb");
                RI_ASSERT(fx != 0, "splice write");
                if (fx) {
                    fwrite(fs, 1u, nnx, fx);
                    fclose(fx);
                }
                rbng_song_init(&r2);
                r2.atrk = back;
                r2.atrk_cap = 3000u;
                RI_ASSERT(rbng_read_song(FM2, &r2, err, sizeof err) != 0,
                    "auto+atrk accepted");
                RI_ASSERT(strstr(err, "AUTO and ATRK together") != 0,
                    "together reason: %s", err);
                /* No cross-chunk rollback in this reader (file
                 * convention): the valid ATRK chunk stays stored. */
            }
        }
    }
    /* ---- record-path capture gating (§12.9c; spec §5 open item) ----
     * Caller-side: the transport RECORD state + cursor quantize decide;
     * the model stays gate-blind. */
    {
        struct RISongTrack tr;
        struct RITransport tp;
        uint64_t bar = 4u * 96u; /* ticks per bar at ppq 96 */
        ri_track_init(&tr);
        memset(&tp, 0, sizeof tp);
        /* Off-record (STOPPED, PLAYING): refused at any cursor, untouched. */
        RI_ASSERT(ri_record_capture(&tr, tp.state, 0u, 96u, 0u, 5u) == 2, "stopped refuses");
        tp.state = RI_TR_PLAYING;
        RI_ASSERT(ri_record_capture(&tr, tp.state, bar + 200u, 96u, 0u, 5u) == 2, "playing refuses");
        RI_ASSERT(ri_track_is_empty(&tr) == 1, "off-record untouched");
        /* RECORD at a downbeat writes that bar. */
        tp.state = RI_TR_RECORD;
        RI_ASSERT(ri_record_capture(&tr, tp.state, bar, 96u, 0u, 5u) == 0, "rec downbeat rc");
        RI_ASSERT(ri_track_selected(&tr, 1u, 0u) == 5u, "rec downbeat stored");
        /* RECORD mid-measure quantizes forward (composition with t58 law). */
        RI_ASSERT(ri_record_capture(&tr, tp.state, bar + 200u, 96u, 1u, 7u) == 0, "rec mid rc");
        RI_ASSERT(ri_track_selected(&tr, 2u, 1u) == 7u, "rec mid stored next");
        /* RECORD near the end clamps to 998, never 999. */
        RI_ASSERT(ri_record_capture(&tr, tp.state, 998u * bar + 200u, 96u, 2u, 9u) == 0, "rec end rc");
        RI_ASSERT(ri_track_selected(&tr, 998u, 2u) == 9u, "rec end stored");
        /* Refusals delegate: NULL track, slot > 31 — prior content kept. */
        RI_ASSERT(ri_record_capture(0, tp.state, bar, 96u, 0u, 5u) == 2, "rec null refuses");
        RI_ASSERT(ri_record_capture(&tr, tp.state, bar, 96u, 0u, 32u) == 2, "rec slot32 refuses");
        RI_ASSERT(ri_track_selected(&tr, 1u, 0u) == 5u, "refusal keeps prior");
    }
    RI_RESULT("songtrack");
}
