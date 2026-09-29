/* gui/skinsect.c — per-section skin assignment bodies (S7, 2026-09-29).
 * Pure C, no allocation: fixed tables, bounded copies, fail-closed.
 */
#include "gui/skinsect.h"

static uint32_t sect_norm(uint32_t section) {
    return section < RI_SEC_COUNT ? section : RI_SEC_COUNT;
}

static int name_ok(const char *mod, uint32_t *len_out) {
    uint32_t n = 0u;
    if (!mod)
        return 0;
    while (mod[n]) {
        unsigned char c = (unsigned char)mod[n];
        int ok = (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
            (c >= '0' && c <= '9') || c == '.' || c == '_' || c == '-';
        if (!ok || n >= RI_SKINSECT_NAME)
            return 0;
        n++;
    }
    if (len_out)
        *len_out = n;
    return 1;
}

void ri_skinassign_init(struct RISkinAssign *a) {
    uint32_t i;
    if (!a)
        return;
    for (i = 0u; i < RI_SEC_COUNT; i++)
        a->sec[i].mod[0] = '\0';
}

int ri_skinassign_set(struct RISkinAssign *a, uint32_t section,
                      const char *mod) {
    uint32_t i, n = 0u;
    if (!a || sect_norm(section) >= RI_SEC_COUNT || !name_ok(mod, 0))
        return 2;
    while (mod[n] && n < RI_SKINSECT_NAME) {
        a->sec[section].mod[n] = mod[n];
        n++;
    }
    a->sec[section].mod[n] = '\0';
    for (i = n; i <= RI_SKINSECT_NAME; i++)
        a->sec[section].mod[i] = '\0';
    return 0;
}

const char *ri_skinassign_get(const struct RISkinAssign *a,
                              uint32_t section) {
    if (!a || sect_norm(section) >= RI_SEC_COUNT)
        return "";
    /* SYNTH2 follows SYNTH1 unless explicitly split (non-empty own entry
     * counts as a split, even back to "same as SYNTH1" — the writer then
     * records it; harmless and explicit). */
    if (section == RI_SEC_SYNTH2 && a->sec[RI_SEC_SYNTH2].mod[0] == '\0' &&
        a->sec[RI_SEC_SYNTH1].mod[0] != '\0')
        return a->sec[RI_SEC_SYNTH1].mod;
    return a->sec[section].mod;
}

int ri_skinassign_uniform_classic(const struct RISkinAssign *a) {
    uint32_t i;
    if (!a)
        return 0;
    for (i = 0u; i < RI_SEC_COUNT; i++)
        if (a->sec[i].mod[0] != '\0')
            return 0;
    return 1;
}

static int slot_of(struct RISkinUse *u, uint32_t n, const char *mod) {
    uint32_t i, k;
    if (!u || n == 0u || !mod || mod[0] == '\0')
        return -1;
    for (i = 0u; i < n; i++) {
        for (k = 0u; ; k++) {
            if (k >= RI_SKINSECT_NAME + 1u)
                break;
            if (u[i].mod[k] != mod[k])
                break;
            if (mod[k] == '\0')
                return (int)i;
        }
    }
    return -1;
}

int ri_skinuse_acquire(struct RISkinUse *u, uint32_t n, const char *mod) {
    int s;
    uint32_t i;
    if (!u || n == 0u || !mod || mod[0] == '\0')
        return 2;
    s = slot_of(u, n, mod);
    if (s >= 0) {
        if (u[s].users >= 255u)
            return 2;
        u[s].users++;
        return 0;
    }
    for (i = 0u; i < n; i++) {
        if (u[i].users == 0u) {
            uint32_t k = 0u;
            while (mod[k] && k < RI_SKINSECT_NAME) {
                u[i].mod[k] = mod[k];
                k++;
            }
            u[i].mod[k] = '\0';
            u[i].users = 1u;
            return 0;
        }
    }
    return 2;
}

int ri_skinuse_release(struct RISkinUse *u, uint32_t n, const char *mod) {
    int s;
    if (!u || n == 0u || !mod || mod[0] == '\0')
        return 2;
    s = slot_of(u, n, mod);
    if (s < 0 || u[s].users == 0u)
        return 2;
    u[s].users--;
    if (u[s].users == 0u)
        u[s].mod[0] = '\0';
    return 0;
}
