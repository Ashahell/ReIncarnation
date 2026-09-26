/* midi_camd.c — AROS backend for ri_pal_midi (portability plan T5).
 * AROS-only. Wraps camd.library: receiver node+link per open_in, a cached
 * sender for send, GetMidi drain via ri_pal_midi_poll (called from the
 * app's event loop — the SPSC into midimap stays app-side, G7 unchanged).
 * time_us is 0 (no backend clock yet; ri_time_us lands in T8).
 */
#ifndef __AROS__
#error "midi_camd.c is AROS-only (portability plan T5)"
#endif

#include "platform/pal/ri_pal_midi.h"

#include <exec/types.h>
#include <exec/io.h>
#include <midi/camd.h>
#include <proto/exec.h>
#include <proto/camd.h>

#include <string.h>

struct Library *CamdBase; /* camd inline base (one definition per link) */
static struct MidiNode *s_rx;
static struct MidiLink *s_rxlink;
static struct MidiNode *s_tx;
static struct MidiLink *s_txlink;
static char s_cluster[64];
static BYTE s_sig = -1;
static ri_midi_in s_cb;
static void *s_user;

static int lazy_base(void) {
    if (!CamdBase)
        CamdBase = OpenLibrary("camd.library", 0);
    return CamdBase ? 1 : 0;
}

int ri_pal_midi_open_in(const char *port, ri_midi_in cb, void *user) {
    uint32_t i;
    if (!port || !port[0] || !cb)
        return 1;
    if (!lazy_base())
        return 1;
    ri_pal_midi_close();
    s_sig = AllocSignal(-1);
    if (s_sig < 0)
        return 1;
    s_rx = CreateMidi(MIDI_Name, (IPTR)"RIPAL",
        MIDI_RecvSignal, (IPTR)s_sig, MIDI_MsgQueue, 512, TAG_END);
    if (!s_rx) {
        FreeSignal(s_sig);
        s_sig = -1;
        return 1;
    }
    s_rxlink = AddMidiLink(s_rx, MLTYPE_Receiver, MLINK_Location, (IPTR)port, TAG_END);
    if (!s_rxlink) {
        DeleteMidi(s_rx);
        s_rx = 0;
        FreeSignal(s_sig);
        s_sig = -1;
        return 1;
    }
    for (i = 0u; i < sizeof s_cluster - 1u && port[i]; i++)
        s_cluster[i] = port[i];
    s_cluster[i] = 0;
    s_cb = cb;
    s_user = user;
    return 0;
}

/* Drain one signal batch into the open callback. Call from the app loop. */
void ri_pal_midi_poll(void) {
    MidiMsg mm;
    if (!s_rx || !s_cb)
        return;
    while (GetMidi(s_rx, &mm)) {
        uint8_t msg[3];
        msg[0] = mm.mm_Status;
        msg[1] = mm.mm_Data1;
        msg[2] = mm.mm_Data2;
        s_cb(s_user, msg, 3u, 0u); /* time_us pending T8 clock */
    }
    SetSignal(0, 1UL << s_sig);
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
    if (s_tx) {
        DeleteMidi(s_tx);
        s_tx = 0;
        s_txlink = 0;
    }
    if (s_rx) {
        DeleteMidi(s_rx);
        s_rx = 0;
        s_rxlink = 0;
    }
    if (s_sig >= 0) {
        FreeSignal(s_sig);
        s_sig = -1;
    }
    s_cb = 0;
    s_user = 0;
    s_cluster[0] = 0;
}
