/* t56_rbng_bank — §12.7a Task 8: RBNG v1.1 BANK chunk + v1.0 conversion.
 * RED-first: no BANK support exists yet.
 */
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "tests/helpers/ri_assert.h"
#include "engine/seq/pattern.h"
#include "project/rbng.h"

#define T56A "/tmp/ri/run/t56-rt.rbng"
#define T56B "/tmp/ri/run/t56-mut.rbng"

static char ERR[256];
static char WARN[512];

/* 303A bank: Up/Down rows, short length; returns slot count used. */
static void build_303a(struct RIPatternBank *b) {
    ri_bank_init(b, 0, RI_PATTERN_KIND_303, 0);
    ri_pattern_set_length(&b->pat[0], 7);
    ri_p303_set(&b->pat[0], 0, 9, 0);
    ri_p303_set(&b->pat[0], 1, 11, RI_STEP_UP);
    ri_p303_set(&b->pat[0], 2, 4, (uint8_t)(RI_STEP_ACCENT | RI_STEP_SLIDE));
    ri_p303_set(&b->pat[0], 6, 12, RI_STEP_DOWN);
    ri_pattern_set_length(&b->pat[5], 1);
    ri_p303_set(&b->pat[5], 0, 0, RI_STEP_UP);
}

static void build_808(struct RIPatternBank *b) {
    ri_bank_init(b, 2, RI_PATTERN_KIND_DRUM, RI_DRUM_CLASS_808);
    ri_pattern_set_length(&b->pat[3], 16);
    ri_pdrum_set(&b->pat[3], 0, RI_L808_BD, RI_HIT_LOW);
    ri_pdrum_set(&b->pat[3], 4, RI_L808_BD, RI_HIT_LOW);
    ri_pdrum_set(&b->pat[3], 4, RI_L808_CH, RI_HIT_LOW);
    ri_pdrum_set_ac(&b->pat[3], 4, 1);
}

static void build_909(struct RIPatternBank *b) {
    ri_bank_init(b, 3, RI_PATTERN_KIND_DRUM, RI_DRUM_CLASS_909);
    ri_pattern_set_length(&b->pat[0], 16);
    ri_pdrum_set(&b->pat[0], 0, RI_L909_BD, RI_HIT_HIGH);
    ri_pdrum_set(&b->pat[0], 2, RI_L909_SD, RI_HIT_FLAM);
    ri_pdrum_set(&b->pat[0], 2, RI_L909_CH, RI_HIT_LOW);
    ri_pdrum_set_ac(&b->pat[0], 2, 1);
}

static uint32_t file_bytes(const char *path, unsigned char *out,
    uint32_t cap) {
    FILE *f = fopen(path, "rb");
    uint32_t n = 0;
    int c;
    if (!f)
        return 0;
    while ((c = fgetc(f)) != EOF && n < cap)
        out[n++] = (unsigned char)c;
    fclose(f);
    return n;
}

/* Offset of the first 'BANK' chunk id in the file image, or 0xFFFFFFFF. */
static uint32_t find_bank(const unsigned char *d, uint32_t n) {
    uint32_t i;
    for (i = 12; i + 4 <= n; i++)
        if (d[i] == 'B' && d[i + 1] == 'A' && d[i + 2] == 'N' &&
            d[i + 3] == 'K')
            return i;
    return 0xFFFFFFFFu;
}

int main(void) {
    static struct RISong s, r;
    static unsigned char img[65536];
    uint32_t n, boff;
    uint32_t i;

    /* Round-trip: 4 banks mixed content. */
    rbng_song_init(&s);
    s.nsteps = 16;
    s.nbanks = 4;
    build_303a(&s.bank[0]);
    ri_bank_init(&s.bank[1], 1, RI_PATTERN_KIND_303, 0);
    build_808(&s.bank[2]);
    build_909(&s.bank[3]);
    RI_ASSERT(rbng_write_song(T56A, &s, ERR, sizeof ERR) == 0,
        "write: %s", ERR);
    memset(&r, 0xA5, sizeof r);
    RI_ASSERT(rbng_read_song(T56A, &r, ERR, sizeof ERR) == 0,
        "read: %s", ERR);
    RI_ASSERT(r.nbanks == 4u, "nbanks %u", r.nbanks);
    for (i = 0; i < 4u; i++)
        RI_ASSERT(memcmp(&s.bank[i], &r.bank[i], sizeof s.bank[i]) == 0,
            "bank %u differs", i);

    /* Writer minor + no-PATT rule. */
    n = file_bytes(T56A, img, sizeof img);
    RI_ASSERT(n > 16u, "short file");
    RI_ASSERT(img[8] == 'R', "form");
    {
        uint32_t off = 12, minor = 99, saw_patt = 0;
        while (off + 8 <= n) {
            uint32_t sz = ((uint32_t)img[off + 4] << 24) |
                ((uint32_t)img[off + 5] << 16) |
                ((uint32_t)img[off + 6] << 8) | img[off + 7];
            if (!memcmp(img + off, "VERS", 4))
                minor = ((uint32_t)img[off + 10] << 8) | img[off + 11];
            if (!memcmp(img + off, "PATT", 4))
                saw_patt = 1;
            off += 8 + sz + (sz & 1u);
        }
        RI_ASSERT(minor == 1u, "minor %u", minor);
        RI_ASSERT(!saw_patt, "PATT written with banks");
    }
    /* Legacy-shaped song stays minor 0 with PATT. */
    rbng_song_init(&s);
    s.nsteps = 2;
    s.steps[0].note = 45;
    s.steps[0].flags = 0;
    s.steps[1].note = 0;
    s.steps[1].flags = RI_RBNG_REST;
    RI_ASSERT(rbng_write_song(T56B, &s, ERR, sizeof ERR) == 0,
        "legacy write: %s", ERR);
    n = file_bytes(T56B, img, sizeof img);
    {
        uint32_t off = 12, minor = 99, saw_patt = 0, saw_bank = 0;
        while (off + 8 <= n) {
            uint32_t sz = ((uint32_t)img[off + 4] << 24) |
                ((uint32_t)img[off + 5] << 16) |
                ((uint32_t)img[off + 6] << 8) | img[off + 7];
            if (!memcmp(img + off, "VERS", 4))
                minor = ((uint32_t)img[off + 10] << 8) | img[off + 11];
            if (!memcmp(img + off, "PATT", 4))
                saw_patt = 1;
            if (!memcmp(img + off, "BANK", 4))
                saw_bank = 1;
            off += 8 + sz + (sz & 1u);
        }
        RI_ASSERT(minor == 0u, "legacy minor %u", minor);
        RI_ASSERT(saw_patt && !saw_bank, "legacy shape");
    }

    /* Sparse: only slot 17 set. */
    rbng_song_init(&s);
    s.nsteps = 16;
    s.nbanks = 1;
    ri_bank_init(&s.bank[0], 1, RI_PATTERN_KIND_303, 0);
    ri_p303_set(&s.bank[0].pat[17], 3, 7, RI_STEP_ACCENT);
    RI_ASSERT(rbng_write_song(T56A, &s, ERR, sizeof ERR) == 0,
        "sparse write: %s", ERR);
    n = file_bytes(T56A, img, sizeof img);
    boff = find_bank(img, n);
    RI_ASSERT(boff != 0xFFFFFFFFu, "no BANK");
    RI_ASSERT(img[boff + 11] == 1u, "sparse count %u", img[boff + 11]);
    RI_ASSERT(img[boff + 12] == 17u, "sparse slot %u", img[boff + 12]);
    RI_ASSERT(rbng_read_song(T56A, &r, ERR, sizeof ERR) == 0,
        "sparse read: %s", ERR);
    RI_ASSERT(r.bank[0].pat[17].row.r303[3].key == 7u, "slot 17 lost");
    {
        struct RIPattern clr;
        ri_pattern_init(&clr, RI_PATTERN_KIND_303, 0);
        for (i = 0; i < 32u; i++) {
            if (i == 17u)
                continue;
            RI_ASSERT(memcmp(&r.bank[0].pat[i], &clr, sizeof clr) == 0,
                "slot %u not cleared", i);
        }
    }

    /* Reject rules via byte patches (each: rc != 0, nothing stored). */
    rbng_song_init(&s);
    s.nsteps = 16;
    s.nbanks = 1;
    build_909(&s.bank[0]);
    RI_ASSERT(rbng_write_song(T56A, &s, ERR, sizeof ERR) == 0,
        "mut source: %s", ERR);
    n = file_bytes(T56A, img, sizeof img);
    boff = find_bank(img, n);
    {
        /* Layout: BANK size | instance kind class count | rec: slot kind
         * len ver + rows. build_909 slot 0: rows start at +16. */
        static const struct { const char *name; uint32_t off;
            unsigned char val; } muts[] = {
            { "instance", 0, 8 },   /* off filled below */
            { "kind", 0, 2 },
            { "class", 0, 2 },
            { "count0", 0, 0 },
            { "count33", 0, 33 },
            { "reckind", 0, 0 },
            { "slot32", 0, 32 },
            { "ver2", 0, 2 },
            { "badlen", 0, 0 },
        };
        uint32_t hdr[9];
        hdr[0] = boff + 8;   /* instance */
        hdr[1] = boff + 9;   /* kind */
        hdr[2] = boff + 10;  /* class */
        hdr[3] = boff + 11;  /* count -> 0 */
        hdr[4] = boff + 11;  /* count -> 33 */
        hdr[5] = boff + 13;  /* rec kind (bank kind 1 -> 0) */
        hdr[6] = boff + 12;  /* rec slot -> 32 */
        hdr[7] = boff + 15;  /* rec ver -> 2 */
        hdr[8] = boff + 4;   /* chunk size -> +1 */
        for (i = 0; i < 9u; i++) {
            unsigned char one = muts[i].val;
            if (i == 8)
                one = (unsigned char)(img[hdr[i]] + 1u);
            RI_ASSERT(rbng_test_patch_bytes(T56A, T56B, hdr[i], &one,
                1) == 0, "patch %s", muts[i].name);
            memset(&r, 0, sizeof r);
            RI_ASSERT(rbng_read_song(T56B, &r, ERR, sizeof ERR) != 0,
                "%s accepted: %s", muts[i].name, ERR);
            RI_ASSERT(r.nbanks == 0u, "%s stored", muts[i].name);
        }
        /* Duplicate slot: copy slot byte of record 0 over... single
         * record file: patch count to 2 and duplicate the record. */
        {
            /* Build a 2-record bank: slots 0 and 5, then alias 5 -> 0. */
            static struct RISong d;
            uint32_t b2, r0len;
            rbng_song_init(&d);
            d.nsteps = 16;
            d.nbanks = 1;
            build_909(&d.bank[0]);
            ri_pdrum_set(&d.bank[0].pat[5], 1, 2, RI_HIT_LOW);
            RI_ASSERT(rbng_write_song(T56A, &d, ERR, sizeof ERR) == 0,
                "dup source: %s", ERR);
            n = file_bytes(T56A, img, sizeof img);
            b2 = find_bank(img, n);
            r0len = 4u + 112u;
            /* Second record slot offset = b2+12+r0len. */
            {
                unsigned char z = 0;
                RI_ASSERT(rbng_test_patch_bytes(T56A, T56B,
                    b2 + 12 + r0len, &z, 1) == 0, "dup patch");
                memset(&r, 0, sizeof r);
                RI_ASSERT(rbng_read_song(T56B, &r, ERR, sizeof ERR) != 0,
                    "dup slot accepted");
            }
        }
        /* Invalid drum combo: high bit without on (row 0, lane 1:
         * on has only lane 0, so high = 0x0002 is invalid). */
        {
            unsigned char bits[2] = { 0x02, 0x00 }; /* high LE16 = 2 */
            /* rows start boff+16: on[2] high[2] flam[2] flags[1]. */
            RI_ASSERT(rbng_test_patch_bytes(T56A, T56B, boff + 18,
                bits, 2) == 0, "combo patch");
            memset(&r, 0, sizeof r);
            RI_ASSERT(rbng_read_song(T56B, &r, ERR, sizeof ERR) != 0,
                "combo accepted");
        }
        /* Minor-0 file carrying BANK rejects. */
        RI_ASSERT(rbng_test_set_vers(T56A, T56B, 1, 0, 0) == 0,
            "setvers");
        memset(&r, 0, sizeof r);
        RI_ASSERT(rbng_read_song(T56B, &r, ERR, sizeof ERR) != 0,
            "minor0+BANK accepted");
    }

    /* v1.0 conversion incl. warnings. */
    {
        static struct RISong v;
        struct RIPatternBank b;
        rbng_song_init(&v);
        v.nsteps = 4;
        v.steps[0].note = 45;
        v.steps[0].flags = 0;
        v.steps[1].note = 47;
        v.steps[1].flags = RI_RBNG_SLIDE;
        v.steps[2].note = 48;
        v.steps[2].flags = RI_RBNG_ACCENT;
        v.steps[3].note = 0;
        v.steps[3].flags = RI_RBNG_REST;
        WARN[0] = '\0';
        RI_ASSERT(rbng_patt_to_bank(&v, &b, WARN, sizeof WARN) == 0,
            "convert rc");
        RI_ASSERT(b.pat[0].length == 4u, "conv len %u",
            b.pat[0].length);
        RI_ASSERT(b.pat[0].row.r303[0].key == 9u &&
            (b.pat[0].row.r303[0].flags & RI_STEP_SLIDE) != 0u,
            "slide shifted to row 0");
        RI_ASSERT(b.pat[0].row.r303[1].key == 11u &&
            (b.pat[0].row.r303[1].flags & RI_STEP_SLIDE) == 0u,
            "row 1 plain");
        RI_ASSERT(b.pat[0].row.r303[2].key == 12u &&
            (b.pat[0].row.r303[2].flags & RI_STEP_ACCENT) != 0u,
            "accent row");
        RI_ASSERT(b.pat[0].row.r303[3].flags == RI_STEP_REST,
            "rest row");
        RI_ASSERT(WARN[0] == '\0', "warn on clean: %s", WARN);
        /* Out-of-range note folds with warning (70 -> semi 34 -> 22). */
        v.steps[0].note = 70;
        WARN[0] = '\0';
        RI_ASSERT(rbng_patt_to_bank(&v, &b, WARN, sizeof WARN) == 0,
            "fold rc");
        RI_ASSERT(b.pat[0].row.r303[0].key == 10u &&
            (b.pat[0].row.r303[0].flags & RI_STEP_UP) != 0u,
            "fold encoding");
        RI_ASSERT(WARN[0] != '\0' && strstr(WARN, "folded") != 0,
            "fold unwarned: %s", WARN);
        /* Step-0 slide + 303 flam drop with warnings. */
        v.steps[0].note = 45;
        v.steps[0].flags = (uint8_t)(RI_RBNG_SLIDE | RI_RBNG_FLAM);
        WARN[0] = '\0';
        RI_ASSERT(rbng_patt_to_bank(&v, &b, WARN, sizeof WARN) == 0,
            "drop rc");
        RI_ASSERT(strstr(WARN, "step 0 slide") != 0, "no slide warn: %s",
            WARN);
        RI_ASSERT(strstr(WARN, "flam") != 0, "no flam warn: %s", WARN);
        RI_ASSERT(rbng_patt_to_bank(0, &b, WARN, sizeof WARN) == 2,
            "conv null song");
        RI_ASSERT(rbng_patt_to_bank(&v, 0, WARN, sizeof WARN) == 2,
            "conv null bank");
    }

    RI_RESULT("rbng_bank");
}
