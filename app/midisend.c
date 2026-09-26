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
 * Return codes: 0 ok, 5 args, 10 camd.library, 11 node/link, 12 script,
 * 20 self-test received nothing.
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

extern struct Library *CamdBase; /* owned by platform/aros/midi_camd.c (T5) */


static int hexv(char c) {
    return c >= '0' && c <= '9' ? c - '0' : c >= 'a' && c <= 'f' ? c - 'a' + 10 : c >= 'A' && c <= 'F' ? c - 'A' + 10 : -1;
}

int main(int argc, char **argv) {
    struct MidiNode *node;
    struct MidiLink *link;
    if (argc < 3)
        return 5;
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
