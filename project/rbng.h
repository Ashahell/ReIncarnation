/* rbng.h — RBNG song codec, full (Task 13, gate G13).
 * Spec §13: IFF-style chunking, big-endian FORM[type] {ID32 size32 data
 * + even pad}; readers MUST skip unknown optional chunks, reject
 * unsupported mandatory features, validate
 * lengths/ranges/references/nesting, never trust chunk sizes for
 * allocation. SHA-256 = content identity; song MODR stores name +
 * SHA-256 + vers. AUTO ticks are song-ppq ticks (recorder quantizes GUI
 * 30 Hz tweaks at ppq/24 per spec §8). CPRG = on-load copyright hook.
 *
 * Binary layout (all integers big-endian):
 *   FORM u32_tot 'RBNG' { chunk }          (u32_tot = bytes after it)
 *   chunk = ID32 u32_size data [pad u8 if size odd]
 *   'VERS': u16 major, u16 minor, u32 feature_flags
 *           (current 1.1/flags 0; major!=1 reject; minor newer loads
 *           known + preserves unknown; unknown flag bits reject).
 *           minor 0 files never carry BANK; minor 1 files carry PATT
 *           only when nbanks == 0 (legacy shape).
 *   'SONG': u16 tempo (30..300), u16 ppq (24..960), u16 nsteps (1..64)
 *   'PATT': nsteps x { u8 note, u8 flags } (flags = RI_RBNG_* below;
 *           length must equal nsteps*2 exactly). Required when minor
 *           is 0, optional when minor >= 1.
 *   'BANK' (v1.1): u8 instance, u8 kind, u8 drum_class, u8 count
 *           (1..32); then count x { u8 slot (0..31), u8 kind, u8 length
 *           (1..16), u8 payload_ver (=1), 16 rows: 303 -> {u8 key,
 *           u8 flags} (32 B); drum -> {LE16 on, LE16 high, LE16 flam,
 *           u8 flags} (112 B) }. Slots ascend; missing slots load
 *           cleared; per-pattern kind must equal the bank kind.
 *   'AUTO': u16 n (0..256); per: u32 tick, u16 ctl, u8 val, u8 pad0
 *   'MODR': u16 n (0..16); per: u8 namelen, name[namelen],
 *           u8 shalen(=64 hex), sha[64], u16 vers
 *   'CPRG': u8 len, text[len] (UTF-8 copyright notice, 0..127)
 *   unknown optional: ID[0] 'A'..'Z' → skip + preserve verbatim;
 *           anything else → reject ("unknown chunk id").
 *   'SKIN' (optional artwork ref): presence clears the art-fallback flag;
 *           absence loads fine with rbng_art_fallback() == 1.
 *
 * Replaces the Task-4 text scaffold parser target: tools/render keeps
 * the text path byte-identical AND gains --rbngsong through this codec.
 * Step flag bits are numerically equal to the walker RI_STEP_* bits
 * (asserted in t1_formats) so PATT maps 1:1 onto struct RIStep.
 */
#ifndef RI_RBNG_H
#define RI_RBNG_H
#include <stdint.h>
#include "engine/seq/pattern.h"

#define RI_RBNG_MAJOR 1u
#define RI_RBNG_MINOR 1u

#define RI_RBNG_MAX_STEPS 64u
#define RI_RBNG_MAX_AUTO 256u
#define RI_RBNG_MAX_MODS 16u
#define RI_RBNG_MAX_UNK 8u
#define RI_RBNG_MAX_UNK_BYTES 1024u
#define RI_RBNG_MAX_MOD_NAME 63u
#define RI_RBNG_MAX_CPRG 127u
#define RI_RBNG_MAX_BANKS 8u /* bounded rack (D-l); Classic uses 4 */

/* Step flag bits (== RI_STEP_* by contract, see t1_formats §16). */
#define RI_RBNG_SLIDE 0x01u
#define RI_RBNG_ACCENT 0x02u
#define RI_RBNG_REST 0x04u
#define RI_RBNG_FLAM 0x08u
#define RI_RBNG_UP 0x10u
#define RI_RBNG_DOWN 0x20u

struct RBSongStep {
    uint8_t note;
    uint8_t flags;
};

struct RBAutoEv {
    uint32_t tick; /* song-ppq ticks */
    uint16_t ctl; /* shared GUI/automation/MIDI/ARexx control ID */
    uint8_t val; /* 0..127 */
};

struct RBModRef {
    char name[RI_RBNG_MAX_MOD_NAME + 1u];
    char sha[65]; /* 64 lowercase hex + NUL */
    uint16_t vers;
};

struct RBUnknown {
    char id[5]; /* 4 chars + NUL */
    uint32_t len;
    unsigned char data[RI_RBNG_MAX_UNK_BYTES];
};

struct RISong {
    uint16_t tempo;
    uint16_t ppq;
    uint16_t nsteps;
    struct RBSongStep steps[RI_RBNG_MAX_STEPS];
    uint16_t nauto;
    struct RBAutoEv auto_ev[RI_RBNG_MAX_AUTO];
    uint16_t nmods;
    struct RBModRef mods[RI_RBNG_MAX_MODS];
    char cprg[RI_RBNG_MAX_CPRG + 1u];
    uint16_t nunknown;
    struct RBUnknown unknown[RI_RBNG_MAX_UNK];
    /* v1.1 pattern banks (empty when nbanks == 0: legacy shape). */
    uint8_t nbanks;
    struct RIPatternBank bank[RI_RBNG_MAX_BANKS];
};

void rbng_song_init(struct RISong *s);

/* File codec. Returns 0 ok, nonzero with err filled
 * ("<ID> @<off>: <reason>" per §17 failure 2 style). */
int rbng_write_song(const char *path, const struct RISong *s, char *err,
    uint32_t errcap);
int rbng_read_song(const char *path, struct RISong *s, char *err,
    uint32_t errcap);

/* On-load CPRG hook: first CPRG text line state. Returns 0 ok with line
 * filled; missing CPRG is NOT an error (caller takes art/copyright
 * fallback) — presence flag reports it. */
int rbng_cprg_line(const char *path, char *line, uint32_t cap);

/* MODR check: 0 = all referenced mods present in have[nhave] (by name),
 * 1 = first missing written as
 * "MODR: mod '<name>' vers <v> not found — expected sha <sha>" (the warn
 * prompt tools/render prints and continues without the mod). */
int rbng_missing_warn(const struct RISong *s, const char *const *have,
    uint32_t nhave, char *warn, uint32_t cap);

/* Partial-art fallback: 1 when no SKIN chunk rode along (load continues
 * on the built-in placeholder), 0 when full art present. */
int rbng_art_fallback(const struct RISong *s);

/* v1.0 PATT -> instance-0/303/slot-0 bank (Task 8.5). Warnings (fold,
 * dropped step-0 slide / 303 flam, truncation) append to warn (may be
 * NULL/0 = no warnings kept). Returns 0 ok, 2 bad arg. */
int rbng_patt_to_bank(const struct RISong *s, struct RIPatternBank *b,
    char *warn, uint32_t warncap);

/* Test-only mutation helpers (tools/fuzz + corpus; never in engine/). */
int rbng_test_inject_unknown(const char *src, const char *dst,
    const char id[4], const void *data, uint32_t len);
int rbng_test_set_vers(const char *src, const char *dst, uint16_t major,
    uint16_t minor, uint32_t flags);
int rbng_test_patch_bytes(const char *src, const char *dst, uint32_t off,
    const void *bytes, uint32_t n);
#endif
