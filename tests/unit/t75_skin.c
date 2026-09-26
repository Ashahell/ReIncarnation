/* t75_skin — skin/mod appearance core (§12.10 G8.1).
 * Oracle: docs/superpowers/specs/2026-09-26-skins-design.md (E0-1..E0-8).
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

static const char MANIFEST_OK[] =
    "# 808-RI appearance half\n"
    "BACKGROUND.2=bg808.ilbm\n"
    "BACKGROUND.3=bg909.ilbm\n"
    "PART.2.0.knob.frame=knob808.strip,64\n"
    "PART.7.1.fader.cap=cap.ilbm,1\n"
    "UNKNOWNKEY=ignored\n"
    "PART.2.0.knob.frame.dup=other.ilbm,64\n";

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

int main(void) {
    struct RISkin s;
    int rc, i;

    /* --- parse: valid manifest --- */
    memset(&s, 0xA5, sizeof s);
    rc = ri_skin_parse(MANIFEST_OK, &s);
    RI_ASSERT(rc == 0, "parse ok rc=%d", rc);
    RI_ASSERT(s.nparts == 5u, "parse nparts=%u", s.nparts);
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
    RI_ASSERT(ri_skin_parse("", &s) == 0, "empty manifest valid");
    RI_ASSERT(s.nparts == 0u, "empty zero parts");
    RI_ASSERT(ri_skin_parse("# only a comment\n", &s) == 0, "comment-only valid");
    memset(&s, 0xA5, sizeof s);
    RI_ASSERT(ri_skin_parse("PART.2.0.knob.frame\n", &s) == -3, "malformed line");
    RI_ASSERT(s.nparts == 0u, "malformed zeroed");
    memset(&s, 0xA5, sizeof s);
    RI_ASSERT(ri_skin_parse("PART.2.0\n", &s) == -3, "truncated part key");
    memset(&s, 0xA5, sizeof s);
    RI_ASSERT(ri_skin_parse("PART.2.0.knob.frame=k.ilbm,64\nPART.2.0.knob.frame=j.ilbm,64\n", &s) == -6, "dup key");
    RI_ASSERT(s.nparts == 0u, "dup zeroed");
    memset(&s, 0xA5, sizeof s);
    RI_ASSERT(ri_skin_parse("PART.18.0.k=f.ilbm,1\n", &s) == -5, "bad section");
    RI_ASSERT(ri_skin_parse("PART.2.9.k=f.ilbm,1\n", &s) == -5, "bad kind");
    RI_ASSERT(ri_skin_parse("PART.2.0.k=f.ilbm,0\n", &s) == -5, "frames 0");
    RI_ASSERT(ri_skin_parse("PART.2.0.k=f.ilbm,257\n", &s) == -5, "frames 257");
    { /* line too long */
        char big[600];
        memset(big, 'A', sizeof big - 2);
        big[sizeof big - 2] = '\n'; big[sizeof big - 1] = '\0';
        memset(&s, 0xA5, sizeof s);
        RI_ASSERT(ri_skin_parse(big, &s) == -2, "long line");
        RI_ASSERT(s.nparts == 0u, "long zeroed");
    }
    { /* table full: 65 distinct parts */
        static char many[65 * 32];
        char *p = many;
        for (i = 0; i < 65; i++)
            p += sprintf(p, "PART.2.0.k%d=f%d.ilbm,1\n", i, i);
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

    /* --- bind --- */
    { static uint32_t px[4] = { 1u, 2u, 3u, 4u };
        RI_ASSERT(ri_skin_bind(&s, 2u, px, 2u, 2u) == 0, "bind ok");
        RI_ASSERT(s.parts[2].w == 2u && s.parts[2].h == 2u &&
                  s.parts[2].rgba == px, "bind stored");
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

    /* --- size-suffixed part names (E0-9) --- */
    { char out[64];
      RI_ASSERT(ri_skin_part_sized("knob.frame", 80u, 112u, out, sizeof out) == 0 &&
                !strcmp(out, "knob.frame.80x112"), "sized ok");
      RI_ASSERT(ri_skin_part_sized("knob.frame", 0u, 112u, out, sizeof out) == -1, "sized zero");
      RI_ASSERT(ri_skin_part_sized(0, 80u, 112u, out, sizeof out) == -1, "sized null");
      RI_ASSERT(ri_skin_part_sized("knob.frame", 80u, 112u, out, 10u) == -1, "sized small");
    }

    /* --- unbind forgets every pixel pointer (review fix: no dangling) --- */
    { static uint32_t px[4] = { 1u, 2u, 3u, 4u };
      memset(&s, 0, sizeof s);
      RI_ASSERT(ri_skin_parse(MANIFEST_OK, &s) == 0, "unbind reparse");
      RI_ASSERT(ri_skin_bind(&s, 2u, px, 2u, 2u) == 0, "unbind bind");
      RI_ASSERT(ri_skin_bind_zoom(&s, 2u, px, 2u, 2u) == 0, "unbind bindz");
      ri_skin_unbind(&s);
      RI_ASSERT(s.parts[2].rgba == 0 && s.parts[2].zrgba == 0 &&
                s.parts[2].w == 0u && s.parts[2].zw == 0u, "unbind cleared");
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

    RI_RESULT("skin");
}
