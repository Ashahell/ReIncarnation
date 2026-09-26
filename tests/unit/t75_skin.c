/* t75_skin — skin/mod appearance core (§12.10 G8.1).
 * Oracle: docs/superpowers/specs/2026-09-26-skins-design.md (format 1:
 * FORMAT/NAME/VERSION header, named sections/kinds, role-named parts,
 * load-time size check against panelgeo; E0-1..E0-11).
 * Manifest/lookup/fallback keys, fail-closed parse codes, premultiplied box
 * downscale exact pixels, sha identity determinism/sensitivity, MODR field
 * helpers (codec signatures untouched), installed-list cycle.
 */
#include <stdio.h>
#include <string.h>
#include "tests/helpers/ri_assert.h"
#include "gui/skin.h"
#include "gui/ctlreg.h"
#include "gui/panelui.h"
#include "gui/keymap.h"
#include "gui/panelgeo.h"

#define HDR "FORMAT=1\nNAME=808-RI\nVERSION=3\n"
static const char MANIFEST_OK[] =
    "# 808-RI appearance half\n"
    HDR
    "BACKGROUND.808=bg808.ilbm\n"
    "BACKGROUND.909=bg909.ilbm\n"
    "PART.808.knob.knob.frame=knob808.strip,64\n"
    "BACKGROUND.rack-device-9=future.png\n"   /* unknown section: later build, ignored */
    "PART.mix-909.fader.fader.cap=cap.ilbm,1\n"
    "PART.808.slider.thing=f.png,1\n"          /* unknown kind: ignored */
    "UNKNOWNKEY=ignored\n"
    "PART.808.knob.knob.frame.dup=other.ilbm,64\n";

static int fake_read_ok(const char *name, unsigned char *buf, uint32_t cap) {
    if (!strcmp(name, "bg808.ilbm") || !strcmp(name, "bg909.ilbm")) {
        uint32_t n = 8u < cap ? 8u : cap;
        memset(buf, 0x41u, n);
        return (int)n;
    }
    if (!strcmp(name, "knob808.strip") || !strcmp(name, "cap.ilbm")) {
        uint32_t n = 4u < cap ? 4u : cap;
        memset(buf, 0x42u, n);
        return (int)n;
    }
    if (!strcmp(name, "other.ilbm")) {
        memset(buf, 0x43u, cap < 4u ? cap : 4u);
        return (int)(cap < 4u ? cap : 4u);
    }
    return -1;
}

static int fake_read_missing(const char *name, unsigned char *buf, uint32_t cap) {
    (void)buf; (void)cap;
    return !strcmp(name, "bg808.ilbm") ? 8 : -1;
}


/* Shipped skins must stay loadable: repo root from this file's path. */
static int root_path(const char *rel, char *out, size_t cap) {
    const char *f = __FILE__, *cut = strstr(f, "tests/unit/");
    size_t n = cut ? (size_t)(cut - f) : 0u;
    if (n + strlen(rel) + 1u > cap)
        return -1;
    memcpy(out, f, n);
    strcpy(out + n, rel);
    return 0;
}

static long slurp(const char *rel, char *buf, size_t cap) {
    char path[512];
    FILE *fp;
    size_t n;
    if (root_path(rel, path, sizeof path) != 0 || !(fp = fopen(path, "rb")))
        return -1;
    n = fread(buf, 1u, cap - 1u, fp);
    fclose(fp);
    buf[n] = '\0';
    return (long)n;
}

/* PNG IHDR width/height (bytes 16..23, big-endian). */
static int png_dims(const char *rel, uint32_t *w, uint32_t *h) {
    unsigned char b[32];
    long n = slurp(rel, (char *)b, sizeof b);
    if (n < 24 || memcmp(b + 12, "IHDR", 4))
        return -1;
    *w = ((uint32_t)b[16] << 24) | ((uint32_t)b[17] << 16) | ((uint32_t)b[18] << 8) | b[19];
    *h = ((uint32_t)b[20] << 24) | ((uint32_t)b[21] << 16) | ((uint32_t)b[22] << 8) | b[23];
    return 0;
}

int main(void) {
    struct RISkin s;
    int rc, i;

    /* --- parse: valid manifest --- */
    memset(&s, 0xA5, sizeof s);
    rc = ri_skin_parse(MANIFEST_OK, &s);
    RI_ASSERT(rc == 0, "parse ok rc=%d", rc);
    RI_ASSERT(s.nparts == 5u, "parse nparts=%u", s.nparts);
    RI_ASSERT(!strcmp(s.name, "808-RI") && s.version == 3u, "header name/version");
    RI_ASSERT(s.nstale == 0u && s.stale_idx == -1, "no stale parts yet");
    RI_ASSERT(s.parts[0].section == 2u && s.parts[0].kind == 0xFFu &&
              !strcmp(s.parts[0].part, "background") &&
              !strcmp(s.parts[0].file, "bg808.ilbm") &&
              s.parts[0].frames == 1u, "background fields");
    RI_ASSERT(s.parts[2].section == 2u && s.parts[2].kind == 0u &&
              !strcmp(s.parts[2].part, "knob.frame") &&
              !strcmp(s.parts[2].file, "knob808.strip") &&
              s.parts[2].frames == 64u, "part fields");
    RI_ASSERT(s.parts[2].rgba == 0 && s.parts[2].w == 0u, "unbound pixels");

    /* --- parse: fail-closed codes, struct zeroed --- */
    memset(&s, 0xA5, sizeof s);
    RI_ASSERT(ri_skin_parse(0, &s) == -1, "null text");
    RI_ASSERT(ri_skin_parse(MANIFEST_OK, 0) == -1, "null skin");
    memset(&s, 0xA5, sizeof s);
    RI_ASSERT(ri_skin_parse(HDR, &s) == 0, "header-only manifest valid (Template)");
    RI_ASSERT(s.nparts == 0u && s.version == 3u, "header-only zero parts");
    RI_ASSERT(ri_skin_parse("# only a comment\n" HDR "# more\n", &s) == 0, "comments around header");
    /* header: required, first, in order, format 1 only */
    memset(&s, 0xA5, sizeof s);
    RI_ASSERT(ri_skin_parse("", &s) == -7, "empty: no header");
    RI_ASSERT(s.nparts == 0u && s.name[0] == 0, "no header zeroed");
    RI_ASSERT(ri_skin_parse("# only a comment\n", &s) == -7, "comment-only: no header");
    RI_ASSERT(ri_skin_parse("BACKGROUND.808=a.png\n" HDR, &s) == -7, "header not first");
    RI_ASSERT(ri_skin_parse("NAME=x\nFORMAT=1\nVERSION=1\n", &s) == -7, "header order");
    RI_ASSERT(ri_skin_parse("VERSION=1\nNAME=x\nFORMAT=1\n", &s) == -7, "header keys are checked by name, not position");
    RI_ASSERT(ri_skin_parse("FORMAT=2\nNAME=x\nVERSION=1\n", &s) == -7, "future format refused");
    RI_ASSERT(ri_skin_parse("FORMAT=1\nNAME=x\n", &s) == -7, "missing VERSION");
    RI_ASSERT(ri_skin_parse("FORMAT=1\nNAME=x y\nVERSION=1\n", &s) == -5, "bad NAME chars");
    RI_ASSERT(ri_skin_parse("FORMAT=1\nNAME=x\nVERSION=0\n", &s) == -5, "VERSION 0");
    RI_ASSERT(ri_skin_parse("FORMAT=1\nNAME=x\nVERSION=65536\n", &s) == -5, "VERSION range");
    RI_ASSERT(ri_skin_parse(HDR "NAME=again\n", &s) == -6, "header key repeated");
    { /* NAME up to 63 chars (MODR field) */
        char nm[160];
        sprintf(nm, "FORMAT=1\nNAME=%s\nVERSION=1\n",
                "a123456789b123456789c123456789d123456789e123456789f123456789abc");
        RI_ASSERT(ri_skin_parse(nm, &s) == 0 && strlen(s.name) == 63u, "NAME 63");
        sprintf(nm, "FORMAT=1\nNAME=%s\nVERSION=1\n",
                "a123456789b123456789c123456789d123456789e123456789f123456789abcd");
        RI_ASSERT(ri_skin_parse(nm, &s) == -5, "NAME 64 refused");
    }
    /* charset: printable ASCII + TAB/CR/LF only */
    memset(&s, 0xA5, sizeof s);
    RI_ASSERT(ri_skin_parse(HDR "# caf\xc3\xa9\n", &s) == -8, "non-ASCII byte");
    RI_ASSERT(s.nparts == 0u, "non-ASCII zeroed");
    RI_ASSERT(ri_skin_parse(HDR "\x01\n", &s) == -8, "control byte");
    RI_ASSERT(ri_skin_parse(HDR "#\ttab ok\r\n", &s) == 0, "tab + CRLF ok");
    memset(&s, 0xA5, sizeof s);
    RI_ASSERT(ri_skin_parse(HDR "PART.808.knob.knob.frame\n", &s) == -3, "malformed line");
    RI_ASSERT(s.nparts == 0u, "malformed zeroed");
    memset(&s, 0xA5, sizeof s);
    RI_ASSERT(ri_skin_parse(HDR "PART.808.knob\n", &s) == -3, "truncated part key");
    RI_ASSERT(ri_skin_parse(HDR "PART.808.knob=f.png,1\n", &s) == -3, "part key without part");
    memset(&s, 0xA5, sizeof s);
    RI_ASSERT(ri_skin_parse(HDR "PART.808.knob.knob.frame=k.ilbm,64\nPART.808.knob.knob.frame=j.ilbm,64\n", &s) == -6, "dup key");
    RI_ASSERT(s.nparts == 0u, "dup zeroed");
    memset(&s, 0xA5, sizeof s);
    RI_ASSERT(ri_skin_parse(HDR "PART.2.0.k=f.ilbm,1\n", &s) == 0 && s.nparts == 0u, "numeric ids are not tokens: ignored");
    RI_ASSERT(ri_skin_parse(HDR "PART.808.knob.k=f.ilbm,0\n", &s) == -5, "frames 0");
    RI_ASSERT(ri_skin_parse(HDR "PART.808.knob.k=f.ilbm,257\n", &s) == -5, "frames 257");
    RI_ASSERT(ri_skin_parse(HDR "PART.future.knob.k=f.ilbm,0\n", &s) == -5, "ignored lines still checked for syntax");
    { /* line too long */
        char big[600];
        memset(big, 'A', sizeof big - 2);
        big[sizeof big - 2] = '\n'; big[sizeof big - 1] = '\0';
        memset(&s, 0xA5, sizeof s);
        RI_ASSERT(ri_skin_parse(big, &s) == -2, "long line");
        RI_ASSERT(s.nparts == 0u, "long zeroed");
    }
    { /* table full: 65 distinct parts */
        static char many[64 + 65 * 32];
        char *p = many;
        p += sprintf(p, "%s", HDR);
        for (i = 0; i < 65; i++)
            p += sprintf(p, "PART.808.knob.k%d=f%d.ilbm,1\n", i, i);
        memset(&s, 0xA5, sizeof s);
        RI_ASSERT(ri_skin_parse(many, &s) == -4, "table full");
        RI_ASSERT(s.nparts == 0u, "full zeroed");
    }

    /* --- find: present vs fallback --- */
    memset(&s, 0, sizeof s);
    RI_ASSERT(ri_skin_parse(MANIFEST_OK, &s) == 0, "reparse");
    RI_ASSERT(ri_skin_find(&s, 2u, 0u, "knob.frame") == 2, "find part");
    RI_ASSERT(ri_skin_find(&s, 2u, 0xFFu, "background") == 0, "find background");
    RI_ASSERT(ri_skin_find(&s, 2u, 0u, "background") == 0, "background matches any kind");
    RI_ASSERT(ri_skin_find(&s, 3u, 5u, "knob.frame") == -1, "fallback -1");
    RI_ASSERT(ri_skin_find(&s, 2u, 0u, 0) == -1, "null part");
    RI_ASSERT(ri_skin_find(0, 2u, 0u, "knob.frame") == -1, "null skin");

    /* --- expected sizes come from the geometry (2x-master px == Q) --- */
    { uint32_t w = 0u, h = 0u;
      RI_ASSERT(ri_skin_expect(2u, 0xFFu, "background", 1u, &w, &h) == 1 && w == 1472u && h == 468u,
                "808 backdrop = p. 148 figure %ux%u", w, h);
      RI_ASSERT(ri_skin_expect(8u, 0xFFu, "background", 1u, &w, &h) == 1 && w == 332u && h == 464u,
                "master backdrop = strip height %ux%u", w, h);
      RI_ASSERT(ri_skin_expect(2u, 0u, "knob.frame", 64u, &w, &h) == 1 && w == 40u && h == 56u * 64u,
                "808 large knob strip %ux%u", w, h);
      RI_ASSERT(ri_skin_expect(2u, 0u, "knob.frame.small", 64u, &w, &h) == 1 && w == 36u && h == 44u * 64u,
                "808 small knob strip %ux%u", w, h);
      RI_ASSERT(ri_skin_expect(0u, 0u, "knob.frame", 1u, &w, &h) == 1 && w == 84u && h == 120u, "303 knob");
      RI_ASSERT(ri_skin_expect(1u, 0xFFu, "background", 1u, &w, &h) == 1 && w == 1464u, "synth 2 = synth 1 art");
      RI_ASSERT(ri_skin_expect(0u, 0u, "knob.frame.small", 64u, &w, &h) == 0, "303 has one knob size");
      RI_ASSERT(ri_skin_expect(2u, 1u, "fader.cap", 1u, &w, &h) == 0, "unchecked part");
      RI_ASSERT(ri_skin_expect(99u, 0xFFu, "background", 1u, &w, &h) == 0, "bad section");
      RI_ASSERT(ri_skin_expect(2u, 0u, "knob.frame", 0u, &w, &h) == 0, "zero frames");
    }
    /* --- knob roles replace size-suffixed names --- */
    RI_ASSERT(ri_skin_knob_role(2u, 40u, 56u) && !strcmp(ri_skin_knob_role(2u, 40u, 56u), "knob.frame"), "808 large role");
    RI_ASSERT(ri_skin_knob_role(2u, 36u, 44u) && !strcmp(ri_skin_knob_role(2u, 36u, 44u), "knob.frame.small"), "808 small role");
    RI_ASSERT(ri_skin_knob_role(1u, 84u, 120u) && !strcmp(ri_skin_knob_role(1u, 84u, 120u), "knob.frame"), "synth 2 role");
    RI_ASSERT(ri_skin_knob_role(2u, 41u, 56u) == 0, "no knob of that size");
    for (i = 0; i < (int)RI_SEC_COUNT; i++) { /* at most two knob sizes anywhere */
        uint32_t w, h;
        const char *r;
        int k, nsz = 0;
        const struct RIGeoSection *g = ri_geo_section((uint32_t)i);
        uint32_t sw[8], sh[8];
        if (!g)
            continue;
        for (k = 0; k < (int)g->nitems; k++) {
            const struct RICtlDef *d = ri_ctlreg_find(g->items[k].reg_id);
            int j, seen = 0;
            if (g->items[k].shape != RI_GEO_KNOB || !d || d->kind != RI_CK_KNOB)
                continue;
            w = (uint32_t)ri_geo_px((int)g->items[k].w, 2);
            h = (uint32_t)ri_geo_px((int)g->items[k].h, 2);
            r = ri_skin_knob_role((uint8_t)i, w, h);
            RI_ASSERT(r != 0, "sec %d knob %ux%u has a role", i, w, h);
            for (j = 0; j < nsz; j++)
                seen |= sw[j] == w && sh[j] == h;
            if (!seen && nsz < 8) {
                sw[nsz] = w; sh[nsz] = h; nsz++;
            }
        }
        RI_ASSERT(nsz <= 2, "sec %d has %d knob sizes (roles cover two)", i, nsz);
    }

    /* --- bind refuses a part whose size no longer matches the geometry --- */
    { static uint32_t big[40 * 56 * 2];
      memset(&s, 0, sizeof s);
      RI_ASSERT(ri_skin_parse(HDR "BACKGROUND.master=bg.png\nPART.808.knob.knob.frame=k.png,2\n", &s) == 0, "stale parse");
      RI_ASSERT(ri_skin_bind(&s, 0u, big, 332u, 392u) == -2, "old 392-high master refused");
      RI_ASSERT(s.parts[0].rgba == 0 && s.nstale == 1u && s.stale_idx == 0, "stale recorded, part unbound");
      RI_ASSERT(ri_skin_bind(&s, 1u, big, 40u, 56u * 2u) == 0, "matching strip binds");
      RI_ASSERT(ri_skin_bind(&s, 1u, big, 40u, 56u) == -2 && s.nstale == 2u && s.stale_idx == 0, "wrong frame count refused, first kept");
      { char key[96];
        RI_ASSERT(ri_skin_key(&s, 0u, key, sizeof key) == 0 && !strcmp(key, "BACKGROUND.master"), "key bg '%s'", key);
        RI_ASSERT(ri_skin_key(&s, 1u, key, sizeof key) == 0 && !strcmp(key, "PART.808.knob.knob.frame"), "key part '%s'", key);
        RI_ASSERT(ri_skin_key(&s, 2u, key, sizeof key) == -1 && ri_skin_key(&s, 0u, key, 8u) == -1, "key bad idx/cap");
      }
    }
    /* --- NAME must match the mod directory (case-insensitive, AROS FS) --- */
    memset(&s, 0, sizeof s);
    RI_ASSERT(ri_skin_parse(MANIFEST_OK, &s) == 0, "name reparse");
    RI_ASSERT(ri_skin_name_matches(&s, "808-RI") == 1 && ri_skin_name_matches(&s, "808-ri") == 1, "name match");
    RI_ASSERT(ri_skin_name_matches(&s, "808-RI2") == 0 && ri_skin_name_matches(&s, "") == 0 &&
              ri_skin_name_matches(&s, 0) == 0 && ri_skin_name_matches(0, "808-RI") == 0, "name mismatch");

    /* --- bind --- */
    memset(&s, 0, sizeof s);
    RI_ASSERT(ri_skin_parse(MANIFEST_OK, &s) == 0, "bind reparse");
    { static uint32_t px[4] = { 1u, 2u, 3u, 4u };
        RI_ASSERT(ri_skin_bind(&s, 3u, px, 2u, 2u) == 0, "bind ok (unchecked part)");
        RI_ASSERT(s.parts[3].w == 2u && s.parts[3].h == 2u &&
                  s.parts[3].rgba == px, "bind stored");
        RI_ASSERT(ri_skin_bind(&s, 64u, px, 2u, 2u) == -1, "bind idx");
        RI_ASSERT(ri_skin_bind(&s, 2u, 0, 2u, 2u) == -1, "bind null px");
        RI_ASSERT(ri_skin_bind(&s, 2u, px, 0u, 2u) == -1, "bind zero dim");
        RI_ASSERT(ri_skin_bind(0, 2u, px, 2u, 2u) == -1, "bind null skin");
        RI_ASSERT(ri_skin_bind_zoom(&s, 2u, px, 2u, 2u) == 0, "bind zoom ok");
        RI_ASSERT(s.parts[2].zw == 2u && s.parts[2].zrgba == px, "bind zoom stored");
        RI_ASSERT(ri_skin_bind_zoom(&s, 64u, px, 2u, 2u) == -1, "bind zoom idx");
        RI_ASSERT(ri_skin_bind_zoom(&s, 2u, px, 0u, 2u) == -1, "bind zoom dim");
    }

    /* --- downscale: opaque 4x4 quadrants -> exact 2x2 --- */
    { static const uint32_t quad[16] = {
        0xFFFF0000u, 0xFFFF0000u, 0xFF00FF00u, 0xFF00FF00u,
        0xFFFF0000u, 0xFFFF0000u, 0xFF00FF00u, 0xFF00FF00u,
        0xFF0000FFu, 0xFF0000FFu, 0xFFFFFFFFu, 0xFFFFFFFFu,
        0xFF0000FFu, 0xFF0000FFu, 0xFFFFFFFFu, 0xFFFFFFFFu };
      uint32_t out[4] = { 0 };
      ri_skin_downscale(quad, 4u, 4u, out, 2u, 2u);
      RI_ASSERT(out[0] == 0xFFFF0000u && out[1] == 0xFF00FF00u &&
                out[2] == 0xFF0000FFu && out[3] == 0xFFFFFFFFu, "quad exact");
    }
    /* --- downscale: premultiplied alpha, exact --- */
    { static const uint32_t am[4] = {
        0x80FF0000u, 0x00000000u, 0x00000000u, 0x00000000u };
      uint32_t out[1] = { 0 };
      ri_skin_downscale(am, 2u, 2u, out, 1u, 1u);
      RI_ASSERT(out[0] == 0x20FF0000u, "alpha exact %08X", out[0]);
    }
    /* --- downscale: mid-grey pins the premultiplied path (no clamp hide) --- */
    { static const uint32_t gr[4] = {
        0xFF808080u, 0xFF808080u, 0xFF808080u, 0xFF808080u };
      uint32_t out[1] = { 0 };
      ri_skin_downscale(gr, 2u, 2u, out, 1u, 1u);
      RI_ASSERT(out[0] == 0xFF808080u, "grey exact %08X", out[0]);
    }
    /* --- downscale: equal size copies; illegal/degenerate is a no-op --- */
    { static const uint32_t cp[4] = { 0xFF000005u, 0xFF000006u, 0xFF000007u, 0xFF000008u };
      uint32_t out[4] = { 0 };
      ri_skin_downscale(cp, 2u, 2u, out, 2u, 2u);
      RI_ASSERT(out[0] == 0xFF000005u && out[3] == 0xFF000008u, "copy");
      out[0] = 0xDEADu;
      ri_skin_downscale(cp, 1u, 1u, out, 2u, 2u);
      RI_ASSERT(out[0] == 0xDEADu, "upscale noop");
      ri_skin_downscale(cp, 2u, 2u, out, 0u, 2u);
      RI_ASSERT(out[0] == 0xDEADu, "zero noop");
      ri_skin_downscale(0, 2u, 2u, out, 1u, 1u);
      RI_ASSERT(out[0] == 0xDEADu, "null noop");
    }

    /* --- identity: format, determinism, sensitivity, failures --- */
    { unsigned char scratch[8192];
      char hex1[65], hex2[65], hex3[65];
      int k;
      memset(&s, 0, sizeof s);
      RI_ASSERT(ri_skin_parse(MANIFEST_OK, &s) == 0, "id reparse");
      RI_ASSERT(ri_skin_identity(&s, MANIFEST_OK, fake_read_ok,
                                 scratch, sizeof scratch, hex1) == 0, "id ok");
      for (k = 0; k < 64; k++)
          RI_ASSERT((hex1[k] >= '0' && hex1[k] <= '9') ||
                    (hex1[k] >= 'a' && hex1[k] <= 'f'), "id hex");
      RI_ASSERT(hex1[64] == '\0', "id nul");
      RI_ASSERT(ri_skin_identity(&s, MANIFEST_OK, fake_read_ok,
                                 scratch, sizeof scratch, hex2) == 0, "id again");
      RI_ASSERT(!memcmp(hex1, hex2, 65), "id deterministic");
      { char other[sizeof MANIFEST_OK];
        memcpy(other, MANIFEST_OK, sizeof MANIFEST_OK);
        other[sizeof MANIFEST_OK - 3] = '5'; /* 64 -> 65 frames */
        RI_ASSERT(ri_skin_identity(&s, other, fake_read_ok,
                                   scratch, sizeof scratch, hex3) == 0, "id other");
        RI_ASSERT(memcmp(hex1, hex3, 65) != 0, "id sensitive");
      }
      RI_ASSERT(ri_skin_identity(&s, MANIFEST_OK, fake_read_missing,
                                 scratch, sizeof scratch, hex2) == -3, "id missing");
      RI_ASSERT(ri_skin_identity(&s, MANIFEST_OK, fake_read_ok,
                                 scratch, 4095u, hex2) == -2, "id scratch");
      RI_ASSERT(ri_skin_identity(0, MANIFEST_OK, fake_read_ok,
                                 scratch, sizeof scratch, hex2) == -1, "id null");
    }

    /* --- MODR helpers --- */
    { struct RISong song;
      char name[64], sha[65];
      uint16_t vers = 0u;
      memset(&song, 0, sizeof song);
      RI_ASSERT(ri_skin_modref_set(&song, "808-RI",
          "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef",
          3u) == 0, "modref set");
      RI_ASSERT(song.nmods == 1u, "modref nmods");
      RI_ASSERT(ri_skin_modref_get(&song, 0u, name, sha, &vers) == 0, "modref get");
      RI_ASSERT(!strcmp(name, "808-RI") && vers == 3u &&
                strlen(sha) == 64u, "modref fields");
      RI_ASSERT(ri_skin_modref_set(&song, "808-RI",
          "ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff",
          4u) == 0, "modref update");
      RI_ASSERT(song.nmods == 1u && song.mods[0].vers == 4u, "modref in place");
      RI_ASSERT(ri_skin_modref_set(&song, "", "ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff", 1u) == -1, "modref empty");
      RI_ASSERT(ri_skin_modref_set(&song, "x", "short", 1u) == -1, "modref sha len");
      RI_ASSERT(ri_skin_modref_set(&song, "x", "gggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggg", 1u) == -1, "modref sha hex");
      for (i = 1; i < 16; i++) {
          char nm[8];
          sprintf(nm, "m%d", i);
          RI_ASSERT(ri_skin_modref_set(&song, nm,
              "0000000000000000000000000000000000000000000000000000000000000000", 1u) == 0, "modref fill");
      }
      RI_ASSERT(song.nmods == 16u, "modref full");
      RI_ASSERT(ri_skin_modref_set(&song, "one-more",
          "0000000000000000000000000000000000000000000000000000000000000000", 1u) == -2, "modref overflow");
      RI_ASSERT(ri_skin_modref_get(&song, 16u, name, sha, &vers) == -1, "modref oob");
      RI_ASSERT(ri_skin_modref_get(0, 0u, name, sha, &vers) == -1, "modref null");
    }

    /* --- cycle --- */
    { static const char *inst[3] = { "Classic", "808-RI", "Template" };
      char next[64];
      RI_ASSERT(ri_skin_cycle("808-RI", inst, 3u, next) == 0 &&
                !strcmp(next, "Template"), "cycle next");
      RI_ASSERT(ri_skin_cycle("Template", inst, 3u, next) == 0 &&
                !strcmp(next, "Classic"), "cycle wrap");
      RI_ASSERT(ri_skin_cycle("Gone", inst, 3u, next) == 0 &&
                !strcmp(next, "Classic"), "cycle unknown");
      RI_ASSERT(ri_skin_cycle("Classic", inst, 0u, next) == -1, "cycle empty");
      RI_ASSERT(ri_skin_cycle("Classic", 0, 3u, next) == -1, "cycle null");
    }

    /* --- Ctrl+M cycles the installed skin list (RI_KM_SELECT_MOD) --- */
    { struct RIPanelUI pu;
      static const char *inst[3] = { "Classic", "808-RI", "Template" };
      ri_panel_init(&pu);
      ri_panel_skins(&pu, inst, 3u, "Classic");
      RI_ASSERT(ri_panel_key(&pu, 0x37, RI_QUAL_CONTROL) == 1 &&
                !strcmp(pu.skin_current, "808-RI"), "ctrl-m advances skin");
      RI_ASSERT(ri_panel_key(&pu, 0x37, RI_QUAL_CONTROL) == 1 &&
                !strcmp(pu.skin_current, "Template"), "ctrl-m again");
      ri_panel_skins(&pu, 0, 0u, "Classic");
      RI_ASSERT(ri_panel_key(&pu, 0x37, RI_QUAL_CONTROL) == 0, "ctrl-m no list");
    }

    /* --- value -> strip frame --- */
    RI_ASSERT(ri_skin_frame(0u, 127u, 64u) == 0u, "frame lo");
    RI_ASSERT(ri_skin_frame(127u, 127u, 64u) == 63u, "frame hi");
    RI_ASSERT(ri_skin_frame(64u, 127u, 64u) == 32u, "frame mid");
    RI_ASSERT(ri_skin_frame(999u, 127u, 64u) == 63u, "frame clamp");
    RI_ASSERT(ri_skin_frame(5u, 127u, 0u) == 0u, "frame noframes");

    /* --- unbind forgets every pixel pointer (review fix: no dangling) --- */
    { static uint32_t px[4] = { 1u, 2u, 3u, 4u };
      memset(&s, 0, sizeof s);
      RI_ASSERT(ri_skin_parse(MANIFEST_OK, &s) == 0, "unbind reparse");
      RI_ASSERT(ri_skin_bind(&s, 3u, px, 2u, 2u) == 0, "unbind bind");
      RI_ASSERT(ri_skin_bind_zoom(&s, 3u, px, 2u, 2u) == 0, "unbind bindz");
      ri_skin_unbind(&s);
      RI_ASSERT(s.parts[3].rgba == 0 && s.parts[3].zrgba == 0 &&
                s.parts[3].w == 0u && s.parts[3].zw == 0u, "unbind cleared");
      RI_ASSERT(s.nparts == 5u, "unbind keeps manifest");
      ri_skin_unbind(0);
      RI_ASSERT(1, "unbind null safe");
    }

    /* --- ARGB -> blit order (R-G8.1-5: ARGB paints ghost-blue) --- */
    { static const uint32_t argb[4] = { 0xFF171410u, 0xFFE8A33Du, 0x00000000u, 0x80FF0000u };
      uint32_t out[4] = { 0 };
      static const uint32_t want[4] = { 0x101417FFu, 0x3DA3E8FFu, 0x00000000u, 0x0000FF80u };
      int k;
      ri_skin_swizzle_blit(argb, out, 4u);
      for (k = 0; k < 4; k++)
          RI_ASSERT(out[k] == want[k], "swizzle %d: %08X", k, out[k]);
      out[0] = out[1] = out[2] = out[3] = 0u;   /* in-place safe */
      ri_skin_swizzle_blit(out, out, 0u);
      RI_ASSERT(out[0] == 0u, "swizzle n0");
    }

    /* --- shipped skins: parse as format 1, NAME == dir, every image the
     *     size the panel expects (stale art fails here, not on a user) --- */
    { static char text[16384];
      static uint32_t dummy[1];
      char rel[256];
      uint32_t k, w = 0u, h = 0u;
      RI_ASSERT(slurp("skins/808-RI/Skin.manifest", text, sizeof text) > 0, "read 808-RI manifest");
      memset(&s, 0, sizeof s);
      RI_ASSERT(ri_skin_parse(text, &s) == 0, "808-RI parses as format 1");
      RI_ASSERT(ri_skin_name_matches(&s, "808-RI") && s.version >= 2u, "808-RI header");
      RI_ASSERT(s.nparts >= 17u, "808-RI parts %u", s.nparts);
      for (k = 0u; k < s.nparts; k++) {
          sprintf(rel, "skins/808-RI/%s", s.parts[k].file);
          RI_ASSERT(png_dims(rel, &w, &h) == 0, "png %s", rel);
          RI_ASSERT(ri_skin_bind(&s, k, dummy, (uint16_t)w, (uint16_t)h) == 0,
                    "808-RI %s is %ux%u: wrong size for the panel", s.parts[k].file, w, h);
      }
      RI_ASSERT(s.nstale == 0u, "808-RI no stale parts");
      RI_ASSERT(slurp("skins/Template/Skin.manifest", text, sizeof text) > 0, "read Template");
      memset(&s, 0, sizeof s);
      RI_ASSERT(ri_skin_parse(text, &s) == 0 && s.nparts == 0u &&
                ri_skin_name_matches(&s, "Template"), "Template header-only");
    }

    RI_RESULT("skin");
}
