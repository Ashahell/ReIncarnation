/*
 * app/midiclock.c — a scripted MIDI clock master that can reach the song's
 * own tempo (G7 proof tool; AROS-ONLY, never in the host build).
 *
 * Why this exists: MIDISEND (app/midisend.c) is a script player, and its
 * per-message cost on a lane is ~25-40 ms, so the fastest clock it can
 * stream is about 25 clocks/s = 60 BPM. The demo song is written at
 * 140 BPM (17.9 ms per clock), so every MIDISEND proof ran RIAPP at less
 * than half the song's tempo -- the owner heard "slow, low tempo" and
 * failed M3 on that, correctly. This tool streams the clock itself and
 * prints the tempo it ACTUALLY achieved, so a lane can prove the app at
 * the song's tempo or say plainly that the lane cannot.
 *
 * Usage: MIDICLOCK <bpm> <seconds> [cluster]
 *   bpm     20..300
 *   seconds how long to stream
 *   cluster CAMD cluster (default "riapp")
 *
 * Sends Start (FA), a steady clock at the mean interval 60000/(bpm*24) ms,
 * Stop (FC). Timing: the system tick is 10 ms, finer than the 17.9 ms a
 * 140 BPM clock needs, so a microsecond accumulator decides how long to
 * sleep each round; the mean rate is exact and the jitter is bounded by
 * one tick, which is inside what a real master's jitter looks like.
 *
 * Raw CAMD node/link, exactly like MIDISEND's SELFTEST path: no PAL, no
 * script parsing, one PutMidi per clock.
 */

#ifndef __AROS__
#error "app/midiclock.c is AROS-only: CAMD proof tool, never in the host build"
#endif

#include <exec/types.h>
#include <stdint.h>
#include <devices/timer.h>
#include <midi/camd.h>
#include <proto/exec.h>
#include <proto/dos.h>
/* The inline timer calls go through our own base (the TU-global TimerBase
 * belongs to other TUs), exactly as platform/aros/midi_camd.c does it. */
#define __TIMER_LIBBASE ClockDev
#include <proto/timer.h>
/* Likewise for CAMD: our own base, so this tool links standalone (no
 * midi_camd.o, whose CamdBase would collide with ours). */
#define __CAMD_LIBBASE CamdBase
#include <proto/camd.h>
#include <stdio.h>
#include <stdlib.h>

struct Library *CamdBase;
static struct Device *ClockDev;
static struct timerequest *ClockReq;
static struct MsgPort *ClockPort;
static ULONG tick_rate;

/* Microseconds from timer.device's EClock (no intuition, no dos: those
 * clocks are not monotonic here). Rate is read once. */
static double now_us(void) {
    struct EClockVal ev;
    uint64_t ticks;
    if (!ClockDev || !tick_rate)
        return 0.0;
    ReadEClock(&ev);
    ticks = ((uint64_t)ev.ev_hi << 32) | (uint64_t)ev.ev_lo;
    if (!tick_rate)
        return 0.0;
    return (double)(ticks / tick_rate) * 1000000.0
        + (double)(ticks % tick_rate) * 1000000.0 / (double)tick_rate;
}

static void put_clock(struct MidiLink *link, ULONG status) {
    PutMidi(link, status << 24);
}

int main(int argc, char **argv) {
    struct MidiNode *node;
    struct MidiLink *link;
    double bpm, per_us, base, next_us, elapsed;
    LONG sec, usec, want, sent = 0, i;

    if (argc < 3)
        return 5;
    bpm = atof(argv[1]);
    want = atol(argv[2]) * (LONG)(atof(argv[1]) * 24.0 / 60.0);
    if (bpm < 20.0 || bpm > 300.0 || want < 1)
        return 5;
    CamdBase = OpenLibrary((CONST_STRPTR)"camd.library", 0);
    if (!CamdBase)
        return 10;
    node = CreateMidi(MIDI_Name, (IPTR)"MIDICLOCK", TAG_END);
    link = node ? AddMidiLink(node, MLTYPE_Sender, MLINK_Location,
        (IPTR)(argc > 3 ? argv[3] : "riapp"), TAG_END) : NULL;
    if (!link) {
        Printf((CONST_STRPTR)"MIDICLOCK: no CAMD sender link\n");
        if (node)
            DeleteMidi(node);
        CloseLibrary(CamdBase);
        return 11;
    }
    /* Wait for the cluster to have a receiver, like a real master would. */
    for (i = 0; i < 100 && !MidiLinkConnected(link); i++)
        Delay(1);

    ClockPort = CreateMsgPort();
    ClockReq = ClockPort
        ? (struct timerequest *)CreateIORequest(ClockPort, sizeof(struct timerequest))
        : NULL;
    if (!ClockReq) {
        Printf((CONST_STRPTR)"MIDICLOCK: no IO request\n");
        return 12;
    }
    {
        LONG rc = OpenDevice((STRPTR)"timer.device", UNIT_ECLOCK,
            (struct IORequest *)ClockReq, 0);
        if (rc != 0) {
            Printf((CONST_STRPTR)"MIDICLOCK: timer.device open failed rc=%ld\n", rc);
            return 12;
        }
    }
    ClockDev = ClockReq->tr_node.io_Device;
    {
        struct EClockVal t0;
        tick_rate = ReadEClock(&t0);
        if (!tick_rate)
            tick_rate = 50UL;
    }
    per_us = 60000000.0 / (bpm * 24.0);
    base = now_us();
    next_us = 0.0;
    /* No stdcio here, so Printf has no %f: print scaled integers. */
    Printf((CONST_STRPTR)"MIDICLOCK: bpm %ld.%01ld (%ld.%02ld ms/clock), %ld clocks\n",
        (LONG)bpm, (LONG)((bpm - (double)(LONG)bpm) * 10.0),
        (LONG)(per_us / 1000.0), (LONG)((per_us - (double)(LONG)(per_us / 1000.0) * 1000.0) / 10.0),
        want);
    put_clock(link, 0xFAu);
    for (i = 0; i < want; i++) {
        double now = now_us();
        LONG wait = (LONG)((base + next_us - now) / 10000.0);
        if (wait > 0)
            Delay(wait);
        put_clock(link, 0xF8u);
        sent++;
        next_us += per_us;
    }
    put_clock(link, 0xFCu);
    elapsed = now_us() - base;
    sec = (LONG)(elapsed / 1000000.0);
    usec = (LONG)(elapsed - (double)sec * 1000000.0);
    {
        double got = elapsed > 0.0 ? (double)sent / 24.0 / (elapsed / 60000000.0) : 0.0;
        Printf((CONST_STRPTR)"MIDICLOCK: sent %ld clocks in %ld.%06lds -> bpm %ld.%02ld realised "
            "(asked %ld.%01ld)\n",
            sent, sec, usec, (LONG)got, (LONG)((got - (double)(LONG)got) * 100.0),
            (LONG)bpm, (LONG)((bpm - (double)(LONG)bpm) * 10.0));
    }
    DeleteMidi(node);
    CloseLibrary(CamdBase);
    return 0;
}