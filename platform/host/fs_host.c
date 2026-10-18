/* fs_host.c — host (CI) backend for ri_pal_fs (portability plan T6).
 * Paths: MODS/S description...
 * Uses stdio + POSIX dirent (host-only, never in the portable core).
 */
#include "platform/pal/ri_pal_fs.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <dirent.h>
#include <time.h>
#include <errno.h>
#include <unistd.h>
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
    case RI_PATH_RUN: b = tmp; break;   /* run state dies with the process host */
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

int ri_pal_probe_sub(const char *sub, char *out, uint32_t cap) {
    /* AROS-only concept: the host has one filesystem and one songs dir, so
     * there is nothing to probe. Declared here so a host build links. */
    (void)sub;
    (void)out;
    (void)cap;
    return 1;
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

/* Single-instance claim. mkdir() is the atomic test-and-set here exactly as
 * CreateDir is on AROS, so this backend exercises the real algorithm rather
 * than a mock of it -- including the stale-holder takeover, which is the
 * behaviour most likely to rot. */
/* 0 read, 1 absent (no live holder), 2 present but unreadable. Same tri-state
 * as the AROS backend -- see the comment there for why absent is stale and
 * unreadable is busy. */
static int claim_hb_read(const char *dir, long *secs) {
    char path[512];
    FILE *f;
    long v = 0;
    struct stat st;
    snprintf(path, sizeof(path), "%s/hb", dir);
    if (stat(path, &st) != 0)
        return 1;
    f = fopen(path, "rb");
    if (!f)
        return 2;
    if (fscanf(f, "%ld", &v) != 1) {
        fclose(f);
        return 2;                      /* there, but not a heartbeat */
    }
    fclose(f);
    *secs = v;
    return 0;
}

static int claim_hb_write(const char *dir) {
    char path[512];
    FILE *f;
    snprintf(path, sizeof(path), "%s/hb", dir);
    f = fopen(path, "wb");
    if (!f)
        return 1;
    fprintf(f, "%ld\n", (long)time(0));
    fclose(f);
    return 0;
}

static char s_claim_why[64];

const char *ri_pal_claim_last(void) {
    return s_claim_why;
}

int ri_pal_claim_acquire(const char *dir) {
    long stamp = 0, age;
    s_claim_why[0] = '\0';
    if (!dir || !dir[0]) {
        snprintf(s_claim_why, sizeof s_claim_why, "no path");
        return 2;
    }
    if (mkdir(dir, 0700) == 0) {
        if (claim_hb_write(dir)) {
            snprintf(s_claim_why, sizeof s_claim_why, "taken but heartbeat unwritable");
            return 2;
        }
        snprintf(s_claim_why, sizeof s_claim_why, "taken");
        return 0;
    }
    if (errno != EEXIST) {
        snprintf(s_claim_why, sizeof s_claim_why, "undecidable (errno %d)", errno);
        return 2;                      /* parent missing, no permission */
    }
    {
        int hb = claim_hb_read(dir, &stamp);
        if (hb == 2) {
            snprintf(s_claim_why, sizeof s_claim_why, "held, heartbeat unreadable -> busy");
            return 1;                  /* cannot tell -> do not evict */
        }
        if (hb == 1) {
            snprintf(s_claim_why, sizeof s_claim_why, "undated (no heartbeat) -> takeover");
            goto takeover;            /* nobody is heartbeating */
        }
    }
    age = (long)time(0) - stamp;
    if (age < 0)
        age = 0;                       /* clock moved: never read as stale */
    if (age <= (long)RI_CLAIM_STALE_SEC) {
        snprintf(s_claim_why, sizeof s_claim_why, "held, heartbeat %ld s old -> busy", age);
        return 1;                      /* a live holder is refreshing it */
    }
    snprintf(s_claim_why, sizeof s_claim_why, "stale (%ld s) -> takeover", age);
takeover:
    /* Stale or undated: the holder stopped refreshing (crash, or a wedged render
     * loop). Take the claim over rather than refuse to open until the next
     * reboot. Renaming the whole directory is what makes the takeover a
     * race with exactly one winner -- removing a file inside it would let
     * two contenders both decide they won. The displaced directory keeps
     * the dead holder's heartbeat next to it, so the takeover stays
     * visible on disk (this SDK has no RemoveDir, so it stays there). */
    /* Renaming the displaced claim aside is what makes the takeover a race
     * with exactly one winner. The name must not collide with an EARLIER
     * displaced claim, though: rename() onto an existing non-empty
     * directory fails, so a fixed ".dead" would make the second takeover
     * in a session always lose the race and report busy forever. */
    {
        /* One attempt with a name nothing else can be holding, for the same
         * reason as the AROS backend: a destination that already exists
         * makes the displacement fail, and retrying after losing that race
         * would steal the claim from whoever won it. Losing here is BUSY. */
        static unsigned long seq;
        char dead[560];
        snprintf(dead, sizeof(dead), "%s.dead-%ld-%lu", dir, (long)time(0),
            ++seq);
        /* Return value ignored on purpose: whether the platform reports
         * success as -1 (AmigaDOS DOSTRUE, as this AROS does for files) or
         * 0 (POSIX), the mkdir below decides who owns the claim. */
        (void)rename(dir, dead);
    }
    if (mkdir(dir, 0700) != 0)
        return 1;                      /* somebody else won the race */
    return claim_hb_write(dir) ? 2 : 0;
}

int ri_pal_claim_refresh(const char *dir) {
    if (!dir || !dir[0])
        return 2;
    if (claim_hb_write(dir)) {
        snprintf(s_claim_why, sizeof s_claim_why, "refresh failed (not the holder?)");
        return 2;
    }
    return 0;
}

void ri_pal_claim_release(const char *dir) {
    char path[512];
    if (!dir || !dir[0])
        return;
    snprintf(path, sizeof(path), "%s/hb", dir);
    unlink(path);
    rmdir(dir);
}
