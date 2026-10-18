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
#include <stdio.h>

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
    /* Run state (the instance claim) belongs on RAM: and nowhere else: the
     * claim has to disappear with the boot, not survive as a stale file on
     * a durable volume where it would keep the instrument unopenable. */
    case RI_PATH_RUN: s = "RAM:"; break;
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

/* Single-instance claim. CreateDir is the atomic test-and-set (it returns a
 * lock, or 0 with IoErr ERROR_OBJECT_EXISTS when the name is taken), and the
 * heartbeat inside is what keeps a CRASHED holder from locking the
 * instrument until the next reboot: the claim lives on RAM:, so a reboot
 * clears it anyway, but a wedge does not reboot. DateStamp is the clock on
 * both ends (same value in every process, unlike a wall clock read through
 * the filesystem), and the stale path renames the whole claim directory so
 * exactly one contender can win the takeover -- this SDK has no RemoveDir,
 * so the displaced directory stays on disk with its dead heartbeat. */
/* SECONDS, and deliberately not "minutes*50 + ticks" (2026-10-18 review):
 * that expression is not seconds and is not monotonic -- ds_Tick resets
 * every minute, so the stamp jumps BACKWARDS at the boundary, while within
 * a minute the age comes out 50x too large (a heartbeat 0.6 s old reads as
 * 30 s and the claim is taken over from a live holder). Age now compares
 * directly against RI_CLAIM_STALE_SEC, and ds_Days keeps the stamp
 * meaningful across a reboot as well as across a minute. */
static long claim_stamp_now(void) {
    struct DateStamp ds;
    DateStamp(&ds);
    return (long)ds.ds_Days * 86400L + (long)ds.ds_Minute * 60L +
        (long)ds.ds_Tick / (long)TICKS_PER_SECOND;
}

/* The heartbeat. What the lane probe (RAM:APIPROBE, raw values in
 * docs/evidence) actually established, corrected where an earlier note in
 * this file was wrong:
 *
 *   1. A file another process holds open for writing can be neither
 *      re-opened MODE_NEWFILE (IoErr 202) nor READ (MODE_OLDFILE returns
 *      NULL, 202). An earlier version of this comment claimed reads were
 *      fine; they are not, and the whole busy/stale split depends on it.
 *   2. Therefore the holder keeps ONE handle open for its lifetime: a
 *      contender that can neither read nor create the heartbeat is looking
 *      at a live holder, and "busy" is structural rather than a lucky race.
 *   3. After a crash nothing holds the file, both operations succeed, and
 *      the age decides -- which is the only path to a takeover.
 *   4. Open(path, MODE_READWRITE) DOES create a missing file here (handle
 *      OK, IoErr 0), exactly as the AmigaOS docs say. An earlier note here
 *      claimed the opposite and sent the first implementation down a dead
 *      end; MODE_NEWFILE is still used because it is explicit about
 *      create-or-truncate.
 */
static BPTR s_hb;
static char s_hb_path[192];
static char s_claim_why[64];

static void claim_why(const char *text) {
    snprintf(s_claim_why, sizeof s_claim_why, "%s", text);
}

const char *ri_pal_claim_last(void) {
    return s_claim_why;
}

/* Create the heartbeat unless something already holds it open. keep != 0
 * means "we are the holder": the handle stays open for the process
 * lifetime (a held-open file is what makes a contender's create fail, which
 * is the evidence it uses). The existence probe passes keep = 0: on the
 * takeover path the directory it wrote into is about to be renamed away,
 * and keeping that handle would leak it -- the second create used to
 * overwrite s_hb and strand the first one open (2026-10-18 review). */
static int claim_hb_try_create(const char *dir, int keep) {
    char path[192];
    long now = claim_stamp_now();
    BPTR f;
    snprintf(path, sizeof(path), "%s/hb", dir);
    f = Open((CONST_STRPTR)path, MODE_NEWFILE);
    if (!f)
        return 1;                      /* held open by someone: a live holder */
    if (Write(f, (APTR)&now, (LONG)sizeof now) != (LONG)sizeof now) {
        Close(f);
        return 1;
    }
    if (!keep) {
        Close(f);
        DeleteFile((CONST_STRPTR)path);
        return 0;                      /* probe only: leave nothing behind */
    }
    /* Kept open on purpose -- see note 2. */
    s_hb = f;
    snprintf(s_hb_path, sizeof s_hb_path, "%s", path);
    return 0;
}

static int claim_hb_refresh(void) {
    long now = claim_stamp_now();
    if (!s_hb)
        return 1;
    if (Seek(s_hb, 0, SEEK_SET) < 0)
        return 1;
    if (Write(s_hb, (APTR)&now, (LONG)sizeof now) != (LONG)sizeof now)
        return 1;
    return 0;
}

static void claim_hb_close(void) {
    if (s_hb) {
        Close(s_hb);
        s_hb = 0;
    }
    if (s_hb_path[0]) {
        DeleteFile((CONST_STRPTR)s_hb_path);
        s_hb_path[0] = '\0';
    }
}

/* 0 = read, *age set. 1 = nothing there (or no live holder) -> stale.
 * 2 = there and unreadable, or held open by a live holder -> busy. */
static int claim_hb_age(const char *dir, long *age) {
    char path[192];
    long stamp = 0;
    uint32_t got = 0u;
    int rc;
    snprintf(path, sizeof(path), "%s/hb", dir);
    rc = ri_pal_read_file(path, &stamp, (uint32_t)sizeof stamp, &got);
    if (rc == 0 && got == (uint32_t)sizeof stamp) {
        *age = claim_stamp_now() - stamp;
        if (*age < 0)
            *age = 0;                  /* clock moved: never read as stale */
        return 0;
    }
    /* Unreadable or absent: the create tells them apart. */
    if (claim_hb_try_create(dir, 0) == 0) {
        return 1;                      /* it was absent: no live holder */
    }
    snprintf(s_claim_why, sizeof s_claim_why,
        "held, heartbeat not readable (rc=%d got=%lu IoErr %ld)", rc,
        (unsigned long)got, (long)IoErr());
    return 2;
}

int ri_pal_claim_acquire(const char *dir) {
    BPTR l;
    long age = 0;
    s_claim_why[0] = '\0';
    if (!dir || !dir[0]) {
        claim_why("no path");
        return 2;
    }
    l = CreateDir((CONST_STRPTR)dir);
    if (l) {
        UnLock(l);
        if (claim_hb_try_create(dir, 1)) {
            claim_why("taken but heartbeat unwritable");
            return 2;
        }
        claim_why("taken");
        return 0;
    }
    if (IoErr() != ERROR_OBJECT_EXISTS) {
        snprintf(s_claim_why, sizeof s_claim_why, "undecidable (IoErr %ld)",
            (long)IoErr());
        return 2;                      /* no parent, read-only volume */
    }
    /* An unreadable heartbeat means something made this directory that is
     * not a holder we can date -- so REFUSE (busy), do not take over.
     * Treating "cannot tell" as "stale" is a takeover-by-default policy:
     * it lets any process that cannot read one small file evict the
     * running instance and become a second writer. Failing safe costs a
     * manual retry in a case that should not happen at all. */
    {
        int hb = claim_hb_age(dir, &age);
        if (hb == 2) {
            claim_why("held, heartbeat unreadable -> busy");
            return 1;
        }
        if (hb == 1) {
            claim_why("undated (no heartbeat) -> takeover");
            snprintf(s_claim_why, sizeof s_claim_why,
                "undated (no heartbeat) -> takeover");
            goto takeover;
        }
    }
    if (age <= (long)RI_CLAIM_STALE_SEC) {
        snprintf(s_claim_why, sizeof s_claim_why, "held, heartbeat %ld s old -> busy",
            age);
        return 1;                      /* a live holder is refreshing it */
    }
    snprintf(s_claim_why, sizeof s_claim_why, "stale (%ld s) -> takeover", age);
takeover:
    /* Displace ONCE, then let the atomic primitive decide (2026-10-18
     * review). The old loop renamed on EVERY iteration: a competitor that
     * won between our rename and our CreateDir had its brand-new live claim
     * renamed away on our next iteration, so "exactly one contender wins"
     * was false and two instances could both end up holding it. One
     * rename, one CreateDir; a failure means somebody else owns it now. */
    {
        /* One attempt, with a name nothing else can already be holding.
         * The probe settled the semantics: Rename() returns DOSTRUE(-1) on
         * success and FALSE(0) with IoErr on failure (205 missing, 203 when
         * the destination exists), so a fixed ".dead" that is already there
         * makes the displacement fail and a stale claim unrecoverable for
         * the rest of the session. The stamp keeps it unique in practice;
         * if the rename fails anyway (a competitor took it microseconds
         * earlier) the CreateDir below decides, and that competitor keeps
         * the claim. Trying further names after losing that race is what
         * used to steal a live claim from whoever won it. */
        static unsigned long seq;
        char dead[208];
        snprintf(dead, sizeof(dead), "%s.dead-%ld-%lu", dir, claim_stamp_now(),
            ++seq);
        /* The return value is deliberately ignored. The probe found
         * DOSTRUE(-1) for a successful FILE rename on this AROS -- which is
         * what the docs say -- but a DIRECTORY rename here comes back 0
         * while still having moved the directory, so any test on the return
         * code is a trap. The CreateDir below is the only verdict that
         * matters, and it is the platform's atomic test-and-set. */
        (void)Rename((CONST_STRPTR)dir, (CONST_STRPTR)dead);
    }
    l = CreateDir((CONST_STRPTR)dir);
    if (!l) {
        snprintf(s_claim_why, sizeof s_claim_why,
            "displaced, but not re-creatable (IoErr %ld) -> busy", (long)IoErr());
        return 1;                      /* somebody else won: they hold it */
    }
    UnLock(l);
    return claim_hb_try_create(dir, 1) ? 2 : 0;
}

int ri_pal_claim_refresh(const char *dir) {
    if (!dir || !dir[0] || !s_hb)
        return 2;
    if (claim_hb_refresh()) {
        snprintf(s_claim_why, sizeof s_claim_why, "refresh failed (lost the claim?)");
        return 2;
    }
    return 0;
}

void ri_pal_claim_release(const char *dir) {
    if (!dir || !dir[0])
        return;
    claim_hb_close();
}
