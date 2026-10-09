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

/* ReadEClock is an inline timer call through clib's TimerBase, and no other
 * TU in this link provides one. Declaring it is what app/stepproof.c does
 * for the same reason -- but declaring is NOT the same as resolving: left
 * NULL, the very first ReadEClock is an illegal memory access, which is
 * the Software Failure this tool died of. It is assigned from the opened
 * timer device below, exactly as stepproof.c does. */
struct Device *TimerBase = NULL;

/* EClock is a 64-bit counter split into hi:lo, where lo is the low 32
 * bits of the hardware counter -- so hi and lo are NOT separate clocks.
 * Every use below goes through this helper, which reassembles the 64-bit
 * value and converts with the split form so it cannot overflow:
 *
 *   us = (v / efreq) * 1e6 + ((v % efreq) * 1e6) / efreq
 *
 * The earlier arithmetic mixed units outright -- `ev_hi + efreq * seconds`
 * added microseconds to a 32-bit-of-64 counter, so the loop's deadline was
 * about 51 billion years out and the tool could never have exited; and
 * `(now.ev_lo - prev.ev_lo)` underflowed every time the low word wrapped. */
static uint64_t ec_us(const struct EClockVal *e, ULONG efreq) {
    uint64_t v;
    if (!e || !efreq)
        return 0u;
    v = ((uint64_t)e->ev_hi << 32) | (uint64_t)e->ev_lo;
    return (v / (uint64_t)efreq) * 1000000ULL +
        ((v % (uint64_t)efreq) * 1000000ULL) / (uint64_t)efreq;
}

#define SLICES 10u

int main(int argc, char **argv) {
    struct MidiNode *node;
    struct MidiLink *link;
    struct timerequest *tr;
    struct MsgPort *port;
    struct EClockVal base, now;
    ULONG seconds = 10UL, efreq;
    uint64_t start_us, deadline_us, now_us, prev_us, slice0_us, slice_span_us;
    ULONG msg_total = 0, f8 = 0, interval = 0;
    ULONG first_us = 0, last_us = 0, mn = 0xFFFFFFFFUL, mx = 0, sum = 0;
    ULONG slice_f8[SLICES];
    ULONG i, slice, taken;
    int rc = 0;

    if (argc < 2)
        return 5;
    if (argc >= 3) {
        /* From index 0, not 1: starting at 1 skipped the leading digit, so
         * every single-digit duration parsed as 0 and was refused with rc=5
         * -- which looked like a tool failure rather than a parser bug. */
        const char *a2 = argv[2];
        size_t len = strlen(a2), k;
        if (len == 0u)
            return 5;
        seconds = 0UL;
        for (k = 0u; k < len; k++) {
            if (a2[k] < '0' || a2[k] > '9')
                return 5;
            seconds = seconds * 10UL + (ULONG)(a2[k] - '0');
        }
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
    /* Resolve the inline timer base from the device just opened, or the
     * first ReadEClock below faults. */
    TimerBase = tr->tr_node.io_Device;
    ReadEClock(&base);
    efreq = ReadEClock(&base);
    start_us = ec_us(&base, efreq);
    deadline_us = start_us + (uint64_t)seconds * 1000000ULL;
    slice_span_us = ((uint64_t)seconds * 1000000ULL) / SLICES;

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
    slice0_us = start_us;
    slice = 0UL;
    prev_us = 0ULL;
    taken = 0UL;

    for (;;) {
        MidiMsg mm;
        ReadEClock(&now);
        now_us = ec_us(&now, efreq);
        if (now_us >= deadline_us)
            break;
        /* Drain everything queued before deciding we are done. */
        while (GetMidi(node, &mm)) {
            msg_total++;
            taken = (ULONG)now_us;
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
            if (prev_us != 0ULL) {
                /* Subtract in 64-bit microseconds: doing it on hi/lo is a
                 * 32-bit underflow the first time the low word wraps. */
                uint64_t d = now_us - prev_us;
                interval = (ULONG)(d > 0ULL ? d : 1ULL);
                if (first_us == 0UL)
                    first_us = interval;
                last_us = interval;
                sum += interval;
                if (interval < mn)
                    mn = interval;
                if (interval > mx)
                    mx = interval;
            }
            prev_us = now_us;
        }
        /* Slice boundaries on the wall clock, so a drifting clock shows up
         * as unequal slices rather than being averaged away. */
        if (slice_span_us && now_us >= slice0_us + slice_span_us) {
            slice0_us = now_us;
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
