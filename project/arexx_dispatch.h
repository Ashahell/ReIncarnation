/* arexx_dispatch.h — ARexx command dispatch to reply (Task 14, G14).
 * Pure/host-tested (no Amiga includes): maps a parsed RIArexxCmd to a
 * host-ownable effect code + human reply line. The AROS RexxMsg port
 * loop (project/arexx_aros.c) calls arexx_parse then this, then replies
 * through the message — the reply TEXT contract lives here so the host
 * suite pins it byte-exact.
 */
#ifndef RI_AREXX_DISPATCH_H
#define RI_AREXX_DISPATCH_H
#include "project/arexx.h"

#define RIAREXX_RC_OK 0
#define RIAREXX_RC_BADCMD 10
#define RIAREXX_RC_BADARG 12

#define RIAREXX_REPLY_MAX 255u

struct RIArexxReply {
    int rc;
    char text[RIAREXX_REPLY_MAX + 1u];
};

/* Pure: fills rep from cmd. NULL cmd/rep, or an unknown cmd id, fails
 * closed (rc RIAREXX_RC_BADCMD, text "ERROR ..."). SETRPPARAM with
 * val > 127 fails closed (parser guarantees it, dispatch re-checks:
 * defense in depth at the trust boundary). */
void arexx_dispatch(const struct RIArexxCmd *cmd, struct RIArexxReply *rep);
#endif
