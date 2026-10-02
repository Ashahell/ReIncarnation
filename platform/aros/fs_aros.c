/* fs_aros.c — AROS backend for ri_pal_fs (portability plan T6).
 * AROS-only. Paths: MODS = SYS:Classes/ReIncarnation/Mods/,
 * TEMP = the first mounted USB/stick volume, else RAM: (the log must
 * survive a reboot), PREFS = ENVARC:ReIncarnation/ (SONGS = MODS sibling).
 * Dir scan keeps the fixed ExAll walk (ED_TYPE, ed_Next, ExAllEnd).
 */
#ifndef __AROS__
#error "fs_aros.c is AROS-only (portability plan T6)"
#endif

#include "platform/pal/ri_pal_fs.h"
#include "platform/pal/ri_pal_sticky.h"

#include <exec/types.h>
#include <dos/dos.h>
#include <dos/exall.h>
#include <proto/dos.h>
#include <proto/exec.h>

#include <string.h>

/* First mounted sticky volume, else "" when there is none. Never opens a
 * requester and never allocates. The candidate list is in
 * platform/pal/ri_pal_sticky.h so the host test can read the same one. */
int ri_pal_sticky_vol(char *out, uint32_t cap) {
    uint32_t i, k;
    struct Process *me;
    APTR oldwin;
    if (!out || cap == 0u)
        return 1;
    out[0] = 0;
    if (!DOSBase)
        return 1;
    me = (struct Process *)FindTask(NULL);
    for (i = 0u; i < RI_PAL_STICKY_COUNT; i++) {
        BPTR lock;
        oldwin = me ? me->pr_WindowPtr : 0;
        if (me)
            me->pr_WindowPtr = (APTR)-1; /* a missing volume must not raise
            a requester: the Dell boots unattended */
        lock = Lock((CONST_STRPTR)ri_pal_sticky_vols[i], ACCESS_READ);
        if (me)
            me->pr_WindowPtr = oldwin;
        if (!lock)
            continue;
        UnLock(lock);
        for (k = 0u; ri_pal_sticky_vols[i][k] && k + 1u < cap; k++)
            out[k] = ri_pal_sticky_vols[i][k];
        out[k] = 0;
        return 0;
    }
    return 1;
}

int ri_pal_path(enum ri_path p, char *out, uint32_t cap) {
    const char *s = 0;
    uint32_t i = 0u;
    if (!out || cap == 0u)
        return 1;
    switch (p) {
    case RI_PATH_MODS: s = "SYS:Classes/ReIncarnation/Mods/"; break;
    case RI_PATH_SONGS: s = "SYS:Classes/ReIncarnation/Songs/"; break;
    /* The log is evidence and RAM: is wiped by every reboot, so TEMP is
     * the durable volume when one is present. No stick, no scratch disk:
     * a machine with neither still logs somewhere rather than not at all. */
    case RI_PATH_TEMP:
        if (ri_pal_sticky_vol(out, cap) == 0)
            return 0;
        s = RI_PAL_STICKY_FALLBACK;
        break;
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
    /* ULONG-aligned ExAll buffer (sectproof's proven shape): a UBYTE
     * array risks unaligned ExAllData access on the device. */
    static ULONG exbuf[256];
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
    /* Process entries BEFORE testing the return: ExAll returns FALSE
     * with entries pending + IoErr 0 when nothing more follows (seen on
     * device: FALSE + 3 entries). Same shape as sectproof's walker. */
    {
        int more;
        do {
            more = ExAll(lock, (struct ExAllData *)exbuf, sizeof(exbuf),
                ED_TYPE, eac);
            if (!more && IoErr() != ERROR_NO_MORE_ENTRIES) {
                rc = 1;
                break;
            }
            if (eac->eac_Entries == 0)
                continue;
            ead = (struct ExAllData *)exbuf;
            while (ead) {
                if (ead->ed_Type > 0) {
                    rc = cb(u, ead->ed_Name);
                    if (rc != 0) {
                        ExAllEnd(lock, (struct ExAllData *)exbuf, sizeof(exbuf), ED_TYPE, eac);
                        FreeDosObject(DOS_EXALLCONTROL, eac);
                        UnLock(lock);
                        return rc;
                    }
                }
                ead = ead->ed_Next;
            }
        } while (more);
    }
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
