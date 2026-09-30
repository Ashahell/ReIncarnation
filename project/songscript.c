/* songscript.c — RBS song script parser (see songscript.h). */
#include "project/songscript.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define RBS_MAXTOK 24

static const char *const L808[RI_DRUM_CLASSIC_LANES] = { "BD", "SD", "LT", "MT", "HT", "RS", "CP", "CB", "CY",
    "OH", "CH" };
static const char *const L909[RI_DRUM_CLASSIC_LANES] = { "BD", "SD", "LT", "MT", "HT", "RS", "CP", "CH", "OH",
    "CC", "RC" };

static int fail(char *err, uint32_t cap, uint32_t line, const char *why) {
    if (err && cap)
        snprintf(err, cap, "line %u: %s", (unsigned)line, why);
    return 2;
}

static int num(const char *t, long lo, long hi, long *out) {
    char *e = 0;
    long v;
    if (!t || !*t)
        return 0;
    v = strtol(t, &e, 0);
    if (!e || *e || v < lo || v > hi)
        return 0;
    *out = v;
    return 1;
}

/* Split a line in place into blank-separated tokens (at most RBS_MAXTOK). */
static uint32_t split(char *s, char **tok) {
    uint32_t n = 0u;
    while (*s && n < RBS_MAXTOK) {
        while (*s == ' ' || *s == '\t')
            s++;
        if (!*s)
            break;
        tok[n++] = s;
        while (*s && *s != ' ' && *s != '\t')
            s++;
        if (*s)
            *s++ = '\0';
    }
    return n;
}

static int auto_cmp(const void *a, const void *b) {
    const struct RBAutoEv *x = (const struct RBAutoEv *)a, *y = (const struct RBAutoEv *)b;
    if (x->tick != y->tick)
        return x->tick < y->tick ? -1 : 1;
    if (x->ctl != y->ctl)
        return x->ctl < y->ctl ? -1 : 1;
    return 0;
}

static int p303(struct RISong *s, char **tok, uint32_t n) {
    long inst, slot, v;
    uint32_t i;
    struct RIPattern *p;
    if (n < 4u || n > 19u || !num(tok[1], 0, 1, &inst) || !num(tok[2], 0, 31, &slot))
        return 0;
    p = &s->bank[inst].pat[slot];
    ri_pattern_init(p, RI_PATTERN_KIND_303, 0u);
    ri_pattern_set_length(p, n - 3u);
    for (i = 0u; i < RI_PATTERN_STEPS; i++)
        ri_p303_set(p, i, 0u, RI_STEP_REST);
    for (i = 3u; i < n; i++) {
        char buf[16], *e;
        uint8_t fl = 0u, key, oct;
        int semi, folded;
        size_t L;
        if (!strcmp(tok[i], "-"))
            continue;
        if (strlen(tok[i]) >= sizeof buf)
            return 0;
        strcpy(buf, tok[i]);
        L = strlen(buf);
        while (L && (buf[L - 1u] == 'a' || buf[L - 1u] == 's')) {
            fl |= buf[L - 1u] == 'a' ? (uint8_t)RI_STEP_ACCENT : (uint8_t)RI_STEP_SLIDE;
            buf[--L] = '\0';
        }
        v = strtol(buf, &e, 10);
        if (!L || *e || v < 0 || v > 127)
            return 0;
        semi = ri_p303_fold((int)v - (int)RI_303_BASE_NOTE, &folded);   /* octave-fold into the 303 range */
        ri_p303_encode(semi, &key, &oct);
        if (ri_p303_set(p, i - 3u, key, (uint8_t)(fl | oct)) != 0)
            return 0;
    }
    return 1;
}

static int lane_of(uint32_t inst, const char *name) {
    uint32_t i;
    for (i = 0u; i < RI_DRUM_CLASSIC_LANES; i++)
        if (!strcmp(name, inst == 2u ? L808[i] : L909[i]))
            return (int)i;
    return -1;
}

static int pdrum(struct RISong *s, char **tok, uint32_t n, int acc) {
    long inst, slot;
    int lane = 0;
    const char *row;
    uint32_t i, len;
    struct RIPattern *p;
    if (n != (acc ? 4u : 5u) || !num(tok[1], 2, 3, &inst) || !num(tok[2], 0, 31, &slot))
        return 0;
    if (!acc && (lane = lane_of((uint32_t)inst, tok[3])) < 0)
        return 0;
    row = tok[acc ? 3 : 4];
    len = (uint32_t)strlen(row);
    if (len < 1u || len > RI_PATTERN_STEPS)
        return 0;
    p = &s->bank[inst].pat[slot];
    if (p->length != len)
        ri_pattern_set_length(p, len);
    for (i = 0u; i < len; i++) {
        char c = row[i];
        if (acc) {
            if (c != '.' && c != 'x')
                return 0;
            ri_pdrum_set_ac(p, i, c == 'x');
        } else {
            uint32_t st = c == '.' ? RI_HIT_OFF : c == 'x' ? RI_HIT_LOW : c == 'X' ? RI_HIT_HIGH
                : c == 'f' ? RI_HIT_FLAM : 99u;
            if (st == 99u || ri_pdrum_set(p, i, (uint32_t)lane, st) != 0)
                return 0;
        }
    }
    return 1;
}

static int plevi(struct RISong *s, char **tok, uint32_t n) {
    long slot, len, step, v;
    char buf[64], *q, *e;
    uint32_t lane = 0u, L;
    struct RIPattern *p;
    if (n != 5u || !num(tok[1], 0, 31, &slot) || !num(tok[2], 1, 16, &len) || !num(tok[3], 0, 15, &step) ||
        step >= len || strlen(tok[4]) >= sizeof buf)
        return 0;
    p = &s->bank[4].pat[slot];
    ri_pattern_set_length(p, (uint32_t)len);
    for (L = 0u; L < RI_LEVI_LANES; L++)
        ri_levi_set(p, (uint32_t)step, L, 0u, 0);
    strcpy(buf, tok[4]);
    for (q = strtok(buf, ","); q; q = strtok(0, ",")) {
        v = strtol(q, &e, 10);
        if (*e || v < 0 || v > 127 || lane >= RI_LEVI_LANES)
            return 0;
        ri_levi_set(p, (uint32_t)step, lane++, (uint8_t)v, 1);
    }
    return lane > 0u;
}

/* Control name match: case-insensitive, blanks and '/' as '_'. */
static int name_eq(const char *want, const char *have) {
    for (; *want && *have; want++, have++) {
        char a = *want, b = *have;
        if (a == ' ' || a == '/')
            a = '_';
        if (b == ' ' || b == '/')
            b = '_';
        if (a >= 'a' && a <= 'z')
            a = (char)(a - 32);
        if (b >= 'a' && b <= 'z')
            b = (char)(b - 32);
        if (a != b)
            return 0;
    }
    return !*want && !*have;
}

/* "<section>.<control>" -> automation key (0 when unknown). Sections: the
 * registry tokens (levi, 808, mix-levi, delay, ...) plus 303a / 303b;
 * controls: the legend, or group_legend ("BD_Level", "Cutoff"). */
static uint16_t ctl_key(const char *spec) {
    char sec[32];
    const char *dot = strchr(spec, '.');
    int si;
    uint32_t i;
    if (!dot || (size_t)(dot - spec) >= sizeof sec)
        return 0u;
    memcpy(sec, spec, (size_t)(dot - spec));
    sec[dot - spec] = '\0';
    si = !strcmp(sec, "303a") ? (int)RI_SEC_SYNTH1 : !strcmp(sec, "303b") ? (int)RI_SEC_SYNTH2
        : ri_ctlreg_section_by_token(sec);
    if (si < 0)
        return 0u;
    for (i = 0u; i < ri_ctlreg_count(); i++) {
        const struct RICtlDef *d = ri_ctlreg_at(i);
        char full[96];
        if (!d || d->section != (uint32_t)si || !d->automatable || !ri_ctlreg_auto_id(d))
            continue;
        snprintf(full, sizeof full, "%s_%s", d->group, d->legend);
        if (name_eq(dot + 1, d->legend) || (d->group[0] && name_eq(dot + 1, full)))
            return ri_ctlreg_auto_id(d);            /* first match in registry order */
    }
    return 0u;
}

static int track(struct RISong *s, char **tok, uint32_t n) {
    long from, to, v;
    uint32_t b, i;
    if (n != 8u || !num(tok[1], 0, RI_SONGTRACK_BARS - 1, &from) || !num(tok[2], from, RI_SONGTRACK_BARS - 1, &to))
        return 0;
    for (i = 0u; i < RI_SONGTRACK_INSTANCES; i++) {
        if (!strcmp(tok[3u + i], "-"))
            continue;
        if (!num(tok[3u + i], 0, RI_SONGTRACK_MAX_SLOT, &v))
            return 0;
        for (b = (uint32_t)from; b <= (uint32_t)to; b++)
            s->track.slot[b][i] = (uint8_t)v;
    }
    return 1;
}

int ri_rbs_parse(const char *text, struct RISong *s, char *err, uint32_t errcap) {
    static char line[512];
    const char *p = text;
    uint32_t ln = 0u, i, k;
    int header = 0;
    if (!text || !s)
        return fail(err, errcap, 0u, "bad args");
    s->nbanks = RI_SONGTRACK_INSTANCES;
    ri_bank_init(&s->bank[0], 0u, RI_PATTERN_KIND_303, 0u);
    ri_bank_init(&s->bank[1], 1u, RI_PATTERN_KIND_303, 0u);
    ri_bank_init(&s->bank[2], 2u, RI_PATTERN_KIND_DRUM, RI_DRUM_CLASS_808);
    ri_bank_init(&s->bank[3], 3u, RI_PATTERN_KIND_DRUM, RI_DRUM_CLASS_909);
    ri_bank_init(&s->bank[4], 4u, RI_PATTERN_KIND_LEVI, 0u);
    for (k = 0u; k < 2u; k++)                 /* a 303 slot no script names stays silent */
        for (i = 0u; i < RI_PATTERN_BANK_PATTERNS; i++) {
            uint32_t st;
            for (st = 0u; st < RI_PATTERN_STEPS; st++)
                ri_p303_set(&s->bank[k].pat[i], st, 0u, RI_STEP_REST);
        }
    ri_track_init(&s->track);
    s->natrk = 0u;
    s->nsteps = 16u;                          /* the v1.0 header field; banks carry the music */
    for (i = 0u; i < 16u; i++) {
        s->steps[i].note = 36u;
        s->steps[i].flags = RI_RBNG_REST;
    }
    while (*p) {
        char *tok[RBS_MAXTOK], *c;
        uint32_t n, L = 0u;
        long v, w, x;
        while (p[L] && p[L] != '\n')
            L++;
        ln++;
        if (L >= sizeof line)
            return fail(err, errcap, ln, "line too long");
        memcpy(line, p, L);
        line[L] = '\0';
        p += L + (p[L] == '\n');
        if ((c = strchr(line, '\r')) != 0)
            *c = '\0';
        if (strncmp(line, "CPRG ", 5) == 0) {
            size_t m = strlen(line + 5);
            if (m > RI_RBNG_MAX_CPRG)
                return fail(err, errcap, ln, "CPRG longer than 127 bytes");
            memcpy(s->cprg, line + 5, m + 1u);
            continue;
        }
        if ((c = strchr(line, '#')) != 0)
            *c = '\0';
        n = split(line, tok);
        if (!n)
            continue;
        if (!header) {
            if (n != 2u || strcmp(tok[0], "RBS") || strcmp(tok[1], "1"))
                return fail(err, errcap, ln, "expected 'RBS 1'");
            header = 1;
            continue;
        }
        if (!strcmp(tok[0], "TEMPO")) {
            if (n != 2u || !num(tok[1], 30, 300, &v))
                return fail(err, errcap, ln, "TEMPO 30..300");
            s->tempo = (uint16_t)v;
        } else if (!strcmp(tok[0], "PPQ")) {
            if (n != 2u || !num(tok[1], 24, 960, &v))
                return fail(err, errcap, ln, "PPQ 24..960");
            s->ppq = (uint16_t)v;
        } else if (!strcmp(tok[0], "P303")) {
            if (!p303(s, tok, n))
                return fail(err, errcap, ln, "P303 <0|1> <slot> <1..16 steps: - | note[a][s]>");
        } else if (!strcmp(tok[0], "PDRUM")) {
            if (!pdrum(s, tok, n, 0))
                return fail(err, errcap, ln, "PDRUM <2|3> <slot> <LANE> <.xXf chars>");
        } else if (!strcmp(tok[0], "PACC")) {
            if (!pdrum(s, tok, n, 1))
                return fail(err, errcap, ln, "PACC <2|3> <slot> <.x chars>");
        } else if (!strcmp(tok[0], "PLEVI")) {
            if (!plevi(s, tok, n))
                return fail(err, errcap, ln, "PLEVI <slot> <len> <step> <note[,note..]>");
        } else if (!strcmp(tok[0], "TRACK")) {
            if (!track(s, tok, n))
                return fail(err, errcap, ln, "TRACK <from> <to> <5 slots or ->");
        } else if (!strcmp(tok[0], "AUTO") || !strcmp(tok[0], "SET")) {
            int set = tok[0][0] == 'S';
            if (n != 4u || !num(tok[1], 0, 0x7FFFFFFF, &v) || !num(tok[3], 0, 127, &x))
                return fail(err, errcap, ln, set ? "SET <tick> <section.control> <0..127>" : "AUTO <tick> <ctl> <0..127>");
            if (set) {
                if (!(w = (long)ctl_key(tok[2])))
                    return fail(err, errcap, ln, "SET: unknown or non-automatable control");
            } else if (!num(tok[2], 0, 0xFFFF, &w)) {
                return fail(err, errcap, ln, "AUTO <tick> <ctl> <0..127>");
            }
            if (!s->atrk || s->natrk >= s->atrk_cap)
                return fail(err, errcap, ln, "AUTO: too many events");
            s->atrk[s->natrk].tick = (uint32_t)v;
            s->atrk[s->natrk].ctl = (uint16_t)w;
            s->atrk[s->natrk].val = (uint8_t)x;
            s->natrk++;
        } else {
            return fail(err, errcap, ln, "unknown statement");
        }
    }
    if (!header)
        return fail(err, errcap, ln, "empty script");
    /* Stable sort by (tick, ctl), then keep the last of each (tick, ctl)
     * group: the later line wins. */
    for (i = 1u; i < s->natrk; i++) {
        struct RBAutoEv e = s->atrk[i];
        k = i;
        while (k > 0u && auto_cmp(&s->atrk[k - 1u], &e) > 0) {
            s->atrk[k] = s->atrk[k - 1u];
            k--;
        }
        s->atrk[k] = e;
    }
    if (s->natrk > 1u) {
        uint32_t o = 0u;
        for (i = 0u; i < s->natrk; i++) {
            if (i + 1u < s->natrk && !auto_cmp(&s->atrk[i], &s->atrk[i + 1u]))
                continue;
            s->atrk[o++] = s->atrk[i];
        }
        s->natrk = o;
    }
    return 0;
}
