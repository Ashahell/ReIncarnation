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
    /* Transport bytes arm; the next F8 fires (MIDI 1.0 downbeat law). */
    RI_ASSERT(midi_follow_rt(&f, 0xFAu, t, &it) == 0 && it.kind == RI_FOLLOW_NONE, "fa arms");
    t += 20833u;
    RI_ASSERT(midi_follow_rt(&f, 0xF8u, t, &it) == 0 && it.kind == RI_FOLLOW_PLAY_START,
        "f8 fires start");
    RI_ASSERT(midi_follow_rt(&f, 0xFCu, t, &it) == 0 && it.kind == RI_FOLLOW_STOP, "stop");
    RI_ASSERT(midi_follow_rt(&f, 0xFBu, t, &it) == 0 && it.kind == RI_FOLLOW_NONE, "fb arms");
    t += 20833u;
    RI_ASSERT(midi_follow_rt(&f, 0xF8u, t, &it) == 0 && it.kind == RI_FOLLOW_CONTINUE,
        "f8 fires cont");
    RI_ASSERT(midi_follow_rt(&f, 0xFCu, t, &it) == 0 && it.kind == RI_FOLLOW_STOP, "stop2");
    /* FA then STOP before any F8: no start. */
    RI_ASSERT(midi_follow_rt(&f, 0xFAu, t, &it) == 0 && it.kind == RI_FOLLOW_NONE, "fa rearms");
    RI_ASSERT(midi_follow_rt(&f, 0xFCu, t, &it) == 0 && it.kind == RI_FOLLOW_STOP, "stop disarms");
    t += 20833u;
    RI_ASSERT(midi_follow_rt(&f, 0xF8u, t, &it) == 0 && it.kind == RI_FOLLOW_NONE, "no start");
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
    /* SPP while running is counted and ignored (E0: stopped-only seeks). */
    RI_ASSERT(midi_follow_rt(&f, 0xFAu, t, &it) == 0 && it.kind == RI_FOLLOW_NONE, "fa run");
    t += 20833u;
    RI_ASSERT(midi_follow_rt(&f, 0xF8u, t, &it) == 0 && it.kind == RI_FOLLOW_PLAY_START,
        "run fires");
    RI_ASSERT(midi_follow_rt(&f, 0xF2u, t, &it) == 0 && it.kind == RI_FOLLOW_NONE, "spp arm run");
    RI_ASSERT(midi_follow_rt(&f, 0x00u, t, &it) == 0 && it.kind == RI_FOLLOW_NONE, "spp lsb run");
    RI_ASSERT(midi_follow_rt(&f, 0x0Au, t, &it) == 0 && it.kind == RI_FOLLOW_NONE, "spp ignored");
    RI_ASSERT(f.spp_ignored == 1u, "spp counted %u", f.spp_ignored);
    /* Song-mode Live sequence: STOP, SPP n, CONTINUE, F8 seeks then continues. */
    RI_ASSERT(midi_follow_rt(&f, 0xFCu, t, &it) == 0 && it.kind == RI_FOLLOW_STOP, "live stop");
    RI_ASSERT(midi_follow_rt(&f, 0xF2u, t, &it) == 0 && it.kind == RI_FOLLOW_NONE, "live spp");
    RI_ASSERT(midi_follow_rt(&f, 0x0Au, t, &it) == 0 && it.kind == RI_FOLLOW_NONE, "live lsb");
    RI_ASSERT(midi_follow_rt(&f, 0x00u, t, &it) == 0 && it.kind == RI_FOLLOW_SEEK &&
        it.seek_tick == 240u, "live seek %u", it.seek_tick);
    RI_ASSERT(midi_follow_rt(&f, 0xFBu, t, &it) == 0 && it.kind == RI_FOLLOW_NONE, "live cont");
    t += 20833u;
    RI_ASSERT(midi_follow_rt(&f, 0xF8u, t, &it) == 0 && it.kind == RI_FOLLOW_CONTINUE,
        "live continues");
    RI_RESULT("midirt");
}
