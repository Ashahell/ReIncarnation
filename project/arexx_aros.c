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
 *   ri_arexx_port_drain()   — drains one message (GetMsg -> ri_arexx_handle -> ReplyMsg)
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
    g_arexx_port->mp_Flags = PA_IGNORE;

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

/* Drain one message from the port.
 * Returns 1 if a message was processed, 0 if queue empty, -1 on error. */
int ri_arexx_port_drain(void) {
    struct RexxMsg *rxm = 0;
    if (!g_arexx_port)
        return -1;

    rxm = (struct RexxMsg *)GetMsg(g_arexx_port);
    if (!rxm)
        return 0;

    UBYTE reply[RIAREXX_REPLY_MAX + 1u];
    const STRPTR line = (rxm->rm_Args[0] && rxm->rm_Args[1])
        ? (STRPTR)rxm->rm_Args[1] : (STRPTR)"";

    ri_arexx_handle(line, reply, sizeof reply);

    LONG rc = (LONG)strtol((const char *)reply, NULL, 10); /* rc is first token */
    rxm->rm_Result1 = rc;
    if (rc >= 0 && (rxm->rm_Action & RXFF_RESULT)) {
        struct Argstring *as = (struct Argstring *)CreateArgstring(reply, strlen((const char *)reply));
        if (as) {
            rxm->rm_Result2 = (IPTR)as;
        }
    }
    ReplyMsg((struct Message *)rxm);
    return 1;
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
        rxm->rm_Result1 = RIAREXX_RC_FAILED;
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