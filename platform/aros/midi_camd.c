/* midi_camd.c — AROS backend for ri_pal_midi (portability plan T5).
 * AROS-only. Wraps camd.library with a dedicated bridge task (M2): the
 * task owns the CAMD receiver node, waits on its signal, stamps every
 * message on arrival with the EClock (microseconds, bridge side only —
 * CAMD mm_Time is the sender's stamp, not an arrival time), and feeds
 * the portable router (midi_bridge.h: realtime/system common to the
 * owned follower, everything to the GUI message queue). The app thread
 * drains the queues; CAMD never runs on the GUI thread and the GUI
 * thread never blocks on the bridge (kill handshake always watched).
 * No MIDI_RecvHook (BarsnPipes register prototype, wrong on x86-64);
 * signal + queue. Event mask stays default (CMD_All excludes realtime).
 */
#ifndef __AROS__
#error "midi_camd.c is AROS-only (portability plan T5)"
#endif

#include "platform/pal/ri_pal_midi.h"
#include "midi_io/midi_out.h"
#include "platform/pal/ri_pal_thread.h"
#include "midi_io/midi_bridge.h"

#include <exec/types.h>
#include <exec/tasks.h>
#include <exec/io.h>
#include <devices/timer.h>
#include <dos/dostags.h>
#include <midi/camd.h>
#include <proto/exec.h>
#include <proto/dos.h>
/* The bridge task owns its timer.device base (s_timer_base); route the
 * inline timer calls there instead of the TU-global TimerBase, which
 * other TUs (audio_ahi_live.c) already define. */
#define __TIMER_LIBBASE s_timer_base
#include <proto/timer.h>
#include <proto/camd.h>

#include <string.h>

#define BRIDGE_STACK 16384u
#define OPEN_TIMEOUT_S 2u

struct Library *CamdBase; /* camd inline base (one definition per link) */

/* GUI-side state. */
static struct RIMidiBridge s_bridge;
static int s_have_bridge;
static ri_midi_in s_cb;
static void *s_user;
static char s_cluster[64];
static BYTE s_parent_sig = -1;
static struct Task *s_parent;
static struct Task *s_task;
static ri_atomic_u32 s_task_done;
/* Sender side (synchronous, caller task — unchanged G7 path). */
static struct MidiNode *s_tx;
static struct MidiLink *s_txlink;

/* Bridge-task side (only touched by the task after spawn). */
static struct MsgPort *s_tport;
static struct timerequest *s_treq;
static struct Device *s_timer_base;
static ULONG s_efreq;
static struct MidiNode *s_rx;
static struct MidiLink *s_rxlink;
static BYTE s_camd_sig = -1; /* -1-init: 0 is a valid signal number */
static BYTE s_kill_sig = -1;
static ri_atomic_u32 s_stop;

static int lazy_base(void) {
    if (!CamdBase)
        CamdBase = OpenLibrary("camd.library", 0);
    return CamdBase ? 1 : 0;
}

/* Resolve the library without opening a port. camd's CreateMidi and
 * AddMidiLink are inline calls through CamdBase, so a tool that talks
 * CAMD directly must resolve it first -- a NULL base is an illegal memory
 * access, not a NULL return. */
int ri_pal_midi_init_lib(void) {
    return lazy_base() ? 0 : 1;
}

static uint64_t eclock_us(void) {
    struct EClockVal ev;
    uint64_t ticks;
    if (!s_timer_base || !s_efreq)
        return 0u;
    ReadEClock(&ev);
    ticks = ((uint64_t)ev.ev_hi << 32) | (uint64_t)ev.ev_lo;
    return ticks / s_efreq * 1000000u + (ticks % s_efreq) * 1000000u / s_efreq;
}

static void bridge_task(void) {
    ULONG camd_mask, kill_mask;
    s_tport = CreateMsgPort();
    if (s_tport)
        s_treq = (struct timerequest *)CreateIORequest(s_tport, sizeof(struct timerequest));
    if (s_treq && OpenDevice((STRPTR)"timer.device", UNIT_ECLOCK,
            (struct IORequest *)s_treq, 0) == 0) {
        struct EClockVal t0;
        s_timer_base = s_treq->tr_node.io_Device;
        s_efreq = ReadEClock(&t0);
    }
    s_camd_sig = AllocSignal(-1);
    s_kill_sig = AllocSignal(-1);
    if (s_camd_sig >= 0 && s_kill_sig >= 0) {
        s_rx = CreateMidi(MIDI_Name, (IPTR)"RIBRIDGE",
            MIDI_RecvSignal, (IPTR)s_camd_sig, MIDI_MsgQueue, 512, TAG_END);
        if (s_rx)
            s_rxlink = AddMidiLink(s_rx, MLTYPE_Receiver, MLINK_Location,
                (IPTR)s_cluster, TAG_END);
    }
    Signal(s_parent, 1UL << s_parent_sig); /* ready or failed: open decides */
    if (!s_rx || !s_rxlink)
        goto out;
    camd_mask = 1UL << s_camd_sig;
    kill_mask = 1UL << s_kill_sig;
    for (;;) {
        ULONG sigs = Wait(camd_mask | kill_mask);
        MidiMsg mm;
        if ((sigs & kill_mask) || ri_atomic_load_acq(&s_stop))
            break;
        while (GetMidi(s_rx, &mm)) {
            uint64_t now = eclock_us();
            midi_bridge_feed(&s_bridge, mm.mm_Status, mm.mm_Data1,
                mm.mm_Data2, now);
        }
        /* CAMD drops whole messages on a full queue (mididistr.c);
         * count them here, oldest-first in the router stays separate. */
        if (GetMidiErr(s_rx) & CMEF_BufferFull)
            midi_bridge_note_camd_drop(&s_bridge);
        /* Hot-unplug (TC-2.8.4): a lost cluster flags status and the
         * task keeps waiting; the app never hangs on us. */
        if (s_rxlink && !MidiLinkConnected(s_rxlink))
            midi_bridge_note_link(&s_bridge, 1u);
    }
out:
    if (s_rx) {
        DeleteMidi(s_rx);
        s_rx = 0;
        s_rxlink = 0;
    }
    if (s_camd_sig >= 0) {
        FreeSignal(s_camd_sig);
        s_camd_sig = -1;
    }
    if (s_kill_sig >= 0) {
        FreeSignal(s_kill_sig);
        s_kill_sig = -1;
    }
    if (s_timer_base) {
        CloseDevice((struct IORequest *)s_treq);
        s_timer_base = 0;
        s_efreq = 0;
    }
    if (s_treq) {
        DeleteIORequest((struct IORequest *)s_treq);
        s_treq = 0;
    }
    if (s_tport) {
        DeleteMsgPort(s_tport);
        s_tport = 0;
    }
    ri_atomic_store_rel(&s_task_done, 1u);
}

int ri_pal_midi_open_in(const char *port, ri_midi_in cb, void *user) {
    uint32_t i;
    struct MsgPort *tport;
    struct timerequest *treq;
    struct Task *task;
    if (!port || !port[0])
        return 1;
    if (!lazy_base())
        return 1;
    ri_pal_midi_close();
    for (i = 0u; i < sizeof s_cluster - 1u && port[i]; i++)
        s_cluster[i] = port[i];
    s_cluster[i] = 0;
    s_cb = cb;
    s_user = user;
    midi_bridge_init(&s_bridge);
    s_have_bridge = 1;
    ri_atomic_store_rel(&s_stop, 0u);
    ri_atomic_store_rel(&s_task_done, 0u);
    s_parent = FindTask(NULL);
    s_parent_sig = AllocSignal(-1);
    if (s_parent_sig < 0)
        return 1;
    task = (struct Task *)CreateNewProcTags(NP_Entry, (IPTR)bridge_task,
        NP_Name, (IPTR)"RIAPP midi", NP_StackSize, BRIDGE_STACK, TAG_DONE);
    if (!task) {
        FreeSignal(s_parent_sig);
        s_parent_sig = -1;
        return 1;
    }
    s_task = task;
    /* Bounded handshake: the task signals ready (or failed) promptly;
     * a timer bounds the wait so open fails closed, never wedged. No
     * timer device means no MIDI timing at all: fail instead. */
    tport = CreateMsgPort();
    treq = tport ? (struct timerequest *)CreateIORequest(tport,
        sizeof(struct timerequest)) : 0;
    if (!treq || OpenDevice((STRPTR)"timer.device", UNIT_MICROHZ,
            (struct IORequest *)treq, 0) != 0) {
        if (treq)
            DeleteIORequest((struct IORequest *)treq);
        if (tport)
            DeleteMsgPort(tport);
        ri_pal_midi_close();
        return 1;
    }
    treq->tr_node.io_Command = TR_ADDREQUEST;
    treq->tr_time.tv_secs = OPEN_TIMEOUT_S;
    treq->tr_time.tv_micro = 0;
    SendIO((struct IORequest *)treq);
    Wait((1UL << s_parent_sig) | (1UL << tport->mp_SigBit));
    if (treq) {
        int timed_out = GetMsg(tport) != 0;
        if (!CheckIO((struct IORequest *)treq))
            AbortIO((struct IORequest *)treq);
        WaitIO((struct IORequest *)treq);
        CloseDevice((struct IORequest *)treq);
        DeleteIORequest((struct IORequest *)treq);
        if (timed_out) {
            DeleteMsgPort(tport);
            ri_pal_midi_close();
            return 1;
        }
    }
    DeleteMsgPort(tport);
    if (!s_rx || !s_rxlink) {
        ri_pal_midi_close();
        return 1;
    }
    return 0;
}

/* Compatibility drain into the open callback (RISECT-era path): the
 * bridge task filled the queues; deliver with arrival stamps. */
void ri_pal_midi_poll(void) {
    struct RIMidiMsg msgs[32];
    uint32_t n, i;
    if (!s_have_bridge || !s_cb)
        return;
    n = midi_bridge_read_ch(&s_bridge, msgs, 32u);
    for (i = 0u; i < n; i++)
        s_cb(s_user, msgs[i].b, msgs[i].n, msgs[i].t_us);
}

struct RIMidiBridge *ri_pal_midi_bridge(void) {
    return s_have_bridge ? &s_bridge : 0;
}

int ri_pal_midi_send(const char *port, const uint8_t *msg, uint32_t len) {
    ULONG pkt;
    const char *dst;
    if (!msg || len == 0u || len > 3u)
        return 1;
    if (!lazy_base())
        return 1;
    dst = (port && port[0]) ? port : s_cluster;
    if (!dst[0])
        return 1;
    if (!s_tx || strcmp(dst, s_cluster) != 0) {
        if (s_txlink) {
            DeleteMidi(s_tx);
            s_tx = 0;
            s_txlink = 0;
        }
        s_tx = CreateMidi(MIDI_Name, (IPTR)"RIPAL-TX", TAG_END);
        if (!s_tx)
            return 1;
        s_txlink = AddMidiLink(s_tx, MLTYPE_Sender, MLINK_Location, (IPTR)dst, TAG_END);
        if (!s_txlink) {
            DeleteMidi(s_tx);
            s_tx = 0;
            return 1;
        }
    }
    pkt = ((ULONG)msg[0] << 24) | ((ULONG)(len > 1u ? msg[1] : 0u) << 16) |
        ((ULONG)(len > 2u ? msg[2] : 0u) << 8);
    PutMidi(s_txlink, pkt);
    return 0;
}

void ri_pal_midi_close(void) {
    if (s_task && !ri_atomic_load_acq(&s_task_done)) {
        uint32_t spins = 0u;
        ri_atomic_store_rel(&s_stop, 1u);
        if (s_kill_sig >= 0)
            Signal(s_task, 1UL << s_kill_sig);
        /* Task exit is prompt (nothing blocks past the wait); bounded
         * Delay loop so close cannot wedge the caller either. */
        while (!ri_atomic_load_acq(&s_task_done) && spins < 100u) {
            Delay(1);
            spins++;
        }
    }
    if (s_tx) {
        DeleteMidi(s_tx);
        s_tx = 0;
        s_txlink = 0;
    }
    s_task = 0;
    if (s_parent_sig >= 0) {
        FreeSignal(s_parent_sig);
        s_parent_sig = -1;
    }
    s_cb = 0;
    s_user = 0;
    s_cluster[0] = 0;
    s_have_bridge = 0;
}

/* =====================================================================
 * M5e2: the clock-out SENDER task.
 *
 * The render fills a ring and stops there (app/core/live_driver.c has no
 * camd in it and never will). These bytes still have to reach camd from
 * somewhere that is not the render, and "somewhere" matters: draining on
 * the app's event loop bunches the clock into whatever rhythm the UI is
 * running at, which is exactly the jitter the follower law on the other
 * end of the wire rejects.
 *
 * So this is a task of its own with a 1 ms tick. That is not a real-time
 * guarantee and does not need to be: the SCHEDULE is exact (t185: the
 * tick count is a pure function of the audio sample clock), so all this
 * task does is carry bytes across in small batches. The lane proof
 * measures the intervals it actually produces.
 * ===================================================================== */

static struct RIMidiOut *s_out;          /* BORROWED: render writes it */
static struct Task *s_stask;
static struct MsgPort *s_sport;
static struct timerequest *s_streq;
static struct IORequest *s_stimer;
static LONG s_skill_sig = -1;
static LONG s_sparent_sig = -1;
static struct Task *s_sparent;
static ri_atomic_u32 s_sstop, s_stask_done, s_sready;
static volatile uint32_t s_sent, s_send_err;

static int send_sink(const uint8_t *msg, uint32_t len, void *user) {
    (void)user;
    if (ri_pal_midi_send(0, msg, len) != 0) {
        s_send_err++;
        return 1;              /* stop this pump; retry next tick */
    }
    s_sent++;
    return 0;
}

static void sender_task(void) {
    struct Task *parent = s_sparent;
    LONG psig = s_sparent_sig;
    s_sport = CreateMsgPort();
    if (s_sport)
        s_streq = (struct timerequest *)CreateIORequest(s_sport,
            sizeof(struct timerequest));
    if (s_streq && OpenDevice((STRPTR)"timer.device", UNIT_MICROHZ,
            (struct IORequest *)s_streq, 0) != 0) {
        if (s_streq) {
            DeleteIORequest(s_streq);
            s_streq = 0;
        }
    }
    if (s_streq)
        s_stimer = (struct IORequest *)s_streq;
    s_skill_sig = AllocSignal(-1);
    ri_atomic_store_rel(&s_sready, 1u);   /* timer and signal are usable */
    Signal(parent, 1UL << psig);          /* ready, for anyone who waits */
    while (s_streq && s_skill_sig >= 0) {
        struct timerequest *tr;
        ULONG sigs;
        if (ri_atomic_load_acq(&s_sstop))
            break;
        /* Send everything the render produced, capped so a burst cannot
         * hold this task: the ring holds at most CAP-1 bytes anyway. */
        if (s_out)
            (void)midi_out_pump(s_out, send_sink, 0, 64u);
        /* A 1 ms wait between drains. NOT a real-time guarantee and it
         * does not need to be: the schedule is exact (t185 derives the
         * tick count from the audio sample clock), so all this task does
         * is carry bytes across in small batches. The lane proof measures
         * the intervals it actually produces.
         *
         * AROS's timer request here is an AllocMem'd timerequest with
         * tr_node/tr_time -- there is no AllocTimReq and SendIO takes one
         * argument in this SDK. */
        tr = (struct timerequest *)CreateIORequest(s_sport,
            sizeof(struct timerequest));
        if (!tr)
            break;
        tr->tr_node.io_Command = TR_ADDREQUEST;   /* as the open handshake */
        tr->tr_time.tv_secs = 0;
        tr->tr_time.tv_micro = 1000;
        SendIO((struct IORequest *)tr);
        WaitPort(s_sport);
        sigs = Wait(1UL << s_skill_sig);
        (void)GetMsg(s_sport);   /* the timer reply; freed with the request */
        if (!CheckIO((struct IORequest *)tr))
            AbortIO((struct IORequest *)tr);
        WaitIO((struct IORequest *)tr);
        DeleteIORequest((struct IORequest *)tr);
        if (sigs & (1UL << s_skill_sig))
            break;
    }
    if (s_stimer) {
        CloseDevice(s_stimer);
        s_stimer = 0;
        s_streq = 0;
    }
    if (s_sport) {
        DeleteMsgPort(s_sport);
        s_sport = 0;
    }
    if (s_skill_sig >= 0) {
        FreeSignal(s_skill_sig);
        s_skill_sig = -1;
    }
    ri_atomic_store_rel(&s_stask_done, 1u);
}

int ri_pal_midi_send_start(const char *port, struct RIMidiOut *o) {
    uint32_t spins = 0u;
    (void)port;               /* ri_pal_midi_send() uses the open cluster */
    if (!o || s_stask)
        return 1;
    s_out = o;
    s_sent = 0u;
    s_send_err = 0u;
    ri_atomic_store_rel(&s_sstop, 0u);
    ri_atomic_store_rel(&s_stask_done, 0u);
    ri_atomic_store_rel(&s_sready, 0u);
    s_sparent_sig = AllocSignal(-1);
    if (s_sparent_sig < 0)
        return 1;
    s_sparent = FindTask(NULL);
    s_stask = (struct Task *)CreateNewProcTags(NP_Entry,
        (IPTR)sender_task, NP_Name, (IPTR)"RIAPP midi-out",
        NP_StackSize, BRIDGE_STACK, TAG_DONE);
    if (!s_stask) {
        FreeSignal(s_sparent_sig);
        s_sparent_sig = -1;
        return 1;
    }
    /* Bounded handshake, in the same shape as the bridge task's: the task
     * reports ready (or failed) and a spin bound stops startup from ever
     * wedging on it. A task that cannot get a timer device never signals,
     * so this times out and we carry on with clock out OFF rather than
     * blocking. Failing closed here is the whole point of the E0. */
    while (!ri_atomic_load_acq(&s_sready) && spins < 200u) {
        Delay(1);
        spins++;
    }
    if (!ri_atomic_load_acq(&s_sready)) {
        ri_pal_midi_send_stop();
        FreeSignal(s_sparent_sig);
        s_sparent_sig = -1;
        return 1;
    }
    return 0;
}

void ri_pal_midi_send_stop(void) {
    uint32_t spins = 0u;
    if (!s_stask)
        return;
    ri_atomic_store_rel(&s_sstop, 1u);
    if (s_stask && s_skill_sig >= 0)
        Signal(s_stask, 1UL << s_skill_sig);
    while (!ri_atomic_load_acq(&s_stask_done) && spins < 100u) {
        Delay(1);
        spins++;
    }
    s_stask = 0;
    s_out = 0;
    if (s_sparent_sig >= 0) {
        FreeSignal(s_sparent_sig);
        s_sparent_sig = -1;
    }
}

uint32_t ri_pal_midi_sent(void) { return s_sent; }
uint32_t ri_pal_midi_send_errors(void) { return s_send_err; }
