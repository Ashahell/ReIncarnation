/* t124_skinsect — S7 per-section skin assignment (2026-09-29).
 * Pure table: init all Classic; set/get per section; SYNTH2 follows
 * SYNTH1 unless explicitly split; uniform-Classic detection (song
 * SKAS written only when non-uniform); refcount acquire/release for
 * the loader's shared loads; bad section/name fail closed.
 * RED-first: stub table ignores writes, refcounts refuse.
 */
#include <stdio.h>
#include <string.h>
#include "tests/helpers/ri_assert.h"
#include "gui/skinsect.h"
#include "gui/ctlreg.h"

int main(void) {
    static struct RISkinAssign a;
    static struct RISkinUse u[RI_SKINSECT_MAX_USERS];
    uint32_t i;
    for (i = 0u; i < RI_SKINSECT_MAX_USERS; i++) {
        u[i].mod[0] = '\0';
        u[i].users = 0u;
    }
    ri_skinassign_init(&a);
    RI_ASSERT(ri_skinassign_uniform_classic(&a), "init uniform");
    RI_ASSERT(!strcmp(ri_skinassign_get(&a, RI_SEC_808), ""), "init classic");
    /* SYNTH2 follows SYNTH1 until split. */
    RI_ASSERT(ri_skinassign_set(&a, RI_SEC_SYNTH1, "808-RI") == 0, "set 303");
    RI_ASSERT(!strcmp(ri_skinassign_get(&a, RI_SEC_SYNTH1), "808-RI"), "get 303");
    RI_ASSERT(!strcmp(ri_skinassign_get(&a, RI_SEC_SYNTH2), "808-RI"), "follow");
    RI_ASSERT(!ri_skinassign_uniform_classic(&a), "non-uniform");
    RI_ASSERT(ri_skinassign_set(&a, RI_SEC_SYNTH2, "Template") == 0, "split");
    RI_ASSERT(!strcmp(ri_skinassign_get(&a, RI_SEC_SYNTH2), "Template"), "split reads");
    RI_ASSERT(!strcmp(ri_skinassign_get(&a, RI_SEC_SYNTH1), "808-RI"), "sibling kept");
    /* Reseat to Classic; other sections untouched. */
    RI_ASSERT(ri_skinassign_set(&a, RI_SEC_SYNTH1, "") == 0, "reseat");
    RI_ASSERT(!strcmp(ri_skinassign_get(&a, RI_SEC_SYNTH1), ""), "reseat reads");
    /* Fail closed. */
    RI_ASSERT(ri_skinassign_set(&a, RI_SEC_COUNT, "808-RI") == 2, "bad section");
    RI_ASSERT(ri_skinassign_set(&a, RI_SEC_808, 0) == 2, "null name");
    RI_ASSERT(!strcmp(ri_skinassign_get(&a, RI_SEC_COUNT), ""), "bad get classic");
    RI_ASSERT(ri_skinassign_set(0, RI_SEC_808, "x") == 2, "null table");
    RI_ASSERT(!strcmp(ri_skinassign_get(0, RI_SEC_808), ""), "null get classic");
    /* Refcounts: shared loads, release to zero, unknown refused. */
    RI_ASSERT(ri_skinuse_acquire(u, RI_SKINSECT_MAX_USERS, "808-RI") == 0, "acq");
    RI_ASSERT(ri_skinuse_acquire(u, RI_SKINSECT_MAX_USERS, "808-RI") == 0, "acq shared");
    RI_ASSERT(ri_skinuse_release(u, RI_SKINSECT_MAX_USERS, "808-RI") == 0, "rel");
    RI_ASSERT(ri_skinuse_release(u, RI_SKINSECT_MAX_USERS, "808-RI") == 0, "rel zero");
    RI_ASSERT(ri_skinuse_release(u, RI_SKINSECT_MAX_USERS, "808-RI") == 2, "rel unknown");
    RI_ASSERT(ri_skinuse_acquire(u, RI_SKINSECT_MAX_USERS, "") == 2, "acq classic never");
    RI_ASSERT(ri_skinuse_acquire(0, RI_SKINSECT_MAX_USERS, "x") == 2, "acq null");
    RI_RESULT("skinsect");
}
