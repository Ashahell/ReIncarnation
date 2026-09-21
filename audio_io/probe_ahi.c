/*
 * probe_ahi.c — M1.1 AHI measurement probe (Task 5, gate G5).
 *
 * AROS-ONLY. Uses ahi.library (low-level API) and ahi.device (CMD_WRITE
 * double-buffering) and must NEVER enter the host build: the #error below
 * fires on any non-AROS compile, scripts/ri_build_host.sh never references
 * this file, and scripts/ri_audit.sh gates both facts.
 *
 * Method (spec §4.2, OPEN-09; prior-art register entries 17-18):
 *   Phase P1 — negotiate the low-level path: AHI_BestAudioID() for 48 kHz
 *     stereo HiFi, then AHI_AllocAudioA() with a PlayerFunc hook. Reads the
 *     ACTUAL mode back via AHI_GetAudioAttrsA(AHI_INVALID_ID, actl, ...)
 *     (the public struct AHIAudioCtrl is opaque — only ahiac_UserData is
 *     public), never assuming the request was met.
 *   Phase P2 — buffer ladder on the low-level path: AHIA_PlayerFreq =
 *     48000/size Hz (Fixed 16.16) for sizes 4096..64 frames via
 *     AHI_ControlAudio(). The AHI developer doc suggests keeping
 *     PlayerFreq below 100-200 Hz (240+ frames @48 kHz); the probe tries
 *     the full ladder anyway and RECORDS what the driver accepts.
 *   Phase P3 — timed verification at the smallest accepted size: count
 *     PlayerFunc invocations over a 5 s Delay() window, report
 *     observed vs expected. While an AudioCtrl is active the driver feeds
 *     silence to the hardware, so no channel needs to play for the mixer
 *     (and the hook) to run. Shortfall = xrun proxy.
 *   Phase P4 — ahi.device path: OpenDevice unit AHI_DEFAULT_UNIT, request
 *     AHIST_S16S @48 kHz, double-buffered CMD_WRITE pair at each ladder
 *     size, record io_Error per size. Buffers are BSS (silence).
 *   Phase P5 — machine-readable RI_PROBE summary block. The operator
 *     pastes these lines verbatim into docs/evidence/formats/m1-1-report.md.
 *
 * Hook-context contract (spec §4.2): the PlayerFunc hook runs in the
 * driver's mixer context. Its ONLY actions are incrementing a counter in
 * preallocated static storage and Signal()ing the task. No library calls,
 * no floating point, no memory access outside static storage.
 *
 * Static storage only (no AllocMem/malloc anywhere, so the probe itself
 * honors the render-path hygiene it measures for). No floating point.
 */

#ifndef __AROS__
#error "probe_ahi.c is AROS-only: it uses ahi.device/ahi.library and must never enter the host build"
#endif

#include <exec/types.h>
#include <exec/io.h>
#include <exec/errors.h>
#include <utility/tagitem.h>
#include <utility/hooks.h>
#include <devices/ahi.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/ahi.h>

/* Engine block the Classic render path is locked to (spec §2.3/§3 W1). */
#define RI_PROBE_RATE      48000UL
#define RI_PROBE_VERIFY_S  5UL

/* Ladder, in device frames. Must stay a multiple-of-64 descending list so
 * every rung is an integer number of 64-frame engine blocks. */
static const ULONG s_ladder[] = { 4096, 2048, 1024, 512, 256, 128, 64 };
#define RI_PROBE_LADDER_N (sizeof (s_ladder) / sizeof (s_ladder[0]))

/* 4096 stereo 16-bit frames per half of the double buffer (BSS = silence). */
#define RI_PROBE_MAXFRAMES 4096
static WORD s_pcm_a[RI_PROBE_MAXFRAMES * 2];
static WORD s_pcm_b[RI_PROBE_MAXFRAMES * 2];

/* Hook state: preallocated, touched from mixer context. */
static volatile ULONG s_player_ticks;
static struct Task *s_player_task;
static struct Hook s_player_hook;

/* AHIBase/DOSBase are the extern globals declared by <proto/ahi.h> and
 * <proto/dos.h>; the inline calls below resolve through them. SysBase and
 * DOSBase are provided by startup.o; the AHI base is ours to define —
 * taken from the P1 session request's io_Device (device-as-library),
 * never via OpenLibrary (there is no LIBS:ahi.library by design). */
struct Library *AHIBase = NULL;

/* PlayerFunc: Signal() + counter only (spec §4.2 hook contract). */
static ULONG player_entry(struct Hook *h, APTR obj, APTR msg)
{
    (void) h;
    (void) obj;
    (void) msg;
    ++s_player_ticks;
    if (s_player_task != NULL)
        Signal(s_player_task, SIGBREAKF_CTRL_F);
    return 0;
}

/* Frames -> Fixed 16.16 PlayerFreq (48000/size calls per second). Uses a
 * 64-bit intermediate so (48000*65536) never overflows. */
static ULONG player_freq_fixed(ULONG frames)
{
    return (ULONG) ((((unsigned long long) RI_PROBE_RATE) * 65536ULL)
                    / (unsigned long long) frames);
}

int main(void)
{
    struct AHIAudioCtrl *actl = NULL;
    ULONG mode_id;
    ULONG low_min = 0;      /* smallest low-level size accepted (frames) */
    ULONG dev_min = 0;      /* smallest ahi.device size clean (frames) */
    ULONG verify_obs = 0;
    ULONG verify_exp = 0;
    ULONG dev_open_rc = 0;
    ULONG i;
    /* Low-level API session handle: the device IS the library provider
     * (classic AmigaOS/MorphOS/AROS model — there is no LIBS:ahi.library).
     * Opened on AHI_NO_UNIT in P1, base taken from io_Device, closed
     * after P3. P4 uses its own separate port/requests below. */
    struct MsgPort *port_api = NULL;
    struct AHIRequest *req_api = NULL;

    DOSBase = (struct DosLibrary *) OpenLibrary("dos.library", 0);
    if (DOSBase == NULL)
        return 20;

    Printf("RI_PROBE program=probe_ahi m1.1 target=AROS-x86_64\n");
    Printf("RI_PROBE compiler=%s built=%s %s\n",
           (STRPTR) "x86_64-aros-gcc-16.1.0", (STRPTR) __DATE__,
           (STRPTR) __TIME__);
    Printf("RI_PROBE engine_block_frames=64 rate=%lu\n", RI_PROBE_RATE);

    /* ---- P1: low-level negotiate (device-as-library) ----
     * The ONLY supported entry: OpenDevice("ahi.device", AHI_NO_UNIT)
     * and take the library base from io_Device. There is no
     * LIBS:ahi.library on AROS by design, so OpenLibrary must NOT be
     * used here (it reports ABSENT and skips P1-P3). */
    port_api = CreateMsgPort();
    if (port_api != NULL)
        req_api = (struct AHIRequest *) CreateIORequest(port_api,
                      sizeof (struct AHIRequest));
    if (req_api == NULL)
    {
        Printf("RI_PROBE ahi.session: NO_PORT_OR_REQUEST\n");
        if (port_api != NULL)
            DeleteMsgPort(port_api);
        port_api = NULL;
    }
    else if (OpenDevice("ahi.device", AHI_NO_UNIT,
                        (struct IORequest *) req_api, 0) != 0)
    {
        Printf("RI_PROBE ahi.session: OPEN_FAIL\n");
        DeleteIORequest((struct IORequest *) req_api);
        DeleteMsgPort(port_api);
        port_api = NULL;
        req_api = NULL;
    }
    else
    {
        AHIBase = (struct Library *) req_api->ahir_Std.io_Device;
        struct TagItem best_tags[] =
        {
            { AHIDB_Frequency, RI_PROBE_RATE },
            { AHIDB_Stereo,    (IPTR) TRUE },
            { AHIDB_HiFi,      (IPTR) TRUE },
            { TAG_DONE,        0 }
        };

        Printf("RI_PROBE ahi.session: OPEN unit=%lu base=0x%p version=%lu\n",
               (ULONG) AHI_NO_UNIT, (APTR) AHIBase,
               (ULONG) AHIBase->lib_Version);
        mode_id = AHI_BestAudioID(best_tags);
        if (mode_id == AHI_INVALID_ID)
        {
            Printf("RI_PROBE best_mode: NONE for 48k-stereo-hifi\n");
        }
        else
        {
            Printf("RI_PROBE best_mode: id=0x%08lx\n", mode_id);
        }

        s_player_task = FindTask(NULL);
        s_player_hook.h_Entry = (ULONG (*)()) player_entry;

        {
            struct TagItem alloc_tags[] =
            {
                { AHIA_AudioID,     mode_id },
                { AHIA_MixFreq,     RI_PROBE_RATE },
                { AHIA_Channels,    1 },
                { AHIA_Sounds,      1 },
                { AHIA_PlayerFunc,  (IPTR) &s_player_hook },
                { AHIA_PlayerFreq,  (IPTR) player_freq_fixed(4096) },
                { AHIA_MinPlayerFreq, (IPTR) player_freq_fixed(4096) },
                { AHIA_MaxPlayerFreq, (IPTR) player_freq_fixed(64) },
                { TAG_DONE,         0 }
            };

            actl = AHI_AllocAudioA(alloc_tags);
            if (actl == NULL)
            {
                Printf("RI_PROBE alloc_audio: FAILED\n");
            }
            else
            {
                /* Read the ACTUAL mode back from the allocated handle
                 * (autodoc: ID=AHI_INVALID_ID + audioctrl; ti_Data is a
                 * pointer to result storage). Never assume the request
                 * was met. */
                ULONG q_freq = 0, q_bits = 0, q_stereo = 0;
                ULONG q_hifi = 0, q_maxch = 0;
                struct TagItem query_tags[] =
                {
                    { AHIDB_Frequency,   (IPTR) &q_freq },
                    { AHIDB_Bits,        (IPTR) &q_bits },
                    { AHIDB_Stereo,      (IPTR) &q_stereo },
                    { AHIDB_HiFi,        (IPTR) &q_hifi },
                    { AHIDB_MaxChannels, (IPTR) &q_maxch },
                    { TAG_DONE,          0 }
                };

                if (AHI_GetAudioAttrsA(AHI_INVALID_ID, actl, query_tags))
                {
                    Printf("RI_PROBE alloc_audio: OK freq=%lu bits=%lu stereo=%lu hifi=%lu maxch=%lu\n",
                           q_freq, q_bits, q_stereo, q_hifi, q_maxch);
                }
                else
                {
                    Printf("RI_PROBE alloc_audio: OK (attrs query FAILED)\n");
                }

                /* ---- P2: PlayerFreq ladder ---- */
                for (i = 0; i < RI_PROBE_LADDER_N; ++i)
                {
                    ULONG rc;
                    struct TagItem ctl_tags[] =
                    {
                        { AHIA_PlayerFreq, (IPTR) player_freq_fixed(s_ladder[i]) },
                        { TAG_DONE,        0 }
                    };

                    rc = AHI_ControlAudioA(actl, ctl_tags);
                    Printf("RI_PROBE lowlevel frames=%lu playerfreq_hz=%lu rc=%lu\n",
                           s_ladder[i], RI_PROBE_RATE / s_ladder[i], rc);
                    if (rc == 0)
                        low_min = s_ladder[i];
                }
                Printf("RI_PROBE low_min_frames=%lu\n", low_min);

                /* ---- P3: timed verification at the smallest accepted size ----
                 * Playback must be STARTED explicitly: AHIC_Play TRUE routes
                 * through AHIsub_Start, which spawns the driver's timing
                 * source; without it the mixer idles and PlayerFunc never
                 * fires (observed: 0 ticks over 5 s on VOID). Stopped after
                 * the window so FreeAudio sees a quiescent driver. */
                if (low_min != 0)
                {
                    struct TagItem lock_tags[] =
                    {
                        { AHIA_PlayerFreq, (IPTR) player_freq_fixed(low_min) },
                        { TAG_DONE,        0 }
                    };
                    struct TagItem play_tags[] =
                    {
                        { AHIC_Play, (IPTR) TRUE },
                        { TAG_DONE,  0 }
                    };
                    struct TagItem stop_tags[] =
                    {
                        { AHIC_Play, (IPTR) FALSE },
                        { TAG_DONE,  0 }
                    };

                    AHI_ControlAudioA(actl, lock_tags);
                    AHI_ControlAudioA(actl, play_tags);
                    s_player_ticks = 0;
                    Delay((ULONG) (50 * RI_PROBE_VERIFY_S));
                    verify_obs = s_player_ticks;
                    AHI_ControlAudioA(actl, stop_tags);
                    verify_exp = RI_PROBE_VERIFY_S * (RI_PROBE_RATE / low_min);
                    Printf("RI_PROBE verify frames=%lu window_s=%lu observed=%lu expected=%lu shortfall=%ld\n",
                           low_min, RI_PROBE_VERIFY_S, verify_obs, verify_exp,
                           (LONG) (verify_exp - verify_obs));
                }
                else
                {
                    Printf("RI_PROBE verify: SKIPPED (no ladder size accepted)\n");
                }

                AHI_FreeAudio(actl);
                actl = NULL;
            }
        }
        CloseDevice((struct IORequest *) req_api);
        DeleteIORequest((struct IORequest *) req_api);
        DeleteMsgPort(port_api);
        port_api = NULL;
        req_api = NULL;
        AHIBase = NULL;
    }

    /* ---- P4: ahi.device double-buffer ladder ---- */
    {
        struct MsgPort *port = CreateMsgPort();
        struct AHIRequest *r1;
        struct AHIRequest *r2;

        if (port == NULL)
        {
            Printf("RI_PROBE device: NO_MSGPORT\n");
        }
        else
        {
            r1 = (struct AHIRequest *) CreateIORequest(port, sizeof (struct AHIRequest));
            r2 = (struct AHIRequest *) CreateIORequest(port, sizeof (struct AHIRequest));
            if (r1 == NULL || r2 == NULL)
            {
                Printf("RI_PROBE device: NO_IOREQUEST\n");
            }
            else
            {
                dev_open_rc = (ULONG) OpenDevice("ahi.device", AHI_DEFAULT_UNIT,
                                                 (struct IORequest *) r1, 0);
                if (dev_open_rc != 0)
                {
                    Printf("RI_PROBE device: unit %lu ABSENT open_rc=%lu\n",
                           (ULONG) AHI_DEFAULT_UNIT, dev_open_rc);
                }
                else
                {
                    Printf("RI_PROBE device: unit %lu PRESENT\n",
                           (ULONG) AHI_DEFAULT_UNIT);
                    /* AHI double-buffer contract: the second request must
                     * be a copy of the opened first request (same device
                     * and unit). An unopened r2 (NULL device/unit) faults
                     * inside SendIO — observed as an Exec SendIO gate
                     * crash on the codec-less guest (2026-09-21: r2 dev=0
                     * unit=0 while r1 was fully initialized). */
                    r2->ahir_Std.io_Device = r1->ahir_Std.io_Device;
                    r2->ahir_Std.io_Unit   = r1->ahir_Std.io_Unit;
                    r1->ahir_Version   = 4;
                    r1->ahir_Type      = AHIST_S16S;
                    r1->ahir_Frequency = RI_PROBE_RATE;
                    r1->ahir_Volume    = 0x10000;
                    r1->ahir_Position  = 0x8000;
                    for (i = 0; i < RI_PROBE_LADDER_N; ++i)
                    {
                        ULONG bytes = s_ladder[i] * 2UL * 2UL; /* stereo S16 */

                        r1->ahir_Std.io_Command = CMD_WRITE;
                        r1->ahir_Std.io_Data    = (APTR) s_pcm_a;
                        r1->ahir_Std.io_Length  = (LONG) bytes;
                        r2->ahir_Std.io_Command = CMD_WRITE;
                        r2->ahir_Std.io_Data    = (APTR) s_pcm_b;
                        r2->ahir_Std.io_Length  = (LONG) bytes;
                        r1->ahir_Link = r2;
                        r2->ahir_Link = r1;
                        SendIO((struct IORequest *) r1);
                        SendIO((struct IORequest *) r2);
                        /* Bounded completion: a linked request whose
                         * predecessor never finishes (e.g. chained
                         * double-buffer on a driver without end-of-sound
                         * notification, observed on VOID) would block
                         * WaitIO forever and wedge the probe. Poll one
                         * second, then abort what is still pending; an
                         * aborted request reports IOERR_ABORTED and can
                         * never count toward dev_min below. */
                        Delay(50);
                        AbortIO((struct IORequest *) r1);
                        AbortIO((struct IORequest *) r2);
                        WaitIO((struct IORequest *) r1);
                        WaitIO((struct IORequest *) r2);
                        Printf("RI_PROBE device frames=%lu err1=%ld err2=%ld\n",
                               s_ladder[i],
                               (LONG) r1->ahir_Std.io_Error,
                               (LONG) r2->ahir_Std.io_Error);
                        if (r1->ahir_Std.io_Error == 0
                            && r2->ahir_Std.io_Error == 0)
                            dev_min = s_ladder[i];
                    }
                    Printf("RI_PROBE dev_min_frames=%lu\n", dev_min);
                    CloseDevice((struct IORequest *) r1);
                }
                if (r1 != NULL)
                    DeleteIORequest((struct IORequest *) r1);
                if (r2 != NULL)
                    DeleteIORequest((struct IORequest *) r2);
            }
            DeleteMsgPort(port);
        }
    }

    /* ---- P5: summary block (paste verbatim into m1-1-report.md) ---- */
    Printf("RI_PROBE SUMMARY low_min_frames=%lu dev_min_frames=%lu "
           "verify_obs=%lu verify_exp=%lu dev_open_rc=%lu\n",
           low_min, dev_min, verify_obs, verify_exp, dev_open_rc);

    CloseLibrary((struct Library *) DOSBase);
    return 0;
}
