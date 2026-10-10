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
#include <dos/dos.h>
#include <midi/camd.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/camd.h>
#include "platform/pal/ri_pal_midi.h"
#include "midi_io/midi_interval.h"

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

int main(int argc, char **argv) {
    struct MidiNode *node;
    struct MidiLink *link;
    if (argc < 3)
        return 5;
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
        if (argc < 6)
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
        lfh = Open((CONST_STRPTR)argv[5], MODE_NEWFILE);
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
