/* t126_midi_rt — realtime parser into the follower (R1 interop).
 * F8 ticks, FA/FB/FC transport, F2 + 2 data bytes SPP; channel
 * voice bytes ignored (G7 path owns them); split SPP across calls;
 * fail-closed.
 */
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "tests/helpers/ri_assert.h"
#include "midi_io/midi_follow.h"

int main(void) {
    struct RIFollow f;
    struct RIFollowIntent it;
    uint64_t t = 5000000u;
    uint32_t i;
    midi_follow_init(&f);
    /* Fail-closed. */
    RI_ASSERT(midi_follow_rt(0, 0xF8u, t, &it) == 2, "null f");
    RI_ASSERT(midi_follow_rt(&f, 0xF8u, t, 0) == 2, "null it");
    /* F8 stream locks like direct ticks. */
    for (i = 0u; i < 25u; i++) {
        t += 20833u;
        RI_ASSERT(midi_follow_rt(&f, 0xF8u, t, &it) == 0, "tick %u", i);
        RI_ASSERT(it.kind == RI_FOLLOW_NONE, "tick silent");
    }
    RI_ASSERT(midi_follow_locked(&f) == 1, "rt locked");
    /* Transport bytes. */
    RI_ASSERT(midi_follow_rt(&f, 0xFAu, t, &it) == 0 && it.kind == RI_FOLLOW_PLAY_START, "start");
    RI_ASSERT(midi_follow_rt(&f, 0xFBu, t, &it) == 0 && it.kind == RI_FOLLOW_CONTINUE, "cont");
    RI_ASSERT(midi_follow_rt(&f, 0xFCu, t, &it) == 0 && it.kind == RI_FOLLOW_STOP, "stop");
    /* SPP split across three calls: F2, lsb, msb. 0x00,0x01 -> 128 beats. */
    RI_ASSERT(midi_follow_rt(&f, 0xF2u, t, &it) == 0 && it.kind == RI_FOLLOW_NONE, "spp arm");
    RI_ASSERT(midi_follow_rt(&f, 0x00u, t, &it) == 0 && it.kind == RI_FOLLOW_NONE, "spp lsb");
    RI_ASSERT(midi_follow_rt(&f, 0x01u, t, &it) == 0 && it.kind == RI_FOLLOW_SEEK &&
        it.seek_tick == 3072u, "spp done %u", it.seek_tick);
    /* Stray data byte outside SPP: ignored. */
    RI_ASSERT(midi_follow_rt(&f, 0x40u, t, &it) == 0 && it.kind == RI_FOLLOW_NONE, "stray");
    /* Channel voice bytes ignored (G7 owns notes/CC). */
    RI_ASSERT(midi_follow_rt(&f, 0x90u, t, &it) == 0 && it.kind == RI_FOLLOW_NONE, "note on");
    RI_ASSERT(midi_follow_rt(&f, 0x3Cu, t, &it) == 0 && it.kind == RI_FOLLOW_NONE, "note arg");
    RI_ASSERT(midi_follow_rt(&f, 0xB0u, t, &it) == 0 && it.kind == RI_FOLLOW_NONE, "cc");
    /* Realtime interleaved inside SPP data still lands (MIDI law). */
    RI_ASSERT(midi_follow_rt(&f, 0xF2u, t, &it) == 0, "arm2");
    t += 20833u;
    RI_ASSERT(midi_follow_rt(&f, 0xF8u, t, &it) == 0 && it.kind == RI_FOLLOW_NONE, "tick inside");
    RI_ASSERT(midi_follow_rt(&f, 0x02u, t, &it) == 0 && it.kind == RI_FOLLOW_NONE, "lsb2");
    RI_ASSERT(midi_follow_rt(&f, 0x00u, t, &it) == 0 && it.kind == RI_FOLLOW_SEEK &&
        it.seek_tick == 48u, "spp2 %u", it.seek_tick);
    RI_RESULT("midirt");
}
