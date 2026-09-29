/* t128_skinchunk — S7c per-section skin song chunk (2026-09-29).
 * SKAS (minor 3): u16 n; per u8 section, u8 namelen, name — sections
 * strictly ascending, names in manifest class, no duplicates. Written
 * only when non-uniform; readers without it take the first MODR mod
 * everywhere (fail-closed). Apply helper: SKAS verbatim + dirty on
 * unknown mods; MODR[0] fallback; empty song reads Classic.
 * RED-first: no SKAS codec, no apply helper (link errors).
 */
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "tests/helpers/ri_assert.h"
#include "project/rbng.h"
#include "gui/skinsect.h"
#include "gui/ctlreg.h"

static struct RISong demo_song(void) {
    static struct RISong s;
    uint32_t i;
    rbng_song_init(&s);
    s.tempo = 140;
    s.ppq = 96;
    s.nsteps = 16;
    for (i = 0u; i < 16u; i++) {
        s.steps[i].note = (uint8_t)(45 + (i % 8));
        s.steps[i].flags = 0;
    }
    return s;
}

/* VERS minor lives at byte 22 (FORM+total+RBNG+head+major). */
static int file_minor(const char *path) {
    FILE *f = fopen(path, "rb");
    unsigned char b[24];
    if (!f)
        return -1;
    if (fread(b, 1, sizeof b, f) != sizeof b) {
        fclose(f);
        return -1;
    }
    fclose(f);
    return ((int)b[22] << 8) | b[23];
}

int main(void) {
    static struct RISong s, r;
    static char err[256];
    static const char *const inst[3] = { "Classic", "808-RI", "Template" };
    struct RISkinAssign a;
    int dirty = 0;
    s = demo_song();
    /* Non-uniform assignment round-trips; minor goes 3. */
    ri_skinassign_init(&a);
    RI_ASSERT(ri_skinassign_set(&a, RI_SEC_808, "808-RI") == 0, "set 808");
    RI_ASSERT(ri_skinassign_set(&a, RI_SEC_SYNTH1, "Template") == 0, "set 303");
    RI_ASSERT(skinassign_to_song(&a, &s) == 0, "to song");
    RI_ASSERT(s.nskin == 2u, "two entries");
    RI_ASSERT(rbng_write_song("/tmp/ri/run/t128/t128-s.rbng", &s, err,
        sizeof err) == 0, "write skas: %s", err);
    RI_ASSERT(file_minor("/tmp/ri/run/t128/t128-s.rbng") == 3, "minor 3");
    memset(&r, 0, sizeof r);
    rbng_song_init(&r);
    RI_ASSERT(rbng_read_song("/tmp/ri/run/t128/t128-s.rbng", &r, err,
        sizeof err) == 0, "read skas: %s", err);
    RI_ASSERT(r.nskin == 2u && r.skin[0].section == RI_SEC_SYNTH1 &&
        !strcmp(r.skin[0].name, "Template") && r.skin[1].section == RI_SEC_808 &&
        !strcmp(r.skin[1].name, "808-RI"), "entries kept + ascending");
    /* Uniform assignment writes no chunk; file stays minor 0. */
    s = demo_song();
    ri_skinassign_init(&a);
    RI_ASSERT(skinassign_to_song(&a, &s) == 0, "uniform to song");
    RI_ASSERT(s.nskin == 0u, "no entries");
    RI_ASSERT(rbng_write_song("/tmp/ri/run/t128/t128-c.rbng", &s, err,
        sizeof err) == 0, "write classic: %s", err);
    RI_ASSERT(file_minor("/tmp/ri/run/t128/t128-c.rbng") == 0, "minor 0");
    /* Apply: SKAS verbatim + dirty on unknown; MODR[0] fallback; empty. */
    memset(&r, 0, sizeof r);
    rbng_song_init(&r);
    RI_ASSERT(rbng_read_song("/tmp/ri/run/t128/t128-s.rbng", &r, err,
        sizeof err) == 0, "reread: %s", err);
    ri_skinassign_init(&a);
    RI_ASSERT(ri_skinassign_from_song(&a, &r, inst, 3u, &dirty) == 0, "apply");
    RI_ASSERT(!dirty, "known clean");
    RI_ASSERT(!strcmp(ri_skinassign_get(&a, RI_SEC_808), "808-RI"), "808 applied");
    RI_ASSERT(!strcmp(ri_skinassign_get(&a, RI_SEC_LEVI), ""), "levi classic");
    {
        static const char *const inst2[2] = { "Classic", "Template" };
        ri_skinassign_init(&a);
        dirty = 0;
        RI_ASSERT(ri_skinassign_from_song(&a, &r, inst2, 2u, &dirty) == 0, "apply2");
        RI_ASSERT(dirty, "unknown dirties");
        RI_ASSERT(!strcmp(ri_skinassign_get(&a, RI_SEC_808), "808-RI"), "name kept");
    }
    s = demo_song();
    strcpy(s.mods[0].name, "808-RI");
    memset(s.mods[0].sha, 'a', 64u);
    s.mods[0].sha[64] = '\0';
    s.mods[0].vers = 1u;
    s.nmods = 1u;
    ri_skinassign_init(&a);
    dirty = 1;
    RI_ASSERT(ri_skinassign_from_song(&a, &s, inst, 3u, &dirty) == 0, "modr apply");
    RI_ASSERT(!dirty, "modr clean");
    RI_ASSERT(!strcmp(ri_skinassign_get(&a, RI_SEC_MIX_909), "808-RI"), "modr everywhere");
    s = demo_song();
    ri_skinassign_init(&a);
    RI_ASSERT(ri_skinassign_from_song(&a, &s, inst, 3u, 0) == 0, "empty apply");
    RI_ASSERT(ri_skinassign_uniform_classic(&a), "empty classic");
    RI_ASSERT(ri_skinassign_from_song(0, &s, inst, 3u, &dirty) == 2, "null assign");
    /* Reject laws: writer refuses descending; reader refuses duplicates. */
    s = demo_song();
    s.skin[0].section = RI_SEC_808;
    strcpy(s.skin[0].name, "808-RI");
    s.skin[1].section = RI_SEC_SYNTH1;
    strcpy(s.skin[1].name, "Template");
    s.nskin = 2u;
    RI_ASSERT(rbng_write_song("/tmp/ri/run/t128/t128-bad.rbng", &s, err,
        sizeof err) != 0, "descending refused");
    {
        /* Patch the valid file's second entry to duplicate the first. */
        FILE *f = fopen("/tmp/ri/run/t128/t128-s.rbng", "rb");
        static unsigned char buf[4096];
        size_t n, o;
        RI_ASSERT(f != 0, "reopen");
        n = f ? fread(buf, 1, sizeof buf, f) : 0u;
        if (f)
            fclose(f);
        RI_ASSERT(n > 32u, "have bytes");
        for (o = 0u; o + 12u < n; o++)
            if (buf[o] == 'S' && buf[o + 1u] == 'K' && buf[o + 2u] == 'A' && buf[o + 3u] == 'S')
                break;
        RI_ASSERT(o + 12u < n, "skas found");
        {
            /* u16 n, then (sec,len,name): entry1 sec = entry0 sec. */
            size_t e0 = o + 8u + 2u;
            size_t l0 = buf[e0 + 1u];
            buf[e0 + 2u + l0] = buf[e0];
            f = fopen("/tmp/ri/run/t128/t128-dup.rbng", "wb");
            RI_ASSERT(f != 0, "dup open");
            if (f) {
                RI_ASSERT(fwrite(buf, 1, n, f) == n, "dup write");
                fclose(f);
            }
        }
        memset(&r, 0, sizeof r);
        rbng_song_init(&r);
        RI_ASSERT(rbng_read_song("/tmp/ri/run/t128/t128-dup.rbng", &r, err,
            sizeof err) != 0, "duplicate refused");
    }
    RI_RESULT("skinchunk");
}
