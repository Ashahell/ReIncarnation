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
/* WHICH CLUSTER THE LIVE LINK WAS ACTUALLY BUILT FOR (2026-10-12).
 *
 * Not `s_cluster`: MIDISEND's script playback sends without ever calling
 * `ri_pal_midi_open_in`, so `s_cluster` is empty there and cannot say what
 * an existing link points at. This is written only when a link is built, so
 * it describes the link and nothing else. */
static char s_txcluster[64];

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

static struct RIMidiOut *s_out;          /* BORROWED: render writes it */
/* THE SENDER'S OWN DESTINATION (R12). Empty means the open cluster, which
 * is what a backend without a separate out setting has always done. */
static char s_sendcluster[64];
static struct Task *s_stask;
static struct MsgPort *s_sport;
static struct timerequest *s_streq;
static struct IORequest *s_stimer;
static LONG s_skill_sig = -1;
static LONG s_sparent_sig = -1;
static struct Task *s_sparent;
static ri_atomic_u32 s_sstop, s_stask_done, s_sready;
static volatile uint32_t s_sent, s_send_err, s_clk_sent, s_noteon_sent;
/* R12 O2 diagnostic: the exact signature of the duplicated accent note-on
 * (303 channel 0, note 36, accent velocity 112). One counter, so the run can
 * say whether the SECOND copy ever left CAMD -- the split between "RIAPP
 * produced it" and "it appeared below CAMD". Remove with the finding. */
static volatile uint32_t s_n36a_sent;

/* THE LINK IS SHARED BY TWO TASKS, AND NOW IT SAYS SO.
 *
 * `ri_pal_midi_send` is called from the SENDER task below (clock out and dev
 * out, every 1 ms) AND from the app's main task -- `app/riapp.c` sends MMC
 * from the one place the transport changes. Both went through `s_tx` /
 * `s_txlink` with nothing between them, so a main-task send racing a
 * sender-task send could `PutMidi` on a link the other was inside.
 *
 * That is the same defect as the teardown bug below and it is why "pressing
 * buttons" was in the report. It is NOT what fired on the Dell -- MMC out was
 * off in that run (`MMC out=0`) -- so it is a latent race, not the crash.
 *
 * A TURNSTILE BUILT ON THE PAL's OWN ATOMIC EXCHANGE. `ri_atomic_xchg_rel`
 * returns the previous value, so 0 means the caller took the lock. Bounded by
 * a spin count and then **FAILS CLOSED**: a caller that cannot get the link
 * returns an error rather than touching it. Failing closed is the whole point
 * -- a dropped MIDI message is recoverable and a crash inside camd is not.
 *
 * (`ri_atomic_fetch_add_rel` is the wrong primitive and says so: it returns
 * void. I reached for it anyway and the AROS link caught it.) */
static ri_atomic_u32 s_tx_busy;

#define RI_TX_SPIN_MAX 2000u

static int tx_claim(void) {
    uint32_t spins = 0u;
    for (;;) {
        if (ri_atomic_xchg_rel(&s_tx_busy, 1u) == 0u)
            return 1;                   /* we hold it */
        if (++spins >= RI_TX_SPIN_MAX)
            return 0;                   /* busy: fail closed */
        Delay(1);
    }
}

static void tx_release(void) {
    ri_atomic_store_rel(&s_tx_busy, 0u);
}

/* A SysEx handed to camd's PutSysEx must START with F0, END with F7 and
 * carry 7-bit data between them: PutSysEx walks the buffer until it sees a
 * byte >= 0x80, so a frame that does not satisfy that contract makes camd
 * read past the caller's buffer. Validate before the link, not after it. */
static int sysex_ok(const uint8_t *msg, uint32_t len) {
    uint32_t i;
    if (msg[0] != 0xF0u || msg[len - 1u] != 0xF7u)
        return 0;
    for (i = 1u; i + 1u < len; i++)
        if (msg[i] >= 0x80u)
            return 0;
    return 1;
}

int ri_pal_midi_send(const char *port, const uint8_t *msg, uint32_t len) {
    ULONG pkt = 0;
    const char *dst;
    if (!msg || len == 0u)
        return 1;
    if (len > 3u && !sysex_ok(msg, len))
        return 1;
    if (!lazy_base())
        return 1;
    dst = (port && port[0]) ? port : s_cluster;
    if (!dst[0])
        return 1;
    /* A SENDER'S PORT NEVER REPLACES A LIVE LINK (2026-10-12).
     *
     * THIS IS THE HALF OF THE CRASH THE TURNSTILE DID NOT CLOSE. The teardown
     * was fixed first -- close now joins the sender before it frees the link
     * -- but `ri_pal_midi_send` freed the link too: whenever a caller passed
     * a port different from the cluster the link was built for, it ran
     * `DeleteMidi(s_tx)` and rebuilt, and it did that BEFORE `tx_claim()`.
     * So the lock guarded `PutMidi` and nothing else, while the thing that
     * frees the request `PutMidi` is about to touch ran unlocked in the same
     * function.
     *
     * `app/riapp.c` is that caller: sync_transport() sends MMC from the MAIN
     * task and passes `s_mset.cluster` explicitly. Against a sender task
     * pumping the same link every 1 ms that is DeleteMidi under a live
     * PutMidi -- the same `exec_777_sendio` report, reached by a second route.
     *
     * So a mismatch REFUSES instead of swapping. Refusing is not the cautious
     * choice bolted on afterwards; a link's destination is a lifetime
     * property, and changing it means tearing the link down, which is
     * `ri_pal_midi_close`'s job because close is what joins the sender first.
     * A send is not allowed to free anything at all.
     *
     * (A blanket "refuse any port that is not s_cluster" would have been
     * wrong and would have failed silently: MIDISEND's script playback --
     * the lane proof's own transport -- sends without ever opening, so
     * `s_cluster` is empty there and every message would have been dropped.) */
    if (s_txcluster[0] && strcmp(dst, s_txcluster) != 0)
        return 1;
    if (len <= 3u)
        pkt = ((ULONG)msg[0] << 24) | ((ULONG)(len > 1u ? msg[1] : 0u) << 16) |
            ((ULONG)(len > 2u ? msg[2] : 0u) << 8);
    /* ONE TASK AT A TIME ON THE LINK, AND THE LINK IS BUILT UNDER IT TOO.
     * The claim goes as late as possible -- after every check that can refuse
     * -- so the contended window is only the create/link/put sequence. Taking
     * it BEFORE the lazy create is what makes the create exclusive: unlocked,
     * two tasks can both find `!s_tx` and both build a link, and the loser
     * orphans the winner's Midi. */
    if (!tx_claim())
        return 1;
    {
        int rc = 0;
        if (!s_tx) {
            s_tx = CreateMidi(MIDI_Name, (IPTR)"RIPAL-TX", TAG_END);
            if (s_tx) {
                s_txlink = AddMidiLink(s_tx, MLTYPE_Sender, MLINK_Location,
                    (IPTR)dst, TAG_END);
                if (s_txlink) {
                    uint32_t i;
                    for (i = 0u; i < sizeof s_txcluster - 1u && dst[i]; i++)
                        s_txcluster[i] = dst[i];
                    s_txcluster[i] = 0;
                }
            }
            /* NOTHING IS FREED HERE ON FAILURE, AND THAT IS DELIBERATE.
             * `s_tx` stays set with no link, so the next send retries
             * AddMidiLink, and close frees the Midi under the join. The old
             * code called DeleteMidi on this path -- correct in isolation and
             * wrong in a function that must not free anything, because the
             * only way to tell "just created, never published" from "live"
             * from outside is a guarantee this function cannot make. */
            if (!s_txlink)
                rc = 1;
        }
        if (rc == 0 && s_txlink) {
            /* MMC (and any other SysEx) is LONGER THAN A SHORT MESSAGE, so
             * PutMidi cannot carry it and the old `len > 3` refusal is why
             * every MMC send was refused on AROS. PutSysEx is the same
             * link's SysEx call: one link, one destination, same turnstile,
             * so the one-link rule the short path enforces still holds. */
            if (len > 3u)
                PutSysEx(s_txlink, (UBYTE *)msg);   /* read-only below */
            else
                PutMidi(s_txlink, pkt);
        } else
            rc = 1;
        tx_release();
        return rc;
    }
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
    /* JOIN THE SENDER TASK BEFORE THE LINK IS FREED (2026-10-12).
     *
     * THIS IS THE DELL CRASH. `ri_pal_midi_close()` stopped the BRIDGE task
     * and then deleted the camd link while `RIAPP midi-out` -- the clock-out
     * sender, named at CreateNewProcTags below -- was still running its 1 ms
     * loop, still calling `ri_pal_midi_send`, still reading `s_txlink` and
     * still calling `PutMidi` on it. Deleting the link under a task that is
     * inside it hands camd a freed IORequest, and the report that came back
     * was:
     *
     *     RIAPP midi out   Error: illegal address access
     *                     exec_777_sendio
     *
     * -- the task's own name and the sendio it was in. The header has said
     * since M5e2 that "`ri_pal_midi_send_stop()` joins the task"; the only
     * caller was the `send_start` failure path, so the documented teardown
     * was never performed.
     *
     * Order matters and is now the only order: stop and JOIN the sender, and
     * only then free what the sender was using. */
    ri_pal_midi_send_stop();
    /* Under the same claim a sender would take, so the delete cannot land
     * between a sender's claim and its `PutMidi`. If the claim cannot be
     * taken the link is LEFT ALONE rather than freed -- leaking a link on a
     * contended teardown is the price of not crashing, and it is the right
     * way round. */
    if (s_tx && !s_stask && tx_claim()) {
        DeleteMidi(s_tx);
        s_tx = 0;
        s_txlink = 0;
        /* CLEARED WITH THE LINK, NOT WITH `s_cluster`. This says what the
         * freed link pointed at, so it dies with the link: clearing it on a
         * teardown that LEAKED the link (claim refused above) would let a
         * later send build a second one alongside the first. */
        s_txcluster[0] = 0;
        tx_release();
    }
    s_task = 0;
    if (s_parent_sig >= 0) {
        FreeSignal(s_parent_sig);
        s_parent_sig = -1;
    }
    s_cb = 0;
    s_user = 0;
    s_cluster[0] = 0;
    s_sendcluster[0] = 0;   /* the sender this named was joined above */
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

static int send_sink(const uint8_t *msg, uint32_t len, void *user) {
    (void)user;
    /* s_sendcluster, NOT NULL: NULL would mean the input cluster. */
    if (ri_pal_midi_send(s_sendcluster, msg, len) != 0) {
        s_send_err++;
        return 1;              /* stop this pump; retry next tick */
    }
    s_sent++;
    /* 0xF8 on its own is a clock. Counted separately because `s_sent` is
     * the whole ring and the accounting needs the clock half measured
     * rather than inferred from tempo and duration (R12 O1). */
    if (len == 1u && msg[0] == 0xF8u)
        s_clk_sent++;
    /* A note-ON among them. `sent` and even `clocks` cannot separate a
     * re-sent note-on from a note-off, and the R12 O1 accounting needs to
     * know whether a duplicate note-on left THIS process or appeared
     * further down the lane (RTP-MIDI, netmidid, CAMD). */
    if (len == 3u && (msg[0] & 0xF0u) == 0x90u && msg[2] != 0u)
        s_noteon_sent++;
    /* R12 O2: the duplicate is always this exact message. Count it HERE,
     * at the CAMD boundary, so a wire count above this number is proof the
     * extra copy appeared downstream of the app. */
    if (len == 3u && msg[0] == 0x90u && msg[1] == 36u && msg[2] == 112u)
        s_n36a_sent++;
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
         * ONE REQUEST, THE ONE THAT WAS OPENED, REUSED EVERY TICK
         * (2026-10-12). THIS IS THE CRASH.
         *
         * This loop used to build a FRESH `CreateIORequest` every 1 ms and
         * call `SendIO` on it. `CreateIORequest` allocates the request and
         * leaves `io_Device` NULL -- only `OpenDevice` fills that in -- so
         * every one of those sends went to a request with no device behind
         * it. The task DID open a request, `s_streq`, via OpenDevice on
         * timer.device above, and then never sent a single tick on it.
         *
         * The lane says what that costs:
         *
         *     Task:   RIAPP midi-out
         *     Error:  0x80000003 - Illegal address access
         *     Function: Exec_777_SendIO
         *     Stack:  __inline_Exec_SendIO / sender_task + 0x2DD
         *
         * That is also what the Dell reported, and I spent the previous fix
         * on the LINK teardown instead -- a real defect, an unrelated one,
         * and one that could not have produced this stack, because this
         * SendIO is called by sender_task directly and never goes near
         * PutMidi. I had a signature and a theory and I matched them on the
         * words rather than the frames.
         *
         * Reusing the opened request also removes the per-tick
         * Create/Delete pair, which is not a tidiness win: that churn is
         * what forced a new unopened request onto the send path at all. */
        s_streq->tr_node.io_Command = TR_ADDREQUEST;
        s_streq->tr_time.tv_secs = 0;
        s_streq->tr_time.tv_micro = 1000;
        SendIO((struct IORequest *)s_streq);
        /* WAIT FOR THE TICK **OR** THE STOP, ONE WAIT (2026-10-19).
         *
         * This used to be `WaitPort(s_sport)` followed by
         * `Wait(1UL << s_skill_sig)`. The first call already waited out the
         * 1 ms tick; the second then blocked until someone signalled stop,
         * which nobody does while clock out is meant to run. So the loop
         * executed exactly once -- the pump at the top, with an empty ring
         * at startup -- and then hung forever: no error, the task present
         * and "waiting" in TaskList, the ring full at 255, `sent=0`,
         * `err=0`, and not one F8 on the wire after 40 s of playback. The
         * heartbeat's `RIAPP clk: on=1 run=1 pend=255 dropped=... sent=0`
         * is the signature (Dell, 2026-10-19).
         *
         * Both wakeups belong in ONE Wait, because the stop must also wake a
         * task that is sitting in its 1 ms tick. The timer reply is reaped
         * only when the port signal is the one that fired; when stop wins the
         * race, CheckIO/AbortIO/WaitIO below cancel the outstanding tick
         * before the loop breaks. */
        sigs = Wait((1UL << s_sport->mp_SigBit) | (1UL << s_skill_sig));
        if (sigs & (1UL << s_sport->mp_SigBit))
            (void)GetMsg(s_sport);   /* the timer reply */
        if (!CheckIO((struct IORequest *)s_streq))
            AbortIO((struct IORequest *)s_streq);
        WaitIO((struct IORequest *)s_streq);
        if (sigs & (1UL << s_skill_sig))
            break;
    }
    if (s_stimer) {
        CloseDevice(s_stimer);
        /* THE REQUEST IS FREED NOW, BECAUSE IT IS THE ONE THAT LASTS. The
         * old code closed the device and dropped the pointer without ever
         * deleting `s_streq`: harmless-looking while a fresh request was
         * allocated every tick, and a straight leak once this task reuses
         * one. Order is close, then free the request, then free the port it
         * was built on. */
        if (s_streq)
            DeleteIORequest((struct IORequest *)s_streq);
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
    uint32_t spins = 0u, i;
    if (!o || s_stask)
        return 1;
    /* THE SENDER USES THE PORT IT WAS GIVEN (R12). It used to ignore `port`
     * and send to the cluster the INPUT open selected. On an app-to-app
     * setup those are the same cluster; on a driver port they are not
     * (`applemidi.in.N` vs `applemidi.out.N`), which is why the first clock
     * proof had to point RIAPP_MIDI_IN at the OUTPUT cluster. NULL/"" keeps
     * the old meaning: ri_pal_midi_send() falls back to the open cluster. */
    for (i = 0u; port && port[i] && i < sizeof s_sendcluster - 1u; i++)
        s_sendcluster[i] = port[i];
    s_sendcluster[i] = 0;
    s_out = o;
    s_sent = 0u;
    s_send_err = 0u;
    s_clk_sent = 0u;
    s_noteon_sent = 0u;
    s_n36a_sent = 0u;
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
    if (s_skill_sig >= 0)
        Signal(s_stask, 1UL << s_skill_sig);
    while (!ri_atomic_load_acq(&s_stask_done) && spins < 100u) {
        Delay(1);
        spins++;
    }
    /* DO NOT CLEAR s_stask UNLESS THE TASK IS ACTUALLY GONE (2026-10-12).
     *
     * The bounded wait is right -- teardown must not wedge -- but clearing
     * `s_stask` after giving up on the wait is the opposite of right. It
     * makes the code claim a task it has not joined: a later
     * `ri_pal_midi_send_start` sees `s_stask == 0` and starts a SECOND sender
     * while the first still holds `s_sport` and the timer, and a caller that
     * trusts the teardown frees the camd link out from under the first one.
     *
     * Both of those end in `PutMidi` on a freed link, which is an
     * `exec_777_sendio` illegal access -- the Dell, 2026-10-12. Leaving the
     * handle set is the honest state: the task is still there, and the next
     * start will refuse rather than create a second. */
    if (!ri_atomic_load_acq(&s_stask_done)) {
        /* Still running. `s_out` and `s_sparent_sig` MUST stay, because the
         * task is about to read and signal them. Deliberately not falling
         * through to the FreeSignal below. */
        return;
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
uint32_t ri_pal_midi_clocks(void) { return s_clk_sent; }
uint32_t ri_pal_midi_noteons(void) { return s_noteon_sent; }
uint32_t ri_pal_midi_n36a(void) { return s_n36a_sent; }
