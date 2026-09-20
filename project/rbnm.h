/* rbnm.h — RBNM mod codec, S909-chunk subset (Task 9, gate G9).
 * Spec §13: IFF-style chunking, big-endian FORM[type] {ID32 size32 data
 * + even pad}; readers MUST skip unknown optional chunks, validate
 * lengths/ranges/references/nesting, never trust chunk sizes for
 * allocation. Full RBNG/RBNM (round-trip, fuzz, CPRG) is Task 13 — NOT
 * here. This subset: parse + validate + manifest check for the 909
 * clean pack (S909 + MANF + SMPL), plus a minimal writer for fixtures
 * and a bounded single-layer loader for tools/render --909pack.
 *
 * Binary layout (all integers big-endian):
 *   FORM u32_tot 'RBNM' { chunk }          (u32_tot = bytes after it)
 *   chunk = ID32 u32_size data [pad u8 if size odd]
 *   'S909': u16 nlayers, per layer:
 *           u8 voice, u8 idlen, id[idlen], u32 rate, u32 frames, u8 lo, u8 hi
 *   'MANF': UTF-8 text, one line per layer:
 *           LAYERID k=v;k=v;...  (10 required keys: src date equip lic
 *           holder proc fmt norm loop map; '#' comments / blank lines skip)
 *   'SMPL': u8 idlen, id[idlen], u32 frames, s16 frames (BE mono)
 *
 * Cross rules: S909/MANF/SMPL id sets are equal (no dup, no missing);
 * SMPL frames == S909 frames; MANF fmt "RATE/DEPTH" RATE == S909 rate.
 */
#ifndef RI_RBNM_H
#define RI_RBNM_H
#include <stdint.h>

/* Manifest keys (10, all mandatory, non-empty). */
#define RBNM_NKEYS 10u

/* Error text buffer discipline: always NUL-terminated, names chunk ID
 * + byte offset (spec §17 failure 2 style). */
int rbnm_validate_file(const char *path, char *err, uint32_t errcap);
int rbnm_validate_manifest_text(const char *text, char *err, uint32_t errcap);

/* Fixture writer (tests/tools only): builds a pack from n layers.
 * manifest = full MANF text (caller-owned); pcm[k] = frames[k] s16 host
 * samples. Returns 0 ok, nonzero IO/arg error. */
struct RBNMWriteLayer {
    const char *id;
    uint8_t voice;
    uint32_t rate;
    uint32_t frames;
    uint8_t lo;
    uint8_t hi;
    const int16_t *pcm;
};
int rbnm_write_pack(const char *path, const struct RBNMWriteLayer *L,
    uint32_t n, const char *manifest);

/* Bounded loader (tools only): validates the file, then converts layer
 * id to f32 mono (s16/32768) into dst[cap]; rate_out gets the S909 rate.
 * Returns frames loaded, or -1 with err filled. */
int32_t rbnm_load_smpl(const char *path, const char *id, float *dst,
    uint32_t cap, uint32_t *rate_out, char *err, uint32_t errcap);

/* Pack inventory (tools only): validates the file, then lists its
 * layers into out[cap] (id/voice/rate/frames/lo/hi). Returns the layer
 * count, or -1 with err filled (truncation to cap is an error). */
struct RBNMLayerInfo {
    char id[32];
    uint8_t voice;
    uint32_t rate;
    uint32_t frames;
    uint8_t lo;
    uint8_t hi;
};
int32_t rbnm_pack_layers(const char *path, struct RBNMLayerInfo *out,
    uint32_t cap, char *err, uint32_t errcap);
#endif
