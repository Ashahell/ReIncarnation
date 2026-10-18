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
    RI_PATH_TEMP = 3,   /* temp/wav/log: AROS first USB/stick vol else RAM:, Win %TEMP% */
    RI_PATH_PREFS = 4,  /* prefs: AROS ENVARC:, Win %APPDATA% */
    RI_PATH_PACKS = 5,  /* sample packs: AROS SYS:Classes/ReIncarnation/Packs/ */
    RI_PATH_RUN = 6    /* run state that must die with the boot: AROS RAM:. */
};

/* Write the base dir for p into out[cap] (NUL-terminated). 0 ok. */
int ri_pal_path(enum ri_path p, char *out, uint32_t cap);
/* First mounted durable volume (AROS: a USB/stick volume; "" when none),
 * else RAM:. Never raises a requester for a missing volume. 0 ok.
 * Lives beside ri_pal_path so the volume list stays in platform/ — the
 * app must not hold its own copy of it. */
int ri_pal_sticky_vol(char *out, uint32_t cap);
/* Probe <sticky volume><sub> for each candidate in turn (plus SYS: last) and
 * return the first that mounts, in out[cap]. AROS only; 1 on every host.
 * Exists because a song library is not at ONE fixed location: the Dell keeps
 * it on the stick, riqemu1 under SYS:, and a static path serves exactly one of
 * them. Never raises a requester on a miss. 0 ok. */
int ri_pal_probe_sub(const char *sub, char *out, uint32_t cap);
/* Join dir + leaf with the platform separator (':' vs '\\' vs '/'). 0 ok. */
int ri_pal_path_join(char *out, uint32_t cap, const char *dir, const char *leaf);
/* List subdirectories of dir via cb(name). Stops if cb returns nonzero. */
int ri_pal_list_dirs(const char *dir, int (*cb)(void *u, const char *name), void *u);
/* Whole-file helpers (bounded, caller buffer). 0 ok. */
int ri_pal_read_file(const char *path, void *buf, uint32_t cap, uint32_t *got);
int ri_pal_write_file(const char *path, const void *buf, uint32_t n);

/* Single-instance claim (2026-10-18). RIAPP writes prefs under
 * ENVARC:ReIncarnation/ and its window is a singleton; the requester that
 * used to announce a second instance did NOT stop it, so two instances
 * wrote the same files. A claim is a directory whose creation is atomic,
 * with a heartbeat file inside so a CRASHED holder cannot lock the
 * instrument until the next reboot (a claim on a volatile volume dies
 * with the boot; a claim on a durable one would not).
 *
 * 0 = claimed / refreshed, 1 = another live instance holds it,
 * 2 = undecidable (no claim, parent missing, heartbeat unreadable on a
 *     fresh claim). The caller MUST run on 2: an unopenable claim must
 * never be the reason the instrument will not open. */
#define RI_CLAIM_STALE_SEC 30
/* A locked heartbeat is taken over only when the UNLOCKED stamp file is
 * this old: twice the staleness window, so the holder has missed several
 * refreshes (they are 10 s apart) and a race between a contender's read
 * and its rename cannot steal a claim from a live instance. */
#define RI_CLAIM_TAKEOVER_SEC (RI_CLAIM_STALE_SEC * 2)
/* How long a blocking UI call (a requester, a file picker) is allowed to
 * hold the claim without the holder having to refresh it. The main loop
 * does not run while such a call blocks, so without this an open dialog
 * looks exactly like a wedge after RI_CLAIM_TAKEOVER_SEC. Longer than any
 * dialog a person is expected to sit in; a modal left open beyond it can
 * still be displaced, which is the documented limit. */
#define RI_CLAIM_MODAL_SEC 900
int ri_pal_claim_acquire(const char *dir);
int ri_pal_claim_refresh(const char *dir);
/* 0 ok, 2 = cannot tell, RI_CLAIM_REFRESH_LOST = the stamp names another
 * holder, i.e. we were displaced and are no longer the singleton. Distinct
 * from 2 because the app must STOP writing prefs then, not merely complain. */
#define RI_CLAIM_REFRESH_LOST 3
/* Stamp a modal deadline before a blocking UI call; clear it after. 0 ok. */
int ri_pal_claim_modal(const char *dir, int enter);
void ri_pal_claim_release(const char *dir);
/* Why the last acquire decided what it decided ("taken", "busy age=12",
 * "stale age=93 -> takeover", "heartbeat unreadable", "IoErr 205"). A lock
 * that decides silently is a lock nobody can debug: the return code alone
 * cannot tell "another instance" from "this volume is read-only". Owned by
 * the PAL so the decision detail never depends on the caller's logger. */
const char *ri_pal_claim_last(void);

#endif
