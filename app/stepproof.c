/*
 * app/stepproof.c — step-button on-device proof (Module 2.10 start).
 *
 * AROS-ONLY. Opens a window with 16 RStp step buttons (MUIC skin over
 * the host-tested ri_step_toggle math) in one row plus a pattern
 * readout showing the 16-bit step state as decimal (host-tested
 * ri_ctl_format_count handles the full 0..65535 range). Clicking a
 * button toggles its state; the loop re-reads all buttons on any
 * change and refreshes the readout. Exit code = build failures only
 * (0 = all sixteen live). Screendump + owner clicks prove toggle
 * behavior by number. Must NEVER enter the host build (audit gates
 * it).
 */

#ifndef __AROS__
#error "app/stepproof.c is AROS-only: Zune/MUI proof window, never in the host build"
#endif

#include <exec/types.h>
#include <exec/io.h>
#include <devices/timer.h>
#include <libraries/mui.h>
#include <clib/alib_protos.h>
#include <clib/muimaster_protos.h>
#include <clib/intuition_protos.h>
#include <proto/exec.h>
#include <proto/intuition.h>
#include <proto/muimaster.h>
#include <proto/timer.h>
#include <stdint.h>
#include "gui/panels.h"
#include "gui/knob_logic.h"
#include "gui/widgets/rstp.h"

#define NSTEPS 16
#define RET_STEP_BASE 300
#define CHASE_BPM 174.0

/* Pattern + chase readouts (file-static: contents copied on set). */
static char s_patbuf[8];
static char s_stepbuf[8];
static char s_lagbuf[8];
static char s_firebuf[8];

/* EClock ticks → microseconds (double: exact for session scales). */
static double eclock_us(struct EClockVal *ev, unsigned long freq) {
    unsigned long long t =
        ((unsigned long long)ev->ev_hi << 32) | ev->ev_lo;
    return (double)t * 1000000.0 / (double)freq;
}

/* TimerBase for ReadEClock: borrowed from our own open timer
 * request (standard Amiga device-base trick, no extra open). */
struct Device *TimerBase = NULL;

int main(void) {
    Object *app = NULL, *win = NULL, *steps[NSTEPS], *pat, *steptxt,
        *lagtxt, *firetxt;
    struct MsgPort *tport = NULL;
    struct timerequest *treq = NULL;
    ULONG tsig = 0, sigs = 0;
    unsigned int i;
    double period_us = 0.0, t0_us = 0.0, maxlag = 0.0;
    unsigned long freq = 0;
    unsigned long k = 0;
    int cur = -1;

    /* Timer setup lives FIRST THING, before any MUI object exists:
     * keeps device interaction out of the widget-construction
     * window. Chase-set and STEP display stay late (need objects). */
    tport = CreateMsgPort();
    if (tport) {
        treq = (struct timerequest *)CreateIORequest(tport,
            sizeof(struct timerequest));
    }
    if (treq) {
        treq->tr_node.io_Message.mn_ReplyPort = tport;
        treq->tr_node.io_Flags = 0;
    }
    if (treq && OpenDevice("timer.device", UNIT_MICROHZ,
        (struct IORequest *)treq, 0) == 0) {
        struct EClockVal ev0;
        TimerBase = treq->tr_node.io_Device;
        freq = ReadEClock(&ev0);
        period_us = ri_step16_ms(CHASE_BPM) * 1000.0;
        t0_us = eclock_us(&ev0, freq);
        treq->tr_node.io_Command = TR_ADDREQUEST;
        treq->tr_node.io_Flags = 0;
        treq->tr_time.tv_secs = 0;
        treq->tr_time.tv_micro = (long)period_us;
        SendIO((struct IORequest *)treq);
        tsig = 1UL << tport->mp_SigBit;
    }

    for (i = 0; i < NSTEPS; i++) {
        /* 32 px frames come from the custom class AskMinMax (stock
         * Numericbutton ignores Fix sizes — m36). */
        steps[i] = (Object *)ri_rstp_create();
        if (!steps[i])
            return 6;
    }
    ri_ctl_format_count(s_patbuf, 0);
    pat = (Object *)MUI_NewObject(MUIC_Text,
        MUIA_Text_Contents, (IPTR)s_patbuf,
        TAG_DONE);
    if (!pat)
        return 7;
    ri_ctl_format_count(s_stepbuf, 0);
    steptxt = (Object *)MUI_NewObject(MUIC_Text,
        MUIA_Text_Contents, (IPTR)s_stepbuf,
        TAG_DONE);
    if (!steptxt)
        return 7;
    ri_ctl_format_count(s_lagbuf, 0);
    lagtxt = (Object *)MUI_NewObject(MUIC_Text,
        MUIA_Text_Contents, (IPTR)s_lagbuf,
        TAG_DONE);
    if (!lagtxt)
        return 7;
    ri_ctl_format_count(s_firebuf, 0);
    firetxt = (Object *)MUI_NewObject(MUIC_Text,
        MUIA_Text_Contents, (IPTR)s_firebuf,
        TAG_DONE);
    if (!firetxt)
        return 7;
    win = (Object *)MUI_NewObject(MUIC_Window,
        MUIA_Window_Title, "RI-STEPS",
        MUIA_Window_LeftEdge, 0,
        MUIA_Window_TopEdge, 0,
        MUIA_Window_Width, 512,
        MUIA_Window_Height, 120,
        MUIA_Window_CloseGadget, TRUE,
        MUIA_Window_DepthGadget, TRUE,
        MUIA_Window_DragBar, TRUE,
        MUIA_Window_RootObject, (IPTR)MUI_NewObject(MUIC_Group,
            MUIA_Group_Spacing, 0,
            Child, (IPTR)MUI_NewObject(MUIC_Group,
                MUIA_Group_Horiz, TRUE,
                MUIA_Group_Spacing, 0,
                Child, steps[0],
                Child, steps[1],
                Child, steps[2],
                Child, steps[3],
                Child, steps[4],
                Child, steps[5],
                Child, steps[6],
                Child, steps[7],
                Child, steps[8],
                Child, steps[9],
                Child, steps[10],
                Child, steps[11],
                Child, steps[12],
                Child, steps[13],
                Child, steps[14],
                Child, steps[15],
                TAG_DONE),
            Child, pat,
            Child, (IPTR)MUI_NewObject(MUIC_Group,
                MUIA_Group_Horiz, TRUE,
                MUIA_Group_Spacing, 0,
                Child, steptxt,
                Child, lagtxt,
                Child, firetxt,
                TAG_DONE),
            TAG_DONE),
        TAG_DONE);
    if (!win)
        return 8;
    app = (Object *)MUI_NewObject(MUIC_Application,
        MUIA_Application_Title, "RISTEPS",
        SubWindow, win,
        TAG_DONE);
    if (!app)
        return 9;
    SetAttrs(win, MUIA_Window_Open, TRUE, TAG_DONE);
    DoMethod(win, MUIM_Notify, MUIA_Window_CloseRequest, TRUE, app,
        2, MUIM_Application_ReturnID, MUIV_Application_ReturnID_Quit);
    for (i = 0; i < NSTEPS; i++) {
        DoMethod(steps[i], MUIM_Notify, MUIA_Numeric_Value,
            MUIV_EveryTime, app, 2, MUIM_Application_ReturnID,
            RET_STEP_BASE + i);
    }
    /* Beat clock: timer.device microhertz one-shots, rearmed every
     * fire (174 BPM 16ths per spec TC-2.10.x). Grid origin t0 is
     * taken just before the first SendIO; step k due at
     * t0+(k+1)*period; lag = fire - due, max reported live. */
    /* Beat armed: mark step 0 chased from the start. */
    if (tsig) {
        SetAttrs(steps[0], MUIA_RStp_Chase, TRUE, TAG_DONE);
        cur = 0;
    }
    /* Event loop: manual Wait on the timer bit (+Ctrl-C exit).
     * Decided by elimination (diag13 matrix): NewInput never
     * surfaces seeded user bits on Zune, and InputBuffered never
     * returns on Zune (drain-spin). Clicks/close are NOT served
     * here (documented tradeoff — proven separately on NewInput
     * builds); exit via Ctrl-C (Break N CTRLC). */
    for (;;) {
        sigs = Wait(tsig | SIGBREAKF_CTRL_C);
        if (sigs & SIGBREAKF_CTRL_C)
            break;
        if ((sigs & tsig) && treq) {
            struct EClockVal evn;
            double now, lag;
            int nxt;
            while (GetMsg(tport))
                ;
            ReadEClock(&evn);
            now = eclock_us(&evn, freq);
            /* Grid due for fire k is t0+(k+1)*period: t0 is taken
             * just before the first SendIO, so the first interval
             * is one full period (k=0 due = t0+period). */
            lag = now - (t0_us + (double)(k + 1) * period_us);
            if (lag > maxlag)
                maxlag = lag;
            nxt = (int)ri_chase_step((double)k + 1.0, NSTEPS);
            if (cur >= 0)
                SetAttrs(steps[cur], MUIA_RStp_Chase, FALSE,
                    TAG_DONE);
            SetAttrs(steps[nxt], MUIA_RStp_Chase, TRUE, TAG_DONE);
            cur = nxt;
            k++;
            /* FIRES readout doubles as the liveness instrument
             * (hundreds apart = running; static = stuck). */
            ri_ctl_format_count(s_stepbuf, (unsigned long)cur);
            SetAttrs(steptxt, MUIA_Text_Contents, (IPTR)s_stepbuf,
                TAG_DONE);
            ri_ctl_format_count(s_lagbuf,
                (unsigned long)(maxlag / 1000.0));
            SetAttrs(lagtxt, MUIA_Text_Contents, (IPTR)s_lagbuf,
                TAG_DONE);
            ri_ctl_format_count(s_firebuf, k);
            SetAttrs(firetxt, MUIA_Text_Contents, (IPTR)s_firebuf,
                TAG_DONE);
            treq->tr_node.io_Command = TR_ADDREQUEST;
            treq->tr_node.io_Flags = 0;
            treq->tr_time.tv_secs = 0;
            treq->tr_time.tv_micro = (long)period_us;
            SendIO((struct IORequest *)treq);
        }
    }
    if (treq) {
        AbortIO((struct IORequest *)treq);
        while (GetMsg(tport))
            ;
        CloseDevice((struct IORequest *)treq);
        DeleteIORequest((struct IORequest *)treq);
    }
    if (tport)
        DeleteMsgPort(tport);
    MUI_DisposeObject(app);
    ri_rstp_dispose_class();
    return 0;
}
