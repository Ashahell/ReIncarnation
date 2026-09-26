/* gui/skin.c — skin/mod appearance core (§12.10 G8.1).
 * Implements gui/skin.h. MODR field rules mirror the rbng codec
 * (project/rbng.c: name 1..63 chars, sha 64 lowercase hex + NUL).
 */
#include "gui/skin.h"
#include <string.h>
#include "gui/ctlreg.h"
#include "gui/panelgeo.h"
#include "project/sha256.h"

#define RI_SKIN_LINE_MAX 255u
#define RI_SKIN_KIND_ANY 0xFFu

static int is_dec(const char *s, uint32_t *v) {
    uint32_t acc = 0u, n = 0u;
    if (!s || !*s)
        return 0;
    while (*s) {
        if (*s < '0' || *s > '9' || n >= 9u)
            return 0;
        acc = acc * 10u + (uint32_t)(*s - '0');
        n++;
        s++;
    }
    if (v)
        *v = acc;
    return 1;
}

static int is_partname(const char *s) {
    uint32_t n = 0u;
    if (!s || !*s)
        return 0;
    while (*s) {
        char c = *s;
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
              (c >= '0' && c <= '9') || c == '.' || c == '_' || c == '-'))
            return 0;
        n++;
        if (n > RI_SKIN_PART_NAME)
            return 0;
        s++;
    }
    return 1;
}

static int is_filename(const char *s) {
    uint32_t n = 0u;
    if (!s || !*s)
        return 0;
    while (*s) {
        char c = *s;
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
              (c >= '0' && c <= '9') || c == '.' || c == '_' ||
              c == '-' || c == '/'))
            return 0;
        n++;
        if (n > RI_SKIN_FILE_NAME)
            return 0;
        s++;
    }
    return 1;
}

static int is_sha64(const char *s) {
    int i;
    if (!s)
        return 0;
    for (i = 0; i < 64; i++) {
        char c = s[i];
        if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f')))
            return 0;
    }
    return s[64] == '\0';
}

/* Split "SEC[.KIND[.PART]]=VALUE": returns pointers into a scratch copy.
 * key_no: 0 = BACKGROUND, 1 = PART, 2 = header key (FORMAT/NAME/VERSION),
 * -2 = unknown prefix (caller ignores), -1 = malformed known key. */
static int split_key(char *key, char **a, char **b, char **c) {
    if (!strcmp(key, "FORMAT") || !strcmp(key, "NAME") || !strcmp(key, "VERSION")) {
        *a = key;
        *b = *c = 0;
        return 2;
    }
    if (!strncmp(key, "BACKGROUND.", 11)) {
        *a = key + 11;
        *b = *c = 0;
        return **a ? 0 : -1;
    }
    if (!strncmp(key, "PART.", 5)) {
        char *p = key + 5, *q;
        *a = p;
        q = strchr(p, '.');
        if (!q)
            return -1;
        *q = '\0';
        *b = q + 1;
        q = strchr(*b, '.');
        if (!q)
            return -1;
        *q = '\0';
        *c = q + 1;
        if (!**a || !**b || !**c)
            return -1;
        return 1;
    }
    return -2;
}

static int is_name(const char *s) { /* part-name charset, 1..63 chars */
    uint32_t n = 0u;
    if (!s || !*s)
        return 0;
    for (; s[n]; n++) {
        char c = s[n];
        if (n >= RI_SKIN_NAME || !((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
              (c >= '0' && c <= '9') || c == '.' || c == '_' || c == '-'))
            return 0;
    }
    return 1;
}

int ri_skin_parse(const char *text, struct RISkin *s) {
    static const char *const hdr_keys[3] = { "FORMAT", "NAME", "VERSION" };
    const char *p;
    int nhdr = 0;
    if (!text || !s)
        return -1;
    memset(s, 0, sizeof *s);
    s->stale_idx = -1;
    for (p = text; *p; p++) { /* format 1 text is printable ASCII */
        unsigned char ch = (unsigned char)*p;
        if (!((ch >= 0x20u && ch <= 0x7Eu) || ch == '\t' || ch == '\r' || ch == '\n'))
            goto fail8;
    }
    p = text;
    for (;;) {
        const char *eol;
        uint32_t len, i;
        char line[RI_SKIN_LINE_MAX + 2u];
        char *key, *val, *a, *b, *c;
        uint32_t frames, num;
        int k, sec, kind;
        struct RISkinPart *pt;
        eol = strchr(p, '\n');
        len = eol ? (uint32_t)(eol - p) : (uint32_t)strlen(p);
        if (len > RI_SKIN_LINE_MAX)
            goto fail2;
        memcpy(line, p, len);
        line[len] = '\0';
        if (len > 0u && line[len - 1u] == '\r')
            line[len - 1u] = '\0';
        p = eol ? eol + 1u : p + len;
        /* skip leading whitespace; blank/comment lines carry nothing */
        key = line;
        while (*key == ' ' || *key == '\t')
            key++;
        if (*key == '\0' || *key == '#') {
            if (!eol)
                break;
            continue;
        }
        val = strchr(key, '=');
        if (!val || val == key || val[1] == '\0')
            goto fail3;
        *val = '\0';
        val++;
        k = split_key(key, &a, &b, &c);
        if (nhdr < 3) { /* FORMAT, NAME, VERSION: first, in order */
            if (k != 2 || strcmp(a, hdr_keys[nhdr]))
                goto fail7;
            if (nhdr == 0) {
                if (!is_dec(val, &num) || num != RI_SKIN_FORMAT)
                    goto fail7;
            } else if (nhdr == 1) {
                if (!is_name(val))
                    goto fail5;
                for (i = 0u; val[i]; i++)
                    s->name[i] = val[i];
            } else {
                if (!is_dec(val, &num) || num == 0u || num > 65535u)
                    goto fail5;
                s->version = (uint16_t)num;
            }
            nhdr++;
            goto next;
        }
        if (k == 2)
            goto fail6; /* header key repeated */
        if (k == -2)
            goto next; /* unknown keys ignored */
        if (k < 0)
            goto fail3;
        if (k == 0) { /* BACKGROUND.<section>=file */
            if (!is_filename(val))
                goto fail5;
            kind = RI_SKIN_KIND_ANY;
            frames = 1u;
            c = (char *)"background";
        } else { /* PART.<section>.<kind>.<part>=file,NFRAMES */
            char *comma = strchr(val, ',');
            if (!comma || comma == val || comma[1] == '\0' ||
                strchr(comma + 1, ','))
                goto fail3;
            *comma = '\0';
            if (!is_partname(c) || !is_filename(val) ||
                !is_dec(comma + 1, &frames) ||
                frames == 0u || frames > RI_SKIN_MAX_FRAMES)
                goto fail5;
            kind = ri_ctlreg_kind_by_token(b);
        }
        sec = ri_ctlreg_section_by_token(a);
        if (sec < 0 || kind < 0)
            goto next; /* a later build's device or kind: ignored, syntax checked */
        for (i = 0u; i < s->nparts; i++) {
            if (s->parts[i].section == (uint8_t)sec &&
                s->parts[i].kind == (uint8_t)kind &&
                !strcmp(s->parts[i].part, c))
                goto fail6;
        }
        if (s->nparts >= RI_SKIN_MAX_PARTS)
            goto fail4;
        pt = &s->parts[s->nparts++];
        pt->section = (uint8_t)sec;
        pt->kind = (uint8_t)kind;
        for (i = 0u; c[i] && i <= RI_SKIN_PART_NAME; i++)
            pt->part[i] = c[i];
        pt->part[RI_SKIN_PART_NAME] = '\0';
        for (i = 0u; val[i] && i <= RI_SKIN_FILE_NAME; i++)
            pt->file[i] = val[i];
        pt->file[RI_SKIN_FILE_NAME] = '\0';
        pt->frames = (uint16_t)frames;
next:
        if (!eol)
            break;
    }
    if (nhdr < 3)
        goto fail7;
    return 0; /* header-only manifest: valid empty skin (Template) */
fail2:
    memset(s, 0, sizeof *s);
    return -2;
fail3:
    memset(s, 0, sizeof *s);
    return -3;
fail4:
    memset(s, 0, sizeof *s);
    return -4;
fail5:
    memset(s, 0, sizeof *s);
    return -5;
fail6:
    memset(s, 0, sizeof *s);
    return -6;
fail7:
    memset(s, 0, sizeof *s);
    return -7;
fail8:
    memset(s, 0, sizeof *s);
    return -8;
}

int ri_skin_find(const struct RISkin *s, uint8_t section, uint8_t kind,
                 const char *part) {
    uint32_t i;
    if (!s || !part)
        return -1;
    for (i = 0u; i < s->nparts; i++) {
        if (s->parts[i].section == section &&
            (s->parts[i].kind == kind || s->parts[i].kind == RI_SKIN_KIND_ANY) &&
            !strcmp(s->parts[i].part, part))
            return (int)i;
    }
    return -1;
}

int ri_skin_bind(struct RISkin *s, uint32_t idx, const uint32_t *rgba,
                 uint16_t w, uint16_t h) {
    uint32_t ew, eh;
    if (!s || idx >= s->nparts || !rgba || w == 0u || h == 0u)
        return -1;
    if (ri_skin_expect(s->parts[idx].section, s->parts[idx].kind, s->parts[idx].part,
                       s->parts[idx].frames, &ew, &eh) == 1 && (ew != w || eh != h)) {
        s->parts[idx].rgba = 0; /* stale art: Classic fallback, reported */
        s->parts[idx].w = s->parts[idx].h = 0u;
        if (s->nstale == 0u || s->stale_idx < 0)
            s->stale_idx = (int16_t)idx;
        s->nstale++;
        return -2;
    }
    s->parts[idx].rgba = rgba;
    s->parts[idx].w = w;
    s->parts[idx].h = h;
    return 0;
}

int ri_skin_bind_zoom(struct RISkin *s, uint32_t idx, const uint32_t *rgba,
                      uint16_t w, uint16_t h) {
    if (!s || idx >= s->nparts || !rgba || w == 0u || h == 0u)
        return -1;
    s->parts[idx].zrgba = rgba;
    s->parts[idx].zw = w;
    s->parts[idx].zh = h;
    return 0;
}

void ri_skin_downscale(const uint32_t *src, uint32_t sw, uint32_t sh,
                       uint32_t *dst, uint32_t dw, uint32_t dh) {
    uint32_t dx, dy;
    if (!src || !dst || sw == 0u || sh == 0u || dw == 0u || dh == 0u ||
        dw > sw || dh > sh)
        return;
    for (dy = 0u; dy < dh; dy++) {
        uint32_t y0 = (dy * sh) / dh, y1 = ((dy + 1u) * sh) / dh;
        for (dx = 0u; dx < dw; dx++) {
            uint32_t x0 = (dx * sw) / dw, x1 = ((dx + 1u) * sw) / dw;
            uint64_t sa = 0u, sr = 0u, sg = 0u, sb = 0u, n = 0u;
            uint32_t x, y, a, r, g, b;
            for (y = y0; y < y1; y++) {
                for (x = x0; x < x1; x++) {
                    uint32_t p = src[y * sw + x];
                    a = (p >> 24) & 0xFFu;
                    r = (p >> 16) & 0xFFu;
                    g = (p >> 8) & 0xFFu;
                    b = p & 0xFFu;
                    sa += a;
                    /* Premultiplied 0..255 (truncating; pinned by t75). */
                    sr += (uint64_t)r * a / 255u;
                    sg += (uint64_t)g * a / 255u;
                    sb += (uint64_t)b * a / 255u;
                    n++;
                }
            }
            if (n == 0u) {
                dst[dy * dw + dx] = 0u;
                continue;
            }
            a = (uint32_t)(sa / n);
            if (a == 0u) {
                dst[dy * dw + dx] = 0u;
                continue;
            }
            /* Truncating division throughout: exact, no libm (pinned by t75). */
            r = (uint32_t)((sr / n) * 255u / a);
            g = (uint32_t)((sg / n) * 255u / a);
            b = (uint32_t)((sb / n) * 255u / a);
            if (r > 255u) r = 255u;
            if (g > 255u) g = 255u;
            if (b > 255u) b = 255u;
            dst[dy * dw + dx] = (a << 24) | (r << 16) | (g << 8) | b;
        }
    }
}

int ri_skin_identity(const struct RISkin *s, const char *manifest,
                     int (*read_file)(const char *name, unsigned char *buf,
                                      uint32_t cap),
                     unsigned char *scratch, uint32_t scratch_cap,
                     char hex[65]) {
    struct RISha256 c;
    unsigned char digest[32];
    uint16_t i;
    int k;
    static const char nib[17] = "0123456789abcdef";
    if (!s || !manifest || !read_file || !scratch || !hex)
        return -1;
    if (scratch_cap < 4096u)
        return -2;
    ri_sha256_init(&c);
    ri_sha256_add(&c, manifest, (uint32_t)strlen(manifest));
    for (i = 0u; i < s->nparts; i++) {
        for (;;) {
            int n = read_file(s->parts[i].file, scratch, scratch_cap);
            uint32_t j;
            if (n < 0)
                return -3;
            if (n == 0)
                break;
            for (j = 0u; j < (uint32_t)n; j += 4096u) {
                uint32_t m = (uint32_t)n - j;
                if (m > 4096u)
                    m = 4096u;
                ri_sha256_add(&c, scratch + j, m);
            }
            if ((uint32_t)n < scratch_cap)
                break;
        }
    }
    ri_sha256_end(&c, digest);
    for (k = 0; k < 32; k++) {
        hex[2 * k] = nib[(digest[k] >> 4) & 0xFu];
        hex[2 * k + 1] = nib[digest[k] & 0xFu];
    }
    hex[64] = '\0';
    return 0;
}

int ri_skin_modref_get(const struct RISong *song, uint32_t idx,
                       char name[64], char sha[65], uint16_t *vers) {
    uint32_t i;
    if (!song || idx >= song->nmods || !name || !sha || !vers)
        return -1;
    for (i = 0u; i <= RI_RBNG_MAX_MOD_NAME; i++)
        name[i] = song->mods[idx].name[i];
    name[RI_RBNG_MAX_MOD_NAME] = '\0';
    for (i = 0u; i < 65u; i++)
        sha[i] = song->mods[idx].sha[i];
    sha[64] = '\0';
    *vers = song->mods[idx].vers;
    return 0;
}

int ri_skin_modref_set(struct RISong *song, const char *name,
                       const char *sha, uint16_t vers) {
    uint32_t nl, i;
    if (!song || !name || !sha)
        return -1;
    nl = (uint32_t)strlen(name);
    if (nl == 0u || nl > RI_RBNG_MAX_MOD_NAME || !is_sha64(sha))
        return -1;
    for (i = 0u; i < song->nmods; i++) {
        if (!strcmp(song->mods[i].name, name)) {
            for (nl = 0u; nl < 65u; nl++)
                song->mods[i].sha[nl] = sha[nl];
            song->mods[i].sha[64] = '\0';
            song->mods[i].vers = vers;
            return 0;
        }
    }
    if (song->nmods >= RI_RBNG_MAX_MODS)
        return -2;
    for (i = 0u; i <= RI_RBNG_MAX_MOD_NAME; i++)
        song->mods[song->nmods].name[i] = name[i];
    song->mods[song->nmods].name[RI_RBNG_MAX_MOD_NAME] = '\0';
    for (i = 0u; i < 65u; i++)
        song->mods[song->nmods].sha[i] = sha[i];
    song->mods[song->nmods].sha[64] = '\0';
    song->mods[song->nmods].vers = vers;
    song->nmods++;
    return 0;
}

int ri_skin_cycle(const char *current, const char *const *installed,
                  uint32_t n, char next[64]) {
    uint32_t i, k;
    if (!current || !installed || n == 0u || !next)
        return -1;
    for (i = 0u; i < n; i++) {
        if (!strcmp(installed[i], current)) {
            const char *w = installed[(i + 1u) % n];
            for (k = 0u; k < 63u && w[k]; k++)
                next[k] = w[k];
            next[k] = '\0';
            return 0;
        }
    }
    for (k = 0u; k < 63u && installed[0][k]; k++)
        next[k] = installed[0][k];
    next[k] = '\0';
    return 0;
}

uint32_t ri_skin_frame(uint32_t v, uint32_t vmax, uint32_t frames) {
    uint64_t f;
    if (frames == 0u)
        return 0u;
    if (v > vmax)
        v = vmax;
    f = (uint64_t)v * frames / ((uint64_t)vmax + 1u);
    if (f >= frames)
        f = frames - 1u;
    return (uint32_t)f;
}

void ri_skin_swizzle_blit(const uint32_t *src, uint32_t *dst, uint32_t n) {
    uint32_t i;
    if (!src || !dst)
        return;
    for (i = 0u; i < n; i++) {
        uint32_t p = src[i];
        uint32_t a = (p >> 24) & 0xFFu, r = (p >> 16) & 0xFFu;
        uint32_t g = (p >> 8) & 0xFFu, b = p & 0xFFu;
        dst[i] = (b << 24) | (g << 16) | (r << 8) | a;
    }
}

uint32_t ri_skin_zoom_num(int zoom) {
    return zoom == 0 ? 4u : zoom == 1 ? 6u : zoom == 2 ? 8u :
           zoom == RI_GEO_ZOOM_COMPACT ? 3u : 0u;
}

void ri_skin_unbind(struct RISkin *s) {
    uint32_t i;
    if (!s)
        return;
    for (i = 0u; i < s->nparts; i++) {
        s->parts[i].rgba = 0;
        s->parts[i].w = s->parts[i].h = 0u;
        s->parts[i].zrgba = 0;
        s->parts[i].zw = s->parts[i].zh = 0u;
    }
}

/* Distinct knob sizes (2x-master px) of a geometry section, largest area
 * first; returns the count (0..2 kept). Knobs = KNOB shapes whose registry
 * kind is RI_CK_KNOB (the instrument selector is drawn procedurally). */
static uint32_t knob_sizes(uint8_t section, uint32_t w[2], uint32_t h[2]) {
    const struct RIGeoSection *g = ri_geo_section(section == RI_SEC_SYNTH2 ? RI_SEC_SYNTH1 : section);
    uint32_t i, n = 0u;
    if (!g)
        return 0u;
    for (i = 0u; i < g->nitems; i++) {
        const struct RICtlDef *d = ri_ctlreg_find(g->items[i].reg_id);
        uint32_t kw, kh, j, seen = 0u;
        if (g->items[i].shape != RI_GEO_KNOB || !d || d->kind != RI_CK_KNOB)
            continue;
        kw = (uint32_t)ri_geo_px((int)g->items[i].w, 2);
        kh = (uint32_t)ri_geo_px((int)g->items[i].h, 2);
        for (j = 0u; j < n; j++)
            seen |= w[j] == kw && h[j] == kh;
        if (seen)
            continue;
        if (n < 2u) {
            w[n] = kw;
            h[n] = kh;
            n++;
        }
    }
    if (n == 2u && w[1] * h[1] > w[0] * h[0]) {
        uint32_t t = w[0];
        w[0] = w[1];
        w[1] = t;
        t = h[0];
        h[0] = h[1];
        h[1] = t;
    }
    return n;
}

int ri_skin_expect(uint8_t section, uint8_t kind, const char *part,
                   uint32_t frames, uint32_t *w, uint32_t *h) {
    const struct RIGeoSection *g;
    uint32_t kw[2], kh[2], n;
    if (!part || !w || !h || frames == 0u || section >= RI_SEC_COUNT)
        return 0;
    g = ri_geo_section(section == RI_SEC_SYNTH2 ? RI_SEC_SYNTH1 : section);
    if (!g)
        return 0;
    if (kind == RI_SKIN_KIND_ANY && !strcmp(part, "background")) {
        *w = (uint32_t)ri_geo_px((int)g->w, 2);
        *h = (uint32_t)ri_geo_px((int)g->h, 2) * frames;
        return 1;
    }
    if (kind != RI_CK_KNOB)
        return 0;
    n = knob_sizes(section, kw, kh);
    if (!strcmp(part, "knob.frame") && n >= 1u) {
        *w = kw[0];
        *h = kh[0] * frames;
        return 1;
    }
    if (!strcmp(part, "knob.frame.small") && n == 2u) {
        *w = kw[1];
        *h = kh[1] * frames;
        return 1;
    }
    return 0;
}

const char *ri_skin_knob_role(uint8_t section, uint32_t w, uint32_t h) {
    uint32_t kw[2], kh[2], n = knob_sizes(section, kw, kh);
    if (n >= 1u && kw[0] == w && kh[0] == h)
        return "knob.frame";
    if (n == 2u && kw[1] == w && kh[1] == h)
        return "knob.frame.small";
    return 0;
}

static uint32_t key_put(char *buf, uint32_t cap, uint32_t at, const char *t) {
    while (t && *t && at < cap)
        buf[at++] = *t++;
    return (t && *t) ? cap : at;
}

int ri_skin_key(const struct RISkin *s, uint32_t idx, char *buf, uint32_t cap) {
    const struct RISkinPart *pt;
    uint32_t at;
    if (!s || !buf || idx >= s->nparts || !cap)
        return -1;
    pt = &s->parts[idx];
    if (pt->kind == RI_SKIN_KIND_ANY) {
        at = key_put(buf, cap, 0u, "BACKGROUND.");
        at = key_put(buf, cap, at, ri_ctlreg_section_token(pt->section));
    } else {
        at = key_put(buf, cap, 0u, "PART.");
        at = key_put(buf, cap, at, ri_ctlreg_section_token(pt->section));
        at = key_put(buf, cap, at, ".");
        at = key_put(buf, cap, at, ri_ctlreg_kind_token(pt->kind));
        at = key_put(buf, cap, at, ".");
        at = key_put(buf, cap, at, pt->part);
    }
    if (at >= cap) {
        buf[0] = '\0';
        return -1;
    }
    buf[at] = '\0';
    return 0;
}

int ri_skin_name_matches(const struct RISkin *s, const char *dirname) {
    uint32_t i;
    if (!s || !dirname || !s->name[0])
        return 0;
    for (i = 0u; s->name[i] || dirname[i]; i++) {
        char a = s->name[i], b = dirname[i];
        if (a >= 'A' && a <= 'Z')
            a = (char)(a - 'A' + 'a');
        if (b >= 'A' && b <= 'Z')
            b = (char)(b - 'A' + 'a');
        if (a != b)
            return 0;
    }
    return 1;
}
