/* ri_pal_fs.h — paths, dir scan, file IO (portability plan T6, §3.6).
 * C99, includes only <stdint.h> (plan §2 gate).
 * Replaces hard-coded SYS:/RAM:/ENV: literals and the ExAll walk.
 * stdio users (rbng.c, rbnm.c, pcf.c) stay on stdio; only their path
 * construction moves to ri_pal_path.
 */
#ifndef RI_PAL_FS_H
#define RI_PAL_FS_H
#include <stdint.h>

enum ri_path {
    RI_PATH_MODS = 1,   /* skins/mods: AROS SYS:Classes/ReIncarnation/Mods/ */
    RI_PATH_SONGS = 2,  /* songs */
    RI_PATH_TEMP = 3,   /* temp/wav/log: AROS RAM:, Win %TEMP% */
    RI_PATH_PREFS = 4,  /* prefs: AROS ENVARC:, Win %APPDATA% */
    RI_PATH_PACKS = 5   /* sample packs: AROS SYS:Classes/ReIncarnation/Packs/ */
};

/* Write the base dir for p into out[cap] (NUL-terminated). 0 ok. */
int ri_pal_path(enum ri_path p, char *out, uint32_t cap);
/* Join dir + leaf with the platform separator (':' vs '\\' vs '/'). 0 ok. */
int ri_pal_path_join(char *out, uint32_t cap, const char *dir, const char *leaf);
/* List subdirectories of dir via cb(name). Stops if cb returns nonzero. */
int ri_pal_list_dirs(const char *dir, int (*cb)(void *u, const char *name), void *u);
/* Whole-file helpers (bounded, caller buffer). 0 ok. */
int ri_pal_read_file(const char *path, void *buf, uint32_t cap, uint32_t *got);
int ri_pal_write_file(const char *path, const void *buf, uint32_t n);

#endif
