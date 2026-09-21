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
 * DEFERRED (needs the AROS run, recorded in
 * docs/evidence/formats/beta-exit.md): creating the MsgPort +
 * RexxPort (ADDRESS REINCARNATION / REBIRTHAROS alias), the
 * WaitPort/GetMsg loop, and ReplyMsg through rexxsyslib. The reply
 * TEXT contract is pinned host-side here; the port loop only moves
 * these bytes.
 */

#ifndef __AROS__
#error "arexx_aros.c is AROS-only: RexxMsg glue, never in the host build"
#endif

#include <exec/types.h>
#include <string.h>
#include <stdio.h>
#include "project/arexx.h"
#include "project/arexx_dispatch.h"

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
    prc = arexx_parse(line, &cmd);
    if (prc != 0) {
        snprintf(reply, replysz, "ERROR bad command");
        return (LONG)RIAREXX_RC_BADCMD;
    }
    arexx_dispatch(&cmd, &rep);
    snprintf(reply, replysz, "%s", rep.text);
    return (LONG)rep.rc;
}
