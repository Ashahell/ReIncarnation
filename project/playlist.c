/* playlist.c — RBPL song playlists (see playlist.h). */
#include "project/playlist.h"

#include <stdio.h>
#include <string.h>

static int fail(char *err, uint32_t cap, uint32_t line, const char *why) {
    if (err && cap)
        snprintf(err, cap, "line %u: %s", (unsigned)line, why);
    return 2;
}

static void trim(char *s) {
    size_t n = strlen(s), a = 0u;
    while (n && (s[n - 1u] == ' ' || s[n - 1u] == '\t' || s[n - 1u] == '\r'))
        s[--n] = '\0';
    while (s[a] == ' ' || s[a] == '\t')
        a++;
    if (a)
        memmove(s, s + a, n - a + 1u);
}

static void copyz(char *dst, const char *src, uint32_t cap) {
    size_t n = strlen(src);
    if (!cap)
        return;
    if (n >= cap)
        n = cap - 1u;
    memcpy(dst, src, n);
    dst[n] = '\0';
}

void ri_playlist_dirname(const char *path, char *out, uint32_t cap) {
    const char *slash, *colon;
    size_t n = 0u;
    if (!out || !cap)
        return;
    out[0] = '\0';
    if (!path)
        return;
    slash = strrchr(path, '/');
    colon = strrchr(path, ':');
    if (slash && (!colon || slash > colon))
        n = (size_t)(slash - path);                /* without the slash */
    else if (colon)
        n = (size_t)(colon - path) + 1u;           /* AROS volume keeps its colon */
    if (n >= cap)
        n = cap - 1u;
    memcpy(out, path, n);
    out[n] = '\0';
}

/* Fold "." and ".." textually (owner 2026-10-02). AROS does not resolve a ".."
 * component in a path; the host PAL does. A playlist that reached a sibling
 * directory therefore resolved in every host test and failed on the Dell with
 * "open failed" -- and only on auto-advance past the last entry, which is why
 * it read as a working playlist (t157).
 *
 * A leading "/" and a volume ("Vk4aros:") are the root: copied first and never
 * popped. A ".." with nothing left above the root is DROPPED, the way the
 * kernel clamps at "/", and is never emitted -- a leading ".." is exactly what
 * AROS cannot resolve. No allocation: one scratch copy plus a backward scan.
 * Returns 0 ok, 2 when the result does not fit. */
static int canon(const char *in, char *out, uint32_t cap) {
    const char *p = in;
    const char *colon;
    size_t n = 0u, root = 0u;
    if (!in || !out || cap == 0u)
        return 2;
    if (*p == '/') {
        out[0] = '/';
        n = 1u;
        root = 1u;
    } else if ((colon = strchr(p, ':')) != 0) {
        size_t rl = (size_t)(colon - p) + 1u;
        if (rl >= cap)
            return 2;
        memcpy(out, p, rl);
        n = rl;
        root = rl;
        p += rl;
    }
    while (*p) {
        size_t cl = 0u;
        while (p[cl] && p[cl] != '/')
            cl++;
        if (cl == 2u && p[0] == '.' && p[1] == '.') {
            if (n > root) {                       /* pop the last component */
                size_t k = n;
                while (k > root && out[k - 1u] != '/')
                    k--;
                n = (k > root) ? k - 1u : root;
            }
        } else if (cl != 0u && !(cl == 1u && p[0] == '.')) {
            if (n > root) {
                if (n + 1u >= cap)
                    return 2;
                out[n++] = '/';
            }
            if (n + cl >= cap)
                return 2;
            memcpy(out + n, p, cl);
            n += cl;
        }
        p += cl;
        if (*p == '/')
            p++;
    }
    out[n] = '\0';
    return 0;
}

int ri_playlist_join(const char *dir, const char *rel, char *out, uint32_t cap) {
    char raw[RI_PLAYLIST_PATH];
    size_t d, r;
    int sep;
    if (!rel || !out || !cap)
        return 2;
    if (!dir || !dir[0] || rel[0] == '/' || strchr(rel, ':')) {
        if (strlen(rel) >= cap)
            return 2;
        strcpy(raw, rel);
    } else {
        d = strlen(dir);
        r = strlen(rel);
        sep = dir[d - 1u] != '/' && dir[d - 1u] != ':';
        if (d + (size_t)sep + r >= cap || d + (size_t)sep + r >= sizeof raw)
            return 2;
        memcpy(raw, dir, d);
        if (sep)
            raw[d++] = '/';
        memcpy(raw + d, rel, r + 1u);
    }
    return canon(raw, out, cap);
}

int ri_playlist_parse(const char *text, const char *dir, struct RIPlaylist *pl, char *err, uint32_t errcap) {
    char line[RI_PLAYLIST_PATH + RI_PLAYLIST_NAME + 16u];
    const char *p = text;
    uint32_t ln = 0u;
    int header = 0;
    if (!text || !pl)
        return fail(err, errcap, 0u, "bad args");
    memset(pl, 0, sizeof *pl);
    while (*p) {
        size_t L = 0u;
        char *bar;
        while (p[L] && p[L] != '\n')
            L++;
        ln++;
        if (L >= sizeof line)
            return fail(err, errcap, ln, "line too long");
        memcpy(line, p, L);
        line[L] = '\0';
        p += L + (p[L] == '\n');
        trim(line);
        if (!line[0] || line[0] == '#')
            continue;
        if (!header) {
            if (strcmp(line, "RBPL 1"))
                return fail(err, errcap, ln, "expected 'RBPL 1'");
            header = 1;
            continue;
        }
        if (!strncmp(line, "TITLE ", 6)) {
            char *t = line + 6;
            trim(t);
            copyz(pl->title, t, sizeof pl->title);
            continue;
        }
        if (pl->n >= RI_PLAYLIST_MAX)
            return fail(err, errcap, ln, "more than 64 songs");
        bar = strchr(line, '|');
        if (bar) {
            *bar = '\0';
            trim(bar + 1);
            copyz(pl->e[pl->n].name, bar + 1, sizeof pl->e[pl->n].name);
            trim(line);
        }
        if (!line[0])
            return fail(err, errcap, ln, "empty song path");
        if (ri_playlist_join(dir, line, pl->e[pl->n].path, sizeof pl->e[pl->n].path) != 0)
            return fail(err, errcap, ln, "path too long");
        pl->n++;
    }
    if (!header)
        return fail(err, errcap, ln, "empty playlist");
    if (!pl->n)
        return fail(err, errcap, ln, "no songs");
    return 0;
}

uint32_t ri_playlist_next(const struct RIPlaylist *pl, uint32_t cur) {
    if (!pl || !pl->n)
        return 0u;
    return (cur + 1u) % pl->n;
}

uint32_t ri_playlist_prev(const struct RIPlaylist *pl, uint32_t cur) {
    if (!pl || !pl->n)
        return 0u;
    return cur % pl->n ? cur % pl->n - 1u : pl->n - 1u;
}

void ri_playlist_name(const struct RIPlaylist *pl, uint32_t i, char *out, uint32_t cap) {
    const char *p, *s, *c;
    char *dot;
    if (!out || !cap)
        return;
    out[0] = '\0';
    if (!pl || i >= pl->n)
        return;
    if (pl->e[i].name[0]) {
        copyz(out, pl->e[i].name, cap);
        return;
    }
    p = pl->e[i].path;
    s = strrchr(p, '/');
    c = strrchr(p, ':');
    if (s && (!c || s > c))
        p = s + 1;
    else if (c)
        p = c + 1;
    copyz(out, p, cap);
    dot = strrchr(out, '.');
    if (dot && dot != out)
        *dot = '\0';
}
