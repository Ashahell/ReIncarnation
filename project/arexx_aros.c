/*
 * arexx_aros.c — ARexx RexxMsg port glue (Task 14, gate G14).
 *
 * AROS-ONLY. String-level glue between the ARexx port and the pure
 * parser+dispatch (project/arexx.c + arexx_dispatch.c, host-tested):
 * ri_arexx_handle takes one command line as received on the
 * REINCARNATION port and formats the reply line the port sends back.
 * Must NEVER enter the host build (probe_ahi.c precedent: #error +
 * build-script exclusion + audit gate).
 *
 * The full port lifecycle:
 *   ri_arexx_port_create()  — creates REINCARNATION port, returns signal bit
 *   ri_arexx_port_drain()   — drains every queued message (GetMsg ->
 *                             ri_arexx_handle -> ReplyMsg)
 *   ri_arexx_port_destroy() — cleans up, replies pending with rc 20
 *
 * DEFERRED no more: the MsgPort creation + RexxPort setup + WaitPort/GetMsg
 * loop + ReplyMsg through rexxsyslib is implemented here.
 */

#ifndef __AROS__
#error "arexx_aros.c is AROS-only: RexxMsg glue, never in the host build"
#endif

#include <exec/types.h>
#include <exec/ports.h>
#include <exec/memory.h>
#include <exec/libraries.h>
#include <dos/dos.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/rexxsyslib.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <inline/rexxsyslib.h>
#include "project/arexx.h"
#include "project/arexx_dispatch.h"

/* Forward declaration */
LONG ri_arexx_handle(const STRPTR line, STRPTR reply, ULONG replysz);

/* Global port state */
static struct MsgPort *g_arexx_port = 0;
static struct Library *g_rexxsyslib = 0;
/* Inline header expects RexxSysBase to be defined as the library base */
#define RexxSysBase g_rexxsyslib
static struct RIArexxEffects g_arexx_effects = {0};

/* Open rexxsyslib and create the REINCARNATION port.
 * Returns the signal bit (0..31) on success, -1 on failure.
 * Fails if port name is already taken. */
int ri_arexx_port_create(const struct RIArexxEffects *effects) {
    if (g_arexx_port || g_rexxsyslib)
        return -1;
    if (!effects)
        return -1;

    g_rexxsyslib = OpenLibrary((CONST_STRPTR)"rexxsyslib.library", 0);
    if (!g_rexxsyslib)
        return -1;

    g_arexx_port = CreateMsgPort();
    if (!g_arexx_port) {
        CloseLibrary(g_rexxsyslib);
        g_rexxsyslib = 0;
        return -1;
    }
    g_arexx_port->mp_Node.ln_Name = "REINCARNATION";
    g_arexx_port->mp_Node.ln_Type = NT_MSGPORT;
    /* mp_Flags stays PA_SIGNAL (CreateMsgPort): the signal is what wakes
     * RIAPP's Wait(). PA_IGNORE here left every command unanswered. */

    Forbid();
    if (FindPort((CONST_STRPTR)"REINCARNATION")) {
        Permit();
        DeleteMsgPort(g_arexx_port);
        g_arexx_port = 0;
        CloseLibrary(g_rexxsyslib);
        g_rexxsyslib = 0;
        return -1;
    }
    AddPort(g_arexx_port);
    Permit();

    g_arexx_effects = *effects;
    return (int)g_arexx_port->mp_SigBit;
}

/* Answer one RexxMsg. ARexx law (Zune application.c does the same): the
 * command string is rm_Args[0]; rm_Result1 is the rc; rm_Result2 is an
 * Argstring only when the sender asked (RXFF_RESULT) and rc is 0. */
static void arexx_answer(struct RexxMsg *rxm) {
    UBYTE reply[RIAREXX_REPLY_MAX + 1u];
    const STRPTR line = rxm->rm_Args[0] ? (STRPTR)rxm->rm_Args[0] : (STRPTR)"";
    LONG rc = ri_arexx_handle(line, reply, sizeof reply);
    rxm->rm_Result1 = rc;
    rxm->rm_Result2 = 0;
    if (rc == 0 && (rxm->rm_Action & RXFF_RESULT))
        rxm->rm_Result2 = (IPTR)CreateArgstring(reply, strlen((const char *)reply));
}

/* Drain every queued message (one signal can cover several).
 * Returns the number answered, -1 when there is no port. */
int ri_arexx_port_drain(void) {
    struct Message *m;
    int n = 0;
    if (!g_arexx_port)
        return -1;
    while ((m = GetMsg(g_arexx_port)) != NULL) {
        if (IsRexxMsg((struct RexxMsg *)m))
            arexx_answer((struct RexxMsg *)m);
        ReplyMsg(m);
        n++;
    }
    return n;
}

/* Cleanup: reply pending messages with rc 20, remove port, close library. */
void ri_arexx_port_destroy(void) {
    struct RexxMsg *rxm = 0;
    if (!g_arexx_port)
        return;

    Forbid();
    RemPort(g_arexx_port);
    Permit();

    while ((rxm = (struct RexxMsg *)GetMsg(g_arexx_port))) {
        if (IsRexxMsg(rxm)) {
            rxm->rm_Result1 = RIAREXX_RC_FAILED;
            rxm->rm_Result2 = 0;
        }
        ReplyMsg((struct Message *)rxm);
    }
    DeleteMsgPort(g_arexx_port);
    g_arexx_port = 0;

    if (g_rexxsyslib) {
        CloseLibrary(g_rexxsyslib);
        g_rexxsyslib = 0;
    }
    memset(&g_arexx_effects, 0, sizeof g_arexx_effects);
}

/* Handle one port line. Returns the reply rc (RIAREXX_RC_*); reply
 * always NUL-terminated within replysz (truncated, never overflowed).
 * A line the parser rejects replies "ERROR ..." (rc 10): the port
 * never hangs a sender. */
LONG ri_arexx_handle(const STRPTR line, STRPTR reply, ULONG replysz) {
    struct RIArexxCmd cmd;
    struct RIArexxReply rep;
    int prc;
    if (!reply || replysz == 0u)
        return (LONG)RIAREXX_RC_BADCMD;
    reply[0] = '\0';
    if (!line)
        return (LONG)RIAREXX_RC_BADCMD;
    prc = arexx_parse((const char *)line, &cmd);
    if (prc != 0) {
        snprintf((char *)reply, replysz, "ERROR bad command");
        return (LONG)RIAREXX_RC_BADCMD;
    }
    arexx_dispatch(&cmd, &rep, &g_arexx_effects);
    snprintf((char *)reply, replysz, "%s", rep.text);
    return (LONG)rep.rc;
}

/* Fill the effect function pointers from the app. */
void ri_arexx_set_effects(const struct RIArexxEffects *effects) {
    if (effects)
        g_arexx_effects = *effects;
}