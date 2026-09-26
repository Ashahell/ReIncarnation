/* gui/skin.h — skin/mod appearance core (§12.10 G8.1).
 * Pure C, stdint only, no allocation, no IO, no libm. Pixel storage is
 * caller-owned: the manifest maps keys to file names, the loader decodes
 * each file (datatypes on AROS) and binds RGBA32 pixels per part; anything
 * unbound renders through the procedural Classic path (per-part fallback).
 * Geometry never lives here — lookup keys restyle, panelgeo positions.
 * On-disk form (format 1, owner-approved 2026-09-26):
 * docs/superpowers/specs/2026-09-26-skins-design.md.
 *
 * Manifest (line-based text, printable ASCII + TAB/CR/LF, lines <= 255 B):
 *   FORMAT=1            first three key lines, in this order, required
 *   NAME=<name>         [A-Za-z0-9._-]{1,63}, equals the mod directory name
 *   VERSION=<1..65535>  stored as the song MODR vers
 *   BACKGROUND.<section>=file
 *   PART.<section>.<kind>.<part>=file,NFRAMES
 * section/kind = ctlreg tokens ("808", "mix-909", "knob", ...); a token this
 * build does not know is IGNORED (a later build's device), after the value
 * is syntax-checked. part = [A-Za-z0-9._-]{1,48}, named by role (e.g.
 * "knob.frame", "knob.frame.small"), never by pixel size. '#' starts a
 * comment line; unknown keys are ignored; any malformed line fails the whole
 * load closed (struct zeroed).
 */
#ifndef RI_SKIN_H
#define RI_SKIN_H
#include <stdint.h>
#include "project/rbng.h"

#define RI_SKIN_MAX_PARTS 64u
#define RI_SKIN_PART_NAME 48u
#define RI_SKIN_FILE_NAME 128u
#define RI_SKIN_MAX_FRAMES 256u
#define RI_SKIN_FORMAT 1u
#define RI_SKIN_NAME 63u

struct RISkinPart {
    uint8_t section;   /* RI_SEC_* */
    uint8_t kind;      /* RI_CK_* */
    char part[RI_SKIN_PART_NAME + 1u];
    char file[RI_SKIN_FILE_NAME + 1u];
    uint16_t frames;   /* 1 for backdrops */
    uint16_t w, h;     /* bound pixels, 0 until ri_skin_bind */
    const uint32_t *rgba; /* caller-owned 2x masters, NULL = Classic fallback */
    uint16_t zw, zh;   /* zoom-scaled copy, 0 until ri_skin_bind_zoom */
    const uint32_t *zrgba;
};

struct RISkin {
    char name[RI_SKIN_NAME + 1u]; /* manifest NAME (== mod directory name) */
    uint16_t version;             /* manifest VERSION */
    uint16_t nparts;
    uint16_t nstale;              /* parts refused at bind: wrong size for the geometry */
    int16_t stale_idx;            /* first refused part, -1 none */
    struct RISkinPart parts[RI_SKIN_MAX_PARTS];
};

/* Manifest parse: 0 ok; <0 fail-closed (struct zeroed):
 * -1 bad arg; -2 line too long; -3 malformed line; -4 part-table full;
 * -5 bad number/range/name; -6 duplicate key; -7 header missing, out of
 * order or unsupported FORMAT; -8 byte outside printable ASCII/TAB/CR/LF.
 * A header-only manifest is a valid empty skin (Template): 0 parts. */
int ri_skin_parse(const char *text, struct RISkin *s);
/* Part lookup: index, or -1 when absent (caller renders Classic). */
int ri_skin_find(const struct RISkin *s, uint8_t section, uint8_t kind,
                 const char *part);
/* Bind decoded 2x-master pixels to part idx: 0 ok, -1 bad arg/idx/dims,
 * -2 size differs from the geometry's expectation (ri_skin_expect): the
 * part stays unbound (Classic fallback) and is counted in nstale/stale_idx
 * so the UI can say so — never a cropped or stretched blit. */
int ri_skin_bind(struct RISkin *s, uint32_t idx, const uint32_t *rgba,
                 uint16_t w, uint16_t h);
/* Bind a zoom-scaled copy (built once per zoom change with ri_skin_downscale,
 * never per frame): 0 ok, -1 bad arg/idx/dims. */
int ri_skin_bind_zoom(struct RISkin *s, uint32_t idx, const uint32_t *rgba,
                      uint16_t w, uint16_t h);
/* Clear every pixel binding (masters + zoom copies stay caller-owned: this
 * only forgets the pointers, never frees). The loader calls it when it
 * releases a skin's buffers so no struct is left dangling. */
void ri_skin_unbind(struct RISkin *s);
/* Box downscale, premultiplied alpha, no libm. dw/dh must be <= sw/sh and
 * nonzero; anything else is a no-op (downscale-only by design E0-3). */
void ri_skin_downscale(const uint32_t *src, uint32_t sw, uint32_t sh,
                       uint32_t *dst, uint32_t dw, uint32_t dh);
/* Content identity: SHA-256 over manifest bytes then each referenced part
 * file's bytes in manifest order. read_file(name, buf, cap) returns bytes
 * read or <0 on missing/unreadable (missing file fails closed, -3).
 * scratch = caller buffer, >= 4096 B. hex[65] lowercase on 0; codes:
 * -1 bad arg, -2 scratch too small, -3 unreadable part file. */
int ri_skin_identity(const struct RISkin *s, const char *manifest,
                     int (*read_file)(const char *name, unsigned char *buf,
                                      uint32_t cap),
                     unsigned char *scratch, uint32_t scratch_cap,
                     char hex[65]);
/* MODR field helpers (never touch the rbng codec signatures):
 * get: copy entry idx out. set: update in place by name, else append.
 * get -1 bad arg/idx; set -1 bad arg/content, -2 table full. */
int ri_skin_modref_get(const struct RISong *song, uint32_t idx,
                       char name[64], char sha[65], uint16_t *vers);
int ri_skin_modref_set(struct RISong *song, const char *name,
                       const char *sha, uint16_t vers);
/* Installed-list cycle for Ctrl+M (RI_KM_SELECT_MOD): copies the entry after
 * current (wrapping) into next; unknown current starts at entry 0.
 * 0 ok, -1 bad arg/empty list. */
int ri_skin_cycle(const char *current, const char *const *installed,
                  uint32_t n, char next[64]);
/* Value (0..vmax) -> strip frame (0..frames-1), clamped; 0 on bad args. */
uint32_t ri_skin_frame(uint32_t v, uint32_t vmax, uint32_t frames);
/* Expected 2x-master size of a part from panelgeo (px == Q): backdrop =
 * section size; "knob.frame" = the section's largest knob, "knob.frame.small"
 * = its other knob size; strips are w x (h * frames). SYNTH2 uses SYNTH1.
 * 1 = expectation set, 0 = none (part unchecked / no such control). */
int ri_skin_expect(uint8_t section, uint8_t kind, const char *part,
                   uint32_t frames, uint32_t *w, uint32_t *h);
/* Role name of a knob drawn at w x h (2x-master px) in a section:
 * "knob.frame", "knob.frame.small", or NULL when no knob has that size. */
const char *ri_skin_knob_role(uint8_t section, uint32_t w, uint32_t h);
/* Manifest key of part idx ("BACKGROUND.master", "PART.808.knob.knob.frame")
 * for messages: 0 ok, -1 bad arg/idx/capacity. */
int ri_skin_key(const struct RISkin *s, uint32_t idx, char *buf, uint32_t cap);
/* 1 when NAME equals the mod directory name (ASCII case-insensitive, as
 * AROS file systems compare), else 0. */
int ri_skin_name_matches(const struct RISkin *s, const char *dirname);
/* ARGB words (core format) -> blit-order words for WritePixelArrayAlpha
 * ((b<<24)|(g<<16)|(r<<8)|a — the proven knob_blit packing; ARGB paints
 * ghost-blue). Pure, in-place-safe (dst may equal src). */
void ri_skin_swizzle_blit(const uint32_t *src, uint32_t *dst, uint32_t n);
/* Zoom factor numerator over 8 for zoom index 0..3 (mirrors the panelgeo
 * 4/6/8/3 series; 0 for anything else). The loader scales masters by it. */
uint32_t ri_skin_zoom_num(int zoom);
#endif
