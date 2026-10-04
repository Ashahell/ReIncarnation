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
/* Probe for <vol><sub> across the sticky candidates, in table order. Returns
 * the first that locks. Used for SONGS, which is NOT at a fixed location: the
 * Dell keeps its library on the stick (Vk4aros:ReIncarnation/songs/) and
 * riqemu1 keeps it under SYS: (SYS:Classes/ReIncarnation/Songs/). One static
 * path cannot serve both, and getting it wrong is invisible until a song fails
 * to open -- which is exactly how it presented (owner, 2026-10-04: "riapp
 * fails to open songs").
 *
 * No requester on a miss, for the same reason as ri_pal_sticky_vol: the Dell
 * boots unattended. SYS: is appended to the END of the probe list rather than
 * tried first, so a stick-mounted library wins where both exist. */
int ri_pal_probe_sub(const char *sub, char *out, uint32_t cap) {
    uint32_t i;
    struct Process *me;
    if (!out || cap == 0u || !sub)
        return 1;
    out[0] = 0;
    if (!DOSBase)
        return 1;
    me = (struct Process *)FindTask(NULL);
    /* +1 on the count: SYS: is probed last, after the whole sticky table. */
    for (i = 0u; i <= RI_PAL_STICKY_COUNT; i++) {
        const char *v = (i < RI_PAL_STICKY_COUNT) ? ri_pal_sticky_vols[i] : "SYS:";
        char cand[160];
        uint32_t vl = 0u, sl = 0u;
        BPTR lock;
        APTR oldwin;
        while (v[vl] && vl + 1u < sizeof cand)
            cand[vl] = v[vl], vl++;
        while (sub[sl] && vl + sl + 1u < sizeof cand)
            cand[vl + sl] = sub[sl], sl++;
        cand[vl + sl] = 0;
        oldwin = me ? me->pr_WindowPtr : 0;
        if (me)
            me->pr_WindowPtr = (APTR)-1; /* a miss must not raise a requester */
        lock = Lock((CONST_STRPTR)cand, ACCESS_READ);
        if (me)
            me->pr_WindowPtr = oldwin;
        if (!lock)
            continue;
        UnLock(lock);
        for (vl = 0u; vl + 1u < cap && cand[vl]; vl++)
            out[vl] = cand[vl];
        out[vl] = 0;
        return 0;
    }
    return 1;
}

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
    /* PROBED, not fixed (2026-10-04). Songs live in two different places: the
     * Dell's library is on the stick at Vk4aros:ReIncarnation/songs/, while
     * riqemu1 has SYS:Classes/ReIncarnation/Songs/. A single static path served
     * one lane and silently failed the other -- the symptom was "riapp fails to
     * open songs" with a correct file sitting there (owner, 2026-10-04).
     * Probing costs one Lock per candidate and needs no requester. */
    case RI_PATH_SONGS:
        /* The LIBRARY ROOT, and only that. Which subdirectory a given song
         * lives in is the loader's business, not the path's: returning
         * ".../songs/local/" here made the demo lookup append "local/" a
         * second time and miss a song that was present (2026-10-04). Probing
         * the deeper layout was solving the caller's problem inside the
         * callee, and the two then disagreed. */
        if (ri_pal_probe_sub("ReIncarnation/songs/", out, cap) == 0)
            return 0;
        if (ri_pal_probe_sub("Classes/ReIncarnation/Songs/", out, cap) == 0)
            return 0;
        return 1;
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
            /* eac_Entries == 0 with more == TRUE means the driver has nothing
             * pending but does not know it yet. `continue` here re-tested
             * `while (more)` and spun FOREVER: it hung RIAPP's startup on the
             * Dell's stick on 2026-10-04, silently, with no requester and no
             * crash -- the log simply stopped mid-startup. An empty page is
             * also not worth another call, so treat it as the end. */
            if (eac->eac_Entries == 0)
                break;
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
