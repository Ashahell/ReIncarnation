/* t1_rel.c — Task 14 host test (gate G14): ARexx dispatch + catalog.
 * Pure modules only (project/arexx_dispatch.c, gui/catalog.c). The
 * AROS shells (arexx_aros.c, camd open/close, datatype registration)
 * are compile-only + audit-grepped; their RUNTIME acceptance on the
 * box is deferred with method in docs/evidence/formats/beta-exit.md.
 */
#include <stdio.h>
#include <string.h>
#include "tests/helpers/ri_assert.h"
#include "project/arexx.h"
#include "project/arexx_dispatch.h"
#include "gui/catalog.h"

int main(void) {
    struct RIArexxCmd cmd;
    struct RIArexxReply rep;
    const char *s;

    /* --- dispatch: all five commands --- */
    RI_ASSERT(arexx_parse("PLAY", &cmd) == 0, "parse PLAY");
    arexx_dispatch(&cmd, &rep);
    RI_ASSERT(rep.rc == RIAREXX_RC_OK, "PLAY rc %d", rep.rc);
    RI_ASSERT(strcmp(rep.text, "OK PLAYING") == 0, "PLAY text '%s'", rep.text);

    RI_ASSERT(arexx_parse("STOP", &cmd) == 0, "parse STOP");
    arexx_dispatch(&cmd, &rep);
    RI_ASSERT(rep.rc == RIAREXX_RC_OK, "STOP rc");
    RI_ASSERT(strcmp(rep.text, "OK STOPPED") == 0, "STOP text '%s'", rep.text);

    RI_ASSERT(arexx_parse("OPENSONG Work:tune.rbng", &cmd) == 0, "parse OPENSONG");
    arexx_dispatch(&cmd, &rep);
    RI_ASSERT(rep.rc == RIAREXX_RC_OK, "OPENSONG rc");
    RI_ASSERT(strcmp(rep.text, "OK OPEN Work:tune.rbng") == 0, "OPENSONG text '%s'", rep.text);

    RI_ASSERT(arexx_parse("EXPORTWAV Work:t.rbng Work:t.wav", &cmd) == 0, "parse EXPORTWAV");
    arexx_dispatch(&cmd, &rep);
    RI_ASSERT(rep.rc == RIAREXX_RC_OK, "EXPORTWAV rc");
    RI_ASSERT(strcmp(rep.text, "OK EXPORT Work:t.rbng Work:t.wav") == 0, "EXPORTWAV text '%s'", rep.text);

    RI_ASSERT(arexx_parse("SETRPPARAM 768 100", &cmd) == 0, "parse SETRPPARAM");
    RI_ASSERT(cmd.ctl == 768u && cmd.val == 100u, "SETRPPARAM fields %u %u", cmd.ctl, cmd.val);
    arexx_dispatch(&cmd, &rep);
    RI_ASSERT(rep.rc == RIAREXX_RC_OK, "SETRPPARAM rc");
    RI_ASSERT(strcmp(rep.text, "OK PARAM 768 100") == 0, "SETRPPARAM text '%s'", rep.text);

    /* port-prefix alias still dispatches */
    RI_ASSERT(arexx_parse("REBIRTHAROS PLAY", &cmd) == 0, "parse alias");
    arexx_dispatch(&cmd, &rep);
    RI_ASSERT(rep.rc == RIAREXX_RC_OK && strcmp(rep.text, "OK PLAYING") == 0, "alias dispatch");

    /* --- dispatch: fail-closed --- */
    memset(&cmd, 0, sizeof cmd);
    cmd.cmd = 99;
    arexx_dispatch(&cmd, &rep);
    RI_ASSERT(rep.rc == RIAREXX_RC_BADCMD, "unknown id rc %d", rep.rc);
    RI_ASSERT(strncmp(rep.text, "ERROR", 5) == 0, "unknown id text");
    arexx_dispatch(0, &rep);
    RI_ASSERT(rep.rc == RIAREXX_RC_BADCMD, "NULL cmd rc");
    memset(&cmd, 0, sizeof cmd);
    cmd.cmd = RIAREXX_SETRPPARAM;
    cmd.ctl = 1u;
    cmd.val = 200u; /* parser would never emit this; dispatch re-checks */
    arexx_dispatch(&cmd, &rep);
    RI_ASSERT(rep.rc == RIAREXX_RC_BADARG, "val>127 rc %d", rep.rc);
    arexx_dispatch(&cmd, 0); /* NULL rep: must not crash */

    /* --- catalog: DE proof string renders through the lookup --- */
    s = ri_catalog_get("DE", "MSG_PLAY");
    RI_ASSERT(strcmp(s, "Abspielen") == 0, "DE PLAY '%s'", s);
    printf("RENDERED-DE: %s\n", s); /* acceptance re-run greps this line */
    s = ri_catalog_get("EN", "MSG_PLAY");
    RI_ASSERT(strcmp(s, "Play") == 0, "EN PLAY '%s'", s);
    s = ri_catalog_get("DE", "MSG_TEMPO"); /* untranslated → EN fallback */
    RI_ASSERT(strcmp(s, "Tempo") == 0, "DE fallback '%s'", s);
    s = ri_catalog_get("FR", "MSG_STOP"); /* unknown locale → EN */
    RI_ASSERT(strcmp(s, "Stop") == 0, "FR locale '%s'", s);
    s = ri_catalog_get("DE", "MSG_NOPE"); /* unknown msgid → echo */
    RI_ASSERT(strcmp(s, "MSG_NOPE") == 0, "unknown msgid '%s'", s);
    s = ri_catalog_get(0, "MSG_PLAY"); /* NULL locale → EN */
    RI_ASSERT(strcmp(s, "Play") == 0, "NULL locale '%s'", s);

    RI_RESULT("rel");
}
