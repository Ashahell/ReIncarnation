/*
 * app/midisend.c — play a MIDI message script into a CAMD cluster
 * (§12.10 G7 proof tool).
 *
 * AROS-ONLY. usage: MIDISEND <cluster> <script>
 * Script lines: "st d1 d2" (hex bytes, e.g. "90 45 64"; realtime "F8"),
 * "D <ms>" to wait, "#" comments. Messages go out through a CAMD sender
 * link, exactly as a hardware or sequencer source would reach a
 * receiver on the same cluster — so RISECT midi exercises the real
 * camd.library path on a lane without MIDI hardware.
 * "MIDISEND <cluster> SELFTEST" sends one Note On through a sender link
 * to a receiver link of the same process and prints what GetMidi saw.
 * "MIDISEND <cluster> LISTEN <secs> <logfile>" opens a RECEIVER through the
 * M2 PAL path (platform/aros/midi_camd.c) and reports F8 arrival intervals
 * -- the clock-out proof, and the reason this mode exists here rather than
 * in a separate tool. See the LISTEN block for why.
 * Return codes: 0 ok, 5 args, 10 camd.library, 11 node/link, 12 script,
 * 20 self-test received nothing, 21 receiver open, 22 log file,
 * 23 nothing arrived.
 */

#ifndef __AROS__
#error "app/midisend.c is AROS-only: CAMD proof tool, never in the host build"
#endif

#include <exec/types.h>
#include <exec/io.h>
#include <devices/timer.h>
#include <dos/dos.h>
#include <midi/camd.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include "platform/pal/ri_pal_midi.h"
#include "midi_io/midi_interval.h"
#include "midi_io/midi_out.h"

/* ReadEClock is an INLINE call through the timer's library base. Leaving
 * it to the TU-global TimerBase is what left MIDIRX with a NULL base and
 * an illegal access -- and four of the five failures in that whole
 * debugging round were unresolved inline bases, not logic errors. So the
 * base is a file-scope variable of ours, set from the request we actually
 * opened, and the include comes AFTER the macro so the inline is
 * rewritten to use it. */
static struct Library *s_timer_base;
#define __TIMER_LIBBASE s_timer_base
#include <proto/timer.h>
#include <proto/camd.h>

extern struct Library *CamdBase; /* owned by platform/aros/midi_camd.c (T5) */


static int hexv(char c) {
    return c >= '0' && c <= '9' ? c - '0' : c >= 'a' && c <= 'f' ? c - 'a' + 10 : c >= 'A' && c <= 'F' ? c - 'A' + 10 : -1;
}

/* LISTEN state. One receiver, one statistics accumulator, written from
 * the receiver task's context and read after the run. */
static struct RIMidiInterval s_ivl;
static uint32_t s_clocks;        /* F8 arrivals */
static uint32_t s_other;         /* everything else, so silence is visible */
static uint32_t s_badlen;        /* messages that were not 1 byte */

/* Decimal append, no snprintf: the v1 SDK's is not worth trusting, and this
 * file is the artifact the proof is read from. */
static char *put_u32(char *p, uint32_t v) {
    char tmp[12];
    int n = 0;
    do { tmp[n++] = (char)('0' + (int)(v % 10u)); v /= 10u; } while (v);
    while (n)
        *p++ = tmp[--n];
    return p;
}

static char *put_kv(char *p, const char *key, uint32_t v) {
    while (*key)
        *p++ = *key++;
    *p++ = '=';
    return put_u32(p, v);
}


/* PROGRESS GOES TO A FILE, NOT Printf. The lane agent captures script output
 * through a pipe, so a child's stdout is BLOCK buffered: while the tool is
 * hung, not one byte reaches the console. That is why two hung runs showed
 * no output at all -- not even the setup prints that had certainly executed
 * -- and it is the same trap as MIDIRX, which wrote MIDIRX.LOG through
 * dos.library for exactly this reason. A proof tool that only speaks on
 * exit says nothing at all when it does not exit.
 *
 * dos Write() goes straight to the filesystem with no userspace buffer, so
 * each line is on the device before the next statement runs. */
static BPTR s_prog;
static void prog(const char *s) {
    LONG n = 0;
    while (s[n])
        n++;
    if (s_prog)
        Write(s_prog, (APTR)s, n);
}

static void on_midi(void *user, const uint8_t *msg, uint32_t len,
    uint64_t time_us) {
    (void)user;
    if (!msg || !len)
        return;
    if (msg[0] == 0xF8u) {
        if (len != 1u) {
            /* A realtime byte must arrive alone. Counting it as a clock
             * anyway would let a framing fault look like a steady wire. */
            s_badlen++;
            return;
        }
        s_clocks++;
        midi_interval_add(&s_ivl, time_us);
        return;
    }
    s_other++;
}

/* One byte to the cluster, for midi_out_pump's sink. The cluster name is a
 * static buffer, not a pointer into argv: the sink is called from deep
 * inside the pump with no lifetime guarantee on argv. */
static char s_send_user[64];

static uint32_t s_sent_bytes;   /* what actually left for camd */

static int send_one(const uint8_t *b, uint32_t n, void *user) {
    int r;
    (void)user;
    r = ri_pal_midi_send(s_send_user, b, n);
    /* A CROSS-CHECK, not the reported figure. `sent` is reported from what
     * midi_out_pump says it pumped; this counts what actually left the sink.
     * An earlier version reported sent=0 because the counter was assigned
     * inside a timing loop a later edit removed, and `lost` is computed from
     * it -- so a zero there silently turned every run into "lost=0". A check
     * that cannot fail is not a check, and two independent sources for one
     * number is cheaper than discovering that again. */
    if (r == 0)
        s_sent_bytes += n;
    return r;
}

/* CLOCKLOOP <secs>: send and receive inside ONE process.
 *
 * The lane runs one script at a time, so a sender and a listener in two
 * processes cannot overlap -- `cmd &` does not background here, the script
 * simply stops after the backgrounded line and the sender never runs, which
 * is why the first CLOCKLOOP attempt reported clocks=0 and looked like a
 * dead wire rather than a scheduling fact.
 *
 * So the two ends are the two TASKS of one process, which is the same shape
 * RIAPP has anyway: the sender runs here on the main task, the receiver is
 * the M2 task stamping arrivals with EClock. The camd path between them is
 * a real link on a real cluster; only the process boundary is gone.
 *
 * WHAT THIS DOES AND DOES NOT PROVE. It proves the camd path carries a
 * clock without dropping, bunching or reordering it, and it counts what
 * arrives against what was sent. It does NOT prove wire TIMING on a
 * real-time host: riqemu1 has no real-time audio pacing, so the sender's
 * own cadence is whatever the emulation manages and the measured intervals
 * are VM time. The interval numbers that mean something come from the Dell.
 */
static int do_clockloop(const char *cluster, ULONG secs, const char *logpath) {
    ULONG i, pumped = 0u;
    struct Device *tkb = NULL;
    BPTR lfh;
    char *p;
    static char buf[2048];
    /* The REAL schedule, not a Delay loop. The first version of this mode
     * sent one F8 per `Delay(1)`, and Delay(1) is a 20 ms shell tick plus
     * the send's own cost -- so it produced min~20000 / max~40000 on the
     * Dell AND on riqemu1 alike, identically. A spread that does not move
     * when the host's real-time pacing changes is the SENDER's spread, and
     * the instrument was measuring itself again, which is the exact defect
     * that killed MIDIRX.
     *
     * So this drives the same `midi_out` producer RIAPP drives, advanced by
     * the wall clock instead of the audio sample clock. The producer is
     * where the no-drift property lives (t185: the tick count at a sample
     * position is a pure function of that position), so the intervals this
     * reports are the schedule's own, not the loop's. */
    static struct RIMidiOut out;
    struct timerequest *trq = NULL;
    struct MsgPort *tp = NULL;
    uint64_t s_efreq = 0ULL;
    uint64_t us0 = 0ULL;
    const uint32_t SR = 48000u;
    {   /* Progress goes beside the caller's log, not at a hard-coded path:
         * the portability gate (T6) forbids Amiga volume literals outside
         * platform/aros/, and deriving it also means the progress file and
         * the result file can never end up on different volumes. */
        char pp[96];
        uint32_t k = 0u;
        while (logpath[k] && k + 3u < sizeof pp) { pp[k] = logpath[k]; k++; }
        pp[k++] = '.';
        pp[k++] = 'P';
        pp[k] = 0;
        s_prog = Open((CONST_STRPTR)pp, MODE_NEWFILE);
    }
    prog("A entered\n");
    if (ri_pal_midi_open_in(cluster, on_midi, NULL) != 0) {
        Printf((CONST_STRPTR)"CLOCKLOOP: receiver open refused\n");
        return 21;
    }
    prog("B receiver open\n");
    /* A MICROHZ timer opened PROPERLY, as an OpenDevice handshake on a
     * real request -- the thing MIDIRX got wrong. */
    tp = CreateMsgPort();
    if (tp)
        trq = (struct timerequest *)CreateIORequest(tp, sizeof(struct timerequest));
    prog("C port+request made\n");
    if (!trq || OpenDevice((STRPTR)"timer.device", UNIT_MICROHZ,
            (struct IORequest *)trq, 0L) != 0) {
        Printf((CONST_STRPTR)"CLOCKLOOP: timer.device refused\n");
        return 24;
    }
    prog("D timer opened\n");
    if (!tkb)
        tkb = (struct Device *)trq->tr_node.io_Device;
    s_timer_base = (struct Library *)tkb;
    {
        struct EClockVal t0;
        s_efreq = ReadEClock(&t0);
    }
    { char b[48]; char *q = b; ULONG v = (ULONG)s_efreq; const char *pr = "E eclock freq ";
      while (*pr) *q++ = *pr++;
      do { *q++ = (char)('0' + (int)(v % 10UL)); v /= 10UL; } while (v);
      *q++ = '\n'; *q = 0; prog(b); }
    if (!s_efreq) {
        Printf((CONST_STRPTR)"CLOCKLOOP: no EClock frequency\n");
        return 24;
    }
    midi_out_init(&out, SR, 140000u /* 140 BPM in milli */, 0u /* no lead */);
    midi_out_enable(&out, 1);
    midi_out_start(&out, 0u);
    /* No signal is allocated. There is nothing to wait on: this SDK's
     * struct IORequest has no io_Signal, so the timer reply arrives as a
     * MESSAGE and WaitPort is the whole of the wait. An earlier version
     * allocated one and Wait()ed on it -- an unconditional hang. */
    prog("F producer started\n");
    Printf((CONST_STRPTR)"CLOCKLOOP: %lu s on %s at 140 BPM\n", secs, (IPTR)cluster);
    prog("G entering loop\n");
    for (i = 0UL; i < secs * 200UL; i++) {   /* 200 Hz service rate */
        struct EClockVal ec;
        uint64_t us, samp, eticks;
        /* Progress, coarse. This loop has now hung the Dell twice with no
         * crash requester, so the ONLY evidence available is which line it
         * reached last. Printed from a lane whose console is legible
         * (riqemu1) rather than the one that cannot be read. */
        if ((i % 200u) == 0u) {
            char b[80]; char *q = b; const char *pr = "H iter "; ULONG v = i;
            while (*pr) *q++ = *pr++;
            do { *q++ = (char)('0' + (int)(v % 10UL)); v /= 10UL; } while (v);
            pr = " sent "; while (*pr) *q++ = *pr++;
            v = pumped; do { *q++ = (char)('0' + (int)(v % 10UL)); v /= 10UL; } while (v);
            pr = " clocks "; while (*pr) *q++ = *pr++;
            v = s_clocks; do { *q++ = (char)('0' + (int)(v % 10UL)); v /= 10UL; } while (v);
            *q++ = '\n'; *q = 0; prog(b);
        }
        ReadEClock(&ec);
        eticks = ((uint64_t)ec.ev_hi << 32) | (uint64_t)ec.ev_lo;
        us = eticks / s_efreq * 1000000u + (eticks % s_efreq) * 1000000u / s_efreq;
        if (!us0)
            us0 = us;
        samp = (us - us0) * (uint64_t)SR / 1000000ULL;
        /* Render at the sample position this wall clock has reached, then
         * carry whatever it produced to camd. */
        if ((i % 200u) == 0u)
            Printf((CONST_STRPTR)"CLOCKLOOP: iter %lu eclock ok samp %lu\n",
                (ULONG)i, (ULONG)samp);
        midi_out_render(&out, samp);
        if ((i % 200u) == 0u)
            prog("   rendered\n");
        pumped += midi_out_pump(&out, send_one, 0, 64u);
        if ((i % 200u) == 0u)
            prog("   pumped\n");
        /* Service the 1 ms timer, on the pattern platform/aros/midi_camd.c
         * already uses and proves: an allocated signal, SetSignal(0) to arm
         * it BEFORE SendIO, then Wait on that bit.
         *
         * The first version of this loop did `WaitPort(tp)` then
         * `Wait(0L)` -- waiting on NO signal bits, which is undefined and
         * on this lane hung forever, taking the agent's whole command path
         * with it and needing an owner reboot. SetSignal-before-SendIO is
         * not optional: an unarmed signal can miss the reply entirely, so
         * the wait never returns.
         *
         * GetMsg is deliberately NOT called here: WaitIO removes the
         * message, and doing both corrupts the port's list. That was one
         * of MIDIRX's four faults and the fix belongs in the code that
         * replaces it. */
        /* NO SendIO IN THIS LOOP, AND THAT IS THE POINT.
         *
         * Three attempts at servicing a 1 ms timer here all failed, and the
         * failures were only ever visible as "the run never finished" or as
         * a crash three layers away:
         *
         *  - reusing the request passed to OpenDevice: hung the lane;
         *  - SetSignal + Wait on an allocated bit: hung the lane, because
         *    this SDK's struct IORequest has NO io_Signal, so nothing ever
         *    raises that bit and Wait(sigbit) is an unconditional hang;
         *  - a fresh request per iteration: SendIO reached Exec_77_SendIO
         *    with RSI=0, a NULL io_Request, and faulted on its first
         *    dereference.
         *
         * None of that is needed. The loop wants a ~1 ms service tick, and
         * Delay(1) provides one -- it is what the delay-loop CLOCKLOOP used,
         * and that version COMPLETED on both lanes. ReadEClock needs the
         * timer's library base and its frequency, both of which come from
         * the OpenDevice handshake below, not from a running timer stream.
         *
         * So there is no SendIO here at all. What is being measured is the
         * PRODUCER's schedule -- the thing this proof exists for -- and the
         * pacing only has to be finer than a tick interval, which Delay(1)
         * is. The one thing this gives up is 20 ms service granularity,
         * which is recorded rather than hidden.
         */
        Delay(1);
        ri_pal_midi_poll();
    }
    (void)tkb;
    /* A cross-check on the sink's own count: what the pump says it carried
     * and what the sink counted must agree, or one of them is lying. */
    if (pumped != s_sent_bytes)
        Printf((CONST_STRPTR)"CLOCKLOOP: pump %lu != sink %lu\n",
            pumped, (ULONG)s_sent_bytes);
    /* Drain whatever is still in flight before closing. */
    for (i = 0UL; i < 50UL; i++) {
        Delay(1);
        ri_pal_midi_poll();
    }
    ri_pal_midi_close();
    if (trq) {
        CloseDevice((struct IORequest *)trq);
        DeleteIORequest((struct IORequest *)trq);
    }
    if (tp)
        DeleteMsgPort(tp);
    lfh = Open((CONST_STRPTR)logpath, MODE_NEWFILE);
    if (!lfh)
        return 22;
    p = buf;
    p = put_kv(p, "sent", (uint32_t)pumped);
    *p++ = '\n';
    p = put_kv(p, "clocks", s_clocks);
    *p++ = '\n';
    p = put_kv(p, "received_f8", s_clocks);
    *p++ = '\n';
    p = put_kv(p, "lost", (uint32_t)(pumped > s_clocks ? pumped - s_clocks : 0u));
    *p++ = '\n';
    p = put_kv(p, "intervals", midi_interval_intervals(&s_ivl));
    *p++ = '\n';
    p = put_kv(p, "min_us", midi_interval_min_us(&s_ivl));
    *p++ = '\n';
    p = put_kv(p, "max_us", midi_interval_max_us(&s_ivl));
    *p++ = '\n';
    p = put_kv(p, "mean_us", midi_interval_mean_us(&s_ivl));
    *p++ = '\n';
    p = put_kv(p, "jitter_us", midi_interval_jitter_us(&s_ivl));
    *p++ = '\n';
    p = put_kv(p, "verdict", midi_interval_verdict(&s_ivl));
    *p++ = '\n';
    p = put_kv(p, "other", s_other);
    *p++ = '\n';
    p = put_kv(p, "badlen", s_badlen);
    *p++ = '\n';
    p = put_kv(p, "backwards", midi_interval_backwards(&s_ivl));
    *p++ = '\n';
    {
        uint32_t k;
        for (k = 0u; k < RI_MIDIINT_SLICES; k++) {
            char key[16];
            char *q = key;
            *q++ = 's'; *q++ = 'l'; *q++ = 'i'; *q++ = 'c'; *q++ = 'e'; *q++ = '_';
            q = put_u32(q, k);
            *q = 0;
            p = put_kv(p, key, midi_interval_slice(&s_ivl, k));
            *p++ = '\n';
        }
    }
    Write(lfh, (APTR)buf, (LONG)(p - buf));
    Close(lfh);
    if (s_prog) { prog("Z done\n"); Close(s_prog); s_prog = 0; }
    Printf((CONST_STRPTR)"CLOCKLOOP done: sent=%lu clocks=%lu lost=%lu "
        "min=%lu max=%lu jitter=%lu verdict=%lu\n",
        pumped, (ULONG)s_clocks,
        (ULONG)(pumped > s_clocks ? pumped - s_clocks : 0u),
        (ULONG)midi_interval_min_us(&s_ivl), (ULONG)midi_interval_max_us(&s_ivl),
        (ULONG)midi_interval_jitter_us(&s_ivl), (ULONG)midi_interval_verdict(&s_ivl));
    return s_clocks ? 0 : 23;
}

int main(int argc, char **argv) {
    struct MidiNode *node;
    struct MidiLink *link;
    if (argc < 3)
        return 5;
    if (argv[2][0] == 'C' && argv[2][1] == 'L') {
        /* MIDISEND <cluster> CLOCKLOOP <secs> <logfile> */
        ULONG secs = 300u;
        uint32_t i;
        if (argc < 5)
            return 5;
        secs = 0u;
        for (i = 0u; argv[3][i] >= '0' && argv[3][i] <= '9'; i++)
            secs = secs * 10u + (ULONG)(argv[3][i] - '0');
        if (!secs)
            secs = 5u;
        s_clocks = 0u; s_other = 0u; s_badlen = 0u;
        s_sent_bytes = 0u;
        {
            uint32_t k = 0u;
            while (argv[1][k] && k + 1u < sizeof s_send_user) {
                s_send_user[k] = argv[1][k];
                k++;
            }
            s_send_user[k] = 0;
        }
        midi_interval_init(&s_ivl, (uint32_t)(secs * 1000000UL));
        return do_clockloop(argv[1], secs, argv[4]);
    }
    if (argv[2][0] == 'L' && argv[2][1] == 'I') {
        /* MIDISEND <cluster> LISTEN <secs> <logfile>
         *
         * The clock-out proof (R2). This lives here, on the proven M2
         * receiver, rather than in a tool of its own: that receiver task
         * already waits on the CAMD signal and stamps each message with
         * EClock the moment it wakes, which is the one thing the retired
         * MIDIRX could not do -- it read its own clock at the top of a poll
         * loop and so measured how often it LOOKED rather than how often
         * bytes CAME. At 140 BPM, where ticks are ~17.9 ms apart, that
         * quantisation would have been most of the reported spread.
         *
         * Only F8 is counted. Other traffic is counted too and reported, so
         * a silent or busy cluster is distinguishable from a steady one.
         */
        ULONG secs = 300, ticks;
        BPTR lfh;
        uint32_t i;
        /* argv: 0 MIDISEND, 1 cluster, 2 LISTEN, 3 secs, 4 logfile. That
         * is FIVE arguments, and the first version of this mode required
         * six and read argv[5] -- so it returned 5 immediately, silently,
         * and the proof run looked like a tool that produced no output
         * rather than a tool that never started. */
        if (argc < 5)
            return 5;
        secs = 0;
        for (i = 0u; argv[3][i] >= '0' && argv[3][i] <= '9'; i++)
            secs = secs * 10 + (ULONG)(argv[3][i] - '0');
        if (!secs)
            secs = 300u;
        midi_interval_init(&s_ivl, (uint32_t)(secs * 1000000UL));
        if (ri_pal_midi_open_in(argv[1], on_midi, NULL) != 0)
            return 21;
        Printf((CONST_STRPTR)"LISTEN: %lu s on %s\n", secs, (IPTR)argv[1]);
        /* Wait on the receiver, not on a poll of our own: the stamps come
         * from the task's wake, so our loop cadence cannot enter the
         * measurement. It only decides how often the batch is drained. */
        for (ticks = 0UL; ticks < secs * 50UL; ticks++) {
            Delay(20);                       /* 50 Hz drain */
            ri_pal_midi_poll();
            if (s_clocks >= 100000u)
                break;
        }
        ri_pal_midi_close();
        lfh = Open((CONST_STRPTR)argv[4], MODE_NEWFILE);
        if (!lfh)
            return 22;
        {
            /* key=value lines: greppable on the lane without a parser, and
             * the slices go in individually so drift is visible per tenth
             * rather than only as one verdict. */
            static char buf[2048];
            char *p = buf;
            p = put_kv(p, "clocks", s_clocks);
            *p++ = '\n';
            p = put_kv(p, "intervals", midi_interval_intervals(&s_ivl));
            *p++ = '\n';
            p = put_kv(p, "min_us", midi_interval_min_us(&s_ivl));
            *p++ = '\n';
            p = put_kv(p, "max_us", midi_interval_max_us(&s_ivl));
            *p++ = '\n';
            p = put_kv(p, "mean_us", midi_interval_mean_us(&s_ivl));
            *p++ = '\n';
            p = put_kv(p, "jitter_us", midi_interval_jitter_us(&s_ivl));
            *p++ = '\n';
            p = put_kv(p, "verdict", midi_interval_verdict(&s_ivl));
            *p++ = '\n';
            p = put_kv(p, "other", s_other);
            *p++ = '\n';
            p = put_kv(p, "badlen", s_badlen);
            *p++ = '\n';
            p = put_kv(p, "backwards", midi_interval_backwards(&s_ivl));
            *p++ = '\n';
            p = put_kv(p, "secs", (uint32_t)secs);
            *p++ = '\n';
            for (i = 0u; i < RI_MIDIINT_SLICES; i++) {
                char k[16];
                char *q = k;
                *q++ = 's'; *q++ = 'l'; *q++ = 'i'; *q++ = 'c'; *q++ = 'e'; *q++ = '_';
                q = put_u32(q, i);
                *q = 0;
                p = put_kv(p, k, midi_interval_slice(&s_ivl, i));
                *p++ = '\n';
            }
            Write(lfh, (APTR)buf, (LONG)(p - buf));
        }
        Close(lfh);
        /* The human-readable summary goes to the console too: on riqemu1 the
         * console is legible, so the run can be judged without a pull. */
        Printf((CONST_STRPTR)"LISTEN done: clocks=%lu intervals=%lu min=%lu max=%lu jitter=%lu verdict=%lu\n",
            (ULONG)s_clocks, (ULONG)midi_interval_intervals(&s_ivl),
            (ULONG)midi_interval_min_us(&s_ivl), (ULONG)midi_interval_max_us(&s_ivl),
            (ULONG)midi_interval_jitter_us(&s_ivl), (ULONG)midi_interval_verdict(&s_ivl));
        Printf((CONST_STRPTR)"LISTEN other=%lu badlen=%lu backwards=%lu\n",
            (ULONG)s_other, (ULONG)s_badlen, (ULONG)midi_interval_backwards(&s_ivl));
        for (i = 0u; i < RI_MIDIINT_SLICES; i++)
            Printf((CONST_STRPTR)"LISTEN slice %lu = %lu\n", (ULONG)i,
                (ULONG)midi_interval_slice(&s_ivl, i));
        return s_clocks ? 0 : 23;
    }
    if (argv[2][0] != 'S') {
        /* Script playback: PAL send only, no CAMD objects here (T5). */
        BPTR sfh;
        char sline[128];
        LONG ssent = 0;
        sfh = Open((CONST_STRPTR)argv[2], MODE_OLDFILE);
        if (!sfh)
            return 12;
        while (FGets(sfh, (STRPTR)sline, sizeof sline)) {
            uint8_t msg[3] = { 0, 0, 0 };
            uint32_t nb = 0;
            char *p = sline;
            if (sline[0] == '#' || sline[0] == '\n')
                continue;
            if (sline[0] == 'D') {              /* D <ms> */
                LONG ms = 0;
                for (p = sline + 1; *p == ' '; p++)
                    ;
                while (*p >= '0' && *p <= '9')
                    ms = ms * 10 + (*p++ - '0');
                Delay(ms / 20 > 0 ? ms / 20 : 1);
                continue;
            }
            while (*p && nb < 3) {
                int h, l;
                while (*p == ' ')
                    p++;
                h = hexv(p[0]);
                l = h >= 0 ? hexv(p[1]) : -1;
                if (h < 0 || l < 0)
                    break;
                msg[nb++] = (uint8_t)(h * 16 + l);
                p += 2;
            }
            if (nb && ri_pal_midi_send(argv[1], msg, nb) == 0)
                ssent++;
        }
        Close(sfh);
        Printf((CONST_STRPTR)"MIDISEND: %ld messages to %s\n", ssent, (IPTR)argv[1]);
        ri_pal_midi_close();
        return 0;
    }
    /* SELFTEST stays on raw CAMD: it diagnoses the stack itself (T5). */
    CamdBase = OpenLibrary((CONST_STRPTR)"camd.library", 0);
    if (!CamdBase)
        return 10;
    node = CreateMidi(MIDI_Name, (IPTR)"MIDISEND", TAG_END);
    link = node ? AddMidiLink(node, MLTYPE_Sender, MLINK_Location, (IPTR)argv[1], TAG_END) : NULL;
    if (!link) {
        if (node)
            DeleteMidi(node);
        CloseLibrary(CamdBase);
        return 11;
    }
    if (argv[2][0] == 'S') {                   /* SELFTEST */
        BYTE sig = AllocSignal(-1);
        struct MidiNode *rn = CreateMidi(MIDI_Name, (IPTR)"MIDISEND-RX", MIDI_RecvSignal, (IPTR)sig,
            MIDI_MsgQueue, 64, TAG_END);
        struct MidiLink *rl = rn ? AddMidiLink(rn, MLTYPE_Receiver, MLINK_Location, (IPTR)argv[1], TAG_END) : NULL;
        MidiMsg mm;
        LONG got = 0;
        ULONG sigs;
        struct MidiNode *sn2 = CreateMidi(MIDI_Name, (IPTR)"MIDISEND-TX2", TAG_END);
        struct MidiLink *sl2 = sn2 ? AddMidiLink(sn2, MLTYPE_Sender, MLINK_Location, (IPTR)argv[1], TAG_END) : NULL;
        Printf((CONST_STRPTR)"SELFTEST connected: early sender %ld, late sender %ld\n",
            (LONG)MidiLinkConnected(link), sl2 ? (LONG)MidiLinkConnected(sl2) : -1L);
        PutMidi(link, (0x90UL << 24) | (0x45UL << 16) | (0x64UL << 8));
        if (sl2)
            PutMidi(sl2, (0xB0UL << 24) | (0x19UL << 16) | (0x40UL << 8));
        sigs = SetSignal(0, 0);
        Printf((CONST_STRPTR)"SELFTEST rx err %lx\n", rn ? (ULONG)GetMidiErr(rn) : 0UL);
        {
            struct MidiCluster *c = NULL;
            LONG nc = 0;
            APTR lock = LockCAMD(CD_Linkages);
            while ((c = NextCluster(c)) != NULL && nc < 12) {
                const UBYTE *nm = (const UBYTE *)c->mcl_Node.ln_Name;
                Printf((CONST_STRPTR)"SELFTEST cluster %ld at %lx name %02lx %02lx %02lx %02lx\n", nc++, (IPTR)c,
                    nm ? (ULONG)nm[0] : 0UL, nm ? (ULONG)nm[1] : 0UL, nm ? (ULONG)nm[2] : 0UL, nm ? (ULONG)nm[3] : 0UL);
            }
            UnlockCAMD(lock);
        }
        while (rn && GetMidi(rn, &mm)) {
            got++;
            Printf((CONST_STRPTR)"SELFTEST got st %lx d1 %lx d2 %lx raw %08lx\n", (ULONG)mm.mm_Status,
                (ULONG)mm.mm_Data1, (ULONG)mm.mm_Data2, mm.mm_Msg);
        }
        Printf((CONST_STRPTR)"SELFTEST rx node %lx link %lx sig %ld signalled %ld got %ld\n", (IPTR)rn, (IPTR)rl,
            (LONG)sig, (LONG)(sig >= 0 && (sigs & (1UL << sig)) != 0), got);
        if (rn)
            DeleteMidi(rn);
        if (sn2)
            DeleteMidi(sn2);
        if (sig >= 0)
            FreeSignal(sig);
        DeleteMidi(node);
        CloseLibrary(CamdBase);
        return got ? 0 : 20;
    }
    return 5; /* unreachable: non-SELFTEST returns above */
}
