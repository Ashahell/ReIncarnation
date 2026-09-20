/* arexx.c — exact-string ARexx parser. No allocation. */
#include "project/arexx.h"
#include <string.h>

static int eq_word(const char *a, const char *b) {
    return strcmp(a, b) == 0;
}

static int parse_uint(const char *s, uint32_t *v, uint32_t hi) {
    uint32_t acc = 0;
    if (!s || !*s)
        return 1;
    while (*s >= '0' && *s <= '9') {
        acc = acc * 10u + (uint32_t)(*s - '0');
        if (acc > hi)
            return 1;
        s++;
    }
    if (*s != '\0')
        return 1;
    *v = acc;
    return 0;
}

int arexx_parse(const char *line, struct RIArexxCmd *cmd) {
    static char w0[32], w1[RIAREXX_MAX_ARG + 1u], w2[RIAREXX_MAX_ARG + 1u];
    uint32_t nw = 0;
    const char *p;
    if (!line || !cmd)
        return 2;
    memset(cmd, 0, sizeof *cmd);
    memset(w0, 0, sizeof w0);
    memset(w1, 0, sizeof w1);
    memset(w2, 0, sizeof w2);
    /* Tokenize on single spaces: empty tokens (double space, leading /
     * trailing space) are malformed — exact strings only. */
    if (line[0] == '\0' || line[0] == ' ')
        return 1;
    p = line;
    while (*p && nw < 3u) {
        char *dst = nw == 0u ? w0 : (nw == 1u ? w1 : w2);
        uint32_t cap = nw == 0u ? (uint32_t)sizeof w0 - 1u : RIAREXX_MAX_ARG;
        uint32_t n = 0;
        while (*p && *p != ' ') {
            if (n >= cap)
                return 1;
            dst[n++] = *p++;
        }
        dst[n] = '\0';
        if (n == 0u)
            return 1; /* double space */
        nw++;
        if (*p == ' ')
            p++;
    }
    if (*p != '\0')
        return 1; /* more than 3 words */
    /* Optional port prefix: shifts words down. No valid command word
     * exceeds 10 chars, so a word that does not fit w0 cannot be one. */
    if (eq_word(w0, "REINCARNATION") || eq_word(w0, "REBIRTHAROS")) {
        uint32_t l1 = (uint32_t)strlen(w1);
        if (nw < 2u || l1 >= sizeof w0)
            return 1;
        strcpy(w0, w1);
        strcpy(w1, w2);
        memset(w2, 0, sizeof w2);
        nw--;
    }
    if (eq_word(w0, "PLAY")) {
        if (nw != 1u)
            return 1;
        cmd->cmd = RIAREXX_PLAY;
        return 0;
    }
    if (eq_word(w0, "STOP")) {
        if (nw != 1u)
            return 1;
        cmd->cmd = RIAREXX_STOP;
        return 0;
    }
    if (eq_word(w0, "OPENSONG")) {
        if (nw != 2u)
            return 1;
        cmd->cmd = RIAREXX_OPENSONG;
        strcpy(cmd->arg1, w1);
        return 0;
    }
    if (eq_word(w0, "EXPORTWAV")) {
        /* Two args packed as "song wav" would need 4 words; the exact
         * wire form is EXPORTWAV <song> <wav> = 3 words. */
        if (nw != 3u)
            return 1;
        cmd->cmd = RIAREXX_EXPORTWAV;
        strcpy(cmd->arg1, w1);
        strcpy(cmd->arg2, w2);
        return 0;
    }
    if (eq_word(w0, "SETRPPARAM")) {
        uint32_t ctl = 0, val = 0;
        if (nw != 3u)
            return 1;
        if (parse_uint(w1, &ctl, 65535u) != 0)
            return 1;
        if (parse_uint(w2, &val, 127u) != 0)
            return 1;
        cmd->cmd = RIAREXX_SETRPPARAM;
        cmd->ctl = ctl;
        cmd->val = (uint8_t)val;
        return 0;
    }
    return 1;
}
