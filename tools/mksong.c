/* tools/mksong — deterministic 10-song RBNG corpus generator
 * (Task 13, gate G13). Writes tests/golden/songs/corpus/sXX.rbng from
 * a fixed table (no RNG, no time, no I/O beyond the outputs), so the
 * audit can regenerate to a scratch dir and cmp byte-identical
 * (generation determinism) before the double-render gate.
 *
 * usage:
 *   mksong OUTDIR             write s01..s10.rbng into OUTDIR
 *   mksong --resave IN OUT    parse IN.rbng, re-serialize to OUT.rbng
 *                             (serialize-parse-serialize proof)
 * exit: 0 ok, 2 usage/IO/codec error.
 */
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include "project/rbng.h"

static void scale_steps(struct RISong *s, uint8_t root, uint8_t acc_every,
    uint8_t slide_every) {
    static const uint8_t DEG[8] = { 0, 2, 4, 7, 9, 12, 14, 16 };
    uint32_t i;
    s->nsteps = 16;
    for (i = 0; i < 16u; i++) {
        s->steps[i].note = (uint8_t)(root + DEG[i % 8u]);
        s->steps[i].flags = 0;
        if (acc_every && i % acc_every == 0u)
            s->steps[i].flags |= RI_RBNG_ACCENT;
        if (slide_every && i % slide_every == 0u)
            s->steps[i].flags |= RI_RBNG_SLIDE;
    }
    s->steps[6].flags |= RI_RBNG_REST;
    s->steps[6].flags &= (uint8_t)~RI_RBNG_ACCENT;
    s->steps[11].flags |= RI_RBNG_FLAM;
}

static void auto_lane(struct RISong *s, uint16_t ctl0, uint16_t ctl1) {
    s->nauto = 4;
    s->auto_ev[0].tick = 0;
    s->auto_ev[0].ctl = ctl0;
    s->auto_ev[0].val = 64;
    s->auto_ev[1].tick = 24;
    s->auto_ev[1].ctl = ctl0;
    s->auto_ev[1].val = 96;
    s->auto_ev[2].tick = 48;
    s->auto_ev[2].ctl = ctl1;
    s->auto_ev[2].val = 32;
    s->auto_ev[3].tick = 72;
    s->auto_ev[3].ctl = ctl1;
    s->auto_ev[3].val = 112;
}

static void mod_one(struct RISong *s, const char *name, uint16_t vers) {
    uint32_t i;
    s->nmods = 1;
    strncpy(s->mods[0].name, name, RI_RBNG_MAX_MOD_NAME);
    s->mods[0].name[RI_RBNG_MAX_MOD_NAME] = '\0';
    for (i = 0; i < 64u; i++)
        s->mods[0].sha[i] = "0123456789abcdef"[(i + vers) % 16u];
    s->mods[0].sha[64] = '\0';
    s->mods[0].vers = vers;
}

/* The 10-song corpus table (fixed; variants cover tempo/ppq/steps/
 * automation/mods/CPRG breadth for the double-render gate). */
static void make_song(uint32_t idx, struct RISong *s) {
    static const uint8_t ROOTS[10] = {
        45, 43, 47, 41, 48, 44, 46, 42, 50, 45
    };
    static const uint16_t TEMPI[10] = {
        120, 140, 100, 174, 128, 140, 90, 150, 132, 140
    };
    static const uint16_t PPQS[10] = {
        96, 96, 48, 96, 192, 96, 24, 96, 96, 96
    };
    static const char *CPRGS[10] = {
        "(C) 2026 RI corpus s01", "(C) 2026 RI corpus s02",
        "(C) 2026 RI corpus s03", "(C) 2026 RI corpus s04",
        "(C) 2026 RI corpus s05", "(C) 2026 RI corpus s06",
        "(C) 2026 RI corpus s07", "(C) 2026 RI corpus s08",
        "(C) 2026 RI corpus s09", "(C) 2026 RI corpus s10"
    };
    rbng_song_init(s);
    s->tempo = TEMPI[idx];
    s->ppq = PPQS[idx];
    scale_steps(s, ROOTS[idx], (uint8_t)(2u + idx % 3u),
        (uint8_t)(3u + idx % 4u));
    if (idx == 4u)
        s->nsteps = 8;
    if (idx == 7u)
        s->nsteps = 32;
    if (idx == 7u) {
        uint32_t i;
        for (i = 16u; i < 32u; i++) {
            s->steps[i].note = (uint8_t)(50 + (i % 5u));
            s->steps[i].flags = (i % 4u == 0u) ? RI_RBNG_ACCENT : 0;
        }
    }
    if (idx % 2u == 0u)
        auto_lane(s, 0x0300u, 0x0A00u);
    else {
        s->nauto = 2;
        s->auto_ev[0].tick = 0;
        s->auto_ev[0].ctl = 0x0301u;
        s->auto_ev[0].val = 40;
        s->auto_ev[1].tick = 48;
        s->auto_ev[1].ctl = 0x0301u;
        s->auto_ev[1].val = 90;
    }
    if (idx == 9u)
        mod_one(s, "acid-01", 3u);
    else if (idx % 3u == 0u)
        mod_one(s, "corpus-mod", (uint16_t)(1u + idx));
    strncpy(s->cprg, CPRGS[idx], RI_RBNG_MAX_CPRG);
    s->cprg[RI_RBNG_MAX_CPRG] = '\0';
}

int main(int argc, char **argv) {
    static char err[192];
    if (argc == 2) {
        uint32_t k;
        for (k = 0; k < 10u; k++) {
            struct RISong s;
            char path[512];
            make_song(k, &s);
            snprintf(path, sizeof path, "%s/s%02u.rbng", argv[1], k + 1u);
            if (rbng_write_song(path, &s, err, sizeof err) != 0) {
                printf("mksong: write %s failed: %s\n", path, err);
                return 2;
            }
        }
        printf("mksong: 10 songs -> %s\n", argv[1]);
        return 0;
    }
    if (argc == 4 && strcmp(argv[1], "--resave") == 0) {
        struct RISong s;
        if (rbng_read_song(argv[2], &s, err, sizeof err) != 0) {
            printf("mksong: read %s failed: %s\n", argv[2], err);
            return 2;
        }
        if (rbng_write_song(argv[3], &s, err, sizeof err) != 0) {
            printf("mksong: write %s failed: %s\n", argv[3], err);
            return 2;
        }
        return 0;
    }
    printf("usage: mksong OUTDIR | mksong --resave IN.rbng OUT.rbng\n");
    return 2;
}
