/* gui/skinsect.h — per-section skin assignment (S7, 2026-09-29).
 * Pure C, no allocation, no IO. Each section names its mod (empty =
 * Classic). SYNTH2 follows SYNTH1 unless explicitly split. Refcount
 * lifecycle for the AROS loader's shared loads (load once per mod,
 * release on last switch-away). The song SKAS chunk persists this
 * table (project/rbng.h); readers without SKAS take the first MODR
 * mod everywhere (fail-closed, never a silent substitute).
 */
#ifndef RI_SKINSECT_H
#define RI_SKINSECT_H
#include <stdint.h>
#include "gui/ctlreg.h" /* RI_SEC_COUNT, RI_SEC_SYNTH1/2 */
#include "project/rbng.h" /* song SKAS persistence */

#define RI_SKINSECT_NAME 63u
#define RI_SKINSECT_MAX_USERS 8u /* sections sharing one loaded mod */

struct RISkinSect {
    char mod[RI_SKINSECT_NAME + 1u]; /* "" = Classic */
};

struct RISkinAssign {
    struct RISkinSect sec[RI_SEC_COUNT]; /* index = section */
};

/* Refcount entry: one loaded mod shared by sections. */
struct RISkinUse {
    char mod[RI_SKINSECT_NAME + 1u];
    uint8_t users; /* sections bound, 0 = free slot */
};

void ri_skinassign_init(struct RISkinAssign *a); /* all Classic */
/* 0 ok, 2 bad section/name. Name "" reseats Classic. */
int ri_skinassign_set(struct RISkinAssign *a, uint32_t section,
                      const char *mod);
/* Effective mod for section (SYNTH2 follows SYNTH1 unless split).
 * Returns "" for Classic/out-of-range section (fail-closed Classic). */
const char *ri_skinassign_get(const struct RISkinAssign *a,
                              uint32_t section);
/* 1 when every section (modulo the SYNTH2 rule) reads Classic. */
int ri_skinassign_uniform_classic(const struct RISkinAssign *a);
/* Refcounts: acquire returns slot (or -1 full), release drops it.
 * 0 ok, 2 bad arg/unknown/empty. */
int ri_skinuse_acquire(struct RISkinUse *u, uint32_t n, const char *mod);
int ri_skinuse_release(struct RISkinUse *u, uint32_t n, const char *mod);
/* Assignment -> song struct: effective non-Classic entries, ascending
 * section order. 0 ok, 2 bad arg. */
int skinassign_to_song(const struct RISkinAssign *a, struct RISong *s);
/* Song struct -> assignment: SKAS verbatim (+dirty on mods missing from
 * installed, NULL installed = every name missing); no SKAS + nmods > 0
 * takes mods[0] everywhere; else all Classic. NULL dirty = no store.
 * 0 ok, 2 bad song/assign. */
int ri_skinassign_from_song(struct RISkinAssign *a, const struct RISong *s,
    const char *const *installed, uint32_t n, int *dirty);

#endif
