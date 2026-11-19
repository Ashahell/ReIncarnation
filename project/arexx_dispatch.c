/* arexx_dispatch.c — pure ARexx dispatch (Task 14, gate G14).
 * No allocation, no libm, no platform includes: host + AROS clean.
 */
#include "project/arexx_dispatch.h"
#include <string.h>
#include <stdio.h>

void arexx_dispatch(const struct RIArexxCmd *cmd, struct RIArexxReply *rep,
                    const struct RIArexxEffects *fx) {
    if (!rep)
        return;
    rep->rc = RIAREXX_RC_BADCMD;
    strcpy(rep->text, "ERROR bad command");
    if (!cmd || !fx)
        return;
    switch (cmd->cmd) {
    case RIAREXX_PLAY:
        if (!fx->play) { rep->rc = RIAREXX_RC_FAILED; strcpy(rep->text, "ERROR play not implemented"); break; }
        fx->play();
        rep->rc = RIAREXX_RC_OK;
        strcpy(rep->text, "OK PLAYING");
        break;
    case RIAREXX_STOP:
        if (!fx->stop) { rep->rc = RIAREXX_RC_FAILED; strcpy(rep->text, "ERROR stop not implemented"); break; }
        fx->stop();
        rep->rc = RIAREXX_RC_OK;
        strcpy(rep->text, "OK STOPPED");
        break;
    case RIAREXX_OPENSONG:
        if (!fx->opensong) { rep->rc = RIAREXX_RC_FAILED; strcpy(rep->text, "ERROR opensong not implemented"); break; }
        if (cmd->arg1[0] == '\0') {
            rep->rc = RIAREXX_RC_BADARG;
            strcpy(rep->text, "ERROR OPENSONG needs <path>");
        } else if (fx->opensong(cmd->arg1) != 0) {
            rep->rc = RIAREXX_RC_FAILED;
            strcpy(rep->text, "ERROR failed to open song");
        } else {
            rep->rc = RIAREXX_RC_OK;
            snprintf(rep->text, sizeof rep->text, "OK OPEN %.247s", cmd->arg1);
        }
        break;
    case RIAREXX_EXPORTWAV:
        if (!fx->exportwav) { rep->rc = RIAREXX_RC_FAILED; strcpy(rep->text, "ERROR exportwav not implemented"); break; }
        if (cmd->arg1[0] == '\0' || cmd->arg2[0] == '\0') {
            rep->rc = RIAREXX_RC_BADARG;
            strcpy(rep->text, "ERROR EXPORTWAV needs <song> <wav>");
        } else if (fx->exportwav(cmd->arg1, cmd->arg2) != 0) {
            rep->rc = RIAREXX_RC_FAILED;
            strcpy(rep->text, "ERROR failed to export WAV");
        } else {
            rep->rc = RIAREXX_RC_OK;
            snprintf(rep->text, sizeof rep->text, "OK EXPORT %.120s %.120s",
                cmd->arg1, cmd->arg2);
        }
        break;
    case RIAREXX_SETRPPARAM:
    case RIAREXX_SETCTL:
        if (!fx->setctl) { rep->rc = RIAREXX_RC_FAILED; strcpy(rep->text, "ERROR setctl not implemented"); break; }
        if (cmd->ctl > 65535u || cmd->val > 127u) {
            rep->rc = RIAREXX_RC_BADARG;
            strcpy(rep->text, "ERROR SETRPPARAM range");
        } else if (fx->setctl(cmd->ctl, cmd->val) != 0) {
            rep->rc = RIAREXX_RC_FAILED;
            strcpy(rep->text, "ERROR failed to set control");
        } else {
            rep->rc = RIAREXX_RC_OK;
            snprintf(rep->text, sizeof rep->text, "OK PARAM %u %u",
                cmd->ctl, (unsigned int)cmd->val);
        }
        break;
    case RIAREXX_GETCTL:
        if (!fx->getctl) { rep->rc = RIAREXX_RC_FAILED; strcpy(rep->text, "ERROR getctl not implemented"); break; }
        if (cmd->ctl > 65535u) {
            rep->rc = RIAREXX_RC_BADARG;
            strcpy(rep->text, "ERROR GETCTL range");
        } else {
            uint8_t v = 0;
            if (fx->getctl(cmd->ctl, &v) != 0) {
                rep->rc = RIAREXX_RC_FAILED;
                strcpy(rep->text, "ERROR failed to get control");
            } else {
                rep->rc = RIAREXX_RC_OK;
                snprintf(rep->text, sizeof rep->text, "OK VALUE %u %u",
                    cmd->ctl, (unsigned int)v);
            }
        }
        break;
    case RIAREXX_TEMPO:
        if (!fx->tempo) { rep->rc = RIAREXX_RC_FAILED; strcpy(rep->text, "ERROR tempo not implemented"); break; }
        if (cmd->ctl < 20u || cmd->ctl > 500u) {
            rep->rc = RIAREXX_RC_BADARG;
            strcpy(rep->text, "ERROR TEMPO range 20..500");
        } else if (fx->tempo(cmd->ctl) != 0) {
            rep->rc = RIAREXX_RC_FAILED;
            strcpy(rep->text, "ERROR failed to set tempo");
        } else {
            rep->rc = RIAREXX_RC_OK;
            snprintf(rep->text, sizeof rep->text, "OK TEMPO %u", cmd->ctl);
        }
        break;
    case RIAREXX_STATUS:
        if (!fx->status) { rep->rc = RIAREXX_RC_FAILED; strcpy(rep->text, "ERROR status not implemented"); break; }
        fx->status(rep->text, sizeof rep->text);
        rep->rc = RIAREXX_RC_OK;
        break;
    case RIAREXX_QUIT:
        if (!fx->quit) { rep->rc = RIAREXX_RC_FAILED; strcpy(rep->text, "ERROR quit not implemented"); break; }
        fx->quit();
        rep->rc = RIAREXX_RC_OK;
        strcpy(rep->text, "OK QUIT");
        break;
    default:
        break;
    }
}