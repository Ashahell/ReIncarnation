/* probe_rate_hw.c — mixer rate M vs TRUE hardware rate H (AHI honest-rate H1).
 *
 * AROS-ONLY. Same #error contract as probe_ahi.c and probe_rate.c and for the
 * same reason: it uses ahi.library/ahi.device and must never enter the host
 * build, and ri_audit.sh gates that fact.
 *
 * WHY THIS EXISTS. probe_rate.c reads AHIC_MixFreq_Query (M, the rate the
 * mixer runs at). That tells what AHI *thinks*, not what the hardware does:
 * HDAudio's _AHIsub_AllocAudio picks the nearest listed rate into
 * card->selected_freq_index and programs the stream format from that index,
 * but never writes ahiac_MixFreq back. So for an off-list request the mixer
 * runs the request while the hardware runs the nearest listed rate, and M
 * alone cannot see it. This probe measures H independently.
 *
 * Method. For each requested AHIA_MixFreq, on one known-good HDAudio mode id:
 * load one 16-bit mono sound of N = 4 * M frames (sample frequency = M, so
 * one sound frame is one mixer frame), play it gaplessly re-queued from the
 * main task on every SoundFunc signal, and stamp every SoundFunc callback
 * with ReadEClock(). One loop takes N/H seconds of wall time, because the
 * mixer produces N frames at M and the hardware consumes them at H.
 * H = N / (mean loop duration) over 8 loops (10 callbacks, first loop
 * dropped as startup). Positive control: at a listed rate (48000), H must
 * equal M within 0.1 %; if it does not, the method is wrong, not the driver.
 *
 * The sound is all zeros: silence still consumes hardware at H, and a Dell
 * in a room does not need minutes of tone. Volume is full so no driver can
 * claim it skipped a "silent" sound.
 *
 * Output is one RI_RATEHW line per request, prefixed so a log can be
 * grepped. No floating point, static storage only.
 */

#ifndef __AROS__
#error "probe_rate_hw.c is AROS-only: it uses ahi.library/ahi.device and must never enter the host build"
#endif

#include <exec/types.h>
#include <exec/io.h>
#include <exec/tasks.h>
#include <exec/memory.h>
#include <devices/ahi.h>
#include <devices/timer.h>
#include <utility/hooks.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/ahi.h>
#include <proto/timer.h>
#include <utility/tagitem.h>

/* AHIBase is ours to define: taken from the session request's io_Device
 * (device-as-library), never via OpenLibrary -- there is no LIBS:ahi.library
 * by design. DOSBase is NOT defined here; <proto/dos.h> declares it and
 * startup.o provides the storage. */
struct Library *AHIBase = NULL;
struct Device *TimerBase = NULL;

static const ULONG s_req[] = { 44100UL, 48000UL, 50000UL, 60000UL,
                               96000UL, 100000UL, 32000UL, 22050UL };
#define NREQ (sizeof s_req / sizeof s_req[0])

/* SoundFunc context: the hook runs in driver context, so it only counts,
 * stamps the EClock, and Signals. Everything else happens in main. */
#define NSTAMP 12u
static volatile ULONG s_count;
static volatile ULONG s_lo[NSTAMP], s_hi[NSTAMP];
static struct Task *s_main;
static ULONG s_mask;
static ULONG s_efreq;

static ULONG hw_entry(struct Hook *h, APTR actrl, APTR msg) {
    struct EClockVal ev;
    ULONG n;
    (void)h;
    (void)actrl;
    (void)msg;
    n = s_count;
    if (TimerBase && n < NSTAMP) {
        ReadEClock(&ev);
        s_lo[n] = ev.ev_lo;
        s_hi[n] = ev.ev_hi;
    }
    s_count = n + 1;
    if (s_main)
        Signal(s_main, s_mask);
    return 0;
}

static unsigned long long ticks_at(ULONG i) {
    return ((unsigned long long)s_hi[i] << 32) | (unsigned long long)s_lo[i];
}

int main(void) {
    struct MsgPort *port = NULL;
    struct AHIRequest *req_api = NULL;
    struct MsgPort *tport = NULL;
    struct timerequest *treq = NULL;
    struct timerequest *wtreq = NULL;
    struct MsgPort *wport = NULL;
    struct Hook hook;
    BYTE sig = -1;
    ULONG mode = AHI_INVALID_ID;
    ULONG r;

    DOSBase = (struct DosLibrary *) OpenLibrary((STRPTR)"dos.library", 0);
    if (!DOSBase)
        return 20;

    Printf("RI_RATEHW probe=probe_rate_hw requests=%lu\n", (ULONG)NREQ);

    port = CreateMsgPort();
    if (port)
        req_api = (struct AHIRequest *)CreateIORequest(port, sizeof(struct AHIRequest));
    if (req_api == NULL) {
        Printf("RI_RATEHW open=NO_PORT_OR_REQUEST\n");
        if (port)
            DeleteMsgPort(port);
        CloseLibrary((struct Library *)DOSBase);
        return 21;
    }
    if (OpenDevice((STRPTR)"ahi.device", AHI_NO_UNIT,
            (struct IORequest *)req_api, 0) != 0) {
        Printf("RI_RATEHW open=FAIL\n");
        DeleteIORequest((struct IORequest *)req_api);
        DeleteMsgPort(port);
        CloseLibrary((struct Library *)DOSBase);
        return 22;
    }
    AHIBase = (struct Library *)req_api->ahir_Std.io_Device;
    Printf("RI_RATEHW version=%lu\n", (ULONG)AHIBase->lib_Version);

    /* EClock for the wall-time stamps (hook side). */
    tport = CreateMsgPort();
    if (tport)
        treq = (struct timerequest *)CreateIORequest(tport, sizeof(struct timerequest));
    if (treq && OpenDevice((STRPTR)"timer.device", UNIT_ECLOCK,
            (struct IORequest *)treq, 0) == 0) {
        struct EClockVal t0;
        TimerBase = treq->tr_node.io_Device;
        s_efreq = ReadEClock(&t0);
    }
    Printf("RI_RATEHW eclock=%lu\n", s_efreq);
    if (!TimerBase || !s_efreq) {
        Printf("RI_RATEHW eclock=FAIL\n");
        goto out;
    }

    /* Watchdog: a 600 s MICROHZ request whose reply also wakes us, so a
     * driver that never calls SoundFunc aborts the probe instead of wedging
     * the lane with AHI held open. (240 s was too short: the full 8-rate
     * sweep takes ~260 s and the watchdog fired mid-sweep on 2026-10-07.) */
    wport = CreateMsgPort();
    if (wport)
        wtreq = (struct timerequest *)CreateIORequest(wport, sizeof(struct timerequest));
    if (wtreq && OpenDevice((STRPTR)"timer.device", UNIT_MICROHZ,
            (struct IORequest *)wtreq, 0) == 0) {
        wtreq->tr_node.io_Command = TR_ADDREQUEST;
        wtreq->tr_time.tv_secs = 600;
        wtreq->tr_time.tv_micro = 0;
        SendIO((struct IORequest *)wtreq);
    }
    else {
        Printf("RI_RATEHW watchdog=FAIL\n");
        goto out;
    }

    sig = AllocSignal(-1);
    if (sig < 0) {
        Printf("RI_RATEHW signal=FAIL\n");
        goto out;
    }
    s_mask = 1UL << sig;
    s_main = FindTask(NULL);

    /* One known-good HDAudio mode for every rate (a BestAudioID refusal says
     * nothing by itself -- read the list, not the selector). 48000 is the
     * rate the selector matches, so it yields the mode id. */
    {
        struct TagItem best[] = {
            { AHIDB_Frequency, 48000 },
            { AHIDB_Stereo,    TRUE },
            { AHIDB_HiFi,      TRUE },
            { TAG_DONE,        0 }
        };
        mode = AHI_BestAudioID(best);
    }
    Printf("RI_RATEHW mode=0x%08lx\n", mode);
    if (mode == AHI_INVALID_ID) {
        Printf("RI_RATEHW mode=INVALID\n");
        goto out;
    }

    hook.h_Entry = (ULONG (*)())hw_entry;

    for (r = 0; r < NREQ; ++r) {
        struct AHIAudioCtrl *actl = NULL;
        struct TagItem at[6];
        WORD *pcm = NULL;
        ULONG want = s_req[r], nframes, i;
        ULONG mixq = 0;
        unsigned long long d[10], dsum = 0, dmin = 0, dmax = 0;
        ULONG got = 0;

        nframes = 4u * want;
        pcm = (WORD *)AllocVec(nframes * sizeof(WORD), MEMF_ANY | MEMF_CLEAR);
        if (!pcm) {
            Printf("RI_RATEHW req=%lu alloc=FAIL\n", want);
            continue;
        }

        at[0].ti_Tag = AHIA_AudioID;   at[0].ti_Data = (IPTR)mode;
        at[1].ti_Tag = AHIA_MixFreq;   at[1].ti_Data = (IPTR)want;
        at[2].ti_Tag = AHIA_Channels;  at[2].ti_Data = 1;
        at[3].ti_Tag = AHIA_Sounds;    at[3].ti_Data = 1;
        at[4].ti_Tag = AHIA_SoundFunc; at[4].ti_Data = (IPTR)&hook;
        at[5].ti_Tag = TAG_DONE;       at[5].ti_Data = 0;

        actl = AHI_AllocAudioA(at);
        if (!actl) {
            Printf("RI_RATEHW req=%lu alloc_audio=FAIL\n", want);
            FreeVec(pcm);
            continue;
        }
        {
            struct TagItem q[] = { { AHIC_MixFreq_Query, (IPTR)&mixq }, { TAG_DONE, 0 } };
            AHI_ControlAudioA(actl, q);
        }
        {
            struct AHISampleInfo si;
            si.ahisi_Type = AHIST_M16S;
            si.ahisi_Address = pcm;
            si.ahisi_Length = nframes;
            if (AHI_LoadSound(0, AHIST_DYNAMICSAMPLE, &si, actl) != AHIE_OK) {
                Printf("RI_RATEHW req=%lu mixq=%lu loadsound=FAIL\n", want, mixq);
                AHI_FreeAudio(actl);
                FreeVec(pcm);
                continue;
            }
        }
        {
            struct TagItem play[] = { { AHIC_Play, TRUE }, { TAG_DONE, 0 } };
            AHI_ControlAudioA(actl, play);
        }
        /* Sample frequency = M: one sound frame per mixer frame, so one
         * loop is N mixer frames consumed at H. */
        AHI_SetFreq(0, mixq ? mixq : want, actl, AHISF_IMM);
        AHI_SetVol(0, 0x10000, 0x8000, actl, AHISF_IMM);

        s_count = 0;
        AHI_SetSound(0, 0, 0, 0, actl, AHISF_IMM);

        /* Collect 10 callbacks (9 loops); re-queue gaplessly while the
         * current loop still plays, so hardware timing stays exact. */
        for (i = 0; i < 10; ++i) {
            ULONG sigs = Wait(s_mask | (1UL << wport->mp_SigBit));
            if (sigs & (1UL << wport->mp_SigBit)) {
                Printf("RI_RATEHW req=%lu TIMEOUT waiting callback %lu/%lu\n",
                       want, i, (ULONG)10);
                break;
            }
            if (i < 9)
                AHI_SetSound(0, 0, 0, 0, actl, AHISF_NONE);
        }
        got = s_count;
        {
            struct TagItem stop[] = { { AHIC_Play, FALSE }, { TAG_DONE, 0 } };
            AHI_ControlAudioA(actl, stop);
        }
        AHI_UnloadSound(0, actl);
        AHI_FreeAudio(actl);
        FreeVec(pcm);

        if (got < 10) {
            Printf("RI_RATEHW req=%lu mixq=%lu callbacks=%lu SHORT\n", want, mixq, got);
            continue;
        }
        /* 9 loop durations from the 10 stamps; drop the first (startup),
         * mean the other 8. */
        for (i = 0; i < 9; ++i)
            d[i] = ticks_at(i + 1) - ticks_at(i);
        for (i = 1; i < 9; ++i) {
            dsum += d[i];
            if (i == 1 || d[i] < dmin)
                dmin = d[i];
            if (d[i] > dmax)
                dmax = d[i];
        }
        {
            unsigned long long mean = dsum / 8u;
            unsigned long long H = mean ? (nframes * (unsigned long long)s_efreq) / mean : 0u;
            unsigned long long permille = mixq ? (H * 1000u) / mixq : 0u;
            Printf("RI_RATEHW req=%lu M=%lu H=%lu permille=%lu nloops=8 tickmean=%lu tickmin=%lu tickmax=%lu efreq=%lu nframes=%lu\n",
                   want, mixq, (ULONG)H, (ULONG)permille,
                   (ULONG)mean, (ULONG)dmin, (ULONG)dmax, s_efreq, nframes);
        }
    }

    Printf("RI_RATEHW done\n");

out:
    if (wtreq && TimerBase) {
        if (!CheckIO((struct IORequest *)wtreq))
            AbortIO((struct IORequest *)wtreq);
        WaitIO((struct IORequest *)wtreq);
    }
    if (wtreq) {
        CloseDevice((struct IORequest *)wtreq);
        DeleteIORequest((struct IORequest *)wtreq);
    }
    if (wport)
        DeleteMsgPort(wport);
    if (sig >= 0)
        FreeSignal(sig);
    s_main = NULL;
    if (TimerBase) {
        CloseDevice((struct IORequest *)treq);
        TimerBase = NULL;
    }
    if (treq)
        DeleteIORequest((struct IORequest *)treq);
    if (tport)
        DeleteMsgPort(tport);
    CloseDevice((struct IORequest *)req_api);
    DeleteIORequest((struct IORequest *)req_api);
    DeleteMsgPort(port);
    AHIBase = NULL;
    CloseLibrary((struct Library *)DOSBase);
    return 0;
}
