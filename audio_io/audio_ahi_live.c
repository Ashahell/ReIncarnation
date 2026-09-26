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
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/ahi.h>
#include "audio_io/audio_ahi_live.h"
#include "engine/live.h"

struct Library *AHIBase = NULL;

static struct AuLive *s_lv;
static struct Task *s_parent;
static BYTE s_parent_sig = -1;
static struct Task *s_render;
static volatile ULONG s_hook_count;
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
    got = ri_live_render(lv->session, s_fl, s_fr, lv->frames);
    for (i = got; i < lv->frames; i++)
        s_fl[i] = s_fr[i] = 0.0f;
    for (i = 0u; i < lv->frames; i++) {
        s_pcm[half][i * 2u] = au_live_f32_to_s16(s_fl[i]);
        s_pcm[half][i * 2u + 1u] = au_live_f32_to_s16(s_fr[i]);
    }
    lv->buffers++;
}

static void live_task(void) {
    struct AuLive *lv = s_lv;
    struct MsgPort *port = NULL;
    struct AHIRequest *req = NULL;
    struct AHIAudioCtrl *actl = NULL;
    BYTE hsig = -1;
    ULONG processed = 0u, queued = 1u;
    int started = 0, dev_open = 0;

    s_render = FindTask(NULL);
    hsig = AllocSignal(-1);
    if (hsig < 0) {
        lv->err = 6;
        goto fail;
    }
    s_hook_mask = 1UL << hsig;
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
    {
        struct TagItem at[] = {
            { AHIA_AudioID, 0 }, { AHIA_MixFreq, 0 }, { AHIA_Channels, 1 },
            { AHIA_Sounds, 2 }, { AHIA_SoundFunc, 0 }, { TAG_DONE, 0 }
        };
        at[0].ti_Data = lv->mode_id;
        at[1].ti_Data = lv->want_rate;
        at[4].ti_Data = (IPTR)&s_sound_hook;
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
    AHIBase = NULL;
    if (req)
        DeleteIORequest((struct IORequest *)req);
    if (port)
        DeleteMsgPort(port);
    if (hsig >= 0)
        FreeSignal(hsig);
    s_render = NULL;
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
    if (!lv || frames < 64u || frames > AU_LIVE_MAXFRAMES || s_lv)
        return 2;
    lv->frames = frames;
    lv->want_rate = want_rate;
    lv->mix_freq = 0u;
    lv->mode_id = 0u;
    lv->xruns = 0u;
    lv->buffers = 0u;
    lv->cmd = AU_LIVE_CMD_NONE;
    lv->state = 0;
    lv->err = 0;
    lv->session = NULL;
    s_parent = FindTask(NULL);
    s_parent_sig = AllocSignal(-1);
    if (s_parent_sig < 0)
        return 6;
    s_lv = lv;
    s_hook_count = 0u;
    p = CreateNewProcTags(NP_Entry, (IPTR)live_task, NP_Name, (IPTR)"RIAPP render",
        NP_Priority, 10, NP_StackSize, 32768, TAG_DONE);
    if (!p) {
        lv->err = 6;
        FreeSignal(s_parent_sig);
        s_parent_sig = -1;
        s_lv = NULL;
        return 6;
    }
    wait_state(lv, 0);
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
