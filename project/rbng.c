/* rbng.c — RBNG song codec (Task 13, gate G13).
 * No allocation: one static image buffer (bounded RI_RBNG_MAX_FILE);
 * chunk sizes are range-checked against the image before use, never
 * trusted for allocation. Unknown optional chunks are preserved
 * verbatim (id + length + data) so serialize-parse-serialize is
 * byte-identical including forward-compat bytes.
 */
#include "project/rbng.h"
#include <stdio.h>
#include <string.h>

#define RI_RBNG_MAX_FILE 65536u /* songs are KB-scale; hard bound */

static unsigned char RI_RBNG_IMG[RI_RBNG_MAX_FILE];

static void put_err(char *err, uint32_t cap, const char *msg) {
    uint32_t i = 0;
    if (!err || cap == 0u)
        return;
    while (i + 1u < cap && msg[i]) {
        err[i] = msg[i];
        i++;
    }
    err[i] = '\0';
}

static void ck_err(char *err, uint32_t cap, const unsigned char *id,
    uint32_t off, const char *msg) {
    char tmp[192];
    int k = 0, i;
    for (i = 0; i < 4 && k < 180; i++) {
        unsigned char c = id ? id[i] : (unsigned char)'?';
        tmp[k++] = (c >= 32 && c < 127) ? (char)c : '?';
    }
    tmp[k++] = ' ';
    tmp[k++] = '@';
    {
        char nb[12];
        int n = 0;
        uint32_t v = off;
        if (v == 0)
            nb[n++] = '0';
        while (v > 0 && n < 10) {
            nb[n++] = (char)('0' + v % 10u);
            v /= 10u;
        }
        while (n > 0 && k < 178)
            tmp[k++] = nb[--n];
    }
    tmp[k++] = ':';
    tmp[k++] = ' ';
    while (*msg && k < 186)
        tmp[k++] = *msg++;
    tmp[k] = '\0';
    put_err(err, cap, tmp);
}

static uint32_t rd32be(const unsigned char *p) {
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) |
        ((uint32_t)p[2] << 8) | (uint32_t)p[3];
}

static uint16_t rd16be(const unsigned char *p) {
    return (uint16_t)(((uint32_t)p[0] << 8) | p[1]);
}

static void wr16be(FILE *f, uint16_t v) {
    fputc((int)((v >> 8) & 0xffu), f);
    fputc((int)(v & 0xffu), f);
}

static void wr32be(FILE *f, uint32_t v) {
    fputc((int)((v >> 24) & 0xffu), f);
    fputc((int)((v >> 16) & 0xffu), f);
    fputc((int)((v >> 8) & 0xffu), f);
    fputc((int)(v & 0xffu), f);
}

static uint32_t read_file(const char *path, char *err, uint32_t errcap) {
    FILE *f = fopen(path, "rb");
    uint32_t n = 0;
    int ch;
    if (!f) {
        put_err(err, errcap, "open failed");
        return 0;
    }
    while ((ch = fgetc(f)) != EOF) {
        if (n >= RI_RBNG_MAX_FILE) {
            fclose(f);
            put_err(err, errcap, "file too large");
            return 0;
        }
        RI_RBNG_IMG[n++] = (unsigned char)ch;
    }
    fclose(f);
    if (n == 0u)
        put_err(err, errcap, "empty file");
    return n;
}

static int is_hex64(const char *s) {
    int i;
    for (i = 0; i < 64; i++) {
        char c = s[i];
        if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f')))
            return 0;
    }
    return s[64] == '\0';
}

void rbng_song_init(struct RISong *s) {
    memset(s, 0, sizeof *s);
    s->tempo = 140;
    s->ppq = 96;
}

/* ---- writer ---- */

static uint32_t auto_len(const struct RISong *s) {
    return 2u + (uint32_t)s->nauto * 8u;
}

static uint32_t modr_len(const struct RISong *s) {
    uint32_t n = 2u, k;
    for (k = 0; k < s->nmods; k++)
        n += 1u + (uint32_t)strlen(s->mods[k].name) + 1u + 64u + 2u;
    return n;
}

static int song_valid(const struct RISong *s, char *err, uint32_t errcap) {
    uint32_t k;
    if (!s || s->tempo < 30u || s->tempo > 300u || s->ppq < 24u ||
        s->ppq > 960u || s->nsteps == 0u ||
        s->nsteps > RI_RBNG_MAX_STEPS) {
        put_err(err, errcap, "SONG header out of range");
        return 1;
    }
    for (k = 0; k < s->nsteps; k++) {
        uint8_t f = s->steps[k].flags;
        if ((f & ~0x0fu) != 0u || (f & RI_RBNG_REST) == 0u) {
            if (s->steps[k].note > 127u) {
                put_err(err, errcap, "PATT note out of range");
                return 1;
            }
        } else if (s->steps[k].note > 127u) {
            put_err(err, errcap, "PATT note out of range");
            return 1;
        }
        if ((f & RI_RBNG_REST) != 0u && (f & RI_RBNG_ACCENT) != 0u) {
            put_err(err, errcap, "PATT rest+accent illegal");
            return 1;
        }
    }
    if (s->nauto > RI_RBNG_MAX_AUTO) {
        put_err(err, errcap, "AUTO count out of range");
        return 1;
    }
    for (k = 0; k < s->nauto; k++) {
        if (s->auto_ev[k].val > 127u) {
            put_err(err, errcap, "AUTO value out of range");
            return 1;
        }
    }
    if (s->nmods > RI_RBNG_MAX_MODS) {
        put_err(err, errcap, "MODR count out of range");
        return 1;
    }
    for (k = 0; k < s->nmods; k++) {
        uint32_t nl = (uint32_t)strlen(s->mods[k].name);
        if (nl == 0u || nl > RI_RBNG_MAX_MOD_NAME ||
            !is_hex64(s->mods[k].sha)) {
            put_err(err, errcap, "MODR name/sha bad");
            return 1;
        }
    }
    if (strlen(s->cprg) > RI_RBNG_MAX_CPRG) {
        put_err(err, errcap, "CPRG too long");
        return 1;
    }
    if (s->nunknown > RI_RBNG_MAX_UNK) {
        put_err(err, errcap, "unknown table overflow");
        return 1;
    }
    for (k = 0; k < s->nunknown; k++) {
        if (s->unknown[k].len > RI_RBNG_MAX_UNK_BYTES) {
            put_err(err, errcap, "unknown chunk too large");
            return 1;
        }
    }
    return 0;
}

static void chunk_head(FILE *f, const char *id, uint32_t len) {
    fwrite(id, 1, 4, f);
    wr32be(f, len);
}

int rbng_write_song(const char *path, const struct RISong *s, char *err,
    uint32_t errcap) {
    FILE *f;
    uint32_t total, k;
    uint32_t cprg_n, patt_n, auto_n, modr_n;
    if (song_valid(s, err, errcap) != 0)
        return 2;
    cprg_n = 1u + (uint32_t)strlen(s->cprg);
    patt_n = (uint32_t)s->nsteps * 2u;
    auto_n = auto_len(s);
    modr_n = modr_len(s);
    total = 4u; /* 'RBNG' */
    total += 8u + 8u; /* VERS (8 data bytes, even) */
    total += 8u + 6u; /* SONG */
    total += 8u + patt_n + (patt_n & 1u);
    total += 8u + auto_n + (auto_n & 1u);
    total += 8u + modr_n + (modr_n & 1u);
    total += 8u + cprg_n + (cprg_n & 1u);
    for (k = 0; k < s->nunknown; k++)
        total += 8u + s->unknown[k].len + (s->unknown[k].len & 1u);
    f = fopen(path, "wb");
    if (!f) {
        put_err(err, errcap, "open failed");
        return 2;
    }
    fwrite("FORM", 1, 4, f);
    wr32be(f, total);
    fwrite("RBNG", 1, 4, f);
    chunk_head(f, "VERS", 8u);
    wr16be(f, RI_RBNG_MAJOR);
    wr16be(f, RI_RBNG_MINOR);
    wr32be(f, 0u);
    chunk_head(f, "SONG", 6u);
    wr16be(f, s->tempo);
    wr16be(f, s->ppq);
    wr16be(f, s->nsteps);
    chunk_head(f, "PATT", patt_n);
    for (k = 0; k < s->nsteps; k++) {
        fputc((int)s->steps[k].note, f);
        fputc((int)s->steps[k].flags, f);
    }
    if (patt_n & 1u)
        fputc(0, f);
    chunk_head(f, "AUTO", auto_n);
    wr16be(f, s->nauto);
    for (k = 0; k < s->nauto; k++) {
        wr32be(f, s->auto_ev[k].tick);
        wr16be(f, s->auto_ev[k].ctl);
        fputc((int)s->auto_ev[k].val, f);
        fputc(0, f);
    }
    if (auto_n & 1u)
        fputc(0, f);
    chunk_head(f, "MODR", modr_n);
    wr16be(f, s->nmods);
    for (k = 0; k < s->nmods; k++) {
        uint32_t nl = (uint32_t)strlen(s->mods[k].name);
        fputc((int)nl, f);
        fwrite(s->mods[k].name, 1, nl, f);
        fputc(64, f);
        fwrite(s->mods[k].sha, 1, 64, f);
        wr16be(f, s->mods[k].vers);
    }
    if (modr_n & 1u)
        fputc(0, f);
    chunk_head(f, "CPRG", cprg_n);
    fputc((int)strlen(s->cprg), f);
    if (cprg_n > 1u)
        fwrite(s->cprg, 1, cprg_n - 1u, f);
    if (cprg_n & 1u)
        fputc(0, f);
    for (k = 0; k < s->nunknown; k++) {
        fwrite(s->unknown[k].id, 1, 4, f);
        wr32be(f, s->unknown[k].len);
        if (s->unknown[k].len > 0u)
            fwrite(s->unknown[k].data, 1, s->unknown[k].len, f);
        if (s->unknown[k].len & 1u)
            fputc(0, f);
    }
    if (fclose(f) != 0) {
        put_err(err, errcap, "write failed");
        return 2;
    }
    return 0;
}

/* ---- reader ---- */

static int parse_image(const unsigned char *img, uint32_t n, struct RISong *s,
    char *err, uint32_t errcap) {
    uint32_t off, total;
    int saw_vers = 0, saw_song = 0, saw_patt = 0, saw_auto = 0, saw_modr = 0,
        saw_cprg = 0;
    uint32_t patt_expect = 0;
    rbng_song_init(s);
    if (n < 12u || memcmp(img, "FORM", 4) != 0) {
        put_err(err, errcap, "not FORM");
        return 1;
    }
    total = rd32be(img + 4);
    if (memcmp(img + 8, "RBNG", 4) != 0) {
        put_err(err, errcap, "FORM type not RBNG");
        return 1;
    }
    if (total + 8u != n) {
        ck_err(err, errcap, img + 8, 0, "FORM length mismatch");
        return 1;
    }
    off = 12;
    while (off < n) {
        uint32_t size, doff, dend;
        const unsigned char *cid;
        if (off + 8u > n) {
            ck_err(err, errcap, 0, off, "chunk header truncated");
            return 1;
        }
        cid = img + off;
        size = rd32be(img + off + 4);
        doff = off + 8u;
        if (doff + size > n) {
            ck_err(err, errcap, cid, off, "chunk length overruns file");
            return 1;
        }
        dend = doff + size;
        if (memcmp(cid, "VERS", 4) == 0) {
            uint16_t major, minor;
            uint32_t flags;
            if (saw_vers) {
                ck_err(err, errcap, cid, off, "duplicate VERS");
                return 1;
            }
            saw_vers = 1;
            if (size != 8u) {
                ck_err(err, errcap, cid, off, "VERS bad length");
                return 1;
            }
            major = rd16be(img + doff);
            minor = rd16be(img + doff + 2);
            flags = rd32be(img + doff + 4);
            if (major != RI_RBNG_MAJOR) {
                ck_err(err, errcap, cid, off, "VERS unsupported major");
                return 1;
            }
            if (flags != 0u) {
                /* All flag bits are mandatory-by-definition: an unknown
                 * feature bit means the file needs behavior we lack. */
                ck_err(err, errcap, cid, off,
                    "VERS unsupported feature flags");
                return 1;
            }
            (void)minor; /* newer minor: load known, preserve unknown */
        } else if (memcmp(cid, "SONG", 4) == 0) {
            if (saw_song) {
                ck_err(err, errcap, cid, off, "duplicate SONG");
                return 1;
            }
            saw_song = 1;
            if (size != 6u) {
                ck_err(err, errcap, cid, off, "SONG bad length");
                return 1;
            }
            s->tempo = rd16be(img + doff);
            s->ppq = rd16be(img + doff + 2);
            s->nsteps = rd16be(img + doff + 4);
            if (s->tempo < 30u || s->tempo > 300u || s->ppq < 24u ||
                s->ppq > 960u || s->nsteps == 0u ||
                s->nsteps > RI_RBNG_MAX_STEPS) {
                ck_err(err, errcap, cid, off, "SONG header out of range");
                return 1;
            }
            patt_expect = (uint32_t)s->nsteps * 2u;
        } else if (memcmp(cid, "PATT", 4) == 0) {
            uint32_t k;
            if (saw_patt) {
                ck_err(err, errcap, cid, off, "duplicate PATT");
                return 1;
            }
            saw_patt = 1;
            if (!saw_song) {
                ck_err(err, errcap, cid, off, "PATT before SONG");
                return 1;
            }
            if (size != patt_expect) {
                ck_err(err, errcap, cid, off,
                    "PATT length != nsteps*2");
                return 1;
            }
            for (k = 0; k < s->nsteps; k++) {
                uint8_t note = img[doff + 2u * k];
                uint8_t fl = img[doff + 2u * k + 1u];
                if (note > 127u || (fl & ~0x0fu) != 0u ||
                    ((fl & RI_RBNG_REST) != 0u &&
                        (fl & RI_RBNG_ACCENT) != 0u)) {
                    ck_err(err, errcap, cid, off, "PATT bad note/flags");
                    return 1;
                }
                s->steps[k].note = note;
                s->steps[k].flags = fl;
            }
        } else if (memcmp(cid, "AUTO", 4) == 0) {
            const unsigned char *q = img + doff;
            uint32_t left = size, na, k;
            if (saw_auto) {
                ck_err(err, errcap, cid, off, "duplicate AUTO");
                return 1;
            }
            saw_auto = 1;
            if (left < 2u) {
                ck_err(err, errcap, cid, off, "AUTO too short");
                return 1;
            }
            na = ((uint32_t)q[0] << 8) | q[1];
            q += 2;
            left -= 2u;
            if (na > RI_RBNG_MAX_AUTO || left != na * 8u) {
                ck_err(err, errcap, cid, off, "AUTO bad count/length");
                return 1;
            }
            for (k = 0; k < na; k++) {
                s->auto_ev[k].tick = rd32be(q);
                s->auto_ev[k].ctl = rd16be(q + 4);
                s->auto_ev[k].val = q[6];
                if (q[7] != 0u || s->auto_ev[k].val > 127u) {
                    ck_err(err, errcap, cid, off, "AUTO bad value/pad");
                    return 1;
                }
                q += 8;
            }
            s->nauto = (uint16_t)na;
        } else if (memcmp(cid, "MODR", 4) == 0) {
            const unsigned char *q = img + doff;
            uint32_t left = size, nm, k;
            if (saw_modr) {
                ck_err(err, errcap, cid, off, "duplicate MODR");
                return 1;
            }
            saw_modr = 1;
            if (left < 2u) {
                ck_err(err, errcap, cid, off, "MODR too short");
                return 1;
            }
            nm = ((uint32_t)q[0] << 8) | q[1];
            q += 2;
            left -= 2u;
            if (nm > RI_RBNG_MAX_MODS) {
                ck_err(err, errcap, cid, off, "MODR bad count");
                return 1;
            }
            for (k = 0; k < nm; k++) {
                uint32_t nl, sl, j;
                if (left < 1u) {
                    ck_err(err, errcap, cid, off, "MODR entry truncated");
                    return 1;
                }
                nl = q[0];
                q++;
                left--;
                if (nl == 0u || nl > RI_RBNG_MAX_MOD_NAME ||
                    nl + 1u + 64u + 2u > left) {
                    ck_err(err, errcap, cid, off, "MODR bad name length");
                    return 1;
                }
                memcpy(s->mods[k].name, q, nl);
                s->mods[k].name[nl] = '\0';
                q += nl;
                left -= nl;
                sl = q[0];
                q++;
                left--;
                if (sl != 64u || 64u + 2u > left) {
                    ck_err(err, errcap, cid, off, "MODR bad sha length");
                    return 1;
                }
                for (j = 0; j < 64u; j++) {
                    char cch = (char)q[j];
                    if (!((cch >= '0' && cch <= '9') ||
                        (cch >= 'a' && cch <= 'f'))) {
                        ck_err(err, errcap, cid, off, "MODR sha not hex");
                        return 1;
                    }
                    s->mods[k].sha[j] = cch;
                }
                s->mods[k].sha[64] = '\0';
                q += 64;
                left -= 64u;
                s->mods[k].vers = rd16be(q);
                q += 2;
                left -= 2u;
            }
            if (left != 0u) {
                ck_err(err, errcap, cid, off, "MODR trailing bytes");
                return 1;
            }
            s->nmods = (uint16_t)nm;
        } else if (memcmp(cid, "CPRG", 4) == 0) {
            uint32_t ln;
            if (saw_cprg) {
                ck_err(err, errcap, cid, off, "duplicate CPRG");
                return 1;
            }
            saw_cprg = 1;
            if (size < 1u || size > RI_RBNG_MAX_CPRG + 1u) {
                ck_err(err, errcap, cid, off, "CPRG bad length");
                return 1;
            }
            ln = img[doff];
            if (ln + 1u != size) {
                ck_err(err, errcap, cid, off, "CPRG length mismatch");
                return 1;
            }
            memcpy(s->cprg, img + doff + 1u, ln);
            s->cprg[ln] = '\0';
        } else if (cid[0] >= 'A' && cid[0] <= 'Z') {
            /* Unknown optional chunk: preserve verbatim for the
             * round trip (forward-compat rule). */
            struct RBUnknown *u;
            if (s->nunknown >= RI_RBNG_MAX_UNK) {
                ck_err(err, errcap, cid, off, "too many unknown chunks");
                return 1;
            }
            if (size > RI_RBNG_MAX_UNK_BYTES) {
                ck_err(err, errcap, cid, off, "unknown chunk too large");
                return 1;
            }
            u = &s->unknown[s->nunknown++];
            u->id[0] = (char)cid[0];
            u->id[1] = (char)cid[1];
            u->id[2] = (char)cid[2];
            u->id[3] = (char)cid[3];
            u->id[4] = '\0';
            u->len = size;
            if (size > 0u)
                memcpy(u->data, img + doff, size);
        } else {
            ck_err(err, errcap, cid, off, "unknown chunk id");
            return 1;
        }
        off = dend + (size & 1u); /* even pad */
        if (off > n) {
            ck_err(err, errcap, cid, off, "pad overruns file");
            return 1;
        }
    }
    if (!saw_vers) {
        put_err(err, errcap, "missing VERS chunk");
        return 1;
    }
    if (!saw_song) {
        put_err(err, errcap, "missing SONG chunk");
        return 1;
    }
    if (!saw_patt) {
        put_err(err, errcap, "missing PATT chunk");
        return 1;
    }
    if (!saw_auto) {
        put_err(err, errcap, "missing AUTO chunk");
        return 1;
    }
    if (!saw_modr) {
        put_err(err, errcap, "missing MODR chunk");
        return 1;
    }
    if (!saw_cprg) {
        put_err(err, errcap, "missing CPRG chunk");
        return 1;
    }
    return 0;
}

int rbng_read_song(const char *path, struct RISong *s, char *err,
    uint32_t errcap) {
    uint32_t n = read_file(path, err, errcap);
    if (n == 0u)
        return 1;
    return parse_image(RI_RBNG_IMG, n, s, err, errcap);
}

int rbng_cprg_line(const char *path, char *line, uint32_t cap) {
    struct RISong s;
    static char err[128];
    uint32_t i = 0;
    if (rbng_read_song(path, &s, err, sizeof err) != 0)
        return 1;
    if (!line || cap == 0u)
        return 1;
    while (i + 1u < cap && s.cprg[i]) {
        line[i] = s.cprg[i];
        i++;
    }
    line[i] = '\0';
    return 0;
}

int rbng_missing_warn(const struct RISong *s, const char *const *have,
    uint32_t nhave, char *warn, uint32_t cap) {
    uint16_t k;
    uint32_t j;
    for (k = 0; k < s->nmods; k++) {
        int found = 0;
        for (j = 0; j < nhave; j++) {
            if (have[j] && strcmp(have[j], s->mods[k].name) == 0) {
                found = 1;
                break;
            }
        }
        if (!found) {
            char tmp[256];
            int n = 0, i = 0;
            const char *pre = "MODR: mod '";
            const char *mid = "' vers ";
            const char *tail = " not found \xe2\x80\x94 expected sha ";
            while (pre[i] && n < 250)
                tmp[n++] = pre[i++];
            i = 0;
            while (s->mods[k].name[i] && n < 250)
                tmp[n++] = s->mods[k].name[i++];
            i = 0;
            while (mid[i] && n < 250)
                tmp[n++] = mid[i++];
            {
                char nb[8];
                int m = 0;
                uint32_t v = s->mods[k].vers;
                if (v == 0)
                    nb[m++] = '0';
                while (v > 0 && m < 6) {
                    nb[m++] = (char)('0' + v % 10u);
                    v /= 10u;
                }
                while (m > 0 && n < 250)
                    tmp[n++] = nb[--m];
            }
            i = 0;
            while (tail[i] && n < 250)
                tmp[n++] = tail[i++];
            i = 0;
            while (s->mods[k].sha[i] && n < 250)
                tmp[n++] = s->mods[k].sha[i++];
            tmp[n] = '\0';
            put_err(warn, cap, tmp);
            return 1;
        }
    }
    if (warn && cap > 0u)
        warn[0] = '\0';
    return 0;
}

int rbng_art_fallback(const struct RISong *s) {
    uint16_t k;
    for (k = 0; k < s->nunknown; k++) {
        if (strcmp(s->unknown[k].id, "SKIN") == 0)
            return 0;
    }
    return 1;
}

/* ---- test-only mutation helpers ---- */

int rbng_test_inject_unknown(const char *src, const char *dst,
    const char id[4], const void *data, uint32_t len) {
    FILE *f = fopen(src, "rb");
    FILE *o;
    uint32_t n = 0, total;
    int ch;
    static unsigned char img[RI_RBNG_MAX_FILE + 2048];
    if (!f || !id || (len > 0u && !data) || len > RI_RBNG_MAX_UNK_BYTES)
        return 2;
    while ((ch = fgetc(f)) != EOF) {
        if (n >= sizeof img) {
            fclose(f);
            return 2;
        }
        img[n++] = (unsigned char)ch;
    }
    fclose(f);
    if (n < 12u || memcmp(img, "FORM", 4) != 0)
        return 2;
    total = rd32be(img + 4) + 8u + len + (len & 1u);
    img[4] = (unsigned char)((total >> 24) & 0xffu);
    img[5] = (unsigned char)((total >> 16) & 0xffu);
    img[6] = (unsigned char)((total >> 8) & 0xffu);
    img[7] = (unsigned char)(total & 0xffu);
    o = fopen(dst, "wb");
    if (!o)
        return 2;
    if (fwrite(img, 1, n, o) != n) {
        fclose(o);
        return 2;
    }
    fwrite(id, 1, 4, o);
    fputc((int)((len >> 24) & 0xffu), o);
    fputc((int)((len >> 16) & 0xffu), o);
    fputc((int)((len >> 8) & 0xffu), o);
    fputc((int)(len & 0xffu), o);
    if (len > 0u)
        fwrite(data, 1, len, o);
    if (len & 1u)
        fputc(0, o);
    if (fclose(o) != 0)
        return 2;
    return 0;
}

int rbng_test_set_vers(const char *src, const char *dst, uint16_t major,
    uint16_t minor, uint32_t flags) {
    FILE *f = fopen(src, "rb");
    FILE *o;
    uint32_t n = 0, off = 12, total;
    int ch;
    static unsigned char img[RI_RBNG_MAX_FILE];
    if (!f)
        return 2;
    while ((ch = fgetc(f)) != EOF) {
        if (n >= sizeof img) {
            fclose(f);
            return 2;
        }
        img[n++] = (unsigned char)ch;
    }
    fclose(f);
    if (n < 12u || memcmp(img, "FORM", 4) != 0)
        return 2;
    total = rd32be(img + 4);
    if (total + 8u != n)
        return 2;
    while (off < n) {
        uint32_t size = rd32be(img + off + 4);
        if (memcmp(img + off, "VERS", 4) == 0 && size == 8u) {
            img[off + 8] = (unsigned char)((major >> 8) & 0xffu);
            img[off + 9] = (unsigned char)(major & 0xffu);
            img[off + 10] = (unsigned char)((minor >> 8) & 0xffu);
            img[off + 11] = (unsigned char)(minor & 0xffu);
            img[off + 12] = (unsigned char)((flags >> 24) & 0xffu);
            img[off + 13] = (unsigned char)((flags >> 16) & 0xffu);
            img[off + 14] = (unsigned char)((flags >> 8) & 0xffu);
            img[off + 15] = (unsigned char)(flags & 0xffu);
            o = fopen(dst, "wb");
            if (!o)
                return 2;
            if (fwrite(img, 1, n, o) != n) {
                fclose(o);
                return 2;
            }
            if (fclose(o) != 0)
                return 2;
            return 0;
        }
        off = off + 8u + size + (size & 1u);
    }
    return 2;
}
