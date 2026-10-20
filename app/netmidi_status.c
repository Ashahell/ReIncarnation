/* netmidi_status.c — read the running netmidid's public status semaphore.
 *
 * AROS-only, and a diagnostic rather than part of the app.  The network
 * MIDI service publishes `netmidi.status` (a public SignalSemaphore) about
 * once a second; this prints the counters per port.
 *
 * WHY IT EXISTS.  R12 O2 found the duplicated 303 accent note-on leaving
 * RIAPP's CAMD send path once (the app's own counter) and arriving on the
 * host wire twice.  This reader made the next split: `msg_out` is what
 * netmidid actually encoded into RTP-MIDI, so a wire count above it puts
 * the extra copy beyond the guest.  That is how the hop was named (the
 * host's rtpmidid replays journal note-ons).  Keep it: the next network
 * MIDI question starts with these counters.
 *
 * usage: NETMIDISTATUS
 * build: scripts/ri_build_v11.sh <src> /tmp/NETMIDISTATUS netmidistatus
 *
 * The struct below is the v1 layout from the netmidid source
 * (network/netmidid/netmidi_status.h, NETMIDI_STATUS_VERSION 1).  It is
 * copied rather than included because the service lives outside this
 * repository; the version field is printed so a mismatch is visible.
 */
#ifndef __AROS__
#error "app/netmidi_status.c is AROS-only: exec semaphore reader, never in the host build"
#endif

#include <exec/exec.h>
#include <exec/semaphores.h>
#include <dos/dos.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <stdio.h>

#define NETMIDI_STATUS_NAME     "netmidi.status"
#define NETMIDI_STATUS_VERSION  1
#define NETMIDI_STATUS_PORTS    32

struct NetMidiPortStatus {
    unsigned char ps_Present;
    unsigned char ps_Pad[3];
    char          ps_Name[64];
    ULONG ps_PacketsIn;
    ULONG ps_PacketsOut;
    ULONG ps_PacketsLost;
    ULONG ps_MessagesIn;
    ULONG ps_MessagesOut;
    ULONG ps_Timed;
    ULONG ps_Late;
    ULONG ps_SysExDropped;
    ULONG ps_JournalsOmitted;
    ULONG ps_Undefined;
};

struct NetMidiStatus {
    struct SignalSemaphore   ns_Semaphore;
    ULONG            ns_Version;
    char                     ns_Session[64];
    ULONG            ns_Refused;
    struct NetMidiPortStatus ns_Port[NETMIDI_STATUS_PORTS];
};

int main(void)
{
    struct SignalSemaphore *sem;
    struct NetMidiStatus st;
    int i;

    Forbid();
    sem = FindSemaphore((STRPTR)NETMIDI_STATUS_NAME);
    if (!sem) {
        Permit();
        printf("netmidi_status: no %s (service not running)\n",
            (char *)NETMIDI_STATUS_NAME);
        return 1;
    }
    ObtainSemaphoreShared(sem);
    st = *(struct NetMidiStatus *)sem;
    ReleaseSemaphore(sem);
    Permit();

    printf("netmidi status v%lu (reader expects v%lu) session=%s refused=%lu\n",
        (unsigned long)st.ns_Version, (unsigned long)NETMIDI_STATUS_VERSION,
        st.ns_Session, (unsigned long)st.ns_Refused);
    for (i = 0; i < NETMIDI_STATUS_PORTS; i++) {
        const struct NetMidiPortStatus *p = &st.ns_Port[i];
        if (!p->ps_Present && !p->ps_PacketsOut && !p->ps_MessagesOut)
            continue;
        printf("port %d present=%u name=%s pkt_out=%lu msg_out=%lu "
               "msg_in=%lu journals_omitted=%lu pkt_lost=%lu late=%lu\n",
            i, (unsigned)p->ps_Present, p->ps_Name,
            (unsigned long)p->ps_PacketsOut,
            (unsigned long)p->ps_MessagesOut,
            (unsigned long)p->ps_MessagesIn,
            (unsigned long)p->ps_JournalsOmitted,
            (unsigned long)p->ps_PacketsLost,
            (unsigned long)p->ps_Late);
    }
    return 0;
}
