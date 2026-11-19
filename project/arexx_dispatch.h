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
#include <stdint.h>
#include <stddef.h>

#define RIAREXX_RC_OK 0
#define RIAREXX_RC_BADCMD 10
#define RIAREXX_RC_BADARG 12
#define RIAREXX_RC_FAILED 20

#define RIAREXX_REPLY_MAX 255u

struct RIArexxReply {
    int rc;
    char text[RIAREXX_REPLY_MAX + 1u];
};

/* Effect function pointers — filled by the app at startup. NULL means
 * the effect is not implemented (dispatch returns RIAREXX_RC_FAILED). */
struct RIArexxEffects {
    void (*play)(void);
    void (*stop)(void);
    int (*opensong)(const char *path);
    int (*exportwav)(const char *song, const char *wav);
    int (*setctl)(uint32_t reg_id, uint8_t value);
    int (*getctl)(uint32_t reg_id, uint8_t *value);
    int (*tempo)(uint32_t bpm);
    void (*status)(char *out, size_t outsz);
    void (*quit)(void);
};

/* Pure: fills rep from cmd + effects. NULL cmd/rep/effects, or an
 * unknown cmd id, fails closed (rc RIAREXX_RC_BADCMD, text "ERROR ...").
 * SETRPPARAM/SETCTL with val > 127 fails closed (parser guarantees it,
 * dispatch re-checks: defense in depth at the trust boundary).
 * Effects returning nonzero cause RIAREXX_RC_FAILED. */
void arexx_dispatch(const struct RIArexxCmd *cmd,
                    struct RIArexxReply *rep,
                    const struct RIArexxEffects *fx);
#endif
