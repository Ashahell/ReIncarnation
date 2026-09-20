/* rbnm.c — RBNM S909-subset validator/writer/loader (Task 9, gate G9).
 * No allocation: one static image buffer (bounded RBNM_MAX_FILE); all
 * tables are caller/static. Never trusts chunk sizes for allocation —
 * every length is range-checked against the image before use.
 */
#include "project/rbnm.h"
#include <stdio.h>
#include <string.h>

#define RBNM_MAX_FILE 2097152u /* 2 MiB: 14 layers x ~1 s x 44.1k x 2 B */
#define RBNM_MAX_LAYERS 16u
#define RBNM_MAX_ID 31u

static unsigned char RI_IMG[RBNM_MAX_FILE];

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

/* Render "ckID @off: msg" into err (id printable ASCII or hex). */
static void ck_err(char *err, uint32_t cap, const unsigned char *id,
    uint32_t off, const char *msg) {
    char tmp[160];
    int k = 0, i;
    for (i = 0; i < 4 && k < 150; i++) {
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
        while (n > 0 && k < 148)
            tmp[k++] = nb[--n];
    }
    tmp[k++] = ':';
    tmp[k++] = ' ';
    while (*msg && k < 155)
        tmp[k++] = *msg++;
    tmp[k] = '\0';
    put_err(err, cap, tmp);
}

static uint32_t rd32be(const unsigned char *p) {
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) |
        ((uint32_t)p[2] << 8) | (uint32_t)p[3];
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
        if (n >= RBNM_MAX_FILE) {
            fclose(f);
            put_err(err, errcap, "file too large");
            return 0;
        }
        RI_IMG[n++] = (unsigned char)ch;
    }
    fclose(f);
    if (n == 0u)
        put_err(err, errcap, "empty file");
    return n;
}

/* ---- MANF text validation ---- */

static const char *const RI_KEYS[RBNM_NKEYS] = {
    "src", "date", "equip", "lic", "holder", "proc", "fmt", "norm",
    "loop", "map"
};

/* One line: "ID k=v;k=v;..." Returns 0 ok (id_out filled), else -1. */
static int manf_line(const char *ln, char *id_out, char *err, uint32_t errcap,
    uint32_t lineno) {
    const char *p = ln;
    uint32_t il = 0, k;
    int seen[RBNM_NKEYS];
    for (k = 0; k < RBNM_NKEYS; k++)
        seen[k] = 0;
    while (*p == ' ' || *p == '\t')
        p++;
    if (*p == '\0' || *p == '#')
        return 1; /* blank/comment: skip */
    while (*p && *p != ' ' && *p != '\t' && il < RBNM_MAX_ID) {
        id_out[il++] = *p++;
    }
    id_out[il] = '\0';
    if (il == 0u || il >= RBNM_MAX_ID) {
        char tmp[96];
        snprintf(tmp, sizeof tmp, "MANF line %u: bad layer id", lineno);
        put_err(err, errcap, tmp);
        return -1;
    }
    while (*p == ' ' || *p == '\t')
        p++;
    /* fields split by ';', each k=v with non-empty v */
    while (*p) {
        const char *ks = p;
        uint32_t kl = 0, vl = 0;
        int ki = -1;
        while (*p && *p != '=' && *p != ';') {
            p++;
            kl++;
        }
        if (*p != '=') {
            char tmp[96];
            snprintf(tmp, sizeof tmp, "MANF line %u: field without '='", lineno);
            put_err(err, errcap, tmp);
            return -1;
        }
        p++;
        while (*p && *p != ';') {
            p++;
            vl++;
        }
        if (kl == 0u || vl == 0u) {
            char tmp[96];
            snprintf(tmp, sizeof tmp, "MANF line %u: empty key or value", lineno);
            put_err(err, errcap, tmp);
            return -1;
        }
        for (k = 0; k < RBNM_NKEYS; k++) {
            if (strlen(RI_KEYS[k]) == kl && strncmp(RI_KEYS[k], ks, kl) == 0)
                ki = (int)k;
        }
        if (ki < 0) {
            char tmp[96];
            snprintf(tmp, sizeof tmp, "MANF line %u: unknown key", lineno);
            put_err(err, errcap, tmp);
            return -1;
        }
        if (seen[ki]) {
            char tmp[96];
            snprintf(tmp, sizeof tmp, "MANF line %u: duplicate key", lineno);
            put_err(err, errcap, tmp);
            return -1;
        }
        seen[ki] = 1;
        if (*p == ';')
            p++;
    }
    for (k = 0; k < RBNM_NKEYS; k++) {
        if (!seen[k]) {
            char tmp[128];
            snprintf(tmp, sizeof tmp, "MANF line %u (%s): missing key '%s'",
                lineno, id_out, RI_KEYS[k]);
            put_err(err, errcap, tmp);
            return -1;
        }
    }
    return 0;
}

int rbnm_validate_manifest_text(const char *text, char *err, uint32_t errcap) {
    char line[1024], id[RBNM_MAX_ID + 1u];
    char seen_ids[RBNM_MAX_LAYERS][RBNM_MAX_ID + 1u];
    uint32_t nids = 0, lineno = 0, li;
    const char *p = text;
    if (!text || !*text) {
        put_err(err, errcap, "MANF: empty text");
        return 1;
    }
    while (*p) {
        uint32_t n = 0;
        int rc;
        uint32_t k;
        lineno++;
        while (*p && *p != '\n' && n + 1u < sizeof line)
            line[n++] = *p++;
        line[n] = '\0';
        if (*p == '\n')
            p++;
        rc = manf_line(line, id, err, errcap, lineno);
        if (rc < 0)
            return 1;
        if (rc > 0)
            continue;
        for (k = 0; k < nids; k++) {
            if (strcmp(seen_ids[k], id) == 0) {
                char tmp[96];
                snprintf(tmp, sizeof tmp, "MANF line %u: duplicate layer '%s'",
                    lineno, id);
                put_err(err, errcap, tmp);
                return 1;
            }
        }
        if (nids >= RBNM_MAX_LAYERS) {
            put_err(err, errcap, "MANF: too many layers");
            return 1;
        }
        for (li = 0; id[li]; li++)
            seen_ids[nids][li] = id[li];
        seen_ids[nids][li] = '\0';
        nids++;
    }
    if (nids == 0u) {
        put_err(err, errcap, "MANF: no layer rows");
        return 1;
    }
    return 0;
}

/* ---- image model (parsed once, cross-checked) ---- */

struct LayerRow {
    char id[RBNM_MAX_ID + 1u];
    uint8_t voice;
    uint32_t rate;
    uint32_t frames;
    uint8_t lo;
    uint8_t hi;
    int has_smpl;
    uint32_t smpl_frames;
    int has_manf;
    char fmt_rate[16]; /* RATE parsed out of MANF fmt for the rate check */
};

static int find_row(struct LayerRow *rows, uint32_t n, const char *id) {
    uint32_t k;
    for (k = 0; k < n; k++)
        if (strcmp(rows[k].id, id) == 0)
            return (int)k;
    return -1;
}

/* Parse the image; fills rows (S909), marks SMPL/MANF presence. */
static int parse_image(const unsigned char *img, uint32_t n,
    struct LayerRow *rows, uint32_t *nrows, char *err, uint32_t errcap) {
    uint32_t off = 0, total;
    int saw_s909 = 0;
    static unsigned char manf[65536];
    uint32_t manf_n = 0;
    int saw_manf = 0;
    *nrows = 0;
    if (n < 12u || memcmp(img, "FORM", 4) != 0) {
        put_err(err, errcap, "not FORM");
        return 1;
    }
    total = rd32be(img + 4);
    if (memcmp(img + 8, "RBNM", 4) != 0) {
        put_err(err, errcap, "FORM type not RBNM");
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
        if (memcmp(cid, "S909", 4) == 0) {
            const unsigned char *q = img + doff;
            uint32_t left = size, nl, k;
            if (saw_s909) {
                ck_err(err, errcap, cid, off, "duplicate S909");
                return 1;
            }
            saw_s909 = 1;
            if (left < 2u) {
                ck_err(err, errcap, cid, off, "S909 too short");
                return 1;
            }
            nl = ((uint32_t)q[0] << 8) | q[1];
            q += 2;
            left -= 2u;
            if (nl == 0u || nl > RBNM_MAX_LAYERS) {
                ck_err(err, errcap, cid, off, "S909 bad layer count");
                return 1;
            }
            for (k = 0; k < nl; k++) {
                struct LayerRow *r;
                uint32_t idlen, rate, frames;
                if (left < 1u + 1u + 4u + 4u + 1u + 1u) {
                    ck_err(err, errcap, cid, off, "S909 entry truncated");
                    return 1;
                }
                if (*nrows >= RBNM_MAX_LAYERS) {
                    ck_err(err, errcap, cid, off, "S909 too many layers");
                    return 1;
                }
                r = &rows[(*nrows)];
                r->voice = q[0];
                idlen = q[1];
                q += 2;
                left -= 2u;
                if (idlen == 0u || idlen > RBNM_MAX_ID || idlen + 4u + 4u + 2u > left) {
                    ck_err(err, errcap, cid, off, "S909 bad id length");
                    return 1;
                }
                memcpy(r->id, q, idlen);
                r->id[idlen] = '\0';
                q += idlen;
                left -= idlen;
                rate = rd32be(q);
                frames = rd32be(q + 4);
                q += 8;
                left -= 8u;
                r->rate = rate;
                r->frames = frames;
                r->lo = q[0];
                r->hi = q[1];
                q += 2;
                left -= 2u;
                if (r->voice > 15u || rate == 0u || frames == 0u || r->lo > r->hi) {
                    ck_err(err, errcap, cid, off, "S909 bad voice/rate/frames/span");
                    return 1;
                }
                if (find_row(rows, *nrows, r->id) >= 0) {
                    ck_err(err, errcap, cid, off, "S909 duplicate layer id");
                    return 1;
                }
                r->has_smpl = 0;
                r->has_manf = 0;
                r->smpl_frames = 0;
                r->fmt_rate[0] = '\0';
                (*nrows)++;
            }
            if (left != 0u) {
                ck_err(err, errcap, cid, off, "S909 trailing bytes");
                return 1;
            }
        } else if (memcmp(cid, "MANF", 4) == 0) {
            if (saw_manf) {
                ck_err(err, errcap, cid, off, "duplicate MANF");
                return 1;
            }
            saw_manf = 1;
            if (size >= sizeof manf) {
                ck_err(err, errcap, cid, off, "MANF too large");
                return 1;
            }
            memcpy(manf, img + doff, size);
            manf[size] = '\0';
            manf_n = size;
        } else if (memcmp(cid, "SMPL", 4) == 0) {
            const unsigned char *q = img + doff;
            uint32_t left = size, idlen, frames, k;
            char id[RBNM_MAX_ID + 1u];
            int ri;
            if (left < 1u + 4u) {
                ck_err(err, errcap, cid, off, "SMPL too short");
                return 1;
            }
            idlen = q[0];
            q++;
            left--;
            if (idlen == 0u || idlen > RBNM_MAX_ID || idlen + 4u > left) {
                ck_err(err, errcap, cid, off, "SMPL bad id length");
                return 1;
            }
            memcpy(id, q, idlen);
            id[idlen] = '\0';
            q += idlen;
            left -= idlen;
            frames = rd32be(q);
            q += 4;
            left -= 4u;
            if (left != frames * 2u) {
                ck_err(err, errcap, cid, off, "SMPL data length != frames*2");
                return 1;
            }
            ri = find_row(rows, *nrows, id);
            if (ri < 0) {
                /* SMPL before S909 (chunk order) or unknown id. Order
                 * is legal IFF: defer by accepting forward refs only if
                 * S909 was already seen. */
                if (!saw_s909) {
                    ck_err(err, errcap, cid, off, "SMPL before S909");
                    return 1;
                }
                ck_err(err, errcap, cid, off, "SMPL for unknown layer");
                return 1;
            }
            if (rows[ri].has_smpl) {
                ck_err(err, errcap, cid, off, "SMPL duplicate layer");
                return 1;
            }
            rows[ri].has_smpl = 1;
            rows[ri].smpl_frames = frames;
            for (k = 0; k < frames * 2u; k += 2u) {
                (void)q[k]; /* touch discipline: bytes are validated by count */
            }
        } else if (memcmp(cid, "CPRG", 4) == 0 || memcmp(cid, "PAD ", 4) == 0 ||
            (cid[0] >= 'A' && cid[0] <= 'Z')) {
            /* Unknown optional chunk: length-delimited skip (spec §13).
             * Uppercase IDs are the forward-compatible namespace; anything
             * else is rejected below. */
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
    if (!saw_s909) {
        put_err(err, errcap, "missing S909 chunk");
        return 1;
    }
    if (!saw_manf) {
        put_err(err, errcap, "missing MANF chunk");
        return 1;
    }
    /* MANF rows: field-complete + id set == S909 set. */
    {
        char line[1024], id[RBNM_MAX_ID + 1u];
        const char *p = (const char *)manf;
        const char *end = (const char *)manf + manf_n;
        uint32_t lineno = 0;
        uint32_t k;
        (void)end;
        while (*p) {
            uint32_t m = 0;
            int rc;
            lineno++;
            while (*p && *p != '\n' && m + 1u < sizeof line)
                line[m++] = *p++;
            line[m] = '\0';
            if (*p == '\n')
                p++;
            rc = manf_line(line, id, err, errcap, lineno);
            if (rc < 0)
                return 1;
            if (rc > 0)
                continue;
            {
                int ri = find_row(rows, *nrows, id);
                if (ri < 0) {
                    char tmp[96];
                    snprintf(tmp, sizeof tmp, "MANF line %u: unknown layer '%s'",
                        lineno, id);
                    put_err(err, errcap, tmp);
                    return 1;
                }
                if (rows[ri].has_manf) {
                    char tmp[96];
                    snprintf(tmp, sizeof tmp, "MANF line %u: duplicate layer '%s'",
                        lineno, id);
                    put_err(err, errcap, tmp);
                    return 1;
                }
                rows[ri].has_manf = 1;
                /* fmt "RATE/DEPTH": RATE must equal the S909 rate. */
                {
                    const char *f = strstr(line, "fmt=");
                    uint32_t rv = 0;
                    if (!f) {
                        char tmp[96];
                        snprintf(tmp, sizeof tmp, "MANF line %u: fmt unreadable",
                            lineno);
                        put_err(err, errcap, tmp);
                        return 1;
                    }
                    f += 4;
                    if (*f < '0' || *f > '9') {
                        char tmp[96];
                        snprintf(tmp, sizeof tmp, "MANF line %u: fmt bad rate",
                            lineno);
                        put_err(err, errcap, tmp);
                        return 1;
                    }
                    while (*f >= '0' && *f <= '9') {
                        rv = rv * 10u + (uint32_t)(*f - '0');
                        f++;
                    }
                    if (rv != rows[ri].rate) {
                        char tmp[128];
                        snprintf(tmp, sizeof tmp,
                            "MANF line %u (%s): fmt rate %u != S909 %u",
                            lineno, id, rv, rows[ri].rate);
                        put_err(err, errcap, tmp);
                        return 1;
                    }
                }
            }
        }
        for (k = 0; k < *nrows; k++) {
            if (!rows[k].has_manf) {
                char tmp[96];
                snprintf(tmp, sizeof tmp, "layer '%s': MANF row missing",
                    rows[k].id);
                put_err(err, errcap, tmp);
                return 1;
            }
            if (!rows[k].has_smpl) {
                char tmp[96];
                snprintf(tmp, sizeof tmp, "layer '%s': SMPL chunk missing",
                    rows[k].id);
                put_err(err, errcap, tmp);
                return 1;
            }
            if (rows[k].smpl_frames != rows[k].frames) {
                char tmp[128];
                snprintf(tmp, sizeof tmp, "layer '%s': SMPL frames %u != S909 %u",
                    rows[k].id, rows[k].smpl_frames, rows[k].frames);
                put_err(err, errcap, tmp);
                return 1;
            }
        }
    }
    return 0;
}

int rbnm_validate_file(const char *path, char *err, uint32_t errcap) {
    static struct LayerRow rows[RBNM_MAX_LAYERS];
    uint32_t n = read_file(path, err, errcap), nrows = 0;
    if (n == 0u)
        return 1;
    return parse_image(RI_IMG, n, rows, &nrows, err, errcap);
}

/* ---- writer (fixtures/tools) ---- */

static void wr32be(FILE *f, uint32_t v) {
    fputc((int)((v >> 24) & 0xffu), f);
    fputc((int)((v >> 16) & 0xffu), f);
    fputc((int)((v >> 8) & 0xffu), f);
    fputc((int)(v & 0xffu), f);
}

int rbnm_write_pack(const char *path, const struct RBNMWriteLayer *L,
    uint32_t n, const char *manifest) {
    FILE *f;
    uint32_t k, mlen, s909len = 2u;
    if (!path || !L || n == 0u || n > RBNM_MAX_LAYERS || !manifest)
        return 2;
    for (k = 0; k < n; k++) {
        uint32_t idlen = (uint32_t)strlen(L[k].id);
        if (idlen == 0u || idlen > RBNM_MAX_ID || !L[k].pcm ||
            L[k].frames == 0u || L[k].rate == 0u || L[k].lo > L[k].hi)
            return 2;
        s909len += 1u + 1u + idlen + 4u + 4u + 1u + 1u;
    }
    mlen = (uint32_t)strlen(manifest);
    /* Single emit pass: total is computed up front so chunk lengths are
     * exact (no seeking games). */
    f = fopen(path, "wb");
    if (!f)
        return 2;
    {
        uint32_t total = 4u; /* 'RBNM' */
        uint32_t k2;
        for (k2 = 0; k2 < n; k2++) {
            uint32_t idlen = (uint32_t)strlen(L[k2].id);
            uint32_t dlen = 1u + idlen + 4u + L[k2].frames * 2u;
            (void)dlen;
        }
        total += 8u + s909len + (s909len & 1u);
        total += 8u + mlen + (mlen & 1u);
        for (k2 = 0; k2 < n; k2++) {
            uint32_t idlen = (uint32_t)strlen(L[k2].id);
            uint32_t dlen = 1u + idlen + 4u + L[k2].frames * 2u;
            total += 8u + dlen + (dlen & 1u);
        }
        fwrite("FORM", 1, 4, f);
        wr32be(f, total);
        fwrite("RBNM", 1, 4, f);
        fwrite("S909", 1, 4, f);
        wr32be(f, s909len);
        fputc((int)((n >> 8) & 0xffu), f);
        fputc((int)(n & 0xffu), f);
        for (k2 = 0; k2 < n; k2++) {
            uint32_t idlen = (uint32_t)strlen(L[k2].id);
            fputc((int)L[k2].voice, f);
            fputc((int)idlen, f);
            fwrite(L[k2].id, 1, idlen, f);
            wr32be(f, L[k2].rate);
            wr32be(f, L[k2].frames);
            fputc((int)L[k2].lo, f);
            fputc((int)L[k2].hi, f);
        }
        if (s909len & 1u)
            fputc(0, f);
        fwrite("MANF", 1, 4, f);
        wr32be(f, mlen);
        fwrite(manifest, 1, mlen, f);
        if (mlen & 1u)
            fputc(0, f);
        for (k2 = 0; k2 < n; k2++) {
            uint32_t idlen = (uint32_t)strlen(L[k2].id);
            uint32_t dlen = 1u + idlen + 4u + L[k2].frames * 2u;
            uint32_t s;
            fwrite("SMPL", 1, 4, f);
            wr32be(f, dlen);
            fputc((int)idlen, f);
            fwrite(L[k2].id, 1, idlen, f);
            wr32be(f, L[k2].frames);
            for (s = 0; s < L[k2].frames; s++) {
                uint16_t v = (uint16_t)L[k2].pcm[s];
                fputc((int)((v >> 8) & 0xffu), f);
                fputc((int)(v & 0xffu), f);
            }
            if (dlen & 1u)
                fputc(0, f);
        }
    }
    if (fclose(f) != 0)
        return 2;
    return 0;
}

int32_t rbnm_pack_layers(const char *path, struct RBNMLayerInfo *out,
    uint32_t cap, char *err, uint32_t errcap) {
    static struct LayerRow rows[RBNM_MAX_LAYERS];
    uint32_t n = read_file(path, err, errcap), nrows = 0, k;
    if (n == 0u)
        return -1;
    if (!out || cap == 0u) {
        put_err(err, errcap, "bad inventory args");
        return -1;
    }
    if (parse_image(RI_IMG, n, rows, &nrows, err, errcap) != 0)
        return -1;
    if (nrows > cap) {
        put_err(err, errcap, "caller inventory too small");
        return -1;
    }
    for (k = 0; k < nrows; k++) {
        uint32_t c = 0;
        while (rows[k].id[c]) {
            out[k].id[c] = rows[k].id[c];
            c++;
        }
        out[k].id[c] = '\0';
        out[k].voice = rows[k].voice;
        out[k].rate = rows[k].rate;
        out[k].frames = rows[k].frames;
        out[k].lo = rows[k].lo;
        out[k].hi = rows[k].hi;
    }
    return (int32_t)nrows;
}

int32_t rbnm_load_smpl(const char *path, const char *id, float *dst,
    uint32_t cap, uint32_t *rate_out, char *err, uint32_t errcap) {
    static struct LayerRow rows[RBNM_MAX_LAYERS];
    uint32_t n = read_file(path, err, errcap), nrows = 0, off, total;
    int ri;
    uint32_t k;
    if (n == 0u)
        return -1;
    if (!id || !dst || cap == 0u) {
        put_err(err, errcap, "bad loader args");
        return -1;
    }
    if (parse_image(RI_IMG, n, rows, &nrows, err, errcap) != 0)
        return -1;
    ri = find_row(rows, nrows, id);
    if (ri < 0) {
        put_err(err, errcap, "layer id not in pack");
        return -1;
    }
    if (rows[ri].frames > cap) {
        put_err(err, errcap, "caller buffer too small");
        return -1;
    }
    /* Re-walk to the SMPL payload (offsets already proven in range). */
    total = rd32be(RI_IMG + 4);
    (void)total;
    off = 12;
    while (off < n) {
        uint32_t size = rd32be(RI_IMG + off + 4);
        const unsigned char *cid = RI_IMG + off;
        uint32_t doff = off + 8u;
        if (memcmp(cid, "SMPL", 4) == 0) {
            const unsigned char *q = RI_IMG + doff;
            uint32_t idlen = q[0];
            if (idlen == strlen(id) && memcmp(q + 1, id, idlen) == 0) {
                const unsigned char *d = q + 1u + idlen + 4u;
                for (k = 0; k < rows[ri].frames; k++) {
                    int16_t v = (int16_t)(((uint16_t)d[2u * k] << 8) |
                        (uint16_t)d[2u * k + 1u]);
                    dst[k] = (float)v / 32768.0f;
                }
                if (rate_out)
                    *rate_out = rows[ri].rate;
                return (int32_t)rows[ri].frames;
            }
        }
        off = doff + size + (size & 1u);
    }
    put_err(err, errcap, "SMPL payload unreachable");
    return -1;
}
