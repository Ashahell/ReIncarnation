/* fs_aros.c — AROS backend for ri_pal_fs (portability plan T6).
 * AROS-only. Paths: MODS = SYS:Classes/ReIncarnation/Mods/,
 * TEMP = RAM:, PREFS = ENVARC:ReIncarnation/ (SONGS = MODS sibling).
 * Dir scan keeps the fixed ExAll walk (ED_TYPE, ed_Next, ExAllEnd).
 */
#ifndef __AROS__
#error "fs_aros.c is AROS-only (portability plan T6)"
#endif

#include "platform/pal/ri_pal_fs.h"

#include <exec/types.h>
#include <dos/dos.h>
#include <dos/exall.h>
#include <proto/dos.h>
#include <proto/exec.h>

#include <string.h>

int ri_pal_path(enum ri_path p, char *out, uint32_t cap) {
    const char *s = 0;
    uint32_t i = 0u;
    if (!out || cap == 0u)
        return 1;
    switch (p) {
    case RI_PATH_MODS: s = "SYS:Classes/ReIncarnation/Mods/"; break;
    case RI_PATH_SONGS: s = "SYS:Classes/ReIncarnation/Songs/"; break;
    case RI_PATH_TEMP: s = "RAM:"; break;
    case RI_PATH_PREFS: s = "ENVARC:ReIncarnation/"; break;
    case RI_PATH_PACKS: s = "SYS:Classes/ReIncarnation/Packs/"; break;
    default: break;
    }
    if (!s)
        return 1;
    while (s[i] && i + 1u < cap) {
        out[i] = s[i];
        i++;
    }
    out[i] = 0;
    return s[i] ? 1 : 0;
}

int ri_pal_path_join(char *out, uint32_t cap, const char *dir, const char *leaf) {
    uint32_t d = 0u, l = 0u;
    int need_sep;
    if (!out || cap == 0u || !dir || !leaf)
        return 1;
    while (dir[d] && d + 2u < cap) {
        out[d] = dir[d];
        d++;
    }
    if (dir[d])
        return 1;
    /* Amiga paths: volume/dir: takes no separator; plain dirs take '/'. */
    need_sep = (d > 0u && out[d - 1u] != ':' && out[d - 1u] != '/');
    if (need_sep) {
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
    BPTR lock;
    struct ExAllControl *eac;
    static UBYTE buf[1024];
    struct ExAllData *ead;
    int rc = 0;
    if (!dir || !cb)
        return 1;
    lock = Lock((CONST_STRPTR)dir, ACCESS_READ);
    if (!lock)
        return 1;
    eac = (struct ExAllControl *)AllocDosObject(DOS_EXALLCONTROL, NULL);
    if (!eac) {
        UnLock(lock);
        return 1;
    }
    eac->eac_LastKey = 0;
    do {
        if (!ExAll(lock, (struct ExAllData *)buf, sizeof(buf), ED_TYPE, eac)) {
            LONG err = IoErr();
            if (err != ERROR_NO_MORE_ENTRIES) {
                rc = 1;
                break;
            }
            break;
        }
        ead = (struct ExAllData *)buf;
        while (ead) {
            if (ead->ed_Type > 0) {
                rc = cb(u, ead->ed_Name);
                if (rc != 0) {
                    ExAllEnd(lock, (struct ExAllData *)buf, sizeof(buf), ED_TYPE, eac);
                    FreeDosObject(DOS_EXALLCONTROL, eac);
                    UnLock(lock);
                    return rc;
                }
            }
            ead = ead->ed_Next;
        }
    } while (eac->eac_Entries > 0);
    FreeDosObject(DOS_EXALLCONTROL, eac);
    UnLock(lock);
    return rc;
}

int ri_pal_read_file(const char *path, void *buf, uint32_t cap, uint32_t *got) {
    BPTR f;
    LONG n;
    if (!path || !buf || cap == 0u || !got)
        return 1;
    f = Open((CONST_STRPTR)path, MODE_OLDFILE);
    if (!f)
        return 1;
    n = Read(f, buf, cap);
    Close(f);
    if (n < 0)
        return 1;
    *got = (uint32_t)n;
    return 0;
}

int ri_pal_write_file(const char *path, const void *buf, uint32_t n) {
    BPTR f;
    LONG w;
    if (!path || (!buf && n > 0u))
        return 1;
    f = Open((CONST_STRPTR)path, MODE_NEWFILE);
    if (!f)
        return 1;
    w = n ? Write(f, (APTR)buf, n) : 0;
    Close(f);
    return w == (LONG)n ? 0 : 1;
}
