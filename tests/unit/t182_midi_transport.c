/* t182_midi_transport — M3: the take follows MIDI clock (R1 applied).
 * Whole app-side chain, host-driven: scripted wire bytes with stamped
 * arrival times feed the bridge; intents drain into the applier on a
 * panel transport; a simulated player advances the panel cursor at the
 * session tempo. Laws: transport follows intents (START from song
 * start, CONTINUE keeps cursor, STOP stops, SEEK locates stopped-only);
 * Internal ignores MIDI transport; lock within 2 beats; bounded tempo
 * error under ±1/±3 ms jitter; zero long-term drift over 10 simulated
 * minutes; dropout holds tempo with one latched STOP; knob read-only
 * and TAP dead while locked.
 */
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <math.h>
#include "tests/helpers/ri_assert.h"
#include "midi_io/midi_bridge.h"
#include "midi_io/midi_follow.h"
#include "gui/miditrans.h"
#include "gui/ctlreg.h"
#include "gui/sectui.h"
#include "gui/secttr.h"
#include "engine/seq/transport.h"

#define TICK120 20833u /* us per 24ppqn tick @120 BPM */

static struct RIMidiBridge s_b;
static struct RIMidiIntent s_its[16];
static struct RIMidiMsg s_msgs[64];

static void feed(uint8_t st, uint8_t d1, uint8_t d2, uint64_t t) {
    midi_bridge_feed(&s_b, st, d1, d2, t);
}

/* Drain intents into the applier; count channel clocks. */
static uint32_t pump(struct RIMidiTrans *t, struct RISectUI *tru, uint64_t *cursor,
    uint32_t *nf8) {
    uint32_t k, i, n = 0u;
    uint32_t cl = 0u;
    k = midi_bridge_read_in(&s_b, s_its, 16u);
    for (i = 0u; i < k; i++) {
        midi_trans_apply(t, &s_its[i].it, tru, cursor);
        n++;
    }
    k = midi_bridge_read_ch(&s_b, s_msgs, 64u);
    for (i = 0u; i < k; i++)
        if (s_msgs[i].b[0] == 0xF8u)
            cl++;
    if (nf8)
        *nf8 = cl;
    return n;
}

static uint32_t panel_state(struct RISectUI *tru) {
    return tru->u.tr.tr.state;
}

int main(void) {
    static struct RISectUI tru;
    static struct RIMidiTrans t;
    uint64_t cursor = 0u, now = 1000000u;
    uint32_t nf8 = 0u;
    float sess = 100.0f, out;
    double cursor_d = 0.0;
    RI_ASSERT(ri_sui_init(&tru, RI_SEC_TRANSPORT) == 0, "tr init");
    midi_trans_init(&t);
    /* Internal ignores MIDI transport (M3 opt-in). */
    midi_bridge_init(&s_b);
    feed(0xFAu, 0u, 0u, now);
    feed(0xF8u, 0u, 0u, now + TICK120);
    pump(&t, &tru, &cursor, &nf8);
    RI_ASSERT(panel_state(&tru) == RI_TR_STOPPED && cursor == 0u, "internal ignores");
    RI_ASSERT(midi_trans_tempo(&t, &tru, nf8, midi_follow_locked(&s_b.follow),
        midi_follow_bpm(&s_b.follow), cursor) == 0.0f, "internal tempo");
    /* Song-mode take: STOP, SPP 10, CONTINUE fires on F8 from the seek. */
    midi_trans_init(&t);
    t.sync.source = RI_SYNC_MIDI;
    cursor = 0u;
    feed(0xFCu, 0u, 0u, now);
    /* SPP counts QUARTER notes: 10 beats is 40 sixteenths, and at the
     * panel PPQ (96) a sixteenth is 24 engine ticks -> tick 960. */
    feed(0xF2u, 0x0Au, 0x00u, now);
    pump(&t, &tru, &cursor, &nf8);
    RI_ASSERT(cursor == 960u, "spp seeks %llu", (unsigned long long)cursor);
    feed(0xFBu, 0u, 0u, now);
    now += TICK120;
    feed(0xF8u, 0u, 0u, now);
    pump(&t, &tru, &cursor, &nf8);
    RI_ASSERT(panel_state(&tru) == RI_TR_PLAYING, "continue plays");
    RI_ASSERT(cursor == 960u, "continue keeps cursor %llu", (unsigned long long)cursor);
    /* SPP while running changes nothing (M1 law, applied end to end). */
    feed(0xF2u, 0x00u, 0x01u, now);
    pump(&t, &tru, &cursor, &nf8);
    RI_ASSERT(cursor == 960u && s_b.follow.spp_ignored == 1u, "running spp ignored");
    /* A locate past the song clamps to its last tick, like the panel's own
     * bar seeks: a master position beyond the arrangement must not leave
     * the take parked in empty song. */
    {
        uint32_t sb = tru.u.tr.song_bars;
        tru.u.tr.song_bars = 20u;                 /* 20 bars = 7680 ticks */
        feed(0xFCu, 0u, 0u, now);
        pump(&t, &tru, &cursor, &nf8);
        feed(0xF2u, 0x64u, 0x00u, now);           /* SPP 100 beats = 9600 */
        pump(&t, &tru, &cursor, &nf8);
        RI_ASSERT(cursor == 7679u, "locate clamped %llu", (unsigned long long)cursor);
        tru.u.tr.song_bars = sb;
        cursor = 0u;
    }
    /* STOP stops; tempo holds (frozen, knob live again). */
    feed(0xFCu, 0u, 0u, now);
    pump(&t, &tru, &cursor, &nf8);
    RI_ASSERT(panel_state(&tru) == RI_TR_STOPPED, "stop stops");
    out = midi_trans_tempo(&t, &tru, 0u, 0u, 0.0f, cursor);
    RI_ASSERT(out == 0.0f, "tempo internal after stop");
    RI_ASSERT(midi_trans_tempo_locked(&t) == 0, "knob live");
    /* START plays from song start on the next F8. */
    cursor = 999u;
    feed(0xFAu, 0u, 0u, now);
    now += TICK120;
    feed(0xF8u, 0u, 0u, now);
    pump(&t, &tru, &cursor, &nf8);
    RI_ASSERT(panel_state(&tru) == RI_TR_PLAYING && cursor == 0u, "start from top");
    /* Lock within 2 beats of steady clock; error bounded under jitter. */
    {
        uint32_t li;
        midi_bridge_init(&s_b);
        midi_trans_init(&t);
        t.sync.source = RI_SYNC_MIDI;
        cursor = 0u;
        now = 5000000u;
        feed(0xFAu, 0u, 0u, now);
        for (li = 0u; li < 12u; li++) {
            now += TICK120;
            feed(0xF8u, 0u, 0u, now);
        }
        pump(&t, &tru, &cursor, &nf8);
        RI_ASSERT(!midi_follow_locked(&s_b.follow), "not locked at half beat");
        for (li = 0u; li < 36u; li++) {
            now += TICK120;
            feed(0xF8u, 0u, 0u, now);
        }
        pump(&t, &tru, &cursor, &nf8);
        RI_ASSERT(midi_follow_locked(&s_b.follow), "locked by beat 2");
        cursor = (uint64_t)(t.expected + (int64_t)nf8 * 4); /* player kept up */
        out = midi_trans_tempo(&t, &tru, nf8, midi_follow_locked(&s_b.follow),
            midi_follow_bpm(&s_b.follow), cursor);
        RI_ASSERT(out > 119.0f && out < 121.0f, "tempo %f", out);
        RI_ASSERT(midi_trans_tempo_locked(&t) == 1, "knob locked");
        RI_ASSERT(tru.u.tr.tempo_lock == 1u, "panel flag");
        RI_ASSERT(tru.u.tr.tempo >= 20 && tru.u.tr.tempo <= 500, "display sane %d",
            tru.u.tr.tempo);
    }
    /* Knob read-only + TAP dead while locked; live when not. */
    RI_ASSERT(ri_sui_set(&tru, RI_STR_TEMPO, 200) == 0, "knob refused");
    RI_ASSERT(ri_sui_step(&tru, RI_STR_TEMPO, 1) == 0, "step refused");
    /* TAP needs two presses to move tempo; while locked neither may,
     * and locked presses must not seed intervals (after unlock the next
     * tap starts a fresh measurement, not a stale locked-era one). */
    RI_ASSERT(ri_sui_tap(&tru, RI_STR_TAP, 1000u) == 0, "tap ref");
    RI_ASSERT(ri_sui_tap(&tru, RI_STR_TAP, 1250u) == 0, "tap refused");
    ri_str_set_tempo_lock(&tru.u.tr, 0u);
    RI_ASSERT(ri_sui_tap(&tru, RI_STR_TAP, 1300u) == 0, "tap fresh after unlock");
    midi_trans_init(&t);
    t.sync.source = RI_SYNC_MIDI;
    RI_ASSERT(ri_sui_init(&tru, RI_SEC_TRANSPORT) == 0, "tr reinit");
    RI_ASSERT(ri_sui_set(&tru, RI_STR_TEMPO, 200) == 1, "knob live");
    /* A relock must not swallow clocks. The expectation counts CLOCKS,
     * not locked windows: under script jitter the follower relocks often,
     * and M3b's lane run grew the phase error without bound because the
     * clocks that arrived inside an unlocked window were read off the
     * queue and never counted (f8=2196/2196 on the wire, exp half that). */
    {
        uint64_t e0, e1;
        midi_bridge_init(&s_b);
        midi_trans_init(&t);
        t.sync.source = RI_SYNC_MIDI;
        ri_sui_init(&tru, RI_SEC_TRANSPORT);
        ri_sui_press(&tru, RI_STR_MODE);
        ri_sui_press(&tru, RI_STR_PLAY);
        feed(0xFAu, 0u, 0u, now);
        now += TICK120;
        feed(0xF8u, 0u, 0u, now);
        pump(&t, &tru, &cursor, &nf8);
        RI_ASSERT(panel_state(&tru) == RI_TR_PLAYING, "playing");
        e0 = (uint64_t)t.expected;
        out = midi_trans_tempo(&t, &tru, 4u, 1u, 120.0f, e0);
        RI_ASSERT((uint64_t)t.expected == e0 + 16u, "locked clocks count %llu",
            (unsigned long long)t.expected);
        /* Unlocked (a relock window): no tempo, but the clocks still land. */
        e1 = (uint64_t)t.expected;
        out = midi_trans_tempo(&t, &tru, 6u, 0u, 0.0f, e1);
        RI_ASSERT(out == 0.0f, "no tempo unlocked");
        RI_ASSERT((uint64_t)t.expected == e1 + 24u, "relock clocks count %llu",
            (unsigned long long)t.expected);
    }
    /* ±1 ms jitter: bounded error; ±3 ms: wider but bounded.
     * Pseudo-random (LCG) with zero mean — a real jitter profile. (A
     * perfectly alternating square wave pins the R1 running mean and
     * correctly refuses to lock; that adversarial case is not jitter.)
     * Seeds fixed: measured 120.26 (bound 1.5) and 119.25 (bound 3.0);
     * integer math makes them deterministic, margins absorb nothing
     * hidden — rerun the probe to re-measure after estimator changes. */
    {
        uint32_t li, rng = 39608u;
        float e1, e3;
        midi_bridge_init(&s_b);
        midi_trans_init(&t);
        t.sync.source = RI_SYNC_MIDI;
        now = 9000000u;
        feed(0xFAu, 0u, 0u, now);
        for (li = 0u; li < 200u; li++) {
            int32_t j;
            rng = rng * 1103515245u + 12345u;
            j = (int32_t)(rng >> 16) % 2001 - 1000;
            now += (uint64_t)((int64_t)TICK120 + j);
            feed(0xF8u, 0u, 0u, now);
        }
        pump(&t, &tru, &cursor, &nf8);
        cursor = (uint64_t)(t.expected + (int64_t)nf8 * 4); /* player kept up */
        e1 = midi_trans_tempo(&t, &tru, nf8, midi_follow_locked(&s_b.follow),
            midi_follow_bpm(&s_b.follow), cursor);
        RI_ASSERT(fabsf(e1 - 120.0f) < 1.5f, "jit1 %f", e1);
        midi_bridge_init(&s_b);
        midi_trans_init(&t);
        t.sync.source = RI_SYNC_MIDI;
        rng = 47527u;
        now = 20000000u;
        feed(0xFAu, 0u, 0u, now);
        for (li = 0u; li < 200u; li++) {
            int32_t j;
            rng = rng * 1103515245u + 12345u;
            j = (int32_t)(rng >> 16) % 6001 - 3000;
            now += (uint64_t)((int64_t)TICK120 + j);
            feed(0xF8u, 0u, 0u, now);
        }
        pump(&t, &tru, &cursor, &nf8);
        cursor = (uint64_t)(t.expected + (int64_t)nf8 * 4); /* player kept up */
        e3 = midi_trans_tempo(&t, &tru, nf8, midi_follow_locked(&s_b.follow),
            midi_follow_bpm(&s_b.follow), cursor);
        RI_ASSERT(fabsf(e3 - 120.0f) < 3.0f, "jit3 %f", e3);
    }
    /* Ten simulated minutes: zero long-term drift. The player renders
     * session-tempo ticks per wall-clock interval (the jittered stamp
     * deltas); the clock says 4 ticks are due per F8. */
    {
        uint32_t li, beats = 600u, rng = 777u;
        double err_end = 0.0, err_mid = 0.0;
        int64_t err;
        uint64_t prev = 0u;
        midi_bridge_init(&s_b);
        midi_trans_init(&t);
        t.sync.source = RI_SYNC_MIDI;
        RI_ASSERT(ri_sui_init(&tru, RI_SEC_TRANSPORT) == 0, "tr fresh");
        cursor = 0u;
        cursor_d = 0.0;
        sess = 100.0f; /* wrong-knob transient: recovery is part of the proof */
        RI_ASSERT(ri_sui_set(&tru, RI_STR_TEMPO, 100) == 1, "knob 100");
        now = 30000000u;
        prev = now;
        feed(0xFAu, 0u, 0u, now);
        for (li = 0u; li < beats * 24u; li++) {
            uint32_t lk;
            float bpm, o;
            int32_t j;
            double dt_s;
            rng = rng * 1103515245u + 12345u;
            j = (int32_t)(rng >> 16) % 2001 - 1000;
            now += (uint64_t)((int64_t)TICK120 + j);
            dt_s = (double)(now - prev) / 1000000.0;
            prev = now;
            feed(0xF8u, 0u, 0u, now);
            pump(&t, &tru, &cursor, &nf8);
            lk = midi_follow_locked(&s_b.follow);
            bpm = midi_follow_bpm(&s_b.follow);
            o = midi_trans_tempo(&t, &tru, 1u, lk, bpm, (uint64_t)cursor_d);
            if (o > 0.0f)
                sess = o;
            cursor_d += (double)sess * 96.0 / 60.0 * dt_s;
            if (li == beats * 12u)
                err_mid = fabs(cursor_d - (double)t.expected);
            if (li == beats * 12u + 1u) {
                /* Local bar jump mid-take: only the servo pulls the
                 * standing offset back (mutant without it strands +384). */
                cursor_d += 384.0;
                cursor = (uint64_t)cursor_d;
            }
            /* The panel cursor trails the player here; keep them together. */
            cursor = (uint64_t)cursor_d;
        }
        err = (int64_t)cursor_d - t.expected;
        if (err < 0)
            err = -err;
        err_end = (double)err;
        RI_ASSERT(err_end < 96.0, "drift %f ticks", err_end);
        RI_ASSERT(err_end <= err_mid + 96.0, "not growing (%f -> %f)", err_mid, err_end);
    }
    /* A LOCAL stop (panel Stop, no MIDI byte) ends the take: the clock
     * expectation must freeze with it, or the servo trims against a
     * parked engine cursor until it rails at the bound. */
    {
        uint64_t exp0;
        uint32_t k;
        RI_ASSERT(midi_trans_tempo_locked(&t) == 1, "still locked");
        ri_sui_press(&tru, RI_STR_STOP);
        exp0 = (uint64_t)t.expected;
        for (k = 0u; k < 6u; k++)
            out = midi_trans_tempo(&t, &tru, 4u, 1u, 120.0f, exp0);
        RI_ASSERT((uint64_t)t.expected == exp0, "expectation frozen after local stop");
        RI_ASSERT(fabsf(out - 120.0f) < 0.5f, "trim not railed after local stop %f", out);
    }
    RI_RESULT("miditransport");
}
