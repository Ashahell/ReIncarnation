/* audio_ahi_live.c — G9.3 live render task + low-level AHI backend.
 * AROS-only. Spec §4.2 (LOCKED): the SoundFunc hook runs in driver context
 * and only counts + Signal()s; the render Task does everything else.
 *
 * Stream: one AHI channel, two AHIST_DYNAMICSAMPLE sounds (the double
 * buffer). AHI calls SoundFunc each time a sound starts; by then the other
 * half has finished, so the task renders that half and queues it
 * (AHI_SetSound ... AHISF_NONE: plays when the current one ends). A late
 * task means AHI loops the last queued half: counted as an xrun, never a
 * hang. The task owns every AHI object from open to free (one owner, one
 * thread), and it is the only caller of ri_live_render / ri_live_play /
 * ri_live_stop — the GUI talks to it through lv->cmd and the SPSC control
 * plane only.
 *
 * Storage: static (one backend per process), sized once; nothing is
 * allocated per buffer. The ahi.device base comes from io_Device on
 * AHI_NO_UNIT (there is no LIBS:ahi.library on AROS — probe_ahi.c, M1.1).
 */
#ifndef __AROS__
#error "audio_ahi_live.c is AROS-only"
#endif

#include <exec/types.h>
#include <exec/tasks.h>
#include <exec/io.h>
#include <exec/memory.h>
#include <dos/dos.h>
#include <dos/dostags.h>
#include <utility/hooks.h>
#include <devices/ahi.h>
#include <devices/timer.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/ahi.h>
#include <proto/timer.h>
#include "audio_io/audio_ahi_live.h"
#include "engine/live.h"

struct Library *AHIBase = NULL;
struct Device *TimerBase = NULL; /* EClock for render timing (task side only) */

static struct AuLive *s_lv;
static struct Task *s_parent;
static BYTE s_parent_sig = -1;
static struct Task *s_render;
/* Open generation (bounded handshake): the GUI bumps s_open_gen around
 * every au_live_open; the task snapshots it at entry and exits quietly
 * (no lv writes, no signals) once its copy goes stale. A broken audio
 * driver must surface as err 7, never wedge the app: riqemu1's sb128
 * DriverInit faults inside the mode scan, and a slow scan must not block
 * past AU_LIVE_OPEN_TIMEOUT_S either. The GUI task owns the counter; the
 * render task only reads it. */
static volatile ULONG s_open_gen;
#define AU_LIVE_OPEN_TIMEOUT_S 10u
static volatile ULONG s_hook_count;
static unsigned long long s_render_us_acc;
static ULONG s_hook_mask;
static struct Hook s_sound_hook;
static WORD s_pcm[2][AU_LIVE_MAXFRAMES * 2u];
static float s_fl[AU_LIVE_MAXFRAMES], s_fr[AU_LIVE_MAXFRAMES];

/* SoundFunc: count + Signal() only (spec §4.2 hook contract). */
static ULONG sound_entry(struct Hook *h, APTR actrl, APTR msg) {
    (void)h;
    (void)actrl;
    (void)msg;
    ++s_hook_count;
    if (s_render != NULL)
        Signal(s_render, s_hook_mask);
    return 0;
}

/* PlayerFunc: present only so AHI accepts AHIA_PlayerFreq, which sets its
 * mixing pass to one device buffer (default ~50 passes/s = ~960 frames:
 * shorter halves would loop inside one pass). Does nothing (spec §4.2). */
static ULONG player_entry(struct Hook *h, APTR actrl, APTR msg) {
    (void)h;
    (void)actrl;
    (void)msg;
    return 0;
}
static struct Hook s_player_hook;

static void tell_parent(void) {
    if (s_parent != NULL && s_parent_sig >= 0)
        Signal(s_parent, 1UL << s_parent_sig);
}

/* Render one half: apply the GUI's transport request first (buffer
 * boundary), then one device buffer through the session, then s16. */
static void render_half(struct AuLive *lv, ULONG half) {
    ULONG i, got;
    LONG cmd = lv->cmd;
    if (cmd != AU_LIVE_CMD_NONE) {
        lv->cmd = AU_LIVE_CMD_NONE;
        if (cmd == AU_LIVE_CMD_PLAY)
            ri_live_play(lv->session);
        else if (cmd == AU_LIVE_CMD_STOP)
            ri_live_stop(lv->session);
    }
    struct EClockVal t0, t1;
    ULONG efreq = 0u;
    if (TimerBase)
        efreq = ReadEClock(&t0);
    got = ri_live_render(lv->session, s_fl, s_fr, lv->frames);
    for (i = got; i < lv->frames; i++)
        s_fl[i] = s_fr[i] = 0.0f;
    for (i = 0u; i < lv->frames; i++) {
        s_pcm[half][i * 2u] = au_live_f32_to_s16(s_fl[i]);
        s_pcm[half][i * 2u + 1u] = au_live_f32_to_s16(s_fr[i]);
    }
    if (lv->cap_on && lv->cap_buf) { /* bounded copy, no IO */
        ULONG n = lv->frames, pos = lv->cap_pos, k;
        if (n > lv->cap_max - pos)
            n = lv->cap_max - pos;
        for (k = 0u; k < n * 2u; k++)
            lv->cap_buf[pos * 2u + k] = s_pcm[half][k];
        lv->cap_pos = pos + n;
        if (lv->cap_pos >= lv->cap_max)
            lv->cap_on = 0;
    }
    if (TimerBase && efreq) {
        unsigned long long a = ((unsigned long long)t0.ev_hi << 32) | t0.ev_lo, b, us;
        ReadEClock(&t1);
        b = ((unsigned long long)t1.ev_hi << 32) | t1.ev_lo;
        us = (b - a) * 1000000ULL / efreq;
        if (us > lv->render_us_max)
            lv->render_us_max = (ULONG)us;
        s_render_us_acc += us;
        lv->render_us_sum_ms = (ULONG)(s_render_us_acc / 1000ULL);
    }
    lv->buffers++;
}

static void live_task(void) {
    struct AuLive *lv = s_lv;
    struct MsgPort *port = NULL;
    struct AHIRequest *req = NULL;
    struct AHIAudioCtrl *actl = NULL;
    struct timerequest *treq = NULL;
    struct MsgPort *tport = NULL;
    BYTE hsig = -1;
    ULONG processed = 0u, queued = 1u;
    int started = 0, dev_open = 0;
    ULONG mygen;

    s_render = FindTask(NULL);
    mygen = s_open_gen; /* snapshot: a stale copy means the GUI timed out */
    hsig = AllocSignal(-1);
    if (hsig < 0) {
        lv->err = 6;
        goto fail;
    }
    s_hook_mask = 1UL << hsig;
    tport = CreateMsgPort(); /* EClock only; timing is optional */
    if (tport)
        treq = (struct timerequest *)CreateIORequest(tport, sizeof(struct timerequest));
    if (treq && OpenDevice((STRPTR)"timer.device", UNIT_ECLOCK, (struct IORequest *)treq, 0) == 0)
        TimerBase = treq->tr_node.io_Device;
    port = CreateMsgPort();
    if (port)
        req = (struct AHIRequest *)CreateIORequest(port, sizeof(struct AHIRequest));
    if (!req) {
        lv->err = 1;
        goto fail;
    }
    req->ahir_Version = 4;
    if (OpenDevice((STRPTR)"ahi.device", AHI_NO_UNIT, (struct IORequest *)req, 0) != 0) {
        lv->err = 2;
        goto fail;
    }
    dev_open = 1;
    if (mygen != s_open_gen)
        goto fail; /* abandoned during the scan: unwind quiet (tail skips lv) */
    AHIBase = (struct Library *)req->ahir_Std.io_Device;
    {
        struct TagItem best[] = {
            { AHIDB_Frequency, 0 }, { AHIDB_Stereo, TRUE }, { AHIDB_HiFi, TRUE }, { TAG_DONE, 0 }
        };
        best[0].ti_Data = lv->want_rate;
        lv->mode_id = AHI_BestAudioID(best);
        if (lv->mode_id == AHI_INVALID_ID)
            lv->mode_id = AHI_DEFAULT_ID;
    }
    s_sound_hook.h_Entry = (ULONG (*)())sound_entry;
    s_player_hook.h_Entry = (ULONG (*)())player_entry;
    {
        /* PlayerFreq (Fixed 16.16) = rate / frames: one mixing pass per
         * device buffer (M1.1: the Dell accepted down to 64 frames). */
        ULONG pf = (ULONG)(((unsigned long long)lv->want_rate << 16) / lv->frames);
        struct TagItem at[] = {
            { AHIA_AudioID, 0 }, { AHIA_MixFreq, 0 }, { AHIA_Channels, 1 },
            { AHIA_Sounds, 2 }, { AHIA_SoundFunc, 0 }, { AHIA_PlayerFunc, 0 },
            { AHIA_PlayerFreq, 0 }, { AHIA_MinPlayerFreq, 0 }, { AHIA_MaxPlayerFreq, 0 },
            { TAG_DONE, 0 }
        };
        at[0].ti_Data = lv->mode_id;
        at[1].ti_Data = lv->want_rate;
        at[4].ti_Data = (IPTR)&s_sound_hook;
        at[5].ti_Data = (IPTR)&s_player_hook;
        at[6].ti_Data = pf;
        at[7].ti_Data = pf;
        at[8].ti_Data = pf;
        /* Owner (Dell 2026-09-26): at 1024 frames the PlayerFreq build
         * "sounds worse" than the plain one (0 xruns logged either way), so
         * the pass override is used only where it is required: halves
         * shorter than AHI's default ~960-frame pass. */
        if (lv->frames >= 1024u)
            at[5].ti_Tag = TAG_DONE;
        actl = AHI_AllocAudioA(at);
    }
    if (!actl) {
        lv->err = 4;
        goto fail;
    }
    {
        IPTR freq = 0; /* AHIC_*_Query stores an IPTR */
        struct TagItem q[] = { { AHIC_MixFreq_Query, 0 }, { TAG_DONE, 0 } };
        q[0].ti_Data = (IPTR)&freq;
        AHI_ControlAudioA(actl, q);
        lv->mix_freq = freq ? (ULONG)freq : lv->want_rate;
        lv->period_us = (ULONG)((unsigned long long)lv->frames * 1000000ULL / lv->mix_freq);
    }
    {
        struct AHISampleInfo si;
        ULONG k;
        for (k = 0u; k < 2u; k++) {
            si.ahisi_Type = AHIST_S16S;
            si.ahisi_Address = s_pcm[k];
            si.ahisi_Length = lv->frames;
            if (AHI_LoadSound((UWORD)k, AHIST_DYNAMICSAMPLE, &si, actl) != AHIE_OK) {
                lv->err = 5;
                goto fail;
            }
        }
    }
    /* Negotiated: the GUI initialises the session at mix_freq and calls
     * au_live_run, which sets lv->session and signals us again. */
    if (mygen != s_open_gen)
        goto fail; /* abandoned: unwind quiet (tail skips lv and the tell) */
    lv->state = 1;
    tell_parent();
    {
        ULONG sigs = Wait(s_hook_mask | SIGBREAKF_CTRL_C | SIGBREAKF_CTRL_E);
        if ((sigs & SIGBREAKF_CTRL_C) || !lv->session)
            goto done;
    }
    render_half(lv, 0u);
    render_half(lv, 1u);
    {
        struct TagItem play[] = { { AHIC_Play, TRUE }, { TAG_DONE, 0 } };
        AHI_ControlAudioA(actl, play);
    }
    AHI_SetFreq(0, lv->mix_freq, actl, AHISF_IMM);
    AHI_SetVol(0, 0x10000, 0x8000, actl, AHISF_IMM);
    AHI_SetSound(0, 0, 0, 0, actl, AHISF_IMM);
    AHI_SetSound(0, 1, 0, 0, actl, AHISF_NONE); /* half 1 follows half 0 */
    lv->state = 2;
    tell_parent();
    for (;;) {
        ULONG sigs = Wait(s_hook_mask | SIGBREAKF_CTRL_C);
        ULONG n, free_half;
        if (sigs & SIGBREAKF_CTRL_C)
            break;
        n = s_hook_count;
        if (n == processed)
            continue;
        if (!started) {        /* the first start is half 0 (IMM); half 1 is */
            if (n < 2u) {      /* already queued, nothing to render yet */
                processed = n;
                continue;
            }
            started = 1;
            processed = 1u;
        }
        if (n - processed > 1u)
            lv->xruns += n - processed - 1u; /* AHI looped a half: late */
        processed = n;
        /* the most recently queued half is the one now playing */
        free_half = queued ^ 1u;
        render_half(lv, free_half);
        AHI_SetSound(0, (UWORD)free_half, 0, 0, actl, AHISF_NONE);
        queued = free_half;
    }
done:
    {
        struct TagItem stop[] = { { AHIC_Play, FALSE }, { TAG_DONE, 0 } };
        AHI_ControlAudioA(actl, stop);
    }
fail:
    if (actl)
        AHI_FreeAudio(actl);
    if (dev_open)
        CloseDevice((struct IORequest *)req);
    if (req)
        DeleteIORequest((struct IORequest *)req);
    if (port)
        DeleteMsgPort(port);
    if (TimerBase) {
        CloseDevice((struct IORequest *)treq);
        if (mygen == s_open_gen)
            TimerBase = NULL;
    }
    if (treq)
        DeleteIORequest((struct IORequest *)treq);
    if (tport)
        DeleteMsgPort(tport);
    if (hsig >= 0)
        FreeSignal(hsig);
    /* Shared globals only when current: a late abandoned task must not
     * clobber a newer stream's base, render pointer, state or signals. */
    if (mygen == s_open_gen) {
        s_render = NULL;
        AHIBase = NULL;
    }
    if (mygen != s_open_gen)
        return; /* abandoned: the GUI already failed over; touch nothing shared */
    /* Forbid so the parent cannot unload our code before we return; the
     * task's exit breaks it (exit path only, never the render path). */
    Forbid();
    lv->state = (lv->state == 0 || lv->state == 1) && lv->err ? -1 : 3;
    tell_parent();
}

static void wait_state(struct AuLive *lv, LONG not_state) {
    while (lv->state == not_state)
        Wait(1UL << s_parent_sig);
}

int au_live_open(struct AuLive *lv, ULONG frames, ULONG want_rate) {
    struct Process *p;
    struct MsgPort *tport = NULL;
    struct timerequest *treq = NULL;
    int timer_ok = 0;
    if (!lv || frames < 64u || frames > AU_LIVE_MAXFRAMES || s_lv)
        return 2;
    lv->frames = frames;
    lv->want_rate = want_rate;
    lv->mix_freq = 0u;
    lv->mode_id = 0u;
    lv->xruns = 0u;
    lv->buffers = 0u;
    lv->render_us_max = 0u;
    lv->render_us_sum_ms = 0u;
    lv->period_us = 0u;
    s_render_us_acc = 0u;
    lv->cmd = AU_LIVE_CMD_NONE;
    lv->cap_buf = NULL;
    lv->cap_max = 0u;
    lv->cap_pos = 0u;
    lv->cap_on = 0;
    lv->state = 0;
    lv->err = 0;
    lv->session = NULL;
    s_parent = FindTask(NULL);
    s_parent_sig = AllocSignal(-1);
    if (s_parent_sig < 0)
        return 6;
    /* Handshake timer first: a broken audio driver must surface as err 7
     * (null fallback), never wedge the app in wait_state. */
    tport = CreateMsgPort();
    if (tport)
        treq = (struct timerequest *)CreateIORequest(tport, sizeof(struct timerequest));
    if (treq && OpenDevice((STRPTR)"timer.device", UNIT_MICROHZ,
        (struct IORequest *)treq, 0) == 0)
        timer_ok = 1;
    if (!timer_ok) {
        if (treq)
            DeleteIORequest((struct IORequest *)treq);
        if (tport)
            DeleteMsgPort(tport);
        FreeSignal(s_parent_sig);
        s_parent_sig = -1;
        lv->err = 6;
        return 6;
    }
    s_open_gen++;
    s_lv = lv;
    s_hook_count = 0u;
    p = CreateNewProcTags(NP_Entry, (IPTR)live_task, NP_Name, (IPTR)"RIAPP render",
        NP_Priority, 10, NP_StackSize, 32768, TAG_DONE);
    if (!p) {
        lv->err = 6;
        CloseDevice((struct IORequest *)treq);
        DeleteIORequest((struct IORequest *)treq);
        DeleteMsgPort(tport);
        FreeSignal(s_parent_sig);
        s_parent_sig = -1;
        s_lv = NULL;
        return 6;
    }
    treq->tr_node.io_Command = TR_ADDREQUEST;
    treq->tr_time.tv_secs = AU_LIVE_OPEN_TIMEOUT_S;
    treq->tr_time.tv_micro = 0;
    SendIO((struct IORequest *)treq);
    while (lv->state == 0) {
        ULONG sigs = Wait((1UL << s_parent_sig) | (1UL << tport->mp_SigBit));
        if (sigs & (1UL << tport->mp_SigBit))
            break;
    }
    if (!CheckIO((struct IORequest *)treq))
        AbortIO((struct IORequest *)treq);
    WaitIO((struct IORequest *)treq);
    CloseDevice((struct IORequest *)treq);
    DeleteIORequest((struct IORequest *)treq);
    DeleteMsgPort(tport);
    if (lv->state == 0) {
        /* Timed out: abandon (the task unwinds quietly on its generation)
         * and fail over to the null backend. */
        s_open_gen++;
        FreeSignal(s_parent_sig);
        s_parent_sig = -1;
        s_lv = NULL;
        lv->err = 7;
        return 7;
    }
    if (lv->state != 1) {
        wait_state(lv, -2); /* already ended (-1) */
        FreeSignal(s_parent_sig);
        s_parent_sig = -1;
        s_lv = NULL;
        return lv->err ? (int)lv->err : 7;
    }
    return 0;
}

int au_live_run(struct AuLive *lv, struct RILiveSession *s) {
    if (!lv || lv != s_lv || lv->state != 1 || !s || !s_render)
        return 2;
    lv->session = s;
    Signal(s_render, SIGBREAKF_CTRL_E);
    wait_state(lv, 1);
    return lv->state == 2 ? 0 : 3;
}

void au_live_request(struct AuLive *lv, LONG cmd) {
    if (lv)
        lv->cmd = cmd;
}

void au_live_close(struct AuLive *lv) {
    if (!lv || lv != s_lv)
        return;
    if (lv->state == 1 || lv->state == 2) {
        Forbid();
        if (s_render)
            Signal(s_render, SIGBREAKF_CTRL_C);
        Permit();
        while (lv->state != 3 && lv->state != -1)
            Wait(1UL << s_parent_sig);
    }
    FreeSignal(s_parent_sig);
    s_parent_sig = -1;
    s_lv = NULL;
}
