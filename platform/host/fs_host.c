/* fs_host.c — host (CI) backend for ri_pal_fs (portability plan T6).
 * Paths: MODS/S description...
 * Uses stdio + POSIX dirent (host-only, never in the portable core).
 */
#include "platform/pal/ri_pal_fs.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <dirent.h>
#include <sys/stat.h>

static void base_dir(enum ri_path p, char *out, uint32_t cap) {
    const char *home = getenv("HOME");
    const char *tmp = getenv("TMPDIR");
    const char *b = ".";
    if (!home || !home[0])
        home = "/tmp";
    if (!tmp || !tmp[0])
        tmp = "/tmp";
    switch (p) {
    case RI_PATH_MODS: b = "skins"; break;
    case RI_PATH_SONGS: b = "songs"; break;
    case RI_PATH_PACKS: b = "packs"; break;
    case RI_PATH_TEMP: b = tmp; break;
    case RI_PATH_PREFS: b = home; break;
    default: break;
    }
    if (p == RI_PATH_TEMP || p == RI_PATH_PREFS) {
        snprintf(out, cap ? cap : 1u, "%s", b);
    } else {
        snprintf(out, cap ? cap : 1u, "%s", b);
    }
    out[cap ? cap - 1u : 0u] = 0;
}

int ri_pal_path(enum ri_path p, char *out, uint32_t cap) {
    if (!out || cap == 0u)
        return 1;
    base_dir(p, out, cap);
    return 0;
}

int ri_pal_path_join(char *out, uint32_t cap, const char *dir, const char *leaf) {
    uint32_t d = 0u, l = 0u;
    if (!out || cap == 0u || !dir || !leaf)
        return 1;
    while (dir[d] && d + 2u < cap) {
        out[d] = dir[d];
        d++;
    }
    if (d > 0u && out[d - 1u] != '/') {
        if (d + 1u >= cap)
            return 1;
        out[d++] = '/';
    }
    while (leaf[l] && d + 1u < cap) {
        out[d++] = leaf[l++];
    }
    out[d] = 0;
    return leaf[l] ? 1 : 0;
}

int ri_pal_list_dirs(const char *dir, int (*cb)(void *u, const char *name), void *u) {
    DIR *dp;
    struct dirent *de;
    struct stat st;
    char full[1024];
    if (!dir || !cb)
        return 1;
    dp = opendir(dir);
    if (!dp)
        return 1;
    while ((de = readdir(dp)) != 0) {
        if (!strcmp(de->d_name, ".") || !strcmp(de->d_name, ".."))
            continue;
        snprintf(full, sizeof(full), "%s/%s", dir, de->d_name);
        if (stat(full, &st) == 0 && S_ISDIR(st.st_mode)) {
            int r = cb(u, de->d_name);
            if (r != 0) {
                closedir(dp);
                return r;
            }
        }
    }
    closedir(dp);
    return 0;
}

int ri_pal_read_file(const char *path, void *buf, uint32_t cap, uint32_t *got) {
    FILE *f;
    size_t n;
    if (!path || !buf || cap == 0u || !got)
        return 1;
    f = fopen(path, "rb");
    if (!f)
        return 1;
    n = fread(buf, 1u, cap, f);
    fclose(f);
    *got = (uint32_t)n;
    return 0;
}

int ri_pal_write_file(const char *path, const void *buf, uint32_t n) {
    FILE *f;
    size_t w;
    if (!path || (!buf && n > 0u))
        return 1;
    f = fopen(path, "wb");
    if (!f)
        return 1;
    w = n ? fwrite(buf, 1u, n, f) : 0u;
    fclose(f);
    return w == n ? 0 : 1;
}
