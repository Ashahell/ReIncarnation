/* t125_midi_follow — clock-in follower core (R1 interop, owner order).
 * Steady clock locks with exact BPM; jitter holds; wild ticks rejected
 * without moving the estimate; dropout yields one latched STOP; transport
 * intents exact; fail-closed; twin determinism.
 */
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "tests/helpers/ri_assert.h"
#include "midi_io/midi_follow.h"

#define TICK120 20833u /* us per 24ppqn tick @120 BPM (500000/24) */

static void feed(struct RIFollow *f, uint64_t *t, uint32_t n, uint32_t step) {
    uint32_t i;
    struct RIFollowIntent it;
    for (i = 0u; i < n; i++) {
        *t += step;
        RI_ASSERT(midi_follow_tick(f, *t, &it) == 0, "tick %u", i);
        RI_ASSERT(it.kind == RI_FOLLOW_NONE, "tick silent");
    }
}

int main(void) {
    struct RIFollow f, g;
    struct RIFollowIntent it;
    uint64_t t = 1000000u;
    float bpm;
    midi_follow_init(&f);
    /* Fail-closed + cold state. */
    RI_ASSERT(midi_follow_tick(0, t, &it) == 2, "tick null");
    RI_ASSERT(midi_follow_tick(&f, t, 0) == 2, "intent null");
    RI_ASSERT(midi_follow_poll(0, t, &it) == 2, "poll null");
    RI_ASSERT(midi_follow_bpm(0) <= 0.0f, "bpm null");
    RI_ASSERT(midi_follow_locked(0) == 0, "locked null");
    RI_ASSERT(midi_follow_bpm(&f) == 0.0f, "cold bpm");
    RI_ASSERT(midi_follow_locked(&f) == 0, "cold unlocked");
    RI_ASSERT(midi_follow_poll(&f, t + 5000000u, &it) == 0 && it.kind == RI_FOLLOW_NONE,
        "cold poll silent");
    /* Steady 120 BPM locks after a quarter; BPM exact-ish. */
    feed(&f, &t, 25u, TICK120);
    RI_ASSERT(midi_follow_locked(&f) == 1, "locked");
    bpm = midi_follow_bpm(&f);
    RI_ASSERT(bpm > 119.5f && bpm < 120.5f, "bpm %f", bpm);
    /* Jitter ±1 ms holds lock and estimate. */
    {
        uint32_t i;
        for (i = 0u; i < 48u; i++) {
            t += TICK120 + (i & 1u ? 1000u : (uint32_t)-1000);
            RI_ASSERT(midi_follow_tick(&f, t, &it) == 0, "jit %u", i);
        }
    }
    RI_ASSERT(midi_follow_locked(&f) == 1, "jit locked");
    bpm = midi_follow_bpm(&f);
    RI_ASSERT(bpm > 118.0f && bpm < 122.0f, "jit bpm %f", bpm);
    /* Wild tick (double gap): streak broken, estimate unmoved. */
    t += 2u * TICK120;
    RI_ASSERT(midi_follow_tick(&f, t, &it) == 0, "wild");
    RI_ASSERT(midi_follow_locked(&f) == 0, "wild unlocks");
    bpm = midi_follow_bpm(&f);
    RI_ASSERT(bpm > 118.0f && bpm < 122.0f, "wild keeps %f", bpm);
    feed(&f, &t, 24u, TICK120);
    RI_ASSERT(midi_follow_locked(&f) == 1, "relock");
    /* Dropout: silence past 96 intervals -> one latched STOP. */
    RI_ASSERT(midi_follow_poll(&f, t + 3000000u, &it) == 0 && it.kind == RI_FOLLOW_STOP,
        "dropout stop");
    RI_ASSERT(midi_follow_poll(&f, t + 9000000u, &it) == 0 && it.kind == RI_FOLLOW_NONE,
        "stop latched");
    /* A fresh tick clears the latch (take resumes, still locked data). */
    t += TICK120;
    RI_ASSERT(midi_follow_tick(&f, t, &it) == 0, "resume");
    /* Transport intents (M1: Start/Continue arm; the next tick fires). */
    midi_follow_init(&f);
    RI_ASSERT(midi_follow_start(&f, &it) == 0 && it.kind == RI_FOLLOW_NONE, "start arms");
    t += TICK120;
    RI_ASSERT(midi_follow_tick(&f, t, &it) == 0 && it.kind == RI_FOLLOW_PLAY_START,
        "tick fires start");
    RI_ASSERT(midi_follow_tick(&f, t + TICK120, &it) == 0 && it.kind == RI_FOLLOW_NONE,
        "fires once");
    RI_ASSERT(midi_follow_stop(&f, &it) == 0 && it.kind == RI_FOLLOW_STOP, "stop take");
    RI_ASSERT(midi_follow_continue(&f, &it) == 0 && it.kind == RI_FOLLOW_NONE, "cont arms");
    t += 2u * TICK120;
    RI_ASSERT(midi_follow_tick(&f, t, &it) == 0 && it.kind == RI_FOLLOW_CONTINUE,
        "tick fires cont");
    RI_ASSERT(midi_follow_stop(&f, &it) == 0 && it.kind == RI_FOLLOW_STOP, "stop");
    RI_ASSERT(midi_follow_start(0, &it) == 2, "start null");
    RI_ASSERT(midi_follow_spp(&f, 16u, 0) == 2, "spp null");
    /* M1: stop disarms, SPP running law, ignore-while-playing. */
    midi_follow_init(&f);
    t = 8000000u;
    RI_ASSERT(midi_follow_start(&f, &it) == 0 && it.kind == RI_FOLLOW_NONE, "arm");
    RI_ASSERT(midi_follow_stop(&f, &it) == 0 && it.kind == RI_FOLLOW_STOP, "disarm stop");
    t += TICK120;
    RI_ASSERT(midi_follow_tick(&f, t, &it) == 0 && it.kind == RI_FOLLOW_NONE,
        "no start after stop");
    RI_ASSERT(midi_follow_spp(&f, 16u, &it) == 0 && it.kind == RI_FOLLOW_SEEK &&
        it.seek_16ths == 64u, "spp stopped %u", it.seek_16ths);
    RI_ASSERT(midi_follow_start(&f, &it) == 0, "arm2");
    t += TICK120;
    RI_ASSERT(midi_follow_tick(&f, t, &it) == 0 && it.kind == RI_FOLLOW_PLAY_START, "fired");
    RI_ASSERT(midi_follow_spp(&f, 16u, &it) == 0 && it.kind == RI_FOLLOW_NONE,
        "spp running silent");
    RI_ASSERT(f.spp_ignored == 1u, "spp counted %u", f.spp_ignored);
    RI_ASSERT(midi_follow_start(&f, &it) == 0 && it.kind == RI_FOLLOW_NONE,
        "start ignored in play");
    t += TICK120;
    RI_ASSERT(midi_follow_tick(&f, t, &it) == 0 && it.kind == RI_FOLLOW_NONE,
        "no double start");
    RI_ASSERT(midi_follow_continue(&f, &it) == 0 && it.kind == RI_FOLLOW_NONE,
        "cont ignored in play");
    t += TICK120;
    RI_ASSERT(midi_follow_tick(&f, t, &it) == 0 && it.kind == RI_FOLLOW_NONE, "no cont fire");
    RI_ASSERT(midi_follow_stop(&f, &it) == 0 && it.kind == RI_FOLLOW_STOP, "stop ends take");
    RI_ASSERT(midi_follow_spp(&f, 16u, &it) == 0 && it.kind == RI_FOLLOW_SEEK,
        "spp seeks again");
    /* Twin determinism: identical streams, identical estimate. */
    midi_follow_init(&g);
    {
        struct RIFollow f2;
        uint64_t t2 = 1000000u, t3 = 1000000u;
        midi_follow_init(&f2);
        feed(&g, &t2, 25u, TICK120);
        feed(&f2, &t3, 25u, TICK120);
        RI_ASSERT(midi_follow_bpm(&g) == midi_follow_bpm(&f2), "twin bpm");
        RI_ASSERT(midi_follow_locked(&g) == midi_follow_locked(&f2), "twin lock");
    }
    RI_RESULT("midifollow");
}
