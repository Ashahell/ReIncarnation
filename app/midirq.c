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
#include <stdint.h>
#include <string.h>

/* INLINE BASES ARE OURS, AND THAT IS THE WHO LESSON OF THIS TOOL.
 *
 * app/midiclock.c gets this right and says why in a comment: the inline
 * timer and CAMD calls are routed through `__TIMER_LIBBASE` /
 * `__CAMD_LIBBASE` so they use bases this file declares, rather than
 * through globals that other TUs own. MIDIRX included <proto/timer.h>
 * plainly, so its calls went through a global `TimerBase` that I declared
 * as `struct Device *` -- while LIBBASETYPEPTR is `struct Library *`. The
 * v11 link deliberately runs WITHOUT -Werror, so that type mismatch
 * compiled, and the CPU paid for it at run time.
 *
 * The symptom pointed nowhere near this file: the report read
 *
 *     Error: 0x80000003 - Illegal address access
 *     Function Exec_49_FindTask (0x1AA4F80) Offset 0xA
 *     Stack: MIDIRX __startup_fromwb -> __startup_entry -> CallEntry
 *
 * i.e. the fault was in the C runtime's startup, BEFORE main() ever ran.
 * Every diagnostic this tool had was past the point of failure, which is
 * why two faults here had to be found by reading source and one by
 * reproducing on the lane whose console is legible (riqemu1) rather than
 * on the Dell, whose crash report renders as garbage.
 *
 * Hence also: this tool links STANDALONE. It does not link midi_camd.o,
 * whose CamdBase would collide with ours, and it does not need the
 * bridge, the clock-out producer or the follower -- it only listens.
 */
struct Library *CamdBase;          /* ours, for the inline CAMD calls */
static struct Device *MidirxClockBase;

/* The SAME pattern for dos.library. A v1 exec library call passes its base
 * in RDX, and the inline Open()/Write()/Close() in <proto/dos.h> go through
 * a global DOSBase. With that unresolved the crash lands INSIDE Kickstart's
 * Exec_77_SendIO with RDX = 0x50 -- a small value, not a base -- and the
 * report points at SendIO rather than at anything this file did. Three
 * inline bases in one tool, three ways to get it wrong, so all three are
 * named and opened here rather than inherited. */
static struct Library *MidirxDosBase;
#define __DOS_LIBBASE MidirxDosBase
#include <proto/dos.h>
#define __TIMER_LIBBASE MidirxClockBase
#include <proto/timer.h>
#define __CAMD_LIBBASE CamdBase
#include <proto/camd.h>

/* AROS EClock is a 64-bit counter split hi:lo, so hi and lo are NOT
 * separate clocks and every use goes through this helper:
 *
 *   us = (v / efreq) * 1e6 + ((v % efreq) * 1e6) / efreq
 *
 * The first version added microseconds to a 32-bit-of-64 counter
 * (`ev_hi + efreq * seconds`), which put the run's deadline about 51
 * billion years out, and subtracted ev_lo values, which underflows the
 * first time the low word wraps. The split form cannot do either. */
static uint64_t ec_us(const struct EClockVal *e, ULONG efreq) {
    uint64_t v;
    if (!e || !efreq)
        return 0u;
    v = ((uint64_t)e->ev_hi << 32) | (uint64_t)e->ev_lo;
    return (v / (uint64_t)efreq) * 1000000ULL +
        ((v % (uint64_t)efreq) * 1000000ULL) / (uint64_t)efreq;
}

#define SLICES 10u

/* REPORT TO A FILE, NOT TO STDOUT. Two reasons, both learned the hard way.
 *
 * The v1 SDK resolves printf through `__aros_getbase_StdCIOBase`, which a
 * -nostartfiles link with startup.o does not provide, so the tool would not
 * even link for the lane that could show me its console. And a lane proof
 * wants the report as a FILE it can pull with --get anyway, not as text
 * scraped from a requester whose own glyphs render as garbage.
 *
 * Open/Write/Close are dos.library, which is already linked, so this adds no
 * dependency at all. */
static BPTR g_out = 0;

static void say(const char *s) {
    if (g_out && s)
        Write(g_out, (CONST_STRPTR)s, (ULONG)strlen(s));
}

static void say_u(const char *tag, unsigned long v) {
    char buf[64];
    int n = 0;
    const char *p = tag;
    while (p && *p && n < 40)
        buf[n++] = *p++;
    buf[n++] = '=';
    if (v == 0UL)
        buf[n++] = '0';
    else {
        char d[24];
        int k = 0;
        unsigned long t = v;
        while (t && k < 22) {
            d[k++] = (char)('0' + (int)(t % 10UL));
            t /= 10UL;
        }
        while (k > 0 && n < 60)
            buf[n++] = d[--k];
    }
    buf[n++] = '\n';
    if (g_out)
        Write(g_out, (CONST_STRPTR)buf, (ULONG)n);
}



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
    ULONG i, slice;
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

    /* Resolve OUR inline bases FIRST, before a single inline call: dos for
     * Open/Write/Close, camd for CreateMidi/AddMidiLink. A NULL base is an
     * illegal memory access rather than a NULL return. */
    MidirxDosBase = OpenLibrary("dos.library", 0);
    if (!MidirxDosBase)
        return 10;
    if (!CamdBase)
        CamdBase = OpenLibrary("camd.library", 0);
    if (!CamdBase)
        return 10;

    /* The report file: argv[3] if given, else MIDIRX.LOG beside us. A proof
     * run should leave an artifact the lane can pull, not text on a
     * console nobody captures. */
    {
        CONST_STRPTR rp = (CONST_STRPTR)((argc >= 4) ? argv[3] : "MIDIRX.LOG");
        g_out = Open(rp, MODE_NEWFILE);   /* the mode name both SDKs agree on */
    }

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
    MidirxClockBase = tr->tr_node.io_Device;
    /* PROGRESS, not decoration. This tool had printed NOTHING until the
     * first message arrived, so a crash told you nothing about how far it
     * got -- which is why two faults in it had to be found by reading
     * rather than by running. Every risky step now announces itself, and
     * the LAST line printed before a Software Failure is the fault. */
    say("MIDIRX stage=open ok\n");
    ReadEClock(&base);
    efreq = ReadEClock(&base);
    say_u("MIDIRX efreq", (unsigned long)efreq);
    say_u("MIDIRX secs", (unsigned long)seconds);
    say("MIDIRX stage=clock ok\n");
    start_us = ec_us(&base, efreq);
    deadline_us = start_us + (uint64_t)seconds * 1000000ULL;
    slice_span_us = ((uint64_t)seconds * 1000000ULL) / SLICES;

    node = CreateMidi(MIDI_Name, (IPTR)"MIDIRX", TAG_END);
    say("MIDIRX stage=node ok\n");
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

    say("MIDIRX stage=link ok\n");
    for (i = 0u; i < SLICES; i++)
        slice_f8[i] = 0UL;
    slice0_us = start_us;
    slice = 0UL;
    prev_us = 0ULL;

    for (;;) {
        MidiMsg mm;
        ReadEClock(&now);
        now_us = ec_us(&now, efreq);
        if (now_us >= deadline_us)
            break;
        /* Drain everything queued before deciding we are done. */
        while (GetMidi(node, &mm)) {
            msg_total++;
            if (mm.mm_Status != 0xF8u) {
                if (mm.mm_Status == 0xFAu)
                    say("MIDIRX saw Start\n");
                else if (mm.mm_Status == 0xFCu)
                    say("MIDIRX saw Stop\n");
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

    say("MIDIRX cluster=");
    say(argv[1]);
    say_u("MIDIRX secs", (unsigned long)seconds);
    say_u("MIDIRX messages", (unsigned long)msg_total);
    say_u("MIDIRX f8", (unsigned long)f8);
    say_u("MIDIRX first_us", (unsigned long)first_us);
    say_u("MIDIRX last_us", (unsigned long)last_us);
    if (f8 >= 2UL) {
        ULONG mean = sum / (f8 - 1UL);
        say_u("MIDIRX interval_min_us", (unsigned long)(mn == 0xFFFFFFFFUL ? 0UL : mn));
        say_u("MIDIRX interval_max_us", (unsigned long)mx);
        say_u("MIDIRX interval_mean_us", (unsigned long)mean);
        say_u("MIDIRX interval_spread_us", (unsigned long)(mx - (mn == 0xFFFFFFFFUL ? 0UL : mn)));
        for (i = 0u; i < SLICES; i++)
            say_u("MIDIRX slice_f8", (unsigned long)slice_f8[i]);
        say((mx - mn <= (mean / 20UL) + 200UL) ?
            "MIDIRX verdict STEADY\n" : "MIDIRX verdict JITTERY\n");
    } else {
        say("MIDIRX verdict NOTHING\n");
        rc = 12;
    }

out:
    if (tr) {
        CloseDevice((struct IORequest *)tr);
        DeleteIORequest((struct IORequest *)tr);
    }
    if (port)
        DeleteMsgPort(port);
    if (g_out) {
        Close(g_out);
        g_out = 0;
    }
    return rc;
}
