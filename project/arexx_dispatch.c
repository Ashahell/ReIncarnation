/* arexx_dispatch.c — pure ARexx dispatch (Task 14, gate G14).
 * No allocation, no libm, no platform includes: host + AROS clean.
 */
#include "project/arexx_dispatch.h"
#include <string.h>
#include <stdio.h>

void arexx_dispatch(const struct RIArexxCmd *cmd, struct RIArexxReply *rep) {
    if (!rep)
        return;
    rep->rc = RIAREXX_RC_BADCMD;
    strcpy(rep->text, "ERROR bad command");
    if (!cmd)
        return;
    switch (cmd->cmd) {
    case RIAREXX_PLAY:
        rep->rc = RIAREXX_RC_OK;
        strcpy(rep->text, "OK PLAYING");
        break;
    case RIAREXX_STOP:
        rep->rc = RIAREXX_RC_OK;
        strcpy(rep->text, "OK STOPPED");
        break;
    case RIAREXX_OPENSONG:
        if (cmd->arg1[0] == '\0') {
            rep->rc = RIAREXX_RC_BADARG;
            strcpy(rep->text, "ERROR OPENSONG needs <path>");
        } else {
            rep->rc = RIAREXX_RC_OK;
            /* Precision-bounded: the reply line is display text, so a
             * >248-char path truncates here (the command struct keeps
             * the full path for the effector). */
            snprintf(rep->text, sizeof rep->text, "OK OPEN %.247s", cmd->arg1);
        }
        break;
    case RIAREXX_EXPORTWAV:
        if (cmd->arg1[0] == '\0' || cmd->arg2[0] == '\0') {
            rep->rc = RIAREXX_RC_BADARG;
            strcpy(rep->text, "ERROR EXPORTWAV needs <song> <wav>");
        } else {
            rep->rc = RIAREXX_RC_OK;
            snprintf(rep->text, sizeof rep->text, "OK EXPORT %.120s %.120s",
                cmd->arg1, cmd->arg2);
        }
        break;
    case RIAREXX_SETRPPARAM:
        if (cmd->ctl > 65535u || cmd->val > 127u) {
            rep->rc = RIAREXX_RC_BADARG;
            strcpy(rep->text, "ERROR SETRPPARAM range");
        } else {
            rep->rc = RIAREXX_RC_OK;
            snprintf(rep->text, sizeof rep->text, "OK PARAM %u %u",
                cmd->ctl, (unsigned int)cmd->val);
        }
        break;
    default:
        break;
    }
}
