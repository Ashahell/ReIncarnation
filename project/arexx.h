/* arexx.h — ARexx command parser (Task 13, gate G13).
 * Spec §1: primary port ADDRESS REINCARNATION; the parser accepts
 * REBIRTHAROS as a deprecated alias. Exact command strings (uppercase,
 * single-space separated):
 *   OPENSONG <path> | PLAY | STOP | EXPORTWAV <song> <wav> |
 *   SETRPPARAM <ctl> <val>   (ctl 0..65535, val 0..127)
 * Pure/host-tested; the AROS port glue (RexxMsg reply) is Task-14 app
 * wiring, not here.
 */
#ifndef RI_AREXX_H
#define RI_AREXX_H
#include <stdint.h>

#define RIAREXX_OPENSONG 1
#define RIAREXX_PLAY 2
#define RIAREXX_STOP 3
#define RIAREXX_EXPORTWAV 4
#define RIAREXX_SETRPPARAM 5

#define RIAREXX_MAX_ARG 255u

struct RIArexxCmd {
    int cmd;
    char arg1[RIAREXX_MAX_ARG + 1u];
    char arg2[RIAREXX_MAX_ARG + 1u];
    uint32_t ctl;
    uint8_t val;
};

/* Returns 0 ok (cmd filled), nonzero on any malformed input. An
 * optional leading "REINCARNATION " / "REBIRTHAROS " port token is
 * accepted and stripped; anything else verbatim fails. */
int arexx_parse(const char *line, struct RIArexxCmd *cmd);
#endif
