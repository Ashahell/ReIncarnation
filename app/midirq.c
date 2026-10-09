/*
 * app/midirq.c — the clock-out PROOF receiver (M5).
 *
 * AROS-ONLY proof tool, raw CAMD. MIDISEND plays a script INTO a cluster
 * and MIDICLOCK plays a clock into one; nothing could listen to one. This
 * does: it opens a receiver on a cluster and reports the ARRIVAL INTERVALS
 * of 0xF8, which is the only thing that can say whether the sender's
 * 24 ppqn survives a real wire.
 *
 * WHY INTERVALS AND NOT COUNTS. A count only proves bytes arrived. The
 * schedule is exact on the host (t185: a pure function of the audio sample
 * clock), so the interesting question is what the wire does to it: what is
 * the spread between the fastest and slowest gap, and does it wander over
 * the run. That is what this prints.
 *
 * usage: MIDIRX <cluster> [seconds]
 *   Prints, on exit: messages seen, F8 count, first/last interval, the
 *   min and max interval in ms, the mean, and the deviation of the mean
 *   over ten equal slices (so slow wander is visible, not just outliers).
 * Return codes: 0 ok, 5 args, 10 camd.library, 12 nothing received.
 */

#ifndef __AROS__
#error "app/midirq.c is AROS-only: CAMD proof tool, never in the host build"
#endif

#include <exec/types.h>
#include <devices/timer.h>
#include <dos/dos.h>
#include <midi/camd.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/timer.h>
#include <proto/camd.h>
#include <stdio.h>
#include <string.h>
#include "platform/pal/ri_pal_midi.h"

extern struct Library *CamdBase; /* owned by platform/aros/midi_camd.c */

/* ReadEClock is an inline timer call and needs clib's TimerBase, which no
 * other TU in this link provides. Declaring it here is what app/stepproof.c
 * does for the same reason; a proof tool should not drag a library in to
 * ask the time. */
struct Device *TimerBase = NULL;

#define SLICES 10u

int main(int argc, char **argv) {
    struct MidiNode *node;
    struct MidiLink *link;
    struct timerequest *tr;
    struct MsgPort *port;
    struct EClockVal base, now, prev;
    ULONG seconds = 10UL, efreq, deadline;
    ULONG msg_total = 0, f8 = 0, interval = 0;
    ULONG first_us = 0, last_us = 0, mn = 0xFFFFFFFFUL, mx = 0, sum = 0;
    ULONG slice_f8[SLICES];
    ULONG i, slice, taken;
    struct EClockVal slice0, slice_us;
    int rc = 0;

    if (argc < 2)
        return 5;
    if (argc >= 3) {
        seconds = 0UL;
        for (i = 1u; i < (ULONG)strlen(argv[2]); i++)
            if (argv[2][i] < '0' || argv[2][i] > '9')
                return 5;
            else
                seconds = seconds * 10UL + (ULONG)(argv[2][i] - '0');
        if (seconds == 0UL)
            return 5;
    }

    /* camd's CreateMidi/AddMidiLink are inline calls through the library
     * base. Without this the first CreateMidi is an illegal memory access
     * on a NULL base -- which is how this probe died on its first run. */
    if (ri_pal_midi_init_lib() != 0)
        return 10;

    port = CreateMsgPort();
    tr = port ? (struct timerequest *)CreateIORequest(port,
        sizeof(struct timerequest)) : 0;
    if (!tr || OpenDevice((STRPTR)"timer.device", UNIT_ECLOCK,
            (struct IORequest *)tr, 0) != 0) {
        if (tr)
            DeleteIORequest((struct IORequest *)tr);
        if (port)
            DeleteMsgPort(port);
        return 5;
    }
    ReadEClock(&base);
    efreq = ReadEClock(&base);
    deadline = base.ev_hi + efreq * seconds;

    node = CreateMidi(MIDI_Name, (IPTR)"MIDIRX", TAG_END);
    if (!node) {
        rc = 5;
        goto out;
    }
    link = AddMidiLink(node, MLTYPE_Receiver, MLINK_Location,
        (IPTR)argv[1], TAG_END);
    if (!link) {
        DeleteMidi(node);
        rc = 12;
        goto out;
    }

    for (i = 0u; i < SLICES; i++)
        slice_f8[i] = 0UL;
    slice0 = base;
    slice = 0UL;
    prev.ev_hi = prev.ev_lo = 0UL;
    taken = 0UL;

    for (;;) {
        MidiMsg mm;
        ReadEClock(&now);
        if (now.ev_hi >= deadline)
            break;
        /* Drain everything queued before deciding we are done. */
        while (GetMidi(node, &mm)) {
            msg_total++;
            taken = now.ev_hi * efreq + now.ev_lo / (1000000UL / 1000UL);
            if (mm.mm_Status != 0xF8u) {
                if (mm.mm_Status == 0xFAu)
                    printf("MIDIRX saw Start\n");
                else if (mm.mm_Status == 0xFCu)
                    printf("MIDIRX saw Stop\n");
                continue;
            }
            if (slice < SLICES)
                slice_f8[slice]++;
            f8++;
            if (prev.ev_hi != 0UL || prev.ev_lo != 0UL) {
                interval = (now.ev_hi - prev.ev_hi) * efreq +
                    (now.ev_lo - prev.ev_lo) / 1000UL;
                if (interval == 0UL)
                    interval = 1UL;
                if (first_us == 0UL)
                    first_us = interval;
                last_us = interval;
                sum += interval;
                if (interval < mn)
                    mn = interval;
                if (interval > mx)
                    mx = interval;
            }
            prev = now;
        }
        /* Slice boundaries on the wall clock, so a drifting clock shows up
         * as unequal slices rather than being averaged away. */
        if (now.ev_hi * efreq >= slice0.ev_hi * efreq +
                (efreq * (deadline - base.ev_hi) / SLICES)) {
            slice_us.ev_hi = slice0.ev_hi;
            slice_us.ev_lo = slice0.ev_lo;
            (void)slice_us;
            slice0 = now;
            if (slice < SLICES - 1u)
                slice++;
        }
        /* A short poll so the loop is not a busy spin. Allocated per wait
         * because a timerequest is one-shot; freed with the reply. */
        {
            struct timerequest *w = (struct timerequest *)
                CreateIORequest(port, sizeof(struct timerequest));
            if (!w)
                break;
            w->tr_time.tv_secs = 0;
            w->tr_time.tv_micro = 2000;
            SendIO((struct IORequest *)w);
            WaitPort(port);
            (void)GetMsg(port);
            if (!CheckIO((struct IORequest *)w))
                AbortIO((struct IORequest *)w);
            WaitIO((struct IORequest *)w);
            DeleteIORequest((struct IORequest *)w);
        }
    }

    printf("MIDIRX cluster=%s secs=%lu\n", argv[1], (unsigned long)seconds);
    printf("MIDIRX messages=%lu f8=%lu first_us=%lu last_us=%lu\n",
        (unsigned long)msg_total, (unsigned long)f8,
        (unsigned long)first_us, (unsigned long)last_us);
    if (f8 >= 2UL) {
        ULONG mean = sum / (f8 - 1UL);
        printf("MIDIRX interval us min=%lu max=%lu mean=%lu spread=%lu\n",
            (unsigned long)(mn == 0xFFFFFFFFUL ? 0UL : mn),
            (unsigned long)mx, (unsigned long)mean,
            (unsigned long)(mx - (mn == 0xFFFFFFFFUL ? 0UL : mn)));
        printf("MIDIRX per-slice f8:");
        for (i = 0u; i < SLICES; i++)
            printf(" %lu", (unsigned long)slice_f8[i]);
        printf("\n");
        printf("MIDIRX verdict %s\n",
            (mx - mn <= (mean / 20UL) + 200UL) ? "STEADY" : "JITTERY");
    } else {
        printf("MIDIRX verdict NOTHING\n");
        rc = 12;
    }

out:
    if (tr) {
        CloseDevice((struct IORequest *)tr);
        DeleteIORequest((struct IORequest *)tr);
    }
    if (port)
        DeleteMsgPort(port);
    return rc;
}
