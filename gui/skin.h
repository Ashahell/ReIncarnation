/* gui/skin.h — skin/mod appearance core (§12.10 G8.1).
 * Pure C, stdint only, no allocation, no IO, no libm. Pixel storage is
 * caller-owned: the manifest maps keys to file names, the loader decodes
 * each file (datatypes on AROS) and binds RGBA32 pixels per part; anything
 * unbound renders through the procedural Classic path (per-part fallback).
 * Geometry never lives here — lookup keys restyle, panelgeo positions.
 * Proposed on-disk form: docs/superpowers/specs/2026-09-26-skins-design.md
 * (E0-6/E0-7 awaiting owner review).
 *
 * Manifest (line-based text):
 *   BACKGROUND.<sec>=file
 *   PART.<sec>.<kind>.<part>=file,NFRAMES
 * sec/kind = decimal (RI_SEC_* / RI_CK_*); part = [A-Za-z0-9._-]{1,48};
 * '#' starts a comment line; unknown keys are ignored; any malformed line
 * fails the whole load closed (struct zeroed).
 */
#ifndef RI_SKIN_H
#define RI_SKIN_H
#include <stdint.h>
#include "project/rbng.h"

#define RI_SKIN_MAX_PARTS 64u
#define RI_SKIN_PART_NAME 48u
#define RI_SKIN_FILE_NAME 128u
#define RI_SKIN_MAX_FRAMES 256u

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
    char name[RI_SKIN_FILE_NAME + 1u]; /* mod directory name (set by loader) */
    uint16_t nparts;
    struct RISkinPart parts[RI_SKIN_MAX_PARTS];
};

/* Manifest parse: 0 ok; <0 fail-closed (struct zeroed):
 * -1 bad arg; -2 line too long; -3 malformed line; -4 part-table full;
 * -5 bad number/range; -6 duplicate key. A comment-only (or empty) manifest
 * is a valid empty skin (Template): 0 parts, everything falls back. */
int ri_skin_parse(const char *text, struct RISkin *s);
/* Part lookup: index, or -1 when absent (caller renders Classic). */
int ri_skin_find(const struct RISkin *s, uint8_t section, uint8_t kind,
                 const char *part);
/* Bind decoded 2x-master pixels to part idx: 0 ok, -1 bad arg/idx/dims. */
int ri_skin_bind(struct RISkin *s, uint32_t idx, const uint32_t *rgba,
                 uint16_t w, uint16_t h);
/* Bind a zoom-scaled copy (built once per zoom change with ri_skin_downscale,
 * never per frame): 0 ok, -1 bad arg/idx/dims. */
int ri_skin_bind_zoom(struct RISkin *s, uint32_t idx, const uint32_t *rgba,
                      uint16_t w, uint16_t h);
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
/* "base.<w>x<h>" into out (E0-9 size-varying parts): 0 ok, -1 bad arg/dims/
 * capacity (need strlen(base)+1+digits+1+digits+1). */
int ri_skin_part_sized(const char *base, uint32_t w, uint32_t h,
                       char *out, uint32_t cap);
/* ARGB words (core format) -> blit-order words for WritePixelArrayAlpha
 * ((b<<24)|(g<<16)|(r<<8)|a — the proven knob_blit packing; ARGB paints
 * ghost-blue). Pure, in-place-safe (dst may equal src). */
void ri_skin_swizzle_blit(const uint32_t *src, uint32_t *dst, uint32_t n);
#endif
