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
